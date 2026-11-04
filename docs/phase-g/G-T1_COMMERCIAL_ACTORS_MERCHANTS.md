# G-T1 — 상업주체·무역회사·무역센터·국내 유통

> 설계 버전: phase-g-trade/1.0 · 2026-11-04 · SPECIFIED / C++ NOT_IMPLEMENTED
> 선행 계약: [G-D0](G-D0_COMMON_CONTRACT.md), C-D0, C-E1/E2/E3/E5, D-D0, E-D0.
> 목적: 누가 거래하는가, 누가 상품·위험·대금을 소유하는가, 무엇이 상업 활동의 물리적·금융적 한계인가.

## T1-1. 기본 행위자

| 행위자 | 실제 주체·제약 | 허용 행동 |
|---|---|---|
| 생산기업 직접영업 | C-E1 Firm·ProductionSite, 판매할 Inventory 및 운전자금 | 자체 생산물 판매, 원료·기계 매입, 직접 수출입 |
| 독립 상인/무역회사 | 기존 Firm + G MerchantOperation + 노동·재고·자금/신용 | 상품 매입·보관·재판매·수출입·유통 위험 부담 |
| 위탁판매 대리상 | Firm/적법한 비법인 상인 + 위탁계약 | 타인 소유 상품에 대한 수수료 판매, 고객 위탁 매입 |
| 중개인(Broker) | 실제 인력·법적 허가·연락/정보 서비스 | 매수/매도 상대 연결, 체결된 중개수수료 수령, 원칙적으로 재화 소유권 없음 |
| 운송·보관업체 | C-E1 Firm, F 또는 C-E5 운송, 실제 노동·창고/선박 | 운송·보관 서비스 제공. 별도 계약 없이는 상품 소유권 취득 없음 |
| 가계/자영 상인 | POP의 기존 OwnAccountActivity/HouseholdPool·적격 권리 | 소규모 수공업·장터 직접 판매·상업, 별도 유령 기업/인구 금지 |
| 정부·공공기업 | C-P의 적법 정부기관 + C-E4 예산 + 기존 Firm(필요시) | 정부조달·구매·공급계약, 국영 상업기업 운영, 법률에 따른 배급/독점 |

'무역회사'는 새로운 회사 정본이 아니라 Firm의 산업활동/역할이며, 정부·국영기업도 C의 동일 회계와 재고·고용 제약을 따른다. 직접영업과 상인을 거치는 방식은 공존한다. 지역 내 일반 소비자 구매는 C-E2의 기존 Market Order/TradeFill로 처리하고 모든 거래를 G로 경유시키지 않는다.

## T1-2. 객체·관계 및 상태

- **CommercialMandateId**: G-D0의 적법 상행위 권한. issuer/holder, goods_scope, location_scope, agency_mode, maximum_exposure, valid_from/until, origin_law_or_contract, revision.
- **MerchantOperationId**: FirmId, SiteId(선택), business_modes(direct/principal/agent/broker/warehouse), supported_goods, origin/destination market refs, staff/employment refs, financed_limit_ref, storage_asset_refs, operation_status, revisions. 직원 수 자체는 저장하지 않는다.
- **CommercialFacilityBindingId**: 기존 ProductionSiteId/CapitalAssetId/ProvinceId와 실제 상업·보관·검수서비스의 연결. 물리시설은 D-C의 ConstructionProject로 조성되며 원본 재산권은 OwnershipLedger.
- **ConsignmentMandateId**: principal_owner_id, agent_id, goods_lot_ref, minimum_sale_conditions, commission_formula, expiry, authority_revocation, custody; 판매 권한이지 소유권 양도 아님.
- **BrokerageMandateId**: client, broker, eligible_market_scope, fee_terms, disclosure_constraints, expiration. 계약을 체결해 실제 서비스가 성립하기 전 수수료 현금 생성 금지.
- **TradingPolicyId(회사 내부)**: 기업/운영주체의 상품별·지역별 목표재고, 허용신용, 최저 예상마진, 위험한도, 지연 허용치, 정보·시장 선호도, 변경시점. 정부 무역법 TradePolicyProjection과 이름·책임을 구별.
- **CommercialCapacityView**: (staff_service, warehouse_free, processing_assets, accessible working_capital, permits, logistics_quotes)에서 파생한 처리 가능량; 독립 거래 '포인트' 원본이 아니다.
- **GoodsLotRef**: InventoryLedger의 lot/batch를 참조하며 각 로트에 소유자(title)·현재 보관자(custodian)·장소와 품질·유효범위를 연결. Shipment/TradeExecution의 수량 참조를 제3의 실물 원본으로 더하지 않는다.

