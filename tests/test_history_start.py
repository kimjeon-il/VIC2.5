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


def local_food_mill(work):
    start = work / "mill.txt"
    header, *rows = FIXTURE.splitlines()
    header = header.replace("HISTORY-1", "HISTORY-2")
    food = "food-start 7 2 240 F SAMPLE-CULTURE SAMPLE-FAITH 0 5 0 100"
    mill = "food-mill 7 1 372 31 2 1"
    start.write_text("\n".join([header, mill, *rows, food]) + "\n", encoding="ascii")
    save, direct, replay = [work / (name + ".sav") for name in ("mill-saved", "mill-direct", "mill-replay")]
    batch = run("--panel-stdio", script="\n".join((
        f"history-open {start}", "choose-country AAA", "step 1", f"save {save}",
        "step 1", f"save {direct}", "step 1", "quit", "",
    )))
    success(batch)
    opened, before, first, _, second, _, exhausted, _ = map(json.loads, batch.stdout.splitlines())
    assert opened["ok"], opened
    def stock(reply, grain, food_stock, household, seller):
        assert reply["ok"], reply
        status = reply["status"]
        eco = status["history"]["economy"]
        assert (eco["grain_stock"], eco["food_stock"], eco["household_cash"], eco["seller_cash"]) == (
            grain, food_stock, household, seller), reply
        assert eco["mills"] == 1 and eco["people"] == 2 and eco["covered_provinces"] == 1
        assert household + seller == 100
        assert "treasury" not in status
    stock(before, 2, 0, 0, 100)
    stock(first, 1, 1, 2, 98)
    stock(second, 0, 2, 4, 96)
    # Existing contracts still pay wages even on a day without production.
    stock(exhausted, 0, 0, 6, 94)
    start.unlink()
    resumed = run("--panel-stdio", script=f"history-load {save}\nstep 1\nsave {replay}\nquit\n")
    success(resumed)
    assert list(map(json.loads, resumed.stdout.splitlines()))[:2] == [first, second]
    assert replay.read_bytes() == direct.read_bytes()
    cli = work / "mill-cli.sav"
    success(run("--resume-history", save, script=f"step 1\nsave {cli}\nquit\n"))
    assert cli.read_bytes() == direct.read_bytes()
    for invalid, message in (
        ([food, mill, mill], "duplicate food-mill"),
        ([mill], "matching food-start"),
        ([food, mill.replace("7 1", "7 3", 1)], "food-mill employment"),
        ([food.replace(" 240 ", " 60 "), mill], "food-mill employment"),
    ):
        start.write_text("\n".join([header, *rows, *invalid]) + "\n", encoding="ascii")
        rejected(run("--history", start), message)


