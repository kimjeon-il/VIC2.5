# VIC2.5 — 사건·결단·국가형성·일지 통합 설계 (Victoria 3 v1.15.0 기준)

- 상태: **설계 확정안 / 코드 미구현 / 컴파일·검사 미실시**
- 기준: Victoria 3 1.15.0 'Ivan-Chai/Fireweed' (2026-11-05), 2026-11-08 조사
- 작업 위치: 기존 `work/politics` 브랜치, 신규 브랜치 금지
- 대상: 일간 VIC2.5 Phase B/C/D/E/G/H 및 기존 C-P1~P5, P5-G 유격전
- 목표: 선택형 역사 사건·능동 결단·국가형성·장기 일지를 **공통 콘텐츠 기반 하나**로 구현. 새로운 경제·POP·군사·영토 정본을 만들지 않음.

## A. 참고한 빅토리아 3 v1.15의 실제 구성

1. Events: 조건/사유별 사건과 선택지, 국가·인물·지역 범위, 역사 콘텐츠; 1.15에 `fire_only_once`, `fire_only_once_globally` 이벤트 플래그 추가.
2. Decisions: 표출 조건/실행 조건/효과가 구별되는 국가별 능동 선택. 복구 가능하거나 재사용 불가한 결단은 콘텐츠 규칙으로 지정.
3. Country Formation: 일반 국가형성과 대통일(Grand Unification) 분리. 문화, 기술, 게임규칙, 영토 및 필요한 비율 등 개별 조건. 통일 지지·경쟁·전쟁은 별도 기제와 연동.
4. Journal Entries: 활성화·진행·기한·변경 효과·다양한 종료경로. 1.15는 고정된 완료/실패 이분법 대신 **각각 트리거와 효과가 있는 임의 개수의 스크립트 결말(outcome)**, 가변 배율의 활성 효과, 사용자 정의 진행 막대·지도 모드, 세계공유 일지를 지원.
5. 1.15 UI: 목표 상단 배치, 테마 그룹, 검색·필터, 활성화/진행/종료 이유 표시. 전용 지도 모드는 필요한 일지에만 사용.
6. 일부 국별 콘텐츠·상호작용은 DLC 소유 여부가 다른 실제 게임의 구분이나 VIC2.5에서는 DLC 수익모델을 모방하지 않는다. `content_pack` 메타데이터로 모드/시나리오 확장을 지원하는 정도로 한정.

참조:
- https://steamdb.info/patchnotes/26453692/ (1.15 출시 및 전체 변경 사항)
- https://steamcommunity.com/app/529340?l=koreana&snr=2_groupannouncements_detail_ (Dev Diary #194, 저널의 스크립트 결과·지도 모드)
- https://vic3tabula.com/en/events (1.15 이벤트 정의 추출)
- https://vic3tabula.com/en/decisions (1.15 결단 정의 추출)
- https://vic3tabula.com/en/formations (1.15 국가형성 정의 추출)
- https://vic3tabula.com/en/journal-entries (1.15 일지 정의 추출)

여기서 파라미터/제약/스키마는 VIC2.5 제안사항이며 빅토리아 3의 정확한 내부 코드를 복제한 것이 아니다.

## B. 네 시스템의 경계

| 종류 | 시작 주체 | 수명 | 데이터 정본 | 기능 |
|---|---|---|---|---|
| Event | 엔진의 on_action/날짜/조건/확률, 일지·결단의 후속 | 선택 완료 후 종료 | 콘텐츠 발생 이력만 | 단기 사건의 선택과 결과 |
| Decision | 플레이어 또는 국가 AI | 선택 시점 | 사용/재사용 이력만 | 능동적 국가 행동 |
| Formation | 국가형성 자격·통일 후보·플레이어·국가 AI | 실행 시점 | 자격·형성 이력만 | 기존 국가 ID/주권·행정·외교의 원자적 재편 |
| Journal | 일지 트리거/선행 콘텐츠/세계 조건 | 여러 날 이상 | 일지 목표·진행·결말 이력만 | 장기 목표·위기·지역/세계 상황 추적 |

