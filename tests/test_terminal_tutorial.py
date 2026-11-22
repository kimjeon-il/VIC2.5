#!/usr/bin/env python3
"""Black-box smoke and deterministic checkpoint checks for the native tutorial."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def native_save_with_origin(raw, origin, old_origin=b"tutorial:market-v1"):
    # Only the outer Daily World origin metadata is changed. The nested
    # committed C/D/E/G/H snapshot and remaining journal bytes stay exact.
    def field_end(offset):
        return offset + 8 + struct.unpack_from("<Q", raw, offset)[0]

    offset = 0
    for _ in range(5):  # four header strings plus nested native payload
        offset = field_end(offset)
    offset += 24  # day index, commit counter, seed
    require(raw[offset + 8:field_end(offset)] == old_origin,
            "fixture did not locate the expected outer origin")
    payload = (raw[:offset] + struct.pack("<Q", len(origin)) + origin
               + raw[field_end(offset):-8])
    checksum = 1469598103934665603
    for byte in payload:  # canonical native checksum, not a corruption test
        checksum = ((checksum ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return payload + struct.pack("<Q", checksum)


def main():
    binary = Path(sys.argv[1]).resolve()
    require(binary.is_file(), f"native terminal entrypoint does not exist: {binary}")

    def run(commands, *flags, success=True):
        out = subprocess.run(
            [str(binary), *map(str, flags)], input=commands, encoding="utf-8",
            capture_output=True, timeout=30, check=False,
        )
        require((out.returncode == 0) == success,
                f"unexpected process status {out.returncode}: {out.stdout} {out.stderr}")
        return out

    def statuses(out):
        return [line for line in out.stdout.splitlines() if line.startswith("date=")]

    with tempfile.TemporaryDirectory(prefix="vic25-terminal-") as temp:
        d = Path(temp)
        start, split, full, resumed, after_invalid = [
            d / (name + ".sav") for name in
            ("start", "split checkpoint", "full", "resumed", "invalid")
        ]
        saved = run(f"status\nsave {start}\nquit\n")
        opening = statuses(saved)[-1]
        require("date=2000-01-01" in opening and "population=4" in opening
                and "treasury=100" in opening and "food_price=10" in opening
                and "buyer_cash=50" in opening and "buyer_food=0" in opening
                and "producer_ore=62" in opening,
                f"opening status doesn't reflect actual tutorial state: {opening}")
        byte_before = start.read_bytes()

        invalid = run(f"buy 99\nstatus\nsave {after_invalid}\nquit\n",
                      "--tutorial", "market-v1", "--resume", start)
        require("could not be filled" in invalid.stderr and statuses(invalid)[-1] == opening,
                "an unaffordable partial order was not rejected atomically")
        require(after_invalid.read_bytes() == byte_before,
                "rejected command changed actual checkpoint bytes")
        overwritten = run(f"step 1\nsave {start}\nquit\n")
        require("refus" in overwritten.stderr.lower() and start.read_bytes() == byte_before,
                "an existing checkpoint was replaced or not reported")

        blocked = d / "blocked.sav"
        staging = Path(str(blocked) + ".vic25-saving")
        staging.mkdir()
        sentinel = staging / "other-file"
        sentinel.write_bytes(b"unchanged")
        staging_error = run(f"save {blocked}\nquit\n")
        require("staging path already exists" in staging_error.stderr and
                not blocked.exists() and sentinel.read_bytes() == b"unchanged",
                "save rewrote a foreign staging directory or published an incomplete save")

        first = run(f"buy 2\nstatus\nsave {split}\nquit\n")
        result = statuses(first)[-1]
        require("date=2000-01-02" in result and "buyer_cash=30" in result
                and "buyer_food=2" in result and "producer_ore=60" in result,
                f"real purchase or same-day production did not commit: {result}")
        uninterrupted = run(f"buy 2\nstep 3\nstatus\nsave {full}\nquit\n")
        continued = run(f"step 3\nstatus\nsave {resumed}\nquit\n",
                        "--tutorial", "market-v1", "--resume", split)
        require(statuses(uninterrupted)[-1] == statuses(continued)[-1] and
                full.read_bytes() == resumed.read_bytes(),
                "same tutorial policies did not restore exact deterministic native bytes")
        bad_tutorial = run("", "--tutorial", "unavailable", "--resume", split, success=False)
        require("unknown tutorial" in bad_tutorial.stderr.lower(),
                "unknown tutorial was not reported")
        implicit = run("", "--resume", split, success=False)
        require("resume requires --tutorial" in implicit.stderr,
                "resume accepted a snapshot without explicit tutorial selection")
        unrelated = d / "unrelated.sav"
        unrelated.write_bytes(native_save_with_origin(byte_before, b"other-scenario-v1"))
        guarded = run("", "--tutorial", "market-v1", "--resume", unrelated, success=False)
        require("checkpoint does not belong to market-v1" in guarded.stderr,
                "a valid native snapshot with an unrelated origin bypassed tutorial selection")
        damaged = d / "damaged.sav"
        damaged.write_bytes(byte_before[:-1] + bytes([byte_before[-1] ^ 1]))
        run("", "--tutorial", "market-v1", "--resume", damaged, success=False)
        flags = ("--trial", "tariff-v1", "--country")
        a_start, a_bad, a_full, a_split, a_resumed = [
            d / (name + ".sav") for name in
            ("tariff start","tariff rejected","tariff full","tariff split","tariff resumed")
        ]
        a = run(f"status\nsave {a_start}\nquit\n", *flags, "A")
        def trade_status(out):
            line = statuses(out)[-1]
            return dict(kv.split("=", 1) for kv in line.split())
        opening = trade_status(a)
        require("synthetic" in a.stdout and opening["country"] == "A"
                and opening["treasury"] == "100" and opening["max_tariff_ppm"] == "200000"
                and opening["tariff_ppm"] == "0" and opening["delivered_ore"] == "0",
                f"unexpected synthetic trial opening: {opening}")
        require("tariff <percent>" in a.stdout and "--resume" in a.stdout,
                "commands and explicit resume path must be discoverable from launch")
        bad = run(f"tariff 21\nsave {a_bad}\nquit\n", *flags, "A", "--resume", a_start)
        require("forbids excessive import duties" in bad.stderr
                and a_bad.read_bytes() == a_start.read_bytes(),
                "over-ceiling tariff did not roll back the whole day")
        x = run(f"tariff 10\nstep 91\nsave {a_full}\nquit\n", *flags, "A")
        y = run(f"tariff 10\nstep 30\nsave {a_split}\nquit\n", *flags, "A")
        z = run(f"step 61\nsave {a_resumed}\nquit\n", *flags, "A", "--resume", a_split)
        no_duty = trade_status(run("step 92\nquit\n", *flags, "A"))
        end = trade_status(x)
        require(end["date"] == "2000-04-02" and end["tariff_ppm"] == "100000"
                and int(end["delivered_ore"]) > 70 and int(end["paid_duty"]) > 70
                and int(end["treasury"]) == 100 + int(end["paid_duty"])
                and no_duty["paid_duty"] == "0" and no_duty["treasury"] == "100"
                and no_duty["delivered_ore"] == end["delivered_ore"],
                f"real settled cargo or treasury duty incorrect: {end}; baseline: {no_duty}")
        require(not x.stderr and not y.stderr and not z.stderr
                and end == trade_status(z) and a_full.read_bytes() == a_resumed.read_bytes(),
                "same selected country and fixed policy failed exact snapshot replay")
        other = run("tariff 10\nstep 2\nquit\n", *flags, "B")
        b = trade_status(other)
        require(not other.stderr and b["country"] == "B" and b["max_tariff_ppm"] == "400000"
                and int(b["treasury"]) > 100 and int(b["delivered_ore"]) > 0,
                f"second country not independently playable: {b}")
        wrong = run("", *flags, "B", "--resume", a_split, success=False)
        require("selected trial country" in wrong.stderr,
                "trial accepted another country's authentic save")
        forged = d / "forged-selected-country.sav"
        forged.write_bytes(native_save_with_origin(
            a_split.read_bytes(), b"trial:tariff-v1:B", b"trial:tariff-v1:A"))
        run("", *flags, "B", "--resume", forged, success=False)
        run("", *flags, "A", "--resume", start, success=False)
        run("", "--tutorial", "market-v1", "--resume", a_split, success=False)
        run("", "--trial", "tariff-v1", success=False)
        panel_save = d / "한글 checkpoint.sav"
        panel_final = d / "panel final.sav"
        corrupt = d / "corrupt.sav"
        corrupt.write_bytes(b"not a snapshot")

        def panel(*commands):
            result = run("\n".join(commands) + "\n", "--panel-stdio")
            require(not result.stderr, result.stderr)
            replies = [json.loads(line) for line in result.stdout.splitlines()]
            require(len(replies) == len(commands), f"unexpected panel replies: {replies}")
            return replies

        replies = panel("status", "new A", "status", "tariff 21",
                        "tariff 10", "step 30", f"save {panel_save}",
                        f"load B {panel_save}", f"load A {corrupt}",
                        f"save {panel_save}", "step 61", f"save {panel_final}", "quit")
        require(replies[0] == {"ok": True, "status": None}, "empty panel status")
        opening = replies[1]["status"]
        require(opening["country"] == "A" and opening["date"] == "2000-01-01"
                and opening["treasury"] == 100 and opening["revenue"] == 0
                and opening["expenditure"] == 0 and opening["debt"] == 0
                and opening["max_tariff_ppm"] == 200000 and opening["law"] == "TRIAL-CUSTOMS-A"
                and opening["ore_price"] == 30 and opening["wars"] == [],
                f"unexpected opening panel projection: {opening}")
        require(replies[2] == replies[1] and not replies[3]["ok"]
                and replies[3]["status"] == opening, "status or rejected tariff advanced")
        split = replies[5]["status"]
        require(all(not r["ok"] and r["status"] == split and r["error"]
                    for r in replies[7:10]), "failed load or save replaced live state")
        finish = replies[10]["status"]
        require(finish["date"] == "2000-04-02" and finish["treasury"] == 194
                and finish["revenue"] == finish["paid_duty"] == 94
                and finish["delivered_ore"] == 93
                and finish["tariff_ppm"] == 100000 and finish["in_transit_ore"] > 0
                and finish["commit"] > opening["commit"],
                f"unexpected final panel projection: {finish}")
        require(panel_save.read_bytes() == a_split.read_bytes()
                and panel_final.read_bytes() == a_full.read_bytes(),
                "panel and CLI native save bytes differ")
        resumed = panel(f"load A {panel_save}", "step 61", "quit")
        require(resumed[0]["status"] == split and resumed[1]["status"] == finish,
                "panel resume differs from uninterrupted native simulation")
        require(panel("new B", "quit")[0]["status"]["max_tariff_ppm"] == 400000,
                "panel second country does not get its own statute")
    print("Native terminal tutorial: launch, real purchase, rollback and exact replay PASS")


if __name__ == "__main__":
    main()
