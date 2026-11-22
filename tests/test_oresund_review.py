"""Audit the archived 1839 chart candidate without a GIS installation."""
from collections import Counter
import csv
import json
from pathlib import Path
import sys
from tempfile import TemporaryDirectory

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from build_oresund_review import build, write  # noqa: E402

DATA = ROOT / "data/geography/review/oresund_1839"


def edges(rings):
    result = Counter()
    for ring in rings:
        assert len(ring) >= 4 and ring[0] == ring[-1]
        for start, end in zip(ring, ring[1:]):
            assert start != end
            result[tuple(sorted((tuple(start), tuple(end))))] += 1
    return result


def hit_ring(ring, x, y):
    crossings = 0
    for (x1, y1), (x2, y2) in zip(ring, ring[1:]):
        if (y1 > y) != (y2 > y):
            crossings += x < (x2 - x1) * (y - y1) / (y2 - y1) + x1
    return crossings % 2 == 1


def hit_polygon(rings, point):
    return hit_ring(rings[0], *point) and not any(hit_ring(r, *point) for r in rings[1:])


def main():
    source = json.loads((DATA / "source_pixels.json").read_text(encoding="utf-8"))
    output = build(source)
    with TemporaryDirectory() as temp:
        write(output, Path(temp))
        for name in ("land", "water", "scope"):
            built = (Path(temp) / (name + ".geojson")).read_text(encoding="utf-8")
            recorded = (DATA / (name + ".geojson")).read_text(encoding="utf-8")
            assert built == recorded, name

    land = [f["geometry"]["coordinates"] for f in output["land"]["features"]]
    water = output["water"]["features"][0]["geometry"]["coordinates"]
    scope = output["scope"]["features"][0]["geometry"]["coordinates"]
    assert len(land) == 3 and len(water) == 2
    shore = edges(water)
    tally = shore.copy()
    for i, region in enumerate(land):
        e = edges(region)
        assert e & shore, "each land component must share a real coast edge"
        assert all(not e & edges(other) for other in land[:i]), "false direct land adjacency"
        tally.update(e)
    boundary = edges(scope)
    assert {edge for edge, n in tally.items() if n == 1} == set(boundary)
    assert all(n == (1 if edge in boundary else 2) for edge, n in tally.items())
    for point in ((12.65, 56.04), (12.63, 55.91), (12.75, 55.91), (12.70, 55.93)):
        assert hit_polygon(water, point), f"blocked chart water: {point}"
    assert not hit_polygon(water, (12.697, 55.908))
    assert hit_polygon(land[2], (12.697, 55.908))
    with (DATA / "engine-witness.csv").open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    assert len(rows) == 3 and [row["tag"] for row in rows] == ["DK", "SE", "SE"]
    assert [int(row["local_land_id"]) for row in rows] == [1, 2, 3]
    assert all(int(row["area_m2"]) > 0 and int(row["local_sea_id"]) == 101 for row in rows)
    print("Verified 1839 Øresund review polygons, preserved passage and sea contacts")


if __name__ == "__main__":
    main()
