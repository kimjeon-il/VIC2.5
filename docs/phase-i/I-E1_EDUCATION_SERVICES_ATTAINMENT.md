# I-E1 — 교육기관·서비스·학력·숙련 상세 설계

> phase-i-education/1.1 · SPECIFIED / NOT IMPLEMENTED · 상위: I-D0(phase-i-common/1.1), POP-D0~D3, C-E1~E4, C-P, D-C/D-T, E-D0
> 언어학습의 별도 능력/진도 모듈은 I-C1/1.1에서 폐기. 수업언어는 문화 메타데이터·법률/교육 접근에만 반영.

## E1-1. 지원 범위
- 기초 문해교육, 보통교육, 중등/고등 교육, 직업/전문 교육을 시대·지역에 따라 다르게 제공한다. 일률적인 현대적 의무교육·대학 제도를 1801년 이후 전 기간에 강제하지 않는다.
- 교육은 독립 '문해율 올리기' 정책 버튼이 아니다. 정부·사학/종교기관·가구·기업 등의 운영행위가 실제 인력/시설/시간/재료·재원 및 법적 자격에 의해 제한된다.
- POP의 EducationProfile(literacy_level, education_stage, skill_tags)는 학력/자격 원본. 교육 프로그램의 등록자 명단·누적 수업시간은 실제 참여/이수 근거일 뿐 새 POP 인구수 원본이 아니다.

## E1-2. 최소 데이터 구조
| 객체 | 필드·해석 |
|---|---|
| EducationProgram | program_id, curriculum_version, age/qualification prerequisites, instruction_language(문화의 하위 언어정보·교육법과 비교할 정책 속성), duration_months, required_service_units, material/teacher qualifications, degree/skill_outcome, tuition_rules, historical_range |
| EducationProviderBinding | provider_institution_id, facility_site_id, scope_id, operator/financial_owner_refs, legal_license, program_ids, valid_months; 실제 건물/고용을 중복 저장하지 않음 |
| EducationParticipation | id, slice_id, program_id, provider_site_id, admitted_people, start_month, attendance_allocation, time_budget_refs, completion_status, source_event_id, revision |
| EducationProgress | participation_id, delivered_service_units, attended_months, interruption/transfer_records, cumulative_evidence, revision |
| EducationDeliveryEvent | source_event_id, month, program, participating_slices, teacher_labor_service, actual_site_service, material_lots, funding_refs, delivered_units, accepted_student_units |

교육프로그램의 '기간 요건'은 달 수이고 교실 서비스 제공량은 student-hour 또는 규칙 파일의 명시적 서비스 단위를 사용한다. 교사의 1시간이 학생 1명에게만 제공된다는 임의의 1:1 가정 대신 교사-학급 교습 가능 범위를 버전 있는 규칙으로 정의한다.

## E1-3. 실제 교육 공급과 등록
1. 유효 학교시설의 수용인원/상태, 교사 고용·자격과 실제 근무시간, 교재·연료/설비서비스, 통학/거리/안전/법률 및 예산/학비를 각각 검사한다.
2. 실제 교육 상한은 `min(학생 적격 인원, 시설·시간 적격 좌석, 교원·운영 시간에서 가능한 학생 서비스, 가용 교재·에너지, 자금/현실적인 접근 제약)`의 공동 제약으로 청산한다. 공급계획은 이수 실적이 아니다.
3. 강제교육이 있어도 가난·전쟁·교사부족·시설부재·이동제약·차별적 집행 때문에 실제 취학률이 낮을 수 있다. 법률만 변경해 문해율·학력 즉시 상승 금지.
4. 학생과 노동/군복무·기존 학업의 시간 중복을 검증한다. 파트타임 학업·야학은 명시적 남는 시간/비용으로 허용. 장애/건강은 개별 기능 요건·편의/접근성으로 평가하고 진단명만으로 무조건 불허하지 않는다.
5. 학력·문해·숙련의 **단계·시험 조건**은 프로그램별 규칙. 교육 서비스 기록이 충족되면 stage10에 인원 보존형 POP-D2 전이를 제안하고 다음달 노동 자격/기술 채택 가능성이 바뀐다. 수업언어는 문화의 대표/보조 언어 정보와 공용/교육언어법을 참고하는 **접근 조건**이지, 개별 POP의 언어 숙련도/학습진도 원본이 아니다.
6. 한 셀 내 수료한 일부만 EducationProfile이 변경된다면 POP-D2가 필요한 만큼만 분할하며 원본 임금/자산·고용/가구 관계 이전을 원자적으로 대조한다.
7. D-T4 현장 기술 훈련과 I-E1 일반 교육이 같은 교사·학교시간을 경쟁할 수 있다. D의 이미 존재하는 TrainingProgram을 무단 복제하지 않고 계약된 교육서비스·자격 전이를 공유한다. 동일 이수 결과를 양쪽에서 2회 기록하지 않는다.

