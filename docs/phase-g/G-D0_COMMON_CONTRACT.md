# G-D0 — 상업·무역 공통 설계 계약
> **2026-11-05 시간 계약 변경 — [ADR-TIME-001](../adr/ADR-TIME-001_DAILY_TICK_B-J.md) 및 [ADR-TIME-002](../adr/ADR-TIME-002_LEGACY_SAVE_MIGRATION.md):** 게임의 기본 진행 단위는 `1일`, 외부 커밋은 `CommitDay`, 정기 월별 정산은 달력상 월말에 정확히 한 번이다. 아래의 `월별 12단계` 및 `effective_month` 문구는 신규 게임의 일별 실행 규범이 아니라 **구형 MonthClose 엔진의 호환 사양**으로 한정한다. 인구·금융·재고·주권의 유일한 정본과 기존 계산의 보존식은 유지한다. 현재 C/D/E/G/H 부분 구현을 전체 B~J 일별 엔진 완료라고 판단하지 않으며, I/J는 설계 상태다.


> 계약: phase-g-common/1.0 · 기준일: 2026-11-04 · 상태: SPECIFIED / C++ 미구현·인수시험 미실행  
> 대상: 독립 Phase G(국내 상업, 국제무역, 상업기업, 거래조직, 무역정책 적용)  
> 작업 브랜치: work/phase-g-h-d0 · 기준선: work/phase-e-cpp-integration  
> 의존: POP-D0~D3, C-D0/C-E1~E5/C-P/C-M/C-D4, D-D0/D-V(설계 정본은 work/phase-d-design), E-D0/E-G1~G5. 후속 Phase F(교통·물류)·Phase H(외교) 인터페이스를 예약한다.

## 0. 목적, 경계, 사용자 조작 원칙

1. Phase G는 독립된 **상업·무역 행위의 결정과 이행 조정자**다. 국내 지역 간 매입·유통·판매, 수출입, 중개, 무역회사·무역센터의 영업, 계약·상대방·거래 경로 선정, 무역 제약 및 실행 이력을 담당한다.
2. Phase C 경제는 상품/시장가격/주문체결·금융/FX·세금 원장, Phase D 산업은 생산과 설비, Phase F 물류는 실제 수송, Phase H 외교는 조약·국제적 의무를 담당한다. Phase G가 이들 원본을 복제하거나 세계시장에 무료 물품을 생성하지 않는다.
3. **원인 중심 조작, 결과 중심 시뮬레이션**: 플레이어는 관세·통상정책·금수조치·정부 조달·전략적 투자 등 실제 행동을 결정한다. 보통 기업의 판매 상대·물량·운송 결정은 수익성과 가용자원에 따라 자율 실행한다. 별도의 '수출량 +10', '무역력 구매' 버튼은 두지 않는다.
4. 국가가 하나의 시장을 구성한다고 전제하지 않는다. 지역별 시장, 복수 국가의 공통시장, 단일 국가 내 관세경계, 국내 중개거래, 제3국 경유·재수출을 모두 지원한다.
5. Victoria 3의 민간 자율무역·무역센터·시장 접근과 세계가격의 **개념만 참고**한다. 자동 생성 상품, 국가 단위 무료 무역능력, 수입량 강제 설정, 하드코딩 원작 수치/파일/계산식은 채택하지 않는다.
6. 본 계약은 구조·소유권·시점·불변식을 확정하는 D0이며 세부 수익성 모델, 역사적 운임·무역세율, 경로탐색 알고리즘은 G-T1~T5, F, 규칙 파일에서 확정한다.

## 1. 상위 계약과 C-E5 이관 원칙

**중요:** C-E5_TRANSPORT_INTERNATIONAL_TRADE.md는 기존 확정 경제 계약이며, 운송·통관·FX·거래를 이미 정의한다. G가 무언가를 '새로 소유'한다고 C-E5 런타임의 원장을 이중 생성하지 않는다.

