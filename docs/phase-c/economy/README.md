# Phase C-E1~E5 — 경제 시스템 설계 색인

- **상태: 설계 확정 / 통합 C++ 구현 전**
- **경제 계약 버전:** `phase-c-economy/1.0` (2026-11-03)
- **선행:** [C-D0 공통 설계](../C-D0_COMMON_CONTRACT.md) `phase-c-common/1.0`; [POP-D0~D3](../../pop/README.md) `pop-design/1.1`
- **영역:** 역사 대전략형 POP 중심 지역경제·금융·국가재정·교역, 지도 없이 동작 가능.
- **원칙:** 국가경제/생산량/부의 숫자를 이벤트 없이 덧셈하지 않고 실제 거래·생산·금융·인구 원장의 결과로 계산한다. 경기침체·기업도산·물가폭등은 가능한 **모형 결과**이고, 분개 불일치·유령 생산·음수 재고는 **엔진 오류**다.

## 하위 계약

| 단계 | 문서 | 고정 범위 |
|---|---|---|
| C-E1 | [기업·노동·생산](C-E1_FIRMS_LABOR_PRODUCTION.md) | 법인/사업장/가계생산, 노동시장·임금·투입·산출·생산능력·기업생멸 |
| C-E2 | [상품·시장·가격](C-E2_GOODS_MARKETS_PRICES.md) | 상품과 서비스·재고·주문·청산·부족·재고목표·차월 가격 |
| C-E3 | [가계·금융·투자](C-E3_HOUSEHOLDS_FINANCE_INVESTMENT.md) | 소득/현물소비·생활수준·예금/대출/은행·지분·배당·부도·투자 |
| C-E4 | [재정·공공부문](C-E4_FISCAL_PUBLIC_SECTOR.md) | 과세권·세목·지출·국채·부채·공공서비스·재정효과 |
| C-E5 | [운송·국제무역](C-E5_TRANSPORT_INTERNATIONAL_TRADE.md) | 운송망·용량·기간·비용·소유/보관·관세·환거래·통관 |
| 검증 | [경제 인수검증](ECONOMIC_ACCEPTANCE_TESTS.md) | 구현 시 필수 불변식·시나리오·장기 회귀 |
| 결정 | [경제 ADR 기록](ECONOMIC_ADRS.md) | 확정 ADR 28개, 상세 구조/역사 계수 보정 경계 |
| 예시 | [단일 시장 1개월 현금/부가가치](examples/one_market_month.json), [2개월 운송·관세](examples/two_month_shipment.json) | 구현용 테스트 fixture의 숫자·단계·예상 결과(실제 엔진 세이브 형식이 아님) |

## 모델 간 의존관계

```text
PopulationSlice (ONLY headcount)
    |                         Political/Jurisdiction (laws, tax authority)
    v                              |
EmploymentLedger ------------------+-------------------> FiscalLedger
    |            |                                    (assessment/entitlement)
    v            v                                        |
ProductionSite  HouseholdPool                              v
    |            |                                  FinancialLedger
    v            v                                        ^
ProductionLedger / InventoryLedger                          |
    |                    ^            <---------------------+
    v                    |
Market Order/TradeLedger ---> Settlement + Actual Delivery ----> Firm/Household/Government
    |                                  ^
    v                                  |
next-month quotes                 Transport/Customs
```

원천: 인구 `PopulationSlice.count`, 고용 `EmploymentLedger`, 재산/지분 `OwnershipLedger`, 현금/대출 `FinancialLedger`, 재고 `InventoryLedger`, 시장 주문·체결 `Order/TradeLedger`, 정책 채무/권리 `FiscalLedger`. 각 분야 파생 캐시는 `revision` 변경 시 폐기한다.

## 공통 경제 자료형/스케일

| 타입 | 규약 |
|---|---|
| `People` | `int64` 비음수 정수, 인원 배분은 POP-D2 원자적 사건 |
| `CurrencyId`, `Money` | `Money = {currency_id, int64_minor_units}`; 금액의 덧셈은 같은 통화끼리만, 128-bit checked 중간연산 |
| `GoodId`, `Quantity` | `Quantity = {good_id, int64_scaled_units}`; 상품별 고정소수점 스케일·최소 거래 로트 |
| `Price` | `{currency_id, good_id, money_minor_units_per_price_lot, price_lot_scaled_units}`; 단위 변환을 명시하고 0 이하 금지 |
| `Rate`, `Ratio` | 버전이 고정된 정수/고정소수점(예: ppm)으로 보존식 계산; 반올림 잔여는 정책별 추적 |
| `Stock`/`Flow` | 스톡은 월초/월말 잔액·재고·설비; 플로는 월간 발생액·유입/유출·변환 기록. 서로 동일시 금지 |
| `Claim`/`Payment` | 세금·임금·보조금·계약 채권·채무는 의무, 지급은 별도 확정 금융분개 |

## 통합 월간 12단계에서의 소유 단계 (POP-D3 준수)