모든 신규 관계는 C-D0 typed EntityId, 사건·revision·valid_month 및 tombstone 규칙을 따른다. 운영실적/재고 합계/인원수/회계잔액은 read-only 파생 통계다.

## T1-3. 상업 활동의 경제적 실체

1. **직접 매매(principal)**: 상인이 실제 자금/신용으로 상품을 매입할 때 Inventory/Ownership 원본의 소유권과 FinancialLedger의 지급/매입채무를 명시한다. 이후 재판매는 **새 거래**이되 원 상품을 두 번 생산한 것으로 처리하지 않는다. 보관·운송 중 소유자가 상인이라면 재고자산/위험 노출도 상인에게 귀속된다.
2. **위탁판매(agent)**: 원판매자가 title을 유지한다. 상인은 주문/고객 관계만 대리하고, 판매 시 수수료채권/지급을 발생시킨다. 원판매자의 매출과 대리인의 총거래금액을 각각 생산 GDP에 합산하지 않는다. 위탁취소로 기존 지급/물자변동 자동 취소 불가.
3. **단순 중개(broker)**: 거래 당사자는 원매수/원매도이고 중개인은 실제 제공한 상업서비스 대가만 매출로 기록한다. brokerage fee와 판매자 상품매출의 소유/현금 흐름을 구별한다.
4. **상업/보관 서비스**: 점포·무역센터·창고는 실제 기업/사업장 및 취업 POP·시설·금융/상품 비용을 사용한다. 상업서비스의 부가가치(매출에서 중간투입 차감)는 원상품 재판매액과 구분한다. 미판매 상품은 남은 실제 재고이며, 미실현 가격상승이 현금이 되지 않는다.
5. **공공구매/국영 상업**: 국가가 식량 수입·군수 조달을 계약할 수 있으나 유효 재정·관할·정책과 실제 지급/배송 제약을 따른다. '전략상 필요'는 민간 이익 조건을 면제할 수 있어도 실물·신용·용량 상한을 면제하지 않는다.
6. **시기별 상업**: 1801년부터의 다양한 역사적 제도(길드·허가상인·행상·상회·상업은행·식민지 독점회사·국영무역·현물거래)는 C-P 법률과 특허/면허·소유/채무·거래 방식으로 표현한다. 모든 시기에 근대 법인·현대 카드결제·자유무역을 강제하지 않는다.

### 간단한 실물·회계 사례

A기업이 실제 생산한 철 10단위를 상인 M에게 총 1,000에 판매하고, M이 B기업에 총 1,400에 재판매했다고 가정한다(동일 통화, 운송/보험/관세 총 150, 다른 비용 없음).

- A→M 거래: A 보유 재고 −10, M 보유 재고 +10, M의 현금/채무 1,000이 A 수취/채권 1,000과 대응.
- 물리적 운송 비용 150: 실제 서비스 제공자/정부 계정으로 지급하고 M 비용에 귀속.
- M→B 거래: M 보유 재고 −10, B 보유 재고 +10, 매출 1,400의 지급/채권 대응.
- M의 단순 거래마진 **250** = 1,400 − 1,000 − 150(자금조달·보관·인건비 등 별도). **철 생산 20단위로 중복 집계하지 않는다.** 현금수취 이전 매출채권·비용발생/손익인식은 C-E3/E2 회계기준을 준수한다.

## T1-4. 시설, 고용, 용량

- 무역센터/상업시설은 건설로 설치한 ProductionSite/CapitalAsset가 없으면 해당 설비능력이 발생하지 않는다. 그러나 직판·소상인의 거래에는 고정 무역센터 **필수 조건이 아니다**. 실제 허가·장소·연락·운송 접근에 따라서 가능한 상업 방식이 달라질 뿐이다.
- 창고 보관용량 = 실제 자산의 유효 공간/품질/온도 설비에서 현재 보관중 수량을 제한한 값. 상품별 중량/부피/부패/위험물 제약을 가진다.
- 상업 처리 가능량 = 상품/거래 서류, 가동 시설, 취업 인력·숙련, 자금/보험/통관 접근으로부터 평가. 범용 '전국 무역센터 레벨 = 무한 거래' 방식 금지.
- 노동 배정은 POP/EmploymentLedger; 영업직·항만·철도/선원·회계 직원이 다른 생산·건설 업무에 동일 인원/서비스시간을 중복 제공하지 못하게 한다.
- 경쟁·규모의 경제·독점/담합/시장 지배는 실제 비용·가격·허가/소유 구조와 법률에서 발생. 법률이 허용하는 허가/독점은 무조건 무료로 상거래를 성사시키는 능력이 아니다.
- 기업의 현금 부족·인력 부족·창고 부족·미허가 시장은 물량 축소·지연/거래 거부 사유. 기업 파산·폐업은 C-E3 법적·금융 절차와 재고자산 매각, 미이행 위탁/계약 이관을 참조한다.