- 현존 구현/계약의 Order/TradeLedger, InventoryLedger, FinancialLedger, FiscalLedger, OwnershipLedger, Shipment/TransitInventory, FXTrade/환율 규약을 그대로 사용한다.
- 목표 분담: **G = 거래 상대·상업활동·상업전략·시장 연결·계약 이행 조정**, **F = TransportNode/Edge/Capacity·FreightContract·Shipment의 물리 운송 실행**, **C-E2/E3/E4/E5 = 시장 체결·재고/금융/FX/조세·통관의 기존 회계 정본**, **H = 통상조약/제재 등 국제 합의의 법적 정본**, **C-P = 국내법과 집행권한**.
- C-E5의 TradeAgreement는 H 조약이 구현되기 전에는 기존 버전의 유효 규칙으로 읽는다. H가 연결되면 법률/조약에서 산출한 TradePolicyProjection 조회로 이행하되 **동시 쓰기 가능한 두 개의 TradeAgreement 정본을 유지하지 않는다**. CustomsAssessment는 기존 재정/통관 권한에 귀속한다.
- Phase F가 미구현이면 **C-E5 운송 기능을 어댑터로 사용**한다. 지원하지 않는 길·운송능력·결제·통관을 임의 상수로 처리하지 않고 Unsupported/NoCapacity 등 명시적인 상태로 처리한다.
- C-E5와 F/H 사이 책임 이전은 ADR + 계약 버전 증가 + 저장데이터 마이그레이션 + C-D4/기존 E5 게이트 회귀를 거쳐서만 적용한다. 이 D0 문서만으로 기존 상위 계약을 자동 폐기하지 않는다.

## 2. 최소 객체·식별자

모든 신규 ID는 기존 C-D0 typed EntityId를 따르는 안정적·비재사용 ID(0은 invalid)이며 생성/해산·시간 이력과 출처를 유지한다.

| 객체 | 필수 관계/필드 | 단일 원본과 구분 |
|---|---|---|
| CommercialMandate | mandate_id, firm_id 또는 적법한 비법인 주체, 상품/지역 범위, 권한, 유효기간, revision | 상업 행위 허가·전략. 신규 Firm 아님 |
| TradeOpportunity | opportunity_id 또는 파생키, origin_market/region, destination, good, expected_spread, assumptions, snapshot/revisions | **파생 제안/캐시**. 재고·돈·확정 주문 아님 |
| TradeExecution | execution_id, buyer/seller/optional broker, linked order/trade/contract/shipment/financial IDs, terms, milestone status, source_event_id | 거래 진행 조정. Order/TradeLedger를 대체하지 않음 |
| MerchantOperation | operation_id, existing firm/site, employed people/asset/service usage, active markets, capabilities | 기업/사업장 관계. 별도 노동/생산 원본 아님 |
| CustomsTerritoryBinding | territory_id, province/region 및 권한·기간/법적 근거 | C-P/E 관할에서 관리되는 관세권의 **조회·참조 관계**; 주권/시장 ID와 동일시 금지 |
| TradePolicyProjection | policy_id, applicable law/treaty terms, good/origin/destination, rate/ban/quota/exemption, effective interval, rule_revision | H/C-P의 유효규칙을 합성한 **read-only 적용 뷰** |
| CommercialPerformanceView | firm/market/country, month, goods, margin, trade flows, costs, trace IDs | 원장/사건에서 재계산한 통계; 금전·실물 원본 아님 |

TradeExecution은 **계약적 진행상태, 실물 인도상태, 지급상태**를 독립 축으로 보유한다. '계약 체결', '통관 완료', '입금 완료'를 하나의 완료 플래그로 혼합하지 않는다. merchant의 재고 소유자가 변경되면 Inventory/Ownership, 돈이 이동하면 Financial에 반드시 대응 사건이 있어야 한다.

기업·운송업자·상업센터는 기존 Firm/ProductionSite/Employment/CapitalAsset를 사용한다. 무역센터를 운영하려면 실제 공간/건물·인력·운전자금·상품 보관과 서비스 비용이 필요하다. **수요를 충족시키는 무형의 국영 상인이나 무제한 무역 슬롯을 생성하지 않는다.** 기업이 직접 수출입하는 것도 허용한다.

## 3. 시장, 가격, 거래의 의미

- Market과 MarketAssignment는 C-E2/C-D0의 정본이며, E-D0 Province·주권·시장 소속은 개별 관계다. '국경이 같음 = 같은 시장', '시장 접근 있음 = 수송 가능', '공동시장 = 영토 통합'은 모두 성립하지 않는다.
- **세계시장(GlobalTradeReference)**은 여러 실제 시장/거래에서 관측한 국제 참고가격·매물·정보접근의 파생 뷰다. 한 가격에 무한 공급하는 세계 공동 창고가 아니다. 매수자와 매도자, 실제 매물과 자금, 통행권·운송능력이 없으면 거래하지 못한다.
- TradeOpportunity는 단위 환산/통화·가격시점·품질·계약조건을 명시해 비교한다. 예상 이익은 아래 수익/비용의 관측 가능한 부분을 합산한 **의사결정용 추정값**이며 현금이나 확정 이윤이 아니다.
  예상 순이익 = 예상 도착지 판매대금 − 출발지 매입대금 − 실제 추정 운송/보관/보험/금융/통관/FX비용 − 기대 손실.
