#!/usr/bin/env python3
"""Audit and execute every G/H/H-V acceptance ID, without pretending a skip is a pass.

Usage:
  python scripts/verify_phase_gh_238.py --root . --audit
  python scripts/verify_phase_gh_238.py --root . --binary build/vic25_phase_gh_acceptance --report build/gh-238-report.json
  python scripts/verify_phase_gh_238.py --root . --binary ... --require-all

Exit 0 means registry/runner are consistent and all runnable cases passed;
BLOCKED cases remain blocked. --require-all is the real release gate and
exits nonzero unless all 238 cases have genuine executable PASS evidence.
"""
import argparse
import json
import pathlib
import re
import subprocess
import sys
from collections import Counter

SOURCE_FILES = [
    "docs/phase-g/G-D0_COMMON_CONTRACT.md",
    "docs/phase-g/TRADE_ACCEPTANCE_TESTS.md",
    "docs/phase-h/H-D0_COMMON_CONTRACT.md",
    "docs/phase-h/DIPLOMACY_ACCEPTANCE_TESTS.md",
    "docs/phase-h/integration/H-V5_TEST_MATRIX.md",
]
EXPECTED_GROUPS = {
    "GD0": 12, "GT1": 10, "GT2": 10, "GT3": 10, "GT4": 10, "GT5": 10,
    "HD0": 20, "HD1": 12, "HD2": 12, "HD3": 12, "HD4": 12,
    "HD5": 12, "HD6": 12, "HD7": 12, "HV": 72,
}
ID_RE = re.compile(r"(?:GD0|GT[1-5]|HD0|HD[1-7]|HV)-T\d\d")
CPP_CASE_RE = re.compile(
    r'\{"((?:GD0|GT[1-5]|HD0|HD[1-7]|HV)-T\d{2})",t_[A-Za-z0-9_]+\}'
)

def fail(message):
    raise RuntimeError(message)

def read_specs(root):
    parsed = {}
    for source in SOURCE_FILES:
        path = root / source
        for line in path.read_text(encoding="utf-8").splitlines():
            if source.endswith("G-D0_COMMON_CONTRACT.md"):
                m = re.match(r"\s*-\s*(GD0-T\d{2})\s+(.+)$", line)
                title = m.group(2).strip() if m else None
            elif source.endswith("H-D0_COMMON_CONTRACT.md"):
                m = re.match(r"\s*-\s*(HD0-T\d{2})\s+(.+)$", line)
                title = m.group(2).strip() if m else None
            else:
                m = re.match(r"\|\s*((?:GT[1-5]|HD[1-7]|HV)-T\d{2})\s*\|\s*([^|]+)\|\s*([^|]+)\|", line)
                title = m.group(2).strip() if m else None
            if m:
                case = m.group(1)
                if case in parsed:
                    fail(f"duplicate specification {case}")
                parsed[case] = {
                    "source_path": source,
                    "title": title,
                    "oracle": m.group(3).strip() if m.lastindex >= 3 else title,
                }
    return parsed

def audit(root, registry):
    specs = read_specs(root)
    cases = registry.get("entries", [])
    by_id = {c["test_id"]: c for c in cases}
    if len(cases) != 238 or len(by_id) != 238:
        fail(f"registry has {len(cases)} cases, {len(by_id)} distinct; expected 238")
    if set(by_id) != set(specs):
        fail(f"registry/spec mismatch: missing={sorted(set(specs)-set(by_id))}, extra={sorted(set(by_id)-set(specs))}")
    counts = Counter(case.split("-")[0] for case in by_id)
    if dict(counts) != EXPECTED_GROUPS:
        fail(f"group counts mismatch: {counts}")
    for id_, row in by_id.items():
        spec = specs[id_]
        for key in ("source_path", "title", "oracle"):
            if row[key] != spec[key]:
                fail(f"registry/source drift {id_}:{key}")
        if row.get("ctest_name") != f"vic25_gh_accept_{id_}":
            fail(f"CTest label drift for {id_}")
    cpp = (root / "tests/test_phase_gh_238.cpp").read_text(encoding="utf-8")
    mapped = CPP_CASE_RE.findall(cpp)
    if len(mapped) != len(set(mapped)):
        fail("duplicate compiled C++ ID registration")
    pending = {r["test_id"] for r in cases if r["runner_state"] == "EXECUTABLE_PENDING_CI"}
    blocked = {r["test_id"] for r in cases if r["runner_state"] == "BLOCKED_UNIMPLEMENTED"}
    if set(mapped) != pending or not pending.isdisjoint(blocked):
        fail(f"registered CPP cases disagree with source registry: actual={len(mapped)} planned={len(pending)}")
    if len(blocked) + len(pending) != 238:
        fail("238 test coverage incomplete")
    if any(not by_id[x].get("blocked_reason") for x in blocked):
        fail("blocked case without a precise blocking reason")
    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    if "vic25_phase_gh_acceptance" not in cmake or 'SKIP_RETURN_CODE 77' not in cmake:
        fail("CMake does not use executable 238 cases and block-as-skip semantics")
    return {"total": len(by_id), "executable": len(pending),
            "blocked": len(blocked), "groups": dict(sorted(counts.items()))}

