import csv
import json
from importlib.util import find_spec
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "review_province_cube.py"
sys.path.insert(0, str(ROOT / "scripts"))

PALETTE = [(0, "land"), (2, "ocean"), (3, "lake"), (4, "land")]


class ProvinceCubeReviewTest(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.png = self.root / "map.png"
        self.palette = self.root / "kinds.csv"
        self.tile = self.root / "tile.rgbi"
        raster = Image.new("RGB", (8, 8))
        raster.putdata([(a, 0, 0) for a in [0]*16+[2]*16+[3]*16+[4]*16])
        raster.save(self.png)
        with self.palette.open("w", newline="", encoding="utf-8") as out:
            writer = csv.writer(out)
            writer.writerow(("rgb", "kind"))
            writer.writerows((f"{n:02X}0000", kind) for n, kind in PALETTE)

    def invoke(self, *other, size=64, bounds=("-4", "-4", "4", "4")):
        return subprocess.run([
            sys.executable, str(SCRIPT), str(self.png), str(self.palette),
            "--bounds", *bounds, "--uv-bounds", "-0.09", "-0.09", "0.09", "0.09",
            "--size", str(size), "--projection", "linear", "--transform", "4", "1", "4", "1",
            *other
        ], capture_output=True, text=True)

    def test_exact_preservation_opaque_black_is_not_missing(self):
        run = self.invoke("--tile-output", str(self.tile))
        self.assertEqual(run.returncode, 0, run.stderr)
        result = json.loads(run.stdout)
        self.assertTrue(result["preservation_passed"])
        self.assertEqual(result["source_cells"], 64)
        self.assertEqual(result["source_ids"], {"land": 2, "ocean": 1, "lake": 1})
        self.assertEqual(result["source_contact_pairs"], 3)
        data = self.tile.read_bytes()
        self.assertEqual(len(data), 64*64*4)
        self.assertEqual(data[:4], b"\x00\x00\x00\x00")
        # Engine convention: rows v (longitude west to east), cols u
        # (latitude north to south); blacks are land, not missing.
        center_row = [data[4*(32*64+i):4*(32*64+i+1)] for i in (16, 25, 39, 47)]
        self.assertEqual(center_row, [b"\x00\x00\x00\x01", b"\x02\x00\x00\x02",
                                      b"\x03\x00\x00\x03", b"\x04\x00\x00\x01"])

    def test_coarse_sample_reports_losses_and_does_not_write_tile(self):
        run = self.invoke("--tile-output", str(self.tile), size=3)
        self.assertEqual(run.returncode, 3, run.stderr)
        result = json.loads(run.stdout)
        self.assertFalse(result["preservation_passed"])
        self.assertGreater(result["missed_source_cells"], 0)
        self.assertGreater(result["missed_source_ids"], 0)
        self.assertFalse(self.tile.exists())
        self.assertGreater(result["unwitnessed_edges"], 0)

    def test_local_diagonal_contact_not_saved_by_distant_true_border(self):
        from review_province_cube import local_witness
        data = [b"A", b"C", b"A", b"B",
                b"C", b"B", b"A", b"B"]
        self.assertFalse(local_witness(data, 4, 8, 0, 5))
        self.assertTrue(local_witness(data, 4, 8, 2, 3))
        self.assertFalse(local_witness(data, 4, 8, 0, 3))
        self.assertFalse(local_witness(data, 4, 8, 3, 4))

    def test_invalid_or_uncovered_input_fails_without_tile(self):
        for bounds, diagnostic in [
            (("175", "-1", "-175", "1"), "no longitude wrap"),
            (("-4", "-4", "4", "50"), "+X cube face"),
            (("-5", "-4", "4", "4"), "outside source raster"),
        ]:
            with self.subTest(bounds=bounds):
                run = self.invoke("--tile-output", str(self.tile), bounds=bounds)
                self.assertEqual(run.returncode, 2, run.stderr)
                self.assertIn(diagnostic, run.stderr)
                self.assertFalse(self.tile.exists())
        self.palette.write_text("rgb,kind\n000000,land\n", encoding="utf-8")
        result = self.invoke()
        self.assertEqual(result.returncode, 2)
        self.assertIn("unregistered color", result.stderr)

    def test_same_label_diagonal_bridge_blocks_tile(self):
        raster = Image.new("RGB", (9, 9))
        cells = [(2, 0, 0)] * 81
        cells[45] = cells[55] = (1, 0, 0)
        raster.putdata(cells)
        raster.save(self.png)
        self.palette.write_text("rgb,kind\n010000,land\n020000,ocean\n", encoding="utf-8")
        run = self.invoke(
            "--tile-output", str(self.tile),
            "--uv-bounds", "-0.34707554003186286", "-0.5253526900535911",
            "0.34707554003186286", "0.08482034376842632",
            "--transform", "7.659954374900259", "0.27637983680697636",
            "0.7150514601870777", "0.4328413681773716",
            size=28, bounds=("-27.71531188163328", "-19.140637076116803",
                             "4.84824305774363", "1.651991960006145"))
        report = json.loads(run.stdout)
        self.assertEqual(report["missed_source_cells"], 0)
        self.assertEqual(report["missed_source_contact_pairs"], 0)
        self.assertEqual(report["new_tile_contact_pairs"], 0)
        self.assertEqual(run.returncode, 3, run.stderr)
        self.assertEqual(report["merged_tile_components"], 1)
        self.assertFalse(self.tile.exists())

    def test_component_relations_detect_split_and_merge_despite_equal_counts(self):
        from review_province_cube import ComponentIndex, component_changes
        source, tile = ComponentIndex(4), ComponentIndex(4)
        source.join(0, 1)
        tile.join(0, 2)
        self.assertEqual(component_changes(source, tile, [0, 1, 2, 3], {0, 1, 2, 3}), (1, 1))

    def test_publish_failure_leaves_no_partial_file(self):
        from unittest.mock import patch
        from review_province_cube import write_review_tile
        for method in ("fsync", "link"):
            with self.subTest(method=method):
                with patch(f"review_province_cube.os.{method}", side_effect=OSError("write unavailable")):
                    with self.assertRaisesRegex(OSError, "write unavailable"):
                        write_review_tile(self.tile, b"review bytes")
                self.assertFalse(self.tile.exists())
                self.assertEqual({f.name for f in self.root.iterdir()}, {"map.png", "kinds.csv"})

    def test_miller_is_an_explicit_review_option(self):
        run = self.invoke("--projection", "miller", "--transform", "4", "1", "4", "0.9")
        self.assertEqual(run.returncode, 0, run.stderr)
        report = json.loads(run.stdout)
        self.assertEqual(report["projection"], "miller")
        self.assertFalse(report["tile_written"])
        self.assertFalse(self.tile.exists())

    def test_opt_in_file_is_not_overwritten(self):
        self.tile.write_bytes(b"keep")
        run = self.invoke("--tile-output", str(self.tile))
        self.assertEqual(run.returncode, 2)
        self.assertEqual(self.tile.read_bytes(), b"keep")

    @unittest.skipUnless(find_spec("osgeo"), "GDAL Python bindings not present")
    def test_gdal_and_pillow_report_same(self):
        a = self.invoke()
        b = self.invoke("--png-reader", "gdal")
        self.assertEqual(b.returncode, 0, b.stderr)
        self.assertEqual(a.stdout, b.stdout)


if __name__ == "__main__":
    unittest.main()
