"""Drive real Tk controls linked to the native trial; needs a display (or Xvfb)."""
import sys
from datetime import date
from pathlib import Path
from tempfile import TemporaryDirectory
from time import monotonic, sleep
from unittest.mock import patch
import tkinter as tk

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import play_panel


def wait(root, panel):
    stop = monotonic() + 15
    while panel.busy and monotonic() < stop:
        root.update()
        sleep(0.01)
    assert not panel.busy, "the native command did not finish"
    root.update()


def run(exe):
    assert play_panel.PlayPanel._protocol_tag("공장 시범판") == "TUT"
    assert play_panel.PlayPanel._protocol_tag("A 입법 시범판") == "LAW-A"
    assert play_panel.PlayPanel._protocol_tag("A 교육 시범판") == "EDU-A"
    assert play_panel.PlayPanel._protocol_tag("A 공동 캠페인") == "CAMPAIGN-A"
    assert play_panel.PlayPanel._protocol_tag("A 민간 증설 시범판") == "EXPANSION-A"
    assert play_panel.PlayPanel._protocol_tag("A 공급망 복구 시범판") == "SUPPLY-A"
    assert play_panel.PlayPanel._protocol_tag("A 가계 소비 시범판") == "STAPLES-A"
    assert play_panel.PlayPanel._protocol_tag("A 철강·건설 시범판") == "STEEL-A"
    root = tk.Tk()
    panel = play_panel.PlayPanel(root, exe)
    try:
        root.update()
        assert panel.current is None
        panel.country.set("A")
        panel.new_button.invoke()
        wait(root, panel)
        opening = panel.current
        assert opening["country"] == "A"
        assert opening["date"] in panel.heading.get()
        assert str(opening["treasury"]) in panel.budget.get()
        assert opening["law"] in panel.policy.get()
        assert str(opening["ore_price"]) in panel.market.get()
        assert panel.diplomacy.get()
        assert opening["max_tariff_ppm"] == 200_000 and "20%" in panel.policy.get()
        panel.tariff.set("25")
        panel.tariff_button.invoke()
        wait(root, panel)
        assert panel.current == opening and "오류:" in panel.notice.get()
        panel.tariff.set("10")
        panel.tariff_button.invoke()
        wait(root, panel)
        adopted = panel.current
        assert adopted["date"] > opening["date"]
        assert adopted["tariff_ppm"] == 100_000 and "10%" in panel.policy.get()
        with TemporaryDirectory() as directory:
            save = Path(directory) / "한글 trial with spaces.vic25"
            with patch.object(play_panel.filedialog, "asksaveasfilename", return_value=str(save)):
                panel.save_button.invoke()
                wait(root, panel)
                assert save.is_file(), panel.notice.get()
                before = save.read_bytes()
                panel.save_button.invoke()
                wait(root, panel)
                assert "오류:" in panel.notice.get() and save.read_bytes() == before
            panel.step_button.invoke()
            wait(root, panel)
            stepped = panel.current
            assert stepped["date"] > adopted["date"]
            with patch.object(play_panel.filedialog, "askopenfilename", return_value=str(save)):
                panel.country.set("B")
                panel.load_button.invoke()
                wait(root, panel)
                assert panel.current == stepped and "오류:" in panel.notice.get()
                panel.country.set("A")
                panel.load_button.invoke()
                wait(root, panel)
                assert panel.current == adopted
            panel.country.set("B")
            panel.new_button.invoke()
            wait(root, panel)
            assert panel.current["country"] == "B"
            assert panel.current["max_tariff_ppm"] == 400_000 and "40%" in panel.policy.get()
            run_income_tax(root, panel, Path(directory) / "소득세 예약.vic25")
            panel.country.set("A 예산 시범판")
            panel.new_button.invoke()
            wait(root, panel)
            assert panel.current["scenario"] == "budget-v1"
            assert panel.spending_button.winfo_ismapped() and panel.spending_button.instate(["!disabled"])
            assert panel.tax_button.instate(["!disabled"]) and panel.tariff_button.instate(["!disabled"])
            panel.spending.set("0")
            panel.spending_button.invoke()
            wait(root, panel)
            assert panel.current["pending_civilian_spending_cap"] == 0
            assert "예약 한도: 0" in panel.budget.get()
            panel._send("step 31")
            wait(root, panel)
            assert panel.current["civilian_spending_cap"] == 0 and "하루 한도: 0" in panel.budget.get()
            panel.new_button.invoke()
            wait(root, panel)
            assert panel.build_button.winfo_ismapped() and panel.build_button.instate(["!disabled"])
            assert "국고 공사비 20" in panel.budget.get() and "필요 철강 2" in panel.budget.get()
            panel.country.set("B 예산 시범판")
            panel.build_button.invoke()
            wait(root, panel)
            assert panel.current["country"] == "A"
            assert panel.current["public_construction"]["state"] == "queued"
            assert panel.build_button.instate(["disabled"])
            panel._send("step 3")
            wait(root, panel)
            assert panel.current["public_construction"]["commissioned_capacity"] == 31
            assert "준공" in panel.budget.get()
            run_industry(root, panel, save)
            run_elected_bill(root, panel, Path(directory) / "선출 의회 공포 대기.vic25")
            run_elected_bill(root, panel, Path(directory) / "교육 법안 공포 대기.vic25",
                             choice="A 교육 시범판")
            run_shared_campaign(root, panel, Path(directory) / "공동 캠페인 예약.vic25")
            run_shared_construction(root, panel, Path(directory) / "공동 건설 예산 승인.vic25")
            run_shared_spending(root, panel, Path(directory) / "공동 공공사업 한도.vic25")
            run_private_expansion(root, panel, Path(directory) / "민간 증설.vic25")
            run_supply_recovery(root, panel, Path(directory) / "공급망 복구.vic25")
            run_household_staples(root, panel, Path(directory) / "가계 소비.vic25")
            run_steel_construction(root, panel, Path(directory) / "철강 조달.vic25")
    finally:
        panel.close()
        stop = monotonic() + 3
        while monotonic() < stop:
            try:
                root.update()
            except tk.TclError:
                break
            if panel.engine.process.poll() is not None:
                break
            sleep(0.01)
        assert panel.engine.process.poll() is not None, "native child did not exit"


