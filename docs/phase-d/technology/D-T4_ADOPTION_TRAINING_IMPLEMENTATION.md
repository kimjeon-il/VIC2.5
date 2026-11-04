# VIC2.5 D-T4 — 기술 실용화·사업장 도입·설비 개축·직업훈련·숙련 정착

> **설계 확정 / C++ 구현·시험 미실행** · `phase-d-technology/1.0` · 2026-11-04  
> 상위: [D-D0](../D-D0_COMMON_CONTRACT.md), [D-T1](D-T1_KNOWLEDGE_TAXONOMY_PREREQUISITES.md), [D-T2](D-T2_RESEARCH_INSTITUTIONS_EXPERIMENTS.md), [D-T3](D-T3_INVENTION_DIFFUSION_IP.md), [D-I3](../industry/D-I3_CAPITAL_AUTOMATION_PRODUCTIVITY.md), [D-C5](../construction/D-C5_COMMISSION_MAINTENANCE_RETIREMENT.md), C-E1 및 POP. [색인](README.md)

## T4-1. 실용화/채택과 현장 운영의 서로 다른 상태

- **지식 `usable`**: 특정 주체가 해당 기술을 기술적으로 재현·응용할 근거 보유.
- **실용화 `viable_implementation`**: 유효 `TechniqueDefinition/ProductionRecipe`·시제품·기계/설비 사양·공정/검사법/안전 기준이 실제 검증된 상태. 이 자체는 기계 상품/연구비·현금·생산실적이 아님.
- **도입 신청 `adoption_proposal`**: 사업장/공공기관의 실제 금융·인력/원료·면허/규제·시장조건을 비교한 의향.
- **설치·훈련/개축 `converting`**: 실제 `CapitalAsset`/D-C `ConstructionProject`의 설비·부지 개축, POP 고용·훈련, 금융·상품/서비스를 사용해 전환하는 과정.
- **현장 적용 `active`**: 유효 지식 접근+법적 실시권·설비/인력·공정/검수·효력월을 충족한 `SiteTechniqueAssignment`. 가동 대상이 없는 기술의 형식적 해금으로 생산량 자동 증가 금지.
- **정착 `routinized`**: 실제 운전 경험/직무숙련·정비체계/품질·수율 변화가 축적된 상태. 단순 연구 완료보다 늦을 수 있으나 기술에 따라 초기부터 충분히 숙련된 기술자를 확보할 수도 있다.

D-D0에 정의된 `SiteTechniqueAssignment.status = proposed/training/retrofit_ready/installed/active/obsolete`를 변경하지 않는다. 위 개념은 별도 보유상태 원본이 아니라 **그 상태를 결정하는 검증 관측/전환 사건 및 캐시**다. `routinized`는 POP 숙련·검증된 장기간 운영경험으로 계산하는 관측값으로 처리하여 D-D0의 상태 enum에 무단 추가하지 않는다.

## T4-2. 현장 배정 자료형·가동 게이트

`SiteTechniqueAssignment`는 `site_id, technique_id, recipe_ids[], license_or_access_refs[], required_capital_asset_refs[], required_job_qualification_refs[], retrofit_project_ref?, training_event_refs[], status, proposed_month, commissioning_effective_month, source_event_id, revision`로 확장한다. 실물 설비의 존재/상태는 C-E1 `CapitalAsset`, 공정/업종은 D-I3 `SiteActivityAssignment`, 법적 실시권은 C-P/Ownership, 노동자 자격은 POP Registry/EmploymentLedger 정본이다.

**현장 가동 검증 `TechnologyAdoptionGate`**:

| Gate | 확인 사항 |
|---|---|
| `KNOWLEDGE` | 기업/기관·인가된 인력의 실효 `KnowledgeState`, 기술 전달/재현 가능한 지식과 필요한 선행/대체 지식 |
| `RIGHTS` | 지역·시점/분야에 유효한 실시·안전·환경·특허/기밀/사업장 허가 |
| `RECIPE` | 유효한 `TechniqueDefinition`↔`ProductionRecipe`, 실제 필요 원료/연료·서비스/품질 규칙 |
| `ASSETS` | 실제 구입·설치·시운전/검수된 장비·시설, 상태/장비 가동시간 및 자산권 |
| `SKILLS` | 실제 고용/배치된 직무별 자격·훈련/보조장비·인력 가용성 |
| `COSTS` | 교육·기계구입/전환/유지보수·사용료·조달 등의 경제계약 및 실제 자금/채무 처리 |
| `TIMING` | 완료검수/설치와 유효 도입 사건이 이번 달 stage0 이전 확정; 같은 달 stage4/9a의 새 발명·훈련은 과거 공정에 소급 X |

