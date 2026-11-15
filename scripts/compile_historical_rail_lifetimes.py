#!/usr/bin/env python3
"""Verify actual historical rail-opening evidence; map only proven GIS links.

No external Python packages. A named railway is never turned into a
province-to-province connection by guessing its location. A track opening is
NOT evidence of available freight, rate, travel time or monthly capacity.
"""
import argparse
import csv
import datetime as dt
from pathlib import Path

CATALOG = ("line_id", "region", "from_station", "to_station",
           "opened_on", "confirmed_freight_start_on", "source_url")
LINKS = ("link_id", "source_feature", "from_province", "to_province", "mode")
CROSSWALK = ("link_id", "line_id", "alignment_source_url")


def read_tsv(path, expected):
    with Path(path).open(encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream, delimiter="\t")
        if tuple(reader.fieldnames or ()) != expected:
            raise ValueError(f"{path}: expected columns {expected}")
        return list(reader)


def certain_month(date_string):
    if not date_string or len(date_string) != 10:
        raise ValueError("day-level verified opening required")
    date = dt.date.fromisoformat(date_string)
    if date.year < 1801:
        raise ValueError("VIC2.5 source scope starts in 1801")
    # If a line opened on the 15th, its first *complete* month is next month.
    # This is an evidence precision rule, not a fabricated first-of-month date.
    return date.year * 12 + date.month - 1 + (date.day != 1)


def validate_catalog(path):
    rows = read_tsv(path, CATALOG)
    if not rows:
        raise ValueError("no historical railway evidence")
    by_id = {}
    for row in rows:
        key = row["line_id"]
        if not key or key in by_id or not row["region"] or not row["from_station"] or not row["to_station"]:
            raise ValueError("duplicate or incomplete historical line")
        if row["from_station"] == row["to_station"] or not row["source_url"].startswith("https://"):
            raise ValueError("missing independent rail-opening evidence")
        opened = certain_month(row["opened_on"])
        freight = (certain_month(row["confirmed_freight_start_on"])
                   if row["confirmed_freight_start_on"] else None)
        if freight is not None and freight < opened:
            raise ValueError("freight service cannot precede first verified opening")
        by_id[key] = (opened, freight, row)
    return by_id


def link_crosswalk(catalog, links_path, crosswalk_path, output_path):
    links = {}
    for row in read_tsv(links_path, LINKS):
        key = row["link_id"]
        if not key or key in links or row["mode"] != "rail":
            if not key or key in links:
                raise ValueError("duplicate or empty physical GIS link")
            continue
        if int(row["from_province"]) <= 0 or int(row["to_province"]) <= 0:
            raise ValueError("invalid source-verified province mapping")
        links[key] = row
    out = []
    already = set()
    for row in read_tsv(crosswalk_path, CROSSWALK):
        link_id = row["link_id"]
        line_id = row["line_id"]
        if (link_id in already or line_id not in catalog or
                link_id not in links or not row["alignment_source_url"].startswith("https://")):
            raise ValueError("orphan or unsupported historic-to-geographic mapping")
        already.add(link_id)
        physical_month, _, _ = catalog[line_id]
        out.append((link_id, str(physical_month), ""))
    out.sort()
    with Path(output_path).open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, delimiter="\t", lineterminator="\n")
        writer.writerow(("link_id", "certain_from_month", "certain_through_month"))
        writer.writerows(out)
    return len(out)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--catalog", default="data/historical_transport/rail_openings.tsv")
    parser.add_argument("--links", help="geographic_links.tsv from proven GIS joins")
    parser.add_argument("--crosswalk", help="manual provenance-backed link_id to line_id TSV")
    parser.add_argument("--output", help="physical_line_lifetimes.tsv for G loader")
    args = parser.parse_args()
    catalog = validate_catalog(args.catalog)
    supplied = (args.links, args.crosswalk, args.output)
    if any(supplied) and not all(supplied):
        parser.error("--links, --crosswalk and --output must be supplied together")
    count = link_crosswalk(catalog, *supplied) if all(supplied) else 0
    unverified_freight = sum(v[1] is None for v in catalog.values())
    print(f"dated_rail_sections={len(catalog)} "
          f"known_freight_openings={len(catalog)-unverified_freight} "
          f"gis_link_crosswalks={count}; unknown freight rates/capacity/days are NOT seeded")


if __name__ == "__main__":
    main()
