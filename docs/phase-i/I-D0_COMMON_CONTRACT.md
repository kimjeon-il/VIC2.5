# I-D0 — 사회·문화·교육 공통 설계 계약

> 계약: phase-i-common/1.0 · 2026-11-04 · SPECIFIED / NOT IMPLEMENTED
>
> 역할: I-S1 사회구조 · I-C1 문화/언어/종교 · I-E1 교육 · I-V 통합 검증
>
> 상위 호환 계약: POP-D0~D3 (pop-design/1.1), C-D0/C-E/C-P/C-M, D-D0 및 D-T, E-D0, G/H. 충돌 시 POP 인원 원본 → C-D0 거래/시점 → 해당 분야 소유 원장 → I-D0의 파생·교육 규칙 순으로 처리한다. I가 다른 분야의 소유권을 빼앗지 않는다.

## I0-1. 목표와 제외 범위

VIC2.5는 정교한 내부 계산, 간결한 원인 중심 플레이를 유지한다. I는 실제 교육 서비스와 사회적 접촉으로 인한 인구 상태의 장기 변화를 계산한다. 고용·소득·자산·정치적 권리·법률·기술·교원 임금·학교 건축을 중복 엔진으로 재구현하지 않는다.

- I-S1 사회계층/불평등/사회이동은 실제 POP 직업·생활수준·자산분포·교육·법적 지위로부터 파생한다. 국가별 사회계급 점수, 별도 가변 사회불안 지수를 원본으로 만들지 않는다.
- I-C1 문화/종교는 기존 DemographicCohortKey의 culture_id/religion_id를 사용하고, 언어 능력은 필요한 경우 별도 희소 자격/언어 프로필 또는 인구 배분에 연결한다. 문화·종교에 선천적 능력/생산성/정치 성향을 부여하지 않는다.
- I-E1 교육은 시설·교원·교육시간·재정·재료·법적 접근성과 실제 이용 가능 인원을 통해 문해율/교육단계/숙련의 전이를 발생시킨다. 교육예산이나 기술 해금이 POP 능력을 즉시 일괄 증가시키지 않는다.
- 보건/의료 서비스, 개별 가족의 전체 친족 그래프, 일일 단위 수업시간표, 미시적 개인 AI, 국가별 문화 포인트 시스템은 I-D0 필수 범위가 아니다.

## I0-2. 안정적 식별자와 원본 소유권

새 I 계열 식별자는 EducationProgramId, EducationParticipationId, EducationOutcomeEventId, CulturalContactEventId, SocialObservationId(파생 통계) 등이며, 타입별 네임스페이스/비재사용 ID, 0 invalid, source_event_id·revision·effective_month·provenance를 따른다. 기존 PopulationSliceId, RegionId, ProvinceId, InstitutionId, ProductionSiteId 등은 원본 재발급 없이 참조한다.

| 정보 | 유일한 정본/소유자 | I 사용/출력 |
|---|---|---|
| 인구수, 나이, 성별, 거주, 문화·종교 | POP PopulationSlice.count / DemographicCohortKey | 학생 수·문화별 인구는 조회; 정체성 변경은 POP-D2 인원 보존 전이 |
| 학력/문해/숙련 | POP EducationProfile 및 교육 자격 분포 | I가 교육 이수 근거를 만들고 POP 전이 요청; 독립 학력 총량 원장 금지 |
| 직업·고용·소득·자산·생활수준 | C Employment/Financial/Ownership, POP 분류 | 사회계층·불평등·사회이동은 파생 조회 |
| 학교/연구시설·설비·공정 기술 | D Construction/CapitalAsset/SiteTechnique, Knowledge | 시설 수용력/설비/접근 가능 기술 읽기; I에서 시설 복제 금지 |
| 교원 노동/임금, 교재/교구 상품, 서비스 | C-E1/E2/D의 실제 노동·시설 서비스·Inventory/Financial | 월별 사용량과 EducationDeliveryEvent의 출처를 대조 |
| 교육 법률·의무교육·차별금지·예산·세금 | C-P/C-E4 및 E 관할 | 학습 적격성/교육권/학교 재정 제약 읽기 |
| 정치적 의견/단체·선거·갈등 | C-P | 사회 변화 영향은 읽기 전용 분석 또는 차월 정치 입력 |
| 교육 프로그램·참여/진도·교육 성과의 근거 | I EducationProgramCatalog / EducationParticipation / EducationProgress | 실제 교육 서비스의 이력. PopulationSlice.count 대체 금지 |