공통 `TriggerEvaluator`, `EffectPlanner`, `ScopeResolver`, `ContentScheduler`, `ContentHistory`만 별도 구현한다. 네 종류별로 새 이벤트버스, 시간 엔진, 자원 원장, RNG, 전투 결과 엔진을 만들지 않는다.

개념 계층:
```
규칙 파일(JSON) ───> 로드 시 검증·컴파일 ───> ContentRegistry
                                         ├── Event
                                         ├── Decision
                                         ├── Formation
                                         └── Journal
공통 WorldReadSnapshot ──> TriggerEvaluator ──> ContentScheduler
                                              └── EffectPlanner
                                                     └── 기존 C/D/E/G/H Stage 변경·검증·원자 커밋
```

## C. 공통 스크립트 콘텐츠 계약

### C1. Definition (읽기 전용)

- 공통 필드: `id`, `schema_version`, `kind`, `title_key`, `description_key`, `scope_kind`, `tags`, `source_pack`, `sort_priority`.
- 범위: `country`, `region`, `province`, `character`, `movement`, `world`, `journal_instance`. 동일 객체는 기존 타입 ID로 연결. 저장된 엔터티와 UI 문자열을 혼동하지 않음.
- 조건 AST: `all`, `any`, `not`, 비교(`eq/ge/le/gt/lt`), `exists`, `has_law`, `law_amendment`, `controls_province`, `has_country`, `movement_pressure`, `government`, `owner_or_subject`, `war_status`, `technology`, `date_between`, `historical_flag`, `journal_status`, `event_recorded`, `has_available_resource`.
- 모든 참조는 정적 타입/범위 검증. 지원하지 않는 엔진 속성은 `Unsupported` 또는 로드시 오류, 임의로 0을 반환하지 않음. 조건 판정은 부작용 없음.
- 효과 AST: `enqueue_law_bill`, `start_policy_process`, `enqueue_diplomatic_action`, `schedule_territory_transfer`, `form_country`, `start_or_update_crisis`, `activate_journal`, `advance_journal_milestone`, `schedule_event`, `apply_timed_modifier`, `adjust_existing_political_attitude`, `reserve_and_spend_budget`, `log_historical_fact`. 모든 효과는 기존 원본 계정·명령으로 라우팅.
- `effect_preconditions`, `issuer_authority`, `expected_revisions`, `effective_day`, `transaction_id`, `source_content_instance_id`를 기록. 불가능한 효과는 원자적으로 거절하고 사용자·AI가 이유를 조회하도록 한다.
- 한 번 실행된 효과를 중복 재생하지 않도록 `command_id`·`content_occurrence_id`를 멱등 키로 사용.
- 이름·국기·설명은 별도 localization 및 기존 국가정의/이력 시스템에서 조회.

### C2. 실제 자원·물리 상태 우선

- 돈을 비용 없이 생성/차감하지 않는다: 재정·기업·가계 원장에 근거한 실제 이전/지출.
- 인구는 POP 원본 참조만, 사건/결단/형성으로 독립 `people`를 생성하지 않음.
- 전투/피해는 C-M 전투 호출 또는 검증된 결과 참조; 무장 병력 복제 금지.
- 영토·주권은 Phase E/Phase C 명령으로, 조약·군사 점령·군사통제와 법적 주권 분리.
- P2 법률과 증보는 법률 절차에 따라 시행; 콘텐츠는 관련 법안/지지/저항을 시작할 수 있으나 법률 집행을 근거 없이 즉시 확정하지 않음. 법을 직접 바꾸는 예외적 합의는 별도 권한·법적 근거 필요.

### C3. 스크립트 형식

- 외부 JSON 규칙 파일을 기본으로 하되, 제한된 정적 DSL(AST)을 사용한다. C++ 자유 함수·임의 JavaScript 실행·재귀 스크립트 호출 금지.
- 로드 시 `content_id` 중복, 필수 locale 키, 참조 범위, 직접 순환 연결, 불가능한 효과 경로를 점검.
- 조건 간섭을 줄이기 위해 `date`, `country`, `law`, `movement`, `province`, `technology`, `war`, `journal` 의존성으로 정적 역색인을 만든다.
- 원칙: 콘텐츠의 간단한 숫자 보너스도 `modifier`의 발효·만료일·근거를 추적하며 기존 산식 입력에만 적용.

## D. 사건(Event)

