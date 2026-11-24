import csv
from importlib.util import find_spec
import json
import subprocess
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

from PIL import Image

SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "preflight_province_raster.py"


class ProvinceRasterPreflightTest(unittest.TestCase):
    def invoke(self, pixels, rows, *options):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            atlas = Image.new("RGB", (len(pixels[0]), len(pixels)))
            atlas.putdata([(p, 0, 0) for row in pixels for p in row])
            atlas.save(directory / "map.png")
            with (directory / "palette.csv").open("w", newline="", encoding="utf-8") as f:
                writer = csv.writer(f)
                writer.writerow(("rgb", "kind"))
                writer.writerows(rows)
            args = [sys.executable, str(SCRIPT), str(directory / "map.png"), str(directory / "palette.csv"), *options]
            return subprocess.run(args, text=True, capture_output=True)

    def test_four_way_contacts_separate_water_and_no_wrap(self):
        pixels = [[1, 1, 3, 4], [1, 2, 3, 4], [1, 1, 4, 4]]
        rows = [("040000", "lake"), ("030000", "ocean"), ("020000", "land"), ("010000", "land")]
        run = self.invoke(pixels, rows)
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(run.stdout, self.invoke(pixels, rows[::-1]).stdout)
        report = json.loads(run.stdout)
        self.assertEqual(report["dimensions_pixels"], [4, 3])
        self.assertEqual(report["counts"], {"land": 2, "ocean": 1, "lake": 1})
        self.assertEqual(report["provinces"], [
            {"id": "010000", "kind": "land", "pixels": 5},
            {"id": "020000", "kind": "land", "pixels": 1},
            {"id": "030000", "kind": "ocean", "pixels": 2},
            {"id": "040000", "kind": "lake", "pixels": 4},
        ])
        self.assertEqual(report["contacts"], [
            ["010000", "020000", 3], ["010000", "030000", 1],
            ["010000", "040000", 1], ["020000", "030000", 1],
            ["030000", "040000", 3],
        ])

    def test_rejects_truncation_of_sixteen_bit_colors(self):
        def chunk(kind, data):
            body = kind + data
            return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))

        png = b"\x89PNG\r\n\x1a\n"
        png += chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 16, 2, 0, 0, 0))
        png += chunk(b"IDAT", zlib.compress(bytes.fromhex("00123456789ABC")))
        png += chunk(b"IEND", b"")
        with tempfile.TemporaryDirectory() as directory:
            image = Path(directory) / "map.png"
            image.write_bytes(png)
            palette = Path(directory) / "palette.csv"
            palette.write_text("rgb,kind\n12569A,land\n", encoding="utf-8")
            result = subprocess.run([sys.executable, str(SCRIPT), str(image), str(palette)],
                                    text=True, capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("8-bit", result.stderr)
            self.assertEqual(result.stdout, "")

    @unittest.skipUnless(find_spec("osgeo"), "GDAL Python bindings not present")
    def test_gdal_rgb_backend_matches_pillow(self):
        rows = [("010000", "land"), ("020000", "ocean"), ("030000", "lake")]
        pixels = [[1, 1, 3], [2, 2, 3]]
        pillow = self.invoke(pixels, rows)
        gdal = self.invoke(pixels, rows, "--png-reader", "gdal")
        self.assertEqual(gdal.returncode, 0, gdal.stderr)
        self.assertEqual(gdal.stdout, pillow.stdout)

    def test_unknown_missing_and_duplicate_do_not_emit_partial_report(self):
        for rows, diagnostic in [
            ([("010000", "land")], "unregistered color 020000 at pixel (1, 0)"),
            ([("010000", "land"), ("020000", "lake"), ("030000", "ocean")], "unused registered color 030000"),
            ([("010000", "land"), ("010000", "lake"), ("020000", "ocean")], "duplicate RGB 010000"),
        ]:
            with self.subTest(diagnostic=diagnostic):
                run = self.invoke([[1, 2]], rows)
                self.assertNotEqual(run.returncode, 0)
                self.assertIn(diagnostic, run.stderr)
                self.assertEqual(run.stdout, "")


    def test_local_game_directory_agrees_with_raster_and_water_lists(self):
        registry = 'x010000 = "plains" # comment\nX020000 = "ocean"\nx030000 = "lakes"\n'
        default = '# comment\nprovinces = "provinces.png"\nsea_starts = { x020000 }\nlakes = { "x030000" }'
        with tempfile.TemporaryDirectory() as directory:
            game = Path(directory) / "game"
            maps = game / "map_data"
            maps.mkdir(parents=True)
            image = Image.new("RGB", (3, 1))
            image.putdata([(1, 0, 0), (2, 0, 0), (3, 0, 0)])
            image.save(maps / "provinces.png")
            terrain = maps / "province_terrains.txt"
            config = maps / "default.map"
            terrain.write_text(registry, encoding="utf-8-sig")
            config.write_text(default, encoding="utf-8-sig")
            args = [sys.executable, str(SCRIPT), "--game-dir", str(game)]

            def run():
                return subprocess.run(args, text=True, capture_output=True)

            result = run()
            self.assertEqual(result.returncode, 0, result.stderr)
            reference = self.invoke([[1, 2, 3]], [
                ("010000", "land"), ("020000", "ocean"), ("030000", "lake"),
            ])
            self.assertEqual(reference.stdout, result.stdout)
            config.write_text("\n".join(line for line in default.splitlines()
                                        if not line.strip().startswith("provinces ")),
                              encoding="utf-8")
            implicit = run()
            self.assertEqual(implicit.returncode, 0, implicit.stderr)
            self.assertEqual(implicit.stdout, result.stdout)
            config.write_text(default.replace("x020000 ", "x020000 x020000 "), encoding="utf-8")
            repeated = run()
            self.assertEqual(repeated.returncode, 0, repeated.stderr)
            self.assertEqual(repeated.stdout, result.stdout)
            self.assertIn("ignored 1 repeated sea_starts entries", repeated.stderr)
            for changed_registry, changed_default, diagnostic in [
                (registry, default.replace("provinces.png", "../alternate.png"), "provinces.png declaration"),
                (registry + "x010000 = plains\n", default, "duplicate province"),
                (registry, default.replace("x020000", "x040000"), "unregistered water province"),
                (registry, default.replace("x030000", "x020000"), "both sea_starts and lakes"),
                (registry, default.replace('"x030000"', "x030000 x030000"), "duplicate province"),
                (registry, default.replace('"x030000"', "range = { x030000 }"), "expected xRRGGBB"),
                (registry, default.replace("sea_starts", "ignored"), "missing sea_starts"),
                (registry, default + "\nlakes = {}", "duplicate lakes"),
                (registry.replace('"ocean"', "plains"), default, "disagrees"),
                (registry.replace("x010000", "x010100"), default, "unregistered color"),
                (registry + "x040000 = plains\n", default, "unused registered color"),
            ]:
                with self.subTest(diagnostic=diagnostic):
                    terrain.write_text(changed_registry, encoding="utf-8")
                    config.write_text(changed_default, encoding="utf-8")
                    result = run()
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(diagnostic, result.stderr)
                    self.assertEqual(result.stdout, "")


if __name__ == "__main__":
    unittest.main()