def run_income_tax(root, panel, save):
    panel.country.set("A")
    panel.new_button.invoke()
    wait(root, panel)
    opening = panel.current
    assert opening["income_tax_ppm"] == 100_000
    assert "소득세율: 10%" in panel.policy.get()
    assert panel.tax_button.instate(["!disabled"]) and panel.tax_button.winfo_ismapped()
    # The selector is a prospective new/load choice, not the active government.
    panel.country.set("B")
    panel.tax.set("15")
    panel.tax_button.invoke()
    wait(root, panel)
    pending = panel.current
    assert pending["country"] == "A" and pending["date"] == "2000-01-02"
    assert pending["income_tax_ppm"] == 100_000 and pending["pending_income_tax_ppm"] == 150_000
    assert pending["pending_income_tax_date"] == "2000-02-01"
    assert "15%" in panel.policy.get() and "2000-02-01" in panel.policy.get()
    assert panel.tax_button.instate(["disabled"])
    with patch.object(play_panel.filedialog, "asksaveasfilename", return_value=str(save)):
        panel.save_button.invoke()
        wait(root, panel)
        assert save.is_file()
    panel.step_button.invoke()
    wait(root, panel)
    assert panel.current["date"] == "2000-01-03"
    with patch.object(play_panel.filedialog, "askopenfilename", return_value=str(save)):
        panel.country.set("A")
        panel.load_button.invoke()
        wait(root, panel)
        assert panel.current == pending
    panel._send("step 31")
    wait(root, panel)
    assert panel.current["income_tax_ppm"] == 150_000
    assert panel.current["pending_income_tax_ppm"] is None
    assert "소득세율: 15%" in panel.policy.get()
    assert panel.tax_button.instate(["!disabled"])


