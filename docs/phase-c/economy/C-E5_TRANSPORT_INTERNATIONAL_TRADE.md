# C-E5 — 운송·지역교역·국제무역 상세 설계

- 상태: **설계 확정 / 구현 전** · 경제 계약 `phase-c-economy/1.0`
- 상위: [C-D0](../C-D0_COMMON_CONTRACT.md), [경제 색인](README.md); 연계 [E2 상품·시장](C-E2_GOODS_MARKETS_PRICES.md), [E3 금융](C-E3_HOUSEHOLDS_FINANCE_INVESTMENT.md), [E4 재정](C-E4_FISCAL_PUBLIC_SECTOR.md).
- 지도 없는 S0에서도 가상 노드/간선의 운송망이 존재하며, GIS 연결은 후속 선택사항이다.

## E5-1. 지역·시장·관할·물리적 경로의 분리

- `Region`은 POP 거주지·생산시설의 주소를 제공한다. `MarketAssignment`는 해당 지역/상품의 거래 청산 시장, `TerritorialJurisdiction`은 주권·민정·과세권·군사통제를 가진다. 네 개의 개념을 동일 ID로 취급하지 않는다.
- 한 시장이 다국가에 걸칠 수 있고 한 국가에 여러 시장이 있을 수 있다. 물리적 운송경로가 연결된다고 자동으로 관세동맹/공동시장이라고 가정하지 않는다.
- `TransportNode`는 지역/항구/철도역/창고/시장 접근점 등을 나타내며 `TransportEdge`는 유효기간 있는 경로·교통수단·양방향 여부·통행료·용량·소요일·위험·차단상태를 가진다.
- 경로의 실제 노드/간선 존재와 상업/군사/외교적 통과 허가가 모두 충족되어야 이동 가능하다. 최단경로 하나만으로 모든 상품이 무제한 통행 가능하다고 처리하지 않는다.

## E5-2. 엔터티 정의

| 객체 | 최소 필드 |
|---|---|
| `TransportNode` | node_id, region_id, node_type, owner/operator, handling_capacity, valid_period |
| `TransportEdge` | edge_id, from_node, to_node, mode, capacity_by_good_class, travel_months, unit_cost, seasonal_rule, loss_risk, control/restrictions |
| `TransportCapacity` | edge_id, month, available_weight/volume/slots, consumed, reserved, service_producer_ref |
| `Shipment` | shipment_id, contract_id, origin, destination, good_id, quantity, route_edges[], dispatch_month, scheduled_arrival_month, status, title_holder, custodian, payment_terms, insurance |
| `TransitInventory` | shipment_id, holder_in_transit_id, goods_qty, condition, ownership_refs; 물량 정본은 InventoryLedger |
| `FreightContract` | service_provider_id, buyer_id, shipment_id, price/currency, liability, payment_ref, valid_period |
| `CustomsAssessment` | customs_id, shipment_id, authority_id, tariff_rule, taxable_base, duty, clearance_status, payment_ref |
| `TradeAgreement` | parties, goods_scope, quota/tariff/preference/embargo rules, entry/effective/expiry |
| `FXTrade` | fx_trade_id, counterparty/financial_institution, from_currency, to_currency, buy/sell_amount, rate, settlement_accounts, transaction_refs |

실물의 **보관자(custodian)**와 법적 **소유자(title owner)**를 분리한다. 수송업체는 물품을 보관·운송할 수 있어도 계약상 소유자가 아닐 수 있다. `InventoryLedger`가 보유량/상태의 원본, `OwnershipLedger`가 권리 귀속의 원본이다. Shipment의 수량은 운송관계에 대한 참조이며 새로운 별개의 물량을 생성하지 않는다.

## E5-3. 운송 수요/비용/용량