def executive_tax(work):
    start = work / "executive.txt"
    header, *rows = FIXTURE.splitlines()
    header = header.replace("HISTORY-1", "HISTORY-2")
    economy = ["food-start 7 2 240 F SAMPLE-CULTURE SAMPLE-FAITH 0 5 0 10000",
               "food-mill 7 1 1922 31 100 1"]
    office = "executive-start AAA SAMPLE-HEAD SAMPLE-MINISTER 50 10"
    rows.append("land 39 OTHER CCC")
    other_office = "executive-start CCC OTHER-HEAD OTHER-MINISTER 25 0"

    def write(*records):
        start.write_text("\n".join([header, *rows, *records]) + "\n", encoding="ascii")
    write(*economy, office, other_office)
    pending = work / "tax-pending.sav"
    direct = work / "tax-direct.sav"
    replay = work / "tax-replay.sav"

    def panel(*commands):
        result = run("--panel-stdio", script="\n".join((*commands, "quit", "")))
        success(result)
        return list(map(json.loads, result.stdout.splitlines()))[:-1]

    prefix = (f"history-open {start}", "choose-country BBB", "income-tax 50",
              "choose-country AAA", "step 29")
    opened, peer, unavailable, selected, january, proposed, _, boundary, changed, _ = panel(
        *prefix, "income-tax 50", f"save {pending}", "step 1", "step 1", f"save {direct}")
    assert opened["ok"], opened
    assert peer["ok"] and "treasury" not in peer["status"]
    assert not unavailable["ok"] and unavailable["status"] == peer["status"]
    assert selected["status"]["treasury"] == 50 and selected["status"]["income_tax_ppm"] == 100_000
    assert january["status"]["date"] == "1836-01-30"
    assert proposed["status"]["date"] == "1836-01-31"
    assert proposed["status"]["income_tax_ppm"] == 100_000
    assert proposed["status"]["pending_income_tax_ppm"] == 500_000
    assert proposed["status"]["pending_income_tax_date"] == "1836-02-01"
    assert boundary["status"]["date"] == "1836-02-01"
    assert boundary["status"]["income_tax_ppm"] == 100_000
    assert changed["status"]["date"] == "1836-02-02"
    assert changed["status"]["income_tax_ppm"] == 500_000
    assert changed["status"]["pending_income_tax_ppm"] is None
    separate = panel(*prefix, "income-tax 50", "step 2", "choose-country CCC")[-1]["status"]
    assert separate["treasury"] == 25 and separate["revenue"] == 0
    assert separate["income_tax_ppm"] == 0 and separate["pending_income_tax_ppm"] is None
    control = panel(*prefix, "step 3")[-1]["status"]
    actual = changed["status"]
    assert control["date"] == actual["date"] and control["income_tax_ppm"] == 100_000
    extra_tax = actual["treasury"] - control["treasury"]
    assert extra_tax > 0 and actual["revenue"] - control["revenue"] == extra_tax
    actual_food, control_food = actual["history"]["economy"], control["history"]["economy"]
    assert control_food["household_cash"] - actual_food["household_cash"] == extra_tax
    assert control_food["food_stock"] == actual_food["food_stock"]
    assert actual["treasury"] + actual_food["household_cash"] + actual_food["seller_cash"] == 10050
    start.unlink()
    loaded, same, _ = panel(f"history-load {pending}", "step 2", f"save {replay}")
    assert loaded == proposed and same == changed
    assert replay.read_bytes() == direct.read_bytes()

    for offices, message in (
        ([office, office], "duplicate executive-start"),
        ([office.replace("AAA", "ZZZ")], "executive-start requires a country with land"),
        ([office.replace("AAA", "BBB")], "food-mill requires a modeled government"),
        ([office.replace("50 10", "50 101")], "invalid executive-start income tax"),
    ):
        write(*economy, *offices)
        rejected(run("--history", start), message)


