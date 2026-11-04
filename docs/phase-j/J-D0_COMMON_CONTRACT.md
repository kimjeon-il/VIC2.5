# J-D0 — 환경·자연 공통 설계 계약

> 계약: phase-j-common/1.0 · 2026-11-04 · SPECIFIED / NOT IMPLEMENTED
>
> 역할: J-N1 자연환경 · J-R1 통합 자원 · J-P1 오염 · J-D1 재해 · J-V 통합 검증
>
> 상위 호환 계약: POP-D0~D3, C-D0/C-E/C-P/C-M, D-D0/D-I2/D-T, E-D0/E-G1~G5, G/H. 단, 아래 J0-7에 명시한 **자원 매장량 모델 폐기**는 사용자가 승인한 변경 계약으로 기존 D-D0/D-I2의 해당 조항을 교체하며, 실제 D 문서/코드의 마이그레이션 게이트가 필요하다. 기타 금전·인구·지리/설비·법률 원본 소유권은 상위 계약을 따른다.

## J0-1. 목표, 영역, 금지

자연환경의 실제 조건이 생산·채굴·농업·시설·인구에 작용하고 산업/개발의 환경적 결과가 다음 달 이후에도 누적되도록 한다. 별도 미니 경제·날씨 예보 엔진·대기 유체역학·개별 동식물 미시 AI는 필수 범위에서 제외한다.

- J-N1: 기후 유형, 계절성, 토양·수분·수자원·산림/식생·토지환경(지형과 구분).
- J-R1: 지질 분포·탐사·자원 발견, 활동 중인 채굴 지점의 생산성·접근/비용, 재생자원의 현존 생물량과 회복.
- J-P1: 실제 활동에 연결된 오염 배출·이동/축적/정화와 건강·토지 생산성의 제약.
- J-D1: 자연재해의 노출/강도/실제 지역 범위와 시설·생산·사람에 대한 사건.
- 고정된 가상 지구 전체 자원 총량이나 자원별 완결된 채굴 정답 데이터를 필요로 하지 않는다.

## J0-2. 공간/ID/원본 소유권

기본 육상 공간 단위는 E의 고정 ProvinceId. 실제 역사 지리 시나리오의 POP RegionId↔ProvinceId는 E-D0의 명시적 1:1 RegionProvinceBinding을 사용한다. 무지도 S0 시나리오는 RegionId 기반 환경 fixture를 허용하고 자연 데이터 미상은 0으로 대체하지 않는다. 해역의 영향은 E의 SeaZoneId와 관련된 명시적 공간/접촉 참조로 표현한다.

새 J 엔터티 ID는 EnvironmentalProfileId, ResourceSiteId, ExplorationAreaId, ResourceDiscoveryEventId, EnvironmentalImpactEventId, HazardEventId. 타입별 안정적 비재사용 ID 및 source_event_id·revision·valid/effective_month·provenance를 요구한다.

| 자료/사건 | 원본 소유자 | J와 타 분야의 경계 |
|---|---|---|
| 프로빈스 정적 형상·지형·해역·접경·위치 | E Geography/ProvinceRegistry | J는 dataset_id/hash 기반 정적 환경 참조/지역 키만 사용; 지도 변경·주권 자동 변경 없음 |
| 현지 환경상태·기후/토양·수분·생태·오염 | J EnvironmentalState / 규칙·사건 | 환경조건은 생산/건설/보건의 읽기 입력; 실제 피해·생산은 타 분야 원장 사건 |
| 자원 탐사·지질 적합도·채굴 지점의 상태 | J ResourceSite/ExplorationState | J는 접근 가능성/생산성·비용 등 제약·발견 이력을 제공; 실제 산업 생산/상품은 C/D |
| 산림·어족 등 재생자원의 현재 생물량 | J RenewablePool | 실제 채취·자연 증가/손실·이동 사건으로 검산; 땅속 미채굴 총매장량 모델과 다름 |
| 광산·유정·학교·산업 설비 및 기술 채택 | C-E1/D Construction/CapitalAsset/SiteTechnique | 생산능력/마모·개축·인력·기계 사용은 원 소유자에서 갱신 |
| 실제 제품 생산·원료/폐기물·판매/운송 | C-E1/E2/G Inventory/Production/Shipment | 채굴·어획·벌목도 실제 ProductionEvent와 InventoryLedger에만 상품 생성 |
| 토지 사용권·벌채/채굴권·법령·환경세 | C-P/Ownership/Fiscal, E 관할 | J가 소유권·허가·화폐원장 따로 만들지 않음 |
| 인구 건강·사상·이주·가구 영향 | POP Health/PopulationEvent, C | J는 노출·재해 원인 사건 제공, 인구수/건강 정본은 POP에만 반영 |

