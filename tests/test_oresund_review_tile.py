"""Verify the archived review tile, independent of graphics or GIS tools."""
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/build_oresund_review_tile.py"


def check_fail_closed():
    sys.path.insert(0, str(SCRIPT.parent))
    from build_oresund_review_tile import classify, polygon_relation

    scope = [[(0, 0), (4, 0), (4, 4), (0, 4), (0, 0)]]
    island = [[(1, 1), (2, 1), (2, 2), (1, 2), (1, 1)]]
    water = scope + island
    assert polygon_relation(water, 1.5, 1.5) is False
    assert classify(scope, {3: island, 101: water}, 1.5, 1.5) == 3
    assert classify(scope, {3: island, 101: water}, 3, 3) == 101
    for regions, point, message in (({101: water}, (1.5, 1.5), "gap or overlap"),
                                     ({3: island, 101: scope}, (1.5, 1.5), "gap or overlap"),
                                     ({3: island, 101: water}, (1, 1.5), "ambiguous review edge")):
        try:
            classify(scope, regions, *point)
        except ValueError as error:
            assert message in str(error), error
        else:
            raise AssertionError(f"accepted an invalid review cell at {point}")


def main():
    check_fail_closed()
    with TemporaryDirectory() as temporary:
        a = Path(temporary) / "a.txt"
        b = Path(temporary) / "b.txt"
        for path in (a, b):
            subprocess.run([sys.executable, str(SCRIPT), "--output", str(path)], check=True)
        raw = a.read_bytes()
        assert raw == b.read_bytes()
    assert raw.endswith(b"\n") and b"\r" not in raw
    lines = raw.decode("ascii").splitlines()
    assert lines[:3] == ["VIC25_ORESUND_CUBE_REVIEW_V1", "128 128 4",
                         "0.654 0.663 0.144 0.152"]
    cells = [list(map(int, row.split(" "))) for row in lines[3:]]
    assert len(cells) == 128 and all(len(row) == 128 for row in cells)
    assert {value for row in cells for value in row} == {0, 1, 2, 3, 101}
    for x, y, expected in ((69, 42, 1), (57, 84, 2), (89, 76, 3),
                           (91, 63, 101), (86, 85, 101), (44, 55, 101), (13, 7, 0)):
        assert cells[y][x] == expected, (x, y, expected, cells[y][x])


if __name__ == "__main__":
    main()