1. 단계 1의 기업·정부·군수·상인/가계의 실행 가능 운송 요청을 수집한다. 공급지역 재고와 수요지역 실효 주문 및 예상 가격차를 검토한다.
2. 배송 가능한 상품 로트·부피/중량·보관 요건과 유효 운송망, 통관/법적 허가/금수, 지정 마감시간을 확인한다.
3. 경로 선택의 기본 비교비용 = `actual_freight_cost + expected_loss_cost + duties + storage_cost + time_value`; 예상비용은 의사결정 입력일 뿐 자동 현금거래가 아니다.
4. 각 구간에서 다른 운송 요청과 경합하며 `reserved_edge_capacity <= available_edge_capacity`. 다단계 경로는 모든 필수 구간의 용량/서비스예약이 확인돼야 유효. 운송업자의 실제 연료·인력·설비(선박/철도 등) 사용으로 서비스 생산/원가를 계산한다.
5. 목적지 유효 판매가격과 실제 수령가격·운임·관세·환전·보관비/위험을 비교해 법인/상인이 주문을 채택한다. 예상 차익이 없더라도 정부 지원/군수 등 법적 목적이 있으면 이동 가능하다.
6. 동일 경로/월의 동률 수송은 계약/법적 우선권, 계약기한, 안정적 ShipmentId 순으로 배정한다. 화물은 단위별 정수 분할 가능량 범위에서만 나눈다.
7. 운송 수수료는 수송업자에게 실제 FinancialLedger 거래로 지급되며, 연료·노동·유지비는 생산/재고/금융 원장에서 발생한다. 지도상 두 지역을 잇는 간선이 공짜 운송을 의미하지 않는다.

## E5-4. 월별 출하·입고의 시간 경계

- **월초 단계 0:** 지난 월 커밋까지 등록된 `scheduled_arrival_month <= current_month`인 화물만 검증하여 실제 입고·통관 가능 상태로 처리하고 개시 스냅샷에 포함한다. 분실·차단/정체·통관 미완료라면 미도착/보류로 남긴다.
- **단계 1:** 신규 `ShipmentPlan`, 운송 용량, 지불 재원/계약, 통관 예상액을 예약한다.
- **단계 3/7:** 실제 출하 발생 시 판매자 보유 재고에서 `in_transit` 보유자로 물량을 이동시킨다. 계약상 선급/배송 시 결제/수령 후 결제를 나누며 미결제 외상은 FinancialLedger 채권·채무로 기록한다.
- **단계 8:** 운송업체의 서비스 수익/비용·보험료·통관 미수금/지연·손실을 결산.
- **단계 11:** 화물의 차월 도착 예정 상태와 보유량·소유권·통관 사건을 검증한다.
- 양의 `travel_months`를 가진 신규 출하는 같은 달의 목적지 단계 3/7 생산 투입/소비에 쓸 수 없다. 목적지의 예약된 도착분은 **실제 입고가 확정된 월초 이후** 사용 가능하다.
- `travel_months=0`은 동일 지역의 현지 즉시인도 또는 검증된 당월 내 배송 서비스에서만 허용하고 출하/인도 순서를 같은 stage 내부 결정론적 하위 사건으로 표시한다. 지도에서 연결되어 보인다는 이유로 모든 원거리 운송을 0개월로 만들지 않는다.
- 월중 충돌/전쟁·기상이변에 따른 화물 손실은 InventoryLedger의 명시적 손실·Ownership/Financial의 보험/배상사건과 연결한다. 손실물량은 수취인의 재고에 추가되지 않는다.

## E5-5. 국내/국경 간 거래·통관

- 국제무역 판정은 `sovereignty`만이 아니라 유효 **customs territory**, 관세/무역협정/시장접근 권한을 통해 수행한다. 동일 국가라도 국내 관세선이 존재할 수 있고 독립국 간 관세동맹은 면세/공동시장일 수 있다.
- 수입·수출/재수출·통과무역을 `TradeFlow` 거래와 실제 `Shipment`으로 구분한다. GDP·국제수지의 재화 소유권 이전 시점(계약 terms)을 명확히 하며 물리적 통과량과 소유권 변경량이 같다고 가정하지 않는다.
- 화물은 국경 통과/실제 통관 시 해당 `CustomsAssessment`를 받는다. 관세율은 품목·원산지·수입자·관할/통관시점의 유효 법률·협정·할당량에 따라 달라진다.
- 관세는 실제 수입원가/소비자가격에 반영되며 E4 재정채권/금융지급으로 연결한다. 수입대상 물품을 자국에서 새로 생산한 것으로 집계하지 않는다.
- 금수조치/허가/수입할당/전쟁봉쇄/해군 수송위협은 법률·국가/군사 통제·운송 용량을 통해 실제 수송 가능량에 제약을 준다. 이미 발송된 화물을 외생적으로 지우지 않고 억류/지연/압류/손실 사건으로 처리한다.
- 외상수입·무역금융·보험·통관 보증은 해당 책임 주체의 채권/채무와 실제 예약자금이 있어야 사용할 수 있다.
- 수송·통관 관련 군사 행정/점령은 관할/징발/위험 경로를 변경할 수 있지만 생산·가격·인구 원본을 임의 복제하지 않는다.

