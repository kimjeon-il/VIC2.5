# C-M4 — 작전·전투·사상자·의료회복 상세 설계

- **상태: 설계 확정 / 구현 전** · `phase-c-military/1.0`
- 상위: [C-D0](../C-D0_COMMON_CONTRACT.md), [POP-D2](../../pop/D2_TRANSITIONS.md), [M1](C-M1_SERVICE_CONSCRIPTION_MOBILIZATION.md), [M2](C-M2_UNITS_EQUIPMENT_READINESS.md), [M3](C-M3_LOGISTICS_SUPPLY_TRANSPORT.md), [군사 색인](README.md).
- **월은 원자적 결과 단위**. 상세 날짜·작전/교전 단위의 서브틱 이벤트는 확장할 수 있으나 12단계 공개 계약과 PopulationEvent·Inventory/FinancialLedger의 월말 원자적 확정은 유지.

## M4-1. 작전/전투 기본 객체

| 객체 | 필수 필드 |
|---|---|
| `OperationPlan` | operation_id, coalition/war_id, commander/authority_id, theater/area, participating_unit_ids[], objective, authorized_start_month, duration/budget/supply reservations, legality_refs |
| `OperationalOrder` | order_id, unit_id, stage1_registered, operation_id, mission/target_area, stance, constraints, priority, source_event_id |
| `OperationalArea` | area_id, region_id/graph_node, geography/terrain/season/weather_tags, access/adjacency, fortification_asset_refs, civilian_presence_refs |
| `Engagement` | engagement_id, operation_id, parties[], committed_unit_assignments[], exposure_window, objective, legal_target_kind, environment_ref |
| `CombatStrengthView` | engagement_id, side, **actual deployable people**, usable equipment, supply, experience/cohesion/morale, terrain/fortifications, command, deterministic_rule_version |
| `CombatResolution` | engagement_id, engaged_people_by_unit, equipment/munitions_used, casualties_by_category, unit_disruption, control_event_refs, result_provenance |
| `CasualtyEvent` | event_id, engagement_id, unit_id, source_slice_id, result_kind, count, health_before/after, service_before/after, civilian_or_military, attribution |
| `MedicalCareEvent` | event_id, patient_slice_id, care_site_id, care_slots/medicine_spent, treatment_success/failure_or_delay, health_transition_refs |
| `Recovery/ReturnEvent` | event_id, prior_event_id, returning_personnel_count, medical_status, service/discharge/rehabilitation destination |
| `WarDamageEvent` | event_id, asset_owner/location/good_id, physical_loss, property_repair_need, displacement/civilian_effect_refs |

지형·통신/기상/도로/운송능력은 인과적 입력으로 반영하지만 지도 구현 없이도 `OperationalArea`의 노드·연결·상태로 실행 가능하다. 추후 GIS 도입은 위치/인접경로 자료만 교체하고 인구·재무·재고 원본은 유지한다.

## M4-2. 명령 → 교전 → 결과의 처리

**단계 1 (작전계획):**
- 유효 정부/군 지휘권, 전쟁상태/교전 가능 법률, 부대 위치/이동 가능 경로, 부대/군수 수요와 예약된 보유자원을 검증한다.
- 해당 월에 기존 현역 상태로 존재하는 부대만 동원 가능. 단계10에 새로 입대한 인원은 같은 달 이미 끝난 작전에는 참가하지 못한다.
- 각각의 부대·전력은 동일 시기 동일 인원/장비로 서로 다른 교전에 **중복 배정될 수 없다**. 여러 교전이 순차적으로 발생하는 경우 확정된 앞 사건 이후의 잔여 병력/재고를 사용한다.

**단계 7 (작전 전 보급):**
- 단계3~4 생산/단계7 실제 입고된 재고라도 유효한 수송/보관·사용권과 관련 예약이 확인된 물량만 그 달 작전 단계10에서 사용할 수 있다. 미도착 주문은 실제 보급으로 계산하지 않는다.

