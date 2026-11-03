# C-M2 — 편제·장비·훈련·지휘·전투준비태세

- **상태: 설계 확정 / 구현 전** · `phase-c-military/1.0`
- 상위: [C-D0](../C-D0_COMMON_CONTRACT.md), [M1 병역](C-M1_SERVICE_CONSCRIPTION_MOBILIZATION.md), [경제 E1/3](../economy/README.md), [군사 색인](README.md).
- 병력수 단일 원본은 POP; 부대 실물 장비는 Inventory/Ownership; 조직의 편제·훈련/사기·상태 사건만 MilitaryRegistry의 원본이다.

## M2-1. 부대·상급 편제·군종

| 객체 | 필수 필드/의미 |
|---|---|
| `MilitaryOrganization` | organization_id, state_or_faction_ref, service_branch, jurisdiction, headquarters_id, ruleset, active_period |
| `Formation` | formation_id, kind, parent_formation_id?, commander_ref?, active_period, theater_id; 사단/군단/군집단/함대 등 |
| `MilitaryUnit` | unit_id, formation_id?, organization_id, organizational_template_id, branch, home_station, position_id, mission, active_period, revision |
| `OrganizationTemplate` | template_id, era/technology/branch, job_role_slots{}, nominal_personnel, desired_equipment{}, logistics_baseline{}, training_rules |
| `UnitMembership` | relation_id, child_unit_or_formation_id, parent_formation_id, valid_period, source_event_id; 사이클 금지 |
| `UnitPosition` | unit_id, operational_area_id, stationing_kind, arrival/effective_month, transit_status, revision |
| `UnitState` | unit_id, organization_cohesion, fatigue, morale, experience_distribution_ref, training_progress, maintenance_status, current_order_id |
| `CommanderAssignment` | assignment_id, office/formation/unit, commander_character_ref?, appointment_authority, qualifications, duty_status, effective_period |
| `EquipmentAssignment` | unit_id, inventory_lot_or_asset_ref, qty_assigned, availability_state, purpose, valid_period, revision |
| `UnitReadiness` | unit_id, month, role, manpower_ready, equipment_ready, logistics_ready, command_ready, condition_tags; **파생·검증 캐시** |

**편제 구조:** 한 월/역할의 실제 부대 소속은 활성 parent 최대 하나, 부모-자식 관계 사이클 금지. `Formation`은 **유닛 멤버십의 컨테이너**로 자체 병력수나 장비를 별도로 생성하지 않는다. 사단 1개 아래 여단 2개가 각각 1,500명이라면 사단의 병력은 3,000명, 6,000명이 아니다. 연합/작전 배속(temporary_control)은 *지휘·명령 권한* 관계로 따로 기록해 상설 편제 소속 중복을 피한다.

육상·해상·항공·수송지원 등 군종/역할은 `service_branch`와 해당 시대의 기술·교리·시설을 통해 정의한다. 1801년 설정에 20세기형 항공전력/현대 기계화 편제를 자동 부여하지 않는다. 부대별 등급/경험·신형 장비 보급률은 실제 인구·훈련·재고와 원장 사건에서 산출한다.

## M2-2. 편제상 정원과 실제 병력

세 가지 수치를 반드시 구분:
- `AuthorizedStrength(unit)`: 정부/군제 규정상 허용된 편제정원, `OrganizationTemplate`/법규의 결과(인원이 아님).
- `AssignedStrength(unit)`: 실제 POP 인구 중 유효 `ServiceAssignment` 인원의 합계.
- `DeployableStrength(unit,role)`: 실제 AssignedStrength 중 해당 작전 역할의 건강/기능/훈련/직무·법적 요건과 지휘/장비 접근이 충족된 **정수 인원 합계**.
- `PresentForDuty`와 `CombatEffective`도 별도. 병가·휴가·포로·실종·후송중 인원과 적절한 지휘/임무를 수행 중인 인원을 구분한다.
- `unit_reinforcement_need=max(0,authorized-assigned)`; 초과 배치는 별도 규칙/임시 편제로 허용 가능하나 정원 증가를 자동 인구 생성으로 해석하지 않는다.

실제 병력은 같은 인구 셀을 군복무 원장에 두 번 연결하지 않는다. 주된 복무 배정은 한 사람·한 시점에 하나. 의무/보급/정보 임무 인력도 실제 군인 또는 합법적 민간 고용에서 온 사람으로 계산한다. 작전참여자와 후방지원자는 관계 태그로 분리할 뿐 세계 총인구를 증가시키지 않는다.

## M2-3. 장비·정비·재고/자산의 관계

- `Good`와 `EquipmentType`은 해당 시대의 상품/장비 분류(개인장비, 화기, 차량, 군함/항공기, 보급차량, 통신장비, 의료·공병 장비 등)를 정의하며 상위 기술/생산레시피와 연결한다.
- 장비는 초기 재고/생산/구매/이동/설치·폐기/노획/수리 사건을 통해서만 수량이 바뀐다. 장비 양도/양륙/정비 단계에 맞춰 `InventoryLedger` 보유자/위치/상태와 `EquipmentAssignment` 사용권이 동시에 갱신된다.
- 내구 장비는 해당 수량을 **serviceable/needs_repair/disabled/destroyed**로 구분한다. `destroyed`는 실제 재고 손실/자산 처분을 동반하고, `needs_repair`는 수리시설·노동·부품·공기·재정을 투입해야 회복된다.
- 기계·함선·차량 등 단일 자산 추적이 중요한 경우 `CapitalAsset/UniqueEquipmentAsset`을 사용하여 식별하고, 대량 소모품은 Good/Inventory로 관리한다. `InventoryLedger`와 `AssetRegistry`에 같은 장비가 **두 번 물리적으로 존재**하지 않도록 설치/자산화/폐기 전이로 연결한다.
- 실제 재고보다 `assigned_equipment`가 많으면 거부. 보급창에 100개 있는 무기를 2개 부대에 각 100개 배정해 두 번 전투력 산정하는 행위를 막는다.
- 장비 소유자(정부/기업/외국 임대 제공자)와 보관자·운용자(부대)가 다를 수 있다. 징발/대여/무기대여 등은 법적 권리·물품 이전·금전거래를 구분한다.