### D1. 유형

1. **즉시 사건**: on_action에 반응. 법률 통과, 전쟁 개전/종료, 정변, 정부 교체, 국가형성, 특정 연구 완료 등.
2. **조건·확률 사건**: 후보 객체가 자격 조건을 만족할 때 설정된 hazard/가중치로 생성. 가능한 경우 일별 전수 조회 대신 조건 변경/예정일 큐.
3. **역사/국가 특수 사건**: 실제 날짜는 허용 기간(earliest/latest)의 힌트이며, 실제 국가·법·인물·영토 조건이 충족되지 않으면 강제 발생 금지. 대체 역사 경로 유지.
4. **연쇄·일지 사건**: 선행 사건이나 일지 진행 단계에서 예약. 미완성 선택지로 무한 연쇄 금지.
5. **알림 전용 사건**: 실질적인 사용자의 선택이 없다면 팝업 대신 정보알림으로 처리.

### D2. EventDefinition 필드

`trigger_sources[]`(`on_action`, `due_day`, `mean_time`, `scripted`), `show_if`, `fire_if`, `fire_only_once`, `fire_only_once_globally`, `cooldown_days`, `min_days_after_prior`, `weight`, `priority`, `options[]`, `fallback_option`, `expiry_days`, `scoped_targets`, `followups`.

선택지: `id`, `visible_if`, `available_if`, `effects`, `ai_weight`, `effect_preview`; 필요시 `chance_outcomes` (고정소수점 weights의 합을 검증). 사용자 선택은 게임 중 지연할 수 있지만 세이브 후 선택 가능해야 한다.

`fire_only_once`의 소유 단위(국가/인물/범위)와 `fire_only_once_globally`의 전역 발동 상태를 분리한다. 중복 방지 상태는 해당 콘텐츠 정의의 버전과 함께 저장.

확률: `hash(world_seed, day, content_id, scope_id, attempt_index)` 기반 결정론적 샘플. 독립 신호를 확률식으로 중복 누적하여 특정 국가에 과도한 팝업이 생기지 않도록 방지. 중요 사건을 우선 표시하고 사소한 사건은 알림 묶음.

## E. 결단(Decision)

### E1. 정의

- `visible_if`: UI 목록에 표시할 조건. 역사·국가·법·기술에 따라 잠긴 결단도 필요 시 표기.
- `available_if`: 현재 실행 허용 조건. 이유/현재치/요구치 출력.
- `cost`: 실제 지출·행정력/정치력 등 **기존** 개념이 있을 때만 사용. 새 결단 전용 마나 생성 금지.
- `effect`: 선택 즉시 명령/기한/시작 시뮬레이션을 예약. 예산/물자 부족, 법적 권한 부족 등으로 실패하면 상태 변경 없음.
- `once`(`country`/`world`/`lineage`), `cooldown`, `repeats`, `expiry`, `ai_weight`, `linked_journal` 선택 사항.
- `DecisionInstanceHistory`: 누가/언제/무엇을/어느 효과로 집행했는지만 보존.
- 결단은 법률을 우회하는 치트 명령이 아니라 **가능한 법적·외교적 행동을 개시하는 국가의 능동적 선택**이다. 예: 해방 정책 추진, 전국적인 기념행사, 특별 조사, 영토 간척 프로젝트, 특정 근대화계획 시작.

### E2. AI·UI

- AI는 유효 결단 후보 중 전략 목표·예산·정치적 비용·기대결과를 기준으로 선택. 매일 같은 결단을 반복 평가하지 않고 조건/자금/쿨다운 변경 때 재평가.
- UI 목록은 `실행 가능`/`잠김(조건표시)`/`완료됨`을 구분하고, 실행 결과 및 부작용·발효시점을 보여준다.
- AI가 처리한 결단도 플레이어와 동일한 검증/실행 경로 사용.

## F. 국가형성(Country Formation)

### F1. 빅토리아3 기반 분류

