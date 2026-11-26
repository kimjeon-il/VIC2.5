#!/usr/bin/env python3
"""Exercise the native historical-start path with independent fictional geography."""
import json
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
ocean 32 - -
ocean 33 - -
contact 7 12 1
contact 12 30 1
contact 19 30 1
contact 7 31 1
contact 30 32 1
contact 32 31 1
"""


def run(*args, script=""):
    return subprocess.run([sys.argv[1], *map(str, args)], input=script,
                          encoding="utf-8", capture_output=True, timeout=30)


def success(result):
    assert result.returncode == 0 and not result.stderr, result


def rejected(result, message):
    assert result.returncode != 0 and message in result.stderr, result


def history_panel(start, work, tutorial, legacy):
    start.write_text(FIXTURE.replace("1836 1", "1835 12")
                     .replace("land 19 EAST BBB", "land 19 EAST BBB\nland 23 INLAND CCC"),
                     encoding="ascii")
    save = work / "역사 저장.sav"
    bad = work / "bad.sav"
    bad.write_bytes(b"not a checkpoint")
    result = run("--panel-stdio", script="\n".join((
        "status", f"history-open {start}", "choose-country CCC", "choose-country AAA",
        "choose-country ZZZ", "tariff 10", "step 31", f"save {save}", f"save {save}",
        "new A", f"history-load {bad}", f"history-load {tutorial}", f"history-load {save}",
        f"history-load {legacy}", "quit", "",
    )))
    success(result)
    (initial, opened, inland, selected, unknown, policy, advanced, saved, exists,
     trial, broken, other, resumed, old, quit_reply) = map(json.loads, result.stdout.splitlines())
    assert initial == {"ok": True, "status": None}
    assert opened["ok"], opened
    assert opened["status"]["history"]["countries"] == ["AAA", "BBB", "CCC"]
    assert opened["status"]["country"] is None
    assert inland["status"]["history"]["territory"] == {"land": 1, "administrations": 1, "coastal": 0}
    assert selected["status"]["history"]["territory"] == {"land": 2, "administrations": 1, "coastal": 1}
    for reply in (unknown, policy):
        assert not reply["ok"] and reply["status"] == selected["status"], reply
    assert advanced["ok"] and advanced["status"]["date"] == "1836-01-01"
    assert saved["ok"] and not exists["ok"] and "overwrite" in exists["error"]
    assert trial["ok"] and "history" not in trial["status"]
    for reply in (broken, other):
        assert not reply["ok"] and reply["status"] == trial["status"], reply
    assert resumed == advanced and resumed["status"]["country"] == "AAA"
    assert old["ok"] and old["status"]["country"] is None and quit_reply == old
    # A fresh process restores the same state without reading the start file.
    start.unlink()
    fresh = run("--panel-stdio", script=f"history-load {save}\nquit\n")
    success(fresh)
    assert json.loads(fresh.stdout.splitlines()[0]) == resumed
    # CLI shares the new checkpoint reader; a re-save must retain the selection.
    cli_input = work / "CLI input.sav"
    cli_input.write_bytes(save.read_bytes())
    replay = work / "CLI replay.sav"
    success(run("--resume-history", cli_input, script=f"save {replay}\nquit\n"))
    assert replay.read_bytes() == save.read_bytes()
    damaged = bytearray(save.read_bytes())
    position = damaged.index(b"AAA")
    damaged[position:position + 3] = b"BBB"
    bad.write_bytes(damaged)
    corrupted = run("--panel-stdio", script=f"history-load {bad}\nquit\n")
    success(corrupted)
    assert not json.loads(corrupted.stdout.splitlines()[0])["ok"]


def local_food_market(work):
    start = work / "local market.txt"
    legacy_header, *rows = FIXTURE.splitlines()
    header = legacy_header.replace("HISTORY-1", "HISTORY-2")
    food = "food-start 7 2 240 F SAMPLE-CULTURE SAMPLE-FAITH 50 5 9 3"
    # Economy records may precede geography. Only the named land province has data.
    start.write_text("\n".join([header, food, *rows]) + "\n", encoding="ascii")
    saved, direct, replay = [work / (name + ".sav") for name in ("market-saved", "market-direct", "market-replay")]
    first = run("--panel-stdio", script="\n".join((
        f"history-open {start}", "choose-country BBB", "choose-country AAA",
        "step 1", f"save {saved}", "step 1", f"save {direct}", "step 3", "step 1", "quit", "",
    )))
    success(first)
    opened, peer, before, after, saved_reply, day_two, persisted, exhausted, no_refill, _ = map(
        json.loads, first.stdout.splitlines())
    assert opened["ok"], opened
    assert peer["ok"] and peer["status"]["history"]["economy"] is None
    def state(reply, cash, stock, seller):
        assert reply["ok"], reply
        economy = reply["status"]["history"]["economy"]
        assert economy == {"covered_provinces": 1, "people": 2, "household_cash": cash,
                           "food_stock": stock, "seller_cash": seller}, reply
        assert cash + seller == 53
        assert "treasury" not in reply["status"]
    state(before, 50, 9, 3)
    state(after, 40, 7, 13)
    state(day_two, 30, 5, 23)
    state(exhausted, 5, 0, 48)
    state(no_refill, 5, 0, 48)
    assert after == saved_reply and day_two == persisted
    start.unlink()
    restored = run("--panel-stdio", script=f"history-load {saved}\nstep 1\nsave {replay}\nquit\n")
    success(restored)
    assert list(map(json.loads, restored.stdout.splitlines()))[:2] == [after, day_two]
    assert direct.read_bytes() == replay.read_bytes()
    reversed_save = work / "market-reversed.sav"
    start.write_text("\n".join([header, *reversed(rows), food]) + "\n", encoding="ascii")
    reordered = run("--panel-stdio", script=f"history-open {start}\nchoose-country AAA\nstep 2\nsave {reversed_save}\nquit\n")
    success(reordered)
    assert reversed_save.read_bytes() == direct.read_bytes()
    for bad_header, bad_rows, message in (
        (header, rows + [food, food], "duplicate food-start"),
        (header, rows + [food.replace("food-start 7 ", "food-start 31 ")], "land province"),
        (header, rows + [food.replace("food-start 7 ", "food-start 79 ")], "land province"),
        (header, rows + [food.replace(" 240 ", " 1441 ")], "age"),
        (header, rows + [food.replace(" 50 ", " -1 ")], "cash"),
        (header, rows + [food.replace(" 5 ", " 0 ")], "price"),
        (header, rows + [food.replace(" 5 ", " 1000000001 ")], "price"),
        (header, rows + [food.replace(" F ", " X ")], "sex"),
        (legacy_header, rows + [food], "requires VIC25-HISTORY-2"),
    ):
        start.write_text("\n".join([bad_header, *bad_rows]) + "\n", encoding="ascii")
        rejected(run("--history", start), message)


def main():
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        start = work / "scenario start.txt"
        start.write_bytes(FIXTURE.replace("\n", "\r\n").encode("ascii"))
        saved, continued, replayed, tutorial = [
            work / f"{name}.sav" for name in ("start saved", "continued", "replayed", "tutorial")
        ]
        result = run("--history", start, script=(
            f"province 7\nprovince 12\nprovince 19\nwater 30\nwater 31\nwater 32\nwater 33\nstep 30\nsave {saved}\n"
            f"step 2\nsave {continued}\nquit\n"))
        success(result)
        water_lines = (
            "Water 30 | ocean | ocean neighbors 32",
            "Water 31 | lake | ocean neighbors -",
            "Water 32 | ocean | ocean neighbors 30",
            "Water 33 | ocean | ocean neighbors -",
        )
        for expected in (*water_lines,
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
        restored = run("--resume-history", saved, script=f"step 2\nprovince 12\nwater 30\nwater 31\nwater 32\nwater 33\nsave {replayed}\nquit\n")
        success(restored)
        assert "1836-02-02" in restored.stdout and "administration WEST" in restored.stdout
        assert all(line in restored.stdout.splitlines() for line in water_lines), restored
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
        history_panel(start, work, tutorial, saved)
        local_food_market(work)
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
