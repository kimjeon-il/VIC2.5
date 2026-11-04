# G-T2 — 시장 연결·세계시장 참조·자율무역 의사결정

> 설계: phase-g-trade/1.0 · 2026-11-04 · SPECIFIED / NOT_IMPLEMENTED
> 상위: [G-D0](G-D0_COMMON_CONTRACT.md), [G-T1](G-T1_COMMERCIAL_ACTORS_MERCHANTS.md), C-E2/C-E3/C-E5, E-D0/E-G1~G5. 후속 F 물류 및 H 조약은 유효 조회/어댑터 경계만 예약한다.
> 질문: 어느 실제 시장과 거래할 수 있는가, 무엇을 얼마에 누구에게 판매할 유인이 있는가, 경쟁 상황에서 어떻게 주문과 자원을 배정하는가?

## T2-1. 시장 구조

- **MarketId**: 주문·체결·가격의 C-E2 단일 정본. **MarketAssignment**: Region/Province 및 상품·시점별 기본 시장 소속(상위 C-D0/E-D0). 한 국가 여러 시장, 여러 국가 한 시장 가능.
- **LocalCommercialLocation**: 실제 창고/사업장·보관자·행정·시장 접근점. 같은 시장에 속해도 지리·운임·보관비·수송 병목이 서로 다를 수 있다.
- **CustomsTerritory**: 과세/통관 관할의 법적 범위. Market 또는 Country와 동일시하지 않는다.
- **PhysicalNetwork**: F/C-E5의 TransportNode/Edge/Capacity 및 소요시간·운송비·경유국 통과권. 해상·내륙·국경·철도·하천·연안 경로 등을 유효 시설/법률에 따라 평가한다.
- **GlobalTradeReference**: 실제 여러 시장의 체결/호가·국제 가격·접근 가능한 매물의 **통계적 참조 뷰**. 세계시장 관측정보를 제공하지만 주문 상대나 실물/현금 보유자가 아니며 세계 통합재고/별도 마켓 청산자로 취급하지 않는다.

### 세계시장 참조값 산출

1. 상품·품질·가격 로트·관측 기간·통화 환산기준·시점이 서로 비교 가능한 **실제 TradeFill 또는 유효 매도/매수 호가**만 채택한다.
2. 관측된 유효 국제 체결량을 가중한 중앙값 또는 구간가격(규칙파일 선택) 등 robust estimator를 사용하며, 시장별 정보공개·통신비용에 따라 기업별 조회 가능성을 제한한다. 세계 단일 가격이 모든 로컬 시장의 법정 체결가가 되는 것은 아니다.
3. FXLiquidity·환율·가격 출처가 불명인 경우 결과는 unknown/estimated이며 '0원'으로 대체하지 않는다. 다른 통화 가격을 하나로 더할 때 기준통화·거래가능 환율과 단위/시점이 필요하다.
4. 현재 시장의 **실효 매수/매도 주문, 상대방, 유효 자금, 배송 가능 재고가 존재하지 않으면** 세계참고가격만 보고 실제 거래를 체결할 수 없다.
5. 전체 국제 가격 참고 지표는 선택 사항. 구현 초기에는 시장별 호가/실제 주문을 통해 거래 가능하며, GlobalTradeReference가 아직 없다는 이유로 상업 전체가 정지하지 않는다.

## T2-2. 수요·공급·접근성과 정보

- 실효 공급은 C-E2가 **그 시점에 매도로 제공할 수 있다고 검증한 Inventory/서비스량**이다. 명목 생산능력, 미완료 공사, 운송 중 재고, 제3자 소유 위탁 물품을 매도자 자기재고로 중복 합산하지 않는다.
- 실효 수요는 POP·기업·정부의 실제 주문 또는 규칙상 유효할 것으로 예측한 계획으로 구별한다. 구매 예산/신용이 없는 잠재 욕구는 거래를 확정할 수 없으나 기회 탐색의 미충족수요 관측값은 될 수 있다.
- 접근성은 실제 연결그래프 + 운송능력·소요시간 + 항만/검수/창고/통신·정보 + 통행권·시장접근·관세법 + 금융/FX 가능 여부를 **각각 검증한 결과**다.
- 육로 접경이 아닌 지역도 합법적인 환적·내륙 경유·해상운송으로 접근 가능할 수 있다. 내륙국은 적법한 통과권과 실제 내륙/해상 경로가 있는 경우만 원거리 교역 가능하며 '내륙 = 무역 불가능'으로 고정하지 않는다.
- 국경이나 시장 소속 변경은 시장접근 재계산 트리거지만 영토·재고/현금을 즉시 이동시키지 않는다.
- 제도와 시대에 따른 가격정보 전달 지연, 상업네트워크의 신뢰/정보접근·품질 차이, 독점 공급은 실제 비용/정보의 제약으로 처리한다. AI 상인의 전지적 전세계 호가 접근 기본값 금지.

## T2-3. 후보 객체·경제적 비교식