1. **일반 국가형성**: 관련 문화·정부·필요 영토 점유 비율·기술·국제 지위·배타적 형성 규칙 충족 시, 사용자가 능동 실행.
2. **대통일**: 독일·이탈리아 등 복수 국가의 후보/지지/반대·외교·전쟁이 맞물리는 별도 진행 과정. 통일 후보가 존재해도 자동으로 상대국의 주권을 획득하지 않음.
3. **일지 연계 형성**: 특수 역사·정치 과정은 일지의 특정 결말로 자격을 부여하거나 국가형성 결단을 해금.
4. **독립·분리**: 기존 Phase E의 `queue_independence`와 P5/H를 활용하되, 형성 가능한 역사적 통일과 별개 종류로 분류.

### F2. CountryFormationDefinition

`id`, `target_country_definition_id`, `kind`(`standard`/`major_unification`/`journal_gated`), `eligible_country_conditions`, `eligible_cultures`, `tech_requirements`, `allowed_time_range`, `minimum_rank`, `required_regions[]`, `territory_threshold`, `must_control_regions[]`, `subject_counting_policy`, `supporter_counting_policy`, `rivals_excluded`, `candidate_rules`, `blocked_by`, `formation_effects`, `ai_score`.

- 기준 영토 집합은 특정 **게임 시대와 지도 데이터의 안정된 Region/Province ID**로 표현. 필요 비율은 정의별이고 하드코딩하지 않음.
- 점령·주장(claim)만으로 법적 영유 판단을 대체하지 않음. 허용된 종속국/통일 지지국 영토 산입은 명시적으로만.
- 형성 후보의 우열: 외교적 지지국·군사력·국제 위상·국내 통일운동 지지·기술/경제력 등 기존 실제값을 읽음. 소속/지원은 외교/정치 관계 사건으로 기록.
- 조건 충족 이후 통일 결정은 해당 관계/선행 조약/군사 합의가 실효된 뒤에만 가능.

### F3. 국가 정체성·ID 정책

- `continuity_rebrand`: 기존 국가의 법인격이 계속되는 통일/국명 변경. **CountryId 보존**, 이름·국기·통일 정의 등 국가 정체성/속성 이력 갱신. 단순 국명/국체 변화에 신규 국가 생성 금지.
- `successor_new_country`: 법적으로 새 연합 법인격이 성립. 신규 ID를 만들고 전신국의 계보를 보존하며, 영토·재산·채무·조약·군·시민권·공직의 각 영역별 승계를 따로 계획.
- `independence_new`: 독립신설국. 기존 E의 원자적 신설 절차를 통해 새 정부·행정/주권을 시작. 기존 POP/국고/채무 자동 복제 금지.
- 실제 C++에 국가 문자열 키를 사용하는 구 경로가 존재하므로, 안정 CountryId 도입/연결 방식은 구현 시 migration 대상으로 명시하고 이번 설계 문서만으로 키를 무리하게 치환하지 않음.
- 통일에 참여하지 않은 국가의 잔여 영토와 역사적 계보를 임의 소멸시키지 않는다.

### F4. FormationPlan 원자 처리

사전 `preview`는 대상 국가·지역·후보·동의/강제 근거·재정·부채/외교/군대/정치적 위험까지 보여준다.

`FormationPlan` (issuer, target, mode, affected_countries, provinces, consent/treaties/war_basis, future_country_identity, successor_plan, date, expected_revisions) ->
1) 외교/주권/관할·국가 생성 가능성 검사
2) 정치 권한·정부/법률·정체성 계획
3) 기존 E 주권/행정 사건 (영역별 기록)
4) H 조약/종속·G 경제 채무·C 군 지휘·B 시민권 등 필요한 명시적 승계
5) 단일 StagingWorld 검증·원자 커밋/롤백
6) 성공 시 이벤트/결단/일지 결과 예약, AI 및 지도 인덱스 국소 무효화

일부 승계 계약이 구현되지 않은 경우 `Unsupported`로 **형성 실행을 거절**; 국가가 형성되었지만 인구·외교·군대 원장이 깨진 부분 완료는 금지.

## G. 일지(Journal Entries) — 1.15 방식 핵심

### G1. JournalDefinition

`id`, `scope_kind`(`country`/`world`/`region`), `group_id`, `parent_id?`, `priority`, `activate_if`, `show_if`, `invalidate_if`, `active_modifiers[]`, `objectives[]`, `progress[]`, `milestones[]`, `outcomes[]`, `timeout?`, `map_overlay?`, `linked_events[]`, `linked_decisions[]`.

