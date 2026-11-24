#!/usr/bin/env python3
"""Black-box smoke and deterministic checkpoint checks for the native tutorial."""
from calendar import monthrange
from datetime import date as civil_date
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

        # Private investment relies on earned sales, not free construction,
        # and the grown facility must pay a previously idle worker.
        private_work = d / "private paid work.sav"
        private_end = d / "private sold output.sav"
        private_replay = d / "private replay.sav"
        steps = panel("new EXPANSION-A", "step 1", *["step 1"] * 11, "quit")
        require(steps[0]["ok"], f"private expansion launch failed: {steps[0]}")
        trace = [row["status"] for row in steps[:-1]]
        require(all(row["ok"] for row in steps), "private expansion rejected an ordinary day")
        require(trace[0]["scenario"] == "expansion-v1" and
                trace[0]["private_expansion"] == {
                    "maker_cash": 30, "sales_receipts": 0, "builder_cash": 1000,
                    "builder_steel": 8, "paid_construction": 0, "commissioned_capacity": 0,
                    "new_worker_cash": 0, "new_worker_employed": False,
                    "expansion_batches": 0, "food_stock": 0,
                }, "the scenario grants a sale, job or completed plant at opening")
        ledgers = [row["private_expansion"] for row in trace]
        require(ledgers[1]["sales_receipts"] == 25 and ledgers[1]["maker_cash"] == 45
                and ledgers[1]["paid_construction"] == 0
                and ledgers[1]["commissioned_capacity"] == 0,
                "a wage-funded sale must settle first: without it the maker has only 20")
        paid = next((i for i, row in enumerate(ledgers) if row["paid_construction"]), None)
        traded = next((i for i, row in enumerate(ledgers) if row["expansion_batches"]), None)
        require(paid is not None and traded is not None and 1 < paid < traded,
                "the buyer-funded expansion did not progress from payment to new output")
        require(ledgers[paid]["paid_construction"] == 30 and ledgers[paid]["builder_steel"] == 7
                and ledgers[paid]["builder_cash"] == 1000 - 70 * paid + 30
                and trace[paid]["treasury"] == 8 * paid and trace[paid]["expenditure"] == 0
                and trace[paid]["debt"] == 0 and ledgers[paid]["commissioned_capacity"] == 0
                and not ledgers[paid]["new_worker_employed"],
                "paid work must transfer the actual invoice and consume steel before commissioning")
        event = ledgers[traded]
        # Both sold units happen to come from the new mill; the economy
        # gains one sale versus the established mill's prior paid day.
        formerly_sold = (ledgers[traded-1]["sales_receipts"] -
                         ledgers[traded-2]["sales_receipts"]) // 25
        now_sold = (event["sales_receipts"] - ledgers[traded-1]["sales_receipts"]) // 25
        require(event["commissioned_capacity"] == 31 and event["new_worker_employed"]
                and event["new_worker_cash"] == 9 and trace[traded]["treasury"] == 8 * traded + 1
                and event["expansion_batches"] == 2 and ledgers[traded-1]["food_stock"] == 0
                and event["food_stock"] == 0 and formerly_sold == 1 and now_sold == 2
                and event["sales_receipts"] - ledgers[traded-1]["sales_receipts"] == 50,
                "extra output must settle in the market, and the actual new payroll must remit tax")
        require(event["maker_cash"] == 30 + event["sales_receipts"] - 10 * traded
                - 10 - event["paid_construction"],
                "sales, wages and capex did not reconcile on the maker's actual wallet")
        first = panel("new EXPANSION-A", f"step {paid}", f"save {private_work}",
                      f"step {traded-paid}", f"save {private_end}", "quit")
        second = panel(f"load EXPANSION-A {private_work}", f"step {traded-paid}",
                       f"save {private_replay}", "quit")
        require(all(row["ok"] for row in first + second)
                and second[0]["status"] == trace[paid] and second[1]["status"] == trace[traded]
                and private_end.read_bytes() == private_replay.read_bytes(),
                "paid but uncommissioned construction changed across save/replay")
        foreign = panel("new EXPANSION-A", f"load EXPANSION-A {start}", "quit")
        require(not foreign[1]["ok"] and foreign[1]["status"] == foreign[0]["status"],
                "foreign checkpoint replaced the private expansion scenario")

        basket_open = d / "staples opening.sav"
        basket_paid = d / "staples after payroll.sav"
        basket_full = d / "staples full.sav"
        basket_replay = d / "staples replay.sav"
        samples = panel("new STAPLES-A", f"save {basket_open}", "step 1", f"save {basket_paid}",
                        "step 2", f"save {basket_full}", "quit")
        require(all(row["ok"] for row in samples), f"staples did not execute: {samples[0]}")
        first, settled = samples[0]["status"], samples[2]["status"]
        require(first["country"] == "A" and first["scenario"] == "staples-v1"
                and first["treasury"] == 0, "staples opened as the wrong scenario or gave away wages")
        goods = ("groceries", "clothes", "fittings")
        for case_name, wage, cash, fitting in (("necessities_only", 45, 0, 0),
                                               ("with_discretionary", 76, 1, 1)):
            case = first["household_staples"][case_name]
            require(case["wage_received"] == case["cash"] == 0
                    and all(case[good + suffix] == 0 for good in goods
                            for suffix in ("_made", "_stock", "_bought", "_receipts"))
                    and (case["grain_stock"], case["fabric_stock"],
                         case["boards_stock"], case["tools_stock"]) == (8, 8, 8, 8),
                    "matched opening must have equal raw inputs and no free finished goods")
            current = settled["household_staples"][case_name]
            require((current["wage_received"], current["cash"]) == (wage, cash)
                    and settled["treasury"] == 13 and settled["expenditure"] == 0,
                    "private wage settlement or taxes did not use existing ledgers")
            require((current["grain_stock"], current["fabric_stock"],
                     current["boards_stock"], current["tools_stock"]) == (7, 7, 7, 7)
                    and tuple(current[good + "_made"] for good in goods) == (1, 1, 1),
                    "finished goods must be produced from equal actual material supplies")
            require(tuple(current[good + "_bought"] for good in goods) == (1, 1, fitting)
                    and tuple(current[good + "_receipts"] for good in goods) == (20, 25, 30 * fitting)
                    and tuple(current[good + "_stock"] for good in goods) == (0, 0, 1 - fitting)
                    and cash + sum(current[good + "_receipts"] for good in goods) == wage,
                    "market purchasing must prioritize essentials over an available optional good")
        baskets_again = panel(f"load CAMPAIGN-A {basket_open}", f"load STAPLES-A {start}",
                              f"load STAPLES-A {basket_paid}", "step 2",
                              f"save {basket_replay}", "quit")
        require(not baskets_again[0]["ok"] and not baskets_again[1]["ok"]
                and baskets_again[2]["status"] == settled
                and baskets_again[3]["status"] == samples[4]["status"] and baskets_again[4]["ok"]
                and basket_replay.read_bytes() == basket_full.read_bytes(),
                "ordinary saves and staples crossed scenarios, or purchases did not replay exactly")

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

        pending_law = d / "law-pending.vic25"
        effective_law = d / "law-effective.vic25"
        replayed_law = d / "law-replayed.vic25"
        law = panel("new LAW-A", "bill-introduce", "step 2", "law-hire",
                    "bill-introduce", "bill-approve FAKE", "bill-promulgate", "quit")
        require(law[0]["ok"] and law[0]["status"]["scenario"] == "law-v1",
                "genuine elected law trial unavailable")
        require(not law[1]["ok"] and law[1]["status"] == law[0]["status"],
                "opening unelected government must not introduce this fixture bill")
        direct = panel("new LAW-A", "step 2", "income-tax 10", "tariff 10", "quit")
        require(all(not row["ok"] and row["status"] == direct[1]["status"] for row in direct[2:4]),
                "direct executive controls must not write to the elected trial")
        elected = law[2]["status"]["law_bill"]
        require(elected["threshold"] == 3 and
                sum(row["seats"] for row in elected["seats"]) == 5 and
                any(row["party"] == "WORKERS" and row["seats"] >= 3 for row in elected["seats"]) and
                any(row["party"] == "OTHER" and 0 < row["seats"] < 3 for row in elected["seats"]),
                "actual autonomous election did not earn chamber quorum")
        require(law[3]["ok"] and law[3]["status"]["workers"][0] ==
                   {"contract_wage": 31, "cash": 1} and law[3]["status"]["employer_cash"] == 999,
                "pre-law worker did not receive actual funded baseline pay")
        proposed = law[4]["status"]
        require(law[4]["ok"] and proposed["law_bill"]["phase"] == "submitted" and
                not proposed["law_bill"]["vote_ready"] and proposed["law_bill"]["next_review"] and
                proposed["minimum_monthly_contract_wage"] == 0 and
                not law[5]["ok"] and not law[6]["ok"],
                "proposal bypassed native political review or unauthorized command succeeded")
        # Same opening election/hire, now advance checkpoints from native dates
        # one at a time rather than presuming fixed bill success.
        def run_law(steps, *tail):
            return panel("new LAW-A", "step 2", "law-hire", "bill-introduce",
                         *(f"step {n}" for n in steps), *tail, "quit")
        steps = []
        for _ in range(12):
            last = run_law(steps)[-2]["status"]
            if last["law_bill"]["vote_ready"]:
                break
            due = last["law_bill"]["next_review"]
            require(due, "actual movement-backed wage bill never became ready")
            steps.append((civil_date.fromisoformat(due) - civil_date.fromisoformat(last["date"])).days + 1)
        else:
            raise AssertionError("actual movement failed to reach its voting stage")
        rows = run_law(steps, "bill-approve OTHER", "bill-approve FAKE",
                       "bill-approve WORKERS", "bill-promulgate",
                       f"save {pending_law}", "law-hire", f"save {effective_law}")
        wrong_party, fake_party, approved, promulgated, _, hired, _ = rows[-8:-1]
        require(not wrong_party["ok"] and not fake_party["ok"] and
                wrong_party["status"] == fake_party["status"] == last and
                approved["status"]["law_bill"]["phase"] == "approved",
                "actual elected quorum did not gate native approval")
        pending = promulgated["status"]
        require(pending["law_bill"]["phase"] == "promulgated" and
                pending["law_bill"]["effective_date"] == pending["date"] and
                pending["minimum_monthly_contract_wage"] == 0,
                "promulgation must wait for actual effective-day processing")
        after = hired["status"]
        processed = civil_date.fromisoformat(pending["date"])
        month_days = monthrange(processed.year, processed.month)[1]
        def due(wage):
            return wage * processed.day // month_days - wage * (processed.day - 1) // month_days
        require(hired["ok"] and after["minimum_monthly_contract_wage"] == 62 and
                after["law_bill"]["phase"] == "effective" and
                after["workers"][0]["contract_wage"] == 31 and
                after["workers"][1]["contract_wage"] == 62 and
                after["workers"][1]["cash"] == due(62) and
                pending["employer_cash"] - after["employer_cash"] == due(31) + due(62) and
                after["workers"][0]["cash"] - pending["workers"][0]["cash"] == due(31),
                "new effective contract failed real pay or rewrote the existing contract")
        control = run_law(steps, "step 2", "law-hire")[-2]["status"]
        require(control["workers"][1]["contract_wage"] == 31 and
                control["workers"][1]["cash"] == due(31) < after["workers"][1]["cash"],
                "no-law worker control did not isolate the effective wage increase")
        resumed_law = panel(f"load A {pending_law}", f"load LAW-A {pending_law}",
                            "law-hire", f"save {replayed_law}", "quit")
        require(not resumed_law[0]["ok"] and resumed_law[1]["status"] == pending and
                resumed_law[2]["status"] == after and
                effective_law.read_bytes() == replayed_law.read_bytes(),
                "law scenario cross-load, pending reopening, or exact paid replay failed")

        # The school trial shares real elected legislation and adds no
        # independent enrollment, classroom, or education wallet.
        school_pending = d / "education-pending.vic25"
        school_effective = d / "education-effective.vic25"
        school_replay = d / "education-replay.vic25"
        school_steps = []
        def run_school(choice, *tail):
            return panel(f"new {choice}", "step 2", "bill-introduce",
                         *(f"step {days}" for days in school_steps), *tail, "quit")
        for _ in range(12):
            current = run_school("EDU-A")[-2]
            require(current["ok"] and current["status"]["scenario"] == "education-v1",
                    "elected education trial unavailable")
            school_status = current["status"]
            if school_status["law_bill"]["vote_ready"]:
                break
            review = school_status["law_bill"]["next_review"]
            require(review, "native school bill failed before a vote")
            school_steps.append((civil_date.fromisoformat(review) -
                                 civil_date.fromisoformat(school_status["date"])).days + 1)
        else:
            raise AssertionError("native movement-backed school bill never became ready")
        school_tail = ("bill-approve WORKERS", "bill-promulgate")
        stayed_closed, closed_after = run_school("EDU-A", "step 2", "step 1")[-3:-1]
        rows = run_school("EDU-A", *school_tail, f"save {school_pending}", "step 1",
                          f"save {school_effective}")[-6:-1]
        passed, school_notice, _, school_next, _ = rows
        before = school_notice["status"]
        actual = school_next["status"]
        remote_before, remote_after = run_school("EDU-REMOTE-A", *school_tail, "step 1")[-3:-1]
        inactive = remote_after["status"]
        require(passed["ok"] and passed["status"]["law_bill"]["phase"] == "approved" and
                before["law_bill"]["effective_date"] == before["date"] and
                before["education"]["usable_slots"] == 0 and
                before["education"]["school_primary"] == before["education"]["comparison_primary"] == 0 and
                stayed_closed["status"]["date"] == before["date"],
                "schooling changed before real approval and processing its effective day")
        require(actual["date"] == inactive["date"] == closed_after["status"]["date"] and
                actual["law_bill"]["phase"] == inactive["law_bill"]["phase"] == "effective" and
                before["education"]["local_staff"] == actual["education"]["local_staff"] == 2 and
                inactive["education"]["local_staff"] == 0 and
                actual["education"]["usable_slots"] == 10 and
                actual["education"]["school_primary"] > actual["education"]["comparison_primary"] > 0 and
                inactive["education"]["usable_slots"] == 0 and
                inactive["education"]["school_primary"] == inactive["education"]["comparison_primary"] ==
                    actual["education"]["comparison_primary"] and
                closed_after["status"]["education"]["school_primary"] ==
                    closed_after["status"]["education"]["comparison_primary"] == 0,
                "actual aggregate access, law-only access, and forbidden control were not distinct")
        for earlier, later in ((before, actual), (remote_before["status"], inactive),
                               (stayed_closed["status"], closed_after["status"])):
            early, late = earlier["education"], later["education"]
            require(early["operator_cash"] - late["operator_cash"] ==
                    late["staff_cash"] - early["staff_cash"] > 0 and late["operator_cash"] > 0,
                    "matched same-date staff payroll did not move real operator funds")
        require(actual["education"]["staff_cash"] == inactive["education"]["staff_cash"] ==
                closed_after["status"]["education"]["staff_cash"],
                "matched controls paid different ordinary labor")
        school_reload = panel(f"load LAW-A {school_pending}", f"load EDU-REMOTE-A {school_pending}",
                              f"load EDU-A {school_pending}", "step 1", f"save {school_replay}", "quit")
        require(not school_reload[0]["ok"] and not school_reload[1]["ok"] and
                school_reload[2]["status"] == before and school_reload[3]["status"] == actual and
                school_replay.read_bytes() == school_effective.read_bytes(),
                "education checkpoint origin replay changed real payroll or demographic attainment")

        # Same A/B institutions, but a grounded rail link is out of
        # service before an alternate road becomes available. Observe actual
        # stock and industry ledgers, not the date/route label alone.
        outage = d / "rail outage.vic25"
        arriving = d / "road cargo.vic25"
        supply_complete = d / "supply recovery.vic25"
        supply_again = d / "supply replay.vic25"
        flow = panel("new SUPPLY-A", "step 20", "step 11", "step 1", "step 6",
                     f"save {outage}", "inspect B", "inspect A", "step 22", "step 1",
                     f"save {arriving}", "step 1", "step 1", "step 1",
                     f"save {supply_complete}", "quit")
        require(all(row["ok"] for row in flow), f"supply recovery did not execute: {flow}")
        (opening, rail, last_rail_cargo, rail_arrived, stalled, _, peer, _,
         reopening, dispatched, _, en_route_a, en_route_b, recovered) = [
            row["status"] for row in flow[:14]
        ]
        def supply(row):
            require(row["scenario"] == "campaign-v1", "supply is not the shared campaign")
            return row["supply_recovery"]
        require(supply(opening)["route"] == "rail" and supply(opening)["iron_batches"] == 0
                and opening["peer_country"] == "B", "the variant must begin with the shared rail economy")
        require(supply(rail)["iron_batches"] > 0 and supply(rail)["smelter_receipts"] > 0
                and rail["carrier_cash"] > 0 and supply(rail)["road_freight_paid"] == 0,
                "the ordinary railway never carried real inputs to paid domestic sales")
        require(last_rail_cargo["date"] == "2000-02-01"
                and supply(last_rail_cargo)["route"] == "closed"
                and last_rail_cargo["in_transit_ore"] == 1
                and rail_arrived["in_transit_ore"] == 0
                and rail_arrived["supplier_cash"] == last_rail_cargo["supplier_cash"]
                and rail_arrived["carrier_cash"] == last_rail_cargo["carrier_cash"]
                and supply(rail_arrived)["iron_batches"] == supply(last_rail_cargo)["iron_batches"] + 1
                and supply(rail_arrived)["smelter_receipts"] == supply(last_rail_cargo)["smelter_receipts"] + 40,
                "rail closure must honor an already paid shipment without booking a new departure")
        require(supply(stalled)["route"] == "closed" and supply(stalled)["ore_stock"] == 0
                and stalled["in_transit_ore"] == 0 and supply(stalled)["artisan_cash"] >= 40
                and supply(stalled)["smelter_worker_cash"] > supply(rail)["smelter_worker_cash"],
                "the ore shortage must leave the funded workforce and payable orders intact")
        require(peer["country"] == "B" and not peer["can_act"] and supply(peer) == supply(stalled),
                "peer inspection must read the very same disruption")
        require(reopening["date"] == "2000-03-01" and supply(reopening)["route"] == "road"
                and reopening["supplier_cash"] == stalled["supplier_cash"]
                and supply(reopening)["iron_batches"] == supply(stalled)["iron_batches"]
                and supply(reopening)["smelter_receipts"] == supply(stalled)["smelter_receipts"],
                "ore purchases and paid iron production did not actually stop through the gap")
        for row in (dispatched, en_route_a, en_route_b):
            require(supply(row)["road_freight_paid"] > 0 and supply(row)["road_in_transit_ore"] > 0
                    and supply(row)["ore_stock"] == 0
                    and supply(row)["iron_batches"] == supply(stalled)["iron_batches"]
                    and supply(row)["smelter_receipts"] == supply(stalled)["smelter_receipts"],
                    "paid road cargo was available to the smelter before the three-day trip ended")
        require(dispatched["supplier_cash"] - reopening["supplier_cash"] == 10
                and supply(dispatched)["road_freight_paid"] == 5
                and supply(dispatched)["road_in_transit_ore"] == 1
                and dispatched["carrier_cash"] == reopening["carrier_cash"],
                "the new road's real cargo purchase and freight did not settle to the right wallets")
        require(recovered["date"] == "2000-03-05"
                and supply(recovered)["iron_batches"] == supply(stalled)["iron_batches"] + 1
                and supply(recovered)["smelter_receipts"] == supply(stalled)["smelter_receipts"] + 40
                and supply(recovered)["artisan_cash"] == supply(en_route_b)["artisan_cash"] + 45 - 40,
                "physically restored ore did not become smelted and paid output")
        supply_replay = panel(f"load CAMPAIGN-A {outage}", f"load SUPPLY-A {arriving}",
                              "step 3", f"save {supply_again}", "quit")
        require(not supply_replay[0]["ok"] and supply_replay[1]["status"] == dispatched
                and supply_replay[2]["status"] == recovered and supply_replay[3]["ok"]
                and supply_again.read_bytes() == supply_complete.read_bytes(),
                "supply-recovery save did not preserve the real delayed cargo exactly")

        timber_close = d / "timber month opening.sav"
        timber_final = d / "timber month close.sav"
        timber_replay = d / "timber month replay.sav"
        opening_months = ("new CAMPAIGN-A", "step 2", "public-build-budget WORKERS",
                          "public-build", "step 27", "step 29", "step 31", "step 30")
        late_timber = panel(*opening_months, *["step 1"] * 31, f"save {timber_close}",
                            *["step 1"] * 30, f"save {timber_final}", "quit")
        require(all(row["ok"] for row in late_timber), "the ordinary timber campaign must run without intervention")
        may_close_index = len(opening_months)-1+31
        june_close_index = may_close_index+1+30
        may_open, june_open, july_open = (
            late_timber[i]["status"] for i in (len(opening_months)-1, may_close_index, june_close_index))
        require(all(row["status"]["supply_chain"][key] == 0
                    for row in late_timber[len(opening_months)-1:june_close_index+1]
                    for key in ("logger_wage_arrears", "mill_wage_arrears",
                                "workshop_wage_arrears", "timber_private_loans")),
                "the chain must not rely on new credit or a missed working-day payroll")
        require([row["date"] for row in (may_open, june_open, july_open)] ==
                ["2000-05-01", "2000-06-01", "2000-07-01"],
                "timber continuation must cover two complete later months")
        for earlier, later in ((may_open, june_open), (june_open, july_open)):
            b, a = earlier["supply_chain"], later["supply_chain"]
            def gain(field):
                return a[field] - b[field]
            require(gain("household_fittings_bought") >= 4,
                    "after starter inventories are gone, each full late month must have paid fittings sales")
            require(gain("logs_harvested") == 8 and gain("logger_receipts") == 32
                    and gain("logger_worker_cash") == gain("mill_worker_cash") == 31
                    and gain("wood_seller_cash") == 32 - 31,
                    "renewal and actual wood sales must support genuinely paid wood-industry wages")
            require(gain("mill_boards_produced") == gain("tools_batches") == 4
                    and gain("mill_wood_bought") == gain("toolworks_wood_bought") == 4
                    and gain("board_vendor_receipts") == 80
                    and gain("tools_sold") == 4,
                    "both real wood-production branches must replenish paid workshop inputs")
            require(gain("household_fittings_bought") == 4 and gain("fittings_sales_receipts") == 360
                    and gain("workshop_worker_cash") == 31
                    and gain("mill_cash") == 80 - 4 * 4 - 31
                    and gain("tool_buyer_cash") == 360 - 4 * 60 - 80 - 31,
                    "real purchases, paid workers and no prime-stock drawdown must reconcile")
        read_timber = panel(f"load CAMPAIGN-A {timber_close}", "step 30",
                            f"save {timber_replay}", "quit")
        require(all(row["ok"] for row in read_timber) and read_timber[0]["status"] == june_open
                and read_timber[1]["status"] == july_open
                and timber_replay.read_bytes() == timber_final.read_bytes(),
                "later timber and household cash must replay at an actual calendar boundary")

        steel_out = d / "steel order.sav"
        steel_done = d / "steel commissioned.sav"
        steel_replay = d / "steel commissioned replay.sav"
        steel_flow = panel("new STEEL-A", "step 1", "step 1", "public-build",
                           "public-build-budget WORKERS", "step 2", "public-build",
                           f"save {steel_out}", *["step 1"] * 5,
                           f"save {steel_done}", "quit")
        require(steel_flow[0]["ok"], f"steel trial launch failed: {steel_flow[0]}")
        opening = steel_flow[0]["status"]
        require(opening["scenario"] == "campaign-v1" and opening["peer_country"] == "B"
                and opening["public_construction"]["invoice"] == 52
                and opening["public_construction"]["treasury_reserve"] == 40
                and opening["public_construction"]["steel_purchase_cost"] == 48
                and opening["public_construction"]["contractor_steel"] == 0
                and all(opening["steel_chain"][key] == 0 for key in
                        ("coal_mined", "coal_stock", "mill_coal_stock", "mill_iron_stock",
                         "steel_produced", "steel_stock", "steel_receipts")),
                "no construction steel, processed material or coal may be granted at opening")
        require(not steel_flow[3]["ok"] and steel_flow[3]["status"] == steel_flow[2]["status"]
                and all(row["ok"] for i, row in enumerate(steel_flow) if i != 3),
                f"steel campaign improperly waived elected approval or rejected a paid step: {steel_flow}")
        paid_coal, paid_iron, mandate, queued = (steel_flow[i]["status"] for i in (1, 2, 4, 6))
        require(paid_coal["steel_chain"]["coal_mined"] == 1
                and paid_coal["steel_chain"]["coal_miner_cash"] == 1
                and paid_coal["steel_chain"]["coal_receipts"] == 4
                and paid_coal["steel_chain"]["mill_coal_stock"] == 1
                and paid_iron["steel_chain"]["mill_coal_stock"] == 1
                and paid_iron["steel_chain"]["mill_iron_stock"] == 1
                and paid_iron["supply_chain"]["smelter_receipts"] == 40
                and paid_iron["steel_chain"]["steel_produced"] == 0,
                "physical, paid coal and iron must precede any steel batch")
        require(mandate["public_construction"]["budget_approval"] == "approved"
                and mandate["public_construction"]["invoice_paid"] == 0
                and queued["public_construction"]["state"] == "queued"
                and queued["public_construction"]["commissioned_capacity"] == 0,
                "authority and an order are not payment or productive capacity")
        evidence = [steel_flow[i]["status"] for i in (1, 2, 4, 5, 6, 8, 9, 10, 11, 12)]
        first_steel = next((state for state in evidence if state["steel_chain"]["steel_produced"]), None)
        steel_transferred = next((state for state in evidence
                                if state["public_construction"]["contractor_steel"] == 2), None)
        paid = next((state for state in evidence if state["public_construction"]["state"] == "paid"), None)
        commissioned = steel_flow[12]["status"]
        require(first_steel and steel_transferred and paid
                and first_steel["date"] < steel_transferred["date"] < paid["date"] < commissioned["date"],
                "smelting, contractor purchase and paid commissioning must be real separate stages")
        work = first_steel["steel_chain"]
        require(work["steel_batches"] >= 1 and work["steel_produced"] == 2 * work["steel_batches"]
                and work["coal_mined"] >= 1 and work["coal_receipts"] >= 4
                and work["coal_miner_cash"] > 0 and work["mill_worker_cash"] > 0
                and work["mill_iron_used"] == work["mill_coal_used"] == work["steel_batches"]
                and first_steel["supply_chain"]["smelter_receipts"] >= 40,
                "steel must use bought coal and bought local pig iron, with real wages")
        require(steel_transferred["public_construction"]["supplier_receipts"] == 48
                and steel_transferred["public_construction"]["invoice_paid"] == 0
                and steel_transferred["steel_chain"]["steel_stock"] == 0
                and paid["public_construction"]["contractor_steel"] == 0
                and paid["steel_chain"]["builder_worker_cash"] ==
                    steel_transferred["steel_chain"]["builder_worker_cash"] + 1
                and paid["public_construction"]["invoice_paid"] == 52
                and paid["public_construction"]["contractor_receipts"] == 52
                and paid["public_construction"]["commissioned_capacity"] == 0
                and paid["treasury"] >= 40 and commissioned["public_construction"]["commissioned_capacity"] == 31,
                "actual construction must consume bought steel and pay the invoice before commission")
        no_order = panel("new STEEL-A", "step 2", "public-build-budget WORKERS", "step 5", "quit")[3]["status"]
        require(no_order["date"] == paid["date"] and no_order["treasury"] - paid["treasury"] == 52
                and no_order["expenditure"] == paid["expenditure"]
                and no_order["public_construction"]["invoice_paid"] == 0,
                "the treasury must really pay the invoice without recording it twice")
        resume_work = panel(f"load CAMPAIGN-A {steel_out}", f"load STEEL-A {steel_out}",
                            "step 5", f"save {steel_replay}", "quit")
        require(not resume_work[0]["ok"] and resume_work[1]["status"] == queued
                and resume_work[2]["status"] == commissioned and resume_work[3]["ok"]
                and steel_replay.read_bytes() == steel_done.read_bytes(),
                "the ordered steel project did not resume through identical paid commission")

        campaign_pending = d / "campaign-pending.vic25"
        campaign_inspected = d / "campaign-inspected.vic25"
        campaign_done = d / "campaign-done.vic25"
        campaign_replay = d / "campaign-replay.vic25"
        campaign = panel("new CAMPAIGN-A", "income-tax 13 WORKERS", "step 2",
                         "bill-introduce", "income-tax 13 OTHER", "step 26",
                         "income-tax 13 WORKERS", "step 1", f"save {campaign_pending}",
                         "inspect B", "bill-introduce", "income-tax 27 B-MAJORITY",
                         f"save {campaign_inspected}", "inspect A", "step 1",
                         "step 28", "step 1", f"save {campaign_done}", "quit")
        fresh = campaign[0]["status"]
        require(campaign[0]["ok"] and fresh["scenario"] == "campaign-v1" and
                fresh["player_country"] == "A" and fresh["country"] == "A" and
                not fresh["peer_is_player"] and fresh["peer_country"] == "B",
                "single common-world campaign is unavailable")
        require(not campaign[1]["ok"] and campaign[1]["status"] == fresh and
                not campaign[4]["ok"] and campaign[4]["status"] == campaign[3]["status"] and
                campaign[3]["status"]["law_bill"]["phase"] == "submitted",
                "campaign needs actual elections, an ordinary P2 bill and valid selected tax quorum")
        morning = campaign[7]["status"]
        require(morning["date"] == "2000-02-01" and morning["income_tax_ppm"] == 100000 and
                morning["law_bill"]["phase"] == "submitted" and
                morning["pending_income_tax_ppm"] == 130000 and
                morning["pending_income_tax_date"] == "2000-02-01" and
                morning["peer_pending_income_tax_ppm"] is None and morning["peer_debt"] == 100 and
                morning["peer_income_tax_ppm"] == 100000 and morning["in_transit_ore"] > 0 and
                0 < morning["delivered_ore"] + morning["in_transit_ore"] <= 2 and
                morning["supplier_cash"] > 0 and
                morning["carrier_cash"] > 0 and morning["public_program_paid"] > 0,
                "one campaign checkpoint needs pending elected tax, real paid freight and B's real pressure")
        inspect = campaign[9]["status"]
        require(inspect["country"] == "B" and inspect["player_country"] == "A" and
                not inspect["can_act"] and not inspect["campaign_tax_allowed"] and
                not campaign[10]["ok"] and not campaign[11]["ok"] and
                campaign[10]["status"] == campaign[11]["status"] == inspect and
                campaign_pending.read_bytes() == campaign_inspected.read_bytes() and
                campaign[13]["status"] == morning,
                "B inspection or a command issued while inspecting B changed authoritative state")
        feb = campaign[14]["status"]
        require(feb["date"] == "2000-02-02" and feb["income_tax_ppm"] == 130000 and
                feb["revenue"] - morning["revenue"] == 13 and
                feb["household_cash"] - morning["household_cash"] == 93 and
                feb["peer_pending_income_tax_ppm"] == 110000 and
                feb["peer_pending_income_tax_date"] == "2000-03-01" and
                feb["supply_chain"]["iron_batches"] > morning["supply_chain"]["iron_batches"],
                "ordinary resumed day needs actual arrival and a fresh independent B decision")
        mar_open = campaign[15]["status"]
        mar_closed = campaign[16]["status"]
        require(mar_open["date"] == "2000-03-01" and mar_closed["peer_income_tax_ppm"] == 110000 and
                mar_closed["peer_revenue"] - mar_open["peer_revenue"] == 11 and
                mar_closed["peer_household_cash"] - mar_open["peer_household_cash"] == 89,
                "B's own enacted tax did not reach its actual treasury and payroll")
        campaign_saved = panel(f"load CAMPAIGN-A {campaign_pending}", "step 1", "step 28",
                               "step 1", f"save {campaign_replay}", "quit")
        require(campaign_saved[0]["status"] == morning and campaign_saved[1]["status"] == feb and
                campaign_saved[3]["status"] == mar_closed and
                campaign_replay.read_bytes() == campaign_done.read_bytes(),
                "one loaded world lost a pending action, paid cargo or AI deterministic replay")

        require(not panel("new CAMPAIGN-B", "quit")[0]["ok"] and
                not panel(f"load LAW-A {campaign_pending}", "quit")[0]["ok"],
                "campaign cannot change player identity or load under an old trial")

        # One elected appropriation authorizes the same treasury to place
        # one ordinary order. It is not itself a contractor payment or work.
        approved_save = d / "campaign-construction-approved.vic25"
        finished_save = d / "campaign-construction-finished.vic25"
        replay_build = d / "campaign-construction-replayed.vic25"
        result = panel("new CAMPAIGN-A", "public-build-budget WORKERS", "step 2",
                       "public-build", "public-build-budget OTHER", "inspect B",
                       "public-build-budget WORKERS", "public-build", "inspect A",
                       "public-build-budget WORKERS", f"save {approved_save}",
                       "public-build-budget WORKERS", "public-build", "public-build",
                       "step 1", "step 1", "step 1", f"save {finished_save}", "quit")
        require(result[0]["ok"] and "public_construction" in result[0]["status"],
                "shared campaign has no real construction offer")
        require(not result[1]["ok"] and result[1]["status"] == result[0]["status"] and
                not result[3]["ok"] and not result[4]["ok"] and
                result[3]["status"] == result[4]["status"] == result[2]["status"],
                "unelected, unappropriated or minority construction changed the campaign")
        require("public_construction" not in result[5]["status"] and
                not result[6]["ok"] and not result[7]["ok"] and
                result[6]["status"] == result[7]["status"] == result[5]["status"],
                "B can see or spend A's construction appropriation")
        mandate = result[9]["status"]
        approved = mandate["public_construction"]
        require(result[9]["ok"] and approved["budget_approval"] == "approved" and
                approved["state"] == "available" and approved["invoice"] == 20 and
                approved["paid_work"] == approved["contractor_receipts"] == 0 and
                not result[11]["ok"] and result[11]["status"] == mandate,
                "appropriation must authorize exactly once without spending on construction")
        queued = result[12]["status"]
        material = result[14]["status"]["public_construction"]
        paid = result[15]["status"]
        completed = result[16]["status"]
        no_build = panel(f"load CAMPAIGN-A {approved_save}", "step 3", "quit")
        # A real cash outflow becomes an asset through D capitalization;
        # the native recurring-cost total must not count the invoice twice.
        require(result[12]["ok"] and queued["public_construction"]["state"] == "queued" and
                queued["public_construction"]["contractor_receipts"] == 0 and
                not result[13]["ok"] and result[13]["status"] == queued and
                material["state"] == "queued" and material["contractor_steel"] == 2 and
                material["supplier_receipts"] == 6 and material["invoice_paid"] == 0 and
                paid["date"] == no_build[1]["status"]["date"] and
                paid["public_construction"]["state"] == "paid" and
                paid["public_construction"]["paid_work"] == 1 and
                paid["public_construction"]["contractor_receipts"] == 20 and
                paid["public_construction"]["invoice_paid"] == 20 and
                paid["public_construction"]["contractor_steel"] == 0 and
                paid["public_construction"]["supplier_receipts"] == 6 and
                no_build[1]["status"]["treasury"] - paid["treasury"] == 20 and
                paid["expenditure"] == no_build[1]["status"]["expenditure"] and
                completed["public_construction"]["state"] == "commissioned" and
                completed["public_construction"]["commissioned_capacity"] == 31,
                "real same-treasury invoice, steel purchase, labor or commissioning failed")
        resumed_build = panel(f"load CAMPAIGN-A {approved_save}", "public-build",
                              "step 3", f"save {replay_build}", "quit")
        require(resumed_build[0]["status"] == mandate and resumed_build[1]["status"] == queued and
                resumed_build[2]["status"] == completed and
                replay_build.read_bytes() == finished_save.read_bytes(),
                "appropriation/order/payment state failed deterministic saved replay")

        # The commissioned project is not a product spawn. Follow paid
        # B freight, actual A smelting, local purchases and ordinary work.
        supply_checkpoint = d / "campaign-operating-chain.vic25"
        supply_completed = d / "campaign-operating-end.vic25"
        supply_replay = d / "campaign-operating-replay.vic25"
        timeline = panel("new CAMPAIGN-A", "step 1", "step 1",
                         "public-build-budget WORKERS", "public-build",
                         f"save {supply_checkpoint}", *["step 1"] * 18,
                         f"save {supply_completed}", "quit")
        require(all(row["ok"] for row in timeline), "ordinary campaign chain threw during a day")
        supply_start, transit = (row["status"] for row in timeline[:2])
        require("supply_chain" in supply_start, "shared campaign does not expose a real supply chain")
        initial = supply_start["supply_chain"]
        en_route = transit["supply_chain"]
        require(initial.get("logs_harvested") == 0 and initial["wood_seller_stock"] == 0 and
                initial.get("mill_boards_produced") == 0 and
                initial.get("logger_receipts") == 0,
                "new campaign did not start a genuine unharvested native timber chain")
        require(initial.get("board_vendor_stock") == 1 and
                initial.get("workshop_board_stock") == 1 and
                initial.get("board_vendor_receipts") == 0,
                "campaign has no independent seller with exactly one physical board")
        require(initial.get("household_fittings_bought") == 0 and
                initial.get("fittings_sales_receipts") == 0 and
                supply_start["household_cash"] == 0,
                "campaign has no fitting demand from an initially cashless wage household")
        require(initial.get("tool_buyer_cash", 0) > 0 and
                initial["tools_sold"] == initial["tool_buyer_batches"] ==
                initial["tool_buyer_tools_stock"] == initial["tool_buyer_output_stock"] == 0,
                "campaign has no independently funded domestic tool buyer")
        require(initial["ore_stock"] == initial["iron_batches"] == initial["tools_stock"] == 0 and
                transit["in_transit_ore"] > 0 and en_route["ore_stock"] == en_route["iron_batches"] == 0,
                "smelter used ore before the first paid cargo physically arrived")

        days = [row["status"] for row in timeline[2:-2]]
        first_iron = next((row for row in days if row["supply_chain"]["iron_batches"] > 0), None)
        first_tool = next((row for row in days if row["supply_chain"]["tools_batches"] > 0), None)
        require(first_iron and first_tool and first_iron["date"] < first_tool["date"] and
                first_iron["supplier_cash"] > 0 and first_iron["carrier_cash"] > 0 and
                first_tool["public_construction"]["state"] == "commissioned" and
                first_tool["supply_chain"]["tools_stock"] + first_tool["supply_chain"]["tools_sold"] > 0 and
                first_tool["supply_chain"]["smelter_receipts"] > initial["smelter_receipts"] and
                first_tool["supply_chain"]["logger_receipts"] > initial["logger_receipts"] and
                first_tool["supply_chain"]["factory_worker_cash"] > 0,
                "paid trade, intermediate iron and staffed public tools operation were not connected")
        require(all(row["supply_chain"]["tools_stock"] == row["supply_chain"]["factory_worker_cash"] == 0
                    for row in days if row["public_construction"]["state"] != "commissioned"),
                "tools or operating wages appeared before actual commissioning")
        later = days[-1]["supply_chain"]
        require(all(row["supply_chain"]["ore_stock"] + row["in_transit_ore"] <= 2
                    for row in (supply_start, transit, *days)) and later["iron_batches"] >= 21,
                "the ore route must replenish a bounded physical buffer without stalling smelting")
        unbuilt = panel("new CAMPAIGN-A", "step 22", "quit")[1]["status"]
        unbuilt_chain = unbuilt["supply_chain"]
        require(unbuilt["date"] == days[-1]["date"] and
                unbuilt_chain["iron_batches"] > 0 and
                unbuilt_chain["tools_stock"] == unbuilt_chain["factory_worker_cash"] == 0 and
                unbuilt_chain["smelter_receipts"] == initial["smelter_receipts"] and
                unbuilt_chain["toolworks_wood_bought"] == 0 and
                unbuilt_chain["household_fittings_bought"] == 0 and
                unbuilt_chain["tools_waiting_for"] == "construction",
                "without commissioning the other industries cannot replace tool production")
        tool_sale = next((row for row in days if row["supply_chain"]["tools_sold"]), None)
        downstream = next((row for row in days if row["supply_chain"]["tool_buyer_batches"]), None)
        before_tool_sale = days[days.index(tool_sale) - 1] if tool_sale in days[1:] else None
        require(tool_sale and downstream and before_tool_sale and
                first_tool["date"] <= tool_sale["date"] < downstream["date"] and
                tool_sale["supply_chain"]["tool_buyer_tools_stock"] == 1 and
                tool_sale["supply_chain"]["tool_buyer_output_stock"] == 0 and
                downstream["supply_chain"]["tool_buyer_output_stock"] == 0 and
                # The first tool contributes exactly 60 in addition to
                # the same ordinary 10 income tax; aggregate later revenue
                # can also include native raw wood resale by the treasury.
                tool_sale["revenue"] - before_tool_sale["revenue"] == 70 and
                days[-1]["revenue"] - unbuilt["revenue"] >= 60 * later["tools_sold"],
                "domestic shop did not pay 60 to the public maker for each actual tool")
        observed = downstream["supply_chain"]
        require(tool_sale["supply_chain"]["household_fittings_bought"] == 0 and
                tool_sale["household_cash"] > 0 and
                observed["household_fittings_bought"] == 1 and
                observed["fittings_sales_receipts"] == 90 and
                downstream["household_cash"] == tool_sale["household_cash"] and
                later["household_fittings_bought"] >= 3 and
                later["fittings_sales_receipts"] == 90 * later["household_fittings_bought"] and
                unbuilt["household_cash"] - days[-1]["household_cash"] == later["fittings_sales_receipts"],
                "harvested wood and milled boards did not enable an additional wage-funded sale")
        for row in days:
            chain = row["supply_chain"]
            require(chain["logs_harvested"] == chain["wood_seller_stock"] +
                    chain["mill_wood_bought"] + chain["toolworks_wood_bought"] and
                    chain["logger_receipts"] ==
                    chain["wood_price"] * (chain["mill_wood_bought"] + chain["toolworks_wood_bought"]) and
                    chain["mill_boards_produced"] + initial["board_vendor_stock"] +
                    initial["workshop_board_stock"] ==
                    chain["board_vendor_stock"] + chain["workshop_board_stock"] +
                    chain["tool_buyer_batches"] and
                    chain["board_vendor_receipts"] == 20 * (
                        chain["tool_buyer_batches"] + chain["workshop_board_stock"] -
                        initial["workshop_board_stock"]) and
                    chain["tools_sold"] == chain["tool_buyer_tools_stock"] + chain["tool_buyer_batches"] and
                    chain["tools_batches"] == chain["tools_stock"] + chain["tools_sold"],
                    "funded transfers and production did not conserve timber, boards and tools")
            ore_owned = chain["ore_stock"] + row["in_transit_ore"]
            ore_bought = ore_owned + chain["iron_batches"]
            require(ore_owned <= 2 and row["supplier_cash"] == 10 * ore_bought and
                    row["carrier_cash"] == 2 * ore_bought,
                    "paid ore must be in transit, held or used by the real smelter")
        require(later["mill_boards_produced"] >= 4 and later["household_fittings_bought"] >= 6,
                "preventing the excess purchase must free real boards and additional paid fittings")
        require(later["logger_worker_cash"] > 0 and later["mill_worker_cash"] > 0 and
                later["mill_wood_bought"] >= later["mill_boards_produced"] > 0 and
                later["toolworks_wood_bought"] >= later["tools_batches"] >= 3 and
                later["board_vendor_receipts"] >= 40 and
                later["tool_buyer_batches"] >= later["household_fittings_bought"] and
                later["tool_buyer_cash"] == initial["tool_buyer_cash"] +
                later["fittings_sales_receipts"] - 60 * later["tools_sold"] -
                later["board_vendor_receipts"] - (days[-1]["commit"] - supply_start["commit"]),
                "both staffed production routes must buy real harvests and support finished sales")

        restarted = panel(f"load CAMPAIGN-A {supply_checkpoint}", "inspect B", "inspect A",
                          "step 18", f"save {supply_replay}", "quit")
        require(all(row["ok"] for row in restarted) and
                "supply_chain" not in restarted[1]["status"] and
                restarted[0]["status"] == restarted[2]["status"] == timeline[5]["status"] and
                restarted[3]["status"] == days[-1] and
                supply_replay.read_bytes() == supply_completed.read_bytes(),
                "active trade/industry and the shortage did not resume identically after saving")
        # Once the mill reaches its finished-stock target, two full idle
        # months must not spend any more on this standing import.
        quiet = panel("new CAMPAIGN-A", "step 60", "step 31", "step 30", "quit")
        march, april, may = (row["status"] for row in quiet[1:4])
        require(all(row["ok"] for row in quiet) and
                [row["date"] for row in (march, april, may)] ==
                    ["2000-03-01", "2000-04-01", "2000-05-01"] and
                march["supply_chain"]["iron_batches"] >= 32 and
                all(row["supplier_cash"] == march["supplier_cash"] and
                    row["carrier_cash"] == march["carrier_cash"] and
                    row["supply_chain"]["iron_batches"] == march["supply_chain"]["iron_batches"] and
                    row["supply_chain"]["ore_stock"] == 2 and row["in_transit_ore"] == 0
                    for row in (march, april, may)),
                "an idle, fully supplied smelter must not pay for further ore or trips")
        mislabeled_campaign = d / "campaign-wrong-dataset.vic25"
        mislabeled_campaign.write_bytes(native_save_with_origin(
            supply_checkpoint.read_bytes(), b"panel:campaign-procurement-v2",
            old_origin=b"panel:campaign-timber-v4"))
        mismatch = panel(f"load CAMPAIGN-A {mislabeled_campaign}", "quit")
        require(not mismatch[0]["ok"],
                "a current snapshot with only its outer origin changed was resumed under old rules")
        purchased_checkpoint = d / "campaign-tool-purchased.vic25"
        until_tool_sale = tool_sale["commit"] - timeline[5]["status"]["commit"]
        purchased = panel(f"load CAMPAIGN-A {supply_checkpoint}", f"step {until_tool_sale}",
                          f"save {purchased_checkpoint}", "quit")
        require(all(row["ok"] for row in purchased) and purchased[1]["status"] == tool_sale,
                "tool purchase needs to remain in the shop before any downstream output")
        retail_checkpoint = d / "campaign-fitting-sold.vic25"
        until_fitting = downstream["commit"] - tool_sale["commit"]
        after_purchase = panel(f"load CAMPAIGN-A {purchased_checkpoint}", f"step {until_fitting}",
                               f"save {retail_checkpoint}", "quit")
        require(all(row["ok"] for row in after_purchase) and
                after_purchase[0]["status"] == tool_sale and
                after_purchase[1]["status"] == downstream,
                "resuming a paid physical tool should consume it once without repeating payment")
        retail_replayed = d / "campaign-fitting-replayed.vic25"
        until_end = days[-1]["commit"] - downstream["commit"]
        resumed_retail = panel(f"load CAMPAIGN-A {retail_checkpoint}", f"step {until_end}",
                               f"save {retail_replayed}", "quit")
        require(all(row["ok"] for row in resumed_retail) and
                resumed_retail[0]["status"] == downstream and
                resumed_retail[1]["status"] == days[-1] and
                retail_replayed.read_bytes() == supply_completed.read_bytes(),
                "resuming after household consumption must not repeat the fitting sale")

        campaign_reviews = []
        def campaign_law(*tail):
            return panel(f"load CAMPAIGN-A {campaign_done}",
                         *(f"step {n}" for n in campaign_reviews), *tail, "quit")
        for _ in range(12):
            waiting_vote = campaign_law()[-2]["status"]
            if waiting_vote["law_bill"]["vote_ready"]:
                break
            review = waiting_vote["law_bill"]["next_review"]
            require(review, "the campaign wage bill was not reviewed natively")
            campaign_reviews.append((civil_date.fromisoformat(review) -
                                     civil_date.fromisoformat(waiting_vote["date"])).days+1)
        else:
            raise AssertionError("shared campaign's actual wage bill never became vote-ready")
        refused, voted, pending_wage, active_wage = campaign_law("bill-approve OTHER",
            "bill-approve WORKERS", "bill-promulgate", "step 1")[-5:-1]
        require(not refused["ok"] and refused["status"] == waiting_vote and voted["ok"] and
                pending_wage["status"]["law_bill"]["effective_date"] == pending_wage["status"]["date"] and
                pending_wage["status"]["minimum_monthly_contract_wage"] == 0 and
                active_wage["status"]["law_bill"]["phase"] == "effective" and
                active_wage["status"]["minimum_monthly_contract_wage"] == 62 and
                active_wage["status"]["workers"][0]["contract_wage"] == 3100 and
                active_wage["status"]["peer_is_player"] is False and
                active_wage["status"]["supply_chain"]["iron_batches"] > morning["supply_chain"]["iron_batches"],
                "the shared world did not enact an actual voted ordinary wage statute")

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
