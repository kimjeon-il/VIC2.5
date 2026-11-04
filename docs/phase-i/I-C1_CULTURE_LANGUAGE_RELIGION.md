# I-C1 — 문화·언어·종교 상세 설계

> phase-i-culture/1.0 · SPECIFIED / NOT IMPLEMENTED · 상위: I-D0, POP-D0~D3, C-P, D-T, E-D0

## C1-1. 핵심 분리와 현실성
- POP DemographicCohortKey의 culture_id·religion_id는 기존 원본이다. 언어 **능력**과 문화·종교 **정체성**은 다른 속성이다. 학교에서 다른 언어를 배워도 자동 동화/개종시키지 않는다.
- 법률상 국교·공용어·강제교육·관용/차별·이주 정책은 C-P가 소유한다. 실효 접근성과 교육은 I-E1이 평가. 종교/문화 자체에 선천적 연구·생산·군사/정치 성향 차이값을 부여하지 않는다.
- 하나의 문화/종교만 존재하는 지역도 가능하고 다언어 사용, 종교 미상/무종교/혼합된 정체성 등 역사 데이터의 결측을 지원한다.
- 국제 이주·군사 점령·행정적 주권 변경은 POP 문화·종교를 즉시 일괄 교체하지 않는다.

## C1-2. 상태와 사건
| 객체 | 최소 필드 | 정본 |
|---|---|---|
| CultureDefinition / ReligionDefinition | typed ID, 분류/역사 시기, 표시명, 출처, 동의어/lineage | ruleset/POP 분류를 참조하는 카탈로그 |
| LanguageCompetenceProfile | language_id, proficiency_band, literacy_band, learning_provenance, validity | POP의 교육/자격 프로필에 연결된 희소 상태. 개별 언어값 수를 복제하지 않음 |
| CulturalContactEvent | region/province, interacting_slice_refs, contact_kind, effective_month, exposure_scale, source_event_id, provenance | I의 접촉 근거; 사람 수를 더하지 않음 |
| LanguageLearningOutcome | participation_id, program_id, verified_instruction, qualified_people, stage10_effect | I-E1 교육 서비스 기반 교육/자격 전이 |
| IdentityTransitionProposal | source_slice, selected_count, from/to identity, cause_refs, month, allocation/precision | POP-D2 전이를 요청하고 검증 후 POP 원본만 변경 |

문화·종교별 지역 비율, 다언어 비율, 지배언어/문화 등은 PopulationSlice.count의 읽기전용 인원가중 집계다. '문화 영향력 포인트'를 가변 원본으로 저장하지 않는다.

## C1-3. 언어 학습·접촉과 정체성 변화
1. 실제 이주/거주·학교 교육언어·가정/지역 접촉·교역·군복무/선전·법률 집행 등의 유효 접촉만 고려한다. 매달 전 국가 주민을 균일한 문화변화 확률에 노출하지 않는다.
2. **언어 학습**은 교육 서비스량·이미 습득한 능력·교사/교육언어·접근·시간에 의해 진행된다. 숙련 여부는 노동시장 등의 언어 자격으로 쓰되 문화 ID 변경을 강제하지 않는다.
3. **문화적 정체성 전이**는 장기 접촉/세대교체·선택·정책/사회환경에 따라 정해진 사람 수의 POP 전이로만 처리한다. 시간차와 개인별 이질성은 희소 셀의 배분으로 표현한다.
4. **종교 전이**도 법적 강요가 자동 복종을 보장하는 공식이 아니다. 제도적 유인/억압은 접근·위험·조직/선택 상황의 원인이지만, 개종 인구는 검증된 사건으로만 확정한다.
5. 겹치는 사건의 중복이동 금지: 한 사람이 같은 월에 서로 충돌하는 정체성 전이를 받으면 POP-D2의 배분/의존관계·ID 정렬로 하나만 확정. 정책·교육/이주 입력으로 두 번 정체성 변경 효과를 더하지 않는다.
6. 확률 모형을 쓰는 경우 정수 인원·고정소수점 확률·(world_seed, month, event_kind, slice_id, draw_index) 난수 스트림을 사용한다. 수용률/전이확률은 **추정 규칙**이며 민족·종교의 본질적 속성값이 아니다.

## C1-4. 12단계 연결과 외부 효과
- stage0: 지역 POP 구성, 법률·학교언어·이주/관할 관계/접촉기록과 정체성 프로필 확정.
- stage2: 기존 언어능력/자격이 필요한 일부 직종의 고용 적격성; 문화 자체의 적격성은 법률·실제 차별에 근거.
- stage4: 유효 언어 교육 서비스, 인쇄·미디어/종교 서비스의 실제 공급·접촉 사건. 
- stage9: 교육언어·관용·국교/검열 등의 제도 변경 승인, 통상 다음 달 발효.
- stage10: 교육 이수로 언어 숙련 전이, 정체성/종교 변화가 있는 경우 POP-D2 사건. 신규 전이는 다음 달 고용·정치에 반영.
- stage11: 문화/언어/종교 구성의 인구 보존·계보/권리·저장/결정론 검증.
- 외교 H-D6의 문화적 영향력은 H가 실제 이동/교육·언어/접촉 등을 바탕으로 파생한다. I에 2중 영향력 원장 금지.

## C1-5. 조회/명령 API
- GetCulturalComposition(scope, month, snapshot) → weighted counts, shares, historical data quality.
- GetLanguageCompetence(scope, language, level, snapshot) → 적격 인원/학습 상태, 정체성 별도.
- EvaluateContact(region, input_events, snapshot) → 근거/노출 가능 범위; 임의 전국 자동변경 금지.
- ProposeLanguageProgress(participation, verified_service, stage10) → 교육 프로필 변경 요청.
- ProposeIdentityTransition(slice, new_identity, count, cause_refs, stage10) → POP-D2 인원 보존 사건.
- ExplainIdentityChange(scope, t0, t1, snapshot) → 출생/사망/이주/실제 전이로 변화 요인 분해.

## C1-6. 필수 검증 (I-C1-T01~T10)
T01 이중언어 학생 문화 ID 불변; T02 법률만 공포해 즉시 동화 없음; T03 국경/군사 점령 후 인구 문화 불변; T04 이민자가 이주 중 중복 카운트되지 않음; T05 정체성 전이 후 전/후 인구 동치; T06 한 셀 동일 월 중복 전이 반려; T07 언어 교육 없는 언어 숙련 급등 금지; T08 문화/종교별 선천적 기술보너스 없음; T09 법률·자격·학교언어 시점 준수; T10 확률 시드/세이브·희소 POP 부하 재현.

**수락 경계:** 상세 계약 SPECIFIED, POP 교육/문화 이벤트 실제 통합·검증은 미실행.
