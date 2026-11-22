#!/usr/bin/env python3
"""Black-box smoke and deterministic checkpoint checks for the native tutorial."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def native_save_with_origin(raw, origin):
    # Only the outer Daily World origin metadata is changed. The nested
    # committed C/D/E/G/H snapshot and remaining journal bytes stay exact.
    def field_end(offset):
        return offset + 8 + struct.unpack_from("<Q", raw, offset)[0]

    offset = 0
    for _ in range(5):  # four header strings plus nested native payload
        offset = field_end(offset)
    offset += 24  # day index, commit counter, seed
    require(raw[offset + 8:field_end(offset)] == b"tutorial:market-v1",
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
            [str(binary), *map(str, flags)], input=commands, text=True,
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
    print("Native terminal tutorial: launch, real purchase, rollback and exact replay PASS")


if __name__ == "__main__":
    main()