def grain_freight(work):
    start = work / "grain-freight.txt"
    header, *rows = FIXTURE.splitlines()
    header = header.replace("HISTORY-1", "HISTORY-2")
    supply = "grain-supply 12 4 3"
    road = "grain-road 12 7 31 2 3 FIXTURE-ROAD"
    economy = ["food-start 7 2 240 F SAMPLE-CULTURE SAMPLE-FAITH 100 8 0 200",
               "food-mill 7 1 372 31 1 3", supply]
    pending, final, replay = (work / f"grain-{name}.sav" for name in ("pending", "final", "replay"))

    def write(records, geography=rows):
        start.write_text("\n".join([header, *geography, *records]) + "\n", encoding="ascii")

    def panel(*commands):
        result = run("--panel-stdio", script="\n".join((*commands, "quit", "")))
        success(result)
        replies = [json.loads(line) for line in result.stdout.splitlines()][:-1]
        assert all(reply["ok"] for reply in replies), replies
        return [reply["status"] for reply in replies]

    def local(status):
        return status["history"]["economy"]

    prefix = (f"history-open {start}", "choose-country AAA")
    write(economy)
    *_, without = panel(*prefix, *("step 1",) * 6)
    assert local(without)["food_stock"] == 0 and local(without).get("grain_in_transit", 0) == 0
    write([*economy, road])
    _, _, first, depleted, dispatched, _, hold_a, hold_b, resumed, _ = panel(
        *prefix, "step 1", "step 1", "step 1", f"save {pending}", "step 1",
        "step 1", "step 1", f"save {final}")
    assert (local(first)["grain_stock"], local(first)["food_stock"]) == (0, 1)
    assert local(depleted)["food_stock"] == 0
    for held in (dispatched, hold_a, hold_b):
        assert local(held)["food_stock"] == 0 and local(held)["grain_stock"] == 0
        assert local(held)["grain_in_transit"] == 1
    assert local(resumed)["food_stock"] == 1
    assert local(resumed).get("grain_in_transit", 0) == 0
    assert local(resumed)["seller_cash"] - local(without)["seller_cash"] == 2 * 8 - 3 - 2
    assert local(without)["household_cash"] - local(resumed)["household_cash"] == 2 * 8
    start.unlink()
    loaded, same, _ = panel(f"history-load {pending}", "step 3", f"save {replay}")
    assert loaded == dispatched and same == resumed
    assert replay.read_bytes() == final.read_bytes()

    for records, geography, error in (
        ([*economy, road], [row for row in rows if not row.startswith("contact 7 12 ")], "grain-road requires adjacent land"),
        ([*economy, road, road], rows, "duplicate grain-road"),
        ([*economy[:-1], road], rows, "grain-road requires a grain supplier and food mill"),
        ([*economy, road.replace("2 3 FIXTURE", "2 0 FIXTURE")], rows, "invalid grain-road transit days"),
    ):
        write(records, geography)
        rejected(run("--history", start), error)


def bundled_trial(work):
    start = Path(__file__).resolve().parents[1] / "data/scenarios/fictional-grain-and-tax.txt"
    save = work / "bundled-trial.sav"
    result = run("--panel-stdio", script="\n".join((
        f"history-open {start}", "choose-country MILL-LAND", "step 3", "step 3",
        "step 23", "income-tax 50", f"save {save}", "step 2", "quit", "",
    )))
    success(result)
    replies = list(map(json.loads, result.stdout.splitlines()))
    assert all(reply["ok"] for reply in replies), replies
    opening, chosen, moving, arrived, _, pending, _, applied, _ = (r["status"] for r in replies)
    assert opening["history"]["dataset"] == "FICTIONAL_GRAIN_AND_TAX"
    assert chosen["treasury"] == 50 and chosen["income_tax_ppm"] == 100_000
    assert moving["date"] == "1840-01-04"
    assert moving["history"]["economy"]["grain_in_transit"] == 1
    assert arrived["date"] == "1840-01-07" and arrived["history"]["economy"]["food_stock"] == 1
    assert "grain_in_transit" not in arrived["history"]["economy"]
    assert pending["pending_income_tax_date"] == "1840-02-01"
    assert applied["date"] == "1840-02-02" and applied["income_tax_ppm"] == 500_000
    assert applied["treasury"] == 87 and applied["revenue"] == 37
    assert save.is_file()
    restored = run("--panel-stdio", script=f"history-load {save}\nstep 2\nquit\n")
    success(restored)
    assert list(map(json.loads, restored.stdout.splitlines()))[:2] == [replies[5], replies[7]]