def run_elected_bill(root, panel, save, choice="A 입법 시범판"):
    education = choice == "A 교육 시범판"
    scenario = "education-v1" if education else "law-v1"
    panel.country.set(choice)
    panel.new_button.invoke()
    wait(root, panel)
    assert panel.current["scenario"] == scenario
    assert not panel.tax_button.winfo_ismapped() and not panel.tariff_button.winfo_ismapped()
    for _ in range(4):
        if panel.current["law_bill"]["seats"]:
            break
        panel.step_button.invoke()
        wait(root, panel)
    opening = panel.current
    seats = opening["law_bill"]["seats"]
    threshold = opening["law_bill"]["threshold"]
    majority = next(p for p in seats if p["seats"] >= threshold)
    minority = next(p for p in seats if 0 < p["seats"] < threshold)
    assert f"가결 기준: {threshold}석" in panel.policy.get()
    assert f"{majority['party']}: {majority['seats']}석" in panel.policy.get()
    assert panel.law_action_button.winfo_ismapped() and panel.law_action_button.instate(["!disabled"])
    panel.country.set("B")  # Prospective chooser must not change the incumbent trial.
    panel.law_action_button.invoke()
    wait(root, panel)
    assert panel.current["country"] == "A" and panel.current["scenario"] == scenario
    assert panel.current["law_bill"]["phase"] == "submitted"
    for _ in range(12):
        bill = panel.current["law_bill"]
        if bill["vote_ready"]:
            break
        today = date.fromisoformat(panel.current["date"])
        review = date.fromisoformat(bill["next_review"])
        assert review >= today and bill["next_review"] in panel.policy.get()
        assert panel.law_action_button.cget("text") == "다음 심의일까지 진행"
        assert panel.law_action_button.instate(["!disabled"])
        panel.law_action_button.invoke()
        wait(root, panel)
    submitted = panel.current
    assert submitted["law_bill"]["vote_ready"]
    assert panel.law_party_choice.winfo_ismapped() and panel.law_party.get() == ""
    assert panel.law_action_button.instate(["disabled"])
    panel.law_party_choice.current(next(
        i for i, p in enumerate(submitted["law_bill"]["seats"]) if p == minority))
    panel.law_party_choice.event_generate("<<ComboboxSelected>>")
    root.update()
    panel.law_action_button.invoke()
    wait(root, panel)
    assert panel.current == submitted and "오류:" in panel.notice.get()
    panel.law_party_choice.current(next(
        i for i, p in enumerate(submitted["law_bill"]["seats"]) if p == majority))
    panel.law_party_choice.event_generate("<<ComboboxSelected>>")
    root.update()
    panel.law_action_button.invoke()
    wait(root, panel)
    assert panel.current["law_bill"]["phase"] == "approved"
    panel.law_action_button.invoke()
    wait(root, panel)
    pending = panel.current
    assert pending["law_bill"]["phase"] == "promulgated"
    assert pending["law_bill"]["effective_date"] in panel.policy.get()
    if not education:
        assert pending["minimum_monthly_contract_wage"] == opening["minimum_monthly_contract_wage"]
    assert panel.law_action_button.cget("text") == "시행일까지 진행"
    assert panel.law_action_button.instate(["!disabled"])
    with patch.object(play_panel.filedialog, "asksaveasfilename", return_value=str(save)):
        panel.save_button.invoke()
        wait(root, panel)
        assert save.is_file()
    with patch.object(play_panel.filedialog, "askopenfilename", return_value=str(save)):
        panel.country.set(choice)
        panel.load_button.invoke()
        wait(root, panel)
        assert panel.current == pending and panel.country.get() == choice
    today = date.fromisoformat(pending["date"])
    effective = date.fromisoformat(pending["law_bill"]["effective_date"])
    assert effective >= today
    panel.law_action_button.invoke()
    wait(root, panel)
    assert panel.current["law_bill"]["phase"] == "effective"
    if education:
        values = panel.current["education"]
        assert values["local_staff"] > 0
        assert f"현지 운영 인력: {values['local_staff']}명" in panel.market.get()
        assert values["usable_slots"] > 0
        assert values["school_primary"] > values["comparison_primary"] > 0
        assert values["staff_cash"] > opening["education"]["staff_cash"]
        assert f"운영 가능 정원: {values['usable_slots']}" in panel.market.get()
        assert f"학교 운영자 잔액: {values['operator_cash']}" in panel.market.get()
        assert f"운영 인력 지갑 잔액: {values['staff_cash']}" in panel.market.get()
        assert f"학교 지역: {values['school_primary']}명" in panel.diplomacy.get()
        assert f"비편입 비교 지역: {values['comparison_primary']}명" in panel.diplomacy.get()
    else:
        assert panel.current["minimum_monthly_contract_wage"] == 62
        assert "월 최저임금: 62" in panel.policy.get()


def run_shared_campaign(root, panel, save):
    panel.country.set("A 공동 캠페인")
    panel.new_button.invoke()
    wait(root, panel)
    status = panel.current
    assert status["scenario"] == "campaign-v1"
    assert "supply_recovery" not in status and "신규 운송 경로" not in panel.market.get()
    assert status["country"] == status["player_country"] == "A" and status["can_act"]
    assert status["peer_country"] == "B" and not status["peer_is_player"]
    assert "B · 조회 전용" in panel.diplomacy.get()
    assert str(status["peer_treasury"]) in panel.diplomacy.get()
    assert not panel.tariff_button.winfo_ismapped()
    assert panel.spending_button.winfo_ismapped() and panel.spending_button.instate(["disabled"])
    for _ in range(4):
        if panel.current["campaign_tax_allowed"]:
            break
        panel.step_button.invoke()
        wait(root, panel)
    opening = panel.current
    assert opening["campaign_tax_allowed"]
    assert f"제철소 재고: 광석 {opening['delivered_ore']}" in panel.market.get()
    assert f"운송 중 광석: {opening['in_transit_ore']}" in panel.market.get()
    assert panel.law_party_choice.winfo_ismapped() and panel.law_party.get() == ""
    assert panel.tax_button.instate(["disabled"])
    seats = opening["law_bill"]["seats"]
    threshold = opening["law_bill"]["threshold"]
    panel.law_party_choice.current(next(i for i, seat in enumerate(seats) if seat["seats"] >= threshold))
    panel.law_party_choice.event_generate("<<ComboboxSelected>>")
    root.update()
    panel.country.set("B")  # Prospective scenario selector must not transfer authority.
    panel.tax.set("15")
    panel.tax_button.invoke()
    wait(root, panel)
    pending = panel.current
    assert pending["country"] == pending["player_country"] == "A"
    assert pending["pending_income_tax_ppm"] == 150_000
    assert pending["pending_income_tax_date"] in panel.policy.get()
    assert pending["peer_pending_income_tax_ppm"] is None
    assert panel.tax_button.instate(["disabled"])
    assert panel.law_action_button.instate(["!disabled"])
    panel.law_action_button.invoke()
    wait(root, panel)
    assert panel.current["law_bill"]["phase"] == "submitted"
    # Use the existing batch step to reach (not process) the tax date.
    due = date.fromisoformat(panel.current["pending_income_tax_date"])
    today = date.fromisoformat(panel.current["date"])
    if today < due:
        panel._send(f"step {(due - today).days}")
        wait(root, panel)
    pending = panel.current
    assert pending["date"] == due.isoformat() and pending["in_transit_ore"] > 0
    assert pending["peer_pending_income_tax_ppm"] is None
    with patch.object(play_panel.filedialog, "asksaveasfilename", return_value=str(save)):
        panel.save_button.invoke()
        wait(root, panel)
        assert save.is_file()
    panel.step_button.invoke()
    wait(root, panel)
    next_day = panel.current
    assert next_day["date"] > pending["date"]
    assert next_day["income_tax_ppm"] == 150_000
    assert next_day["peer_pending_income_tax_ppm"] == 110_000
    assert next_day["peer_pending_income_tax_date"] in panel.diplomacy.get()
    assert next_day["supply_chain"]["iron_batches"] > pending["supply_chain"]["iron_batches"]
    with patch.object(play_panel.filedialog, "askopenfilename", return_value=str(save)):
        panel.country.set("A 공동 캠페인")
        panel.load_button.invoke()
        wait(root, panel)
        assert panel.current == pending and panel.country.get() == "A 공동 캠페인"
    panel.step_button.invoke()
    wait(root, panel)
    assert panel.current == next_day