환경 데이터는 (province_id 또는 sea_zone_id, month/season, dataset_id/hash, provenance, data_precision, revision, known/unknown)의 계약을 따른다. 지질/기후 품질이 낮은 역사 자료는 확률/추정범위로 기술하되 사실값인 척한 임의 총량은 생성하지 않는다. 프로빈스 경계나 역사 해안선·영토 귀속 변경은 J의 권한 밖이다.

## J0-3. 통합 자원 모델 — 총매장량의 완전한 제거

**강행 규칙**: 지하·비재생성 자원에 대해 지질학적 총부존량/총매장량, 추정 총매장량, 확인 회수가능 총량, 숨겨진 가상 총량, '남은 매장량' 또는 최대 채굴누적량을 게임 상태·지도 데이터·규칙·세이브·게임 내부 난수 시드로 생성/보유/사용하지 않는다. 이를 근거로 채굴량을 차감하는 고갈 판정도 금지한다. 국가 총매장량을 프로빈스에 면적 비례 배분하지 않는다.

- ResourceSite: (site_id, province_id, resource_kind, geology/quality/depth/access descriptors, discovery_status, exploration_source, operational_status, observed_productivity, extraction_history, decline_parameters, cost_conditions, rights_ref, data_precision, revision). 숫자량이 필요하더라도 **현재 작업 가능한 유량/처리량/생산성**으로 표현한다. 총량이나 잔존량이 아니다.
- ExplorationArea: 지질학적 유망도·공간별 조사 범위/증거·조사 이력·탐사기술/비용; 미발견 자원의 존재 가능성은 확률/불확실성이다. 탐사 완료 구역과 기발견 site_id를 기록해 동일 유전을 중복 생성하지 않는다.
- ResourceDiscoveryEvent: 실제 조사 노력·지질적 적합성·탐사기술·시드/근거에 따라 신규 현장이 발견됨을 기록한다. 발견은 상품재고·화폐·총매장량 생성이 아니다. 탐사 면적과 조사 기회를 유한하게 관리하고 동일 구역 무한 반복 탐사로 무제한 유전을 생성하는 악용을 차단한다.
- 현장별 월 최대 생산은 실효 현장 생산성·가동 설비/작업자·에너지/물류·안전/허가·계절 제약의 최소값을 따른다. 유정의 생산성 감소는 운영기간/실제 생산이력/현재 유량·회수기술/접근 난이도의 검증 가능한 모델로 표현하며 물리적 '남은 매장량'에서 역산하지 않는다.
- 기술 개선·가격 상승·신규 조사·현장 개보수는 실효 생산성/경제성/접근성을 개선할 수 있으나 이미 완료된 생산이나 재고를 소급 생성하지 않는다. 낮은 유량·경제성 악화로 휴지/폐쇄된 현장은 조건 변화 시 재가동 가능하다.
- 엄밀한 물질 보존에 필요한 **이미 채굴한 상품의 실제 양**은 기존 Inventory/ProductionLedger에 기록한다. 광산/유전에서의 출처와 채굴실적·환경피해는 사건으로 연결한다. 총매장량 없이도 상품 생산의 출처 검증을 할 수 있다.
- 재생자원(산림·수산자원 등)은 현재 생물량·생장·채취·자연손실을 RenewablePool의 실제 동적 상태로 표현할 수 있다. 이것은 지하의 총매장량과 다른 현재 생태계 재고다. 생장/수확은 검증된 사건으로 반영하고 음수 방지.
- 따라서 게임은 모든 석유의 '최종 고갈 연도'를 미리 결정하지 않는다. 현장별 수익성·유량·운영 중단/재발견이 생산 경로를 결정한다. 단순 무한 무료 생산도 허용하지 않으며 노동·설비·현장 생산성·운영비·환경/법률 제약을 필수로 적용한다.

## J0-4. 자연환경·오염·재해의 영향 규약