def staple_price_shock(work):
    start = work / "prices.txt"
    source = Path(__file__).resolve().parents[1] / "data/scenarios/fictional-grain-and-tax.txt"
    text = source.read_text(encoding="ascii")
    text = text.replace("100 8 0 800", "10000 100 0 800 100 104")
    save = work / "price-shock.sav"

    def panel(*commands):
        result = run("--panel-stdio", script="\n".join((*commands, "quit", "")))
        success(result)
        replies = list(map(json.loads, result.stdout.splitlines()))[:-1]
        assert all(reply["ok"] for reply in replies), replies
        return [reply["status"] for reply in replies]

    prefix = (f"history-open {start}", "choose-country MILL-LAND")
    start.write_text(text.replace("2 3 SAMPLE-ROAD", "2 1 SAMPLE-ROAD"), encoding="ascii")
    _, _, regular = panel(*prefix, "step 4")
    start.write_text(text, encoding="ascii")
    _, _, dispatch, delayed, _, cap, recovered = panel(
        *prefix, "step 3", "step 1", f"save {save}", "step 1", "step 1")
    def economy(status):
        return status["history"]["economy"]
    def price(status):
        return economy(status)["local_food_prices"]
    assert regular["date"] == delayed["date"] == "1840-01-05"
    assert price(dispatch) == {"minimum": 102, "maximum": 102, "purchase_gap": 2}
    assert price(regular) == {"minimum": 101, "maximum": 101, "purchase_gap": 0}
    assert price(delayed) == {"minimum": 103, "maximum": 103, "purchase_gap": 2}
    assert economy(regular)["food_stock"] == 1
    assert economy(delayed)["food_stock"] == 0 and economy(delayed)["grain_in_transit"] == 1
    assert price(cap)["maximum"] == 104
    assert recovered["date"] == "1840-01-07"
    assert price(recovered) == {"minimum": 103, "maximum": 103, "purchase_gap": 0}
    assert economy(recovered)["food_stock"] == 1
    start.unlink()
    assert panel(f"history-load {save}", "step 2") == [delayed, recovered]

    unavailable = "\n".join(line for line in text.splitlines()
                            if not line.startswith("grain-road ")) + "\n"
    start.write_text(unavailable, encoding="ascii")
    _, _, limited = panel(*prefix, "step 20")
    assert price(limited) == {"minimum": 104, "maximum": 104, "purchase_gap": 2}
    assert economy(limited)["food_stock"] == 0

    # Physical need is not price pressure when no customer can pay.
    no_wallet = "\n".join(line for line in text.splitlines()
                          if line.startswith(("VIC25-", "land ", "food-start")))
    no_wallet = no_wallet.replace("10000 100", "0 100")
    start.write_text(no_wallet + "\n", encoding="ascii")
    _, _, poor = panel(*prefix, "step 3")
    assert price(poor) == {"minimum": 100, "maximum": 100, "purchase_gap": 0}
    assert economy(poor)["food_stock"] == economy(poor)["household_cash"] == 0

    for tail, message in (("100", "malformed food-start price band"),
                          ("101 104", "food-start price outside band"),
                          ("100 99", "food-start price outside band"),
                          ("0 104", "invalid food-start price floor")):
        start.write_text(text.replace("800 100 104", "800 " + tail), encoding="ascii")
        rejected(run("--history", start), message)


