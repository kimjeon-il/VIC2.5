# C-E3 — 가계·소득·금융·투자 상세 설계

- 상태: **설계 확정 / 구현 전** · 경제 계약 `phase-c-economy/1.0`
- 상위: [C-D0](../C-D0_COMMON_CONTRACT.md), [POP-D0~D3](../../pop/README.md); 연계 [E1](C-E1_FIRMS_LABOR_PRODUCTION.md), [E2](C-E2_GOODS_MARKETS_PRICES.md), [E4](C-E4_FISCAL_PUBLIC_SECTOR.md).

## E3-1. 가계와 자산 소유자의 엄격한 분리

- `HouseholdPool`은 소비·공동 생활비를 집계하는 **통계적 가구 풀**이다. 실제 개인/배우자/친자 관계를 생성하지 않는다. 풀의 인구수는 `PopulationSlice.count`의 합이다.
- `HouseholdBudget`은 `household_pool_id, month, expected_needs, available_funds, assigned_in_kind, spending_priority, planned_orders`를 참조하는 파생/계획 객체다. 가계 예금의 **원본 잔액은 FinancialLedger**에 있다.
- 소유자(`OwnerRef`)는 Person-like allocation partition/HouseholdPool, Firm, Government, FinancialInstitution, Estate 등 여러 타입의 주체가 될 수 있다. 명시적으로 공동소유하는 경우 가구 풀 명의 계좌와 분배 규칙을 쓰고, 개인/부분집단 소유 계좌를 가구 자산에 임의 합산·이체하지 않는다.
- `PopulationSlice`가 분할될 때 기존 금융자산/채무와 지분을 무조건 인원비례로 쪼개지 않는다. 명시적 소유/계약 자료나 provenance를 보유한 allocation rule이 필요하다.
- 아동·고령자·현역·병상 인구를 노동소득자로 자동 간주하지 않는다. 부양 필요·현물 소비·실제 이전지급의 계산에는 이들 모두 포함한다.

## E3-2. 소득의 분류: 발생과 지급

| 소득 유형 | 원천 | 기록 조건 |
|---|---|---|
| 현금 임금 | EmploymentLedger × 실제 급여 | 근무/계약에 따른 기업·정부 임금채무 + 실제 FinancialLedger 지급 |
| 자영업 소득 | OwnAccountActivity 실제 생산/판매 | 판매대금, 비용·재고의 귀속. 매출 전 현금을 생성하지 않음 |
| 현물 자급소비 | OwnAccountActivity 상품·가계 재고 | InventoryLedger로 실물 인도/소비, 비화폐 실질 후생 반영 |
| 배당 | OwnershipLedger 유효 지분 | 이익/배당선언 ≠ 수령; 실제 금전 지급 시 POP 현금 소득 |
| 이자·지대 | 예금·채권·토지 임대/권리 | 실제 수령과 발생 채권 구분 |
| 사회적 이전 | FiscalLedger 유효 자격 | 정부지급 의무와 실제 수령 분리 |
| 사적 이전 | 송금·가구 공동예산 이체 | 금융 분개 시에만 수령 |
| 자본이득 | 자산가치/매매차익 | 미실현 평가증가는 구매력이나 임금 아님 |

`cash_income_received`는 실제 외부에서 지급받은 금전, `accrued_income`은 발생주의 채권/수익, `in_kind_value`는 가격/원가 평가에 따른 비화폐 가치, `transfer_within_pool`은 이미 존재한 가구자금을 옮긴 내역이다. 이 네 값을 모두 더해 가처분 현금으로 사용하면 **이중계산**이므로 금지한다.

월별 예:
```text
gross_payroll = actual_net_pay_to_workers + tax_withheld + other_legal_withholdings
cash_available_at_stage_6 = opening_spendable_balance
                          + actually_received_payments_up_to_stage_5
                          - payments_and_reservations_already_executed
                          - obligations_reserved_before_stage_6
```
임금의 원천징수는 고용주 지급총액 안에 포함되며, 노동자 세후입금에 같은 총임금을 별도로 다시 가산하지 않는다.

## E3-3. 소비·저축·실제 생활수준

