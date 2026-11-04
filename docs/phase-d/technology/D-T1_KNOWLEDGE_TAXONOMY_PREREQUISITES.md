# VIC2.5 D-T1 — 기술·지식 분류, 선행관계, 보유 상태

> **설계 확정 / C++ 구현·시험 미실행** · `phase-d-technology/1.0` · 2026-11-04  
> 상위: [D-D0](../D-D0_COMMON_CONTRACT.md), POP-D0~D3, C-D0/C-E1~E5/C-P1~P5/C-M1~M5/C-D4, [산업 I3](../industry/D-I3_CAPITAL_AUTOMATION_PRODUCTIVITY.md). [기술 색인](README.md)

## T1-1. 개념과 정본 소유

| 개념 | 정본·유효 의미 |
|---|---|
| `TechnologyDefinition` | `tech_id, display_labels, discovery_metadata, domain_tags[], prerequisite_expression, verification_rule, knowledge_difficulty_class, data_revision`. 버전 있는 `TechniqueCatalog`/기술 규칙 원본 |
| `TechniqueDefinition` | 기술을 실제 공정/설비·작업방법으로 사용하는 구현규칙; C-E1 `ProductionRecipe`/D-I3의 실제 인력·자재·설비 요구·공정과 참조 |
| `KnowledgeState` | `holder_ref, tech_id, status, evidence_event_refs, available_from_month, access_scope, revision`; `KnowledgeRegistry`에 속한 **주체별 지식 상태**. 인구·설비·소유권 원본 아님 |
| `ResearchProject` | `project_id, performer_ref, funder_refs[], target_tech_refs, research_mode, lab_site_ref, milestone_refs, risk_rule_ref, start_month, state, revision`. 누적 연구량은 실제 사건에서 도출 |
| `ResearchWorkEvent` | 실제 연구생산·실험성과/소모·불확실성의 유효 사건; 인력·재고·금융/시설을 참조 |
| `KnowledgeTransferEvent` | 수신자·발신자, 이전/접촉·자료/교육·이해 검증, 권리/비밀제약을 기록. 원본 소유자가 ‘지식량’을 잃는 재고 이전이 아님 |
| `SiteTechniqueAssignment` | `site_id, technique_id, status, capital/qualification/license_refs, conversion_project_ref, effective_period, source_event_id`. 시설별 도입/적용 관계 |
| `Skill/Qualification` | POP/교육 분류·자격/훈련 결과가 유일한 정본; `KnowledgeRegistry`에 사람별 별도 숙련 인구를 생성하지 않음 |
| `Patent/License/KnowHowRight` | C-P 유효 지식재산법·계약, `OwnershipLedger` 사용/실시·권리, `FinancialLedger` 로열티/대금 정본 |

**정본 원칙:** 국가의 '기술 레벨'은 다양한 연구기관·기업/가계·공공기관의 유효 접근 가능 지식과 현장 채택에서 **파생하는 통계**다. 국가나 지역에 `technology_bonus` 또는 별도 쓰기 가능한 `research_progress`를 저장하지 않는다. 1개 발명이 국가 전체 사업장의 설비·임금·사람 수를 임의 변경하지 않는다.

## T1-2. 기술 분류

기술은 **단일 고정 연구나무** 대신 복수 태그를 가지는 지식·실용기술 그래프로 관리한다.

| 태그 | 기술 예시(실제 데이터/연대 계수는 후속) | 연결 분야 |
|---|---|---|
| `AGRICULTURE` | 관개·작물개량·농기구·토양 관리 | D-I2 토지·작황, POP 자급경제 |
| `MATERIALS` | 야금·합금·건재·화학공정 | D-I2 레시피·D-C 공법 |
| `POWER` | 동력전환·기계·발전·연료 효율 | C-E1 설비·D-I2 에너지 |
| `TRANSPORT` | 선박·철도·항만·도로·통신 | C-E5 실물 수송/서비스 |
| `MEDICINE` | 위생·진료·재활·제약 제조 | POP 건강·C-E4 실제 보건서비스 |
| `EDUCATION` | 교육법·교육도구·정보전달 | POP 교육/숙련·C-E4 |
| `ORGANIZATION` | 생산조직·측정·회계/행정 기법 | C-E1 금융/기업·C-P 기관 |
| `MILITARY` | 무장·훈련·지휘·보급·공학 | C-M 실제 편제/장비·훈련/군수 |
| `SCIENCE` | 자연/수학/사회 지식·실험방법 | 타 기술의 선행지식·실험 가능성 |
| `CROSS_DOMAIN` | 산업·민간·군사 등 복수 적용 | 기능 경계마다 별도 검증 |

세부 태그는 추가 가능하며 분류 태그가 곧 출시연대·법적 허가나 모든 시대에 있는 대학/전력망을 의미하지 않는다.

## T1-3. 선행 조건과 지식 그래프

`PrerequisiteExpression`은 `ALL(known techs/empirical conditions)`, `ANY(alternatives)`, `THRESHOLD(n of listed)`, `SITE_REQUIREMENTS` 등 **유형이 있는 선언적 조건**으로 정의한다.

