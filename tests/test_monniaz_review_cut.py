#!/usr/bin/env python3
"""Check the stored Monniaz review partition without GIS or network dependencies."""
from collections import Counter
import json
from pathlib import Path
import unittest

FIXTURE = Path(__file__).resolve().parents[1] / "data/geography/review/jussy_monniaz_1836"


def read_features(name):
    return json.loads((FIXTURE / name).read_text(encoding="utf-8"))["features"]


def outer_ring(feature):
    if feature["geometry"]["type"] != "Polygon":
        raise AssertionError("Expected a review polygon")
    ring, = feature["geometry"]["coordinates"]
    return list(map(tuple, ring))


def edges(ring):
    return Counter(zip(ring, ring[1:]))


class MonniazReviewCut(unittest.TestCase):
    def test_review_cuts_share_only_the_frontier_and_partition_the_window(self):
        land = read_features("cut_land.geojson")
        self.assertEqual([f["properties"]["historical_side"] for f in land],
                         ["Swiss Geneva", "Kingdom of Sardinia"])
        self.assertTrue(all(f["properties"]["status"] == "review_only" for f in land))
        self.assertTrue(all("province_id" not in f["properties"] for f in land))
        west, east = map(outer_ring, land)
        scope, = map(outer_ring, read_features("cut_scope.geojson"))
        for ring in (west, east, scope):
            self.assertEqual(ring[0], ring[-1])
            self.assertEqual(len(set(ring[:-1])), len(ring) - 1)
        shared_west = [edge for edge in edges(west) if edge[::-1] in edges(east)]
        self.assertGreaterEqual(len(shared_west), 2)
        vertices = [shared_west[0][0]] + [b for _, b in shared_west]
        self.assertEqual(shared_west, list(zip(vertices, vertices[1:])))
        self.assertEqual(vertices, sorted(vertices, key=lambda v: v[1]))
        self.assertIn(vertices[0], scope)
        self.assertIn(vertices[-1], scope)
        self.assertTrue(set(vertices[1:-1]).isdisjoint(scope))
        exterior = edges(west) + edges(east)
        for e in shared_west:
            del exterior[e]
            del exterior[e[::-1]]
        self.assertEqual(exterior, edges(scope))


if __name__ == "__main__":
    unittest.main()
