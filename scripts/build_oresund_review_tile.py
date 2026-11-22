"""Sample the archived Øresund chart review on its fixed +Z cube tile."""
import argparse
import json
import math
from pathlib import Path

SOURCE = Path(__file__).resolve().parents[1] / "data/geography/review/oresund_1839"
SIZE = 128
U0, U1, V0, V1 = 0.654, 0.663, 0.144, 0.152
HEADER = "VIC25_ORESUND_CUBE_REVIEW_V1\n128 128 4\n0.654 0.663 0.144 0.152\n"
LAND_IDS = {"North Zealand cut": 1, "West Scania cut": 2, "Ven": 3}
EDGE = -1


def read_polygons(path, expected):
    data = json.loads(path.read_text(encoding="utf-8"))
    if data["type"] != "FeatureCollection":
        raise ValueError(f"{path.name}: expected FeatureCollection")
    result = {}
    for feature in data["features"]:
        name = feature["properties"]["name"]
        geometry = feature["geometry"]
        if (feature["type"] != "Feature" or name not in expected or name in result
                or geometry["type"] != "Polygon"):
            raise ValueError(f"{path.name}: unexpected polygon {name!r}")
        rings = geometry["coordinates"]
        if len(rings) != expected[name]:
            raise ValueError(f"{path.name}: unexpected ring count for {name}")
        for ring in rings:
            if len(ring) < 4 or ring[0] != ring[-1]:
                raise ValueError(f"{path.name}: unclosed or short ring for {name}")
            for p, q in zip(ring, ring[1:]):
                if (len(p) != 2 or p == q or
                        not all(type(c) in (float, int) and math.isfinite(c) for c in p)
                        or not (-180 <= p[0] <= 180 and -90 <= p[1] <= 90)):
                    raise ValueError(f"{path.name}: invalid ring position for {name}")
        result[name] = rings
    if result.keys() != expected.keys():
        raise ValueError(f"{path.name}: missing expected review polygon")
    return result


def ring_relation(ring, x, y):
    inside = False
    for (a, b), (c, d) in zip(ring, ring[1:]):
        dx, dy = c - a, d - b
        # A center on, or within numerical tolerance of, any edge fails.
        tol = 1e-12
        if (min(a, c) - tol <= x <= max(a, c) + tol
                and min(b, d) - tol <= y <= max(b, d) + tol
                and abs(dx * (y - b) - dy * (x - a)) <= tol * math.hypot(dx, dy)):
            return EDGE
        if (b > y) != (d > y) and x < a + dx * (y - b) / dy:
            inside = not inside
    return inside


def polygon_relation(rings, x, y):
    outer = ring_relation(rings[0], x, y)
    if outer is not True:
        return outer
    for hole in rings[1:]:
        hit = ring_relation(hole, x, y)
        if hit == EDGE:
            return EDGE
        if hit:
            return False
    return True


def classify(scope, regions, x, y):
    in_scope = polygon_relation(scope, x, y)
    relations = [(local_id, polygon_relation(rings, x, y))
                 for local_id, rings in regions.items()]
    if in_scope == EDGE or any(hit == EDGE for _, hit in relations):
        raise ValueError(f"ambiguous review edge at {x:.12g}, {y:.12g}")
    hits = [local_id for local_id, hit in relations if hit]
    if in_scope and len(hits) == 1:
        return hits[0]
    if not in_scope and not hits:
        return 0
    raise ValueError(f"review coverage gap or overlap at {x:.12g}, {y:.12g}: {hits}")


def build():
    land = read_polygons(SOURCE / "land.geojson", dict.fromkeys(LAND_IDS, 1))
    water = read_polygons(SOURCE / "water.geojson", {"Øresund cut": 2})["Øresund cut"]
    scope = read_polygons(SOURCE / "scope.geojson", {"Study cut": 1})["Study cut"]
    regions = {local_id: land[name] for name, local_id in LAND_IDS.items()}
    regions[101] = water
    rows = []
    for row in range(SIZE):
        v = V0 + (row + 0.5) * (V1 - V0) / SIZE
        ids = []
        for col in range(SIZE):
            u = U0 + (col + 0.5) * (U1 - U0) / SIZE
            lon = math.degrees(math.atan2(v, u))
            lat = math.degrees(math.atan2(1, math.hypot(u, v)))
            ids.append(str(classify(scope, regions, lon, lat)))
        rows.append(" ".join(ids))
    return (HEADER + "\n".join(rows) + "\n").encode("ascii")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True, metavar="PATH")
    args = parser.parse_args()
    args.output.write_bytes(build())


if __name__ == "__main__":
    main()
