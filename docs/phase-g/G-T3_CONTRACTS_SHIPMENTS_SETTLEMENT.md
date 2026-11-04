# G-T3 — 거래계약·체결·선적·인도·대금정산

> 설계: phase-g-trade/1.0 · 2026-11-04 · SPECIFIED / NOT_IMPLEMENTED
> 상위: G-D0, G-T1, G-T2, C-D0(12 stages + atomic World), C-E2 Order/TradeLedger, C-E3 Financial, C-E5 Shipment/Customs, E-D0. 물리 운송은 향후 F, 현재 C-E5 어댑터.
> 핵심: **계약의 약속, 주문의 체결, 상품의 소유권/보관권, 물리적 이동, 통관, 현금 지급은 각각 별개**.

## T3-1. 객체·생명주기

| 객체·ID | 필드와 책임 |
|---|---|
| TradeContractId | contract_id, buyer/seller(+ broker/agent), good/quality, contracted_quantity, price/price_lot/currency, origin/destination, timetable, delivery_terms, title_transfer_rule, risk_transfer_rule, payment_terms, dispute_rule, source_offer/order refs, status, signed/effective/expiry, revision |
| TradeExecutionId | G-D0의 실행 추적 ID; contract_id 또는 linked order refs, cumulative milestones, delivered/settled quantities, outstanding/refund/penalty refs, source_events |
| TradeLotAllocationId | contract, Inventory lot_ref, allocated_quantity, shipped_qty, arrived_qty, accepted_qty, rejected_qty, returned_qty; 원본 재고 아님 |
| DeliveryInstructionId | source location/custodian, destination, delivery deadline, freight/insurance scope, required document refs, permitted split/partial, transaction refs |
| TradeDocumentId | invoice, bill of lading, origin certificate, permit, inspection, warehouse receipt 등 법적/증빙 문서와 실제 ledger refs. **문서 발행만으로 현금·상품을 생성하지 않음** |
| SettlementScheduleId | payment milestones, due dates, obligor/beneficiary/currency, linked financial obligation refs, collateral/escrow/credit facility refs; 금전 원본 아님 |
| TradeDisputeId | claimants, challenged quantity/quality/delay/payment, evidence, governing rule/law, mediation/award, penalty/remedy refs |
| TradeExecutionStatus | 계약/물류/통관/대금의 **독립 축 상태**를 보이는 파생/진행 뷰 |

- 기존 C-E2 TradeFill/Order, C-E5 Shipment/TransitInventory/FreightContract/CustomsAssessment/FXTrade, C-E3 FinancialLedger, OwnershipLedger/InventoryLedger가 각각 쓰기 정본이다. G 계약 객체는 **법적 약속과 이행 추적**의 정본이다.
- 계약에는 spot(현물), forward(장래 인도), recurring_supply(반복 납품), consignment(위탁), brokerage(중개), public_procurement(정부조달), barter(현물교환/복수 재고 인도) 등을 허용. 계약이 모두 완전한 민법상 현대법인 형태라고 가정하지 않으며 시대별 법적 효력을 C-P 규칙에서 확인.
- 장래계약 체결은 재고생산이나 현금 창출이 아니다. 이행시점에 실제 물자·대금/신용·합법성 검증을 재실행한다. 계약 금액을 즉시 매출로, 미생산 물량을 즉시 상품재고로 잡지 않는다.

## T3-2. 독립 상태축

1. **계약축**: proposed → agreed → active → fully_performed / terminated / breached / disputed; 일부/전부 이행 여부와 유효기간을 기록.
2. **물류축**: awaiting_dispatch → partly_dispatched / in_transit → arrived / partly_delivered / delivered / damaged / lost / detained / returned. 복수 shipment 각각 별도 상태.
3. **통관축**: not_required / pending / cleared / inspected / delayed / seized / denied. 통관 완료가 곧 구매대금 완납을 뜻하지 않음.
4. **결제축**: not_due / invoiced / payable / partly_paid / paid / overdue / defaulted / refunded. 선급·현금인도·후급/분납을 허용.
5. **소유권/위험 축**: 계약상 인도조건을 따른다. title holder와 risk bearer는 서로 다를 수 있으며 custodian(운송/창고 보관자)과도 다르다. title만 운송 중 매수인에게 넘어가도 재고의 물리적 위치는 도착지로 이동하지 않음.

