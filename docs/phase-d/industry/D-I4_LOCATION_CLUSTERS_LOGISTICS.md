# VIC2.5 D-I4 — 산업 입지·집적·공급망·물류·지역산업

> **설계 확정 / C++ 연산·시험 미실행** · `phase-d-industry/1.0` · 2026-11-04  
> 상위: [D-D0](../D-D0_COMMON_CONTRACT.md), C-D0 `Region/TerritorialJurisdiction/MarketAssignment`, C-E1/E2/E3/E5, D-C1~C5, POP-D0~D3. [색인](README.md)

## I4-1. 산업의 실제 지리적 주소

- 산업의 위치는 기존 C-D0 `RegionId`/`ProductionSite.region_id`를 사용한다. 별도 `IndustryRegion.population` 같은 인구 원본이나 전국 단일 공장 좌표를 만들지 않는다. S0에서는 지도 없이 지역 3개와 유효 `TransportNode/Edge`만으로 시험.
- `Region`과 `MarketAssignment`, `AdministrativeRegion`, `TerritorialJurisdiction`(주권/민정/과세/군사통제)는 서로 다른 관계. 동일 국가 안에서도 다른 상품시장/관세·법률/물류/토지·노동 상황일 수 있고, 외국 간 같은 시장도 가능.
- `SiteLocationRequirements`(규칙/계획): `site_id, allowed_region_types, mineral/land_water_access_refs, industrial_land_requirements, resource_logistics_constraints, legal_zoning_ref, power/transport_service_requirements, workforce_access_rule, pollution_environment_constraints`. 부지권과 허가/건설은 D-C1/Ownership/C-P, 설비 활성은 C-E1이 관리한다.
- 도심/항만/광산/농경지/공업지대의 강점은 **실제 위치에 존재하는 자원·토지·노동·시장·운송간선/서비스·환경규제·기존 시설**에서 도출한다. 이름만 '산업집적지'라서 생산량이 생기지 않는다.

## I4-2. 신규 시설의 입지 선택과 투자

- 기업/정부/가계가 신규 시설·확장·이전을 제안할 때 후보 지역의 유효 토지권/매장량·현장 접근, 지역별 숙련 노동/임금/근로규칙, 현지 및 타 지역 원료가격, 목적지 판매수요·가격/배송시간, 에너지·용수/폐기 비용, 세금·공공서비스, 자본·안전·법적 위험을 조사한다.
- 예상 순현재가치/현금흐름(할인·재고/운송 위험)을 사용해 후보 입지를 비교할 수 있으나 **예상값은 실제 자금 또는 상품 공급이 아니다**. 할인계수·위험프리미엄·임대/토지 비용·최소 사업크기는 버전 있는 룰셋.
- 시설 건설/증설/이설은 기존 D-C `ConstructionProject`에 자금·자재/노동·공기/운송·준공/차월 활성화를 요구한다. 지리적 좌표/지역 ID 변경만으로 설비가 순간이동하지 않는다.
- 기존 시설 이전은 실제 `AssetRelocationEvent`(철거/운송·수송기간/재설치·소유권/보험·정지 기간), 출발·목적지의 C-E1 `CapitalAsset`/`ProductionSite` 상태 일치가 필요.
- 유효 법률상 기업의 산업입지 제한/자원 개발권, 주거지/농경지 전환·토지 수용/피해 보상, 군사 점령·관세권은 C-P/C-E4/C-M을 참조. 시장·관할 변동만으로 기업 장부/법적 자산이 사라지지 않는다.

## I4-3. 지역적 산업집적은 파생 효과

`IndustrialClusterView`는 `region_id, connected_region_set, active_sites[], linked_supplier_sites[], actual_employment/qualifications, installed_infrastructure_refs[], shared_service_capacity, realized_transactions, period` 등의 **관측 뷰**이고 별도 가변 경제력/산업발전 점수가 아니다.

집적의 작용 경로를 다섯 가지로 구분:

1. **중간재 공유·납품망:** 가까운 유효 기업끼리 실제 인도/운송기간·운임이 낮으면 중간재 수급 제약이 줄 수 있다. 물류거리가 짧아져도 원료는 실제 창고·거래/재고가 필요.
2. **노동시장 풀:** 유효한 전문 직무·교육/훈련·교통/통근권에 따른 구직자/고용주 접근성이 상승할 수 있다. 총 주민 인구가 증가하지 않으며 타 지역 노동자를 즉시 추가 생성하지 않는다.
3. **공동 인프라:** 전력/용수/항만·철도·창고/정비 같은 설치된 `CapitalAsset`/`TransportEdge/Node`의 유효 서비스용량·실제 경쟁과 비용으로 생산 제약 변화.
4. **지식 접촉:** 기업·교육/연구기관·숙련직종의 유효 기술이전/훈련 경로는 D-T `KnowledgeRegistry`·`SiteTechniqueAssignment`에서 실제 지식 확산의 **입력**. 인근 연구소가 있다고 모든 시설의 생산공정이 자동 업데이트되지 않는다.
5. **부정적 집적효과:** 고용 경쟁·임금/임대료/가격 상승, 혼잡/대기·환경오염/수질 악화, 위험·보건 부담 및 인프라 과부하도 실제 시장·행정/환경 서비스에서 발생한다.