작동 가능한 `SiteTechniqueAssignment.active`와 **실제 생산량 0**은 양립 가능하다. 자재·시장수요·운전자금·연료 부족 시 해당 월 공장 가동량이 0이어도 생산방식 자체가 무효가 되는 것은 아니다.

## T4-3. 전환 유형과 공사·인력 비용

1. **단순 운용법 변경:** 현재 설비·허가·이미 적격인 직원이 모두 새 레시피에 호환되고 필요한 설정/훈련 서비스가 실제 제공되면 다음 달 stage0부터 전환 가능. 필요 없는 대규모 공사를 강제하지 않는다.
2. **설비 개량/교체:** 신규 `CapitalAsset` 상품을 실제 구매/운송·설치하고 D-C `ConstructionProject/WorkPackage` 공정·WIP/검사·차월 효력을 거친다. 기계 1개를 창고 `InventoryLedger`와 설치 설비 양쪽에 보유하지 않음.
3. **직무 재편/재교육:** 노동자의 새 역할은 POP 교육/숙련/직업자격 전이 및 `EmploymentLedger` 실제 계약·인력 배치에 따른다. 자동화로 노동요구 10→4가 되더라도 POP 6명 삭제/통계 감소 금지.
4. **규제/공공사업/군수 방식 전환:** 유효 정부·현장/소유권·법률, 장비/군수물자·안전·교육·재정의 실제 사건을 요한다. 기술 확보→부대 전술능력/학교·병원·철도 효과를 즉시 국가 전체에 일괄 추가 금지.
5. **혼합 기술/점진 도입:** 같은 사업장의 여러 라인/서로 다른 운영 기술이 공존 가능. 기계/직무·연료/생산능력의 공유 예약을 엄격히 분리. '새 방식 선택' 하나로 기존 모든 장비/근로자 사용량이 무효화되지는 않는다.
6. **기술 역행/폐기/복구:** 경제적으로 수익성이 낮은 신공정을 중단하거나 옛 방식으로 복귀 가능하나, 실제 기존 설비/기능·인력·자원·면허/법/전환시간을 확인. `obsolete`는 특정 현장의 운용중단·레시피 보관으로서 전 세계 지식 상실이 아님.

## T4-4. 직업훈련·숙련 학습·기술 경험

교육기관·기업내 사내교육·길드 견습/장인 전수·군사교관·자가 실습은 각 시대의 실제 유효 학습 주체로 인정한다. `TrainingProgram` 관계: `trainer_ref, trainees_population_partition_refs, target_qualification, training_activity_refs, schedule, fees/funding_refs, lab/equipment_refs, competency_check_refs, source_event_id`.

- 훈련/교육 서비스 제공은 `stage4`의 실제 인력·교구/시설·고용·재고/서비스 슬롯을 필요로 한다. 연구활동과 훈련을 동시에 수행하면 다른 역할/노동 서비스가 중복될 수 없고 신규 성과·학력/숙련이 두 번 창출되지 않는다.
- `TrainingOutcomeEvent`는 실제 훈련/자격검증 성공을 stage10 POP-D2 교육·직업자격·건강/서비스 참조의 **원자적 전이**로 반영한다. stage4의 현장 업무 수행과 단계5 임금을 소급 변경하지 않고 **다음 달 stage2 노동적격성**부터 새로운 자격을 반영.
- **학습-by-doing(현장 경험):** 실제 `ProductionEvent`·`EmploymentContract`·장비 작업/품질, 지도자·안전여건·기존 역량으로 경험기반 검증 이벤트를 생성. 이 경험은 연구개발의 새로운 발명을 자동 생성하지 않고, POP 숙련/작업자격에 근거 있는 전이를 예약한다.
- 기술의 지식/면허를 알고 있는 기업과 실제 해당 기술 작업을 수행할 수 있는 특정 인원의 자격/숙련은 다른 정본. 고용인이 사망·이주·징병·퇴직해도 보유한 지식 정의는 삭제하지 않으나 현장의 실제 운용역량이 낮아질 수 있다.
- 신기술 실습의 건강/안전/장애와 편의제공은 **업무별 실제 기능/배치 요건**으로 처리하고 장애 진단 자체로 전체 기술자 자격을 자동 0으로 만들지 않는다.