상태축은 한 boolean '완료'로 합치지 않는다. 한 거래가 **상품 10 중 8 출하·6 도착·5 검수·대금 3/10 지급** 같은 미완료 조합을 합법적으로 가질 수 있다. 수량 관계는 실제 처리 단계와 반품/손상·교환 이력에 따라 누적 이벤트로 검증하며, 직선적인 모두동일 수량으로 강제하지 않는다.

## T3-3. 계약 조건과 체결

- 매수자·매도자/대리인의 법적 권한, 상품·품질/수량, 기준통화·가격 로트, 인도 위치·기한, 재고 소유권의 이전 조건, 사고 위험 부담, 운송비·통관비·보험 부담, 지급 시점, 검수/반품/지체배상, 위반 시 규칙을 계약 필수·선택 조항으로 정의한다.
- 실제 한 기업이 어느 상품/시점에 무슨 매입·판매를 할지는 G-T2의 자율 후보와 C-E2의 유효 주문·체결로 결정. G가 시장원장의 TradeFill을 별도 생성해 중복 거래로 계상하지 않는다.
- 새 계약의 합의와 이행 계획을 혼동하지 않는다. stage1에서 예약이 가능해도 새 상품의 존재/운송가능/송금가능 여부가 보장되지 않으면 체결·출하 확정은 허용되지 않음.
- **거래형태**: 직접 매매(매수자가 실물 title 취득), 위탁판매(소유자 유지, agent는 권한만), 중개(원매도인/원매수인 직접 계약), 다자계약(운송·보험·은행은 별도 계약/채권)으로 구별한다.
- 청약/승낙/수정은 offer_revision과 expected_revision으로 경쟁 변경을 거부한다. 계약 관계의 합의·비준/정부 조달 승인과 실제 금전 출납은 독립 이벤트.
- 동일 판매자가 타 시장에 동시에 판매 제안할 수 있지만 동일 상품 로트의 **실효 예약량 합계**는 반드시 가용 재고 이하여야 한다. 매입 신용·법적 쿼터·운송 용량도 동일하게 검증한다.

## T3-4. 원자적 예약 및 실행 계획

준수 순서:
1. 0단계에서 이미 유효한 계약·미지급 청구·도착예정 화물을 opening snapshot으로 확정하고, 유효 도착/검수 또는 억류를 처리한다(C-E5 Stage0).
2. 1단계에서 구매자 자금/신용·상품/품질·매도자의 가용 재고·상업시설/통관 쿼터·계약권한·창고·운송구간·외환유동성/해당 법률을 조회한다.
3. **전세계 공유 예약 관리자(C-D0/D-D0)**가 자금·재고·노동/설비 서비스·운송·정책/쿼터 예약을 동일 사건 의존관계로 잡는다. 임의의 G 독립 잠금은 원장과 경합할 수 없다.
4. 복수 출하/복수 구매가 같은 자원을 요청하면 상위 계약/기존 C-E2 청산 우선순위를 따른다. 충돌 시 부분체결(계약 허용) 또는 거부, 실패한 부분의 잠금 해제를 원자적으로 처리한다.
5. 실제 해당 stage(3 또는 7)에서 Order/TradeFill과 Inventory/Ownership/Financial의 관련 journal batch를 확정한다. 배송에 양의 운송기간이 있으면 매도처 → in_transit custodial holding만 변경하고 목적지 가용재고로 쓰지 않는다.
6. 자금은 지급 조건에 따라 prepayment / cash_on_delivery / credit / staged payment로 분개한다. 신용장은 실제 은행/채무·담보가 존재하는 경우에만 지급보증이며 빈 계약서만으로 즉시 돈을 발행하지 않는다.
7. 출하/인도/검수·통관 완료 후 stage8 회계에서 실제 매출·비용·채권·채무·부도/배상 상태를 대조하고 stage11 전체 원장 보존식을 검증.

