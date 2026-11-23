"""Map-free native VIC2.5 trial panel; requires an existing Python/Tk installation."""
import json
from pathlib import Path
from queue import Empty, Queue
import subprocess
import sys
from threading import Thread
from time import monotonic
import tkinter as tk
from tkinter import filedialog, ttk


class NativeEngine:
    """Run one local, line-oriented child process without blocking Tk."""
    def __init__(self, executable):
        self.process = subprocess.Popen(
            [str(Path(executable).resolve()), "--panel-stdio"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            text=True, encoding="utf-8", bufsize=1,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )
        self.commands = Queue()
        self.replies = Queue()
        Thread(target=self._exchange, daemon=True).start()

    def _exchange(self):
        try:
            while True:
                command = self.commands.get()
                self.process.stdin.write(command + "\n")
                self.process.stdin.flush()
                line = self.process.stdout.readline()
                if not line:
                    raise EOFError("the native engine exited without a reply")
                self.replies.put((command, json.loads(line), None))
                if command == "quit":
                    return
        except (OSError, ValueError, EOFError) as exc:
            self.replies.put((None, None, str(exc)))
        finally:
            try:
                self.process.stdin.close()
            except OSError:
                pass  # The child may already have closed its input.
            self.process.stdout.close()


class PlayPanel:
    FACTORY = "공장 시범판"
    BUDGETS = {"A 예산 시범판": "BUDGET-A", "B 예산 시범판": "BUDGET-B"}

    @classmethod
    def _protocol_tag(cls, choice):
        return "TUT" if choice == cls.FACTORY else cls.BUDGETS.get(choice, choice)

    def __init__(self, root, executable):
        self.root = root
        self.current = None
        self.busy = False
        self.closing = False
        self.disconnected = False
        self.country = tk.StringVar(root, "A")
        self.tariff = tk.StringVar(root, "0")
        self.tax = tk.StringVar(root, "0")
        self.spending = tk.StringVar(root, "0")
        self.heading = tk.StringVar(root, "열린 시범 시나리오 없음")
        self.budget = tk.StringVar(root, "국가를 선택하고 시나리오를 시작하세요.")
        self.policy = tk.StringVar(root, "법률·관세·세금 정보 없음")
        self.market = tk.StringVar(root, "시장 정보 없음")
        self.diplomacy = tk.StringVar(root, "외교 정보 없음")
        self.notice = tk.StringVar(root, "국가 또는 공장 시범판을 선택한 뒤 시작하거나 저장본을 불러오세요.")
        root.title("VIC2.5 | 시범판")
        root.geometry("740x520")
        root.minsize(540, 440)
        root.columnconfigure(0, weight=1)
        root.rowconfigure(0, weight=1)

        frame = ttk.Frame(root, padding=(18, 14))
        frame.grid(sticky="nsew")
        frame.columnconfigure(0, weight=1)
        ttk.Label(frame, text="VIC2.5 시범판",
                  font=("TkDefaultFont", 16, "bold")).grid(row=0, sticky="w")
        ttk.Label(frame, text="가상 시나리오 · 실제 역사 지도 없음").grid(row=1, sticky="w")
        setup = ttk.Frame(frame, padding=(0, 10))
        setup.grid(row=2, sticky="ew")
        ttk.Label(setup, text="시작·불러올 국가/시나리오:").grid(row=0, column=0, padx=(0, 6))
        self.selector = ttk.Combobox(setup, textvariable=self.country, values=("A", "B", self.FACTORY, *self.BUDGETS),
                                     width=16, state="readonly")
        self.selector.grid(row=0, column=1, padx=(0, 8))
        self.new_button = ttk.Button(setup, text="새로 시작", command=self.new)
        self.load_button = ttk.Button(setup, text="불러오기…", command=self.load)
        self.save_button = ttk.Button(setup, text="다른 이름으로 저장…", command=self.save)
        for column, button in enumerate((self.new_button, self.load_button, self.save_button), 2):
            button.grid(row=0, column=column, padx=(0, 5))
        ttk.Label(frame, textvariable=self.heading, font=("TkDefaultFont", 11, "bold")
                  ).grid(row=3, sticky="w")
        details = ttk.Frame(frame)
        details.grid(row=4, sticky="nsew")
        details.columnconfigure((0, 1), weight=1, uniform="card")
        details.rowconfigure((0, 1), weight=1)
        frame.rowconfigure(4, weight=1)
        self.cards = []
        for i, (name, value) in enumerate((
            ("재정", self.budget), ("법률·관세·세금", self.policy),
            ("광석 시장", self.market), ("외교", self.diplomacy),
        )):
            card = ttk.LabelFrame(details, text=name, padding=(10, 6))
            self.cards.append(card)
            card.grid(row=i//2, column=i%2, sticky="nsew", padx=(0, 6), pady=4)
            label = ttk.Label(card, textvariable=value, justify="left")
            label.pack(fill="both", expand=True, anchor="nw")
            card.bind("<Configure>", lambda event, label=label:
                      label.configure(wraplength=max(100, event.width - 25)))

        actions = ttk.Frame(frame, padding=(0, 8))
        actions.grid(row=5, sticky="ew")
        self.step_button = ttk.Button(actions, text="하루 진행", command=self.step)
        self.step_button.grid(row=0, column=0, padx=(0, 12))
        self.tariff_label = ttk.Label(actions, text="수입 관세율(%):")
        self.tariff_label.grid(row=0, column=1)
        self.tariff_entry = ttk.Entry(actions, textvariable=self.tariff, width=9)
        self.tariff_entry.grid(row=0, column=2, padx=5)
        self.tariff_button = ttk.Button(actions, text="관세 적용 후 하루 진행", command=self.set_tariff)
        self.tariff_button.grid(row=0, column=3)
        self.tariff_entry.bind("<Return>", lambda event: self.set_tariff())
        self.tax_label = ttk.Label(actions, text="소득세율(%):")
        self.tax_label.grid(row=1, column=1, pady=(4, 0))
        self.tax_entry = ttk.Entry(actions, textvariable=self.tax, width=9)
        self.tax_entry.grid(row=1, column=2, padx=5, pady=(4, 0))
        self.tax_button = ttk.Button(actions, text="소득세 제출 후 하루 진행", command=self.set_income_tax)
        self.tax_button.grid(row=1, column=3, pady=(4, 0))
        self.tax_entry.bind("<Return>", lambda event: self.set_income_tax())
        self.spending_label = ttk.Label(actions, text="하루 공공사업 한도:")
        self.spending_label.grid(row=2, column=1, pady=(4, 0))
        self.spending_entry = ttk.Entry(actions, textvariable=self.spending, width=9)
        self.spending_entry.grid(row=2, column=2, padx=5, pady=(4, 0))
        self.spending_button = ttk.Button(actions, text="지출 한도 제출 후 하루 진행", command=self.set_spending_cap)
        self.spending_button.grid(row=2, column=3, pady=(4, 0))
        self.spending_entry.bind("<Return>", lambda event: self.set_spending_cap())
        ttk.Separator(frame).grid(row=6, sticky="ew")
        notice = ttk.Label(frame, textvariable=self.notice, justify="left", wraplength=500)
        notice.grid(row=7, sticky="ew", pady=(5, 0))
        frame.bind("<Configure>", lambda event: notice.configure(wraplength=max(100, event.width - 36)))
        root.bind("<Control-l>", lambda event: self.load())
        root.bind("<Control-s>", lambda event: self.save())
        root.protocol("WM_DELETE_WINDOW", self.close)
        self.engine = NativeEngine(executable)
        self._update_controls()
        self.selector.focus_set()
        root.after(30, self._poll)

    def _update_controls(self):
        ready = not (self.busy or self.closing or self.disconnected)
        for widget in (self.new_button, self.load_button):
            widget.state(["!disabled"] if ready else ["disabled"])
        for widget in (self.save_button, self.step_button):
            widget.state(["!disabled"] if ready and self.current else ["disabled"])
        factory = self._industry(self.current)
        for widget in (self.tariff_label, self.tariff_button, self.tariff_entry):
            widget.state(["!disabled"] if ready and self.current and not factory else ["disabled"])
            if factory:
                widget.grid_remove()
            else:
                widget.grid()
        tax_ready = ready and self.current and not factory and self.current["pending_income_tax_ppm"] is None
        for widget in (self.tax_label, self.tax_entry, self.tax_button):
            widget.state(["!disabled"] if tax_ready else ["disabled"])
            if factory:
                widget.grid_remove()
            else:
                widget.grid()
        budget = self._budget(self.current)
        spending_ready = ready and budget and self.current["pending_civilian_spending_cap"] is None
        for widget in (self.spending_label, self.spending_entry, self.spending_button):
            widget.state(["!disabled"] if spending_ready else ["disabled"])
            if budget:
                widget.grid()
            else:
                widget.grid_remove()
        self.selector.configure(state="readonly" if ready else "disabled")

    def _send(self, command):
        if self.busy or self.closing or self.disconnected:
            return
        self.busy = True
        self.notice.set("진행 중…")
        self._update_controls()
        self.engine.commands.put(command)

    def _poll(self):
        if self.closing:
            return
        try:
            command, reply, failure = self.engine.replies.get_nowait()
        except Empty:
            if self.engine.process.poll() is not None and not self.disconnected:
                self.disconnected = True
                self.busy = False
                self.notice.set("오류: 게임 엔진이 종료됐습니다.")
                self._update_controls()
        else:
            self.busy = False
            if failure:
                self.disconnected = True
                self.notice.set("오류: " + failure)
            elif not isinstance(reply, dict) or not isinstance(reply.get("ok"), bool):
                self.disconnected = True
                self.notice.set("오류: 게임 엔진의 응답 형식이 잘못됐습니다.")
            elif not reply["ok"]:
                self.notice.set("오류: " + str(reply.get("error", "게임 엔진이 요청을 거절했습니다.")))
            elif reply.get("status") is not None:
                try:
                    self._show(reply["status"])
                    if command.startswith(("new ", "load ")):
                        choice = self.current["country"]
                        if self._industry(self.current):
                            choice = self.FACTORY
                        elif self._budget(self.current):
                            choice += " 예산 시범판"
                        self.country.set(choice)
                    if command.startswith("income-tax "):
                        self.notice.set("소득세 변경을 예약했습니다.")
                    elif command.startswith("spending-cap "):
                        self.notice.set("공공사업 지출 한도를 예약했습니다.")
                    else:
                        self.notice.set("저장했습니다." if command.startswith("save ") else "준비됐습니다.")
                except (KeyError, TypeError, ValueError):
                    self.disconnected = True
                    self.notice.set("오류: 게임 엔진에서 잘못된 상태를 받았습니다.")
            else:
                self.notice.set("준비됐습니다.")
            self._update_controls()
        self.root.after(30, self._poll)

    @staticmethod
    def _percent(ppm):
        whole, fraction = divmod(ppm, 10_000)
        return f"{whole}.{fraction:04d}".rstrip("0") if fraction else str(whole)

    @staticmethod
    def _industry(status):
        return bool(status and status.get("scenario") == "industry-v1")

    @staticmethod
    def _budget(status):
        return bool(status and status.get("scenario") == "budget-v1")

    def _show(self, status):
        factory = self._industry(status)
        perspective = self.FACTORY if factory else f"국가 {status['country']}"
        if self._budget(status):
            perspective += " · 예산 시범판"
        headline = f"{perspective}  |  {status['date']}  |  진행 기록 {status['commit']}"
        budget = (f"국고: {status['treasury']}\n"
                  f"누적 수입: {status['revenue']}\n"
                  f"누적 지출: {status['expenditure']}\n"
                  f"부채: {status['debt']}")
        if self._budget(status):
            cap = status["civilian_spending_cap"]
            pending = status["pending_civilian_spending_cap"]
            budget += f"\n공공사업 하루 한도: {cap if cap is not None else '미설정'}"
            budget += f"\n공공사업 누적 지급: {status['public_program_paid']}"
            if pending is not None:
                budget += f"\n예약 한도: {pending} ({status['pending_civilian_spending_date']} 시행)"
            self.spending.set(str(pending if pending is not None else cap if cap is not None else 0))
        if factory:
            policy = ("광석을 투입해 식량을 생산합니다.\n"
                      "광석이 부족하면 생산이 멈춥니다.")
            market = (f"공장 광석 재고: {status['ore_stock']}\n"
                      f"배치당 필요한 광석: {status['ore_per_batch']}\n"
                      f"공장 식량 재고: {status['food_stock']}\n"
                      f"누적 생산 배치: {status['production_batches']}\n"
                      + ("현재 광석 부족" if status["ore_insufficient"] else "배치에 필요한 광석 있음"))
            diplomacy = (f"공공사업 지급처 잔액: {status['public_works_cash']}\n"
                         "실제 국고 지급액을 보여줍니다.\n"
                         "일정 금액은 국고에 남겨 둡니다.")
            titles = ("재정", "생산 조건", "공장 재고·생산", "공공사업 지급")
        else:
            policy = (f"무역법: {status['law']}\n"
                      f"수입 관세율: {self._percent(status['tariff_ppm'])}%\n"
                      f"법정 상한: {self._percent(status['max_tariff_ppm'])}%\n"
                      f"소득세율: {self._percent(status['income_tax_ppm'])}%")
            pending_tax = status["pending_income_tax_ppm"]
            if pending_tax is not None:
                policy += (f"\n예약: {self._percent(pending_tax)}% "
                           f"({status['pending_income_tax_date']} 시행)")
            market = (f"광석 시세: {status['ore_price']}\n"
                      f"수입업자 도착 광석: {status['delivered_ore']}\n"
                      f"운송 중 광석: {status['in_transit_ore']}\n"
                      f"납부된 광석 관세(수입에 포함): {status['paid_duty']}")
            diplomacy = "\n".join(f"{war['opponent']}: {war['state']}" for war in status["wars"])
            titles = ("재정", "법률·관세·세금", "광석 시장", "외교")
        for card, title in zip(self.cards, titles):
            card.configure(text=title)
        self.heading.set(headline)
        self.budget.set(budget)
        self.policy.set(policy)
        self.market.set(market)
        self.diplomacy.set(diplomacy or "현재 분쟁 없음.")
        if not factory:
            self.tariff.set(self._percent(status["tariff_ppm"]))
            self.tax.set(self._percent(status["income_tax_ppm"] if pending_tax is None else pending_tax))
        self.current = status

    def new(self):
        self._send("new " + self._protocol_tag(self.country.get()))

    def step(self):
        if self.current:
            self._send("step 1")

    def set_tariff(self):
        if not self.current or self._industry(self.current):
            return
        value = self.tariff.get().strip()
        if not value or not value.isascii() or not value.isdecimal() or len(value) > 3 or int(value) > 100:
            self.notice.set("오류: 관세율을 0부터 100 사이의 정수로 입력하세요.")
            return
        self._send("tariff " + value)

    def set_income_tax(self):
        if (not self.current or self._industry(self.current)
                or self.current["pending_income_tax_ppm"] is not None):
            return
        value = self.tax.get().strip()
        if not value or not value.isascii() or not value.isdecimal() or len(value) > 3 or int(value) > 100:
            self.notice.set("오류: 소득세율을 0부터 100 사이의 정수로 입력하세요.")
            return
        self._send("income-tax " + value)

    def set_spending_cap(self):
        if not self._budget(self.current) or self.current["pending_civilian_spending_cap"] is not None:
            return
        value = self.spending.get().strip()
        if not value or not value.isascii() or not value.isdecimal() or len(value) > 7 or int(value) > 1_000_000:
            self.notice.set("오류: 하루 지출 한도를 0부터 1000000 사이의 정수로 입력하세요.")
            return
        self._send("spending-cap " + value)

    def _file(self, verb, path):
        if not path:
            return
        if "\n" in path or "\r" in path or path != path.strip():
            self.notice.set("오류: 저장 경로에는 줄바꿈이나 앞뒤 공백을 넣을 수 없습니다.")
            return
        self._send(verb + " " + path)

    def save(self):
        if not self.current or self.busy or self.closing or self.disconnected:
            return
        self._file("save", filedialog.asksaveasfilename(
            parent=self.root, title="시나리오 저장", defaultextension=".vic25",
        ))

    def load(self):
        if self.busy or self.closing or self.disconnected:
            return
        self._file("load " + self._protocol_tag(self.country.get()), filedialog.askopenfilename(
            parent=self.root, title="시나리오 불러오기",
        ))

    def close(self):
        if self.closing:
            return
        self.closing = True
        self._update_controls()
        self.engine.commands.put("quit")
        deadline = monotonic() + 1

        def finish():
            child = self.engine.process
            if child.poll() is None and monotonic() < deadline:
                self.root.after(30, finish)
                return
            if child.poll() is None:
                child.kill()
                child.wait()
            self.root.destroy()

        finish()


def main():
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python scripts/play_panel.py <vic25_play path>")
    root = None
    try:
        root = tk.Tk()
        PlayPanel(root, sys.argv[1])
    except (OSError, tk.TclError) as exc:
        if root is not None:
            root.destroy()
        raise SystemExit(f"VIC2.5 화면을 열지 못했습니다: {exc}") from exc
    root.mainloop()


if __name__ == "__main__":
    main()
