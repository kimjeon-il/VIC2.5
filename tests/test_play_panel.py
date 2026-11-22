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
            save = Path(directory) / "trial with spaces.vic25"
            with patch.object(play_panel.filedialog, "asksaveasfilename", return_value=str(save)):
                panel.save_button.invoke()
                wait(root, panel)
                assert save.is_file()
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


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python tests/test_play_panel.py <vic25_play path>")
    run(Path(sys.argv[1]))
    print("Tk native trial panel PASS")