def staffed_grain_farm(work):
    start = work / "farm.txt"
    sample = Path(__file__).resolve().parents[1] / "data/scenarios/fictional-grain-and-tax.txt"
    base = sample.read_text(encoding="ascii")
    base = base.replace("grain-supply 22 24 3", "grain-supply 22 0 3")
    base += "food-start 22 1 240 F FARM-CULTURE FARM-FAITH 0 1 0 0\n"
    farm = "grain-farm 22 1 31 50 31 2 31"
    save = work / "farm-continued.sav"

    def write(text=base, record=farm):
        start.write_text(text + record + "\n", encoding="ascii")

    def panel(*commands):
        result = run("--panel-stdio", script="\n".join((*commands, "quit", "")))
        success(result)
        replies = [json.loads(line) for line in result.stdout.splitlines()][:-1]
        assert all(reply["ok"] for reply in replies), replies
        return [reply["status"] for reply in replies]

    def economy(status):
        return status["history"]["economy"]

    def actual_farm(status):
        return economy(status)["grain_farm"]

    prefix = (f"history-open {start}", "choose-country MILL-LAND")
    write()
    _, opening, first, in_transit, arrived, _, month_end, across_month = panel(
        *prefix, "step 1", "step 2", "step 3", f"save {save}", "step 25", "step 4")
    assert actual_farm(opening) == {"sites": 1, "harvested": 0, "saleable": 0, "cash": 50}
    assert actual_farm(first) == {"sites": 1, "harvested": 1, "saleable": 1, "cash": 49}
    assert economy(first)["household_cash"] == 96 and first["treasury"] == 51
    assert economy(in_transit)["grain_in_transit"] == 1 and economy(in_transit)["food_stock"] == 0
    assert actual_farm(in_transit)["cash"] == 50  # Three wages and one paid grain sale.
    assert economy(arrived)["food_stock"] == 1
    assert month_end["date"] == "1840-02-01" and actual_farm(month_end)["harvested"] > 2
    assert across_month["date"] == "1840-02-05"
    assert actual_farm(across_month)["harvested"] > actual_farm(month_end)["harvested"]
    start.unlink()
    assert panel(f"history-load {save}", "step 29") == [arrived, across_month]

    no_road = "\n".join(line for line in base.splitlines()
                        if not line.startswith("grain-road ")) + "\n"
    write(no_road)
    _, _, isolated = panel(*prefix, "step 6")
    assert actual_farm(isolated)["harvested"] == 1 and actual_farm(isolated)["saleable"] == 1
    assert actual_farm(isolated)["cash"] == 44 and economy(isolated)["food_stock"] == 0

    for record, text in (
        (farm.replace("31 50", "31 0"), base),  # No payroll funding.
        (farm.replace("1 31 50", "1 93 50"), base),  # Wage equals price.
    ):
        write(text, record)
        _, _, frozen = panel(*prefix, "step 6")
        assert actual_farm(frozen)["harvested"] == 0 and economy(frozen)["food_stock"] == 0
        assert economy(frozen).get("grain_in_transit", 0) == 0

    for record, text, message in (
        (farm, base.replace("food-start 22", "food-start 33"), "grain-farm requires a local food-start and grain-supply"),
        (farm.replace("22 1 31", "22 2 31"), base, "invalid grain-farm employment"),
        ("", base, "zero initial grain requires a grain-farm"),
    ):
        if record:
            write(text, record)
        else:
            start.write_text(text, encoding="ascii")
        rejected(run("--history", start), message)