- 목표(`objective`)와 진행도(`progress`)를 별도 표현; 예: 상위 목표 = 체제 개혁, 하위 진행도 = 실제 법률/이익집단/재정 상태.
- 동적 진행도 `min/max/current`는 기존 시뮬레이션에서 파생하거나, 실제 검증된 이력 누적 수치로 유지. 진행 막대는 표현이지 독립 리소스가 아님.
- `active_modifiers`에는 데이터 상태에 따라 강도가 변하는 항목을 허용; 법 집행력·정부 비용·외교 평가 등 기존 도메인 유효 파생 입력에만 적용. 즉시 인구/돈 생성 금지.
- `milestone`: 25/50/75 같은 고정 단계만 강제하지 않음. 조건을 충족했을 때 등록하는 명시된 이정표.
- `timeout`: 절대일/활성화 후 N일/실제 전쟁의 종결 등 별도 기준. 시계·조건 조회가 가능한 상태에서만 만료.
- `outcomes[]`: **임의 개수의 결과** {id,label,description,trigger,priority,effects,next_journal,visual}; 성공/실패 단정 금지. 결과 후보가 여럿이면 명시적 우선순위 후 안정 ID 정렬로 1개 결과를 원자적으로 확정. 다중결과를 의도할 때만 `allow_composite_outcome`와 상호배타 안전 검사.
- `invalidate_if`는 실패를 강제하는 조건이 아니라 해당 일지 적용 맥락의 상실. 결과의 하나로서 `terminated`를 정의할 수도 있고, 미종료 시 명시적 중단 사유 기록.
- 전개 중 종료 후에도 과거 발생·결과·기간을 조회할 수 있게 저장한다.

### G2. 세계 일지(Global Journal)

- `scope_kind=world`: **하나의 WorldJournalInstance**에 세계 공통 진행도/참가조건/결말이 존재. 각 참가국에는 해당 인스턴스의 `CountryParticipationView`를 파생 제공한다.
- 세계 일지 참가/탈퇴는 활성 상태를 자동 복제하지 않음. 국가별 기여만 실제 도메인 이력으로 집계.
- 예: 국제 혁명 확산, 기근 충격, 여러 국가가 연계된 통일/지역 위기. 빅토리아 3 1.15의 `Reform or Revolution`, 연방 결성 등 세계 일지에 대응.
- 1일에 참가국이 N개라도 같은 세계 일지의 결과를 N회 중복 실행하지 않음.

### G3. 지도·UI·일지 그룹

- 상단: 고정 목표, 다음 이정표, 남은 기한(있는 경우), 진행 조건/결과·차단 사유.
- 본문: 주제별 그룹(정치/산업/외교/군사/국가 형성/사회), 상위 일지 강조, 관련 하위일지.
- 일지 목록: 검색, 필터(진행/예정/종료·주제·국가·지역), 알림핀, 결과 이력.
- 전용 지도 표현은 **조회 시** 해당 일지에서 관련 프로빈스 색/표시 값을 반환. 전역 지도를 매일 새로 구축하는 별도 캐시 금지.
- 수치 변화의 원인을 설명하는 로그: `progress_source_event_ids` + 실제 도메인 파생 정보. UI에 임의 허위 진행률을 표시하지 않는다.
- 긴 효과 툴팁은 단계별/접힌 상세로 표시. 플레이 중 추가 클릭 부담 최소화.

### G4. 기존 정치위기/유격전과의 연결

- `P5Crisis`: 실제 정치 위기 정본. 이를 일지로 보여주는 `PoliticalCrisisJournalAdapter`를 둔다.
- `P5GuerrillaCampaign`: 지역 활동·실병력·탄약·교전 원본. `GuerrillaJournalAdapter`가 조직화→활동→보급고갈→정규전/휴전/종결의 일지를 표시.
- **기존 P5/P5-G 네이티브 상태를 새 JournalInstance로 복제하거나 별도 진압/전투 엔진을 만들지 않음.**
- 군사정변 가능 결과(정확한 현황에 근거): `preempted`(동원조직 해체), `cabinet_overthrow`(수도 실제 통제/공직 승계), `negotiated`(유효 법률 협정), `civil_war_escalation`(실존 전투부대/통제와 연동), `stalled`(지원·보급 부족). 이미 구현된 `p5_settle` 등의 엄격한 전제조건 사용.
- 유격전 가능 결과: `demobilized`, `negotiated_ceasefire`, `reformed`, `regular_war_transition`, `protracted_unresolved`. 실제 인구·실물 소비와 독립.
- 일지는 전쟁의 주권·종속 승계를 자동 처리하지 않는다.