def run_shared_spending(root, panel, save):
    panel.country.set("A 공동 캠페인")
    panel.new_button.invoke()
    wait(root, panel)
    assert panel.spending_button.winfo_ismapped() and panel.spending_button.instate(["disabled"])
    panel._send("step 2")
    wait(root, panel)
    bill = panel.current["law_bill"]
    assert panel.spending_button.instate(["disabled"]) and panel.current["campaign_spending_cap_allowed"]
    panel.law_party_choice.current(next(i for i, seat in enumerate(bill["seats"])
                                       if seat["seats"] >= bill["threshold"]))
    panel.law_party_choice.event_generate("<<ComboboxSelected>>")
    root.update()
    assert panel.spending_button.instate(["!disabled"])
    panel.spending.set("0")
    panel.spending_button.invoke()
    wait(root, panel)
    assert panel.current["pending_civilian_spending_cap"] == 0
    assert "예약 한도: 0" in panel.budget.get() and panel.spending_button.instate(["disabled"])
    due = date.fromisoformat(panel.current["pending_civilian_spending_date"])
    today = date.fromisoformat(panel.current["date"])
    panel._send(f"step {(due - today).days}")
    wait(root, panel)
    pending = panel.current
    assert pending["civilian_spending_cap"] is None
    with patch.object(play_panel.filedialog, "asksaveasfilename", return_value=str(save)):
        panel.save_button.invoke()
        wait(root, panel)
        assert save.is_file()
    panel.step_button.invoke()
    wait(root, panel)
    active = panel.current
    assert active["civilian_spending_cap"] == 0
    assert active["public_program_paid"] == pending["public_program_paid"]
    assert "하루 한도: 0" in panel.budget.get()
    panel._send("inspect B")
    wait(root, panel)
    assert panel.current["country"] == "B" and not panel.spending_button.winfo_ismapped()
    with patch.object(play_panel.filedialog, "askopenfilename", return_value=str(save)):
        panel.load_button.invoke()
        wait(root, panel)
        assert panel.current == pending
    panel.step_button.invoke()
    wait(root, panel)
    assert panel.current == active


