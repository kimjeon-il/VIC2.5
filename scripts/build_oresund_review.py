#!/usr/bin/env python3
"""Rebuild the small, approximate 1839 chart review; not a campaign importer."""
import argparse
import json
from pathlib import Path

DATA = Path(__file__).resolve().parents[1] / "data/geography/review/oresund_1839"
SCAN = "08e8bbb1a9fcb7fa385756333b9789d59dc23113b701c93f0694d4394511a0ca"


def chart_coordinate(point):
    x, y = point
    # The printed chart defines its Ferro as 20°30′ west of Paris.
    greenwich_offset = 2 + 20 / 60 + 14.025 / 3600 - 20.5
    # Fitted chart meridians at 30° and 31° Ferro and the 56° parallel.
    # Units: pixels on the review image (1105 × 1570); dy is per 10 arcmin.
    x30 = 145.0 - 0.009 * (y - 515)
    x31 = 635.0 + 0.004 * (y - 515)
    y56 = 515.75 - 0.00175 * (x - 540)
    lon = 30 + (x - x30) / (x31 - x30) + greenwich_offset
    lat = 56 + (y56 - y) / 144.25 / 6
    return [round(lon, 7), round(lat, 7)]


def ring(points, outer):
    coordinates = [chart_coordinate(point) for point in points]
    if coordinates[0] != coordinates[-1]:
        raise ValueError("open ring")
    area = sum(x1 * y2 - x2 * y1 for (x1, y1), (x2, y2) in zip(coordinates, coordinates[1:]))
    if area == 0:
        raise ValueError("collapsed ring")
    return coordinates if (area > 0) == outer else coordinates[::-1]


def feature(name, shell, holes=()):
    return {
        "type": "Feature",
        "properties": {"name": name, "geometry_date": 1839, "status": "historical-chart-review-only"},
        "geometry": {"type": "Polygon", "coordinates": [ring(shell, True), *(ring(hole, False) for hole in holes)]},
    }


def build(points):
    if (
        points["source_sha256"] != SCAN
        or points["projected"] is not False
        or (points["working_pixel_width"], points["working_pixel_height"]) != (1105, 1570)
    ):
        raise ValueError("unexpected chart or pixel coordinate space")
    x0, y0, x1, y1 = points["bounds"]
    w, e, ven = points["west"], points["east"], points["ven"]
    for trace in (w, e):
        if trace[0][1] != y0 or trace[-1][1] != y1:
            raise ValueError("shore does not meet the study cut")
    p0, p1, p2, p3 = (x0, y0), (x0, y1), (x1, y1), (x1, y0)
    # Reuse exactly the same coastal arcs on both sides of the land/water partition.
    # The other edges close the chart crop, not a historical or administrative border.
    west = [p0, *w, p1, p0]
    east = [p3, p2, *reversed(e), p3]
    scope = [p0, w[0], e[0], p3, p2, e[-1], w[-1], p1, p0]
    water = [*w, *reversed(e), w[0]]
    layers = {
        "land": [feature("North Zealand cut", west), feature("West Scania cut", east), feature("Ven", ven)],
        "water": [feature("Øresund cut", water, [ven])],
        "scope": [feature("Study cut", scope)],
    }
    return {name: {"type": "FeatureCollection", "features": features} for name, features in layers.items()}


def write(layers, output):
    output.mkdir(parents=True, exist_ok=True)
    for name, collection in layers.items():
        (output / (name + ".geojson")).write_text(
            json.dumps(collection, indent=2) + "\n", encoding="utf-8", newline="\n"
        )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-pixels", type=Path, default=DATA / "source_pixels.json")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    write(build(json.loads(args.source_pixels.read_text(encoding="utf-8"))), args.output)


if __name__ == "__main__":
    main()
