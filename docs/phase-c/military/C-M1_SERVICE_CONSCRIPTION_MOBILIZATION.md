# C-M1 — 병역·모집·징병·동원·제대

- **상태: 설계 확정 / 구현 전** · `phase-c-military/1.0`
- 선행: [C-D0](../C-D0_COMMON_CONTRACT.md), [POP-D2 사건](../../pop/D2_TRANSITIONS.md), [군사 색인](README.md).
- **정본:** `PopulationSlice.count`의 인원수와 DemographicCohort의 `service_status`, `ServiceAssignment`의 실제 부대/복무 배정, `EmploymentLedger`의 민간 주된 노동 배정이 공동으로 검증된다.

## M1-1. 병역 제도의 모델

| 객체 | 핵심 필드/역할 |
|---|---|
| `ServiceLaw` | law_id, jurisdiction, eligibility_policy, minimum/maximum_age, sex/legal_status/citizenship_rule, exemption/deferment, peacetime/wartime rules, valid_period |
| `ServiceSystem` | government_id, recruitment_mode, enlistment_term, reserve_term, rotation/training_policy, release_obligations, enforcement_capacity |
| `RecruitmentDemand` | demand_id, unit/branch/occupation, headcount_target, required_qualification, eligible_regions, pay, funding_reservation, required_training |
| `ServiceEligibility` | `slice_id`, valid_month, available_eligible_count, reasons, rule_revision; 계산결과 캐시(인구 정본 아님) |
| `ServiceAssignment` | assignment_id, slice_id, unit_id?(정확한 배정 없으면 pool_id), count, service_kind, effective_period, contract_ref, provenance |
| `MilitaryPersonnelPool` | pool_id, authority, branch/region, personnel_state, allocation_refs; **편제 대기 인구 관계**이며 별도 병력 원본 아님 |
| `MobilizationOrder` | order_id, government_authority, originating_law, target_eligible_slices, destination_units, people_requested, supplies/finance refs, effective_month |
| `DemobilizationOrder` | order_id, affected_service_assignments, count, resettlement_policy, veteran_support_refs, effective_month |
| `ServiceTransition` | source_slice_id, destination_cohort_key, count, from/to service state, service_assignment_delta, employment_delta, source_event_id |

**복무상태는 POP-D1의 `civilian/active/reserve/pow/missing`를 그대로 사용**하며 legacy_ambiguous를 포로 또는 실종 중 하나로 임의 추정하지 않는다. `reserve`인 사람은 주민 인구에 포함되고, 법/계약이 허용하는 범위에서 민간 취업을 유지할 수 있다. `pow`는 포로, `missing`은 생사가 확인되지 않은 실종으로 별개이며 자동 사망자가 아니다.

정치·병역제도에 따라 모병, 추첨징집, 보편적 징병, 직업군인제, 지역/영지 동원, 용병계약, 동원예약제, 병역면제·대체복무, 비상동원과 강제부역을 표현할 수 있다. **어느 시대의 어느 국가도 보편징병/같은 복무기간을 기본값으로 강제하지 않는다**. 시대별 법률/국제 규정의 적법성 검증도 제도 버전을 참조한다.

## M1-2. 적격성 및 실제 동원 가능한 인원

`EligibleCount(slice,m,role)`는 다음을 평가하여 실제 인원 상한을 반환한다.

1. 월초에 유효한 병역법/정부·지역의 법적 징집 권한과 국적·신분 및 거주.
2. 출생월에서 구한 실제 연령, 교육·훈련 자격, 직무별 기능 요구와 개인의 건강 상태/편의 제공.
3. 이미 현역·포로·실종·다른 소속 부대/징집 계약 등에 점유된 인원 및 **동원 명령의 잔여가용 인원**.
4. 보호직종/필수산업의 동원 유예, 학업/돌봄/가족 부양, 자발적 입대, 면제/거부권과 다른 법적 예외.
5. 군비·훈련 수용능력·군복·식량·수송 용량·실제 병영수용능력. 인구 적격자가 많다고 즉시 현역 배속할 수 있는 것이 아니다.

건강상태나 장애 명칭 하나만으로 모든 군사직무 부적격으로 취급하지 않는다. 직무의 요구능력, 실제 기능상 제약, 보조수단/편의 제공 여부로 평가한다. 법률상 불평등/차별은 기술 계수가 아니라 역사적 법·제도 데이터로 표시한다. 노동/병역 적격성이 불명인 집단은 의사결정 전에 적절한 `allocation_partition`으로 세분화하고 `unknown`을 `0`으로 변환하지 않는다.

월간 병역 후보 `Available`는 현재 활성인구(정수)와 각 상태별 분포의 교집합으로 계산하며, 단순히 지역 총인구에서 군인·취업자 수를 빼는 근사값을 원본으로 사용하지 않는다.

## M1-3. 모집·징병·동원 매칭

**단계 1 — 제안:** 정부/군사 지휘 권한·법령의 근거로 `RecruitmentDemand`/ `MobilizationOrder` 제안. 예비비·군복·수용시설·훈련과 보급 수요를 정부예산/금융/재고 원장에 예약. 명령 제출 자체가 징병 성공 아님.

**단계 2 — 기존 인력 경계:** 월초 active 상태의 인원을 민간 주된 노동계약에서 제외한다. reserve는 동원되지 않았다면 민간 취업 가능. **당월 신규 징집 계획은 즉시 해당 월의 생산 인력을 소급 감축하지 않는다.**