**단계 10 (작전/교전 해결):**
1. Engagement를 `operation_id, engagement_id`의 안정적 순서 및 명시된 `dependencies`로 정렬한다. 서로 연동되지 않은 지역은 병렬 제안할 수 있으나 정본 원장 반영은 결정론적 조정과 충돌 검증으로 한다.
2. 관련 지휘·법적 권한, 각 Unit 실제 service status/배치 인원·전투역할 적격, 장비의 사용가능 상태, 해당 지역/기간의 지형·날씨/통신·운송로·보급량을 스냅샷/이미 반영된 staged 사건에서 확인한다.
3. 해당 교전의 노출 병력 `exposed_people`와 사용 가능한 장비·군수품을 상한으로 산정하고, 같은 인구/장비 예약이 다른 교전에서 이미 소비/점유됐는지 검사한다.
4. 시대별 전투 규칙 `CombatRuleSet`으로 교전강도·공격/방어/조직/훈련·장비·지형·군수·지휘·교전기간·피로에 따른 **비율/조건부 손실 기대값**을 만든다. 마법적인 '1병력이 1명을 직접 삭제' 계산이 아니라 상대 자원·조건·사건의 결과로 처리.
5. 정수/고정소수점 확률·잔여량과 `world_seed/month/phase/unit/engagement/draw`로 결정론적 사건을 산출한다. 부대별 피해 기대량을 정수 인원으로 배분할 때에는 최대 노출인원과 상호배타성/우선순위·largest remainder 규칙을 적용한다.
6. 결과는 `killed`, `acute_injury`, `long_term_impairment_from_this_event`, `captured`, `missing`, `unharmed`의 **서로 겹치지 않는 사건 결말**로 분류. 기존의 질병/장애 프로필이 있다고 해서 새 손상과 병존하지 못하는 것은 아니다. `wounded` 같은 통계적 합계는 acute+longterm 등 명시된 정의에서 파생.
7. 모든 전사자는 `PopulationEvent`로 인원 감소, 부상은 `HealthProfile`의 새로운/기존 상태로 인원 이전, 포로·실종은 각 service status로 이전하고 `ServiceAssignment`/Unit명부를 변경한다. 자연사·전투 사망·전염병에 중복 적용 불가(경쟁위험 또는 선후 사건 위험집합 재검증).
8. 탄약·연료·식량/물·장비 손실/점령지 이동을 `SupplyConsumptionEvent`, `InventoryLedger`, `EquipmentAssignment`, `UnitPosition`/Jurisdiction의 대응 사건으로 기록. 장비/부대 상태·피로·사기·전투경험의 실제 참여 분포 갱신.
9. `CombatResolution`과 연계된 모든 JournalBatch를 StagingWorld에 적용한다. 하나라도 인구/재고/분개/법률/참조 검증 실패 시 **전체 월 롤백**. 월말 단계11에서 최종 정합성 검사 후 확정.

**작전결과와 인구 피해는 반드시 구별.** 군이 후퇴하거나 특정 지역의 통제권을 얻었더라도 상대 병력이 모두 사망한 것은 아니다. 후퇴·항복·재편·재보급·포로·실종 등 별도 상태를 가진다.

## M4-3. 전투 계산의 공통 수학적 형태

세부 전투 수치 모형은 시대·전장 유형별로 변경 가능한 **버전 있는 규칙 함수**로 고정한다. 최초 구현에서 단일 전투력 점수의 상대비로 손실을 전부 결정하지 않도록 다음 분리 인터페이스를 사용한다.

```text
E[u]      = min(unit_assigned_and_eligible_people,
                mission_personnel_limit,
                accessible_supply_personnel_limit,
                equipment_supported_personnel_limit)
Q[side]   = ResolveCapability(E, serviceable_equipment, training,
                              morale, cohesion, fatigue, command_access,
                              weather, terrain, fortifications, doctrine, era)
I[battle] = ResolveIntensity(Q_sides, objective, duration, defensive_posture,
                             supply_capacity, ruleset)
ExpectedCasualties[u] = E[u] * ExposureRisk(I, role, protection,
                        organization, logistics, environment, ruleset)
AllocateCappedLosses(KIA, WIA_temporary, WIA_longterm, POW, MIA, survivors,
                     each_unit_slice_exposure, fixed_point_policy)
```

- `E`와 `Q`는 **부대 실제 인구/장비/보급에 근거한 계산값**이며 그 자체가 인구 원본은 아니다.
- `ExpectedCasualties`는 예측값이고 실제 손실은 정수 `PopulationEvent`. 보급·장비 소모도 실제 `InventoryLedger`의 수량으로 제한.
- 모든 `ExposureRisk`의 확률 및 하위결과 확률은 0~1 범위이며 총결과 인원은 노출 인원 초과 불가. 한 부대의 여러 교전에서는 앞 교전 손실을 차감한 실제 잔여 인원을 다음 교전의 위험집합으로 사용한다.
- `Q`, `I`, 확률계수/포획·부상·사망 비율·시간/지형 영향·성능/효율은 모두 규칙 데이터/실험에서 수치 보정한다. **역사 근거 없이 특정 병종 전투력과 피해계수를 임의 상수로 확정하지 않는다**.
- 월 단위로 수백 번의 상세 총격·사격을 추적하지 않더라도 **대전략 수준의 병력·물자 소모·작전결과**를 재현한다. 월중 시간/일자 확장은 서브틱 규약/적합한 저장 로그로 추가한다.

## M4-4. 사상자 분류 및 1,000명 예시

예시: 건강하고 현역인 1,000명의 특정 전투에 대한 결과가 KIA 50명, 일시 부상 180명, 장기 기능손상 70명, 무손상 700명인 경우.

| 결과 | 인구 변화 | 군사/보건 결과 |
|---|---:|---|
| 전사 50 | 세계인구 -50 | 부대 인원/복무관계 종료, 자산 상속/청산 |
| 일시 부상 180 | 보존: 기존 셀 -180, 급성부상 코호트 +180 | 상태/의료 필요 기록; 임무적격성 재평가 |
| 장기 기능손상 70 | 보존: 기존 셀 -70, 장기손상 건강 프로필 +70 | 기존 복합 건강상태 병존 가능, 의료/재활 필요 |
| 무손상 700 | 그대로 | 실제 가능한 전투 역할 조건 재평가 |