EducationParticipation은 (participation_id, slice_id, program_id, provider_site_id, region_id, effective_period, allocated_people, allocation_basis, source_event_id, revision)의 희소 참여/예약 관계다. allocated_people는 유일 인구 원본이 아니며, 해당 Slice에서 동시에 참석 가능한 실제 사람 수를 초과할 수 없다. 풀타임 노동/복무/중복 학업과 충돌하는 시간은 명시적 시간용량 계약으로 청산한다. 허용된 파트타임 학습은 실제 남은 시간 범위 내에 제한한다.

EducationProgramCatalog는 교육단계·기간·수업언어·필요 교원/시설/재료·자격 및 이수조건을 버전으로 정의한다. EducationProgress는 참여 인원에 귀속된 누적 유효 수업량과 기준 충족 여부의 희소 상태로, 금액/사람 수/학력 원본을 복제하지 않는다. 시설별 학급 수는 실제 가용 서비스를 집계하여 도출한다.

## I0-3. 사회구조·문화/언어·교육의 계산 경계

1. 사회적 계층, 지니계수/격차, 교육 접근률, 문해율, 사회이동률 등은 지정 기간/지역/인구 범위를 고정한 파생 지표다. 평균값 하나로 소득·자산 분포를 날리지 않으며 모수/결측/추정 정확도를 표시한다.
2. 문화·종교 구성 변화는 출생/사망/이주 또는 명시적 POP 정체성 변경 사건으로만 나타난다. 언어 학습은 문화 정체성 변경과 다르다. 지역적 접촉은 공거주·학교/언어교육·이동·정책과 관측 가능한 사건에 근거한다. 자동 전국 동화율 버튼/인종별 고정 보너스 금지.
3. 역사적 제도 차이(학교 의무 여부, 종교 교육, 신분·성별에 따른 합법/불법 차별, 교육언어)는 C-P 유효 법률과 실제 집행을 통해 평가한다. 특정 문화/성별에 선천적 능력 패널티를 주지 않는다.
4. 실제 교육 산출은 유효 교사/시설/시간/재료/통학·법적 접근의 최소 병목으로 제한한다. 서비스 제공 및 교육 이수는 구분하며 교육 이수는 정해진 기간/충족 기준 이후 POP EducationProfile 전이로 반영한다.
5. 한 인구 배분 셀 내 서로 다른 수료·언어/문화 변화가 발생하면 POP-D2의 인원 보존형 분할/병합과 교육/고용·가구·자산 관계 이전을 사용한다. 프로그램마다 무분별한 POP 셀 신규 생성 금지.
6. 문화 변화 및 사회적 불평등의 정치적 효과는 C-P에서 결정한다. I는 기존 정치 태도 원장을 직접 수정하거나 사회불안/급진화 수치를 중복 가산하지 않는다.
7. 불명 자료(unknown)와 실측 0을 구별하고 역사 시나리오의 근거/정밀도/추정을 보존한다.

## I0-4. 월별 12단계 내 삽입 (단계 추가/순서 변경 금지)