def run_shared_construction(root, panel, save):
    panel.country.set("A 공동 캠페인")
    panel.new_button.invoke()
    wait(root, panel)
    opening = panel.current["public_construction"]
    assert "벌목 0 · 원목 확보: 공구공장 0 · 제재소 0 · 판재 생산 0" in panel.market.get()
    assert opening["budget_approval"] == "available" and opening["state"] == "available"
    assert opening["invoice"] == 20 and opening["treasury_reserve"] == 40
    assert opening["contractor_steel"] == 0 and opening["commissioned_capacity"] == 0
    assert "국고 공사비 20" in panel.budget.get()
    assert panel.build_button.winfo_ismapped() and panel.build_button.instate(["disabled"])
    for _ in range(4):
        if panel.current["public_construction"]["budget_vote_allowed"]:
            break
        panel.step_button.invoke()
        wait(root, panel)
    assert panel.current["public_construction"]["budget_vote_allowed"]
    assert panel.law_party_choice.winfo_ismapped()
    assert panel.law_party.get() == "" and panel.build_button.instate(["disabled"])
    bill = panel.current["law_bill"]
    panel.law_party_choice.current(next(
        i for i, seat in enumerate(bill["seats"]) if seat["seats"] >= bill["threshold"]))
    panel.law_party_choice.event_generate("<<ComboboxSelected>>")
    root.update()
    election_day = panel.current["date"]
    panel.country.set("B")  # This is a prospective load/new selector, not a transfer of authority.
    panel.build_button.invoke()
    wait(root, panel)
    approved = panel.current
    quote = approved["public_construction"]
    assert approved["country"] == approved["player_country"] == "A"
    assert approved["date"] > election_day
    assert quote["budget_approval"] == "approved" and quote["state"] == "available"
    assert quote["contractor_steel"] == 0 and quote["commissioned_capacity"] == 0
    assert "건설 예산: 승인됨" in panel.budget.get()
    assert panel.build_button.instate(["!disabled"])
    with patch.object(play_panel.filedialog, "asksaveasfilename", return_value=str(save)):
        panel.save_button.invoke()
        wait(root, panel)
        assert save.is_file()
    with patch.object(play_panel.filedialog, "askopenfilename", return_value=str(save)):
        panel.country.set("A 공동 캠페인")
        panel.load_button.invoke()
        wait(root, panel)
        assert panel.current == approved
    assert panel.build_button.instate(["!disabled"])
    panel.build_button.invoke()
    wait(root, panel)
    assert panel.current["public_construction"]["state"] == "queued"
    assert panel.current["public_construction"]["commissioned_capacity"] == 0
    panel._send("step 3")
    wait(root, panel)
    assert panel.current["public_construction"]["commissioned_capacity"] == 31
    assert "준공" in panel.budget.get() and "지급 20" in panel.budget.get()
    assert panel.current["public_construction"]["invoice_paid"] == 20
    assert panel.current["peer_country"] == "B"
    for _ in range(15):
        chain = panel.current["supply_chain"]
        if chain["household_fittings_bought"] >= 3:
            break
        panel.step_button.invoke()
        wait(root, panel)
    chain = panel.current["supply_chain"]
    assert chain["tools_sold"] > 0 and chain["tool_buyer_batches"] > 0
    assert chain["household_fittings_bought"] >= 3
    assert chain["board_vendor_receipts"] >= 40
    assert chain["logs_harvested"] > 0 and chain["logger_receipts"] > 0
    assert chain["toolworks_wood_bought"] > 0 and chain["mill_wood_bought"] > 0
    assert chain["mill_boards_produced"] > 0
    assert chain["fittings_sales_receipts"] == 90 * chain["household_fittings_bought"]
    assert chain["wood_seller_cash"] > 0
    market = panel.market.get()
    assert f"제철소 재고: 광석 {chain['ore_stock']} · 철 {chain['iron_stock']}" in market
    assert f"공구공장 재고: 철 {chain['factory_iron_stock']} · 목재 {chain['factory_wood_stock']} · 공구 {chain['tools_stock']}" in market
    assert f"목재 공급자 잔액 {chain['wood_seller_cash']}" in market
    assert f"공구 판매량 {chain['tools_sold']}" in market
    assert f"목공소 목재 부품 재고 {chain['tool_buyer_output_stock']}" in market
    assert f"가계 부품 구매 {chain['household_fittings_bought']}" in market
    assert f"목공소 판매대금 {chain['fittings_sales_receipts']}" in market
    assert f"판재 판매대금 {chain['board_vendor_receipts']}" in market
    assert f"벌목 {chain['logs_harvested']}" in market
    assert f"원목 확보: 공구공장 {chain['toolworks_wood_bought']} · 제재소 {chain['mill_wood_bought']}" in market
    assert f"판재 생산 {chain['mill_boards_produced']}" in market


