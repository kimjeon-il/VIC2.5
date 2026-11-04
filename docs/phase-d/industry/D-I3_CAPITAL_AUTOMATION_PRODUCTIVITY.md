# VIC2.5 D-I3 — 생산설비·기계화·생산방식·숙련·생산성

> **설계 확정 / C++ 구현·회귀시험 미실행** · `phase-d-industry/1.0` · 2026-11-04  
> 상위: [D-D0](../D-D0_COMMON_CONTRACT.md), [D-C5](../construction/D-C5_COMMISSION_MAINTENANCE_RETIREMENT.md), C-E1(설비/고용/생산), C-E3(금융·투자), POP-D0~D3, 차후 D-T1~T5. [색인](README.md)

## I3-1. 생산방식의 정의

VIC2.5 **생산방식(ProductionConfiguration)**은 사업장/활동/월 기준 **호환 가능한 레시피의 구성·비율, 가동 가능 CapitalAsset·노동자/숙련·투입재·법적 권한·기술 사용권**에 의해 유효성이 결정되는 실제 운영 상태다. 단순한 ‘공장 기술 레벨’이나 국가 전체 성장 보너스가 아니다.

- `SiteActivityAssignment`(I1)에서 `RecipeId`별 유효 활동을 참조한다. 여러 레시피/라인을 병렬 운영할 때는 `ActivityLineAssignment`(relationship only)가 `site_id, activity_id, recipe_ref, allowed_capital_asset_refs, effective_period, production_plan_ref`를 가진다.
- 공정의 실효 기술은 D-D0 `SiteTechniqueAssignment`와 연구/지식·설비·숙련/면허·환경 법령에서 읽는다. 기술을 발견했어도 같은 공장 전환·훈련/개조 비용이 없으면 사용 불가.
- 기계·소유구조를 분리한다. `ProductionConfiguration` 선택은 `OwnershipLedger` 지분·노동자의 법적 지위/노동계약을 자동으로 변경하지 않는다. 특정 시기에는 회사 소유지분이 기술과 무관하게 국가/영주/협동조합에 속할 수 있다.
- 모든 공정은 자원/노동/설비/법률 병목(C-E1/E2/D-I2)을 평가한다. 동일 사업장의 내부 설비·장비 시간을 서로 다른 공정에 이중 계상하지 않는다.
- 가장 발전된 방식이 반드시 이윤 최대/생산량 최대가 아니다. 원료/부품·에너지 가격, 숙련 노동 확보, 자본비용·품질, 시장수요·규제에 따라 낡은 공법이 존속할 수 있다.

## I3-2. 생산설비와 가동 능력

| 속성 | 원본과 의미 |
|---|---|
| `CapitalAsset.rated_capacity` | C-E1의 **공칭 생산 서비스 능력**. 월/공정별 호환성·규모/개체식별 및 설비 상태 |
| `CapitalAsset.condition` | 물리적 마모·손상·유지보수 상태, D-C5에서 실제 수리/손실 |
| `CapitalAsset.installation_status` | `planned/under_construction`은 D-C 진행/WIP, 설치·검수·차월 가동 이후만 유효 설비 |
| `OwnershipLedger` | 법적 소유/임대·사용권·담보; 임차 설비는 사용 가능하나 시공사 소유로 자동 이전되지 않음 |
| `ServiceCapacityLedger` 또는 C-E1 asset-service capacity view | `AssetId × month × service_type`의 총량/예약/사용을 C-E1 원장에 귀속. D-I3은 읽기/예약 관계만 사용 |
| `AssetAvailabilityWindow` | D-C5 정비/고장/개축·법적 사용제한·운송/교체로 당월 실제 가용시간과 공정별 기능 결정 |

- 공칭 capacity와 실사용 throughput이 다르다. `usable_service = rated_service × condition_factor × availability_factor × legal_access_factor` (고정소수점·예약). 생산 실제 가능량은 레시피별 인력·설비·연료/자재/배출·현장 용량의 최소.
- 설비수 2가 의미하는 것은 실제 `CapitalAssetId`로 식별된 가동 가능 설비 2개 또는 그에 상당한 확장 용량. ‘building_level=2’만으로 자동 제조설비·채용/생산 증가 금지.
- 제철 고로/기계공장/전력시설 등 생산능력은 용량 단위가 다르다. 각 사업장별 서비스 단위(예: smelting_hours, lathe_hours)는 버전 있는 변환 정책 없이는 합산 불가.
- 부하↑는 설비 마모·고장확률/에너지 요구·노동 위험·품질에 영향을 줄 수 있으나 수치/역사 보정은 별도 Ruleset. 회계 감가상각과 물리적 condition은 다른 정본.

## I3-3. 다중 생산라인과 실제 생산공정 전환

- 같은 사업장에 A/B 제품을 만드는 라인이 있어도 **라인별 유효 사용권·필수 장비 서비스·재료·고용·부지/설비 용량**을 동시에 대조한다.
- 공정 혼합비는 `planned_batch_mix[recipe_id -> batches]`(계획)이며, stage4 확정 물량은 `actual_batches`(실행)다. 수요가 갑자기 바뀌어도 기계·노동을 한 달에 무제한 재도구화/전환할 수 없다.
- `ProcessConversionPlan`: `site_id, from_recipe_set, to_recipe_set, required_equipment_changes, training_requirements, shutdown_windows, commissioning_refs, legal/knowledge_refs, financing_ref, earliest_effective_month`. **계획·승인과 실제 사용은 분리**.
- 단순 공정 전환(기존 설비로 두 규칙 모두 실행 가능한 경우)은 필요한 재교육/재설정·기계 서비스와 실제 유효 월을 확인하고 다음 달 stage0부터 변경 가능; 공사·설비개량이 필요한 경우 D-C1~C5 `ConstructionProject`의 실제 재료·시공·비용·준공/가동 효력에 종속.
- 기존 노동자 해고/채용·재훈련은 `EmploymentLedger`·POP 교육/숙련 이벤트, 신규 기계는 `InventoryLedger` 상품구입→D-C 인도/설치→C-E1 `CapitalAsset` 활성/Ownership 연계. '새 기계화 방식 선택'만으로 자동 정리해고·이익/생산 보너스·주주교체 금지.
- 병렬 개축/기계 교체 중 실제 공장 가동가능량을 `AssetAvailabilityWindow`와 `SiteActivityAssignment`에 따라 제한. 모든 시설을 무조건 0 또는 100%로만 운전하지 않아도 된다(독립 라인 가동).