후보는 (trader, seller, buyer 또는 eligible market, good+quality, origin_market/physical_origin, destination_market/physical_destination, path, expected delivery month, settlement currency, legal_scope, validity, snapshot/revisions, evidence)로 구성한다.

단위는 C-E2의 Price{currency_id, good_id, price_lot}와 Quantity{scaled_units}를 그대로 사용한다. 공통 통화 비교는 **실제 가능한 FX 견적·현금/신용/상대방**이 있을 때만 한다.

표준 후보 비교(설명용, 세부 계수는 규칙파일):
- expected_sales = executable estimated destination revenue for quantity q (실제 매수 호가/계약·판매 가능성 반영)
- acquisition = origin goods purchase + nonrecoverable acquisition fees
- expected_cash_cost = freight + loading + storage + insurance premium + expected actual customs duty + financing interest and fees + FX and handling costs
- expected_loss = expected physical/credit loss severity × likelihood, adjusted for **who contractually bears risk**, with uncertainty
- expected_net = expected_sales − acquisition − expected_cash_cost − expected_loss
- expected_unit_margin = expected_net / q (rounded fixed point, q>0); expected return on constrained working capital, transit time, concentration risk, maturity mismatch are separate decision inputs.

**주의:** 기대손실은 이미 현금비용에 포함된 보험료를 중복 계산하지 않는다. 보험으로 위험이 이전되었다면 실제 지급능력과 자기부담만 남는다. 관세는 정당한 납세 관할·과세기준·시점에 해당하는 것만 비용으로 평가한다. 운송에 필요한 연료·선원·상품 중량과 통과 허가는 F/C-E5 정본; G가 추상 convoy를 생성하지 않는다.

## T2-4. 구현 대상 의사결정 파이프라인

**핵심 정책: '개별 기업 후보선택 → 실제 주문·예약 → 기존 시장 청산'을 사용한다.** 세계 단위 단일 의지/국가별 수출할당 엔진은 만들지 않는다.

1. **관측(0단계)**: 각 기업의 known markets 및 상품 정보, 매도호가/매수호가, 유효 통관·조약·FX·물류 견적을 opening snapshot에 고정. stage3/7의 후속 확정 거래는 앞 단계에 소급 영향 없음.
2. **후보 생성(1단계)**: 상품별 실제 매도 가능한 공급처와 주문·실효수요가 있는 목적지를 인덱스로 조회. 법적으로 불가/물류 불가/재고·신용 전혀 없는 쌍은 초기부터 제거.
3. **견적·상한**: 1개 후보의 최대량은 실제 판매 재고, 매수/판매능력, 자금·FX/신용한도, 소유권·매매허가, 운송 구간별 용량, 상품 로트·상업시설/검수 처리량, 유효 할당량, 위험한도, 기한 모두의 상한 이하.
4. **기업별 선택**: 재고회전·유동성·마진·장기 계약 의무와 위험노출을 감안해 주문을 제안. **단기 손실 예상인 거래도 장기 공급계약의 법적 의무나 정부가 실제 지불하는 보조금이 있으면 가능**하지만 실제 재원 없이 무한 거래하지 않음.
5. **공통 예약(1단계)**: Order/TradeLedger의 제안 주문, Financial/Inventory 원장 및 F/C-E5 운송 예약을 C-D0의 하나의 제한자원 조정 순서로 승인. 동일 로트/현금/창고/경로/쿼터의 경쟁 신청은 전체 수요를 비교해 중복을 거부·부분배정.
6. **체결·이행(3/7단계)**: C-E2의 실제 시장 청산과 G-T3 계약/실물 인도가 확정 가능한 수량만 실행. 계약의 도착 예정량을 도착지 당월 공급으로 미리 계상하지 않음.
7. **관측·학습(8/11단계)**: 실제 수익·손실, 주문부족, 미도착, 수급 변화로 다음달 운영전략/시장가격을 갱신(C-E2). **월중 고정 MarketQuote**를 당월 반복해서 흔들지 않는다.

### 동률·경합·비결정성

- C-E2의 시장 주문 체결 우선순위/가격/유효시점은 C-E2를 **우선**한다. G는 독자적 OrderFill 알고리즘을 만들지 않는다.
- G 상업후보 선정의 동점은 (유효 계약 우선권, expected net per binding resource, created_month, stable typed opportunity/firm ID)처럼 공개·버전 있는 총순서로 결정한다. 결과는 int64·정수/고정소수점 비교, checked 128-bit 계산을 따른다.
- 글로벌 최적해를 구하기 위해 모든 국가·기업·품목을 매월 무제한 조합 탐색하지 않는다. 후보 수 제한/희소 인덱스/캐시 재사용을 하되 같은 입력에서 같은 결과를 보장한다. 후보 누락이 발생하면 탐색상한·정보제약을 provenance로 기록한다.
- 개별 주체의 최적이 세계 총후생 극대화를 뜻하지 않는다. G는 상업 이익·시장 질서를 모형화하되, 필요한 정책적 외부효과는 C-P/G-T4에서 처리한다.

## T2-5. 국내·국제·환적 거래 유형