1. 정적 기후대/지형/지질 적합도는 검증 가능한 참조 자료; 시간가변 토양·수분·삼림·산림관리·가뭄/홍수는 J의 환경 상태다. 온도·강수 등 입력 자료가 부족하면 출처 표기된 규칙 기반 추정치를 사용하고 실제 관측과 구별한다.
2. 농업 생산의 기후·토양 제약은 J가 제공하지만 농업 생산·가격·식량재고는 C/D의 실제 생산·시장 결과다. 건설 기상 제약과 시설 손상은 D가 집행한다.
3. 오염은 C/D 생산·연료사용/운송/폐기·실제 배출 사건으로 생성한다. J는 오염 종류·지역·농도 또는 질량(단위 지정)·이동/감쇠/정화 이력을 유지한다. 환산이 불가능한 다른 물리 단위의 양을 섞어 합산하지 않는다.
4. 수질/대기 오염의 확산은 E 인접/수계·지형/기상 영향이 허용하는 범위에서 근사한다. 전 지구 유체역학 전산 모델은 기본 범위가 아니다. 환경정화는 실제 자금·노동·시설·재료가 투입되어야만 환경 상태를 개선한다.
5. 재해는 (hazard_event_id, kind, affected_provinces/sea_zones, onset_phase, severity, exposure, cause/provenance, seed_stream, source_event_id)로 정의한다. 동일 발생 사건이 여러 프로빈스에 미쳐도 하나의 전역 사건 ID 아래 지역별 피해만 계산한다.
6. 재해의 **당월 조업 제약**은 stage0에 공개/동결된 유효 월간 환경·재해 조건을 stage1~4에서 사용한다. stage10의 실제 시설 파손/인구·건강 손실은 원 소유자 사건으로 확정하고 생산·임금·시장 등 이미 지난 단계를 소급 변경하지 않는다. 예상 외 말기 재해는 stage10 기록 후 차월 운영 제약으로 반영한다.
7. 재해 피해는 소유자 자산·재고·시설·인구·건강에 분할 기록하며 같은 사상자·손실·시설을 J·D·C가 중복 차감하지 않는다. 재건/보험/구호금은 실제 계약·정부예산·금융원장에서만 발생한다.
8. 수질·오염 노출에 대한 건강 변화는 POP 보건 모델의 위험 사건으로 전달한다. 오염지수에 따라 '전국 사망자 +X'를 직접 만드는 별도 인구 원장은 금지한다.

## J0-5. 월별 12단계 내 실행

| Stage | J가 읽거나 산출하는 것 |
|---|---|
| 0 opening_snapshot | 전월 확정 환경상태/지질 탐사·기존 현장/해역/관할·법률·기술/시설과 월간 계절/재해 운영 제약 확정 |
| 1 planning_and_credit | 탐사·채취·복구/방재·환경정화 계획, 실제 재원·권리/재해 위험 확인 |
| 2 labor_clearance | 탐사/광업/벌채/환경정화 노동 수요, 자격·현장 접근·타 산업과 인력 중복 제한 |
| 3 intermediate_goods | 시추·광업·정화/시설보수에 이미 도착한 실제 투입재/에너지·설비 사용 가능량 확인 |
| 4 production | 실제 현장 생산성/환경·노동·기계/레시피 제약하에 채취·농업·산업 생산, ResourceExtraction/EnvironmentalImpactEvent 및 C 재고 산출 |
| 5 wages_and_transfers | 채굴/정화·구호 등 실제 임금·이전지출·세금은 C 금융·재정 원장 |
| 6 pop_consumption_plans | 실제 가격·소득·상품공급·필요에 따른 가구 소비; 환경 지수로 화폐 원본 직접 변경 금지 |
| 7 final_goods_market | 방재/정화/시설재건 관련 실제 조달은 기존 시장·재고, stage4 자원투입에 소급 불가 |
| 8 closing_accounts | 자원 사업/정화·보험·보상/세금 비용·권리/의무를 C 원장과 대조 |
| 9 saving_and_investment | 차월 탐사·시설/정화투자·환경규제·토지 이용 정책 예약; 새 법률은 기본 m+1 stage0 |
| 10 pop_demography_and_health | 재해 손상·인구 피해·오염노출·서식지 손실·자연 회복/재생 및 탐사/현장 상태 장기 변화의 사건 검증. 차월 운영에 반영 |
| 11 repricing_and_validation | 환경 상태/생물량/유량·탐사 이력/지역 피해·권리/원장/중복사건·불변조건 검증, ONE Commit |

stage0의 월간 조건은 재해 영향을 반영해 산업 생산·건설 계획에 사용 가능하나, stage10 신규 손상으로 stage4 생산을 재계산하지 않는다. 동일 월의 오염 발생을 이미 종료된 stage2 고용·stage4 생산에 소급 적용하지 않는다.

