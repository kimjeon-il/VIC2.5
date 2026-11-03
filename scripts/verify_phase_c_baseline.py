#!/usr/bin/env python3
"""Auditable D4-T48 contract/source baseline gate (not a complete-game certification).

The full C++ integration prototype is tested separately by CTest. This audit
blocks that test suite if POP/C-D0/C-E/C-P/C-M/D4 canonical contracts drift,
if registered Phase B sources disappear, or if D4 IDs/versions are inconsistent.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path


def fail(why: str) -> None:
    raise SystemExit("D4-T48 CONTRACT BASELINE FAIL: " + why)


def blob_hash(data: bytes) -> str:
    return hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()


def numbered_rows(text: str, prefix: str, width: int, n: int) -> None:
    match = re.findall(
        r"^\|\s*" + re.escape(prefix) + r"(\d{" + str(width) + r"})\s*\|",
        text,
        flags=re.MULTILINE,
    )
    actual = [int(x) for x in match]
    expected = list(range(1, n + 1))
    if actual != expected:
        fail(f"{prefix} expected 1..{n}, found {actual}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--root", type=Path, default=Path(__file__).resolve().parents[1]
    )
    opts = parser.parse_args()
    root = opts.root.resolve()
    baseline_file = root / "docs/phase-c/integration/contract_baseline.json"
    manifest = json.loads(baseline_file.read_text(encoding="utf-8"))
    if manifest["schema_version"] != "d4-contract-baseline/1.0":
        fail("unexpected contract baseline schema")
    if manifest["scope"] != "source-and-version-registration-gate-only":
        fail("scope must not falsely claim complete game simulation acceptance")

    versions = manifest["contract_versions"]
    expected_versions = {
        "pop": "pop-design/1.1",
        "common": "phase-c-common/1.0",
        "economy": "phase-c-economy/1.0",
        "politics": "phase-c-politics/1.0",
        "military": "phase-c-military/1.0",
        "integration": "phase-c-integration/1.0",
    }
    if versions != expected_versions:
        fail("contract version schema drift")

    pinned = manifest["canonical_git_blobs"]
    if len(pinned) < 27:
        fail("missing canonical economic/political/military integration contracts")
    for name, want_sha in pinned.items():
        path = (root / name).resolve()
        if not path.is_relative_to(root) or not path.is_file():
            fail(f"missing or outside repository: {name}")
        actual = blob_hash(path.read_bytes())
        if actual != want_sha:
            fail(f"canonical document changed without versioned baseline: {name} {actual} != {want_sha}")

    for name in manifest["required_runtime_files"]:
        path = (root / name).resolve()
        if not path.is_relative_to(root) or not path.is_file():
            fail(f"missing integrated Phase B/C++ source or test: {name}")

    indexed_versions = {
        "pop": "docs/pop/D0_ARCHITECTURE.md",
        "common": "docs/phase-c/C-D0_COMMON_CONTRACT.md",
        "economy": "docs/phase-c/economy/README.md",
        "politics": "docs/phase-c/politics/README.md",
        "military": "docs/phase-c/military/README.md",
        "integration": "docs/phase-c/integration/README.md",
    }
    for domain, file in indexed_versions.items():
        if versions[domain] not in (root / file).read_text(encoding="utf-8"):
            fail(f"missing {domain} version token: {file}")

    checks = [
        ("docs/phase-c/economy/ECONOMIC_ADRS.md", "CE-ADR-", 3, 28),
        ("docs/phase-c/economy/ECONOMIC_ACCEPTANCE_TESTS.md", "CE-INV-", 2, 12),
        ("docs/phase-c/economy/ECONOMIC_ACCEPTANCE_TESTS.md", "CE-T", 2, 24),
        ("docs/phase-c/politics/POLITICAL_ADRS.md", "CP-ADR-", 3, 30),
        ("docs/phase-c/politics/POLITICAL_ACCEPTANCE_TESTS.md", "CP-INV-", 2, 12),
        ("docs/phase-c/politics/POLITICAL_ACCEPTANCE_TESTS.md", "CP-T", 2, 25),
        ("docs/phase-c/military/MILITARY_ADRS.md", "CM-ADR-", 3, 34),
        ("docs/phase-c/military/MILITARY_ACCEPTANCE_TESTS.md", "CM-INV-", 2, 16),
        ("docs/phase-c/military/MILITARY_ACCEPTANCE_TESTS.md", "CM-T", 2, 30),
        ("docs/phase-c/integration/D4-7_RELEASE_GATES_AND_ADRS.md", "D4-ADR-", 3, 20),
        ("docs/phase-c/integration/D4-3_INVARIANTS.md", "D4-INV-", 2, 30),
        ("docs/phase-c/integration/D4-5_TEST_MATRIX.md", "D4-T", 2, 48),
    ]
    count_by_key = manifest["expected_ids"]
    if (
        count_by_key.get("D4_T") != 48
        or count_by_key.get("D4_INV") != 30
        or count_by_key.get("CP_T") != 25
    ):
        fail("declared design test counts diverged")
    for path, prefix, width, n in checks:
        numbered_rows((root / path).read_text(encoding="utf-8"), prefix, width, n)

    test_source = (root / "tests/test_phase_c.cpp").read_text(encoding="utf-8")
    runnable = [int(x) for x in re.findall(r"test\.run\((\d+),", test_source)]
    if sorted(runnable) != list(range(1, 49)) or len(runnable) != 48:
        fail(f"all 48 acceptance IDs must be real runnable cases: got {sorted(runnable)}")
    if "test.skip(" in test_source:
        fail("D4 numbered harness contains blocked/skipped cases")
    if "test.run(48," not in test_source:
        fail("missing 48th aggregate contract baseline test")
    if "SnapshotCodec::contract_header()" not in test_source:
        fail("aggregate C++ test must verify binary save versions")

    cpp = (root / "include/vic25/phase_c.hpp").read_text(encoding="utf-8")
    if any(x not in cpp for x in (
        "phaseb::World people_", "FinancialLedger money_", "InventoryLedger goods_",
        "OwnershipLedger ownership_", "StagingWorld", "void validate() const",
    )):
        # StagingWorld is an architecture role, not a distinct class in the first
        # implementation. The 'stage = *this' snapshot is its current representation.
        if "IntegratedWorld stage = *this;" not in cpp:
            fail("C-D0 shared staged-world ownership not implemented")
    snapshot = (root / "include/vic25/phase_c_snapshot.hpp").read_text(encoding="utf-8")
    if manifest["checkpoint_schema"] not in snapshot:
        fail("checkpoint schema version absent")
    for version in versions.values():
        if version not in snapshot:
            fail(f"checkpoint is not locked to {version}")
    for value in ("numeric_policy", "quantity_policy", "ruleset_identity"):
        if manifest[value] not in snapshot:
            fail(f"checkpoint metadata missing {value}={manifest[value]}")

    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    for executable in (
        "vic25_phase_c_tests", "vic25_phase_c_longrun",
        "vic25_phase_c_combined_longrun", "vic25_phase_c_population_crisis",
        "vic25_phase_c_scaling", "vic25_phase_c_contract_audit",
    ):
        if executable not in cmake:
            fail(f"build regression is missing target: {executable}")

    for name in (
        "docs/phase-c/politics/examples/election_three_seats.json",
        "docs/phase-c/politics/examples/law_effective_next_month.json",
        "docs/phase-c/integration/examples/cross_domain_reform_mobilization.json",
        "docs/phase-c/integration/examples/logistics_war_occupation.json",
    ):
        fixture = json.loads((root / name).read_text(encoding="utf-8"))
        if not isinstance(fixture, dict):
            fail(f"invalid fixture payload: {name}")

    print(json.dumps({
        "gate": "D4-T48-source-contract-baseline",
        "status": "PASS",
        "canonical_git_blobs_verified": len(pinned),
        "design_test_counts": count_by_key,
        "runnable_D4_ids": 48,
        "contracts": versions,
        "checkpoint_schema": manifest["checkpoint_schema"],
        "scope": "prototype baseline only; does not certify every C-E/C-P/C-M feature",
    }, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (KeyError, ValueError, OSError, json.JSONDecodeError) as error:
        fail(str(error))
