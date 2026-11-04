# VIC2.5 D-C3 — 공정·기간·실제 작업·품질·검수

> **설계 확정 / C++ 연산 미실행** · `phase-d-construction/1.0` · 2026-11-04  
> 상위: [D-D0](../D-D0_COMMON_CONTRACT.md), [D-C1](D-C1_PROJECTS_CONTRACTS_RIGHTS.md), [D-C2](D-C2_LABOR_MATERIALS_CAPACITY.md), C-E1~E5, C-D4 월별 12단계. [색인](README.md)

## C3-1. 세 가지 진행 개념

1. **작업 시도량(`performed_work`)**: 노동·설비·자재를 실제 소비해 실행한 공정 작업량. 실패한 시공도 물자·임금은 사용했을 수 있음.
2. **검수 통과량(`accepted_work`)**: 규칙에 따라 품질·내력·선행·법적 검사 요건을 통과한 순 유효 작업량. 전체 공정의 `done_work`는 이 값(손상 시 명시적 차감).
3. **자산 효력(`commissioned_capacity`)**: 최종검사/허가·설비 인수 및 차월 stage0 활성화가 완료된 실제 C-E1 `CapitalAsset`/C-E5 시설의 가용용량. 작업 진도 100%와 별개.

국가 전체 `construction_points`를 누적하거나 `ConstructionProject.done_work`를 새 상품 재고로 추가하지 않는다. 진척률은 계약된 공정 가중치/물량을 기준으로 한 **읽기 전용 통계**다.

## C3-2. WorkPackage와 공정 DAG

`WorkPackage` 필드 제안:

~~~text
WorkPackage {
  work_package_id, project_id, kind, technique_id, revision,
  work_unit_id, required_work, accepted_work,
  predecessor_package_ids[], successor_package_ids[],
  earliest_start_month, minimum_calendar_months, hold_months,
  recipe_inputs_per_unit[], workforce_services_per_unit[],
  machine_services_per_unit[], site_access_requirement,
  permit_refs[], quality_rule_ref, inspection_rule_ref,
  started_month?, last_accepted_month?, verified_ready_month?,
  damage_events[], source_event_refs[]
}
~~~

- `WorkUnitId`는 공정별 단위(굴착 m³, 토공량, 기초/벽체 면적, 일정 설비 설치수 등)이며 전 업종·지역의 공통 건설점수가 아니다. 공정 간 합산시 `weighted_completion_fraction` 조회를 별도 계산.
- 공정 선후관계는 DAG (무순환)이며 `PredecessorGate`가 완료·검수 또는 명시적 최소 진행조건을 요구. 다음 공정이 부분 선행률로 시작될 수 있으면 규칙파일에서 부분 허용 범위를 반드시 선언.
- 같은 프로젝트 내 여러 공정 동시 진행 가능(서로 다른 작업조/자재/장비가 있어야 함). 서로 충돌하는 작업은 `site_slot`/안전·동선으로 제한.
- 허용 공정 예시: 조사/측량 → 토지정리 → 기반/기초 → 구조/골조 → 외장/지붕 → 설비/기계 설치 → 시험·검사. 시대별 공법/시설종류에 따라 일부 단계 생략 또는 다른 DAG 사용.
- 공사계획 변경 시 새 `plan_revision`와 신규 공정 lineage, 이미 승인된 실적/대금의 과거 기록 유지. 순환 DAG·없는 선행공정/미상 단위는 hard error.

## C3-3. 월별 최대 수행량과 실제 투입

공정 p·월 m에 대해 **기술/규칙별 환산된 동일 `WorkQuantity` 단위**로 상한을 계산한다:

~~~text
q_feasible(p,m) = min(
    q_remaining_accepted,
    q_ordered_and_funded,
    q_labor_by_required_occupations,
    q_equipment_by_service_type,
    min_for_each_good(actual_delivered_good / input_per_unit),
    q_site_space_and_access,
    q_weather_and_season,
    q_legal_safety,
    q_calendar_and_predecessor
)
q_performed = nonnegative_rounded_lot(q_feasible, numeric_policy)
q_accepted  = q_performed - q_rejected_quality
~~~

- 투입 0이 필요한 재료·서비스는 `min` 구성에서 제외(0으로 나눔 금지). 공정별 최소 작업 로트 이하의 가용량은 실제 공사로 계상하지 않음.
- `q_labor...`은 C-E1 `EmploymentLedger`의 실제 적격자·중복 없는 `CrewAssignment`로부터, `q_equipment...`는 C-E1 설치/대여 장비의 실제 유효 서비스로부터, `actual_delivered_good`은 C-E2/E5 stage3 이후 실제 현장/수급자 보관 재고로부터 환산.
- 자재 구매 예약/발주/`in_transit`만으로 `q_performed` 증가 금지. stage7 신규 물품 인도는 이미 종료된 stage4 시공에 소급 투입 불가.
- **실제 근무/기계 사용·자재 소비(및 폐기)·시공 서비스·검수 결과·`WorkEvent`**는 동일 `JournalBatch`에 묶여야 하며 금융원장의 매출/기성청구와 단계8에 교차 참조 가능해야 한다.
- 연료·전력/용수·소모성 공구·안전·보관 용량·기상이변·통행금지 등도 특정 공법이 필요로 하는 경우 실제 자원/공공 인프라/법령 병목으로 처리. '건설 효율 120%' 같은 출처 없는 보너스는 금지.

## C3-4. 기간 제약

