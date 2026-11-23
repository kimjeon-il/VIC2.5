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

        tax_start = d / "예약 전 소득세.sav"
        tax_pending = d / "예약 소득세.sav"
        tax_failed = d / "중복 거절 소득세.sav"
        tax_effective = d / "그대로 진행 소득세.sav"
        tax_replayed = d / "불러오기 소득세.sav"
        taxes = panel("new A", f"save {tax_start}", "income-tax 101",
                      "income-tax 15", f"save {tax_pending}", "income-tax 20",
                      f"save {tax_failed}", "step 30", "step 1",
                      f"save {tax_effective}", "quit")
        tax_open = taxes[0]["status"]
        require(tax_open.get("income_tax_ppm") == 100000
                and tax_open.get("pending_income_tax_ppm") is None,
                "the selected country must expose its actual opening income-tax rate")
        require(not taxes[2]["ok"] and taxes[2]["status"] == tax_open,
                "invalid income-tax order must not advance the trial")
        proposed = taxes[3]["status"]
        require(taxes[3]["ok"] and proposed["date"] == "2000-01-02"
                and proposed["income_tax_ppm"] == 100000
                and proposed["pending_income_tax_ppm"] == 150000
                and proposed["pending_income_tax_date"] == "2000-02-01",
                f"native tax proposal must change the later schedule, not today's rate: {taxes[3]}")
        require(not taxes[5]["ok"] and "duplicate" in taxes[5]["error"]
                and taxes[5]["status"] == proposed
                and tax_pending.read_bytes() == tax_failed.read_bytes(),
                "a duplicate pending income-tax bill must be rejected atomically")
        require(taxes[7]["status"]["date"] == "2000-02-01"
                and taxes[7]["status"]["income_tax_ppm"] == 100000
                and taxes[7]["status"]["pending_income_tax_ppm"] == 150000,
                "an unprocessed Feb 1 must still expose the effective-day tax order")
        effective = taxes[8]["status"]
        require(effective["date"] == "2000-02-02" and effective["income_tax_ppm"] == 150000
                and effective["pending_income_tax_ppm"] is None,
                f"lawful income tax must start on the processed effective day: {effective}")
        rerun = panel(f"load B {tax_pending}", f"load A {tax_pending}",
                      "step 31", f"save {tax_replayed}", "quit")
        require(not rerun[0]["ok"] and rerun[0]["status"] is None
                and rerun[1]["status"] == proposed and rerun[2]["status"] == effective
                and tax_effective.read_bytes() == tax_replayed.read_bytes(),
                "pending and effective policy failed exact selected-country native save replay")
        cli_tax_save = d / "터미널 소득세.sav"
        cli_tax = run(f"income-tax 15\nsave {cli_tax_save}\nquit\n", *flags, "A")
        terminal_tax_status = trade_status(cli_tax)
        require(not cli_tax.stderr
                and terminal_tax_status["pending_income_tax_ppm"] == "150000"
                and terminal_tax_status["pending_income_tax_date"] == "2000-02-01"
                and cli_tax_save.read_bytes() == tax_pending.read_bytes(),
                "the terminal action and graphical protocol must use the same native law path")
        b_tax = panel("new B", "income-tax 20", "step 31", "quit")
        require(b_tax[1]["status"]["country"] == "B"
                and b_tax[1]["status"]["pending_income_tax_ppm"] == 200000
                and b_tax[2]["status"]["income_tax_ppm"] == 200000,
                "second selected country must independently use its own native authority")

        budget_pending = d / "예산 예약.sav"
        budget_final = d / "예산 시행.sav"
        budget_replayed = d / "예산 재실행.sav"
        budgets = panel("new BUDGET-A", "spending-cap -1", "spending-cap 0",
                        f"save {budget_pending}", "spending-cap 3", "step 30",
                        "step 1", f"save {budget_final}", "quit")
        require(budgets[0]["ok"], "the selected country's funded public spending trial must open")
        budget_open = budgets[0]["status"]
        require(budget_open["country"] == "A" and budget_open["scenario"] == "budget-v1"
                and budget_open["treasury"] == 200 and budget_open["public_program_paid"] == 0
                and budget_open["civilian_spending_cap"] is None,
                f"wrong native spending trial opening: {budget_open}")
        require(not budgets[1]["ok"] and budgets[1]["status"] == budget_open,
                "negative cap changed the actual budget")
        pending = budgets[2]["status"]
        require(budgets[2]["ok"] and pending["public_program_paid"] == 5
                and pending["pending_civilian_spending_cap"] == 0
                and pending["pending_civilian_spending_date"] == "2000-02-01"
                and pending["civilian_spending_cap"] is None,
                "zero must be stored as a real next-month limit, not a missing value")
        require(not budgets[4]["ok"] and budgets[4]["status"] == pending,
                "duplicate active-month spending submission was not rejected atomically")
        boundary, settled = budgets[5]["status"], budgets[6]["status"]
        require(boundary["public_program_paid"] == 155
                and boundary["pending_civilian_spending_cap"] == 0
                and settled["public_program_paid"] == 155 and settled["treasury"] == 45
                and settled["civilian_spending_cap"] == 0
                and settled["pending_civilian_spending_cap"] is None
                and settled["peer_public_program_paid"] == 160,
                f"zero must stop only that government's real February payments: {settled}")
        budget_resume = panel(f"load A {budget_pending}", f"load BUDGET-B {budget_pending}",
                              f"load BUDGET-A {budget_pending}", "step 31",
                              f"save {budget_replayed}", "quit")
        require(not budget_resume[0]["ok"] and not budget_resume[1]["ok"]
                and budget_resume[2]["status"] == pending
                and budget_resume[3]["status"] == settled
                and budget_final.read_bytes() == budget_replayed.read_bytes(),
                "pending public spending did not survive exact country/scenario replay")
        positive = panel("new BUDGET-B", "spending-cap 3", "step 31", "step 4", "quit")
        require(positive[2]["status"]["public_program_paid"] == 158
                and positive[2]["status"]["civilian_spending_cap"] == 3
                and positive[3]["status"]["public_program_paid"] == 160
                and positive[3]["status"]["treasury"] == 40,
                "both programs must share one daily cap and stop at actual treasury reserve")
        budget_cli = run("spending-cap 3\nstep 31\nquit\n",
                         "--trial", "budget-v1", "--country", "B")
        require(not budget_cli.stderr
                and trade_status(budget_cli)["civilian_spending_cap"] == "3"
                and trade_status(budget_cli)["public_program_paid"] == "158",
                "standalone budget trial did not use the same native spending order")
        same_country = panel("new BUDGET-A", "tariff 10", "income-tax 15",
                             "spending-cap 2", "step 29", "quit")
        combined = same_country[4]["status"]
        require(all(reply["ok"] for reply in same_country)
                and combined["tariff_ppm"] == 100000 and combined["paid_duty"] > 0
                and combined["income_tax_ppm"] == 150000
                and combined["civilian_spending_cap"] == 2,
                "tariff, tax and public spending must operate together for the same trial government")
        legacy_spending = panel(f"load BUDGET-A {a_split}", "new A", "spending-cap 3", "quit")
        require(not legacy_spending[0]["ok"] and legacy_spending[0]["status"] is None
                and not legacy_spending[2]["ok"]
                and legacy_spending[2]["status"] == legacy_spending[1]["status"],
                "legacy tariff scenario must not create native spending accounts")

        build_pending = d / "공공 건설 접수.sav"
        build_done = d / "공공 건설 완공.sav"
        build_replay = d / "공공 건설 재실행.sav"
        builds = panel("new BUDGET-A", "step 25", "public-build",
                       f"save {build_pending}", "public-build", "step 1", "step 1",
                       "step 1", f"save {build_done}", "quit")
        offer = builds[0]["status"].get("public_construction")
        require(offer is not None and offer["state"] == "available"
                and offer["invoice"] == 20 and offer["treasury_reserve"] == 40
                and offer["steel_required"] == 2 and offer["contractor_steel"] == 0
                and offer["commissioned_capacity"] == 0,
                "the real trial must show the ordinary construction order and cost before purchase")
        queued = builds[2]["status"]
        require(builds[2]["ok"] and queued["country"] == "A"
                and queued["public_construction"]["state"] == "queued"
                and queued["public_construction"]["commissioned_capacity"] == 0
                and queued["public_construction"]["contractor_steel"] == 0
                and queued["treasury"] == 70,
                "accepting the real order must not invent a facility, material or paid work")
        require(not builds[4]["ok"] and builds[4]["status"] == queued,
                "duplicate order changed the actual pending public construction")
        bought = builds[5]["status"]
        require(bought["public_construction"]["contractor_steel"] == 2
                and bought["public_construction"]["supplier_receipts"] == 6
                and bought["public_construction"]["paid_work"] == 0
                and bought["treasury"] == 65,
                f"steel must be bought locally before construction: {bought}")
        paid, commissioned = builds[6]["status"], builds[7]["status"]
        require(paid["public_construction"]["state"] == "paid"
                and paid["public_construction"]["paid_work"] == 1
                and paid["public_construction"]["contractor_steel"] == 0
                and paid["public_construction"]["contractor_receipts"] == 20
                and paid["public_construction"]["commissioned_capacity"] == 0
                and paid["treasury"] == 40 and paid["public_program_paid"] == 140,
                f"real paid construction must consume steel and leave the treasury reserve: {paid}")
        require(commissioned["public_construction"]["state"] == "commissioned"
                and commissioned["public_construction"]["commissioned_capacity"] == 31
                and commissioned["treasury"] == 40
                and commissioned["public_program_paid"] == 140
                and commissioned["peer_public_program_paid"] == 145,
                "only following-day physical commissioning should add capacity, not free money")
        rerun = panel(f"load BUDGET-B {build_pending}", f"load BUDGET-A {build_pending}",
                      "step 3", f"save {build_replay}", "quit")
        require(not rerun[0]["ok"] and rerun[1]["status"] == queued
                and rerun[2]["status"] == commissioned
                and build_done.read_bytes() == build_replay.read_bytes(),
                "native paid construction did not replay exactly from its queued save")
        no_funds = panel("new BUDGET-B", "step 34", "public-build", "step 2", "quit")
        require(no_funds[1]["status"]["treasury"] == 40
                and not no_funds[2]["ok"] and "unfunded" in no_funds[2]["error"]
                and no_funds[2]["status"] == no_funds[1]["status"]
                and no_funds[3]["status"]["treasury"] == 40
                and no_funds[3]["status"]["public_construction"]["paid_work"] == 0
                and no_funds[3]["status"]["public_construction"]["commissioned_capacity"] == 0,
                "a government at its real treasury reserve cannot fund or commission the order")
        build_cli = run("step 25\npublic-build\nstep 3\nquit\n",
                        "--trial", "budget-v1", "--country", "A")
        require(not build_cli.stderr and trade_status(build_cli)["public_build_state"] == "commissioned"
                and trade_status(build_cli)["public_build_paid_work"] == "1"
                and trade_status(build_cli)["treasury"] == "40",
                "terminal and country panel must use the same public construction command")
        legacy_build = panel("new A", "public-build", "quit")
        require(not legacy_build[1]["ok"] and legacy_build[1]["status"] == legacy_build[0]["status"],
                "the old tariff trial must not receive a construction offer")

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
        require(replies[6]["ok"] and replies[6]["status"] == split,
                f"Unicode checkpoint save failed: {replies[6]}")
        require(replies[11]["ok"], f"final checkpoint save failed: {replies[11]}")
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
        industry_initial = d / "industry opening.sav"
        industry_full = d / "industry final.sav"
        industry_replayed = d / "industry replayed.sav"
        rows = panel("new TUT", "tariff 10", f"load TUT {panel_save}",
                     f"load TUT {start}", f"save {industry_initial}", "step 1",
                     "step 3", "step 1", f"save {industry_full}", "quit")
        def industry_values(status):
            require(status["scenario"] == "industry-v1" and status["country"] == "TUT"
                    and status["ore_per_batch"] == 1, f"wrong industry identity: {status}")
            return tuple(status[key] for key in (
                "ore_stock", "food_stock", "production_batches", "treasury",
                "expenditure", "public_works_cash", "ore_insufficient"))
        require(rows[0]["ok"], f"industry launch failed: {rows[0]}")
        initial = rows[0]["status"]
        require(initial["date"] == "2000-01-01" and industry_values(initial)
                == (8, 4, 0, 100, 0, 0, False), f"unexpected industry opening: {initial}")
        require(all(not row["ok"] and row["status"] == initial for row in rows[1:4]),
                "industry tariff or foreign checkpoint changed the live session")
        require(rows[4]["ok"] and rows[8]["ok"], "industry save failed")
        expected = [(6, 8, 2, 98, 2, 2, False), (0, 20, 8, 98, 2, 2, True),
                    (0, 20, 8, 98, 2, 2, True)]
        for idx, date, values in zip((5, 6, 7), ("2000-01-02", "2000-01-05", "2000-01-06"), expected):
            require(rows[idx]["ok"] and rows[idx]["status"]["date"] == date
                    and industry_values(rows[idx]["status"]) == values,
                    f"unexpected causal industry trace: {rows[idx]}")
        replay = panel(f"load TUT {industry_initial}", "step 5",
                       f"save {industry_replayed}", "quit")
        require(replay[0]["ok"] and replay[0]["status"] == initial
                and replay[1]["status"] == rows[7]["status"]
                and replay[2]["ok"] and industry_full.read_bytes() == industry_replayed.read_bytes(),
                "industry did not restore native policies and replay exactly")
        forged_industry = d / "forged industry origin.sav"
        forged_industry.write_bytes(native_save_with_origin(
            start.read_bytes(), b"panel:industry-v1:TUT"))
        forged_reply = panel("new TUT", f"load TUT {forged_industry}", "quit")
        require(not forged_reply[1]["ok"] and forged_reply[1]["status"] == initial,
                "industry accepted a forged outer-only scenario ID")
        cross = panel("new A", f"load A {industry_initial}", "quit")
        require(not cross[1]["ok"] and cross[1]["status"] == cross[0]["status"],
                "tariff trial loaded the industry checkpoint")
        run("", "--tutorial", "market-v1", "--resume", industry_initial, success=False)
    print("Native terminal tutorial: launch, real purchase, rollback and exact replay PASS")


if __name__ == "__main__":
    main()