def run_private_expansion(root, panel, save):
    choice = "A 민간 증설 시범판"
    panel.selector.current(panel.selector.cget("values").index(choice))
    panel.new_button.invoke()
    wait(root, panel)
    opening = panel.current
    assert opening["country"] == "A" and opening["scenario"] == "expansion-v1"
    assert "민간 증설 시범판" in panel.heading.get()
    assert opening["private_expansion"]["paid_construction"] == 0
    assert opening["private_expansion"]["commissioned_capacity"] == 0
    assert "증설 대기" in panel.policy.get() and "미고용" in panel.diplomacy.get()
    for widget in (panel.tariff_button, panel.tax_button, panel.spending_button,
                   panel.build_button, panel.law_action_button):
        assert not widget.winfo_ismapped()
    with patch.object(play_panel.filedialog, "asksaveasfilename", return_value=str(save)):
        panel.save_button.invoke()
        wait(root, panel)
        assert save.is_file(), panel.notice.get()

    steps = 0
    seen_paid_before_commission = False
    for _ in range(25):
        panel.step_button.invoke()
        wait(root, panel)
        steps += 1
        result = panel.current["private_expansion"]
        if result["paid_construction"] and not result["commissioned_capacity"]:
            assert "준공 대기" in panel.policy.get()
            seen_paid_before_commission = True
        if result["expansion_batches"] and result["new_worker_cash"] and result["new_worker_employed"]:
            break
    assert seen_paid_before_commission
    assert "준공 완료" in panel.policy.get()
    assert result["paid_construction"] > 0 and result["commissioned_capacity"] > 0
    assert result["sales_receipts"] > 0 and result["expansion_batches"] > 0
    assert result["new_worker_employed"] and result["new_worker_cash"] > 0
    assert f"공사비 지급: {result['paid_construction']}" in panel.policy.get()
    assert f"준공 설비 용량: {result['commissioned_capacity']}" in panel.policy.get()
    assert f"누적 판매대금: {result['sales_receipts']}" in panel.market.get()
    assert f"증설 설비 생산 횟수: {result['expansion_batches']}" in panel.market.get()
    assert "신규 직원: 고용됨" in panel.diplomacy.get()
    assert f"신규 직원 지갑 잔액: {result['new_worker_cash']}" in panel.diplomacy.get()
    reached = panel.current
    with patch.object(play_panel.filedialog, "askopenfilename", return_value=str(save)):
        panel.country.set("A 공동 캠페인")
        panel.load_button.invoke()
        wait(root, panel)
        assert panel.current == reached and "오류:" in panel.notice.get()
        panel.country.set(choice)
        panel.load_button.invoke()
        wait(root, panel)
        assert panel.current == opening and panel.country.get() == choice
    panel._send(f"step {steps}")
    wait(root, panel)
    assert panel.current == reached


def run_supply_recovery(root, panel, save):
    choice = "A 공급망 복구 시범판"
    panel.selector.current(panel.selector.cget("values").index(choice))
    panel.new_button.invoke()
    wait(root, panel)
    assert panel.current["scenario"] == "campaign-v1"
    assert panel.current["country"] == panel.current["player_country"] == "A"
    assert "공급망 복구 시범판" in panel.heading.get()
    assert panel.current["supply_recovery"]["route"] == "rail"
    assert "신규 운송 경로: 철도" in panel.market.get()
    assert "B · 조회 전용" in panel.diplomacy.get()

    def advance_to(target):
        days = (date.fromisoformat(target) - date.fromisoformat(panel.current["date"])).days
        assert days > 0
        panel._send(f"step {days}")
        wait(root, panel)
        assert panel.current["date"] == target

    advance_to("2000-02-01")
    assert panel.current["supply_recovery"]["route"] == "closed"
    assert "신규 운송 경로: 없음" in panel.market.get()
    with patch.object(play_panel.filedialog, "asksaveasfilename", return_value=str(save)):
        panel.save_button.invoke()
        wait(root, panel)
        assert save.is_file(), panel.notice.get()
    closed = panel.current
    advance_to("2000-03-01")
    ready = panel.current["supply_recovery"]
    assert ready["route"] == "road" and ready["road_in_transit_ore"] == 0
    assert "신규 운송 경로: 도로 우회" in panel.market.get()
    panel.step_button.invoke()
    wait(root, panel)
    pending = panel.current["supply_recovery"]
    assert pending["road_in_transit_ore"] > 0 and pending["road_freight_paid"] > 0
    assert pending["iron_batches"] == ready["iron_batches"]
    assert pending["smelter_receipts"] == ready["smelter_receipts"]
    assert f"도로 운송 중 광석: {pending['road_in_transit_ore']}" in panel.market.get()
    assert f"도로 운임 지급: {pending['road_freight_paid']}" in panel.market.get()
    assert f"제철소 광석 재고: {pending['ore_stock']}" in panel.market.get()
    for _ in range(5):
        state = panel.current["supply_recovery"]
        if state["iron_batches"] > pending["iron_batches"]:
            break
        panel.step_button.invoke()
        wait(root, panel)
    resumed = panel.current
    state = resumed["supply_recovery"]
    assert state["iron_batches"] > pending["iron_batches"]
    assert state["smelter_receipts"] > pending["smelter_receipts"]
    assert f"철 생산 횟수: {state['iron_batches']}" in panel.market.get()
    assert f"철 판매대금: {state['smelter_receipts']}" in panel.market.get()

    with patch.object(play_panel.filedialog, "askopenfilename", return_value=str(save)):
        panel.country.set("A 공동 캠페인")
        panel.load_button.invoke()
        wait(root, panel)
        assert panel.current == resumed and "오류:" in panel.notice.get()
        panel.country.set(choice)
        panel.load_button.invoke()
        wait(root, panel)
        assert panel.current == closed and panel.country.get() == choice
    advance_to(resumed["date"])
    assert panel.current == resumed


