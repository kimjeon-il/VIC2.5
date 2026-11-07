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
import csv
import hashlib
import json
import pathlib
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
    links = {}
    for feature in features(line_path):
        props = feature.get("properties") or {}
        mode = mode_of(props)
        sid = props.get("source_feature") or props.get("osm_id") or feature.get("id")
        if mode is None or sid is None:
            continue  # Unsupported topology or undocumented line ID.
        sid = str(sid)
        if not sid or any(c in sid for c in "\r\n\t"):
            raise ValueError("Invalid source feature reference")
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
                source = "%s:%s" % (mode, sid)
                key = (source, origin, destination, mode)
                link_id = "GIS:" + hashlib.sha256(
                    ("%s|%s|%s|%s" % key).encode("utf-8")
                ).hexdigest()[:24]
                if link_id in links and links[link_id] != key:
                    raise ValueError("Source link hash collision")
                links[link_id] = key
    return links, len(polygons)


def make_services(path, links, digest):
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
    links, province_count = make_links(args.provinces, args.lines)
    digest_lines = sha256(args.lines)
    digest_provinces = sha256(args.provinces)
    digest_services = sha256(args.services) if args.services else None
    services = make_services(args.services, links, digest_services or "")
    args.output.mkdir(parents=True, exist_ok=True)
    link_rows = [(id, key[0], key[1], key[2], key[3])
                 for id, key in sorted(links.items())]
    write_tsv(args.output / "geographic_links.tsv", LINK_COLUMNS, link_rows)
    write_tsv(args.output / "transport_services.tsv", SERVICE_COLUMNS, services)
    (args.output / "provenance.json").write_text(json.dumps({
        "province_geodata_sha256": digest_provinces,
        "network_line_data_sha256": digest_lines,
        "verified_service_file_sha256": digest_services,
        "license_notice": "Original map data may be © OpenStreetMap contributors (ODbL)",
        "province_count": province_count,
        "physical_links": len(links),
        "dated_verifiable_transport_services": len(services),
        "historical_routes_not_inferred": True,
        "travel_days_and_capacity_not_inferred": True,
    }, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