| Stage | I가 읽거나 제출하는 사건 |
|---|---|
| 0 opening_snapshot | 전월 확정 POP 학력·문화, 교육 법률/예산/시설·프로그램·거주/관할/입학 대기, 자격 기준 동결 |
| 1 planning_and_credit | 학교·정부/민간 제공자의 실제 재원과 운영계획 확인, 당월 교육서비스/등록 가능량 계획 |
| 2 labor_clearance | 교원 및 다른 교육 직원의 실제 고용/자격/시간 수용력 배정; 타 산업/군사 중복 예약 금지 |
| 3 intermediate_goods | stage4 교육 제공에 필요한 교재·에너지 등 이미 도착한 물품과 확보된 서비스 검증 |
| 4 production | 실제 교원/시설/시간/재료로 교육 서비스 제공 및 EducationDeliveryEvent 기록; 계획을 실적으로 간주 금지 |
| 5 wages_and_transfers | 교원 임금/교육보조금·장학금 등 실제 지급은 기존 금융·재정 원장에서 처리 |
| 6 pop_consumption_plans | 유료 교육비와 다른 가구 소비 경쟁, 실제 가처분 자금 반영; 이미 끝난 stage4 교육서비스 소급 불가 |
| 7 final_goods_market | 교육 관련 당월 구매/공공조달을 기존 시장·재고로 청산; 이때 도착한 교재는 stage4 교육에 소급 투입 불가 |
| 8 closing_accounts | 교육 제공자 비용·실제 수입/지급과 원장 정합 검증 |
| 9 saving_and_investment | 차월 교육법/예산/프로그램·신설 학교·투자/제도 개편을 적법하게 예약; 월 m 승인 → 통상 m+1 stage0 유효 |
| 10 pop_demography_and_health | 실제 이수 근거로 문해/학력/숙련 및 문화·종교 변경 등 POP-D2 보존형 사건 확정; 다음 달 노동·정치 적격성에 반영 |
| 11 repricing_and_validation | 참여/교육시간/학력 전이/교원 원장·분할 계보·사건 중복·세계 원장 검증 후 ONE Commit |

동월 stage10의 새 학력은 이미 종료된 stage2 고용이나 stage4 연구/생산을 소급 변경할 수 없다. stage10 사회변화가 정치에 영향을 주는 경우 다음 달 유효 관측 입력만 전달하고 동일 효과를 I→C-P와 I→C-E에 중복 가산하지 않는다.

## I0-5. 공개 API 및 사건

- GetSocialIndicators(scope, period, population_filter, world_snapshot) → sourced read-only measures + denominators + uncertainty; 교육·소득·문화/종교 비율은 재계산 가능한 조회값.
- ResolveEducationEligibility(slice_id, program_id, opening_snapshot) → eligible_people, legal/age/time/location constraints, explanation.
- PlanEducationCapacity(provider_id, program_id, month, opening_snapshot) → 교사·시설·시간/자금·교재 계획과 병목 (실적이나 예약 결과 아님).
- RecordEducationDelivery(provider, program, participant_allocations, actual_resources, stage4) → 검증 가능한 원인 사건; C 노동/서비스·시설/재료와 대응해야 한다.
- EvaluateEducationCompletion(participation, stage10) → POP EducationProfile 전이 요청 (인원 보존/이력/기존 관계 검증).
- ProposeCulturalTransition(slice, cause_refs, stage10) → 정체성 전이 요청; 세대교체·이주·언어 노출/법률의 실제 원인 없으면 거부.
- ExplainSocialChange(scope, period, snapshot) → 교육·소득·이주·법률 변화에 대한 원본 사건 경로 및 추정 구간.

모든 쓰기 요청은 C-D0의 Command/ProposedEvent→검증→JournalBatch→StagingWorld→월말 원자 Commit을 사용하며 소유자 원장에만 반영한다. 이벤트 멱등성·권한/법률·시점/revision 검사 필수.

## I0-6. 저장·캐싱·결정론·성능

