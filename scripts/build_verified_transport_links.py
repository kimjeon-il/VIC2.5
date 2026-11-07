#!/usr/bin/env python3
"""Build E/G transport links only from real geometry and documented service rates.

Inputs:
 - Game province polygon GeoJSON with properties.province_id (integer).
 - Measured line features, GeoJSON or osmium GeoJSONSeq with stable feature IDs.
 - Optional externally researched service-rates CSV. The script NEVER guesses
   carriage capacity, fare, route days or the historical opening date.
Requires shapely >=2. Output TSV plugs into verified_transport_seed.hpp.
"""
import argparse
import calendar
import csv
import hashlib
import json
import pathlib
import re
from collections import defaultdict

LINK_COLUMNS = ("link_id", "source_feature", "from_province",
                "to_province", "mode")
SERVICE_COLUMNS = ("service_id", "link_id", "carrier", "from_province",
                   "to_province", "mode", "monthly_capacity",
                   "freight_per_unit", "transit_days", "valid_from_month",
                   "valid_through_month", "source_hash")
ALLOWED = {"road", "rail", "sea", "river", "air"}


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(4 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def features(path):
    if path.suffix.lower() in (".geojsonseq", ".jsonseq", ".ndjson"):
        with path.open("r", encoding="utf-8") as stream:
            for row in stream:
                row = row.lstrip("\x1e").strip()
                if not row:
                    continue
                feature = json.loads(row)
                if feature.get("type") != "Feature":
                    raise ValueError("Expected one GeoJSON Feature per line")
                yield feature
    else:
        data = json.loads(path.read_text(encoding="utf-8"))
        if data.get("type") != "FeatureCollection":
            raise ValueError("Expected GeoJSON FeatureCollection")
        yield from data["features"]


def mode_of(props):
    candidate = str(props.get("mode", "")).lower()
    if candidate in ALLOWED:
        return candidate
    if props.get("highway"):
        return "road"
    if props.get("railway"):
        return "rail"
    if props.get("waterway") in ("river", "canal"):
        return "river"
    if props.get("route") == "ferry":
        return "sea"
    # An airport point is not proof of a flight route.
    return None


def known_month(value, is_start):
    """Earliest/last *certain* complete month of an OHM dated feature.

    A year-only date is not silently changed to a fabricated January 1.
    The complete existence window starts the next year for an undated
    month, the next month for an uncertain day, and similarly ends
    conservatively before the month/year containing an unknown end.
    """
    match = re.fullmatch(r"(\\d{4})(?:-(\\d{2}))?(?:-(\\d{2}))?",
                         str(value or ""))
    if not match:
        return None
    year = int(match[1])
    month = int(match[2]) if match[2] else None
    day = int(match[3]) if match[3] else None
    if year < 1 or year > 9999 or (month is not None and
                                    (month < 1 or month > 12)):
        raise ValueError("Historical transport source date out of range")
    if month is None:
        return (year + 1) * 12 if is_start else (year - 1) * 12 + 11
    index = year * 12 + month - 1
    if day is None:
        return index + 1 if is_start else index - 1
    if day < 1 or day > calendar.monthrange(year, month)[1]:
        raise ValueError("Invalid historical transport feature date")
    if is_start:
        return index if day == 1 else index + 1
    return index if day == calendar.monthrange(year, month)[1] else index - 1


def feature_lifetime(properties):
    begin = properties.get("start_date")
    finish = properties.get("end_date")
    first = known_month(begin, True) if begin else None
    last = known_month(finish, False) if finish else None
    if first is not None and last is not None and last < first:
        raise ValueError("Historical feature has no complete active month")
    return first, last


def lines_of(geom):
    if geom.geom_type == "LineString":
        yield geom
    elif geom.geom_type == "MultiLineString":
        yield from geom.geoms


def center_distance(line, clipped):
    segments = list(lines_of(clipped))
    if not segments:
        return None
    segment = max(segments, key=lambda g: g.length)
    if segment.length <= 1e-12:
        return None
    middle = segment.interpolate(segment.length / 2)
    return line.project(middle)


def make_links(province_path, line_path):
    from shapely.geometry import shape
    from shapely.strtree import STRtree

    polygons, ids = [], []
    for row in features(province_path):
        props = row.get("properties") or {}
        pid = props.get("province_id")
        if not isinstance(pid, int) or isinstance(pid, bool) or pid <= 0:
            raise ValueError("Missing positive integer province_id")
        poly = shape(row["geometry"])
        if not poly.is_valid or poly.is_empty or poly.geom_type not in (
                "Polygon", "MultiPolygon"):
            raise ValueError("Invalid province geometry: %s" % pid)
        ids.append(pid)
        polygons.append(poly)
    if len(ids) != len(set(ids)):
        raise ValueError("Duplicate province identifier")
    spatial_index = STRtree(polygons)
    province_by_id = dict(zip(ids, polygons))
    links, lifetimes = {}, {}
    for feature in features(line_path):
        props = feature.get("properties") or {}
        mode = mode_of(props)
        sid = props.get("source_feature") or props.get("osm_id") or feature.get("id")
        if mode is None or sid is None:
            continue  # Unsupported topology or undocumented line ID.
        sid = str(sid)
        if not sid or any(c in sid for c in "\r\n\t"):
            raise ValueError("Invalid source feature reference")
        initial_month, last_month = feature_lifetime(props)
        geometry = shape(feature["geometry"])
        for line in lines_of(geometry):
            if line.length <= 0:
                continue
            positions = []
            for index in spatial_index.query(line):
                poly = polygons[int(index)]
                if not line.intersects(poly):
                    continue
                d = center_distance(line, line.intersection(poly))
                if d is not None:
                    positions.append((d, ids[int(index)]))
            positions.sort()
            ordered = []
            for _, pid in positions:
                if not ordered or ordered[-1] != pid:
                    ordered.append(pid)
            for origin, destination in zip(ordered, ordered[1:]):
                if origin == destination:
                    continue
                if mode in ("road", "rail", "river"):
                    # Road/rail/river cannot skip an intermediate province.
                    # A physical line must actually cross the two polygons'
                    # common frontier, not just pass near both centroids.
                    from_poly, to_poly = (province_by_id[origin],
                                          province_by_id[destination])
                    common = from_poly.boundary.intersection(to_poly.boundary)
                    if common.is_empty or not line.intersects(common):
                        continue
                source = "%s:%s" % (mode, sid)
                key = (source, origin, destination, mode)
                link_id = "GIS:" + hashlib.sha256(
                    ("%s|%s|%s|%s" % key).encode("utf-8")
                ).hexdigest()[:24]
                if link_id in links and links[link_id] != key:
                    raise ValueError("Source link hash collision")
                if link_id in lifetimes and lifetimes[link_id] != (
                        initial_month, last_month):
                    raise ValueError("Conflicting dates on the same real link")
                links[link_id] = key
                lifetimes[link_id] = (initial_month, last_month)
    return links, len(polygons), lifetimes


def make_services(path, links, digest, lifetimes):
    rows = []
    if path is None:
        return rows
    required = ("service_id", "link_id", "carrier", "monthly_capacity",
                "freight_per_unit", "transit_days", "valid_from_month",
                "valid_through_month", "source_url")
    seen = set()
    with path.open("r", encoding="utf-8", newline="") as f:
        reader = csv.DictReader(f)
        if not reader.fieldnames or any(c not in reader.fieldnames
                                          for c in required):
            raise ValueError("Missing independently sourced service columns")
        for row in reader:
            sid, lid = row["service_id"], row["link_id"]
            if not sid or sid in seen or lid not in links:
                raise ValueError("Duplicate service or unverified geographic link")
            seen.add(sid)
            if not row["carrier"] or not row["source_url"].startswith(
                    ("https://", "http://")):
                raise ValueError("A service needs a real carrier and source URL")
            cap = int(row["monthly_capacity"])
            fare = int(row["freight_per_unit"])
            days = int(row["transit_days"])
            first = int(row["valid_from_month"])
            last = int(row["valid_through_month"])
            if cap <= 0 or fare < 0 or days <= 0 or first < 0 or last < first:
                raise ValueError("Missing actual capacity/fare/day or dating")
            geometry_first, geometry_last = lifetimes.get(
                lid, (None, None))
            if ((geometry_first is not None and first < geometry_first) or
                    (geometry_last is not None and last > geometry_last)):
                raise ValueError("Dated service exceeds certain OHM "
                                 "physical network lifetime")
            _, origin, destination, mode = links[lid]
            rows.append((sid, lid, row["carrier"], origin, destination, mode,
                         cap, fare, days, first, last, digest))
    return rows


def write_tsv(path, columns, rows):
    with path.open("w", encoding="utf-8", newline="") as out:
        writer = csv.writer(out, delimiter="\t", lineterminator="\n")
        writer.writerow(columns)
        writer.writerows(rows)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--provinces", required=True, type=pathlib.Path)
    p.add_argument("--lines", required=True, type=pathlib.Path)
    p.add_argument("--services", type=pathlib.Path)
    p.add_argument("--output", required=True, type=pathlib.Path)
    args = p.parse_args()
    links, province_count, lifetimes = make_links(
        args.provinces, args.lines)
    digest_lines = sha256(args.lines)
    digest_provinces = sha256(args.provinces)
    digest_services = sha256(args.services) if args.services else None
    services = make_services(
        args.services, links, digest_services or "", lifetimes)
    args.output.mkdir(parents=True, exist_ok=True)
    link_rows = [(id, key[0], key[1], key[2], key[3])
                 for id, key in sorted(links.items())]
    write_tsv(args.output / "geographic_links.tsv", LINK_COLUMNS, link_rows)
    write_tsv(args.output / "transport_services.tsv", SERVICE_COLUMNS, services)
    lifetime_rows = [
        (key, first if first is not None else "",
         last if last is not None else "")
        for key,(first,last) in sorted(lifetimes.items())
        if first is not None or last is not None
    ]
    if lifetime_rows:
        write_tsv(args.output / "physical_line_lifetimes.tsv",
                  ("link_id","certain_from_month","certain_through_month"),
                  lifetime_rows)
    (args.output / "provenance.json").write_text(json.dumps({
        "province_geodata_sha256": digest_provinces,
        "network_line_data_sha256": digest_lines,
        "verified_service_file_sha256": digest_services,
        "license_notice": "Original map data may be © OpenStreetMap contributors (ODbL)",
        "province_count": province_count,
        "physical_links": len(links),
        "dated_verifiable_transport_services": len(services),
        "historical_routes_not_inferred": True,
        "date_evidence_complete_months": sum(
            1 for first,last in lifetimes.values()
            if first is not None and last is not None),
        "dated_physical_lines": len(lifetime_rows),
        "travel_days_and_capacity_not_inferred": True,
    }, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