예약 해제 사유: 만료, 계약 불성립/거부, 미이행 자원 취소, 재계획, 월말 확정 또는 오류 전역 Abort. **예약을 유지할 기간과 실제 채무 존속기간을 구별**한다. 실패 예약이 다음 월까지 유령 자금·선박 용량을 점유할 수 없음.

## T3-5. Shipment·운송기간과 복수 단계 거래

- **Shipment**은 F/C-E5의 정본이며 식별자는 선적건별 하나. 계약 하나에 여러 선적건이 매핑될 수 있고, 복수 계약은 실물로 구분된 화물에 대해서만 한 운송수단을 공유한다.
- **TransitInventory**는 실제 InventoryLedger 보유자/보관자 전이를 참조한다. 선적 10, 운송중 10, 도착 0의 경우 총 재고가 +10 늘어난 것처럼 집계하지 않는다.
- 경유국/환적: 출발–운송중–중간 창고–재출하–도착의 custody trace를 유지. 제3국을 거쳤다는 이유만으로 재수출 통계/원산지·영토 주권을 바꾸지 않는다.
- 수송기간 > 0으로 m월 출하한 화물은 같은 m의 목적지 단계3/7 생산/소비에 쓰이지 않는다. 'm+2 도착'이라면 해당 도착월 stage0 유효 도착/통관 조건이 성립할 때부터 쓸 수 있다.
- 수송기간=0은 C-E5에서 허용한 같은 지역 즉시 인도 또는 검증된 당월 내 배송의 결정론적 하위 사건에 한함. 시각적으로 붙어 있는 지역이라는 이유만으로 0개월 운송 허용 금지.
- 손실·변질·압류·반품·지연은 Cargo/Ownership/Financial/Insurance의 별도 사건과 계약의 위험부담 조항에 연결. 손실된 재고는 수취인 가용재고를 늘릴 수 없음.
- 국경 점령/봉쇄/긴급 조약의 새 사실이 발생해도, 그 월의 stage0에 동결된 통관/시장 계약을 사후 무차별 소급 변경하지 않는다. C-M/C-D0의 명시적 sub-tick 확장 전에는 차월 효과가 기본.

## T3-6. 국제계약과 부분 미이행

- 계약 가격과 실제 도착지 판매가격(시장 청산)은 다를 수 있다. 고정가격/시장연동가격·품질 검수/연체 시 조정을 계약으로 명시하고 C-E2 체결가 규약과 충돌하지 않는다.
- 단일 화물에서 선급금 수령·아직 미배송 상태, 완납·손상 물품 반품, 출하 완료·수령자 부도 등 모든 경우에 소유권/재고·채권/채무를 분리한다.
- 보상금 청구/보험금·계약위약금은 **금전채권**이 먼저 성립할 수 있으며 실제 지급은 FinancialLedger를 통해 별도 발생. 자동 환불/무료 보상 금지.
- 거래가 정부 간 조약 이행인 경우 H TreatyObligation의 obligation_id를 참조해 납품·지급을 실현하되 동일 인도를 H에서도 또 한 번 집행하지 않는다.
- 국적·관세영역·원산지·실제 통관·소유권 이전은 G-T4가 별도 판정. 국제배송이 소유권 이전을 동반하지 않는 위탁 보관/반품도 존재한다.
- **인도 검사/오더 체결/현금 지급의 모든 상태 변경은 원인 사건과 대응 원장 ref가 있어야 한다**. 가짜 결제완료 플래그만 수정 금지.

