#!/usr/bin/env python3
"""EV-T60 strict final acceptance. Returns nonzero for *any* incomplete gate.

This is intentionally NOT part of the success-oriented bounded CTest suite.
Run explicitly when all E-V tests and historical-province proof exist.
"""
from __future__ import annotations
import argparse
import re
import subprocess
import sys
from pathlib import Path

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--phase-e-test", type=Path, required=True)
    ap.add_argument("--historical-border-proof", type=Path)
    ap.add_argument("--real-gpu-proof", type=Path)
    ap.add_argument("--scale-proof", type=Path)
    ap.add_argument("--full-regression-proof", type=Path)
    args = ap.parse_args()

    result = subprocess.run([str(args.phase_e_test)], capture_output=True,
                            text=True, check=False)
    rows = re.findall(r"^EV-T(\d{2}) (PASS|BLOCKED|FAIL)\s*$",
                      result.stdout, re.MULTILINE)
    per_id: dict[int, list[str]] = {}
    for number, status in rows:
        per_id.setdefault(int(number), []).append(status)
    missing = [n for n in range(1, 61) if n not in per_id]
    duplicates = [n for n, statuses in per_id.items() if len(statuses) != 1]
    unknown = [n for n in per_id if not 1 <= n <= 60]
    if result.returncode != 0 or missing or duplicates or unknown:
        print("EV-T60 FAIL_INVALID_TEST_REPORT", {
              "runner_exit": result.returncode,
              "missing": missing, "duplicates": duplicates, "unknown": unknown})
        print(result.stderr)
        return 2

    blocking = sorted(n for n in range(1, 60) if per_id[n][0] != "PASS")
    proof_files = {
        "historical GIS border fidelity (EV-T10)": args.historical_border_proof,
        "real GPU context / device recovery (EV-T49)": args.real_gpu_proof,
        "10k/100k province and 100k/1m POP low-end benchmarks (EV-T59)": args.scale_proof,
        "B/C/D/E full cross-platform regression": args.full_regression_proof,
    }
    absent_proofs = [name for name, value in proof_files.items()
                     if value is None or not value.is_file() or value.stat().st_size == 0]
    if blocking or absent_proofs or per_id[60][0] != "PASS":
        print("EV-T60 BLOCKED", {
            "remaining_test_ids": blocking,
            "missing_independent_proof": absent_proofs,
            "self_gate": per_id[60][0],
        })
        return 1
    print("EV-T60 PASS: all 59 tests and independent proof artifacts verified")
    return 0

if __name__ == "__main__":
    sys.exit(main())