## I3-4. 노동과 기술/기계화의 상호작용

- 공정별 요구 직무: `LaborRequirement{occupation, qualification, service_units_per_batch, safety_eligibility}`. 실제 근로자는 기존 `PopulationSlice`/`EmploymentLedger`·POP 교육/건강/복무에서 공급.
- 숙련도·교육/기능적 적격성·현장 경험은 **구체적 업무 요건 및 보조설비/편의제공**과 함께 평가. 장애 자체를 무조건 실업/0생산성으로 환원하지 않는다.
- 기계화는 보통 특정 업무의 노동서비스 요구를 줄일 수 있지만, 기계 조작·정비 인력/에너지·부품·초기투자와 고급기술자가 필요할 수 있다. 따라서 기계화 효과는 `Recipe`의 입력/노동/설비 계수 변경(실제 공정)으로 표현.
- 장기간의 숙련 학습·직무 전환/실업·임금 변화는 POP의 교육/직업전이·노동시장/E1 임금제안이 실제 처리한다. 새로운 레시피 선택만으로 POP 전체의 학력을 즉시 올리거나 모든 실직 인원에게 임의 임금/자산을 지급하지 않는다.
- 강제노동·길드/장인·가내수공업·근대 공장제·자동화 공장이 동일 시기/지역에서 공존할 수 있다. 노동형태와 기술 숙련을 하나의 사회적 계급/업종 속성으로 합치지 않는다.
- **기술 준비도 단계** `known → adaptable → training/retrofit → verified_active`는 D-D0 기술 도입 관계와 호환한다. 기술수준의 국가 단위 ‘잠금 해제’만으로 공장 전환 불가.

## I3-5. 예상 경제성과 선택 논리

기업의 레시피·공정 선택은 단계1에 **예상 실제 확보가능한 조건**을 근거로 수행한다:

~~~text
expected_operating_margin_r
  = expected_delivered_output_revenue_r
  - expected_intermediate_input_cost_r
  - expected_payroll_and_safety_cost_r
  - expected_maintenance_energy_transport_tax_cost_r
  - expected_financing_and_conversion_cost_r
~~~

이 값은 계획 비교이지 즉시 발생한 이익/예금이 아니다. 신규 대출·기계 매입·공사비/자재·임금 예약이 실제 지급되고 영업한 뒤 단계8에 발생주의/현금주의로 대조한다.

- 기업은 제품시장/가격·미충족 실효 주문·공정별 잠재 매출/위험·가용 금융과 법적 제약에 따라 생산을 조절한다. 자기자본/은행/정부 지원이 없는 회사는 적자가 예상된다고 무조건 새 설비를 무료 증설하지 않는다.
- 급여/원료를 확보할 수 없을 때 고용/생산 축소, 정상 유휴설비 유지, 계약 변경·외주/파산 등이 가능한 모형 결과다. 생산량이 0이어도 고정비·이자·시설 마모·실업 등의 결과는 원장에 남을 수 있다.
- 원료 사용과 판매량을 동시에 확정해 무조건 균형을 만드는 자동 시장은 금지. 실제 잔여 생산품은 재고, 부족 주문은 미충족 수요, 상품 가격의 차월 조정은 기존 C-E2에서 수행.

## I3-6. 월별 사건·검증

| 단계 | 산업/기계화 |
|---:|---|
| `0` | 지난달부터 유효한 시설/공정 배정·기술 면허·설비 상태·법률/시장가격 freeze |
| `1` | 각 활동 계획/유효 레시피·채용/원료/장비 예약, 장기 자본투자/개축 자금 확인 |
| `2` | 기존 C-E1 노동시장에 따라 공정별 실제 적격 노동배정 |
| `3` | 확정 재료/연료·중간서비스 인도, 공정별 사용 예약 |
| `4` | 실제 레시피별 생산/부산물·스크랩/마모를 C-E1·Inventory에 기록 |
| `5` | 실제 임금/체불·원천징수·공적 이전 C-E3/E4 처리 |
| `7` | 생산품 판매·재고/인도·설비 구매와 향후 개축용 투자재 |
| `8` | 제조원가/매출·감가상각/이자/세금·기업/은행/정부 결산 |
| `9a` | 재투자·공정전환/신규 고용·훈련/공사 제안 예약(차월 효력) |
| `10` | POP 건강/복무·전쟁/재난 설비/공장/시설 피해 및 자산 정정 |
| `11` | 재고·자산·임금/원가/규칙·차월 가격/공정·시설 효력 검사 후 전 World 원자 커밋 |

**검증:** ① 설치되지 않은 기계/미확보 노동력 생산 증가 거부; ② 공정 두 개에 같은 설비서비스 중복 예약 거부; ③ 기계화 비용·훈련/개축의 지연과 차월 효력; ④ 실제 노동/원료·법률 병목; ⑤ 기존 C-E1의 생산·시장·가계/금융 정합과 결정론.

**상세 시험:** `DI-T19..T27`. [인수시험](INDUSTRY_ACCEPTANCE_TESTS.md).