def run_household_staples(root, panel, save):
    choice = "A 가계 소비 시범판"
    panel.selector.current(panel.selector.cget("values").index(choice))
    panel.new_button.invoke()
    wait(root, panel)
    opening = panel.current
    assert opening["scenario"] == "staples-v1" and opening["country"] == "A"
    assert "가계 소비 시범판" in panel.heading.get()
    assert "식량과 의류 우선" in panel.policy.get()
    assert not any(case["cash"] or case["wage_received"] or case["groceries_bought"]
                   or case["clothes_bought"] or case["fittings_bought"]
                   for case in opening["household_staples"].values())
    for widget in (panel.tariff_button, panel.tax_button, panel.spending_button,
                   panel.build_button, panel.law_action_button):
        assert not widget.winfo_ismapped()
    with patch.object(play_panel.filedialog, "asksaveasfilename", return_value=str(save)):
        panel.save_button.invoke()
        wait(root, panel)
        assert save.is_file(), panel.notice.get()
    panel.step_button.invoke()
    wait(root, panel)
    paid = panel.current
    basket = paid["household_staples"]
    for case, shown, expected in (
        (basket["necessities_only"], panel.market.get(), (45, 0, 1, 1, 0)),
        (basket["with_discretionary"], panel.diplomacy.get(), (76, 1, 1, 1, 1)),
    ):
        assert tuple(case[k] for k in ("wage_received", "cash", "groceries_bought",
                                       "clothes_bought", "fittings_bought")) == expected
        assert f"누적 실수령 임금: {case['wage_received']}" in shown
        assert f"가계 잔액: {case['cash']}" in shown
        assert f"식량 구매: {case['groceries_bought']}" in shown
        assert f"의류 구매: {case['clothes_bought']}" in shown
        assert f"목재 부품 구매: {case['fittings_bought']}" in shown
        assert shown.index("식량 구매") < shown.index("목재 부품 구매")
        assert shown.index("의류 구매") < shown.index("목재 부품 구매")
    with patch.object(play_panel.filedialog, "askopenfilename", return_value=str(save)):
        panel.country.set("A 공동 캠페인")
        panel.load_button.invoke()
        wait(root, panel)
        assert panel.current == paid and "오류:" in panel.notice.get()
        panel.country.set(choice)
        panel.load_button.invoke()
        wait(root, panel)
        assert panel.current == opening and panel.country.get() == choice
    panel.step_button.invoke()
    wait(root, panel)
    assert panel.current == paid


def run_steel_construction(root, panel, save):
    choice = "A 철강·건설 시범판"
    panel.selector.current(panel.selector.cget("values").index(choice))
    panel.new_button.invoke()
    wait(root, panel)
    assert panel.current["scenario"] == "campaign-v1"
    assert panel.current["country"] == panel.current["player_country"] == "A"
    assert "철강·건설 시범판" in panel.heading.get()
    opening = panel.current
    assert opening["steel_chain"]["steel_stock"] == 0
    assert opening["public_construction"]["contractor_steel"] == 0
    assert "철강 재고: 0" in panel.market.get()
    assert "철강 생산 대기" in panel.budget.get()
    assert panel.build_button.winfo_ismapped() and panel.build_button.instate(["disabled"])
    for _ in range(4):
        if panel.current["public_construction"]["budget_vote_allowed"]:
            break
        panel.step_button.invoke()
        wait(root, panel)
    assert panel.current["public_construction"]["budget_vote_allowed"]
    bill = panel.current["law_bill"]
    panel.law_party_choice.current(next(
        i for i, seat in enumerate(bill["seats"]) if seat["seats"] >= bill["threshold"]))
    panel.law_party_choice.event_generate("<<ComboboxSelected>>")
    root.update()
    panel.build_button.invoke()
    wait(root, panel)
    approved = panel.current
    assert approved["public_construction"]["budget_approval"] == "approved"
    assert approved["public_construction"]["invoice_paid"] == 0
    assert "건설 예산: 승인됨" in panel.budget.get()
    if approved["steel_chain"]["steel_stock"] < approved["public_construction"]["steel_required"]:
        assert "철강 생산 대기" in panel.budget.get()
        assert panel.build_button.instate(["disabled"])
    for _ in range(10):
        if panel.build_button.instate(["!disabled"]):
            break
        panel.step_button.invoke()
        wait(root, panel)
    ready = panel.current
    assert ready["steel_chain"]["steel_stock"] >= ready["public_construction"]["steel_required"]
    assert ready["public_construction"]["state"] == "available"
    assert ready["public_construction"]["invoice_paid"] == 0
    assert "철강 생산 대기" not in panel.budget.get()
    assert panel.build_button.instate(["!disabled"])
    panel.build_button.invoke()
    wait(root, panel)
    pending = panel.current
    assert pending["public_construction"]["state"] == "queued"
    assert pending["public_construction"]["commissioned_capacity"] == 0
    assert pending["public_construction"]["invoice_paid"] == 0
    if pending["public_construction"]["contractor_steel"] < pending["public_construction"]["steel_required"]:
        assert "철강 조달 대기" in panel.budget.get()
    assert "철강 생산 대기" not in panel.budget.get()
    with patch.object(play_panel.filedialog, "asksaveasfilename", return_value=str(save)):
        panel.save_button.invoke()
        wait(root, panel)
        assert save.is_file(), panel.notice.get()
    steps = 0
    purchased_before_paid = False
    paid_before_commission = False
    for _ in range(20):
        panel.step_button.invoke()
        wait(root, panel)
        steps += 1
        project = panel.current["public_construction"]
        if project["state"] == "queued" and project["contractor_steel"] == project["steel_required"]:
            assert project["invoice_paid"] == 0
            assert f"확보 {project['contractor_steel']}" in panel.budget.get()
            assert "철강 조달 대기" not in panel.budget.get()
            purchased_before_paid = True
        if project["state"] == "paid":
            assert f"공사비 지급 완료 · 지급 {project['invoice_paid']}" in panel.budget.get()
            assert project["invoice_paid"] == project["invoice"]
            assert project["commissioned_capacity"] == 0
            paid_before_commission = True
        if project["state"] == "commissioned":
            break
    assert purchased_before_paid and paid_before_commission and project["state"] == "commissioned"
    assert project["commissioned_capacity"] > 0
    assert f"준공 · 지급 {project['invoice_paid']}" in panel.budget.get()
    result = panel.current
    steel = result["steel_chain"]
    assert steel["coal_mined"] > 0 and steel["coal_receipts"] > 0
    assert steel["steel_produced"] >= project["steel_required"]
    assert steel["steel_receipts"] >= project["steel_purchase_cost"] > 0
    assert f"철강 생산: {steel['steel_produced']}" in panel.market.get()
    assert f"제강소 판매대금: {steel['steel_receipts']}" in panel.market.get()
    with patch.object(play_panel.filedialog, "askopenfilename", return_value=str(save)):
        panel.country.set("A 공동 캠페인")
        panel.load_button.invoke()
        wait(root, panel)
        assert panel.current == result and "오류:" in panel.notice.get()
        panel.country.set(choice)
        panel.load_button.invoke()
        wait(root, panel)
        assert panel.current == pending and panel.country.get() == choice
    panel._send(f"step {steps}")
    wait(root, panel)
    assert panel.current == result