| 유형 | 거래 조건 | 특기사항 |
|---|---|---|
| 동일 지역 직접판매 | 현재 거래 가능한 재고·주문·동일 장소/허용 즉시인도 | 불필요한 F 장거리 수송 없음 |
| 국내 타 지역 판매 | 서로 다른 물류지점·유효 수송능력 | 같은 국가여도 실제 거리/통행료·부족 발생 |
| 공동시장 역내 거래 | 유효 MarketAssignment, 서로 다른 Country 가능 | 물류비·국경·과세권은 별도; 주권 통합 아님 |
| 독립시장 간 수입/수출 | 상대주체/오퍼·관세구역·FX·경로 | 단일 세계시장 매물 무제한 공급 금지 |
| 통과무역(transit) | 제3국 통과/환적 권리·세관 규칙 | 경유국이 항상 상품 소유·생산하는 것 아님 |
| 재수출(reexport) | 이전 수입상품의 실제 보유·소유 이력 | 원산지/소유권/통관과 GDP를 별도 집계 |
| 국가 간 전략적 거래 | H 조약/정부계약 + 실물·예산·인도·결제 | 정부 약정만으로 물품 자동 생성 불가 |

## T2-6. 가격·수요·공급의 시간 경계

- C-E2 현행 규약은 **당월 가격 견적 고정, 단계11 차월 가격 갱신**. 임의의 당월 가격 재청산·장래 수출호가 즉시 적용 금지. 지역 가격·매수/매도 호가·실제 체결가격의 구분도 유지.
- 단계3의 수입 중간재는 월초 실제 입고가 아니면 생산재료로 쓸 수 없다. 단계4에 새로 생산된 상품은 단계7의 유효 주문·예약/실물 인도 조건을 충족하면 최종판매 가능; stage1에 계획된 원거리 배송의 물리적 용량 예약이 필요하다.
- 소비자가 빈곤해 수요가 줄거나 투자가 감소한 효과는 C-E3/POP에서 유효 수요로 나타나야 하며, '국가 무역 정책 보너스'만으로 수요를 증가시키지 않는다.
- 세계 참고가격은 지역 가격·통화·유효 재고 및 위험을 설명하는 지표. 그 수치 하나로 모든 수입량을 강제 배분하거나 시장가격을 덮어쓰지 않는다.

## T2-7. 성능/조회

인덱스: (good, quality, market, period), (Firm/Operation→accessible market), (route edge capacity revisions), (treaty/policy scope), (currency/FX liquidity), (warehouse/Inventory available), (order side). 조회는 read-only snapshot/revision으로 캐싱. 변경된 품목/시장/정책·경로 주변만 재생성한다.

캐시의 revision mismatch는 재계산 또는 unknown/invalid로 반환. stale 캐시로 주문·물류 확정 금지. 후보 점수는 게임플레이에서 강제 관리를 요구하지 않고 상인/정부 정책 설명툴팁의 근거로 사용한다.

API: ListAccessibleMarkets(actor, good, month, snapshot), GetGlobalTradeReference(good, valuation_basis, snapshot), GenerateTradeOpportunities(firm, goods_scope, snapshot), QuoteLandedCost(opportunity, quantity, terms, snapshot), RankFeasibleOpportunities(firm, constraints, snapshot), ExplainMarketAccessFailure(origin, destination, good, snapshot).
상태변경은 G-T3과 C-E2/G-D0만 담당한다.

## T2-8. 불변식·거부·인수 경계

- GT2-INV01 world price/reference는 real orders/trades의 파생정보이며 별도 물품 공급자·무한 창고 아님.
- GT2-INV02 Market/CustomsTerritory/Country/Province/TransportNetwork ID와 지위 분리.
- GT2-INV03 마진 비교는 통화/품질/로트·결제조건을 일치시킨 뒤에만 수행.
- GT2-INV04 경로·허가·현금/FX·실제 매물 중 하나라도 불가능하면 성공 주문/수출입 생성 금지.
- GT2-INV05 C-E2 당월 MarketQuote/정본 거래체결·월말 가격 규약을 침범하지 않음.
- GT2-INV06 모든 경쟁 예약은 global constraint를 준수하고 안정적인 동률순서·결정론 유지.
- GT2-INV07 재판매·환적이 새로운 생산이나 수요/공급을 중복 생성하지 않음.
- GT2-INV08 정보미상·가상가격·관측 없음과 0가격/거래량을 구분.

오류코드: GT2_ERR_NO_ROUTE, NO_MARKET_ACCESS, UNKNOWN_PRICE, NO_COUNTERPARTY, NO_FX_QUOTE, CAPACITY_SHORTFALL, STALE_MARKET_REVISION, UNSUPPORTED_TRADE_MODE.

인수시험은 GT2-T01~T10 및 GD0-T04/T05/T06/T08. 자세한 시험행렬은 [TRADE_ACCEPTANCE_TESTS](TRADE_ACCEPTANCE_TESTS.md). **설계 등록만 완료, 실행은 미실시.**
