# J-R1 — 통합 자원: 탐사·발견·월별 생산능력·재생 상세 설계

> phase-j-resource-flow/1.0 · SPECIFIED / PARTIAL C++ RUNTIME MIGRATED · 상위: J-D0, C-E1/C-E2, D-D0/D-I2, D-T4, E-D0
>
> **강행 불변식:** 지하/비재생 광물·석유·가스의 총매장량·미채취 잔량·추정 총부존량·확인 회수가능 누적 총량·숨겨진 난수 총량은 게임 데이터/엔진/세이브·규칙에 **전혀 존재하지 않는다**. 위 값이 0인지 양수인지도 고갈 조건으로 검사하지 않는다.

## R1-1. 자원 통합 모델
자원은 독립 '재생/비재생 경제'로 이중 분리하지 않고 한 ResourceSite/탐사·채취 인터페이스로 취급한다. 단, 살아 있는 산림·어족·초지 등은 **현재 존재하는 생물량**을 계측하므로 RenewablePool 보존식을 따르고, 석유·광물은 잔존 지하 매장량 보존식을 만들지 않는다.

| 데이터 | 정본·최소 필드 | 의미 |
|---|---|---|
| GeologicalProspectivity | province_id/feature_id/resource_kind, geology_signatures, prospectivity_band, coverage_confidence, dataset_hash | 역사적/지질 근거와 조사 유망도. 매장량 수치 아님 |
| ExplorationState | area_id, surveyed_extent/coverage, techniques_used, access/permit, prior_survey_event_ids, exhausted_survey_opportunities, revision | 실제 탐사 범위·비용·증거. 유전 숫자나 자원량 확정 아님 |
| ResourceSite | site_id, province_id/region_binding, good, facility/rights refs, quality/access/depth, discovered/operational status, current_monthly_flow_capacity, unit_operating_cost_assumptions, history, decline_params, revision | 활동 중 현장의 **현재 월별 생산능력**. '몇 배럴 남았나'를 저장하지 않음 |
| ResourceProductionHistory | site_id, month, actual_output, active_time, facility/technique refs, event_id | 실제 이미 채굴한 양·활동 이력. 누적 생산량은 과거 실적이지 잔량/매장량이 아님 |
| RenewablePool | site_id, standing_biomass, growth_rule, ecological_carrying_capacity, actual_harvest/loss/migration_event_refs | 살아 있는 생물량의 현재 상태. 총 지질 매장량이 아님 |
| ResourceDiscoveryEvent | surveyed_area, evidence, unique_feature_signature, new_site_id, effective_month, source/seed, costs/effort | 조사→현장 발견. 상품·현금·총량 지급 금지 |

기존 C++ 제한적 수직 슬라이스에서의 구현형은 `phased::ResourceSite`: `monthly_capacity`, `produced_this_month`, `cumulative_output`, `decline_ppm_active_month`, `decline_remainder`, 재생 자원 전용 `standing_biomass`/성장·수용력, 권리/시설 ID 참조. 이 구조는 총량 필드가 없다. 전체 지질 GIS 탐사 객체/가격/기술 전파는 후속 구현이며 이번에 모두 구현됐다고 주장하지 않는다.

## R1-2. 생산·감소·경제성
1. 현재 월 생산 가능량은 `min(현장 월 유량−당월 생산 실적, 인력 가능 물량, 시설 서비스 가능량, 에너지/물류/투입재 한도, 안전/허가/환경제약, 필요 시 현존 재생 생물량)`. 지하 잔량에 기반한 상한식은 금지.
2. 실효 생산성은 작업기간·실제 과거 생산 이력·생산 방식/설비상태·기술 채택·지질 위험/비용 등에 따라 바뀐다. 감소가 반드시 모든 현장에서 동일한 단조함수가 될 필요는 없다. 수량 고갈을 유도하기 위해 '숨긴 초기 총량'을 분모로 쓰면 위반이다.
3. C++ 단순형: 활성 채취했던 현장에 한해 `next_month_capacity = capacity * (1 - decline_ppm_active_month / 1e6)`을 정수·잔여율 누적으로 갱신. 이는 **현장 가동 유량 감소 근사**이며 지하 매장량 차감식이 아니다. `decline_ppm=0`은 일정 기간 안정적 유량을 허용하므로 기술·운영·시장 비용을 누락하면 영구 저비용 채굴이 가능하다는 현실성 한계가 있다. 상세 J-R1 단계에서 기술·원가/압력·노후 모델과 보정해야 한다.
4. 신규 채굴과 재가동에는 개발 허가·소유/권리·실제 설비·노동·에너지/자금·기술을 요구한다. 기술발전은 실제 현장별 회수 효율·추진/시추 장비·월 유량/원가 제약을 개선하지만 이미 종료된 생산을 소급 증식하지 않는다.
5. 가격이 올라도 지질 총량이 늘어나는 게 아니라 수익성 있는 프로젝트와 현장 운영 여부가 바뀐다. 생산재/가격·순이익은 C-E2/C-E3의 기존 원장.
6. 채굴된 석유/광석은 C-E1 ProductionEvent + InventoryLedger의 유효 source_event/사업장/상품/물량을 갖고 판매/운송/저장. 지하 원본 없는 상품 생성·무상 대출/허가 없이 생산은 거부.
7. 재생자원의 실제 월말 생물량 = 전월 현존 생물량 + 관측/모형 생장·이입 − 수확 − 자연손실 − 유출. 생물량을 초과한 수확 금지. 생태계 수용량은 '지하 광물 총량'과 무관한 현재 동태적 한계다.

