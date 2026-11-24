#!/usr/bin/env python3
"""Synthetic CLI regressions for shared linear land frontiers."""
import csv
import json
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "scripts/build_verified_transport_links.py"
MODES = ("road", "rail", "river")


def rect(x1, y1, x2, y2):
    return [[(x1, y1), (x2, y1), (x2, y2), (x1, y2), (x1, y1)]]


def feature(kind, coords, **props):
    return {"type": "Feature", "properties": props,
            "geometry": {"type": kind, "coordinates": coords}}


def collection(path, rows):
    path.write_text(json.dumps({"type": "FeatureCollection", "features": rows}),
                    encoding="utf-8")


def tsv(path):
    with path.open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream, delimiter="\t"))


class VerifiedTransportFrontier(unittest.TestCase):
    def test_route_must_touch_a_shared_linear_frontier(self):
        corner = rect(-1, 2, 0, 3)
        for kind, coords, expected in (
                ("Polygon", corner, []),
                ("MultiPolygon", [rect(2, 0, 3, 1), corner],
                 sorted((f"{mode}:edge", "1", "2") for mode in MODES))):
            with self.subTest(kind=kind), TemporaryDirectory() as folder:
                root = Path(folder)
                provinces, lines = root / "provinces.geojson", root / "lines.geojson"
                output = root / "out"
                collection(provinces, [
                    feature("Polygon", rect(0, 0, 2, 2), province_id=1),
                    feature(kind, coords, province_id=2),
                ])
                collection(lines, [
                    feature("LineString", path, mode=mode, source_feature=name)
                    for mode in MODES for name, path in (
                        ("edge", [(1.5, .5), (2.5, .5)]),
                        ("corner", [(-.5, 2.5), (.5, 1.5)]),
                    )
                ])
                subprocess.run([sys.executable, str(SCRIPT), "--provinces", str(provinces),
                                "--lines", str(lines), "--output", str(output)],
                               check=True, capture_output=True, text=True)
                actual = sorted((r["source_feature"], r["from_province"], r["to_province"])
                                for r in tsv(output / "geographic_links.tsv"))
                self.assertEqual(actual, expected)
                self.assertEqual(tsv(output / "transport_services.tsv"), [])


if __name__ == "__main__":
    unittest.main()