1. 단계 6에서 각 HouseholdPool의 유효 지불계좌 잔액/사용 가능한 신용과 연령·건강·가구구성·지역·시대의 욕구를 확인한다.
2. 이미 가구가 보유한 현물 식량/연료·내구재 사용/공적 무상서비스의 실제 공급가능량을 먼저 배정한다. 현물 소비는 가계에 현금소득이 생기는 것이 아니다.
3. 생존필수·법정의무·주거·의료·교육·기타 소비·사치의 우선순위(시나리오 규칙으로 선택)를 따라 실효 구매주문과 별도 예약을 생성한다. 동률/부분구매의 정수 배분 방식은 E2에 따른다.
4. 단계 7에서 인도된 상품/서비스만 실제 소비/장래 내구자산으로 처리한다. 미체결 주문은 unmet_need 및 시장관측값이며 실제 지출 아님.
5. 남는 유동성은 여전히 동일 은행계좌에 존재한다. **저축을 별도 화폐 증식 사건으로 가산하지 않는다.** 단계 9의 저축결정은 가용현금의 예금이체/채권·지분 매입 등 명시적 자산재배분만 수행한다.
6. `NeedFulfillment`: 충족량/최소 필요량/권장량을 욕구별로 보존. `StandardOfLiving`은 실제 영양·의복·주거·의료·내구재 서비스·안전 등 가중 만족 결과로 산출하고 소득만으로 임의 인상하지 않는다.
7. 가격·가계규모·부양부담·지역 관행을 반영하여 생활비·실질소득과 구매력 지수를 계산한다. 질병/장애는 보건요구·지원/편의 제공의 비용과 실제 기능 요건으로 반영하며 일률적 효용 페널티로 환원하지 않는다.
8. 분포상의 부·소득 차이가 행동에 중요하면 `allocation_partition_id`와 출처가 있는 금융분포 자료로 세분화한다. 재산 등급을 POP 고정 정체성으로 만들지 않는다.

## E3-4. 복식부기 금융 원장

### 계정 및 계약

| 객체 | 최소 필드/역할 |
|---|---|
| `FinancialAccount` | account_id, owner_id, institution_id, currency_id, ledger_class, revision, spendability |
| `JournalEntry` | transaction_id, source_event_id, account_id, debit_minor, credit_minor, currency_id, effective_month |
| `DepositContract` | deposit_id, depositor_owner, bank_id, currency, callable/maturity, rate_rule, linked_accounts |
| `LoanContract` | loan_id, lender, borrower, currency, principal_outstanding, rate_rule, due_schedule, collateral_refs, status |
| `Bond` | bond_id, issuer, holders(Ownership), currency, issue_principal, coupon, maturity, seniority |
| `CreditFacility` | facility_id, bank_or_lender, eligible_borrower, undrawn_limit, approved_terms; 미인출 한도 자체는 예금 아님 |
| `FundingReservation` | reservation_id, account_id, currency, amount, purpose, expiry, priority, revision |
| `Default/ResolutionEvent` | contract_ref, due/paid/unpaid, claimant/obligor, bankruptcy_priority, actual write-down & asset transfer refs |

금융채권/채무의 정본은 FinancialLedger이며 OwnershipLedger는 지분·채권증서·담보권의 **권리 귀속**을 참조한다. 채권증서 자체의 얼굴값과 FinancialLedger 채무를 별개의 추가 재산으로 더하지 않는다.

모든 분개는 경제주체별 차변=대변이며, 동일 통화별 자산·부채와 기관 간 결제의 대응관계를 검증한다. 단일 `Money` 정수에 통화 없는 숫자를 섞어 더할 수 없다. 계좌 잔액은 회계 방향(자산/부채)에 따라 처리하며, `spendable_balance=positive_available_balance-unreleased_reservations`이다. 명시적 승인 없는 마이너스 예금과 중복지출은 거부한다.

### 예시: 은행의 신규 대출 100

| 장부 | 차변 | 대변 |
|---|---|---|
| 은행 | 대출채권 100 | 고객 예금부채 100 |
| 차입 기업 | 예금자산 100 | 대출채무 100 |

고객이 재화 대금을 송금하면 기업 예금은 감소하고 판매자의 예금이 증가한다. 다른 은행으로 보내면 **은행 간 결제자산/준비금**도 이동·차입되어야 하므로 유동성 제약을 피할 수 없다. 신규 대출로 예금이 생성되는 것은 허용되지만 이를 은행의 무상자산 생성으로 처리하지 않는다.

### 금융기관의 대출 승인 (단계 1)

- 대출자격·법률/금리제도·담보 가치·차주 예상현금흐름·기존 부채·은행 자본손실 흡수력·유동성/결제 접근을 평가한다.
- 일반/시대별 은행업 허용 여부는 금융제도·정치법률로 결정한다. 은행이 없는 시대에는 대부업자·상인신용·자기자금 등이 별도 자금제약 아래 작동한다.
- `credit_limit` 승인만으로 예금 잔액이 늘지 않는다. `LoanDrawdown`의 실제 은행/차입자 분개가 발생한 만큼만 차입 재원이 생긴다.
- 비용 예약과 실제 지급은 원장상 별개. 같은 운전자금 100으로 임금 100과 원재료 100을 동시 확보할 수 없다.
- 법정 준비금/정책금리/금융규제는 유효한 제도 규칙에 따르며, 단순 고정 예금승수로 모든 대출을 계산하지 않는다.

