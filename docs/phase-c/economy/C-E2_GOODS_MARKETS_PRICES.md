# C-E2 — 상품·시장·가격 상세 설계

- 상태: **설계 확정 / 구현 전** · 경제 계약 `phase-c-economy/1.0`
- 상위: [C-D0](../C-D0_COMMON_CONTRACT.md), [경제 색인](README.md) · 연계: [E1 생산](C-E1_FIRMS_LABOR_PRODUCTION.md), [E3 소비·금융](C-E3_HOUSEHOLDS_FINANCE_INVESTMENT.md), [E5 운송](C-E5_TRANSPORT_INTERNATIONAL_TRADE.md).

## E2-1. 상품/시장/재고의 분리

| 객체 | 최소 필드·책임 |
|---|---|
| `Good` | good_id, name, category, unit, quantity_scale, price_lot, storage_kind, perishability, substitution_tags, quality_tags |
| `GoodBatch`(선택적) | lot_id, good_id, quantity, quality, production_month, expiry_month, provenance, current_holder; 필요한 경우에만 품질/유통기한 추적 |
| `Market` | market_id, region/상품 접근관계, currency_id, market_rules_id; **상품 소유자가 아님** |
| `MarketQuote` | market_id, good_id, quote_month, price_ticks, ceiling/floor/limit_policy, currency_id, revision |
| `InventoryPosition` | holder_id, location_id, good_id, available, reserved, damaged, in_transit_ref, revision; 총수량 원본은 InventoryLedger |
| `BuyOrder` | order_id, buyer, market, good, max_qty, max_unit_price, funding_reservation, priority_rule_id, stage, expiry |
| `SellOrder` | order_id, seller, holder_id, market, good, max_qty, min_unit_price, inventory_reservation, stage, expiry |
| `TradeFill` | fill_id, buy/sell_order_ids, good, actual_qty, unit_price, buyer/seller, tax/transport_refs, payment_ref, delivery_ref |
| `NeedCategory` | need_id, age/household/health policy, acceptable_goods[], substitution_priority, minimum/aspirational_quantities |
| `MarketObservation` | period, solvent_buy_qty, offer_sell_qty, filled_qty, unmet_solvent_qty, unsold_qty, end_stock, losses, provenance |

`Stock`(보유 물량), `Order`(의향), `Fill`(실제 체결), `Delivery`(인도 완료), `Settlement`(대금 지급 완료)은 다르다. 매수 주문만으로 재고 이동이나 임금·소득 발생을 기록하지 않는다. 시장은 **주문을 중개·청산하고 차월 가격을 계산**하며 상품을 생성하거나 소유하지 않는다.

**Good 분류:**
1. 소비성 물품(식량, 피복, 연료 등): 실제 사용 시 소모, 저장·부패 가능.
2. 내구소비재: 구매 후 가계 소유 자산/재고로 이전, 사용·마모에 따라 서비스 흐름 생성. 구매 순간에 전체 물량을 물리적으로 소멸시키지 않는다.
3. 생산 중간재: 생산 단계 3에서 인도, 단계 4에서 레시피 소모.
4. 자본재: 취득 뒤 설치/건설 이벤트로 CapitalAsset 전환(Ownership/Inventory 동기화).
5. 비저장 서비스(운송·교육·의료 등): 해당 월 실제 생산/제공 가능 슬롯과 사용량을 기록, 월말 미사용분은 자동 이월하지 않고 명시적으로 만료.
6. 희소 토지·광물매장량 등의 권리는 일반 소비재 무한 재고가 아닌 Asset/ResourceStock 원장으로 다룬다.

## E2-2. 상품별 욕구와 실효수요

- POP 가구의 욕구는 `NeedCategory` (칼로리/식량, 의복, 주거, 난방, 위생, 의료, 교육, 사치 등)와 지역·연령·가구구성·시대별 법/기술로 산정한다.
- `required_need[h,k]` = 가구의 실제 인구 구성(성인/아동/노인/부상자 등)에 따른 **단위당 필요량** 합산. 건강 요인은 의학적 필요·비용으로 모델링하고 단일 장애 플래그가 소비자 능력을 임의로 낮추지 않는다.
- 먼저 `in_kind_available` (이미 가구가 생산·소유하는 소비 가능 물품)를 배정하고, 부족한 부분만 시장 구매 필요량으로 제출한다. 동일 음식이 자급소비와 시장구매에 모두 포함될 수 없다.
- 구매예산은 가구/정부/기업의 실제 이용 가능한 자금 및 허용된 실행 신용으로 제한한다. `desired`(원하는 수량)과 `solvent_demand`(현재 가격으로 실제 지불 가능한 주문량)을 분리한다.
- 기본 소비 우선순위는 기본 생존 필요 → 의무지출/필수 서비스 → 기타 필요 → 재량 소비. 생활수준/문화·가격·대체재 선호는 규칙과 분포에 따라 달라질 수 있다.
- 대체 가능 상품은 `SubstitutionSet`으로 정의. 기본 청산에서는 **최대 두 번의 미체결 수요 대체**만 허용하고 각 반복의 확정 주문/자금예약을 상호 일관되게 수정한다. 무한 재최적화 금지.
- 구매 불능/공급 부족분은 충족되지 않은 욕구로 측정하고 건강·불만·정치적 선호 등 후속 POP-D2 사건/행동에 사용할 수 있지만, 자동 소비 성공이나 현금 손실로 계산하지 않는다.