## E1-4. 예산·교원·경제 연결
- 학교 운영자는 실제 정부 예산/법인/가구 자금을 기반으로 임금·교재/시설유지보수·학비를 C Financial/Fiscal/Inventory에서 지급·인수한다. 비용 집행 실패로 제공하지 못한 수업은 EducationDeliveryEvent 생성 불가.
- 교원 수는 POP EmploymentLedger에서 산출. 학교가 교원 확보를 위해 제조업/군과 노동시장에서 경쟁할 수 있다.
- 무료 공교육은 교육에 필요한 실제 재원이 정부 지출로 정산되는 것이지 교육서비스 자체가 비용·인력 없는 무상 생산이라는 뜻이 아니다.
- 대학·연구소가 교육과 연구를 동시에 수행하면 실제 시설·교수/연구자/실험재·시간 용량의 중복 점유를 거부한다.
- 교육 수준 개선이 산업 생산성을 변화시키려면 기존 C-E1 직종 자격 및 D-T의 실제 도입·고용·연구 역량을 거친다.

## E1-5. 12단계 배치
- stage0 학력·법·학교/시설·교사 계약·예산/학업 진도 동결.
- stage1 교육 운영·학생등록/학비·정부 운영자금 계획/예약; stage2 교원 실제 고용/노동 시간 경쟁; stage3 stage4에 쓸 실제 확보 교재와 서비스.
- stage4 교육 서비스 실적(EducationDeliveryEvent), 참여/결석·실제 좌석 사용. stage5 교원 급여/보조금 실제 지급. stage6~7 가구 학비·교재 구매/학교 조달(이미 끝난 stage4 소급 금지). stage8 교육 회계/채무 대조.
- stage9 다음달 학교 설립/법·예산/교육 프로그램 예약. stage10 수업 누적·중단/졸업/학력·숙련 변화만 POP-D2 사건으로 확정(언어 숙련 전이 없음). stage11 인원/서비스·금융·물품·이수/월별 재현성을 검증하고 ONE commit.

## E1-6. API 및 성능
GetEducationAccess(scope, program, month, snapshot) → 유효 수용력/부족 요인/미취학 추정; PlanAdmissions(provider, slices, month, snapshot); ReserveInstructionResources(provider, teachers, materials, schedules); RecordEducationDelivery(... stage4); EvaluateGraduation(... stage10) → 인원 보존형 POP 전이; ExplainEducationOutcome(scope, period) → 건물·교원·예산·법·인구 원인 경로.

월별 희소 참여·진도만 보유, 프로그램×모든 POP 셀의 밀집행렬 금지. 지역/프로그램 변경 시만 통계 캐시 무효화. 100만 셀·1,200개월·중단/복학/전쟁·이주·저장/복구 시나리오 측정은 I-V 게이트다.

## E1-7. 필수 검증 (I-E1-T01~T12)
T01 학교만 건설해 교원 없이 학력 상승 불가; T02 교원 있지만 학교/교재 없음; T03 법률/교육예산 공포 당월 소급 증가 없음; T04 교원 이중 고용/학생 이중시간 거부; T05 무료 교육 정부 실제 비용 검산; T06 수업 누적/학력 취득 기간 준수; T07 단계10 신규 학력 차월 고용 반영; T08 1/3 수료 집단 분할·인원 보존; T09 D-T4 직무훈련과 일반교육 동일 이수 중복 금지; T10 결석/전쟁·이주 이후 진도 정합; T11 장애·법/통학 적격성 및 종교 관련 법령의 실제 적용, 교육언어 접근은 별도 숙련 POP 분할 없이 평가; T12 저장/복구·worker 재현 및 100만 셀 메모리 계측.

**수락 경계:** 교육 인과·사건·자격·회계 계약 상세 설계 확정. 교육 엔진 구현과 시험 PASS는 아님.