- 기술관계 `knowledge_prerequisites`는 유효 버전 내 순환을 금지하는 DAG를 기본으로 한다. 여러 독립적인 개발 경로가 있을 때 OR/alternative를 허용하되, 기술 A/B가 무조건 상호 선행이라는 순환은 거부.
- **연구 선행조건**과 **실용화/현장 도입 조건**을 다르게 기록한다. 지식에 접근했어도 공장·증기보일러·기술자가 없으면 새 공정 가동 불가; 한 공장에 장비가 있어도 기업의 지식·면허/숙련 자동 생성 금지.
- 역사 초기 시나리오는 1801년 등 시점·지역별 `initial_knowledge_evidence`와 출처, 미상/추정/확정(또는 무효)을 구분한다. 과거 지식 전파와 공식 국가권력의 승인/법적 사용권은 분리.
- 어떤 문화·지역이 특정 기술을 알았는지를 국가 하나에 기계적으로 환원하지 않는다. 교역/수입/이민·비공식 장인 활동과 국가 연구기관의 지식 보유는 별개의 사실이며 동시 공존.
- 동일 사실에 여러 이름이 있거나 기술 분류가 분할/병합되어도 `TechId` 재사용 금지. `KnowledgeLineage/ClassificationCrosswalk`로 버전·변천과 조건 변경을 추적. 전체 기술 레벨을 단순 1→2→3으로 강제하지 않는다.

## T1-4. 보유 주체·접근 상태

`KnowledgeHolderRef`는 기존 typed `Country/Government/Institution/Firm/OwnAccountActivity` 등 유효 주체의 참조 래퍼로 정의한다. 국가 통계 집계가 주체별 구체적 접근·보유 상태를 덮어쓰지 않는다. 지식/사업장/가구/군사 자료는 관리권한·공개여부에 따라 접근 가능 범위를 계산한다.

상태는 D-D0에 지정된 `unknown → observed → understood → usable`를 **한 주체가 가진 해당 버전 지식의 근거 수준**으로 사용:
- `unknown`: 유효 지식근거 없음. '실제 역사에 존재하지 않는다'와 구별.
- `observed`: 공개 문헌·현물/강의·장치 등으로 실존/원리를 접했으나 정확한 재현/활용 불충분.
- `understood`: 유효 실험·학습/이전을 통해 원리·방법 이해. 반드시 사용 가능한 설비/상용제품은 아님.
- `usable`: 규칙상 기술적 재현가능성/실용화 근거를 갖는 지식. 개별 사업장의 `SiteTechniqueAssignment.active`와 같지 않음.

`KnowledgeState`는 해당 주체의 유효 상태·근거 이벤트를 기록하며 **상태 중복 쌍/동일 이벤트 재적용 금지**. 프로젝트별 실제 연구량/확률은 `ResearchWorkEvent`로부터 파생, 별도 writable `progress_stock` 없음.

## T1-5. 시간·정합성

- 월 `m` stage4 연구/현장관측 실적, `stage9a` 지식 검증·기술 공개/이전 예약을 서로 분리. 새 KnowledgeState/전파의 적용은 기본적으로 `m+1 stage0`. 기술확정만으로 `m stage4`의 이미 생산된 상품/임금 소급 변경 불가.
- 유효기간(`valid_from_month, valid_until_month`), provenance·version, 지식수신자/권리·공정 접근을 검증. 하나의 `TechnologyDefinition`이 여러 생산 레시피·농업·군사·보건에 적용 가능해도 각 영역에는 실제 자원·허가/시설 조건 필요.
- 지식은 양도가 가능한 상품 재고와 달리 여러 사람이 알 수 있는 비경합 정보일 수 있다. `KnowledgeTransferEvent`가 송신자의 `KnowledgeState`를 자동 차감하지 않음. 하지만 유효 전파·교육/매체·접근 경로/권리가 없으면 모든 국가에 자동 도달하지 않음.
- 복제·공개/비밀과 **사용을 허용하는 법적 권리**는 분리: 공개 지식이어도 법령상 특정 상업적 사용이 제한될 수 있고, 법적 실시권이 있어도 실제 기술이해/설비·노동력이 없으면 가동 불가.

## T1-6. 인수 조건

- 기술선행 DAG에 순환/미존재 참조/효력 역전이 있으면 규칙 버전 검증 거부.
- 국가 A의 연구기관이 `usable`이어도 같은 국가의 외부 민간기업/사업장이 자동 `usable/active`가 되지 않음.
- 한 기술을 복수 기관이 독립 연구하거나 외부 공개/교육으로 접근해도 인구/화폐·장비 원본 생성이 없고 lineage 유지.
- 지식 상태·가동 설비·POP 직무자격·특허권을 분리하고 월 m→m+1 시간경계를 준수.

**세부 시험:** `DT-T01..T09`. [기술 인수시험](TECHNOLOGY_ACCEPTANCE_TESTS.md)

## 참고 자료

- [Victoria 3 Dev Diary 기술·혁신 설명](https://store.steampowered.com/news/posts/?enddate=1639070046&feed=steam_community_announcements): 연구·기술확산 문제영역 비교용, 국가 혁신점수/3종 기술트리·수치 복제 금지.
- [OECD Oslo Manual 2018](https://www.oecd.org/en/publications/oslo-manual-2018_9789264304604-en.html): 기술 발명과 실제 활용(혁신)의 구별, 지식 전달의 비용·수용 역량 개념.
