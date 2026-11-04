#!/usr/bin/env python3
"""Optional *real-source* EV-T10 sample: 1815 vs 1900 European borders.

This does not generate the final 10k-20k province data. It compares two
historical world-map snapshots using a geographically bounded, temporary
candidate from the 1815 features. GPL-3.0 source geometry stays in-memory and
is NOT committed to VIC2.5. Only counts/hashes are printed.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import tempfile
import urllib.request

from shapely.geometry import box, mapping

from ev10_historical_overlay import Candidate, HistoricalLine, audit, strict_geom
from ev10_history_polygon_import import convert

SOURCE_COMMIT="4b5039e50d8d2464b065bd76ad6086cd4597108d"
BASE="https://raw.githubusercontent.com/aourednik/historical-basemaps/"
SOURCE_URL=f"https://github.com/aourednik/historical-basemaps/tree/{SOURCE_COMMIT}"
EUROPE_BOX=box(-10,43,21,58)


def download_year(year:int):
    url=f"{BASE}{SOURCE_COMMIT}/geojson/world_{year}.geojson"
    request=urllib.request.Request(url,headers={"User-Agent":"VIC25-EV10-GIS-Audit/1.0"})
    with urllib.request.urlopen(request,timeout=55) as response:
        data=response.read(12_000_000)
        if len(data)>=12_000_000:
            raise RuntimeError("historical source exceeds download budget")
    payload=json.loads(data)
    if payload.get("type")!="FeatureCollection":
        raise RuntimeError("invalid historical source object")
    return payload.get("features",[]), hashlib.sha256(data).hexdigest()


def europe_features(features,year):
    accepted=[]
    invalid=[]
    for pos,feature in enumerate(features):
        try:
            geom=strict_geom(feature,{"Polygon","MultiPolygon"},f"{year} region {pos}")
            if not geom.intersects(EUROPE_BOX):
                continue
            # Temporary clip for this sample ONLY. Does NOT alter production
            # ProvinceId or authoritative historical source geometry.
            cropped=geom.intersection(EUROPE_BOX)
            if cropped.is_empty or cropped.geom_type not in ("Polygon","MultiPolygon"):
                continue
            item={"type":"Feature","properties":feature.get("properties",{}),
                  "geometry":mapping(cropped)}
            accepted.append((pos,item))
        except Exception as ex:
            invalid.append((pos,str(ex)[:120]))
    return accepted,invalid


def main():
    first,sha_1815=download_year(1815)
    last,sha_1900=download_year(1900)
    a,invalid_a=europe_features(first,1815)
    b,invalid_b=europe_features(last,1900)
    if len(a)<10 or len(b)<3:
        raise RuntimeError("historic Europe sample unexpectedly sparse")
    candidates=[]
    for original,feature in a:
        geom=strict_geom(feature,{"Polygon","MultiPolygon"},"sample province")
        candidates.append(Candidate(original+1,geom))
    converted,unresolved=convert([feature for _,feature in b],year=1900,
                                 source="Aourednik historical-basemaps",
                                 source_url=SOURCE_URL,license_name="GPL-3.0",
                                 border_kind="sovereignty",snap_degrees=.0001)
    lines=[]
    from shapely.geometry import shape
    for feature in converted:
        prop=feature["properties"]
        lines.append(HistoricalLine(
            prop["boundary_id"],shape(feature["geometry"]),1900,
            "sovereignty",prop["source"],prop["source_url"],
            prop["license"],prop["BORDERPRECISION"]))
    if not lines:
        raise RuntimeError("historical polygons yielded zero shared border lines")
    result=audit(candidates,lines,min_inside_m=200)
    if result["mismatch_count"]==0:
        raise RuntimeError("real-source 1815-vs-1900 comparison did not detect change")
    print("EV10_REAL_HISTORICAL_SAMPLE "
          f"source_1815_sha256={sha_1815} "
          f"source_1900_sha256={sha_1900} "
          f"candidate_historical_polygons={len(candidates)} "
          f"historical_1900_lines={len(lines)} "
          f"mismatched_pairs={result['mismatch_count']} "
          f"quarantined={result['quarantined_count']} "
          f"unresolved_polygons={len(unresolved)} "
          f"invalid_1815={len(invalid_a)} invalid_1900={len(invalid_b)}")
    print("EV10_REAL_SOURCE_SAMPLE_PASS (limited sample, not the 10-20k world candidate)")


if __name__=="__main__":
    main()
