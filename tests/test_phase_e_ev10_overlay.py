#!/usr/bin/env python3
"""EV-T10 source/province overlay unit and 30,000-province performance probes.

All geometry is SYNTHETIC by design. Historical truth requires independent
licensed source layers, provenance and full-world mismatch coverage.
"""
from __future__ import annotations
import json
import math
import tempfile
import time
import unittest
from pathlib import Path
import sys
from shapely.geometry import LineString, box, mapping, shape

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"scripts"))
from ev10_historical_overlay import (Candidate, HistoricalLine, audit,
                                      read_provinces, read_historical_lines)
from ev10_history_polygon_import import convert


def line_feature(fid:str,coords:list,year=1850,precision=3):
    return {"type":"Feature",
            "properties":{
                "boundary_id":fid,"year":year,"border_kind":"sovereignty",
                "source":"Synthetic EV10 test","source_url":"https://example.invalid/fixture",
                "license":"TEST_FIXTURE","precision":precision,
            },
            "geometry":mapping(LineString(coords))}


class OverlayTests(unittest.TestCase):
    def test_crossing_vs_aligned_border(self):
        provinces=[Candidate(1,box(0,0,1,1)),Candidate(2,box(1,0,2,1))]
        sources=[
            HistoricalLine("cross-1850",LineString([(0,.5),(2,.5)]),1850,
                           "sovereignty","fixture","test://f","TEST",3),
            HistoricalLine("aligned-1900",LineString([(1,0),(1,1)]),1900,
                           "sovereignty","fixture","test://f","TEST",3),
            HistoricalLine("before-1801",LineString([(0,.75),(2,.75)]),1700,
                           "sovereignty","fixture","test://f","TEST",3),
        ]
        outcome=audit(provinces,sources)
        self.assertEqual(outcome["audited_lines"],2)
        self.assertEqual(outcome["mismatch_count"],2)
        self.assertEqual({m["historical_boundary_id"] for m in outcome["mismatches"]},
                         {"cross-1850"})
        self.assertTrue(all(m["inside_length_m"]>50 for m in outcome["mismatches"]))
        self.assertTrue(all(m["province_id"] in (1,2) for m in outcome["mismatches"]))

    def test_historical_polygon_shared_border_only(self):
        provinces=[
            Candidate(1,box(0,0,1.5,1)),
            Candidate(2,box(1.5,0,3,1)),
        ]
        def feature(geom,name,sovereign):
            return {"type":"Feature",
                    "properties":{"NAME":name,"SUBJECTO":sovereign,
                                  "BORDERPRECISION":3},
                    "geometry":mapping(geom)}
        layer=[
            feature(box(0,0,1,1),"A","A"),
            feature(box(1,0,2,1),"B","B"),
            feature(box(2,0,3,1),"B-sub","B"),
        ]
        lines, unresolved=convert(layer,year=1850,source="Fixture",
                                  source_url="test://source",
                                  license_name="TEST_FIXTURE",
                                  border_kind="sovereignty")
        self.assertEqual(len(lines),1)
        self.assertFalse(unresolved)
        self.assertEqual(lines[0]["properties"]["jurisdiction_left"],"A")
        self.assertEqual(lines[0]["properties"]["jurisdiction_right"],"B")
        candidate_lines=[
            HistoricalLine("poly-derived",shape(lines[0]["geometry"]),1850,
                           "sovereignty","fixture","test://f","TEST",3)
        ]
        result=audit(provinces,candidate_lines)
        self.assertEqual(result["mismatch_count"],1)
        self.assertEqual(result["mismatches"][0]["province_id"],1)

    def test_provenance_and_duplicate_ids(self):
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/"prov.geojson"
            def feature(pid):
                return {"type":"Feature","properties":{"province_id":pid},
                        "geometry":mapping(box(0,0,1,1))}
            path.write_text(json.dumps({"type":"FeatureCollection",
                                        "features":[feature(4),feature(4)]}),encoding="utf-8")
            with self.assertRaisesRegex(ValueError,"duplicate"):
                read_provinces(path)
            path2=Path(temp)/"history.geojson"
            broken=line_feature("line-1",[(0,0),(1,1)])
            broken["properties"]["license"]=""
            path2.write_text(json.dumps({"type":"FeatureCollection",
                                         "features":[broken]}),encoding="utf-8")
            with self.assertRaisesRegex(ValueError,"source/provenance"):
                read_historical_lines(path2)

    def test_untrusted_antimeridian_is_quarantined(self):
        provinces=[Candidate(1,box(170,-10,180,10))]
        source=[HistoricalLine("seam",LineString([(175,0),(-175,0)]),1914,
                               "sovereignty","fixture","test://seam","TEST",2)]
        result=audit(provinces,source)
        self.assertEqual(result["mismatch_count"],0)
        self.assertEqual(result["quarantined_count"],1)
        self.assertEqual(result["quarantined"][0]["reason"],
                         "antimeridian_requires_geometry_split")

    def test_30000_province_indexed_query(self):
        # 200 x 150 rectangles, geographical coordinates (near equator).
        provinces=[]
        for y in range(150):
            for x in range(200):
                p=1+y*200+x
                west=x*.01
                south=y*.01
                provinces.append(Candidate(p,box(west,south,west+.01,south+.01)))
        line=HistoricalLine("historic-diagonal",
            LineString([(.0001,.003),(1.9999,1.497)]),1914,
            "sovereignty","Synthetic historical event","test://fixture","TEST",2)
        started=time.perf_counter()
        report=audit(provinces,[line],minimum_inside_m=50)
        elapsed=time.perf_counter()-started
        self.assertEqual(report["province_count"],30000)
        self.assertFalse(report["over_budget"])
        self.assertGreater(report["mismatch_count"],20)
        self.assertLess(report["pair_intersections"],1500)
        self.assertEqual(report["quarantined_count"],0)
        print("EV10_GIS_30K_PROFILE provinces=30000 "
              f"rtree_hits={report['pair_intersections']} "
              f"mismatches={report['mismatch_count']} elapsed_ms={elapsed*1000:.3f}")


if __name__=="__main__":
    unittest.main(verbosity=2)
