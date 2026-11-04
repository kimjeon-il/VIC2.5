#!/usr/bin/env python3
"""EV-T10 offline historical-boundary/province intersection auditor.

Usage:
  python scripts/ev10_historical_overlay.py --provinces candidate.geojson \
      --history-lines lines.geojson --output report.json

Requires shapely>=2 and pyproj>=3. Geometry never enters the real-time
simulation; only immutable per-province IDs and compact adjacency are exported.
This tool reports omissions; it deliberately does NOT split a province.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable

from pyproj import Geod
from shapely.geometry import LineString, shape
from shapely.strtree import STRtree

GEOD = Geod(ellps="WGS84")
VERSION = "ev10-overlay/1.0"


@dataclass(frozen=True)
class Candidate:
    province_id: int
    geometry: Any


@dataclass(frozen=True)
class HistoricalLine:
    record_id: str
    geometry: Any
    year: int
    kind: str
    source: str
    source_url: str
    license: str
    precision: int


def read_collection(path: Path) -> tuple[list[dict[str, Any]], str]:
    if not path.is_file() or path.stat().st_size > 400_000_000:
        raise ValueError(f"missing or too large GeoJSON: {path}")
    raw = path.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    data = json.loads(raw)
    if data.get("type") != "FeatureCollection" or not isinstance(data.get("features"), list):
        raise ValueError(f"{path}: expected GeoJSON FeatureCollection")
    return data["features"], digest


def strict_geom(feature: dict[str, Any], wanted: set[str], label: str) -> Any:
    geometry = feature.get("geometry")
    if not isinstance(geometry, dict):
        raise ValueError(f"{label}: missing geometry")
    geom = shape(geometry)
    if geom.geom_type not in wanted or geom.is_empty or not geom.is_valid:
        raise ValueError(f"{label}: invalid or unsupported {geom.geom_type}")
    if not all(math.isfinite(float(v)) for v in geom.bounds):
        raise ValueError(f"{label}: nonfinite coordinates")
    west, south, east, north = geom.bounds
    if west < -180 or east > 180 or south < -90 or north > 90:
        raise ValueError(f"{label}: longitude/latitude bounds not CRS84")
    return geom


def read_provinces(path: Path) -> tuple[list[Candidate], str]:
    features, digest = read_collection(path)
    candidates: list[Candidate] = []
    seen: set[int] = set()
    for index, feature in enumerate(features):
        prop = feature.get("properties") or {}
        value = prop.get("province_id")
        if isinstance(value, bool) or not isinstance(value, int) or value <= 0 or value in seen:
            raise ValueError(f"province #{index}: duplicate/missing/invalid stable province_id")
        geom = strict_geom(feature, {"Polygon", "MultiPolygon"}, f"province {value}")
        seen.add(value)
        candidates.append(Candidate(value, geom))
    if not candidates:
        raise ValueError("empty province collection")
    candidates.sort(key=lambda c: c.province_id)
    return candidates, digest


def read_historical_lines(path: Path) -> tuple[list[HistoricalLine], str]:
    features, digest = read_collection(path)
    records: list[HistoricalLine] = []
    seen: set[str] = set()
    for index, feature in enumerate(features):
        prop = feature.get("properties") or {}
        line_id = str(prop.get("boundary_id") or "").strip()
        year = prop.get("year")
        kind = str(prop.get("border_kind") or "").strip()
        source = str(prop.get("source") or "").strip()
        source_url = str(prop.get("source_url") or "").strip()
        license_name = str(prop.get("license") or "").strip()
        precision = prop.get("BORDERPRECISION", prop.get("precision", 0))
        if (not line_id or line_id in seen or not isinstance(year, int) or
                not kind or not source or not source_url or not license_name or
                not isinstance(precision, int) or not (0 <= precision <= 3)):
            raise ValueError(f"historical boundary #{index}: invalid source/provenance/year")
        geom = strict_geom(feature, {"LineString", "MultiLineString"}, f"boundary {line_id}")
        records.append(HistoricalLine(line_id, geom, year, kind, source,
                                      source_url, license_name, precision))
        seen.add(line_id)
    if not records:
        raise ValueError("no historical lines")
    return records, digest


def iter_line_geoms(geom: Any) -> Iterable[LineString]:
    if geom.is_empty:
        return
    if geom.geom_type in ("LineString", "LinearRing"):
        if geom.length:
            yield geom
    elif geom.geom_type in ("MultiLineString", "GeometryCollection"):
        for part in geom.geoms:
            yield from iter_line_geoms(part)


def geodesic_line_m(geom: Any) -> float:
    total = 0.0
    for line in iter_line_geoms(geom):
        lon, lat = line.xy
        total += abs(GEOD.line_length(lon, lat))
    return total


def cross_antimeridian(geom: Any) -> bool:
    for line in iter_line_geoms(geom):
        lon, _ = line.xy
        if any(abs(lon[k+1]-lon[k]) > 180 for k in range(len(lon)-1)):
            return True
    return False


def audit(provinces: list[Candidate], historical: list[HistoricalLine],
          *, tolerance_degrees: float = 0.00001,
          minimum_inside_m: float = 50,
          from_year: int = 1801,
          max_provinces_budget: int = 30000) -> dict[str, Any]:
    if not (0 <= tolerance_degrees <= .001 and
            0 <= minimum_inside_m <= 100000 and
            from_year >= 0 and max_provinces_budget >= 1):
        raise ValueError("invalid precision/budget options")
    timer = time.perf_counter()
    shapes = [p.geometry for p in provinces]
    tree = STRtree(shapes)
    mismatch: list[dict[str, Any]] = []
    quarantined: list[dict[str, Any]] = []
    pairs_considered = 0
    audited_lines = 0
    for record in sorted(historical, key=lambda l: (l.year, l.record_id)):
        if record.year < from_year:
            continue
        if cross_antimeridian(record.geometry):
            quarantined.append({"boundary_id": record.record_id,
                                "reason": "antimeridian_requires_geometry_split"})
            continue
        audited_lines += 1
        candidates = tree.query(record.geometry, predicate="intersects")
        for position in sorted(int(k) for k in candidates):
            province = provinces[position]
            pairs_considered += 1
            try:
                clipped = record.geometry.intersection(province.geometry)
                # Exclude segments lying on or within the declared positional
                # uncertainty of the fixed province edge; not a boundary error.
                if tolerance_degrees:
                    clipped = clipped.difference(
                        province.geometry.boundary.buffer(tolerance_degrees))
                else:
                    clipped = clipped.difference(province.geometry.boundary)
                inside = geodesic_line_m(clipped)
            except Exception as ex:
                quarantined.append({"boundary_id": record.record_id,
                                    "province_id": province.province_id,
                                    "reason": "topology_operation_failed",
                                    "detail": str(ex)[:200]})
                continue
            if inside <= minimum_inside_m:
                continue
            mismatch.append({
                "province_id": province.province_id,
                "historical_boundary_id": record.record_id,
                "year": record.year,
                "border_kind": record.kind,
                "source": record.source,
                "source_url": record.source_url,
                "license": record.license,
                "precision": record.precision,
                "inside_length_m": round(inside, 3),
                "status": "needs_boundary_review" if record.precision < 2
                          else "cannot_represent_with_fixed_province",
            })
    mismatch.sort(key=lambda r:(r["year"],r["historical_boundary_id"],r["province_id"]))
    return {
        "contract": VERSION,
        "status": "DIAGNOSTIC_ONLY",
        "province_count": len(provinces),
        "target_range": [10000,20000],
        "stress_budget_provinces": max_provinces_budget,
        "over_budget": len(provinces) > max_provinces_budget,
        "from_year": from_year,
        "audited_lines": audited_lines,
        "pair_intersections": pairs_considered,
        "mismatch_count": len(mismatch),
        "quarantined_count": len(quarantined),
        "mismatches": mismatch,
        "quarantined": quarantined,
        "elapsed_ms": round((time.perf_counter()-timer)*1000,3),
        "note": ("No runtime province split, no change to PopulationSlice, "
                 "ownership, money, military or world state. "
                 "This is not proof of complete historical coverage."),
    }


def main() -> int:
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--provinces", type=Path, required=True)
    cli.add_argument("--history-lines", type=Path, required=True)
    cli.add_argument("--output", type=Path, required=True)
    cli.add_argument("--from-year", type=int, default=1801)
    cli.add_argument("--snap-tolerance-degrees", type=float, default=0.00001)
    cli.add_argument("--min-inside-m", type=float, default=50)
    cli.add_argument("--budget", type=int, default=30000)
    args=cli.parse_args()
    try:
        provinces,province_hash=read_provinces(args.provinces)
        lines,source_hash=read_historical_lines(args.history_lines)
        result=audit(provinces,lines,tolerance_degrees=args.snap_tolerance_degrees,
                     minimum_inside_m=args.min_inside_m,from_year=args.from_year,
                     max_provinces_budget=args.budget)
        result["provinces_sha256"]=province_hash
        result["historical_lines_sha256"]=source_hash
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2,sort_keys=True)+"\n",
                               encoding="utf-8")
        print(f"EV10_AUDIT provinces={result['province_count']} "
              f"lines={result['audited_lines']} "
              f"mismatch={result['mismatch_count']} "
              f"quarantined={result['quarantined_count']} "
              f"ms={result['elapsed_ms']} output={args.output}")
        # Unrepresentable history is the expected finding, not an engine error.
        return 0
    except (ValueError, OSError, json.JSONDecodeError) as err:
        print(f"EV10_INVALID_INPUT: {err}")
        return 2

if __name__=="__main__":
    raise SystemExit(main())
