#!/usr/bin/env python3
"""Static guard: geological reserves must not re-enter Phase D's resource runtime."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
header = (root / "include/vic25/phase_d.hpp").read_text(encoding="utf-8")
snapshot = (root / "include/vic25/phase_d_snapshot.hpp").read_text(encoding="utf-8")
industry = (root / "tests/test_phase_d_industry.cpp").read_text(encoding="utf-8")
checks = {
    "ResourceSite present": "struct ResourceSite {" in header,
    "underground total omitted from site schema": False,
    "flow capacity and production history present": all(
        x in header for x in ("monthly_capacity", "produced_this_month", "cumulative_output")
    ),
    "renewable biomass isolated": "standing_biomass" in header,
    "legacy type and API removed": not any(
        x in header + snapshot + industry
        for x in ("ResourceDeposit", "seed_resource_deposit", "d_.deposits", "domain_.deposits")
    ),
    "no legacy depletion guard": "extraction exceeds actual remaining stock" not in header,
    "modern snapshot schema": all(x in snapshot for x in (
        "VIC25_PHASED_SNAPSHOT_6", "phase-d-cpp/0.6",
        "phase-j-resource-flow/1.0", "F(monthly_capacity)", "F(standing_biomass)"
    )),
    "flow regression present": "test_no_mineral_total_and_activity_decline" in industry,
}
start = header.find("struct ResourceSite {")
end = header.find("};", start)
if start >= 0 and end >= 0:
    source_def = header[start:end]
    checks["underground total omitted from site schema"] = not re.search(
        r"\b(remaining|total_reserves|reserve_total|confirmed_extractable_stock|estimated_quantity|known_stock|in_situ_total)\b",
        source_def
    )
for name, good in checks.items():
    print(f"{'PASS' if good else 'FAIL'}: {name}")
if not all(checks.values()):
    raise SystemExit(1)