- 기대손실은 확률·사건 기반 추정치며 현금의 별도 수수료가 아니다. 실제 손실, 세금, 보험료, 금융이자, 가격차는 발생한 사건에 따라 정산한다.
- 수익성만으로 거래 가능량을 결정하지 않는다. 입고 가능 재고, 주문량, 최소 거래 단위, 법적 허가, 운송구간별 용량, 통관 한도, FX 유동성, 자금/대출 가능액, 회사의 위험 허용수준이 동시에 상한이다.
- 동일 상품이 유통·재판매될 수 있으나 GDP 등 생산 통계에 재판매액을 신규 생산으로 다시 합하지 않는다. 상업서비스가 실제 노동·자본으로 생산한 부가가치와 상품 재판매 차익/평가손익을 구별한다.
- 국내 직접거래/기업간 주문도 정상적인 C-E2 거래다. 모든 국내 매매가 무역회사를 경유해야 하는 것은 아니다.

## 4. 실행 단계와 상태 전이

1. **Discovery:** 기업/정부의 실효 수요와 시장별 공급·가격/접근권·운송비·위험·정책 스냅샷을 기반으로 후보를 탐색한다. 외부 정보를 모르는 주체는 그 정보 접근 조건에 따른 후보만 검토한다.
2. **Feasibility:** 거래 당사자 유효성, 실제 매물과 자금/신용, 적법한 통관·관세·쿼터, 운송 경로·수송능력, FX·보험·기한을 검증한다.
3. **Reservation:** C/D 공통 자금·상품·노동·설비 및 F/C-E5 수송 예약에 명시적으로 참여한다. 독립 G 예약 장부를 병렬 정본으로 만들지 않는다. 실패한 복합 예약은 전체 취소한다.
4. **Contract/Order:** 실효 주문/체결은 C-E2 Order/TradeLedger에 기록한다. 계약상 소유권 이전시점·위험 부담·대금 조건·운임/수수료·인도 조건을 확정한다.
5. **Dispatch/Transit/Clearance:** 실제 출하와 운송 중 상태는 C-E5/F Shipment + InventoryLedger로 처리한다. 관세 채무는 유효 통관사건에서 발생한다. 통과무역·재수출은 거래와 물리적 통과량을 구분한다.
6. **Delivery/Settlement:** 실제 도착·통관·수령검수와 판매대금/외상/FX/보험을 각각 이행한다. 미지급·미배송/압류·분실·반품·파산은 독립된 상태·보상 사건으로 처리한다.
7. **Close/Replan:** 당월 관측 가격·거래량·손실·마진을 파생 통계로 작성하고 다음 달 계획에 사용한다.

부분 출하·부분 인도·부분 대금지급은 각각 누적치와 잔량·채무가 있어야 한다. 취소나 계약 위반이 이미 인도된 물품과 지급된 돈을 자동 복구하지 않는다.

## 5. 국내상업·국제무역·관세·통화

- 국내 지역간 상업도 실제 물자/서비스가 지역을 이동하면 유효 운송과 유통비용을 요구한다. 동일 창고 내 소유권만 바뀌는 거래는 불필요한 장거리 운송을 발생시키지 않는다.
- **국제무역 여부**는 계약상 국적·관세구역·상품 소유권 이전·물리적 국경 통과 중 어떤 통계/규칙을 말하는지에 따라 판정한다. 관세징수는 주권 변경 자체가 아니라 유효 CustomsTerritory/CustomsAssessment 조건에 따른다.
- 각 거래에 적용되는 관세·수출입 제한·보조금·쿼터·원산지 규칙은 C-P 유효 법률과 H 유효 조약의 **하나의 TradePolicyProjection**으로 조회한다. 충돌·우선순위/예외는 법적 위계와 유효일에 따라 명시적으로 해결한다. G가 임의 법적 효력이나 외교조약을 새로 생성하지 않는다.
- 환전/환율/외환통제·국제결제는 C-E3/E5의 FXTrade와 FinancialLedger가 정본. 통화별 지급자·수취자·대응 채권과 유동성이 없으면 허구의 1:1 환전을 허용하지 않는다.
- 제재·봉쇄·전쟁·전염병·기후·조약 위반은 수송·거래의 위험/접근가능성을 바꾼다. 이미 출하한 화물에 대해서는 압류·지연·손실 사건을 남기고 조용히 삭제하지 않는다.
- 국가별 수출입·무역수지·관세수입·국제수지는 원장·통관·실제 소유권 이전 사건의 **시점·통화·평가기준을 명시한 집계**다. 같은 재수출 화물을 순수 국내 생산/생산 GDP로 다시 잡지 않는다.

