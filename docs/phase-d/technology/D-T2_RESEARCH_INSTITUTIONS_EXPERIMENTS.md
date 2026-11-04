# VIC2.5 D-T2 — 연구기관·연구인력·연구재정·실험·연구성과

> **설계 확정 / C++ 구현·장기시험 미실행** · `phase-d-technology/1.0` · 2026-11-04  
> 상위: [D-D0](../D-D0_COMMON_CONTRACT.md), [D-T1](D-T1_KNOWLEDGE_TAXONOMY_PREREQUISITES.md), C-E1~E5, POP-D0~D3, C-P1~P5 및 C-D4. [색인](README.md)

## T2-1. 연구는 실제 경제·사회 활동

연구소·대학·길드·공방·기업 연구부서·군 연구기관·관립 실험소·개인 발명가 등 **유효 법인/기관/가계의 기존 실재 주체**가 수행하며, 현대적인 대학·법인제도 없이도 역사적 연구·실험/장인개량은 가능하다. 연구기관의 이름·건물 존재만으로 국가별 `innovation_points` 자동 생산 금지.

**연구개발**은 신규성·창의성·불확실성·체계성·전달/재현 가능성 등 OECD Frascati 기준을 참고한 `research_mode`로 분류할 수 있으나, 역사적 전통/비공식 발명 활동에는 시대별 규칙을 적용한다. 단순 제품/교육훈련·사용 장비 구입·홍보/판매나 면허 계약만으로 **신규 R&D 성과가 발생한 것으로 계상하지 않는다**. 혁신의 실현과 연구 성과도 구분한다.

| 활동 | 원장과 상태 |
|---|---|
| `basic_research` | 일반 원리/기초지식 탐색, 후속 응용과 바로 연결되지 않을 수 있음 |
| `applied_research` | 유효 기술 목표·제품/공정 개선의 지식 확보 |
| `experimental_development` | 검증된 원리/실험을 바탕으로 원형제작·성능시험/방법 실용화 |
| `engineering_adaptation` | 기존 기술을 현장에 적용하기 위한 설비·작업 수정, 신규 지식이 없다면 R&D로 중복계상 금지 |
| `occupational_training` | 실제 POP 자격/기능 형성, D-T4/POP 교육 정본; 연구 프로젝트의 연구성과로 무료 계산하지 않음 |
| `artisan_trial_and_error` | 소규모 공방/자가 활동·현물 연구/발명. 실험이 실제 새로운 지식을 만들면 `KnowledgeEvent` 가능 |

## T2-2. 객체와 원본 경계

| 객체 | 필드·정본 책임 |
|---|---|
| `ResearchProject` | `project_id, sponsor_refs[], performer_refs[], laboratory_site_id?, target_tech_or_domain, research_mode, milestone_rules, required_evidence, actual_work_event_refs[], start/end_month, disclosure_policy, status, revision`; KnowledgeRegistry가 연구상태/프로젝트를 관리 |
| `ResearchMilestone` | `milestone_id, project_id, objective, prerequisites, required_validated_work_by_kind, evidence_test_refs[], due_rule, acceptance_rule, research_result_ref?`. 목표 조건은 실제 성공보장 아닌 계약 |
| `ResearchWorkEvent` | `event_id, month, research_project_id, performer_site_or_own_account_ref, research_work_units, researcher_assignment_refs[], laboratory_capital_service_refs[], actual_inputs[], validation_observation_ref?, failure_or_partial_result, recipe/ruleset_hash` |
| `ExperimentRecord` | `experiment_id, project_id, expected_hypothesis, actual_input_journals, observed_result, quality/evidence, reproducibility_rule, safety_refs, provenance` |
| `ResearchResultCandidate` | `project_id, tech_id/new_discovery_proposal, evidence_refs[], successful_or_inconclusive, proposer, review_rule, source_event_refs, claimed_effective_month`; 실제 `KnowledgeState`와 별개 |
| `KnowledgeValidationEvent` | `holder_ref, tech_id, prior_state, proposed_state, supporting_research_or_transfer_events, verdict, effective_month, reviewer_authority/verification_rule, version`. D-T1 정본에 반영되는 승인/기록 |
| `ResearchFundingPlan` | `research_project_id, funder_refs[], amount/currency, schedule, executed_loan/grant/reservation_refs[], contractual_claims`; 별도 현금 소유하지 않음 |