## T4-5. 생산성·품질·수익의 실제 발생

`ProductionEffect`는 다음 경로로만 실현:

~~~text
ActualOutput(site,recipe,month)
  = min(
      recipe_planned_batches,
      available_installed_capital_services,
      qualified_employed_labor_service,
      delivered_intermediate_goods_and_fuel,
      lawful_resource_site_capacity,
      technique_effective_recipe_constraint
    ) × validated_recipe_yield_and_quality_rule
~~~

- 공정변경은 고정 `+X% productivity` 보너스가 아니라 레시피의 구체적 `input_goods/output_goods/labor/machine_services/quality/waste/maintenance` 요구량 또는 공법별 실제 설비/서비스 조건을 변화시킨다.
- 같은 명목 생산능력에서 숙련/불량률·학습의 효과는 **실제 적격 노동·품질/수율·설비 운용/정비 규칙**에 한해 적용. 계수는 기술·시대별 Ruleset/근거와 시험 fixture. 같은 기술을 갖고도 높은 연료비/수요·직업숙련 부족이면 다른 결과 가능.
- 기술료/로열티·감가/설비 매입비와 실효 노무/자재·원가·이윤은 C-E3 금융·C-E1 생산·C-E2 시장/가격 원장에서 별도로 기록. 고정 보너스는 소득/현금을 생성할 수 없다.
- 비저장 실시간 전력·열의 당월 stage4 생산/동월 소비가 필요한 공정은 D-I2의 `I2-SAFE/ I2-DISPATCH` 선행 CE/C-D4 계약 변경·시험 게이트를 준수. 새 기술을 확보했다는 사실만으로 해당 지원되지 않는 급전을 조용히 허용하면 안 됨.

## T4-6. 월별 사건·상태유효시점

| 기존 단계 | 기술 도입·훈련 |
|---:|---|
| 0 | 전월 승인된 지식/면허/현장설비·훈련효력·사업장 활성 레시피 최종 검증/OpeningSnapshot freeze |
| 1 | 도입/설비 개축·실효 자금·유효 법/기술 권한·고용/훈련/재료·운송 예약 |
| 2 | 기존 POP 자격/Employment를 조회해 연구·훈련·공정별 적격 고용 배분 |
| 3 | 실제 자재/교육·연구/설비 서비스·원료 도착·사용예약 |
| 4 | 실제 공정 생산·교육/현장 훈련 서비스/학습경험·실습 실적, 다른 산업과 자원경합 |
| 5 | 실제 임금·공적 이전·원천징수/교육비 지급 |
| 7 | 설비상품/공법용 부품·교육 자료 실제 거래·설치용 운송/인도, 당월 stage4 소급투입 금지 |
| 8 | 원가/매출·연구/교육/설비투자 비용·채권/채무·로열티/공사 WIP 결산 |
| 9a | 다음 달 이후 도입·추가 훈련/공정 전환계획 및 유효 기술검증/지식 사건 예약 |
| 10 | POP 교육·자격/이동·질병/전투/재해의 인구/설비 손실·현장기술 접근성 변화 |
| 11 | 다음 달 유효 공정/설비·법/자격·권한/원장 검증·단일 월 커밋 |

## T4-7. 수락 기준

- 국가가 기술을 확보했어도 미전수/미계약 기업·현장에 공짜 생산성/전환이 발생하지 않음.
- 기계가 아직 `InventoryLedger` 상품이거나 전월 공사가 미검수이면 현장의 `active` 전환 실패·생산량 0/구공정 유지.
- 10명 고용 사업장에서 노동 요구 4인 공정으로 전환 시 POP 10 유지, 고용 4/나머지 6은 실제 Employment 전직/실업 경로; 현금/지분 자동변화 없음.
- 월 m 실제 훈련은 stage10의 적법한 교육/자격 사건 뒤 **m+1 stage2**부터 신규 기술 직무 적격성에 반영.
- 신공정/새 기술이 산업 생산·환경/노무/자산·부채에 영향을 주는 경우 모든 관련 레시피/시장/회계 원장과 일치.

**세부 시험:** `DT-T28..T36`. [기술 인수시험](TECHNOLOGY_ACCEPTANCE_TESTS.md).

## 참고

[OECD Oslo Manual 2018](https://www.oecd.org/en/publications/oslo-manual-2018_9789264304604-en.html) — 혁신은 도입/사용에 의해 식별한다는 측정 원칙 참고. VIC2.5는 사업장·POP/설비의 실제 전이로 표현.