## E5-6. 통화·환율·국제결제

- 모든 통화량은 `{currency_id, minor_units}`로 식별. 다른 통화의 금액을 교환 거래 없이 단순 덧셈하지 않는다.
- `ExchangeRateQuote`는 통화쌍·기준월·고정소수점 비율/최소 거래단위·유효 법률을 포함한다. `FXTrade`는 실제 은행·상인·중앙은행이 보유한 두 통화/외환채권/예금을 교환하는 **복수의 대응 금융 분개**로 처리한다. 환율 변경만으로 이미 보유한 외화가 새로 발행되지 않는다.
- 환율 형성은 통화 발행제도(금·은 본위, 고정/관리/변동환율, 외환통제), 외환수요/공급·국제수지·금융시장 접근을 근거로 한 추후 `CurrencyRule`을 사용한다. **가격표만 있어도 거래 상대방/유동성 없으면 무제한 환전 불가**.
- 국제 지급결제는 상대 금융기관 계정·준비금/예치금·무역신용을 통해 청산한다. 외화 부족·결제제재·전쟁·은행파산은 실제 제약이 된다.
- **초기 통합 인수시험은 단일 통화**로 실행 가능하되 `currency_id`와 환거래 인터페이스는 처음부터 필수. 다중 통화 전체 기능을 구현하지 않은 경우 타 통화 거래는 명시적 Unsupported/NoFXLiquidity로 거부하며, 가짜 1:1 환전으로 처리하지 않는다.
- 실질 환율·무역수지·국제수지·대외채무는 해당 통화·평가시점·가격 기준을 명시한 관측 지표로 계산한다. 명목금액들을 환율 없이 혼합해 세계 총계를 만들지 않는다.

## E5-7. 수량 보존·거래가격·통계

- `initial_stock + production + domestic/import_arrival - uses - consumer_delivery - export_dispatch - losses = end_stock`을 보유자/품목/지역별로 대조. 세계합에서는 지역 간 이동이 순상쇄되며 출하·운송중·도착으로 물량이 중복되지 않는다.
- 국제무역의 지급 총액 = 판매자 수령대금 + 실제 운송·보험·관세·기타 수수료와 해당 금융채권/부채의 정합적 합. 배송약관에 따라 누가 비용을 부담하는지 구분한다.
- 무역수지·경상수지·금융계정·외환자산·실물 수송은 동일한 개념이 아니다. 원장에 대응하는 범위와 평가가격/통화 환산 기준을 명시한 뒤 계산한다.
- 지표: 지역별 상품 접근성/운송 비용/재고 부족, 운송 지연/파손, 평균 배송기간, 운송시설 가동률, 수출입 물량·가격·원산지·통관 세수, 화폐별 FX 유동성·외채/무역신용.

## E5 검증 게이트

- E5-G1: A→B에 2개월 운송되는 철 10톤은 출하월 A 재고 -10, 운송중 +10, B 당월 소비/투입 0; 실제 도착월 B +10(손실 없음).
- E5-G2: 경로 용량 8인데 10 출하 요구 시 실제 예약/출하는 8 이하.
- E5-G3: 동일 화물에 `seller inventory`, `in_transit`, `buyer inventory`가 동시에 10씩 존재하지 않는다.
- E5-G4: 임의 관세 10은 유효 통관 사건과 수입자/통관 정부 금융 채권/지급으로 일치; 단순 국가 경계 도형 변화만으로 일괄 징수 금지.
- E5-G5: 통관 이전 화물 봉쇄/억류는 도착예정인 만큼 생산 투입되는 것을 차단한다.
- E5-G6: 서로 다른 통화 100과 200을 환율 없이 같은 단위 현금 300으로 합산할 수 없다.
- E5-G7: 단일 통화 시험에서는 FX 미구현이어도 정상 교역; 다중 통화 거래는 실제 FX 상대/유동성 없으면 명시적으로 실패.
- E5-G8: 국경·행정·군사통제·MarketAssignment 변경은 인구/상품/돈의 소유·수량을 생성하거나 삭제하지 않는다.
- E5-G9: 운송 손실/보험 보상 발생 시 물량 감소와 권리·현금 청구권이 일관되게 맞는다.
