#!/usr/bin/env python3
"""Exercise the native historical-start path with independent fictional geography."""
from pathlib import Path
import subprocess
import sys
import tempfile


FIXTURE = """VIC25-HISTORY-1 FICTIONAL_TEST 1836 1
land 7 WEST AAA
land 12 WEST AAA
land 19 EAST BBB
ocean 30 - -
lake 31 - -
contact 7 12 1
contact 12 30 1
contact 19 30 1
contact 7 31 1
"""


def run(*args, script=""):
    return subprocess.run([sys.argv[1], *map(str, args)], input=script,
                          text=True, capture_output=True, timeout=30)


def success(result):
    assert result.returncode == 0 and not result.stderr, result


def rejected(result, message):
    assert result.returncode != 0 and message in result.stderr, result


def main():
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        start = work / "scenario start.txt"
        start.write_bytes(FIXTURE.replace("\n", "\r\n").encode("ascii"))
        saved, continued, replayed, tutorial = [
            work / f"{name}.sav" for name in ("start saved", "continued", "replayed", "tutorial")
        ]
        result = run("--history", start, script=(
            f"province 7\nprovince 12\nprovince 19\nstep 30\nsave {saved}\n"
            f"step 2\nsave {continued}\nquit\n"))
        success(result)
        for expected in (
            "History FICTIONAL_TEST | 1836-01-01 | land provinces 3",
            "History FICTIONAL_TEST | 1836-01-31 | land provinces 3",
            "History FICTIONAL_TEST | 1836-02-02 | land provinces 3",
            "Province 7 | country AAA | administration WEST | neighbors 12 | coasts -",
            "Province 12 | country AAA | administration WEST | neighbors 7 | coasts 30",
            "Province 19 | country BBB | administration EAST | neighbors - | coasts 30",
        ):
            assert expected in result.stdout, result
        rejected(run("--history", start, "--resume", saved), "mutually exclusive")
        start.unlink()  # Native history checkpoints do not depend on the source file.
        restored = run("--resume-history", saved, script=f"step 2\nprovince 12\nsave {replayed}\nquit\n")
        success(restored)
        assert "1836-02-02" in restored.stdout and "administration WEST" in restored.stdout
        assert replayed.read_bytes() == continued.read_bytes()
        # Resuming may not replace an existing checkpoint or treat a lake as land.
        unchanged = saved.read_bytes()
        guarded = run("--resume-history", saved, script=f"save {saved}\nprovince 31\nquit\n")
        assert guarded.returncode == 0 and "refusing to overwrite" in guarded.stderr
        assert "unknown land province" in guarded.stderr and saved.read_bytes() == unchanged
        # Contacts may precede their declarations; record order is not game state.
        header, *rows = FIXTURE.splitlines()
        start.write_text("\n".join([header, *reversed(rows)]) + "\n", encoding="ascii")
        reordered = work / "reordered.sav"
        success(run("--history", start, script=f"step 30\nsave {reordered}\nquit\n"))
        assert reordered.read_bytes() == unchanged
        start.unlink()
        rejected(run("--tutorial", "market-v1", "--resume", saved), "does not belong")
        success(run("--tutorial", "market-v1", script=f"save {tutorial}\nquit\n"))
        rejected(run("--resume-history", tutorial), "does not belong")
        rejected(run("--history", start), "regular file")
        rejected(run("--history", work), "regular file")
        for invalid, message in (
            (FIXTURE.replace("1836 1", "1836 13"), "invalid start month"),
            (FIXTURE.replace("land 12 WEST AAA", "land 12 WEST BBB"), "conflicting country"),
            (FIXTURE.replace("ocean 30 - -", "ocean 30 EAST BBB"), "no land ownership"),
            (FIXTURE + "contact 12 7 1\n", "repeated province contact"),
            (FIXTURE + "contact 19 99 1\n", "unknown province"),
            (FIXTURE + "ocean 7 - -\n", "duplicate province ID"),
            (FIXTURE + "route 12 19 1\n", "unknown record"),
            (FIXTURE.replace("land 7", "land +7"), "invalid province ID"),
            (FIXTURE.replace("contact 7 12 1", "contact 7 12 0"), "invalid shared sides"),
            (FIXTURE + " ", "malformed record"),
        ):
            start.write_text(invalid, encoding="ascii")
            rejected(run("--history", start), message)
        with start.open("wb") as big:
            big.truncate(8 * 1024 * 1024 + 1)
        rejected(run("--history", start), "8 MiB limit")


if __name__ == "__main__":
    main()