def run(root, registry, binary, strict, report_path):
    audit_summary = audit(root, registry)
    binary = pathlib.Path(binary).resolve()
    if not binary.is_file():
        fail(f"C++ acceptance executable missing: {binary}")
    results = []
    for case in registry["entries"]:
        id_ = case["test_id"]
        try:
            process = subprocess.run([str(binary), id_],
                                     capture_output=True, text=True, timeout=60, check=False)
            raw = (process.stdout or "") + (process.stderr or "")
            if process.returncode == 0:
                if case["runner_state"] != "EXECUTABLE_PENDING_CI" or not raw.startswith("PASS " + id_):
                    state = "FAIL"
                    detail = "unexpected PASS without explicitly mapped C++ oracle"
                else:
                    state, detail = "PASS", raw.strip()
            elif process.returncode == 77:
                if case["runner_state"] == "BLOCKED_UNIMPLEMENTED" and raw.startswith("BLOCKED " + id_):
                    state, detail = "BLOCKED", case["blocked_reason"]
                else:
                    state, detail = "FAIL", "unexpected BLOCKED for registered executable"
            else:
                state, detail = "FAIL", f"return_code={process.returncode}: {raw.strip()[-900:]}"
        except (subprocess.TimeoutExpired, OSError) as error:
            state, detail = "FAIL", repr(error)
        results.append({"test_id": id_, "status": state, "evidence": detail,
                        "source_path": case["source_path"],
                        "expected_oracle": case["oracle"]})
    counts = Counter(r["status"] for r in results)
    result = {
        "schema": "vic25.phase-gh-238.execution/1.0",
        "source_registry": "docs/phase-gh-acceptance/acceptance_registry.json",
        "audit": audit_summary,
        "counts": {"total": 238, "pass": counts["PASS"], "fail": counts["FAIL"],
                   "blocked": counts["BLOCKED"]},
        "status": ("FULL_ACCEPTED" if counts["PASS"] == 238 else
                   "FAILED" if counts["FAIL"] else "PARTIAL_BLOCKED"),
        "release_gate": ("PASS" if counts["PASS"] == 238 else "BLOCKED"),
        "metagate_HV_T72": next((r["status"] for r in results if r["test_id"] == "HV-T72"), "MISSING"),
        "results": results,
    }
    if report_path:
        target = pathlib.Path(report_path)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("G/H 238 ACCEPTANCE:", json.dumps(result["counts"], sort_keys=True))
    print("Gate:", result["release_gate"], " HV-T72:", result["metagate_HV_T72"])
    for group in ("GD0", "GT1", "GT2", "GT3", "GT4", "GT5",
                  "HD0", "HD1", "HD2", "HD3", "HD4", "HD5", "HD6", "HD7", "HV"):
        subset = [r for r in results if r["test_id"].split("-")[0] == group]
        c = Counter(r["status"] for r in subset)
        print(f" {group}: PASS={c['PASS']:2} FAIL={c['FAIL']:2} BLOCKED={c['BLOCKED']:2}")
    if counts["FAIL"] or (strict and counts["PASS"] != 238):
        return 1
    return 0

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--registry", default="docs/phase-gh-acceptance/acceptance_registry.json")
    ap.add_argument("--audit", action="store_true")
    ap.add_argument("--binary")
    ap.add_argument("--report")
    ap.add_argument("--require-all", action="store_true")
    opt = ap.parse_args()
    root = pathlib.Path(opt.root).resolve()
    registry = json.loads((root / opt.registry).read_text(encoding="utf-8"))
    if opt.audit and not opt.binary:
        result = audit(root, registry)
        print("238-ID registry audit PASS:", json.dumps(result, ensure_ascii=False, sort_keys=True))
        return 0
    if not opt.binary:
        ap.error("provide --binary for actual C++ acceptance execution")
    return run(root, registry, opt.binary, opt.require_all, opt.report)

if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as e:
        print("G/H 238 ACCEPTANCE AUDIT FAIL:", e, file=sys.stderr)
        sys.exit(2)