## E3-5. 이자·상환·연체·부도

- 이자는 `principal × contract_rate × accrual_period`를 버전 있는 고정소수점/반올림 규칙으로 발생시킨다. 이자발생은 현금 지급과 다르다.
- 원금상환 시 차입자 예금자산·대출채무가 감소하고 은행 예금부채·대출채권이 감소한다. **원금상환은 기업의 생산비/가계 소비비가 아니다.**
- 이자 지급은 차주 비용과 채권자/은행 수익의 대응 거래이며 월 결산(단계 8)에 발생주의/지급상태를 각각 기록한다. 월초 만기 의무는 단계 1에서 계획/유동성 확보; 단계 8에서는 실제 매출 후 잔액으로 채무를 이행하거나 명시적 연체/부도를 기록할 수 있다.
- 현금 부족만으로 확정된 금융원장을 사후 마이너스 잔액으로 바꾸지 않는다. 차환, 조건변경, 유예, 부도, 담보처분, 회생, 파산을 `Default/ResolutionEvent`로 단계별 처리한다.
- 부실채권 손실은 대출자산 상각과 은행 자본/손익 감소로 반영한다. 고객예금 자체는 부실대출 상각의 역분개로 임의 삭감하지 않는다. 은행 파산 시 예금보호·채권순위·정리 등 별도 법률/재정 거래가 필요하다.
- 최초 엔진은 규제·파산의 **금액/상태 정합성**을 구현하고, 금융기관 신뢰·패닉/뱅크런의 행동 계수는 별도 규칙파일에서 보정한다.

## E3-6. 저축·소유권·배당·투자

- 기업 이윤은 기업에 유보된다. 주주에게 배당하려면 `DividendDeclaration`(stage 9)과 후속 월의 `DividendPayable` 및 실제 `FinancialLedger` 지급이 필요하며 이익 전부를 무조건 POP에게 이전하지 않는다.
- `OwnershipLedger`는 개인/가구/기업/정부/금융기관의 주식·토지·설비·채권 권리 비율을 관리한다. 신규 주식 발행시 신주/주주 청구권과 실제 출자금 지급이 같은 batch에 있어야 한다.
- 부동산·유가증권의 평가가격 변화는 `Valuation` 파생 뷰이며 즉시 사용 가능한 현금이 아니다. 자산 매각·담보대출·배당·임대료 수령에 실제 지급 분개가 있어야 한다.
- 기업의 자본확충은 이익유보, 주식 신규 발행·유상증자, 실행 대출, 정부 출자/보조금 등만 사용한다. 소요 설비·건설 자재·인력/기간은 E1/E2/E5에서 실제 투입한다.
- 월말(9단계) 저축/투자계획은 **다음 월 단계 1부터** 투자/대출/매수 준비에 반영. 이미 단계 7이 종료된 뒤 당월 계획만으로 설비를 구매·가동시키지 않는다.
- 국가 기업·정부의 지분 배당은 동일 회계 규약을 사용하며 정부예산은 실제 수령 후에만 증가한다.

## E3-7. 소득·부의 측정 및 정치적 연계

가계별 명목/실질 가처분소득(현금/발생주의 구분), 현물 자급소비, 소비충족률, 주거·식량 지출부담, 예금/대출/채권/지분·토지의 실제 소유분포, 부채상환 부담, 금융기관 자본비율, 연체/부도율, 소비자물가/실질 구매력을 제공한다. 정치 시스템은 이러한 **원장의 파생 지표와 적용 법률**을 읽되 별도 가계 자금 원본을 만들지 않는다.

## E3 검증 게이트

- E3-G1: 같은 POP의 임금 100(원천징수 10)은 가계 순수령 90·정부 세금 10으로 한 번만 분개된다.
- E3-G2: 대출 100 실행 → 은행 자산/부채 +100, 차입자 자산/부채 +100, 예금 창출 허용.
- E3-G3: 은행 간 20 송금은 차입은행 예금부채 및 지급결제 자산·수취은행 계정 대응이 일치.
- E3-G4: 대출 원금 30 상환을 생산비/소비로 잘못 계산하지 않는다.
- E3-G5: 미실현 이익/자산평가 상승·미지급 배당 선언을 소비재 지불가능액으로 더하지 않는다.
- E3-G6: 가족 공동예산을 쓰더라도 개인/가구 계좌의 자금을 추가로 생성하지 않는다.
- E3-G7: 자급식량 소비의 실제 소비충족률은 증가하되 가계 현금은 변하지 않는다.
- E3-G8: 부도·청산으로 채권자 손실/차주 채무 처리 및 잔여 자산 소유권이 명시적 분개로 맞는다.
- E3-G9: 같은 금융계좌 100으로 임금 100·원재료 100을 각각 예약하려 하면 두 번째 예약을 거부하거나 축소한다.