## H. 일간 스케줄·성능

### H1. 일간 Stage 결합

1. Stage 0: 오늘 유효한 어제 예약 효과/일지 시간 만료/일시 보정을 로드하고 읽기용 기준을 고정.
2. Stage 1~8: 기존 경제·정치·외교/군사 실제 일간 이벤트 수집. 이 시점 과거 생산/가격을 콘텐츠가 소급 변경하지 않음.
3. Stage 9: `on_action` 기록과 조건 변경으로 후보 사건·결단·일지 갱신. AI/플레이어가 선택한 효과는 정확한 권한·자원·버전을 검증 후 예약. 다른 도메인의 일간 기결산을 소급하지 않음.
4. Stage 10: 원래의 병력·전투/사상·인구·국가 정산. 그 결과 이벤트는 다음 평가 시점에 통보; 동일날 무한 연쇄 금지.
5. Stage 11: 콘텐츠 이력+기존 도메인 원장 **단일 원자 커밋/롤백**. 저장·재시작은 미선택 팝업/예약 효과/세계 일지/확률 시도 상태를 정확히 재개.

실제 법률의 시행일, 군사점령 및 전투의 확정일, 주권 변경의 유효일은 각각 기존 시뮬레이션 계약을 우선. `effective_day`로 소급 변조 불가.

### H2. 비용 상한(저사양 최적화)

- 전 세계 모든 콘텐츠 정의 × 국가 × 하루 전수 반복 금지.
- `on_action` 구독 + `dependency dirty-index` + `due_day min-heap` + 살아있는 `JournalInstance`의 주기 점검.
- 지표가 변할 때만 재판정, 내용 없는 이벤트는 no-op, 짧은 간격 연쇄를 제한.
- 한 일지의 다국가 진행은 국가별 `contribution` 증분집계를 사용, 필요한 조회만 합산.
- 세계 상태 정렬·결정론적 시드, idempotence, 부동소수 RNG 대신 고정소수점/정수.
- 큰 UI 목록은 데이터 변경 시에만 갱신, 렌더링은 뷰가 열릴 때 수행.

## I. 최소 저장 데이터

```
ContentSave {
  contract_version, content_ruleset_hash,
  processed_occurrences,
  completed_decisions_by_scope,
  pending_event_choices,
  due_triggers,
  journal_instances, global_journal_instances,
  formation_histories,
  effect_command_refs
}
```

- 새 형식은 선택적 꼬리표/별도 내용으로 이행, 기존 P1~P5/P5-G 저장 형식을 새 원본으로 복사하지 않음.
- 기존 이벤트·일지 로그와 `ContentSave`는 참조 가능하게 연결하되 재생 시 다시 인구·돈을 쓰지 않음.
- ruleset 변경 시 동일 ID의 의미 변경/삭제를 감지하고 명시적 migration 또는 안전한 로드 거부.
- 세이브가 일지의 결과를 확정했고 다시 읽어도 다른 결과로 덮어쓰지 않는다.

## J. 구체적인 콘텐츠 예시(구현 예정의 설계 샘플)

### J1. 산업화·국가계획 일지
- 활성화: 실제 국가의 계획경제 법/관련 기술 등 콘텐츠별 자격.
- 진행: 실생산, 경제자원/재정/고용의 검증된 변화; 투자 선택은 결단으로 시작.
- 분기: 계획 목표 달성 / 재정 위기 / 혼합 결과 / 중단.
- 진행률에 따라 이득/비용이 달라질 수 있으나 실물 생산이나 돈을 팝업 보상으로 무에서 만들지 않음.