## M2-4. 훈련·경험·조직도

- `TrainingProgram`: 직종·교리·교육역량·필수 훈련기간·실물 훈련재/실습장·교관·훈련 장비·안전/사고 위험을 정의한다.
- `TrainingEvent`는 실제 훈련병/교관 POP 및 훈련 시간·시설 슬롯·소모품·정부예산·훈련 이수 수준을 함께 기록한다. 인력 훈련은 실물/비용 제약 없이 자동으로 경험치만 올리는 행동이 아니다.
- 훈련효과는 `skill_tags`/교육 프로필과 전투임무별 `UnitState.training_progress`의 분리된 변동으로 관리한다. 군사 임무의 개인 숙련과 부대의 조직적 숙련은 다르다.
- 전투 경험은 해당 전투에 실제 참가해 생존한 병력/지휘조직에만 사건으로 배분한다. 부대 ID만 유지되었다고 전사·전역/교체된 인원까지 경험이 남아 있는 것으로 간주하지 않는다.
- `morale`는 급료지급, 피로, 부상·전사/포로, 보급부족, 정치적 충성/목표/지휘정통성 등 실제 원인과 함께 변한다. `cohesion`는 배속 병력 교체·훈련, 지휘관 변동, 이동·전투 이벤트에 영향을 받는다.
- `morale`, `cohesion`, `fatigue`, `training`은 분리된 상태 값이며, 각각 명시적인 사건·상한·감쇠 규칙으로 갱신한다. **통합 전투력 단일 수치를 쓰기 가능한 진실값으로 보관하지 않는다**.

## M2-5. 지휘·명령·충성

- `ChainOfCommand`는 정치기관이 설정한 임명/작전통제권과 `Organization`의 실제 지휘관계로 산출한다. 인사변동에는 `AppointmentEvent`와 유효기간/권한 검증을 사용한다.
- 지휘관을 개별 인물로 표시하는 선택적 `CharacterRegistry`를 도입해도 새로운 사람을 만들어내지 않는다. 인물은 실제 POP의 **한 명에 대한 유일한 앵커**(slice_id/partition/allocated_headcount=1)로 연결하거나 추상 직책만 기록한다. 인물의 사망/퇴역은 인구 셀 및 임명 원장과 함께 변경되어야 한다.
- 지휘결속/명령전달/지역통신·고립 여부는 시대별 통신수단·조직·지역 접근성을 반영한다. `Order`는 명령권 있는 주체가 시점/목표·범위·부대/우선순위를 기재해야 유효하며, 명령 제출만으로 즉시 전장에서 승리하는 것은 아니다.
- 내전·쿠데타·반란 시 각 부대의 allegiance(충성·지휘권 전환)는 해당 부대 인원의 정치·사회/법적 상황, 조직적 지휘·보급·장비 관리 등 실제 인계 사건으로 처리한다. 부대의 소속국가가 바뀐 것만으로 해당 주민의 국적/지역이 자동 바뀌지 않는다.

## M2-6. 작전준비태세 계산 계약

`Readiness(unit,role,m)`는 분리된 집계 `{eligible_people, serviceable_equipment_by_type, consumable_days, logistics_access, morale, cohesion, fatigue, command_access}`를 계산한다. 사용자 인터페이스에 종합 지수를 **표시용 파생값**으로 제공할 수 있으나, 병력·장비·보급 제약을 숨기거나 단일 수치만으로 전투 결과를 결정하지 않는다.

기초 작전가능 인원의 상한:
```text
unit_role_personnel <= assigned_people
unit_role_personnel <= health_and_qualification_eligible_people
unit_role_personnel <= equipment_supported_people(rule_specific)
unit_role_personnel <= deployable_logistics_supported_people(rule_specific)
```
병참 접근성 0이면 장시간 지속작전은 불가능할 수 있지만, 해당 부대의 현재 장비·현원·실제 식량 재고가 소멸하는 것은 아니다. 보급 부족과 기초 병력 소멸을 구분한다. 전투 준비도의 정확한 함수/계수·부대별 임무는 C-M4 ruleset에서 결정한다.

## M2 검증 게이트

- M2-G1: 50명 + 60명 두 하위 부대를 가진 상급 편제의 병력은 110명. 부모 편제에 110명을 중복 추가하지 않음.
- M2-G2: 두 부대가 동일 무기 70개 재고를 각각 70개 장비했다고 처리하지 않음.
- M2-G3: 부대정원 1,000명·실제 배속 800명·전투적격 620명이면 실제 병력 800/전투적격 620을 별도 산출.
- M2-G4: 군 복무자와 민간 주된 고용의 중복 인원 관계를 찾아 거부.
- M2-G5: 함선/장비의 파괴·수리·노획은 Inventory/Asset/Ownership/Military 원장 정합 유지.
- M2-G6: 군인이 부상 또는 사망하면 해당 부대·전투경험/근무 가능 분포가 실제 인구 전이와 동기화.
- M2-G7: 상급부대 편제 고리(사단A→여단B→사단A)는 거부.
- M2-G8: 임명권 없는 정치주체의 지휘관/군조직 변경은 거부; 지휘 변경으로 국적·주민수 변동 없음.