## T3-7. 월별 소유 단계

| Stage | 계약/실물/결제의 주된 단계 |
|---:|---|
| 0 | 예정화물 실제 도착·검역/통관 가능 여부 및 기존 의무/시장/법적 규칙 동결 |
| 1 | 청약·기존 계약 이행계획, 공통 자금/재고/통관/FX·구간용량 예약 |
| 2 | 운송/검수/상업 인력 실제 배정 |
| 3 | 생산 중간재 구매·즉시 인도 또는 출하; 실제 대금/채권 |
| 4 | 새 생산품의 생산 후 판매 가용성 반영 |
| 5 | 상업·운송 서비스 임금 지급 |
| 6 | 가계 실수령액 기반 소비 주문 |
| 7 | 최종상품 주문체결·인도/출하/통관·지급 또는 외상 기록 |
| 8 | 외상/송장/채무·이자/비용·부도/수수료·클레임 회계 마감 |
| 9 | 차월 신규 사업·투자/공공조달 정책의 확정/예약 |
| 10 | 군사/POP 사건이 배송·교역 가능성에 미치는 차월 효과 예약 |
| 11 | 계약의 이행 누적·소유/보관·통관/금전 대조 및 원자 커밋 |

**당월 stage8 이자 결산**이 stage1의 물품 매입 예산을 소급 증가시키지 않음.

## T3-8. 인터페이스·검증

- ProposeTradeContract(parties, goods, quantity, price, delivery_terms, payment_terms, scope, expected_revisions)
- AcceptContractOffer / AmendContract / CancelUnfulfilledContract(authorized_action)
- ReserveTradeExecution(contract_id, staged_world, cross_domain_reservations)
- AttachShipment / ObserveShipment/Customs / ConfirmDeliveryInspection / RequestPartialDelivery
- RegisterSettlementMilestone / ObserveReceivablePayable / DisputeTradePerformance
- ExplainTradeExecution(execution_id, snapshot) → 계약/배송/지급/관세/위험/원인별 실제 상태

모든 변경은 C-D0 Command→ProposedEvent→JournalBatch→StagingWorld→Commit. 내부적 부정상태(합의 안 된 상품의 소유권 변경 등)는 AbortMonth 후 HARD ERROR.

불변식:
- GT3-INV01 계약·체결·출하·도착·소유권·위험·통관·결제 독립 상태.
- GT3-INV02 계약 총수량과 누적 출하/인도/반품·취소·미이행 잔량 정합.
- GT3-INV03 운송 중 상품은 정확히 한 실물 보유자와 유효 소유자/위험부담 관계 유지.
- GT3-INV04 전세계 재고·금전/채권 보존; 지급/발생/선급/보증 혼합 금지.
- GT3-INV05 shipment 도착 전 목적지 생산·소비/재판매 가용재고 없음.
- GT3-INV06 환적·재수출·분할/부분결제·보험/반품의 중복 집행 금지.
- GT3-INV07 타 영역 원장/수송/관할/FX 충돌 시 동월/다음달 이행 실패를 추적.
- GT3-INV08 모든 계약/선적/결제 사건의 멱등·유효일·revision·재생가능성.

오류코드: GT3_ERR_INVALID_TERMS, NO_LEGAL_SIGNATORY, DUPLICATE_LOT_ALLOCATION, OWNERSHIP_MISMATCH, SETTLEMENT_OVERPAYMENT, INVALID_DELIVERY_WINDOW, STALE_SHIPMENT, DELIVERY_NOT_CONFIRMED, INSUFFICIENT_SETTLEMENT, ALREADY_EXECUTED.

검증: [GT3-T01~T10](TRADE_ACCEPTANCE_TESTS.md)와 G-D0. **설계만 작성했으며 C++ 구현·실측 인수는 수행하지 않았다.**