### J2. 군사정변 일지
- 활성화: P3 운동 및 P5 실존 조직화·부대 동원 조건.
- 진행: 병력 충성·실병력·수도 통제·정부 협상·군수/치안 실제 사건.
- 사건: 지휘관의 이탈, 진압/협상, 특정 지역 충돌. 선택에 따라 P5 명령 예약.
- 결과: 무혈 정권 교체 / 협상 타결 / 진압 / 전면 내전 확대 / 교착.
- P5 전투 결과·주권·지휘를 재계산하지 않음.

### J3. 독일 대통일
- 조건: 규칙별 문화·기술·정치적 인정 및 필요한 영토 점유·후보 경쟁과 지지.
- 일지: 후보국·지지국·반대국의 관계 및 준비 상황을 표시.
- 결단/외교적 행동: 통일 지원 확보, 회의 개최, 실제 조약/전쟁 결과로 통일 진행.
- 형성: `FormationPlan` 검증을 통과하면 국가 정체성·국명·국기/관할 갱신; 전신국의 인구·재정·군대·외교를 무조건 초기화하지 않음.
- 역사적 날짜는 강제 통일 트리거가 아님. 1.15의 실제 수치·경계 목록은 각 formation 정의를 자료 조사한 뒤 데이터 파일로 기록.

### J4. 문화·세계 혁명
- 세계 조건 도달 시 하나의 `GlobalJournalInstance` 생성. 참여국별 P3 조직/법률/경제 상황을 분리 표시.
- 세계 공통 결과는 단 1회 확정, 개인 국가에서는 뒤따르는 법률·정당·운동 변동을 각 도메인 이력으로 적용.

## K. 구현 대상과 단계

P0 — 공통 Trigger/Effect/Scope/Registry와 결정론적 on_action/due 큐 및 원자적 명령 어댑터.
P1 — 일반 Event/Decision (선택지, 쿨다운, 한 번 발생, AI, 알림).
P2 — 일지(국가·지역, 동적 지표, 다중 결말, 활성 효과, 기한, 저장). P5/P5-G는 별도 상태 복제 없이 어댑터.
P3 — Country Formation (일반 형성, 기존 국가 계보/영토/정부 및 합법적 승계), 그다음 대통일 후보·지지·외교/전쟁.
P4 — World Journal, 콘텐츠별 지도 오버레이, 1.15식 그룹/검색/진행·결과 설명, AI 보강.
P5 — 실제 콘텐츠 데이터 팩 및 회귀/장기 일간·저사양 성능 검증.

**원칙상 설계 단계에서는 C++·tests·CI를 변경하거나 실행하지 않음.** 기존 `work/politics`에서 후속 구현을 이어간다.

## L. 향후 인수 기준(구현 시 검사)

- 이벤트 1회/세계1회/쿨다운·세이브 불변, 동일 입력·시드로 동일 일자/선택/결과.
- 잘못된 법/영토/재정/군사 명령이면 콘텐츠까지 전체 롤백.
- 같은 날 사건과 결단이 같은 자산을 차감하려면 명시적 예약/충돌 처리, 음수·이중 비용 없음.
- 상충하는 2개 일지 결말은 결정론적 단일 우선 결과, 이전 결말은 절대 재확정되지 않음.
- 다수 참가국에 대한 세계 일지 1회 결과, 참가국별 이중 법/자금/인구 이전 없음.
- 통일 형성 실패 시 국가 ID·주권·정부·부채·조약·인구 불변.
- 국명 변경/국가정체성 변경만으로 POP/정부·국가 법인격 중복 없음.
- 기존 P5-G의 병력·탄약·실제 유격활동 결과와 새 일지 UI 조회가 동일함.
- 하루 수만 지역·수백 국가 수준에서도 **모든 정의의 국가별 전수스캔 0건**을 성능 회귀 조건으로 지정.

## M. 비범위

- 역사 사건 수천 건을 처음부터 일괄 복사·완전 재현하지 않음.
- 사건·결단에 POP·재산·기업·군대를 복제하는 추상 내장 자원 금지.
- 별도 사회심리 시뮬레이터·가상의 인물 AI·인종/문화별 고정 정치반응 추가 금지.
- 일지 전용 전쟁 엔진·외교 엔진·세계지도 데이터베이스 추가 금지.
- 이번 단계는 **설계 기록**만 수행; 엔진 구현·테스트/CI는 별도 지시 시 진행.