시설·고용·교육·의료/부상은 C-E1/POP, 실제 실험재료는 C-E2 `InventoryLedger`, 실험 장비는 C-E1 `CapitalAsset`, 예산·보조금/급여·계약·연구비는 C-E3/E4 `Financial/FiscalLedger`가 유일한 원본.

## T2-3. 실제 연구 공급량 계산과 노동배정

`stage1`에 연구 주체의 예산/자금·실험 목표·자료 접근/연구계약, 실험 장비·기술·인력 채용 계획을 검증하고 실제 잔액·실행가능 신용/보조금만 예약한다.

`stage2`에 `EmploymentLedger`의 적격 노동·직무/교육·POP 건강/복무/유효 노동법·필수 편의제공을 평가해 실제 연구인력을 배정한다. 연구자·교사·의사·군인·공장근로자 같은 사람을 동일한 주된 노동 계약에서 별도 인구로 만들어 여러 기관에 중복 고용하지 않는다.

`stage3`에 실험재·연구용 서비스·연료 등 실제 인도와 설비/운송·서비스 예약 확인. 실험재 미도착 또는 연구실 미설치라면 연구량 상한에 반영하고 `stage4`에서 부적법한 연구를 생성하지 않는다.

`ResearchWorkEvent`의 실효 연구서비스 상한:

~~~text
research_work_feasible = min(
    planned_and_funded_research_work,
    qualified_research_labor_service,
    usable_lab_and_equipment_service,
    delivered_experiment_material_or_data_access_service,
    authorization_and_safety_service_capacity,
    available_technique_and_method_prerequisites
)
realized_research_work = round_down_to_research_lot(research_work_feasible)
~~~

- 모든 항목은 연구종류별 같은 고정소수점 작업단위로 **검증된 변환계수**를 사용한다. 투입이 필요 없는 항목은 무조건 0으로 처리하지 않으며 실제 요구조건 있는 항목만 병목으로 평가.
- `research_work_units`는 **이미 소비된 인력/재료/장비 서비스를 증빙하는 비저장 활동 실적**이며 은행 화폐·국가 연구점수·상품 재고가 아니다. 비사용 연구서비스 용량을 차월에 이월하여 무료 성과 생성 금지.
- 기초·응용·개발 연구가 공유 실험실/연구자/장비를 사용하면 다른 기업·산업/군사/건설과 동일 capacity·인원·현금 예약 테이블에서 경합한다.
- 실제 위험(실험사고, 연구장비 손상, 재료 폐기/폐수)은 `Inventory/Asset` 및 POP 건강·D-I2 오염/안전/`stage10` 사건과 연결. 연구 실패도 투입재·급여는 이미 지불/소모됐을 수 있다.

## T2-4. 검증·성공/실패·연구 진척

