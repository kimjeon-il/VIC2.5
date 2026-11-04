# VIC2.5 D-I2 — 자연자원·에너지·중간재·생산 레시피

> **2026-11-04 자원 모델 이관본:** 과거 D-I2의 광상 `ResourceDeposit`/확인매장량/잔존량 차감 규칙을 폐기하고 [J-R1](../../phase-j/J-R1_RESOURCE_EXPLORATION_FLOW.md)의 유량·탐사·재생 생물량 계약으로 교체. 과거 [D 산업 상세 설계](https://github.com/kimjeon-il/VIC2.5/tree/work/phase-d-design/docs/phase-d/industry)와 다른 항목은 별도 통합 전 상태.


> **설계 확정 / C++ 구현·시험 미실행** · `phase-d-industry/1.0` · 2026-11-04  
> 상위: [D-D0](../D-D0_COMMON_CONTRACT.md), [C-E1](../../phase-c/economy/C-E1_FIRMS_LABOR_PRODUCTION.md), [C-E2](../../phase-c/economy/C-E2_GOODS_MARKETS_PRICES.md), [C-E5](../../phase-c/economy/C-E5_TRANSPORT_INTERNATIONAL_TRADE.md). [산업 색인](README.md)

## I2-1. 원본 구분

1. **미채취 자연자원:** J-R1의 탐사된 지질 특성과 유효 `ResourceSite`별 월 생산능력·품위/접근비용, 재생 자원만의 `RenewablePool` 현재 생물량·성장, 토지/기후 제약. 지하 광물·석유 총매장량·잔량은 일절 저장하지 않는다. 실제 자원 권리는 C-P·Ownership; 미채취 자원은 `InventoryLedger` 상품재고가 아니다.
2. **채취·생산된 물품:** C-E1 `ProductionEvent`의 실제 산출 후 `InventoryLedger`의 `GoodId/Quantity`, 원료·완제품·폐기물의 보유주체/로트·조건을 가진다.
3. **물질 전환:** 기존 `ProductionRecipe`는 입력(재료·에너지·기계/노동 서비스), 주산출·결합산출·부산물/회수재, 폐기·손실·품질·배출/제약의 **버전 있는 기술 함수**다. `IndustryCatalog`는 이를 분류만 한다.
4. **에너지:** 저장 가능한 연료(석탄·목재·석유·가스·축전품 등)는 `GoodId/InventoryLedger`; 전력·열처럼 비저장 서비스는 실제 `ServiceDelivery`·생산/소비 슬롯으로 표현. 단순 국가 전력 수치/무상 전국 전기를 만들지 않는다.

## I2-2. 자원 단위·이력

| 객체/관계 | 필드(개념)·불변식 |
|---|---|
| `ResourceSite` (J-R1) | `site_id, region/province_binding, resource_kind/good, effective_facility, geology_quality/access_cost, current_monthly_flow_capacity, produced_this_month, cumulative_output, activity_decline_params, rights, provenance, revision`. **총·잔존량 필드 없음** |
| `RenewablePool` (J-R1) | `site_id, land_or_water_ref, standing_biomass, regeneration_rule, season, climate/soil_index, ecological_carrying_capacity, rights, historical_metadata` — 현존 생물량의 실제 수지 |
| `ResourceExtractionEvent` | `site_id, production_event_id, actual_output_lots[], waste_lots[], labor/site_service_refs, cumulative_output_observation, source_event_id`. 실제 월 유량·노동/시설 검증, 지하 매장량 차감 아님 |
| `ResourceUseRight` | 기존 C-P/Ownership 권리의 참조, 사용량·제한/시점; 권리를 자원량으로 착각하지 않는다 |
| `EnvironmentalImpactEvent` | `source_production_event, region, emissions/waste/soil_depletion/water_use, legal_limit_refs, restoration_obligation_ref`. 오염/소모는 실제 생산과 연결 |
| `InventoryLot` | C-E2 InventoryLedger의 `good_id, holder/custodian, location, quantity, condition/grade, source_event_id`; 재고 버전 재정의 금지 |

- **광물/석유/가스/석탄의 총매장량·확인매장량·추정회수가능 총량·숨겨진 총량·잔존량은 게임 스키마에서 제외.** 지질 유망도·탐사 범위/증거·활동 중인 현장과 현장별 월 유량/원가는 별개. 새 유전 발견은 `ResourceDiscoveryEvent`의 조사출처/범위·중복 탐사 방지로 기록하며 실제 산출과 구별한다.
- 비재생성 자원: `actual_output_this_month <= current_monthly_flow_capacity - previously_produced_this_month`. 다음 달 실효 생산능력은 가동 이력·설비/기술·채굴비용/유량 감소와 현장 개보수로 변동한다. **지하 재고 감소식 금지**. 누적 생산량은 이미 채굴한 역사적 실적에 불과하다.
- 재생성 자원: `standing_biomass_end = clamp(standing_biomass_open + biological_growth + verified_inflow - actual_harvest - natural_losses - verified_outflow, 0, ecological_carrying_capacity)`. 현재 산림/어족의 생물량과 지하 총매장량을 혼동하지 않는다; 생장·수확은 별도의 실제 사건.
- 농업은 토지·종자·비료/관개·노동과 재배기간/수확계절을 필요로 한다. 농지 면적이 산출을 직접 '무료 생산'하지 않는다. 수확은 `HarvestYield` 사건이 필요하며 자가소비도 `InventoryLedger` 현물이동.
- 축산은 실제 사료/개체 자산, 번식/사망·도축·유제품 부산물을 구분; 번식/성장 규칙은 후속 I2 역사 계수로 저장. 땅·어족·수계/광석의 소유/주권·채굴허가/전쟁 봉쇄와 물리적 이용가능성은 별개.
- 원료/자원 채취의 재활용은 이전에 물리적으로 존재하던 폐자재/스크랩의 합법적인 인수·정제 비용을 요구하고 무한 순환율 100%를 가정하지 않는다.

## I2-3. 산업 공정 레시피

공정별 설계 구조:

~~~text
ProductionRecipe {
  recipe_id, ruleset_version, industry_activity_tag,
  batch_unit, minimum_batch_lot, nominal_cycle_duration,
  material_inputs[good_id -> quantity_per_batch],
  allowed_substitute_groups[goods, ratios, constraints],
  energy_inputs[good_or_service_id -> amount_per_batch],
  capital_service_requirements[asset_type -> service_unit_per_batch],
  labor_requirements[occupation/qualification -> work_unit_per_batch],
  land_water_resource_requirements[],
  main_outputs[good_id -> quantity_per_batch],
  co_outputs[good_id -> quantity_per_batch],
  recovered_waste_outputs[good_id -> quantity_per_batch],
  nonmarket_waste_and_emissions[],
  quality_rule, permitted_site_types, legal_limit_refs,
  source_provenance, revision
}
~~~

- 각 `batch_unit`와 자원/상품의 최소 로트·스케일은 명시적으로 고정. 단위 미정, 재료 0으로 나눔, overflow, 자원출처 없음 → hard error.
- 복수 대체 원료가 가능하면 `RecipeVariant` 또는 버전 있는 대체 매트릭스에서 **검증 가능한 물질·성능·품질 변화**로 승인. 단순히 값싼 원료 1로 다른 원료 1을 교체하는 것을 무조건 허용하지 않는다.
- 한 배치의 주산출·결합산출은 실제 동일 `ProductionEvent`에 함께 기록. 부산물·폐기/배출 물량도 사건에 대응해야 한다. 판매 불가능 부산물은 재고가 아니라 실제 폐기/비시장 산출로 남는다.
- 물리적 물질/에너지 수지는 레시피의 보존 규칙(질량·원소/반응·손실 또는 인증된 부분 산출과 외생 자연 채취)으로 대조. **다른 물리 단위의 숫자 합을 단순 비교해서 잘못된 보존 실패를 만들지 않는다.**
- 가공/정제에 따른 품질등급·수율/손상·보관 노후·중간재 재활용은 고정소수점·이력 있는 `YieldRule/QualityRule`의 실제 입력/출력 이벤트로 기록.
- 서비스(운송/교육/진료·연구 등)는 실제 제공 기간·인력/시설·서비스 용량을 기록하고, 무한 보관/차월 자동 이월되는 물품으로 다루지 않는다.

## I2-4. 당월 계획과 실제 생산량

- `stage1`: C-E1 `FirmPlan`이 예상 수요·재고목표·기존 월초 가격/임금·실효 시장 주문·자금조달/권한을 근거로 생산·구매/운송 계획을 제안한다. 생산 가능량 계획과 실제 제조 결과를 분리.
- `stage2`: 특정 활동/공정별 실제 근로 적격·C-E1 계약/노무 서비스/중복 예약을 확정한다.
- `stage3`: 확정된 실제 원재료·저장 가능한 에너지 연료·중간 서비스의 인도/결제, 소비용 재고 예약을 확정한다. 미도착/당월 stage4에서 나올 상품은 stage3에 사용 불가.
- `stage4`: 생산 예정 배치량을 다음 제약의 최소로 줄인다.

~~~text
feasible_batches = min(
  planned_batches,
  physical_asset_batch_capacity,
  labor_batch_capacity_by_required_occupation,
  min_materials(delivered_and_reservable_goods / input_per_batch),
  min_fuels(delivered_and_reservable_energy_goods / input_per_batch),
  extraction_or_land_rights_and_access_limit,
  water_and_site_capacity_limit,
  legal_environmental_safety_limit
)
actual_batches = round_down_to_recipe_lot(feasible_batches)
~~~

- 실제 배치로 `ProductionEvent + InventoryGoodsConsumed + GoodsCreated + losses + ResourceSiteFlowObservation + (재생자원일 때만 RenewableBiomassDelta) + capacity_services_consumed`를 동일 `JournalBatch`에 기록; **지하 매장량 차감 필드 금지**. 종사자 임금은 stage5, 매출/원가 결산 stage8과 대조한다.
- `stage7`: 완성제품은 실제 판매·인도/출하되고 비축/미판매는 재고로 남는다. 주문·계약만으로 상품 인도/지급·판매가 발생하지 않음.
- 어떤 회사도 계획된 원료·자금·시장 수요만으로 물리 산출을 무조건 달성하지 못한다. 상한 부족은 정상적인 기업 병목·경기충격이며 추적할 `SupplyConstraintTrace` 출력.

## I2-5. 에너지 특례 및 C-E1 시점 충돌 해결

**핵심 문제:** C-D4/CE는 생산 투입 중간재가 stage3에 실제 인도되어야 stage4 생산 가능하다고 확정한다. 그러나 전력처럼 비저장 서비스는 발전과 소비가 같은 달/단계에 연속 발생할 수 있다. 이를 문서만으로 임의 허용하면 기존 CE-ADR-006·단계3~4의 계약과 충돌한다.

따라서 다음 두 경로를 분리한다.

1. **I2-SAFE 기본(상위 계약 그대로):** 석탄·목재·석유 등 **재고 가능한 연료/원료**와 stage3 이전에 실제 공급된 검증 서비스만 생산투입으로 허용한다. 공급되지 않은 당월 발전 예정 전력은 `planned_energy`이며 stage4 공정에 투입 금지. 전력 생산·송배전의 용량/자금 계획은 작성할 수 있으나 **실시간 비저장 전력 급전이 필요한 산업공정은 미지원/명시 거부**. 거짓으로 '전기를 저장 가능한 상품'이라 간주하지 않는다.
2. **I2-DISPATCH 후속 선택(활성 전 선행 ADR 필수):** CE-ADR 변경/버전 갱신을 통해 `NonStorableServiceReservation`(stage1/3 예약) → stage4 발전 실현·계통 실제 송전 제약 → 동일 stage4의 **결정론적 원자적 의존 DAG**로 전력 소비 공장 가동을 다룬다. 출력/소비/손실/요금·부족분을 실제 service ledger로 확정하고, 서비스 생산이 실패하면 전체 관련 연쇄를 축소/거부한다. 이 모드는 **C-E1/C-D4/Phase D의 선행 마이그레이션 및 회귀 전까지 사용 불가**이며 임의로 현재 계약에 추가하지 않는다.

이 구분은 계통 전기·증기/열·상하수·비저장 통신/운송 서비스에 일반화 가능하다. 월별 세부 급전과 전국 송배전망을 자연스럽게 처리하되 **13번째 월 단계**를 만들지 않는다.

## I2-6. 환경·외부효과와 충격

- 산업생산은 대기/수질오염·토양 및 산림/어족의 고갈, 강수/가뭄·질병, 산업재해·전쟁/봉쇄의 실제 사건과 연결될 수 있다. 환경비용/건강 결과는 C-P 허가/규제·C-E4 행정/보건·POP 건강 전이에 **명시적으로** 연결한다.
- 배출·폐기 자체가 즉시 정부의 세수나 회사의 현금 비용이 되지 않는다. 환경세·배출권/벌금은 **유효 법률 + Fiscal/Financial** 의무/지급과 연결한다.
- 채굴은 현장 월 유량·실제 설비/인력/권리 상한을 초과할 수 없으며 벌채·어획은 실제 현존 생물량을 초과할 수 없다. 봉쇄·운송 중 재료 손실은 Inventory/Shipment 사건으로 처리. 실제 원인 없는 재화 창출·재고 음수는 엔진 오류.

**인수시험:** `DI-T10..T18`. [검증 매트릭스](INDUSTRY_ACCEPTANCE_TESTS.md).
