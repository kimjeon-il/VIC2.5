#!/usr/bin/env python3
"""Exact-day chronology checks based on the checked-in rail evidence."""
import csv
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from compile_historical_rail_lifetimes import (  # noqa: E402
    CATALOG, certain_month, read_tsv, validate_catalog,
)

TRACKED = ROOT / "data/historical_transport/rail_openings.tsv"
JAPAN = "jp-shimbashi-yokohama"


class HistoricalRailChronologyTests(unittest.TestCase):
    def catalog_with_freight_date(self, path, freight_date):
        rows = read_tsv(TRACKED, CATALOG)
        japan = next(row for row in rows if row["line_id"] == JAPAN)
        self.assertEqual(japan["opened_on"], "1872-10-14")
        japan["confirmed_freight_start_on"] = freight_date
        with path.open("w", encoding="utf-8", newline="") as stream:
            writer = csv.DictWriter(stream, CATALOG, delimiter="\t")
            writer.writeheader()
            writer.writerows(rows)
        return validate_catalog(path)

    def test_earlier_same_month_freight_is_rejected_before_month_projection(self):
        self.assertEqual(certain_month("1872-10-13"), certain_month("1872-10-14"))
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaisesRegex(ValueError, "freight service cannot precede"):
                self.catalog_with_freight_date(Path(tmp) / "earlier.tsv", "1872-10-13")

    def test_same_day_freight_is_allowed(self):
        with tempfile.TemporaryDirectory() as tmp:
            result = self.catalog_with_freight_date(Path(tmp) / "same.tsv", "1872-10-14")
        self.assertEqual(result[JAPAN][:2], (1872 * 12 + 10, 1872 * 12 + 10))

    def test_tracked_catalogue_and_first_complete_months_are_unchanged(self):
        result = validate_catalog(TRACKED)
        self.assertEqual(len(result), 8)
        self.assertEqual([key for key, value in result.items() if value[1] is not None],
                         [JAPAN])
        self.assertEqual(result[JAPAN][:2], (1872 * 12 + 10, 1873 * 12 + 9))
        self.assertEqual(certain_month("1872-10-01"), 1872 * 12 + 9)


if __name__ == "__main__":
    unittest.main()