## J0-6. API·사건·오류 처리

- GetEnvironmentState(province_id, month, world_snapshot) → 기후/토양/수문/오염·정밀도/출처·유효성; 변경 불가능한 조회.
- GetResourceProspectivity(province_id, kind, exploration_state, snapshot) → 지역 유망도/지질 근거/불확실성; 총량·남은 매장량 반환 금지.
- EvaluateExtractionCapacity(site_id, technique, labor/assets, month, snapshot) → 현재 유효 월간 물량 상한/비용·설명. 지하 총량 차감식 없음.
- PlanExploration(area_id, survey_work, effective_month) → 탐사 계획, 증거/면적/비용/정치 권한 검증; 발견/매장량 자동 지급 금지.
- RecordResourceExtraction(site_id, actual_production_event, waste/impact_refs, stage4) → C/D 생산 사건의 동일 출처 확인 및 J 현장 운영 이력 변경.
- ResolveEnvironmentalConstraints(province, site/sector, month) → 농업/건설/보건에 대한 유효 제약과 source_refs.
- RealizeEnvironmentalImpact(source_event, affected_region, pollutant_kind, amount, unit, stage) → 배출/이동/정화 원인 검증. 중복배출 금지.
- ResolveHazardEffects(hazard_event_id, affected_regions, source_entities, stage10) → 생산/자산/인구/재고 원장에 제출할 피해 변경집합; J가 직접 금전/인구 원본 수정 금지.
- ExplainResourceChange(site, period, snapshot) → 기술/신규 발견/실제 생산·비용·현장 유량 변화의 인과 설명. '남은 양' 대신 가동 가능한 현장·월 생산능력·위험·정밀도 표시.

모든 외부 명령은 권한/정책/관할·월 효력 및 expected_revisions 검사 후 StagingWorld/JournalBatch로 전달한다. 단계·법률 충돌/음수 생물량/누락 프로빈스·시설/중복 사건/검산 불일치는 정상 입력 reject 또는 내부 hard error로 구분하고 실패 시 전체 월을 Abort한다.

## J0-7. 기존 자원 설계와 충돌하는 부분의 명시적 이관 (BREAKING CONTRACT)

D-D0의 '자연자원 매장/재생/채굴권' 소유 행 및 DD0-INV-08, D-I2 I2-2의 ResourceDeposit(estimated_quantity/confirmed_extractable_stock), known_stock_end 계산식 및 ResourceStockDelta를 이용한 광물 재고 차감 규약은 **J-R1에 한해 더 이상 유효하지 않다**. 채굴 상품 원장·출처 보존 자체는 유효하다.

- 옛 광물/석유 ResourceDeposit → 신규 J ResourceSite + ExplorationState + 운영/생산 이력. 옛 총매장량/잔량 컬럼은 신규 세이브 스키마에 포함하지 않으며 형식상 복사·숨겨진 변수로 유지하지 않는다.
- 옛 광물 ResourceStockDelta 기반 지하 총량 감소 → 실제 채굴된 상품(기존 C-E1/C-E2) + 현장 가동/생산성 변화(J) + 환경 영향(J)으로 대체.
- DD0-INV-08의 자연자원 생산 출처 요구는 유지하되, **광물/석유의 수량 상한을 무조건 '확인 매장량'에서 차감하여 검증하는 조건은 삭제**. 재생자원의 실제 생물량 보존과 상품/시설/금융 보존은 계속 적용.
- 기존 D-D0/D-I2 문서와 구현은 J 통합 전 ADR, 버전/마이그레이션 규칙과 회귀시험으로 갱신해야 한다. 현재 코드가 자동으로 이 계약을 준수한다고 주장하지 않는다.
- 이전 저장파일에 지하 총매장량 필드가 존재하는 경우 불확실성 없는 정답값으로 이식 금지. 명시적인 데이터 계약 버전 전환 또는 로드 거부가 기본이다.
- 전체 확정된 경제·산업 원장을 J가 일괄 대체하지 않는다. 수정 범위는 지하 자원 상태/출처/자원 생산능력 추정 계약에 한정한다.

ADR: J0-ADR-001 총매장량/잔량·숨겨진 유한 수량 금지; 002 탐사·유량·운영생산성·비용 기반 지질 자원; 003 재생 생물량과 지하 매장량 구별; 004 생산/화폐/인구 원장 단일 소유; 005 D-D0/D-I2 특정 조항 버전 마이그레이션 필수.