def cross_border_grain(work):
    start = work / "trade.txt"
    sample = Path(__file__).resolve().parents[1] / "data/scenarios/fictional-grain-and-tax.txt"
    base = sample.read_text(encoding="ascii")
    base = base.replace("land 22 FARM-DISTRICT MILL-LAND", "land 22 FARM-DISTRICT FIELD-LAND")
    base = base.replace("grain-supply 22 24 3", "grain-supply 22 24 6")
    base = base.replace("SAMPLE-MINISTER 50 10", "SAMPLE-MINISTER 50 0")
    base += "executive-start FIELD-LAND FIELD-HEAD FIELD-MINISTER 20 0\n"
    assessed = base + "grain-tariff MILL-LAND 50\n"
    save = work / "customs-paid.sav"

    def panel(*commands):
        result = run("--panel-stdio", script="\n".join((*commands, "quit", "")))
        success(result)
        return list(map(json.loads, result.stdout.splitlines()))[:-1]

    def state(reply):
        assert reply["ok"], reply
        return reply["status"]

    def food(status):
        return status["history"]["economy"]

    prefix = (f"history-open {start}", "choose-country MILL-LAND")
    start.write_text(assessed, encoding="ascii")
    opened, opening, departure, _, raised, arrived, stalled, exporter, forbidden = panel(
        *prefix, "step 3", f"save {save}", "tariff 100", "step 2", "step 3",
        "choose-country FIELD-LAND", "tariff 50")
    state(opened)
    opening, departure, raised, arrived, stalled, exporter = map(state, (
        opening, departure, raised, arrived, stalled, exporter))
    assert opening["grain_trade"] == {"tariff_ppm": 500_000, "duties_paid": 0}
    assert departure["grain_trade"]["duties_paid"] == 3
    assert departure["treasury"] == 53 and departure["revenue"] == 3
    assert food(departure)["grain_in_transit"] == 1
    assert raised["grain_trade"] == {"tariff_ppm": 1_000_000, "duties_paid": 3}
    assert food(raised)["grain_in_transit"] == 1
    assert food(arrived)["food_stock"] == 1 and arrived["grain_trade"]["duties_paid"] == 3
    assert food(stalled)["food_stock"] == 0 and food(stalled).get("grain_in_transit", 0) == 0
    assert stalled["grain_trade"]["duties_paid"] == 3
    assert exporter["treasury"] == 20 and "grain_trade" not in exporter
    assert not forbidden["ok"] and forbidden["status"] == exporter
    start.unlink()
    reloaded, same, same_arrival = map(state, panel(f"history-load {save}", "tariff 100", "step 2"))
    assert (reloaded, same, same_arrival) == (departure, raised, arrived)

    start.write_text(base + "grain-tariff MILL-LAND 100\n", encoding="ascii")
    *_, blocked = panel(*prefix, "step 6")
    blocked = state(blocked)
    assert blocked["grain_trade"]["duties_paid"] == 0 and blocked["treasury"] == 50
    assert food(blocked)["food_stock"] == 0 and food(blocked).get("grain_in_transit", 0) == 0
    start.write_text(base, encoding="ascii")  # Open trade without an editable duty rule.
    *_, no_duty, open_trade = panel(*prefix, "step 3", "step 3")
    no_duty, open_trade = state(no_duty), state(open_trade)
    assert food(no_duty)["seller_cash"] - food(departure)["seller_cash"] == 3
    assert food(no_duty)["household_cash"] == food(departure)["household_cash"]
    assert "grain_trade" not in open_trade and food(open_trade)["food_stock"] == 1
    assert open_trade["treasury"] == 50

    for text, error in (
        (assessed.replace("executive-start FIELD-LAND FIELD-HEAD FIELD-MINISTER 20 0\n", ""),
         "cross-border grain-road requires both governments"),
        (assessed.replace("grain-tariff MILL-LAND 50", "grain-tariff FIELD-LAND 50"),
         "grain-tariff requires a cross-border grain import"),
        (assessed + "grain-tariff MILL-LAND 20\n", "duplicate grain-tariff"),
        (assessed.replace("MILL-LAND 50\n", "MILL-LAND 101\n"), "invalid grain-tariff percent"),
    ):
        start.write_text(text, encoding="ascii")
        rejected(run("--history", start), error)



def bundled_border_trade():
    sample = Path(__file__).resolve().parents[1] / "data/scenarios/fictional-grain-border-trade.txt"
    prefix = (f"history-open {sample}", "choose-country MILL-LAND")

    def panel(*commands):
        result = run("--panel-stdio", script="\n".join((*prefix, *commands, "quit", "")))
        success(result)
        replies = [json.loads(line) for line in result.stdout.splitlines()[:-1]]
        assert all(reply["ok"] for reply in replies), replies
        return [reply["status"] for reply in replies][2:]

    def food(status):
        return status["history"]["economy"]

    departure, arrived, exporter = panel("step 3", "step 3", "choose-country FIELD-LAND")
    assert departure["date"] == "1840-01-04" and departure["treasury"] == 53
    assert food(departure)["grain_in_transit"] == 1
    assert arrived["date"] == "1840-01-07" and food(arrived)["food_stock"] == 1
    assert arrived["grain_trade"]["duties_paid"] == 3
    assert exporter["treasury"] == 20 and food(exporter)["grain_farm"]["harvested"] == 2
    _, blocked = panel("tariff 100", "step 6")
    assert blocked["grain_trade"]["duties_paid"] == 0 and food(blocked)["food_stock"] == 0
    _, raised, still_arrived = panel("step 3", "tariff 100", "step 2")
    assert food(raised)["grain_in_transit"] == 1 and still_arrived["date"] == arrived["date"]
    assert food(still_arrived) == food(arrived) and still_arrived["treasury"] == 53


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
        local_food_mill(work)
        executive_tax(work)
        grain_freight(work)
        bundled_trial(work)
        staple_price_shock(work)
        staffed_grain_farm(work)
        cross_border_grain(work)
        bundled_border_trade()
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