## E2-3. 월중 주문·거래 청산

**단계 0:** 시장별 상품 가격표가 고정된다. 공정 가격을 강제하거나 모든 상품이 존재한다고 가정하지 않는다. 새 시장/상품은 사전 도입가격·출처와 규칙이 있어야 거래할 수 있다.

**단계 1:** 기업·정부·군은 생산용 원료, 재고, 운송, 투자재, 공공서비스에 대한 계획과 지불·재고 예약을 마련한다.

**단계 3 중간재 시장 청산:**
1. 동일 상품·시장·가격에서 유효하고 실제 지불 가능한 BuyOrder, 판매 가능 재고/적격 판매자의 SellOrder만 선별.
2. 기존에 예약된 물량·자금 차감 후 동일 물건/계좌를 이중 배정하지 않는다.
3. 법률에 따른 구매 우선권/장기계약이 있으면 명시된 룰로 처리. 그 외 기본 청산은 주문 제출 시각(월내 규약 순서), 단가 허용범위, 안정적 order_id를 기반으로 결정론적 매칭. 가격에 의한 경쟁은 아래 차월 갱신과 차별된 것.
4. 공급 부족 시 실제 물량 한도로 부분체결. 동일 우선순위 그룹의 불가분 최소로트 초과 요청을 정수 단위 비례 배분할 경우 **largest remainder / ID 순 동률**. 임의 추첨은 버전 있는 난수 스트림만 사용.
5. TradeFill과 양쪽 Inventory/Financial 원장의 배송·대금/채권 지급 효과가 동일 트랜잭션으로 검증될 때에만 체결 확정. 인도지연 거래는 E5에 따른 escrow/선급/외상/채권 규칙이 적용된다.
6. 미체결 주문의 지급/재고 예약은 계약의 유효기간에 따라 명시적으로 해제 또는 유지. **실물 인도 완료 전 생산 투입 불가**.

**단계 4:** E1의 실제 생산으로 새 상품/서비스 산출. 이 산출은 단계 3의 거래에 소급 판매할 수 없다.

**단계 6:** POP 가구의 당월 실제 수령액과 기존 현금/예금, 인근 현물 및 가격에 따라 소비/대체 후보를 생성한다.

**단계 7 최종재 청산:** 단계 3과 같은 reservation/Fill 검증을 적용하되 소비·정부 구매·군수·투자용 최종재를 구분하고 새로운 단계 4 산출을 판매할 수 있다. 우선구매권·배급제·보조금은 유효 법률/제도/실물 공급에 한정한다.

**단계 8:** 남은 매도 재고와 현물 손실, 만료·폐기, 미지급 채권을 결산한다. 이미 인도된 상품을 미청산 주문으로 다시 판매하지 않는다.

## E2-4. 수량/대금/가격의 단위

- 가격은 `Money/canonical_quantity`이며 시장에 표시되는 숫자 외에 `price_lot`, 통화, 반올림 정책이 필수.
- `trade_total = round_money(price_ticks * filled_quantity / price_lot)`. 128-bit checked 중간 계산, 동일 계산방식을 모든 구매·판매·세금·부가비용 분개에서 사용. 라운딩 잔액은 별도 rounding/fee account에 등록하고 사라지는 금액을 만들지 않는다.
- 단위당 매도자 수령액, 운임, 보험료, 관세, 내국세, 구매자가 지불하는 총액을 구분한다. `consumer_total`을 공급자 매출로 모두 계산하지 않는다.
- 시장의 사전공시 `MarketQuote`와 체결 가격은 다르거나 같을 수 있으나 초기 엔진은 **고정 월중 quote를 기준 체결가로 사용**하고 세금·운송비를 별도 항목으로 부가한다. 입찰/협상 특례는 버전 있는 `MarketRule`로 확장한다.
- 무상 공공제공은 실제 물품/서비스 제공과 정부 비용 지출로 기록하되 시장 매도가 0으로 전체 가격을 붕괴시키지 않는다.

## E2-5. 차월 가격 동학 (단계 11)