수치적인 지역 집적 보정(예: 교육/전문서비스 접근에 따른 설정비 절감)이 필요하면 반드시 **관측 가능한 원천 관계, 명시적 한계와 비용/병목, 근거 있는 Ruleset**을 갖도록 한다. 여러 시너지 계수를 무한 곱하거나 국가 전체에 무료 '공업화 보너스'를 뿌리지 않는다.

## I4-4. 공급망 그래프와 운송/재고 일치

- `SupplyChainLink`는 `buyer_site_id, seller_site_id, good_id, order/contract_refs, shipment_refs, actual_deliveries, valid_month`를 참조하는 **관측 관계**. 상인/재고/중간단계가 있으면 그 실제 소유·보관·거래 흐름대로 경로를 기록.
- `stage1`의 생산계획/원료 주문은 `OpeningSnapshot` 가격·재고와 이미 선적된 물량/유효 운송망을 확인. 조달부족/수송지연·분쟁·관세·금수/전시 봉쇄는 기대조달비·실효 발주량을 바꿀 수 있다.
- `stage3`에 실제 도착·인도/통관된 물품만 `stage4` 생산 투입 가능. `stage4` 새로 생산한 상품은 `stage7` 시장 거래/출하는 가능하지만 같은 달의 과거 `stage3` 조달로 소급될 수 없다.
- 운송은 C-E5 `Shipment/TransportCapacity`에서 실제 보유자·법적 소유자·수송 서비스·통과권과 `travel_months`를 보존한다. 무제한 대륙간 0개월 이동이나 외국 기업의 무허가 통과를 금지.
- 특정 상품·지역 공급망의 붕괴는 다른 산업의 원료 부족·가격 상승/일감 감소·소득/세금 감소·정치적 불만/군수 부족을 초래할 수 있으나, **실제 거래·인원·세금·보급 이벤트의 결과**로 전파한다. 공장 A의 문제가 모든 다른 공장의 재고를 즉시 삭제하지 않음.
- 공급망 구조가 여러 단계로 순환해도 월별 생산 입력이 `stage3` 이전에 확정된 상품에만 의존하므로 같은 월의 무한 상호생산 원형루프는 금지. 장기적으로는 반제품·재활용상품이 월을 지나 순환 가능.

## I4-5. 공익·환경·인구 이동

- 산업 성장/폐업의 임금/구인·주거/교육/보건/인프라 접근·물가·오염/환경은 POP의 실제 가계소득·교육/보건·이주/정치적 반응 입력으로 전달한다. 단순히 '공장 수 +10'으로 자동 도시인구 증가 금지.
- 산업화·통근·노동자/가족 이동은 기존 POP-D2의 `PopulationEvent`/`residence_region_id` 및 가구·계좌/고용 관계를 함께 이전. 군사동원·사상/질병 발생 시 당월 확정 노동과 임금을 소급 무효화하지 않음.
- 공공 인프라/산업단지 조성은 D-C의 실제 공사·법적 토지권과 C-E4 예산/계약을 거쳐야 하며, 완공 후 유효 서비스용량을 제공하는 시설에만 효과.
- 환경배출·수질/사고·산업재해의 건강/정치/사회 영향은 실제 피해 인구·의료/행정 서비스와 유효 법률에 연결. 규제를 위반했다는 사실만으로 정부 계좌에 벌금 수입 자동 생성 금지.

## I4-6. 지역 산업 통계·안전게이트

- 산업별 `production_origin_region`(실제 생산 지역)과 `sale_market`(판매 청산 지역), `customs_region`(통관 영역), `ownership_home_jurisdiction`을 독립 유지. 수출은 생산산업·무역/운송업·국내/해외소유와 각각 별도의 지표다.
- 지역별 산업 생산·부가가치·시설가동률·실업/임금/투자·운송원가/부족률·지역산업 다각화·집적/혼잡 정도는 실제 원장/시장 통계에서 산출하며 기업본사 소재지 하나에 모든 생산을 귀속시키지 않음.
- 입지 변경·산업단지 생성·시장통합/군사 점령·관세협정이 없어도 산업분류 통계는 생성되며, 반대로 행정지도 화면이 없어도 실제 생산/교역은 동일하게 유효.

**상세 시험:** `DI-T28..T36`. [인수시험](INDUSTRY_ACCEPTANCE_TESTS.md).