## J0-8. 저장·성능·불변식·인수조건

- Save header: phase_j_contract_version=phase-j-common/1.0, environmental/geology_dataset_ids+hashes, environment/exploration/resource_site revisions, ruleset/source_provenance, world_seed, RNG/numeric policy, C/D/E contracts, last_commit_id. **총매장량/남은 매장량 필드 없이** 저장/복구·replay가 가능해야 한다.
- 정적 프로빈스·기후/지질 마스크는 별도 버전 파일로 공유/지연 로드; 가변 오염/생태·탐사·현장은 희소 상태로 관리. 최대 30,000 프로빈스 부하 표적에서 변경 지역 + 실제 활동 현장 + 인접 환경 이동만 우선 갱신하고 전국 POP×프로빈스 전체 곱셈/복수 원장 전면 재계산을 금지.
- 같은 세계/시드/규칙/데이터/명령에서 재해·탐사 결과와 월말 원장 해시 재현; 무작위 streams는 entity/event별 분리. 10k·20k·30k Province 및 장기 1,200개월을 구현 후 실제 측정하고 PASS/FAIL 구분.
- J0-INV01 ProvinceId/지리 dataset/region 바인딩 불변, 환경 변화로 주권·형상 자동 변경 금지.
- J0-INV02 지하 자원의 전체/잔존/숨겨진 총매장량 어떤 상태·규칙·세이브에도 존재하지 않음.
- J0-INV03 원료 상품 생성은 유효 활동 현장+기술/노동/설비/레시피/실제 ProductionEvent에서만 이루어지고 생산량은 현장 월간 용량을 넘지 않음.
- J0-INV04 탐사 재시도·기발견 유전 중복 생성 금지, 발견은 금융·상품·사람 수 무상 생성 아님.
- J0-INV05 재생자원: 기초 현존 생물량 + 생장/이입 − 채취/손실 − 유출 = 말기 현존 생물량(음수 금지); 지하 광물에는 해당 수량보존식 적용 금지.
- J0-INV06 배출/정화/이동과 시설·인구 피해는 단일 source_event와 원본 소유자 원장에 매핑. 손실·사상·세금 중복 반영 금지.
- J0-INV07 법률/재원/사용권/월 효력 준수, stage10 새 피해로 stage4 결과 소급 수정 금지.
- J0-INV08 공적 탐사·산업·정화 재원은 실제 금융·고용·설비·물품 원장을 거치고 무근거 화폐/재고 생성 금지.
- J0-INV09 unknown ≠ 0, 역사 지질·환경 자료의 근거/정밀도와 셀 단위 가정 유지.
- J0-INV10 재해/탐사 결정론, 월간 원자적 rollback, 저장/복구·문서/스키마 버전 호환.
- J0-INV11 물리 오염·자원·수계의 단위 일치 및 불가능한 음수/overflow 거부.
- J0-INV12 프로빈스 30,000개에서 실제 계측 전 성능 합격 주장 금지.

최소 J-D0 게이트: JD0-T01 무총량 석유 유정 채굴/시간 경과; T02 신기술/가격으로 휴면 유정 재가동; T03 반복 탐사 기발견 중복 방지; T04 산림 수확/생장/이월 보존; T05 산업 배출→환경상태→차월 보건 영향; T06 홍수/가뭄의 농업·시설 경로; T07 재해 인구/시설 손실 단 한 번; T08 허가/환경세와 실제 금융; T09 전체 rollback/save-reload/replay에서 총량 필드 없음; T10 30k Province·100만 POP/1,200개월 성능과 교차회귀. 아직 실행되지 않은 구현 게이트다.

## J0-9. 후속 구현 순서

J-N1 자연환경 → J-R1 자원 탐사/현장 생산성 → J-P1 오염·복구 → J-D1 재해 → J-V 통합 검증. J-R1 구현 전에 D-D0/D-I2 구자원 수량 계약 충돌을 ADR·코드·fixture로 해소해야 하며, J 전체 '구현 완료' 판정 시 타 Phase 회귀를 재실행한다.

**완료 정의:** J-D0 설계 계약 확정. 기존 D 구계약과의 구체 충돌·이관 요구사항까지 문서화했으며 C++ 구현/검증이나 실제 총매장량 데이터 제거 수행을 뜻하지 않는다.