- Save header: phase_i_contract_version=phase-i-common/1.0, program/ruleset/content hashes, 관련 POP/C/D/E 계약 버전, entity revisions, source_provenance_hash, RNG/numeric policy, last_commit_id.
- 교육 참여·진도·연속성·이력은 희소 레코드. 인구 셀 분할/합병 시 추정 배분이 아니라 원인별 배분과 관계 계보를 보존한다.
- 동일 world_snapshot/ruleset/seed/명령 및 안정적 event ordering에서 같은 결과. 인원 정수, 교육시간·진도는 검산 가능한 고정소수점/정수. 모든 계량은 기본 단위를 명시한다.
- 캐시는 (month, scope, relevant POP/Financial/Education/policy revisions, dataset hash)와 결합. 월말 집계는 실제 변경한 셀과 관련 지역을 우선 처리하고 불필요한 전국×POP×학교 전체 재계산을 금지한다.
- 10만/100만 POP 셀, 여러 교육·문화 구성, 장기 1,200개월, 재시작 및 worker 수 변화의 실측 검증을 I-V 구현 게이트로 둔다. 미측정 성능을 PASS로 표시하지 않는다.

## I0-7. 공통 불변식 및 기본 수락 게이트

| ID | 필수 검사 |
|---|---|
| ID0-INV01 | 인구 총량의 유일 원본은 PopulationSlice.count. 진학·문화변화·졸업으로 인구 증식/소멸 없음 |
| ID0-INV02 | 프로그램별 동시 참여와 교원 업무 시간은 실제 인원/시간 적격량·고용/복무 배정 범위를 초과하지 않음 |
| ID0-INV03 | 학교 수업량/학력 변화는 교원·시설·시간·재원·서비스 실적으로 입증되어야 함 |
| ID0-INV04 | 문해/학력/숙련은 POP 정본만 갱신; 사회계층·격차·문화 구성은 파생 결과 |
| ID0-INV05 | 문화·종교 변화는 POP-D2 사건과 근거·분할/병합/기존 계약 관계의 정합을 가짐 |
| ID0-INV06 | 교육·문화·사회효과를 고용/정치에 이중 가산하지 않으며 유효 정책 시점과 다음달 효력을 지킴 |
| ID0-INV07 | 학교 운영·장학금·급여·교재의 실제 화폐·재고 이동은 C 원장과 정확히 일치 |
| ID0-INV08 | 잘못된 자격/음수 인원/누락된 참조/중복 사건은 reject, 세계 불변식 파괴는 rollback |
| ID0-INV09 | 동일 시드·world/revisions에서 동일 결과, 저장/복구로 참여 진도·이력 손실 없음 |
| ID0-INV10 | unknown과 실제 0·인구 가중 분모·역사적 정밀도 구별, 선천적 문화/종교 능력 보너스 금지 |

최소 I-D0 게이트: ID0-T01 6인 POP 교육 중복 방지; T02 교원/산업 경쟁; T03 예산만 증가해도 즉시 문해 상승 없음; T04 학교/교재 부족; T05 실제 이수와 차월 취업 자격; T06 언어 학습 ≠ 문화 정체성 강제변경; T07 문화 이주/전이 후 인구 보존; T08 금전/재고 정합; T09 rollback/save-reload/replay; T10 100만 POP/장기 실행 성능 측정. 모두 향후 구현 인수시험이며 현재 PASS 아님.

## I0-8. 후속 세부 설계와 ADR

- I-S1 사회구조/불평등/이동의 분포와 조회 규칙.
- I-C1 문화/언어/종교의 지역 접촉·인구 변화 경로.
- I-E1 학교·취학·교육서비스·학력/숙련 전이.
- I-V 불변조건·성능·장기 회귀 및 C/D/E/G/H와의 상호작용.

확정 ADR: I0-ADR-001 인원 정본 POP 재사용; 002 사회계층/격차 파생값; 003 교육은 실물 서비스+학력 POP 전이; 004 문화와 언어 분리, 문화·종교 선천적 보너스 없음; 005 기존 12단계/월 커밋 준수; 006 지표는 원본 사건에 근거·이중효과 금지.

**완료 정의:** 위 D0 계약을 설계 정본으로 확정. 실제 C++ 코드 구현/실측 시험/Phase I 세부 알고리즘 확정은 아직 아님.