- 연구의 **투입실적**과 **지식상태 변화**는 별개. 성공·부분 성공·무성과·오류/재시험·우발적 발견/상호 관련 기술 성과를 실제 `ResearchWorkEvent/ExperimentRecord`와 후속 `KnowledgeValidationEvent`로 기록.
- 진척도는 동일 프로젝트에서 실제 검증된 `ResearchWorkEvent` 집계, `milestone/tech`별 검증 결과에 따라 산출하는 조회값. 중간 진척이 있다고 보유자의 `KnowledgeState`를 자동 `usable`로 변경하지 않는다.
- 성공/발견 확률(`P`)이 있는 규칙은 `P = bounded_fixed_point(rule_version, validated_work_evidence, relevant_knowledge, method_risk, personnel_skill, instrumentation_quality, objective_complexity)` 등의 **명시적 함수**와 `RNG(world_seed,month,stage,project_id,event_kind,draw_index)`를 사용. 계수는 Ruleset/출처에 보존. 동일 실행 입력에서 재현되어야 한다.
- `stage4`에 연구서비스/실험기록을 만들고 `stage8` 원가·지급/외상·정부보조금 결산, `stage9a`에서 이미 확정된 검증 증거로 `KnowledgeValidationEvent/ResearchProjectStatus`를 확정/예약한다. `m` 성과의 새로운 지식 효력은 **원칙적 m+1 stage0**.
- 같은 `stage9a`의 다른 연구 프로젝트가 새 지식을 즉시 소비하여 연쇄적인 무한 `research_complete`를 만들면 안 된다. 의존 프로젝트가 새로운 결과를 이용할 수 있는 최초 시점은 그 결과가 실제 적용되는 다음 달 `stage0`.
- 기존 연구 결과를 재검토해 반증·수정하거나 실패 원인을 밝혀 지식의 유효상태·실용화 가능성을 바꿀 수 있으나, 과거에 실제 실험한 자원/임금을 환불하거나 이미 제조/인도된 상품을 소급 재계산하지 않는다.
- 연구예산 부족이나 대학/기업의 파산은 합법적 `paused/terminated` 연구 상태를 만든다. 연구성과 문서·기술자/계약·설비·장부/차입을 자동 삭제하지 않음.

## T2-5. 공동연구·재정/권리

- 연구주체(`performer`)와 출자자(`sponsor`), 연구결과의 저작/특허권자·기술 사용권자는 서로 다를 수 있다. `ResearchContract`는 공동 연구·위탁 연구·군사 비밀 연구, 공개·성과귀속/로열티·지식 이전 조건을 유효 정치법/소유권과 참조한다.
- 정부 `R&D` 지출은 실제 인력·시설/연구용 물품·서비스 공급에 쓰이며, 예산 승인/연구지원계획만으로 `KnowledgeValidationEvent`가 확정되지 않는다. `FiscalLedger`의 보조금 의무와 `FinancialLedger` 실지급은 별개.
- 연구원 고용/급여·공공 대학·연구비 인건비는 생산/가계소득/정부 예산에서 **한 번만** 인식. 자금 조달로 은행 신용/예금이 생길 수 있으나 근거 없는 `innovation_money`라는 독립 화폐 원본은 없음.
- 연구성과의 회계 처리는 적격 비용과 자본화된 무형자산을 회계 정책/법적 권리에 따라 구분. `R&D expenditure`, `KnowledgeState`, `OwnershipLedger.`intangible_right`는 별개이며 연구비가 무조건 특허가격/현금을 창출하지 않는다.

## T2-6. 인수 검증

- 연구계획 100, 실제 가용 연구원·시설/재료가 각 5/3/4 작업단위라면 실효 연구량은 최대 3; `ResearchWorkEvent`가 실제 자재/노무/금융·실험 근거를 참조.
- 연구기간 종료·예상 목표치·시설 건설만으로 공짜 연구성과 생성 거부.
- 단계4 연구 결과는 9a 검증 뒤 차월부터 영향, 월 m 기존 공장 가동/임금/전투 결과 소급 금지.
- 같은 연구원/실험설비·계좌를 군수/의료/공업 연구와 중복 예약 시 실제 인력·장비/자금 한도 준수.
- 공동 연구에서 계약자·연구 수행기관·권리자·수혜기관을 구별, 연구비 집행·공공지원/특허 자산 중복 계상 없음.

**세부 시험:** `DT-T10..T18`. [기술 인수시험](TECHNOLOGY_ACCEPTANCE_TESTS.md).

## 참고

[OECD Frascati Manual 2015](https://doi.org/10.1787/9789264239012-en)·[OECD Oslo Manual 2018](https://www.oecd.org/en/publications/oslo-manual-2018_9789264304604-en.html)의 R&D와 혁신활동/실제 적용의 정의 참고. 통계상 경계를 게임 속 연구방식의 필수 역사조건으로 과도하게 소급하지 않는다.