| 단계 | 경제 계산 및 확정 범위 |
|---:|---|
| 0 | 입고예정 운송물·만기 채무·유효 법령과 전월 재고/가격/계정을 개시상태로 확정, 이후 불변 읽기 스냅샷 |
| 1 | 기업 생산/채용/원료·투자·정부지출·운송계획, 근거 있는 은행대출/국채/자금확보, 지불 예약 |
| 2 | 고용자격·노동 공급·임금제안·계약 청산, 지급 재원 잠금 |
| 3 | 생산중간재/운송서비스 구매·실물 이전·결제 및 해당 물량 잠금 해제 |
| 4 | 실제 노동·설비·투입재의 범위 내 생산, 현물생산·서비스 산출·손실 사건 |
| 5 | 실제 임금·원천징수·유효 정부이전·공무원/군인 급여 지급 |
| 6 | 가계 공동예산과 실수령 잔액·현물공급을 근거로 소비 계획 및 유효 주문 |
| 7 | 가계/정부/투자/군수 최종재 거래, 창고 인도·서비스 사용·운송출하, 세금·결제 |
| 8 | 실제 매출/비용·이자·세금채무·부도, 금융기관/법인/정부 결산 |
| 9 | 남은 유동성·자금조달에 근거한 저축·지분·차월 투자/배당 결정(당월 실현되지 않은 것은 차월 예약) |
| 10 | POP-D2 인구/건강/교육/복무/이주 사건 및 부수적 계좌/계약 이전 |
| 11 | 매출 가능 수요·실제 공급·월말 재고로 차월 가격 산출, 전체 정합성·원자적 월 커밋 |

**시간 경계:** 단계 n은 단계 0 개시상태와 n 이전에 확정된 결과만 읽는다. 같은 단계의 하위 순서는 문서에 고정한다. 이번 달 매출로 단계 1 운전자금을 소급 조달하지 않는다. 단계 9 배당 선언을 당월 현금지급으로 취급하지 않는다.

## 공통 보존식과 통계

- 인구: 세계 주민 인구 = `sum(active PopulationSlice.count)`, 기업/군/시장마다 별도 원본 인구 없음.
- 노동: 노동 적격인구는 중복 주된 직장을 가지지 않음; 자영업·생계생산 인력도 실제 인구 셀의 분할/점유로 계측.
- 재고: `I_end = I_open + output + arrivals - intermediate_inputs - final_consumption - dispatches - waste` (운송중 보유자 포함).
- 금융: 경제주체별 차변=대변, 통화별 계정/채권·채무 대응, 미확보 자금 이중사용 금지. 은행대출은 일관된 자산·부채 생성일 수 있으므로 총예금 고정 금지.
- 소유: 발행 지분/물권 = 유효 소유자 권리의 합; 설비와 상품의 재산권 변경은 실제 사건을 요구.
- GDP: 생산 부가가치(총산출-중간소비), 최종지출, 소득 방식의 가능한 범위에서 상호 검증. **중간재를 최종재에 다시 더하지 않음**. 기준시점·발생주의/현금주의를 구분하고 물량·가격·평가불확실성을 보존.
- 가격/실질임금/물가/실업률/생활수준은 관측 원장에서 유도. 가격 미관측은 0이 아니라 unknown.

## 역사적 확장 허용

- 농노/예속/강제노동, 소작·현물지대·십일조·자급경제, 사영/공영/협동/정부 직영, 노동법·영업허가·노동조합·독점/규제, 비은행 대부업, 은본위·금본위·불환화폐, 화폐권 분리 등은 **시대별 법/소유권/현물거래/금융제도 파라미터**로 표현할 수 있어야 한다.
- 현대적 기업·은행·임금·보통선거가 모든 시대에 존재한다고 가정하지 않는다.
- 역사/정책/업종별 계수는 버전 관리 자료로 분리. 예시용 테스트 수치를 실제 역사통계라고 표현하지 않는다.

## 참고 원칙

이 설계는 부문별 스톡·플로 일치(Stock-flow consistency), 복식부기, 생산/중간소비 기반 부가가치, 은행대출과 예금의 대응이라는 경제회계 원칙을 따른다. 참고: [UN System of National Accounts](https://unstats.un.org/unsd/Nationalaccount/sna.asp), [Bank of England: Money creation in the modern economy](https://www.bankofengland.co.uk/quarterly-bulletin/2014/q1/money-creation-in-the-modern-economy), [Statistics Norway: national accounts concepts](https://www.ssb.no/en/nasjonalregnskap-og-konjunkturer/metoder-og-dokumentasjon/begreper-i-nasjonalregnskapet). 이는 모델 설계 참고이며 외부 게임의 계산식을 복제한 것이 아니다.

## 구현과 설계의 구별

- **설계 완료:** 인터페이스/자료형/유효시점/대금과 재고의 정합성/가격·생산·임금 의사결정 알고리즘의 형태/검증 게이트.
- **미구현:** 전체 Financial·Employment·Production·Fiscal·TradeLedger의 단일 staging, 구체적인 시장청산·금융손실/파산·무역·경제 지표, 다중 통화 결제, 통합 스트레스 검증.
- 경제 세부 설계에서 보정 대기 중인 **수치 계수**는 규칙 파일의 별도 버전 값이다. 설계 미확정으로 착각하지 않으며, 구현/역사 보정 뒤 해당 수치를 명시한다.
