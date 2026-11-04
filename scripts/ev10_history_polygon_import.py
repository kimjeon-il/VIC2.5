#!/usr/bin/env python3
"""Offline polygon-snapshot -> boundary-line normalizer for EV-T10.

Extracts only genuinely shared boundaries of DIFFERENT jurisdiction values.
Does not equate country coastlines with international borders and does not
silently reconcile overlaps, gaps or antimeridian-spanning polygons.
The source dataset's license and provenance must be supplied explicitly.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
from typing import Any
from shapely.geometry import mapping
from shapely.ops import snap
from shapely.strtree import STRtree

from ev10_historical_overlay import read_collection, strict_geom, iter_line_geoms


def convert(features: list[dict[str, Any]], *, year: int,
            source: str, source_url: str, license_name: str,
            border_kind: str, snap_degrees: float = .00001
            ) -> tuple[list[dict[str,Any]],list[dict[str,Any]]]:
    if (year<0 or not source or not source_url or not license_name or
            border_kind not in {"sovereignty","administration"} or
            not 0<=snap_degrees<=.001):
        raise ValueError("invalid polygon source parameters")
    rows=[]
    for i,f in enumerate(features):
        geom=strict_geom(f,{"Polygon","MultiPolygon"},f"historical polygon {i}")
        prop=f.get("properties") or {}
        key=("SUBJECTO" if border_kind=="sovereignty" else "NAME")
        label=str(prop.get(key) or prop.get("NAME") or "").strip()
        if not label:
            raise ValueError(f"historical polygon {i} lacks {key}/NAME")
        precision=prop.get("BORDERPRECISION",1)
        if not isinstance(precision,int) or precision not in (1,2,3):
            raise ValueError(f"historical polygon {i} invalid BORDERPRECISION")
        rows.append((geom,label,precision))
    shapes=[row[0] for row in rows]
    tree=STRtree(shapes)
    boundaries=[]
    unresolved=[]
    for i,(a,identity,precision) in enumerate(rows):
        for j in sorted(int(v) for v in tree.query(a,predicate="intersects") if int(v)>i):
            b,other,other_precision=rows[j]
            if identity==other:
                continue
            if a.intersection(b).area > 1e-7:
                unresolved.append({"polygon_a":i,"polygon_b":j,
                                   "reason":"jurisdictions_overlap",
                                   "labels":[identity,other]})
                continue
            # Exact boundaries preferred. Controlled snapping is explicitly
            # flagged and must not be interpreted as improving source accuracy.
            first=a.boundary
            second=b.boundary
            shared=first.intersection(second)
            if shared.length==0 and snap_degrees:
                shared=snap(first,second,snap_degrees).intersection(second)
            pieces=[geom for geom in iter_line_geoms(shared) if geom.length>1e-8]
            if not pieces:
                unresolved.append({"polygon_a":i,"polygon_b":j,
                                   "reason":"possible_gap_or_point_touch",
                                   "labels":[identity,other]})
                continue
            for k,line in enumerate(pieces):
                boundaries.append({
                    "type":"Feature",
                    "properties":{
                        "boundary_id":f"{year}:{i}:{j}:{k}",
                        "year":year,
                        "border_kind":border_kind,
                        "source":source,
                        "source_url":source_url,
                        "license":license_name,
                        "BORDERPRECISION":min(precision,other_precision),
                        "jurisdiction_left":identity,
                        "jurisdiction_right":other,
                        "derived_from_polygon_pair":[i,j],
                        "snap_tolerance_degrees":snap_degrees,
                    },
                    "geometry":mapping(line),
                })
    boundaries.sort(key=lambda f:f["properties"]["boundary_id"])
    return boundaries,unresolved


def main() -> int:
    cli=argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--polygons",type=Path,required=True)
    cli.add_argument("--output",type=Path,required=True)
    cli.add_argument("--year",type=int,required=True)
    cli.add_argument("--source",required=True)
    cli.add_argument("--source-url",required=True)
    cli.add_argument("--license",required=True)
    cli.add_argument("--kind",choices=["sovereignty","administration"],required=True)
    cli.add_argument("--snap-degrees",type=float,default=.00001)
    args=cli.parse_args()
    try:
        features,digest=read_collection(args.polygons)
        lines,unresolved=convert(features,year=args.year,source=args.source,
                                 source_url=args.source_url,license_name=args.license,
                                 border_kind=args.kind,snap_degrees=args.snap_degrees)
        output={
            "type":"FeatureCollection",
            "metadata":{
                "source_polygon_sha256":digest,
                "derived_boundary_count":len(lines),
                "unresolved_pairs":unresolved,
                "year":args.year,
                "source":args.source,
                "source_url":args.source_url,
                "license":args.license,
            },
            "features":lines,
        }
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(output,ensure_ascii=False,sort_keys=True)+"\n",
                               encoding="utf-8")
        print(f"EV10_POLYGON_CONVERT features={len(features)} borders={len(lines)} "
              f"unresolved_pairs={len(unresolved)} output={args.output}")
        return 0
    except (ValueError,OSError,json.JSONDecodeError) as ex:
        print(f"EV10_POLYGON_INVALID: {ex}")
        return 2


if __name__=="__main__":
    raise SystemExit(main())