## 6. 통합 월별 12단계(변경 금지)

| Stage | 무역 실행 책임 |
|---:|---|
| 0 opening_snapshot | 전월 확정 화물 도착/검역·통관 가능성, 유효 시장·관할·조약/법률/관세를 이번 달 개시 스냅샷으로 고정 |
| 1 planning_and_credit | 기업별 거래 후보·계약/운송/자금·FX/쿼터 검증, 공통 예약, 무역회사 운전자금/고용 계획 |
| 2 labor_clearance | 실제 상업시설/운송업체 직원의 고용·노동역량을 기존 EmploymentLedger로 검증 |
| 3 intermediate_goods | 원자재/중간재 실효 구매·당월 가능 출하, 재고·금융 이동; 원거리 미도착분을 현재 생산에 사용하지 않음 |
| 4 production | 산업에서 실제 생산한 신규 재고만 후속 유효 판매 후보로 반영; 상업기업 서비스 생산은 실제 투입 제약 준수 |
| 5 wages_and_transfers | 상업·물류업체 실제 임금/정부 지원 지급을 기존 원장에 기록 |
| 6 pop_consumption_plans | 가계의 현재 현금·가격·접근성에 근거한 최종재 수요 계획 |
| 7 final_goods_market | 최종재·정부조달·투자재 실제 체결·출하·인도·세금·대금 사건 |
| 8 closing_accounts | 무역업체 손익/채권/채무·관세채무·손실·보험/금융 비용 결산 |
| 9 saving_and_investment | 차월 투자/정책·법률·국제조약 변경 예약, 아직 미실현 결과를 당월 거래로 소급하지 않음 |
| 10 pop_demography_and_health | 사건으로 발생한 국경/군사·이주/위험 변화의 다음달 영향 예약. 현 월 이미 실행된 거래를 소급 수정하지 않음 |
| 11 repricing_and_validation | 차월 시장가격과 상업 참고가격·재고/재정/수송 예약/법적 이력 검증 및 세계 원자 커밋 |

기존 C-E5의 양(+) 운송기간과 월초 입고 규약을 따른다. 동일 월 도착 가능한 단거리 거래는 C-E5의 결정론적 하위 사건이 공식 허용할 때만 가능하다. 단계 간 순환 의존은 다음달 스냅샷을 통해 해소하고 무한 반복/비결정적 실시간 재계산으로 처리하지 않는다.

## 7. 공개 인터페이스 초안

- EvaluateTradeOpportunities(firm, origin, destination_scope, good, opening_snapshot) → estimated candidates + assumptions + cause refs. **상태 변경 없음**.
- ProposeTradeExecution(issuer, order_scope, constraints, expected_revisions) → C-D0 ProposedEvent 및 예약 요청. 승인 전 확정 재고/현금 변화 없음.
- RequestTradeCapacity(execution, route, date) → F 또는 C-E5 운송 어댑터의 예약 결과.
- ResolveApplicableTradeRules(good, origin, destination, transit, month, snapshot) → C-P/H/E 기반 effective policy view.
- ObserveTradeExecution(execution_id, snapshot) → 주문/운송/인도/결제별 진행 및 오류.
- GetTradeStatistics(scope, period, valuation_basis) → 원장 기반 국가·시장·기업별 수출입/수익/의존도 및 provenance.
- ExplainTradeDecision(execution_or_policy, snapshot, scenario) → 예상 직접효과/간접효과/불확실성, 실제 실현치와 원인 경로.

각 호출은 snapshot_id, ruleset_version, 관련 레지스트리 revision을 고정한다. 상태변경은 C-D0 Command→ProposedEvent→JournalBatch→StagingWorld→one Commit으로만 가능하다.

## 8. 설명 가능성·플레이어 UI

기본 UI는 수입/수출/가격/부족·관세 및 선택 가능한 정책만 요약한다. 툴팁은 관세 변경의 예상 수입원가·기업원료비·소비자물가·세수·상대국 교역 및 장기 공급망 영향 중 근거 있는 효과를 나타낸다. 실제 변화와 사전 추정치는 분리, 지연·자금 부족·운송 병목 등 상쇄원인을 추적한다. 수치 예측은 확정이 아닌 한 범위·가정·불확실성을 표시한다. 호버 전용 UI를 강제하지 않는다.

## 9. 공통 불변식