## R1-3. 탐사·발견과 무한 발견 방지
- 탐사 가능성은 유망한 **지질 특징의 위치·탐사 범위·증거의 누적**으로 표현. 무한한 '탐사 버튼' 확률 롤이 아니다. 실측 조사 인력·기간·시추장비·자금/권리·교통·안전 조건을 청산한다.
- 동일 특징/좌표·조사면적/층준·법적 source_event를 정규화하여 중복 발견 방지. 이미 충분히 조사한 구역은 증거가 실질적으로 추가되지 않으면 신규 발견 기회가 저하된다.
- 월별 탐사 성과는 결정론적 시드/지질 유망도·조사 실제 투입량/기술을 사용한 **정보 획득 사건**으로만 생성된다. 신규 광산 현장은 후속 건설·인허가를 거쳐야 생산 가능.
- 탐사 가능 지질구역이 제한되어 있더라도 '숨겨진 전체 광물 양'이나 가짜 유한 총매장량·게임 종료 고갈 연도를 미리 생성하지 않는다.
- 특정 국가·프로빈스에서 유전/광산의 역사적 위치·발견 시기가 자료로 확인되면 그 지질/탐사 결과를 초기 시나리오 데이터로 반영하되, 불명확한 숫자를 임의의 총량으로 기입하지 않는다.

## R1-4. 데이터 마이그레이션과 소유권
**기존 `ResourceDeposit.remaining`, `confirmed_extractable_stock`, `estimated_quantity`, `known_stock_end = known_stock_open + discovery - extraction` 규약을 지하자원에 사용할 수 없다.** Phase D의 미채굴 매장량/채굴권 표현을 'J ResourceSite·ExplorationState/ResourceUseRight 참조'로 대체하고, 상품/화폐·시설·노동은 C/D 정본을 유지한다.

- 런타임: `ResourceDeposit`과 `seed_resource_deposit` 제거 → `ResourceSite`/`seed_resource_site`; 예전 `remaining` 지하 차감 대신 실제 월 유량/활동량 검사. 재생 자원만 `standing_biomass` 차감/재성장.
- 세이브: Phase D 내포 스냅샷을 `VIC25_PHASED_SNAPSHOT_6`/`phase-d-cpp/0.6`으로 갱신, `phase-j-resource-flow/1.0` 호환 계약 추가. 구 `_5`/0.5는 **의도적으로 로드 거부**, 추측 총량 마이그레이션 없음. E/G/H의 중첩 Phase D 저장도 새 버전을 따른다.
- 설계 정본: [자원 모델 교체·검증](RESOURCE_MODEL_REPLACEMENT.md)을 참고. J-D0의 금지규칙이 D-D0·D-I2의 이전 매장량 수식보다 우선한다.
- Phase D의 '재생·채취 실물 근거/산출 보존'은 유지하되, 지하 물량을 물리적 실물 보존 원장의 유일한 출처로 삼는 것은 금지한다. 지하 유전의 실제 최종 채굴가능 총량을 정확히 보존한다는 주장은 하지 않는다.

## R1-5. 월별 12단계 연결
stage0 유효 현장·사업장·월 유량/계절·이전월 활동 감소·재생 생물량 조건 고정. stage1 탐사/개발·운영 재원 및 권리. stage2 채굴 인력, stage3 에너지/장비 서비스, stage4 실제 채취·상품 생산과 당월 유량 차감/활동실적. stage5 임금, stage7 판매/운송, stage8 결산, stage9 다음달 기술/투자/허가, stage10 새 탐사/현장·환경·재생·피해 전이, stage11 전원장 보존/원자 커밋. stage4 후 신규 발견은 **차월 이후** 실제 개발 전까지 즉시 생산할 수 없다.

## R1-6. 공개 인터페이스
GetResourceProspectivity(province, kind, snapshot) → data_quality, likelihood, geology; Survey(area, actual_work/technology/right, stage) → evidence; GetOperationalExtractionCapacity(site, snapshot) → current flow and constraints; RecordExtraction(site, actual_production_event, stage4) → 역사적 생산량·자원 현장 상태; GetRenewableBiomass(site, snapshot); ExplainSiteOutput(site, period) → 생산성/기술/원가·이력. 모든 조회는 **잔량/총량 비포함**.

## R1-7. 필수 수락 (J-R1-T01~T14)
T01 석유 현장에 총매장량 필드 없음; T02 월 유량 초과 생산 rollback; T03 누적 채굴량이 과거 임의 총량(예: 5)을 초과해도 유효 월별 유량이면 생산 가능; T04 실제 가동시 유량감소·휴면시 감소 규약; T05 기술/설비 투자로 차월 유량 증가 가능(후속 구현); T06 가격 상승시 경제성 개선·상품 무상생성 없음(후속); T07 산림 생물량 성장/수확 수지; T08 다른 시설/고용자의 자원 불법 채굴 거부; T09 실제 재고만 거래·운송 가능; T10 동일 지역·지질 특징 중복 발견 거부(후속); T11 동일 난수·사건의 탐사 재현(후속); T12 v0.5 세이브 로드 거부/신 v0.6 라운드트립; T13 100만 POP/30k Province 장기 부하(후속); T14 Phase C/D/E/G/H 회귀. 각 항목별 실행 완료 여부는 별도 테스트 매니페스트에서 구분.

**상태:** 구 `ResourceDeposit` 기반의 Phase D 간이 C++ 경로는 월 유량형 모델로 교체됨. **전체 J-R1 탐사/GIS/경제성/기술/생태 통합 엔진은 아직 미구현**이다.
