"""Native VIC2.5 trial and historical preview panel; requires Python/Tk."""
from datetime import date
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
    LEGISLATURE = "A 입법 시범판"
    EDUCATION = "A 교육 시범판"
    CAMPAIGN = "A 공동 캠페인"
    EXPANSION = "A 민간 증설 시범판"
    SUPPLY = "A 공급망 복구 시범판"
    STAPLES = "A 가계 소비 시범판"
    STEEL = "A 철강·건설 시범판"
    HISTORY = "역사 지리 미리보기"

    @classmethod
    def _protocol_tag(cls, choice):
        if choice == cls.LEGISLATURE:
            return "LAW-A"
        if choice == cls.EDUCATION:
            return "EDU-A"
        if choice == cls.CAMPAIGN:
            return "CAMPAIGN-A"
        if choice == cls.STAPLES:
            return "STAPLES-A"
        if choice == cls.STEEL:
            return "STEEL-A"
        if choice == cls.SUPPLY:
            return "SUPPLY-A"
        if choice == cls.EXPANSION:
            return "EXPANSION-A"
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
        self.law_party = tk.StringVar(root, "")
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
        ttk.Label(frame, text="시범판 또는 역사 지리 미리보기 · 지도 화면 없음").grid(row=1, sticky="w")
        setup = ttk.Frame(frame, padding=(0, 10))
        setup.grid(row=2, sticky="ew")
        ttk.Label(setup, text="시작·불러올 국가/시나리오:").grid(row=0, column=0, padx=(0, 6))
        choices = ("A", "B", self.FACTORY, *self.BUDGETS, self.LEGISLATURE,
                   self.EDUCATION, self.CAMPAIGN, self.EXPANSION, self.SUPPLY, self.STAPLES, self.STEEL, self.HISTORY)
        self.selector = ttk.Combobox(setup, textvariable=self.country, values=choices,
                                     width=16, state="readonly")
        self.selector.grid(row=0, column=1, padx=(0, 8))
        self.new_button = ttk.Button(setup, text="새로 시작", command=self.new)
        self.load_button = ttk.Button(setup, text="불러오기…", command=self.load)
        self.save_button = ttk.Button(setup, text="다른 이름으로 저장…", command=self.save)
        for column, button in enumerate((self.new_button, self.load_button, self.save_button), 2):
            button.grid(row=0, column=column, padx=(0, 5))
        self.history_label = ttk.Label(setup, text="미리 볼 국가:")
        self.history_label.grid(row=1, column=0, sticky="w", pady=(6, 0))
        self.history_choice = ttk.Combobox(setup, width=16, state="readonly")
        self.history_choice.grid(row=1, column=1, sticky="w", pady=(6, 0))
        self.history_choice.bind("<<ComboboxSelected>>", lambda event:
                                 self._send("choose-country " + self.history_choice.get()))
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
        self.ore_pact_button = ttk.Button(actions, text="광석 면제 협정 제안", command=self.propose_ore_pact)
        self.ore_pact_button.grid(row=3, column=0, sticky="w", padx=(0, 8), pady=(4, 0))
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
        self.build_button = ttk.Button(actions, text="건설 신청 후 하루 진행", command=self.public_build)
        self.build_button.grid(row=3, column=1, columnspan=3, sticky="w", pady=(4, 0))
        self.law_party_choice = ttk.Combobox(actions, textvariable=self.law_party,
                                             width=17, state="readonly")
        self.law_party_choice.grid(row=0, column=1, columnspan=2, padx=(0, 6), sticky="w")
        self.law_party_choice.bind("<<ComboboxSelected>>", lambda event: self._update_controls())
        self.law_action_button = ttk.Button(actions, command=self.advance_bill)
        self.law_action_button.grid(row=0, column=3, sticky="w")
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
        history = self._history(self.current)
        can_advance = self.current and (not history or self.current["country"] is not None)
        for widget in (self.save_button, self.step_button):
            widget.state(["!disabled"] if ready and can_advance else ["disabled"])
        if history:
            self.history_label.grid()
            self.history_choice.grid()
            self.history_choice.configure(state="readonly" if ready else "disabled")
        else:
            self.history_label.grid_remove()
            self.history_choice.grid_remove()
        factory = self._industry(self.current)
        legislature = self._legislature(self.current)
        campaign = self._campaign(self.current)
        direct_actions = not (history or factory or legislature or campaign or self._expansion(self.current)
                              or self._staples(self.current))
        can_campaign_act = self._campaign_can_act(self.current)
        pact = self.current.get("ore_trade_pact") if can_campaign_act else None
        self.ore_pact_button.state(["!disabled"] if ready and pact and pact["can_propose"] else ["disabled"])
        if pact:
            self.ore_pact_button.grid()
        else:
            self.ore_pact_button.grid_remove()
        bill = self.current.get("law_bill") if legislature or can_campaign_act else None
        campaign_tax = bool(can_campaign_act and self.current["campaign_tax_allowed"])
        elected_spending = self._campaign_spending_scope(self.current)
        spending_vote = elected_spending and self.current["campaign_spending_cap_allowed"]
        campaign_party = self._selected_bill_party(bill) if (campaign_tax or spending_vote) and bill else None
        campaign_tariff = self._campaign_tariff_scope(self.current)
        can_set_tariff = campaign_tariff and self.current["campaign_ore_tariff_allowed"]
        history_tariff = history and "grain_trade" in self.current
        self.tariff_label.configure(text="곡물 수입 관세율(%):" if history_tariff else
                                    "광석 수입 관세율(%):" if campaign_tariff else "수입 관세율(%):")
        for widget in (self.tariff_label, self.tariff_button, self.tariff_entry):
            enabled = ready and self.current and (direct_actions or can_set_tariff or history_tariff)
            widget.state(["!disabled"] if enabled else ["disabled"])
            if direct_actions or campaign_tariff or history_tariff:
                # Row 0 is the elected party picker in the shared campaign.
                widget.grid(row=4 if campaign_tariff else 0, pady=(4, 0) if campaign_tariff else 0)
            else:
                widget.grid_remove()
        history_tax = history and "income_tax_ppm" in self.current
        direct_tax = (ready and self.current and (direct_actions or history_tax)
                      and self.current["pending_income_tax_ppm"] is None)
        self.tax_button.configure(text="소득세 표결 후 하루 진행" if campaign
                                  else "소득세 제출 후 하루 진행")
        for widget in (self.tax_label, self.tax_entry, self.tax_button):
            can_edit = (campaign_tax and ready and (campaign_party or widget is not self.tax_button)
                        if campaign else direct_tax)
            widget.state(["!disabled"] if can_edit else ["disabled"])
            if direct_actions or can_campaign_act or history_tax:
                widget.grid()
            else:
                widget.grid_remove()
        budget = self._budget(self.current)
        spending_ready = ready and (spending_vote or
                    (budget and self.current["pending_civilian_spending_cap"] is None))
        self.spending_button.configure(text="지출 한도 표결 후 하루 진행" if elected_spending
                                       else "지출 한도 제출 후 하루 진행")
        for widget in (self.spending_label, self.spending_entry, self.spending_button):
            can_edit = spending_ready and (not elected_spending or campaign_party or
                                          widget is not self.spending_button)
            widget.state(["!disabled"] if can_edit else ["disabled"])
            if budget or elected_spending:
                widget.grid()
            else:
                widget.grid_remove()
        construction = self._build_quote(self.current)
        build_command, build_label = self._build_action(self.current, construction)
        self.build_button.configure(text=build_label)
        self.build_button.state(["!disabled"] if ready and build_command else ["disabled"])
        if construction:
            self.build_button.grid()
        else:
            self.build_button.grid_remove()
        if bill:
            command, label = self._law_action(self.current)
            voting = (campaign_tax or spending_vote or self._build_budget_vote(construction)
                      or (bill["phase"] == "submitted" and bill["vote_ready"]))
            self.law_party_choice.configure(state="readonly" if ready and voting else "disabled")
            if voting:
                self.law_party_choice.grid()
            else:
                self.law_party_choice.grid_remove()
            self.law_action_button.configure(text=label)
            self.law_action_button.state(["!disabled"] if ready and command else ["disabled"])
            self.law_action_button.grid()
        else:
            self.law_party_choice.grid_remove()
            self.law_action_button.grid_remove()
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
                    opening = command.startswith(("new ", "load ", "history-open ", "history-load "))
                    if opening:
                        self.law_party.set("")
                    self._show(reply["status"])
                    if opening:
                        choice = self.current["country"]
                        if self._history(self.current):
                            choice = self.HISTORY
                        elif self._industry(self.current):
                            choice = self.FACTORY
                        elif self._budget(self.current):
                            choice += " 예산 시범판"
                        elif self._campaign(self.current):
                            if self.current.get("steel_chain"):
                                choice = self.STEEL
                            else:
                                choice = self.SUPPLY if self.current.get("supply_recovery") else self.CAMPAIGN
                        elif self._staples(self.current):
                            choice = self.STAPLES
                        elif self._expansion(self.current):
                            choice = self.EXPANSION
                        elif self._education(self.current):
                            choice = self.EDUCATION
                        elif self._legislature(self.current):
                            choice = self.LEGISLATURE
                        self.country.set(choice)
                    if command.startswith("income-tax "):
                        self.notice.set("소득세 변경을 예약했습니다.")
                    elif command.startswith("spending-cap "):
                        self.notice.set("공공사업 지출 한도를 예약했습니다.")
                    elif command.startswith("public-build-budget "):
                        self.notice.set("건설 예산이 승인됐습니다. 공사는 아직 신청하지 않았습니다.")
                    elif command == "public-build":
                        self.notice.set("건설 신청을 접수했습니다.")
                    elif command == "ore-pact":
                        self.notice.set("광석 관세 면제 협정을 제안했습니다.")
                    elif self._history(self.current) and self.current["country"] is None:
                        self.notice.set("미리 볼 국가를 선택하세요.")
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
    def _history(status):
        return bool(status and "history" in status)

    @staticmethod
    def _industry(status):
        return bool(status and status.get("scenario") == "industry-v1")

    @staticmethod
    def _budget(status):
        return bool(status and status.get("scenario") == "budget-v1")

    @staticmethod
    def _legislature(status):
        return bool(status and status.get("scenario") in ("law-v1", "education-v1"))

    @staticmethod
    def _education(status):
        return bool(status and status.get("scenario") == "education-v1")

    @staticmethod
    def _campaign(status):
        return bool(status and status.get("scenario") == "campaign-v1")

    @staticmethod
    def _expansion(status):
        return bool(status and status.get("scenario") == "expansion-v1")

    @staticmethod
    def _staples(status):
        return bool(status and status.get("scenario") == "staples-v1")

    @staticmethod
    def _basket_text(case):
        return (f"누적 실수령 임금: {case['wage_received']}\n"
                f"가계 잔액: {case['cash']}\n"
                f"식량 구매: {case['groceries_bought']}\n"
                f"의류 구매: {case['clothes_bought']}\n"
                f"목재 부품 구매: {case['fittings_bought']}")

    @classmethod
    def _campaign_can_act(cls, status):
        return cls._campaign(status) and status["can_act"] and status["country"] == status["player_country"]

    @classmethod
    def _campaign_tariff_scope(cls, status):
        return cls._campaign_can_act(status) and "campaign_ore_tariff_allowed" in status

    @classmethod
    def _campaign_spending_scope(cls, status):
        return cls._campaign_can_act(status) and "campaign_spending_cap_allowed" in status

    @staticmethod
    def _seat_label(seat):
        return f"{seat['party']} ({seat['seats']}석)"

    def _selected_bill_party(self, bill):
        chosen = self.law_party.get()
        for seat in bill["seats"]:
            party = seat["party"]
            if (seat["seats"] > 0 and chosen == self._seat_label(seat)
                    and party and not any(char.isspace() for char in party)):
                return party
        return None

    def _law_action(self, status):
        bill = status["law_bill"]
        phase = bill["phase"]
        if phase == "available":
            return ("bill-introduce" if bill["seats"] else None, "법안 발의 후 하루 진행")
        if phase == "submitted" and bill["vote_ready"]:
            party = self._selected_bill_party(bill)
            return ("bill-approve " + party if party else None, "선택 정당으로 표결 후 하루 진행")
        if phase == "approved":
            return ("bill-promulgate", "공포 후 하루 진행")
        if phase in ("submitted", "promulgated"):
            reviewing = phase == "submitted"
            due = bill["next_review" if reviewing else "effective_date"]
            label = "다음 심의일까지 진행" if reviewing else "시행일까지 진행"
            if due:
                # The native cursor is the next date to process, not the
                # last date processed. Include the scheduled day itself.
                days = (date.fromisoformat(due) - date.fromisoformat(status["date"])).days + 1
                if 1 <= days <= 366:
                    return (f"step {days}", label)
            return (None, label)
        return (None, "추가 법안 조치 없음")

    def _bill_text(self, bill, proposal):
        phase = {"available": "발의 전", "submitted": "심의 중", "approved": "가결",
                 "promulgated": "공포됨", "effective": "시행 중", "rejected": "부결"}[bill["phase"]]
        if bill["phase"] == "submitted" and bill["vote_ready"]:
            phase = "표결 가능"
        policy = f"{proposal}: {phase}\n가결 기준: {bill['threshold']}석"
        if bill["seats"]:
            policy += "\n" + " · ".join(f"{s['party']}: {s['seats']}석" for s in bill["seats"])
        else:
            policy += "\n선거 결과 대기 중"
        if bill["next_review"]:
            policy += f"\n다음 심의일: {bill['next_review']}"
        if bill["effective_date"]:
            policy += f"\n시행일: {bill['effective_date']}"
        self.law_party_choice.configure(values=tuple(self._seat_label(s) for s in bill["seats"]))
        return policy

    def _show_history(self, status):
        history, tag = status["history"], status["country"]
        self.history_choice.configure(values=history["countries"])
        self.history_choice.set(tag or "")
        perspective = f"국가 {tag}" if tag else "국가 선택"
        self.heading.set(f"역사 지리 미리보기  |  {perspective}  |  {status['date']}")
        territory = history["territory"]
        self.budget.set((f"육지 프로빈스: {territory['land']}\n"
                         f"행정구역: {territory['administrations']}\n"
                         f"해안 프로빈스: {territory['coastal']}")
                        if territory is not None else "목록에서 미리 볼 국가를 선택하세요.")
        policy = f"자료 ID: {history['dataset']}"
        has_fiscal = "income_tax_ppm" in status
        if has_fiscal:
            self.budget.set(self.budget.get() + f"\n국고: {status['treasury']} · 누적 세입: {status['revenue']}")
            pending = status["pending_income_tax_ppm"]
            policy += f"\n소득세율: {self._percent(status['income_tax_ppm'])}%"
            if pending is not None:
                policy += (f"\n소득세 예약: {self._percent(pending)}% "
                           f"({status['pending_income_tax_date']} 시행)")
            self.tax.set(self._percent(status["income_tax_ppm"] if pending is None else pending))
        if grain_trade := status.get("grain_trade"):
            tariff = self._percent(grain_trade["tariff_ppm"])
            self.tariff.set(tariff)
            policy += f"\n곡물 수입 관세율: {tariff}% · 누적 관세 수입: {grain_trade['duties_paid']}"
        self.policy.set(policy)
        economy = history["economy"]
        if economy is None:
            market = "경제 자료 없음. 시작 파일에 입력된 지역에만 표시됩니다."
        else:
            market = (f"자료가 있는 프로빈스: {economy['covered_provinces']}/{territory['land']}\n"
                      f"입력 지역의 인구: {economy['people']}명\n"
                      f"식량 재고: {economy['food_stock']}\n"
                      f"가계 잔액: {economy['household_cash']} · 판매자 잔액: {economy['seller_cash']}")
            if economy.get("mills"):
                market += f"\n식량 작업장: {economy['mills']}곳 · 원료 곡물: {economy['grain_stock']}"
                if economy.get("grain_in_transit"):
                    market += f" · 운송 중: {economy['grain_in_transit']}"
            if farms := economy.get("grain_farm"):
                market += (f"\n곡물 농장: {farms['sites']}곳 · 누적 수확: {farms['harvested']} "
                           f"· 출하 가능: {farms['saleable']}")
            if prices := economy.get("local_food_prices"):
                low, high = prices["minimum"], prices["maximum"]
                price = str(low) if low == high else f"{low}~{high}"
                market += (f"\n변동가 지역의 식품 단가: {price} "
                           f"· 전날 살 돈은 있었지만 못 산 양: {prices['purchase_gap']}")
        self.market.set(market)
        scope = ("입력된 시험용 정부에서만 소득세를 바꿀 수 있습니다. "
                 "새 세율은 다음 달 첫날 처리부터 임금 원천징수에 적용됩니다."
                 if has_fiscal else "이 국가에는 조작 가능한 재정·정부 자료가 없습니다.")
        self.diplomacy.set(scope + " 식량 생산·임금·구매는 입력된 지역에서 자동으로 처리됩니다.")
        titles = ("영토·국고" if has_fiscal else "선택 국가의 영토",
                  "자료·소득세" if has_fiscal else "시작 자료", "지역 식량 시장", "미리보기 범위")
        for card, title in zip(self.cards, titles):
            card.configure(text=title)
        self.current = status

    def _show(self, status):
        if self._history(status):
            self._show_history(status)
            return
        factory = self._industry(status)
        expansion = self._expansion(status)
        staples = self._staples(status)
        campaign = self._campaign(status)
        recovery = status.get("supply_recovery") if campaign else None
        steel = status.get("steel_chain") if campaign else None
        perspective = self.FACTORY if factory else f"국가 {status['country']}"
        if steel:
            perspective += " · 철강·건설 시범판"
        elif campaign:
            perspective += " · 공급망 복구 시범판" if recovery else " · 공동 캠페인"
        elif expansion:
            perspective += " · 민간 증설 시범판"
        elif staples:
            perspective += " · 가계 소비 시범판"
        elif self._budget(status):
            perspective += " · 예산 시범판"
        elif self._education(status):
            perspective += " · 교육 시범판"
        elif self._legislature(status):
            perspective += " · 입법 시범판"
        headline = f"{perspective}  |  {status['date']}  |  진행 기록 {status['commit']}"
        construction = self._build_quote(status)
        cost_label = "누적 비용(공사비 별도)" if construction else "누적 지출"
        budget = (f"국고: {status['treasury']}\n"
                  f"누적 수입: {status['revenue']}\n"
                  f"{cost_label}: {status['expenditure']}\n"
                  f"부채: {status['debt']}")
        elected_spending = self._campaign_spending_scope(status)
        if self._budget(status) or elected_spending:
            cap = status["civilian_spending_cap"]
            pending = status["pending_civilian_spending_cap"]
            default_cap = status["campaign_spending_cap_max"] if elected_spending else None
            shown_cap = cap if cap is not None else default_cap
            budget += f"\n공공사업 하루 한도: {shown_cap if shown_cap is not None else '미설정'}"
            budget += f"\n공공사업 누적 지급: {status['public_program_paid']}"
            if pending is not None:
                budget += f"\n예약 한도: {pending} ({status['pending_civilian_spending_date']} 시행)"
            self.spending.set(str(pending if pending is not None else shown_cap or 0))
        if construction:
            state = construction["state"]
            if self._campaign(status):
                budget += ("\n건설 예산: 승인됨" if construction["budget_approval"] == "approved"
                           else "\n건설 예산: 승인 전")
            label = {"available": "미신청", "queued": "신청 접수",
                     "paid": "공사비 지급 완료", "commissioned": "준공"}[state]
            budget += f"\n공공 공구공장: {label}"
            if state in ("available", "queued"):
                budget += (f"\n국고 공사비 {construction['invoice']} · 보유 최소액 {construction['treasury_reserve']}"
                           f"\n필요 철강 {construction['steel_required']} "
                           f"(예상 시공사 구매비 {construction['steel_purchase_cost']})")
                if state == "queued":
                    held = construction["contractor_steel"]
                    budget += f" · 확보 {held}"
                    if steel and held < construction["steel_required"]:
                        budget += "\n철강 조달 대기"
                elif self._steel_shortfall(steel, construction):
                    budget += "\n철강 생산 대기"
            else:
                budget += f" · 지급 {construction['invoice_paid']}"
                if state == "commissioned":
                    budget += f" · 설비 용량 {construction['commissioned_capacity']}"
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
        elif staples:
            basket = status["household_staples"]
            policy = ("식량과 의류 우선\n"
                      "예산이 남고 재고가 있으면 목재 부품도 구매합니다.\n"
                      "구매 수량은 누적입니다.")
            market = self._basket_text(basket["necessities_only"])
            diplomacy = self._basket_text(basket["with_discretionary"])
            titles = ("재정", "구매 순서", "낮은 임금 가계", "높은 임금 가계")
        elif expansion:
            result = status["private_expansion"]
            capacity = result["commissioned_capacity"]
            paid = result["paid_construction"]
            if capacity:
                construction_state = "준공 완료"
            elif paid:
                construction_state = "준공 대기"
            else:
                construction_state = "증설 대기"
            policy = (f"{construction_state}\n"
                      f"공사비 지급: {paid}\n"
                      f"준공 설비 용량: {capacity}\n"
                      f"시공사 철강 재고: {result['builder_steel']}")
            market = (f"식량 업체 잔액: {result['maker_cash']}\n"
                      f"누적 판매대금: {result['sales_receipts']}\n"
                      f"증설 설비 생산 횟수: {result['expansion_batches']}\n"
                      f"식량 재고: {result['food_stock']}")
            employment = "고용됨" if result["new_worker_employed"] else "미고용"
            diplomacy = (f"신규 직원: {employment}\n"
                         f"신규 직원 지갑 잔액: {result['new_worker_cash']}")
            titles = ("재정", "민간 공사", "식량 생산·판매", "고용")
        elif self._campaign(status):
            acting = self._campaign_can_act(status)
            budget += f"\n플레이어 조작 국가: {status['player_country']}"
            policy = f"소득세율: {self._percent(status['income_tax_ppm'])}%"
            pending = status["pending_income_tax_ppm"]
            if pending is not None:
                policy += f"\n소득세 예약: {self._percent(pending)}% ({status['pending_income_tax_date']} 시행)"
            if acting and "law_bill" in status:
                policy += "\n" + self._bill_text(status["law_bill"], "최저임금 법안")
                self.tax.set(self._percent(status["income_tax_ppm"]))
            else:
                policy += "\n조회 전용"
            market = (f"도착 광석: {status['delivered_ore']}\n"
                      f"운송 중 광석: {status['in_transit_ore']}")
            chain = status.get("supply_chain") if acting else None
            if steel:
                market = (f"누적 석탄 채굴: {steel['coal_mined']}\n"
                          f"제강소 재고: 철 {steel['mill_iron_stock']} · 석탄 {steel['mill_coal_stock']}\n"
                          f"철강 재고: {steel['steel_stock']}\n"
                          f"철강 생산: {steel['steel_produced']} · 누적 수량\n"
                          f"제강소 판매대금: {steel['steel_receipts']}")
            elif recovery:
                route = {"rail": "철도", "closed": "없음", "road": "도로 우회"}[recovery["route"]]
                market = (f"신규 운송 경로: {route}\n"
                          f"전체 운송 중 광석: {status['in_transit_ore']}\n"
                          f"도로 운송 중 광석: {recovery['road_in_transit_ore']}\n"
                          f"도로 운임 지급: {recovery['road_freight_paid']}\n"
                          f"A 제철소 광석 재고: {recovery['ore_stock']}\n"
                          f"철 생산 횟수: {recovery['iron_batches']} · 철 판매대금: {recovery['smelter_receipts']}")
            elif chain:
                waiting = {"construction": "공구공장 준공 대기", "iron": "철 부족",
                           "wood": "목재 부족", "staff": "인력 부족",
                           "none": None}.get(chain.get("tools_waiting_for"))
                market = (f"운송 중 광석: {status['in_transit_ore']}"
                          f"\n제철소 재고: 광석 {chain['ore_stock']} · 철 {chain['iron_stock']}"
                          f"\n공구공장 재고: 철 {chain['factory_iron_stock']} · 목재 {chain['factory_wood_stock']}"
                          f" · 공구 {chain['tools_stock']}"
                          f"\n목재 공급자 잔액 {chain['wood_seller_cash']}")
                if waiting:
                    market += f" · {waiting}"
                harvested = chain.get("logs_harvested")
                tool_wood = chain.get("toolworks_wood_bought")
                mill_wood = chain.get("mill_wood_bought")
                boards = chain.get("mill_boards_produced")
                if None not in (harvested, tool_wood, mill_wood, boards):
                    market += (f"\n벌목 {harvested} · 원목 확보: 공구공장 {tool_wood} · 제재소 {mill_wood}"
                               f" · 판재 생산 {boards}")
                tools_sold = chain.get("tools_sold")
                buyer_output = chain.get("tool_buyer_output_stock")
                if tools_sold is not None and buyer_output is not None:
                    market += f"\n공구 판매량 {tools_sold} · 목공소 목재 부품 재고 {buyer_output}"
                    board_receipts = chain.get("board_vendor_receipts")
                    if board_receipts is not None:
                        market += f" · 판재 판매대금 {board_receipts}"
                fittings_bought = chain.get("household_fittings_bought")
                fittings_receipts = chain.get("fittings_sales_receipts")
                if fittings_bought is not None and fittings_receipts is not None:
                    market += f"\n가계 부품 구매 {fittings_bought} · 목공소 판매대금 {fittings_receipts}"
            pact = status.get("ore_trade_pact")
            if self._campaign_tariff_scope(status):
                tariff = self._percent(status["tariff_ppm"])
                limit = self._percent(status["max_tariff_ppm"])
                self.tariff.set(tariff)
                effective = " · 협정 적용 관세: 0%" if pact and pact["state"] == "active" else ""
                market = (f"광석 수입 관세: {tariff}% (상한 {limit}%){effective}"
                          f" · 누적 관세 징수: {status['paid_duty']}\n" + market)
            role = "플레이어" if status["peer_is_player"] else "조회 전용"
            diplomacy = (f"{status['peer_country']} · {role}\n"
                         f"국고: {status['peer_treasury']}\n"
                         f"소득세율: {self._percent(status['peer_income_tax_ppm'])}%")
            if pact:
                phase = {"available": "미제안", "proposed": "B 비준 대기",
                         "ratified": "양국 비준 완료", "active": "발효"}[pact["state"]]
                diplomacy += f"\n양국 광석 관세 면제: {phase}"
                if pact["effective_date"]:
                    diplomacy += f" ({pact['effective_date']})"
            wars = status.get("wars")
            if wars is not None:
                labels = {"active": "전쟁 중", "armistice": "휴전 중"}
                current = " · ".join(f"{war['opponent']} ({labels[war['state']]})" for war in wars)
                diplomacy += f"\n현재 분쟁: {current or '없음'}"
            peer_pending = status["peer_pending_income_tax_ppm"]
            if peer_pending is not None:
                diplomacy += (f"\n소득세 예약: {self._percent(peer_pending)}% "
                              f"({status['peer_pending_income_tax_date']} 시행)")
            market_title = "제강·자재 조달" if steel else "광석 운송·생산" if recovery else "공동 교역"
            titles = ("재정", "소득세·의회", market_title, "다른 나라")
        elif self._legislature(status):
            proposal = "공립학교 법안" if self._education(status) else "최저임금 법안"
            policy = self._bill_text(status["law_bill"], proposal)
            if self._education(status):
                education = status["education"]
                market = (f"현지 운영 인력: {education['local_staff']}명\n"
                          f"운영 가능 정원: {education['usable_slots']}\n"
                          f"학교 운영자 잔액: {education['operator_cash']}\n"
                          f"운영 인력 지갑 잔액: {education['staff_cash']}")
                diplomacy = (f"학교 지역: {education['school_primary']}명\n"
                             f"비편입 비교 지역: {education['comparison_primary']}명")
                titles = ("재정", "교육 법안·의석", "학교 운영", "초등교육 이상 취득")
            else:
                minimum = status["minimum_monthly_contract_wage"]
                policy += f"\n월 최저임금: {minimum if minimum else '없음'}"
                market = "새 근로계약에 적용합니다. 기존 계약의 임금은 자동으로 바뀌지 않습니다."
                diplomacy = "선출 의회에서 지지 정당을 명시적으로 골라 표결합니다."
                titles = ("재정", "최저임금 법안·의석", "근로계약 적용", "의회 조작")
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
        if not (factory or expansion or staples or self._legislature(status) or self._campaign(status)):
            self.tariff.set(self._percent(status["tariff_ppm"]))
            self.tax.set(self._percent(status["income_tax_ppm"] if pending_tax is None else pending_tax))
        self.current = status

    def new(self):
        if self.country.get() == self.HISTORY:
            self._file("history-open", filedialog.askopenfilename(
                parent=self.root, title="역사 시작 자료 열기",
            ))
        else:
            self._send("new " + self._protocol_tag(self.country.get()))

    def step(self):
        if self.current:
            self._send("step 1")

    def propose_ore_pact(self):
        pact = self.current.get("ore_trade_pact") if self._campaign_can_act(self.current) else None
        if pact and pact["can_propose"]:
            self._send("ore-pact")

    def set_tariff(self):
        if (not self.current or self._industry(self.current) or self._legislature(self.current)
                or self._expansion(self.current) or self._staples(self.current)):
            return
        if self._history(self.current) and "grain_trade" not in self.current:
            return
        shared = self._campaign(self.current)
        if shared and (not self._campaign_tariff_scope(self.current)
                       or not self.current["campaign_ore_tariff_allowed"]):
            return
        limit = self.current["max_tariff_ppm"] // 10_000 if shared else 100
        value = self.tariff.get().strip()
        if not value or not value.isascii() or not value.isdecimal() or len(value) > 3 or int(value) > limit:
            self.notice.set(f"오류: 관세율을 0부터 {limit} 사이의 정수로 입력하세요.")
            return
        self._send("tariff " + value)

    def set_income_tax(self):
        if (not self.current or self._industry(self.current) or self._legislature(self.current)
                or self._expansion(self.current) or self._staples(self.current)
                or "pending_income_tax_ppm" not in self.current
                or self.current["pending_income_tax_ppm"] is not None):
            return
        value = self.tax.get().strip()
        if not value or not value.isascii() or not value.isdecimal() or len(value) > 3 or int(value) > 100:
            self.notice.set("오류: 소득세율을 0부터 100 사이의 정수로 입력하세요.")
            return
        if self._campaign(self.current):
            if not self._campaign_can_act(self.current) or not self.current["campaign_tax_allowed"]:
                return
            party = self._selected_bill_party(self.current["law_bill"])
            if not party:
                return
            self._send("income-tax " + value + " " + party)
        else:
            self._send("income-tax " + value)

    def set_spending_cap(self):
        elected = self._campaign_spending_scope(self.current)
        if not (elected or self._budget(self.current)) or self.current["pending_civilian_spending_cap"] is not None:
            return
        limit = self.current["campaign_spending_cap_max"] if elected else 1_000_000
        value = self.spending.get().strip()
        if not value or not value.isascii() or not value.isdecimal() or len(value) > 7 or int(value) > limit:
            self.notice.set(f"오류: 하루 지출 한도를 0부터 {limit} 사이의 정수로 입력하세요.")
            return
        if elected:
            if not self.current["campaign_spending_cap_allowed"]:
                return
            party = self._selected_bill_party(self.current["law_bill"])
            if not party:
                return
            self._send("spending-cap " + value + " " + party)
        else:
            self._send("spending-cap " + value)

    def _build_quote(self, status):
        if self._budget(status) or self._campaign_can_act(status):
            return status.get("public_construction")
        return None

    @staticmethod
    def _build_budget_vote(quote):
        return bool(quote and quote["state"] == "available"
                    and quote.get("budget_approval") == "available" and quote["budget_vote_allowed"])

    @staticmethod
    def _steel_shortfall(steel, quote):
        return bool(steel and steel["steel_stock"] + quote["contractor_steel"] < quote["steel_required"])

    def _build_action(self, status, quote):
        build_label = "건설 신청 후 하루 진행"
        if not quote or quote["state"] != "available":
            return None, build_label
        if self._campaign(status) and quote["budget_approval"] != "approved":
            party = self._selected_bill_party(status["law_bill"]) if self._build_budget_vote(quote) else None
            return ("public-build-budget " + party if party else None), "건설 예산 표결 후 하루 진행"
        if self._steel_shortfall(status.get("steel_chain"), quote):
            return None, "철강 생산 대기"
        return "public-build", build_label

    def public_build(self):
        command, _ = self._build_action(self.current, self._build_quote(self.current))
        if command:
            self._send(command)

    def advance_bill(self):
        if self._legislature(self.current) or self._campaign_can_act(self.current):
            command, _ = self._law_action(self.current)
            if command:
                self._send(command)

    def _file(self, verb, path):
        if not path:
            return
        if "\n" in path or "\r" in path or path != path.strip():
            self.notice.set("오류: 저장 경로에는 줄바꿈이나 앞뒤 공백을 넣을 수 없습니다.")
            return
        self._send(verb + " " + path)

    def save(self):
        if (not self.current or self.busy or self.closing or self.disconnected
                or (self._history(self.current) and self.current["country"] is None)):
            return
        self._file("save", filedialog.asksaveasfilename(
            parent=self.root, title="시나리오 저장", defaultextension=".vic25",
        ))

    def load(self):
        if self.busy or self.closing or self.disconnected:
            return
        choice = self.country.get()
        verb = "history-load" if choice == self.HISTORY else "load " + self._protocol_tag(choice)
        self._file(verb, filedialog.askopenfilename(parent=self.root, title="시나리오 불러오기"))

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