- GD0-INV01 상업시설/무역회사는 기존 Firm/ProductionSite/Employment의 관계로만 존재하며 중복 기업·인구 원본 없음.
- GD0-INV02 상품 보유량은 InventoryLedger 원본 하나; 주문·계약·화물 참조를 중복 재고로 합산하지 않음.
- GD0-INV03 소유권·보관권·인도·결제는 독립 상태; 대응 Ownership/Financial 사건 없이 변경된 것으로 보고하지 않음.
- GD0-INV04 통화별 대금/외상/FX 분개 균형 및 지불·재고·용량 이중예약 금지.
- GD0-INV05 실효 경로/법적 허가/관세/쿼터/FX를 충족하지 않으면 출하·통관·지급 거부.
- GD0-INV06 양의 운송기간 화물은 월초 실제 입고 이전에 수취지 가용재고로 사용 금지.
- GD0-INV07 무역량·상업이익·국가 무역수지·GDP는 확정 원장/정의된 평가 기준에서만 집계.
- GD0-INV08 생산량·소유권 이전량·물리적 국경 통과량·통관량을 혼합하여 이중 계수하지 않음.
- GD0-INV09 TradePolicyProjection은 유효한 법률/조약/관할 참조와 일치; 별도 무역법 원본 금지.
- GD0-INV10 시장·국가·관세구역·행정구역·프로빈스를 같은 ID로 간주하지 않음.
- GD0-INV11 부분거래·분실·취소·반품·부도에 대한 잔액/재고·채권 추적 정합.
- GD0-INV12 타 분야 사건·계약 효력은 월 단계·유효시점·원장 revision을 존중; 당월 소급수정 금지.
- GD0-INV13 동일 Command/Event/Execution replay 멱등·안정적 ID·완료/취소 이력 보존.
- GD0-INV14 잘못된 복합 실행은 전영역 staging 롤백; 실패 후 부분 자금/상품/운송용량 예약 없음.
- GD0-INV15 동일 초기 상태·명령·시드·규칙과 동등한 워커 수에서 같은 결과/원장 해시.
- GD0-INV16 정보 접근 제한과 불확실성·임의 매수자/매도자 부재를 정당한 거래량 0과 구분.

## 10. 최소 인수 시나리오(세부 G-V에서 확장)

- GD0-T01 국내 제조업체 직판; 무역회사 부재에도 주문·지급 정상 체결.
- GD0-T02 별도 무역회사가 상품을 매입·재판매; 이중 생산/GDP/재고 계수 없음.
- GD0-T03 2개월 해상운송·운송중 사고/보험과 도착시점 검증.
- GD0-T04 운송용량 8·구매요청 10이면 최대 8만 실효 예약/출하.
- GD0-T05 양국 공동시장이나 관세동맹에서도 무조건 영토/주권 통합이나 관세 납부가 발생하지 않음.
- GD0-T06 동일 국가 내 관세선과 다국가 단일 시장을 구분하여 징수.
- GD0-T07 관세 조약 m 체결·m+1 효력: m 기존 계약/통관 소급 변경 없음.
- GD0-T08 FX 유동성 없음: 타 통화 거래 명시 거부, 가짜 1:1 환율 없음.
- GD0-T09 관세/봉쇄로 인한 가격·공급 영향이 재정·기업·고용·외교 통계에 단 한 번 반영됨.
- GD0-T10 부분 배송/반품/외상 미지급의 인도·채권·상품 수량 일치.
- GD0-T11 실패 명령 재시도/저장복구/워커1/N 결정론·원자적 실패 롤백.
- GD0-T12 정책 툴팁의 예상/실제/제약·기여 원인 분리 및 동일 엔진 데이터 기반 설명.

## 11. 다음 단계와 완료 조건

- G-T1 상업 행위자·기업/무역센터·국내 유통.
- G-T2 시장 연결·지역간/국제 상업·수익성/정보 접근.
- G-T3 계약/선적/재수출·무역 실행/서비스.
- G-T4 통상정책·통관/환전/관세/제재.
- G-T5 공급망 위험·무역금융/손실·실제 통계/설명.
- G-V 교차 인수시험: 모든 GD0 불변식과 위 시나리오, 기존 C-E2/E3/E4/E5 및 D/E·H(연결 시)의 회귀와 장기·성능·세이브 검증.

**명시적 상태:** 본 문서의 작성은 설계 정본 등록이다. C-E5의 구현 이전, F/H 미설계/미연결 부분, 전체 Phase G C++ 엔진 및 G-V 실제 통과는 별도이며 DONE으로 표기하지 않는다.