def run_industry(root, panel, tariff_save):
    factory = "공장 시범판"
    panel.selector.current(2)
    assert panel.country.get() == factory
    assert panel.tariff_button.instate(["!disabled"]), "pending selector hid active tariff controls"
    panel.new_button.invoke()
    wait(root, panel)

    def state(expected):
        status = panel.current
        assert status["country"] == "TUT" and status["scenario"] == "industry-v1"
        assert tuple(status[k] for k in ("ore_stock", "food_stock", "production_batches",
                                        "treasury", "expenditure", "public_works_cash")) == expected
        assert status["ore_insufficient"] == (status["ore_stock"] < status["ore_per_batch"])
        assert f"누적 생산 배치: {status['production_batches']}" in panel.market.get()
        assert f"배치당 필요한 광석: {status['ore_per_batch']}" in panel.market.get()
        assert f"공공사업 지급처 잔액: {status['public_works_cash']}" in panel.diplomacy.get()
        assert panel.tariff_button.instate(["disabled"]) and not panel.tariff_button.winfo_ismapped()
        assert panel.tax_button.instate(["disabled"]) and not panel.tax_button.winfo_ismapped()
        assert not panel.spending_button.winfo_ismapped()
        return status

    state((8, 4, 0, 100, 0, 0))
    panel.step_button.invoke()
    wait(root, panel)
    paid = state((6, 8, 2, 98, 2, 2))
    assert "서비스 완료" not in panel.diplomacy.get()
    factory_save = tariff_save.with_name("공장 저장.vic25")
    with patch.object(play_panel.filedialog, "asksaveasfilename", return_value=str(factory_save)):
        panel.save_button.invoke()
        wait(root, panel)
        assert factory_save.is_file(), panel.notice.get()
    for _ in range(3):
        panel.step_button.invoke()
        wait(root, panel)
    exhausted = state((0, 20, 8, 98, 2, 2))
    assert "광석 부족" in panel.market.get()
    panel.step_button.invoke()
    wait(root, panel)
    stalled = state((0, 20, 8, 98, 2, 2))
    assert stalled["date"] > exhausted["date"]
    with patch.object(play_panel.filedialog, "askopenfilename", return_value=str(tariff_save)):
        panel.load_button.invoke()
        wait(root, panel)
        assert panel.current == stalled and "오류:" in panel.notice.get()
    panel.country.set("A")
    with patch.object(play_panel.filedialog, "askopenfilename", return_value=str(factory_save)):
        panel.load_button.invoke()
        wait(root, panel)
        assert panel.current == stalled and "오류:" in panel.notice.get()
        assert panel.tariff_button.instate(["disabled"])
        panel.country.set(factory)
        panel.load_button.invoke()
        wait(root, panel)
        assert panel.current == paid and panel.country.get() == factory


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python tests/test_play_panel.py <vic25_play path>")
    run(Path(sys.argv[1]))
    print("Tk native trial panel PASS")