**단계 10 — 실제 확정:** 등록 우선순위/자격/지역·훈련 접근/법적 예외에 따라 안정적 ID 및 결정론적 정수 배분으로 사람 수를 확정하고 `PopulationEvent`로 `civilian/reserve → active` 전이. 같은 `JournalBatch`에서 `ServiceAssignment` 증가/배치, 민간 `EmploymentLedger` 해당 인원 분리·고용종료, 필요한 가계·교육·자산/계약 분포 이전을 기록한다.

**단계 11 — 검증:** 인구 변화 보존, 모집 정원/실제 현원 상한, 군민 겸직 불가 정책과 노동계약·금융채무의 정합성, 차월 급여·훈련/군수책임 예약을 검사한다. 동원 효과는 **다음 달 단계 0 이후** 민간노동공급·현역 급여·전투 준비에 반영된다.

같은 사람을 상비군 모병과 예비군 동원에서 동시에 뽑는 두 명령이 있으면 하나를 먼저 배정하고 잔여를 재산정하거나 revision 충돌로 거부한다. `ServiceAssignment.count`는 실제 `active/reserve/pow/missing` 코호트에 할당된 인원을 초과할 수 없다.

## M1-4. 복무·급여·훈련과 경제

- 복무 중 군인 급여는 `FiscalLedger`에 법적 지급근거/채무를 등록하고 실제 월별 `FinancialLedger` 지급 시에만 가계 수입이 된다. 군급여 체불과 미지급 채무는 별개로 기록한다.
- 군인의 가족은 원래 `HouseholdPool`에 남거나 명시적인 집단거주·재배정 사건을 따른다. 출정 위치로 주민 등록주소를 자동 바꾸지 않는다. 군 숙식/의약품은 실제 군수 재고를 소모/배정한다.
- 훈련 기간의 민간 생산 기회비용, 훈련을 위한 강사·시설·장비/소모품을 실제 노동/시설/상품 서비스로 계산한다. 훈련 지출은 장병 숙련 상승과 별개 원장 사건이다.
- 동원 예산 부족 시 모집 실패/훈련 대기/미지급 급여/수용 병목을 명시한다. 예산액만으로 현역이 생겨나지 않는다.

## M1-5. 제대·예비군·포로/실종·장기 건강

- 제대는 `active → reserve/civilian`의 별도 인구 전이, `ServiceAssignment` 종료, 부대 편제와 군 보유 물품 반납/전출을 포함한다. 노동시장 복귀는 **다음 달 이후** E1 고용청산에서 결정하며 제대 즉시 취업 보장 안 됨.
- 정당한 예비군 소집은 연령/훈련/법률/사업장 상황에 영향을 받는다. 예비군 훈련은 유효한 별도 휴가·소득·훈련 비용을 통해 처리할 수 있으나, 월간 정본에서는 주된 고용과 현역 상태를 중복 보유하지 않는다.
- `Captured/Missing/Returned`는 실제 포로·실종 상태로 분리. 포로는 적국이 보관/수용하는 인도적 보호 대상이지만 해당 인구가 원 국가의 주민/국적과 곧바로 분리된다는 뜻은 아니다. 수용·교환·송환은 별도의 인구/군사/금융/보급 사건.
- 부상·장기 손상·질병이 있는 군인은 복합 `HealthProfile`로 추적하며 원칙적으로 현역에 남거나 치료/전역 과정에서 단계별 전이될 수 있다. 장기 장애를 무조건 사망/실업으로 만들지 않는다.
- 군인·유족 보조·연금, 재활/의료, 직업훈련은 C-E3/E4 및 POP-D2 원장의 실제 지급·서비스로 처리한다. 사망자의 금융자산/채무는 유산/청산 계정으로 이전한다.

## M1-6. 군사 인원 측정 정의

`ActiveStrength` = 해당 시점 실제 `active` 상태인 PopulationSlice 중 유효 ServiceAssignment로 부대/병력대기 풀에 배속된 인원 합계. `ReserveStrength`는 reserve 인구의 별도 합계(민간 취업 중일 수 있음). `DeployableStrength`는 ActiveStrength에서 질병/부상·훈련·법률·편제·장비/보급 병목을 차감해 실제 작전 적격 조건을 충족하는 사람의 합계. 군인 인구와 실 전투가능 인구를 동일시하지 않는다.

## M1 설계 게이트

- M1-G1: 지역 100명 중 현역 10·예비군 20·민간 70인 경우 세계 주민 수는 **100**, 130이 아니다.
- M1-G2: civilian 적격 15명에게 서로 다른 두 명령이 각 10명 동원 요청 → 합계 15명 이하.
- M1-G3: 예비군 취업 12명 중 5명만 월말 동원 → 당월 생산에 12명이 유효, **다음 달** 민간 노동가능 인구가 5명 줄고 현역이 5명 증가한다.
- M1-G4: 징병법 월 m 제정 → m의 지난 고용/전투에 소급 반영되지 않음.
- M1-G5: 군급료 100 중 지급 70, 체불 30이면 가계에 현금 70만 입금.
- M1-G6: 제대 8명은 총 주민 8명을 보존하고 군 현역/민간 노동 적격을 차월 기준으로 갱신.
- M1-G7: 부상·장애·포로/실종·유족 관계가 재산/고용/가구 원장과 함께 원자적으로 전이.
- M1-G8: 거주지와 실제 파견 위치가 달라도 국가별 주민 집계에서 군인이 중복되지 않음.