- 모든 공사에 무조건 동일한 최소 1개월을 강제하지 않는다. 공법별 `minimum_calendar_months`, `earliest_start_month`, `hold_months`(양생·안전검사 등)와 월별 달력/계절 규칙을 구분한다.
- 12단계 월 실행을 벗어난 매일 시뮬레이션을 추가하지 않는다. 월중 세부 순서가 필요한 공정은 승인된 `stage4` 하위 DAG 사건 순서로 기록하며 최소 경과월은 달력 게이트로 검사한다.
- 선행공정은 `m` 이전 확정실적 또는 **stage4 내부에서 먼저 확정된 실적**을 읽을 수 있다. 이 경우 작업조·설비·재고 예약을 두 번 사용할 수 없고 공정 DAG/자원 예약 순으로 실행한다.
- 작업이 1월에 시작해 3개월 이상 양생이 필요하다면 다른 공정이 선행 검사 조건을 만족해도 `hold_months` 이전 조기 가동 불가.
- 공사 완료 예정일·비용 예측은 `expected_end_month` 등 관측/계획값이지 미래 실적을 확정하는 상태가 아님. 지연되면 일정·계약 위약/금융비용을 갱신한다.
- 특이조건: 철도·관개·운하/도로·항만처럼 여러 구간으로 나뉘는 공사는 `WorkPackage`/독립 `AssetSpec` 단위의 인도/개통을 허용. 지리적으로 끊어진 간선을 연결됐다고 자동 가정하지 않는다.

## C3-5. 생산·검수의 결정론적 하위 단계

기존 공개 `MonthlyPipeline.stage4` 안에서 **로컬 전용** 정렬 규약:

- `4c-prepare`: 해당월 단계0 법/인구/기상·단계1 계약/예약·단계2 노동·단계3 재료를 바탕으로 작업 허용 및 예정 인수 검증.
- `4c-work`: 프로젝트·공정 `(region_id, project_id, topological_order, work_package_id)`에 따른 정렬, 작업량/자재·설비서비스/노무 점유/시공 산출 사건 확정. 동률/공유 자원은 C-D4의 안정적 정렬·예약 정책 적용.
- `4c-quality`: 실제 품질검사 인력·비용·서비스와 공법/안전 규칙에 따라 검수 가능량·하자를 기록. 통과하지 못한 시공의 이미 소비한 자재·임금은 사라지지 않음.
- `4c-finish`: `accepted_work`와 조건을 근거로 `physically_complete`/`inspection_passed` 후보 생성. stage11에서 stage10 피해와 원장·허가를 최종 대조하고 `ready_for_commission` 예약.
- `4c`는 기존 공개 월 단계가 아니다. 일반 공장 stage4의 산출을 construction stage4 이전 중간재처럼 허용하지 않는다. 다만 동일 stage4에서 순서대로 생산한 **건설 현장 내부 비저장 시공 서비스 및 선행 공정 실적**은 검증된 의존 이벤트가 있을 때만 후속 `WorkPackage`에서 참조 가능하다.

## C3-6. 품질·하자·재시공·사고

- 품질은 `recipe/technique`의 요구치, 실제 적격 숙련, 자재 상태/품질, 안전·감독 서비스 및 검수 규칙에 의해 결정. 확률이 있는 경우 `world_seed, project_id, work_package_id, month, rule_version`으로 결정론적 난수 생성 및 추적.
- `WorkInspectionEvent` 결과: `accepted_qty`, `rejected_qty`, `defect_type`, `rework_requirement`, `inspection_cost`. 실패·미검수 작업량을 `done_work`에 합산하지 않음.
- 재시공은 별도 `ReworkOrder`가 다음 실행 가능 공정에 필요한 추가 노동·자재/장비를 요구한다. 품질 실패로 이미 소비한 자재를 현장 창고에 자동 환원하면 안 됨.
- 사고·질병/사상은 기존 POP/Health 사건으로 단계10에 처리. stage10 노동자 피해는 이미 stage4 수행한 작업과 stage5 급여에 소급되지 않고 차월 노동공급에 반영.
- 진행중 시설의 재해·교전 피해: `ProjectDamageEvent`가 `accepted_work`를 **명시적** 감소시키고 손실/잔존자산·WIP손상을 Financial/Inventory/Ownership 및 보험청구로 대조. `accepted_work`는 항상 `0 <= accepted_work <= required_work`.
- `completion_percentage`는 `sum(weight_by_work_package × accepted_work/required_work) / sum(weight)`의 파생 관측치(재작업/손상으로 감소 가능). 가중치는 룰셋·계획 버전 고정이며 총공사비의 증가만으로 진척률이 증가하지 않음.

## C3-7. 공정 사건/불변조건

`ProjectWorkEvent` 최소 필드: `event_id, project_id, work_package_id, month, stage=4, work_unit_id, attempted_qty, accepted_qty, rejected_qty, input_inventory_journal_refs[], equipment_service_refs[], crew_assignment_refs[], quality_event_ref, revenue_or_wip_refs[], ruleset_hash, depends_on[], revision`.

보존/정합:
- `0 <= accepted_work <= required_work`, `attempted_qty >= accepted_qty >= 0`, 전월 확정량에서 손상/정정 사건 없는 부당 감소 거부.
- `accepted_qty + rejected_qty = attempted_qty`(정의된 검사 범위 내), rejection은 이미 소비한 원료를 원복하지 않음.
- `WorkEvent`는 해당 실제 단계4 투입/시공서비스의 유효 출처가 없으면 기록할 수 없음.
- `ready_for_commission`은 공정완료·최소기간·검수·법률/설비 상태 확인 후 stage11 예약, 실제 `CapitalAsset` 가동은 **다음 달 stage0 이후**.
- 사업계획/자재/작업 순서가 바뀌어도 같은 규칙·초기 상태·seed/명령이면 동일 공정 이벤트·월말 해시.

**상세 시험:** `DC-T19..T27`.