## T1-5. 기업 의사결정과 민간 자율성

상업기업은 (예상 실현매출, 직접매입비, 운송·관세·보험·보관·금융비, 위험, 재고회전, 시장 정보, 기존 계약 의무, 유동성)으로 후보를 비교한다. 실제 주문과 경쟁 청산은 C-E2에 제출하며, 우선권은 유효 계약/법률에 따른다.

결정 규칙(기본형):
1. 예상 현금흐름과 담보·허용위험에서 **지속가능한 노출한도**를 계산. 자금 부족이면 거래량 축소 또는 계획 보류.
2. 이미 확보한 계약 이행·노동·시설 용량과 상품 품질·보관기한을 충족하는 후보만 평가.
3. 자사 순기대이익과 위험한도에 따라 유효 주문 제출(현재 시장의 확정 가격은 C-E2 규약에 따름).
4. 시장 체결·운송 예약에서 실제 가능량만 계약에 반영; 나머지 주문은 유효기간/취소규칙에 따라 잔류/소멸.
5. 상품 도착/대금수령/손실·파산의 **실현 결과**가 후속 월의 기업 의사결정에 반영된다. 계획 이익을 실제 이익처럼 확정하지 않는다.

상인·기업 AI의 정보접근은 실제 시장 접근과 시기/제도의 통신·정보통계에 따른다. 시장 가격을 알 수 없는 경우 unknown; 세계의 완벽한 가격 정보가 무료로 모두 보인다고 가정하지 않는다.

## T1-6. 월별 단계·인터페이스

- Stage 0: 유효 Firm·허가·시설·위탁/중개·기존 주문/재고/채무를 opening snapshot으로 고정.
- Stage 1: 상업 영업 계획, 운전자금·유효 계약·시설/운송/재고 예약 요청. C/D 공유 예약 사용.
- Stage 2: C-E1 고용·실제 상업/보관 노동 서비스 확정.
- Stage 3/7: 중간재/최종재의 실효 주문·매매, 위탁/중개 수수료의 발생 및 실제 지급 조건 처리. 상품은 실제 Inventory/Ownership 이전.
- Stage 5: 상업 종사자 임금 실제 지급.
- Stage 8: 미수금·채권·영업손익 및 상품재고 평가/계약 위반·도산 관련 발생주의 정리.
- Stage 9: 다음달 상업 투자·무역센터 건설/개축 계획·허가신청 예약.
- Stage 11: 고용/재고/회계·권한/용량 불변식 검증 후 C-D0 단일 월 커밋.

API: RegisterMerchantOperation(authorized_issuer, firm_ref, site_ref, scopes), GetCommercialCapacity(firm, site, snapshot), GetConsignmentRights(goods_lot, actor, month), PlanMerchantOrders(operation, snapshot), ObserveMerchantPerformance(operation, valuation_basis), ExplainCommercialOutcome(operation/event, snapshot).
모든 쓰기 함수는 C-D0 ProposedEvent/JournalBatch를 통해서만 반영한다.

## T1-7. 불변식·거부 사유·게이트

- GT1-INV01 Firm/ProductionSite/POP/Ownership/Financial/Inventory 정본 중복 금지.
- GT1-INV02 상인 principal과 agent/broker의 title·custody·위탁수수료 경계 유지.
- GT1-INV03 동일 실제 로트가 본사·상인·수취인·운송중에 중복 계수되지 않음.
- GT1-INV04 무역센터/보관시설의 실제 소유/고용/용량·비용 조건 위반 불가.
- GT1-INV05 직거래 허용 시 무역센터 없음만으로 유효 거래 부정하지 않음.
- GT1-INV06 무계약·무노동·무자금 상인의 재판매/무료 수수료 지급 금지.
- GT1-INV07 완료 거래를 임의 취소해 과거 물품·대금 자동 복원 금지.
- GT1-INV08 상품 재판매를 신규 생산량·국가 GDP에 두 번 합산 금지.

거부코드: GT1_ERR_NO_AUTHORITY, INVALID_PRINCIPAL, INSUFFICIENT_WORKING_CAPITAL, NO_STORAGE_CAPACITY, INVALID_CONSIGNMENT, NO_LABOR_SERVICE, DUPLICATE_OWNERSHIP, REVISION_CONFLICT. 정당한 시장 실패/자금부족은 정상 거부·부분 체결로 처리; 원장 훼손/ID 고아 참조는 HARD ERROR.

검증은 [무역 인수시험](TRADE_ACCEPTANCE_TESTS.md)의 GT1-T01~T10 및 G-D0 공통시험을 따른다. **문서 설계일 뿐 구현·실행되지 않았다.**