초기 기준 모형은 **당월 가격 고정 → 월말 다음 달 호가 수정**이다. 공급부족이 있을 때 같은 달 수차례 가격을 무제한 반복 계산하지 않는다.

관측값:
- `D_t`: 월중 해당 시장/품목의 **실제 지불 가능한 유효 주문량**, 단계 3/7 중 같은 주문을 중복 합산하지 않음.
- `S_t`: 그 기간에 실제 판매하려고 제공한 유효 재고량+제공 가능한 서비스 슬롯의 일회 집계, 같은 재고 두 번 매도 제안 금지.
- `I_t`: 월말 실제 **판매 가능한** 재고량. 운송 중, 손상/예약된 재고는 별도.
- `T_t`: 상품별 업종·계절성을 반영한 목표재고량.
- `alpha, beta >=0, 0<delta<1`: 버전 관리된 상품/시장별 반응계수와 월 최대 변경폭.

기본형 산식(모든 연산은 정수/고정소수점 구현):
```text
shortage_gap = (D_t - S_t) / max(D_t, S_t, epsilon_q)
stock_gap    = (T_t - I_t) / max(T_t, I_t, epsilon_q)
change       = clamp(alpha*shortage_gap + beta*stock_gap, -delta, +delta)
P_{t+1}      = clamp_price(round_to_tick(P_t * (1 + change)), min_price, max_price)
```
- 기존 `docs/MONTHLY_RULES.md`의 `exp(...)`은 S0 레거시 참고 산식이다. 신규 기준 산식은 여기의 **유한 차분 고정소수점 방식**이며 계수는 역사 시나리오 보정 전까지 규칙 파일 값으로 분리한다. 부동소수 `exp`에 의존한 다중 플랫폼 비결정성을 만들지 않는다.
- **휴면시장:** `D=S=0`이면 부족분에 따른 가격 갱신 중단. 재고가 있거나 계절 저장비가 발생하면 버전 있는 재고정책만 적용. `unknown price`는 0가격과 다르며 신규 시장 발행/관측 입력 규칙 필요.
- **재고 0·수요 >0:** 위 월 상한 내 상승. 상한 때문에 당월 거래에서 공급되지 않은 물품이 자동 생성되지 않는다.
- **공급 과잉:** 가격 하락·초과재고·보관손실과 다음 달 기업 감산이 가능하다. 가격 0 이하·음수 재고 불가.
- 가격 규제·통제·배급·암시장 등은 `PricePolicy`/유효 법률과 실제 거래 채널로 따로 표현. 정가 자체가 시장 부족을 해소하지 않는다.
- 가격 반올림 잔여/최저 tick/최소 로트와 계절성은 계약 버전을 세이브에 포함한다.

## E2-6. 정합성·지표

시장별 유효 수요, 총 공급 제안량, 실제 체결량, 매도 미체결량, 소비 충족률, 품목별 부족률, 목표재고 대비 실제 재고, 가격/물가 지수 및 변동률, 실질구매력, 시장 거래 결제불능·실패 이벤트.

불변식:
- 거래량 ≤ 구매자가 확보한 예산으로 가능한 수량, 판매자 실제 미예약 재고, 법적 할당 상한.
- 동일 상품배치/보유자 재고는 복수 시장/단계/운송중에 이중 매각되지 않는다.
- 모든 실제 TradeFill에는 대응하는 보관권·소유권/실물 인도와 금융 채권 또는 실제 결제가 있다.
- `planned`, `solvent`, `filled`, `delivered`, `consumed` 수량의 관계를 알 수 있고 모두 같은 값이라고 강제하지 않는다.

## E2 검증 게이트

- E2-G1: 자금 10, 상품 가격 4인 시장에서는 3단위 구매 실패/축소, 2단위 초과 실매수 불가(부가비용이 없다고 가정).
- E2-G2: 재고 7을 단계 3에 5 판매했으면 단계 7에 남은 원래 재고 2 외 더 판매 금지(단계 4 신규 생산은 별도 가능).
- E2-G3: 가격/수요 충격 후 차월 가격만 갱신, 전월 판매 영수증 불변.
- E2-G4: 공적 배급/가격상한에서도 미충족 수요·암시장 여부를 기록하고 재고가 음수가 되지 않는다.
- E2-G5: 상품 0수요·0공급 월에 NaN/음수 가격 발생 금지; 새 품목 unknown과 0 구분.
- E2-G6: 가계의 현물 식량 소비량은 해당 월 시장구매량에 중복 포함되지 않는다.
- E2-G7: 서비스 공급 한도 초과 판매 또는 전월 미사용 서비스 무제한 이월 금지.
- E2-G8: 두 번 소비 대체 후에도 재고/자금과 재현성이 일치한다.