기말 주민 수 = `1000-50=950`. 현역 복무 계정에 **치료 중 250명이 포함되어도** 총 생존자 수 950과 일치해야 한다. 임무수행 가능자(여기서는 다른 제약이 없다고 가정할 때 700명)와 생존현역 950명을 혼동하지 않는다. 포로/실종이 0인 이 사례에 포로/실종 인원을 중복 가산하지 않는다.

기존 만성질환·장애와 새 부상을 모두 갖는 건강 프로필이 허용되며, 기능적 제약을 특정 직무에만 적용할 수 있다. 모든 장기 기능손상자가 영구적으로 실업/제대한다고 자동 확정하지 않는다.

## M4-5. 회복·재활·제대와 보건

- `MedicalCareEvent`는 치료 가능한 실제 인구분할, 치료기관의 해당 기간 사용 가능 병상/의료인·약품·수송·재정과 매칭한다. 의료 서비스 신청/예산 승인만으로 실제 회복 발생 금지.
- 단계10에서 회복·자연사·전쟁 피해의 위험집합을 계산할 때 동일 생존 인원이 여러 인과사건에 중복 소비되지 않도록 사건 의존순서/경쟁위험을 사용한다.
- 회복 결과는 기존 HealthProfile의 급성 질병/부상 개선·잔여 기능변화·재활 기간을 합성한 새 상태로 이전. 장기 손상·후유증/치료결과를 과거 기록 없이 지우지 않는다.
- `ReturnToDuty`는 건강 회복, 법률/부대 규정, 재훈련·장비·실제 복무관계를 모두 충족할 때만 가능한 행정/군사 사건. 회복 직후 민간 노동에 자동 편입하는 것과 구분.
- 군인 가족·유족/포로·전쟁 난민의 현물 식량/보조금/연금/치료지출은 C-E3/E4 원장에서 실제 지급·인도와 함께 기록. 사망자의 금융자산은 금융권리/유산/법적 승계 원장으로 이전.
- 의료·상병/피로·사기에는 이전 달·현재 전투 결과의 비가역적 경력/상태 변화가 가능하다. '전투 종료 즉시 원상복구'하는 임시 피해 리셋은 금지한다.

## M4-6. 민간인·시설 피해와 준수 규범

- 주민·주거·생산시설·공공의료·물류·농업 자산 등의 피해를 **별도 인구/WarDamageEvent**로 관리하고 군인 사상자/전투 참여 인원으로 무분별하게 합산하지 않는다.
- 민간인의 질병/사망/주거 상실·이주·가계소득 감소는 실제 대상 POP·Health/Household/Financial/Asset/Inventory 원장으로 연결. 지역 통제 변동만으로 주민을 자동 징집하거나 재산을 소각하지 않는다.
- 전쟁 시기의 법/국제규범·유효 협약에 따라 민간인 보호·의료시설 보호·포로·점령 책임을 평가한다. 피해의 발생/법적 평가/정부 대응은 서로 다른 기록이다. 국제인도법의 구별·비례성·예방 원칙은 해당 시대에 법적으로 적용되는 범위에서 규칙 모델의 근거로 참조하며 현대 규범을 모든 역사시대로 기계적으로 소급하지 않는다.
- 민간인 의도적 공격 또는 강제이주 등 위법행동의 조작적 최적화·보상 보너스는 설계 범위 밖이다. 전쟁범죄 성격의 사건이 역사자료/게임 결과로 발생하면 피해·법적 책임·구호·전후 복구의 **기록 및 결과**로 처리한다.

## M4 검증 게이트

- M4-G1: 현역 1,000명 전사50·일시부상180·장기손상70·무손상700 → 생존 950, 전사 인원 중복 없음.
- M4-G2: 동일 부대 100명에 두 교전이 각각 80명 피해 요구 → 앞 사건 반영 후 잔여를 넘는 사건은 감축/거부; 생존자 음수 없음.
- M4-G3: 기존 장애+새 급성부상 동시 건강 프로필 유지·다중 건강 상태 증발 금지.
- M4-G4: 미수령 탄약 100을 가지고 전투한 것처럼 소모/효과 발생 금지.
- M4-G5: 장비 20대 중 실제 3대 파괴 → 가동/손상 장부·Inventory/Asset/Ownership에서 3대 처리.
- M4-G6: 교전 이전/이후 부대 위치·군사 통제 변경에도 주권·주민 주소·국적 임의 변경 금지.
- M4-G7: 사상자에게 의료서비스·부상회복을 적용할 때 실물 시설/의약품/인력 용량 준수.
- M4-G8: 단계10 전투 중 발생한 장부상 전투손실이 단계11 최종 재고/자산·금융원장 검증에 포함됨.
- M4-G9: 실패/중복 event_id일 때 전쟁·인구·재고·월 index의 모든 변경을 롤백.
- M4-G10: 동일 초기 snapshot/ruleset/seed·정해진 정렬 및 worker 수 변화에서도 정해진 결정론 범위의 전투 결과/전이 일치.
