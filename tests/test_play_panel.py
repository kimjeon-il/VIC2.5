"""Drive real Tk controls linked to the native trial; needs a display (or Xvfb)."""
import sys
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
            run_industry(root, panel, save)
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
