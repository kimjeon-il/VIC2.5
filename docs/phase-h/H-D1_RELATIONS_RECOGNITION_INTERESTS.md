# H-D1 — 국가 간 관계·외교적 인식·승인·이해관계

> phase-h-diplomacy/1.0 · 2026-11-04 · SPECIFIED / C++ NOT_IMPLEMENTED
> 상위 H-D0, C-D0/C-P1~P5/C-M5, E-D0, POP-D0~D3, C-E/G-D0. H-D1은 국가·정부·원장의 복제 없이 외교관계의 **정본 사건 및 인식 모델**을 정의한다.

## D1-1. 외교의 객체·관계 분해

- CountryId: C-P1에 존재하는 국가/국제법상 주체. GovernmentId는 정권이며 Country와 구분. PoliticalActor/Institution/Office 권한은 C-P, 정치·인구 원본은 POP.
- DiplomaticRelationId: (observer_actor_id, target_actor_id, relation_kind, valid_from/until, source_event_id). **A→B와 B→A는 별개**. 동맹 등 상호 의무는 Treaty의 조항을 참조하지 서로 동의한 관계인 양 각자 변경하지 않는다.
- DiplomaticRecognitionId: recognizer, recognized_claimant(국가·정부·역사적 반란/망명 정부 등 적법 식별자), recognized_as(state/government/belligerent/limited_contact 등), legal_basis, status(active/suspended/withdrawn/contested), effective_period, source_event. **승인 = 국가 생성 = 영토 주권 이전이 아니다.**
- DiplomaticMissionId: sending_authority, receiving_authority, diplomatic_channel(embassy/consulate/agent/liaison/envoy), scope/host_region, staff employment refs, permitted activities, funding, valid_period. 역사적 제도에서 현대 대사관만 강제하지 않는다.
- DeclaredPositionId: actor, issue/Claim/Dispute/TreatyTerm/Crisis reference, stated_policy, communicated_to, effective_month, source event. 공식 발표·외교적 약속과 실제 유효 조약/군사원조를 구분.
- PerceptionRecord: observer, target, evidence_ids, information_as_of, observed_from/source, confidence, secrecy/access, belief_factors, expiry/revision. 관측 정본 사건이 아니라 **특정 행위자가 무엇을 알고 있는지**의 유효 인식 기록이다. 동일 사실에 대해 관찰국마다 인식이 달라도 허용.
- BilateralAssessmentView: observer→target, open_snapshot, revision keys, threat/trust/compatibility/issue alignment/economic exposure/security interdependence, explanation refs, estimated/unknown. 관계의 파생 평가이며 하나의 우호도 원장을 추가하지 않는다.
- InterestAssessment: actor, issue, policy alternatives, direct economic/security/political/legal/social consequences, uncertainty/time horizon, explanation refs. 정책별 예상비용·편익 비교의 read-only 결과. 외교 AI/툴팁이 공통 사용.

## D1-2. 관계의 독립 차원과 계산

| 평가 차원 | 실제 근거 | 혼동 방지 |
|---|---|---|
| 신뢰/약속 이행 기대 | 조약 이행, 제때 지급된 지원, 기존 약속 위반/시정·장기 협상 이력 | 우호도가 높아도 위반 반복 시 신뢰는 낮을 수 있음 |
| 정책 일치도 | 특정 영토/전쟁/통상·정치제도 관련 현실적인 선호와 공식 입장 | 동맹이라는 이유로 모든 정책에 동의하지 않음 |
| 위협 인식 | 실제 국경분쟁·군사력/배치·군수 지속능력·공격적 요구/동원·국내 상황 | 상대 GDP/군대 규모가 곧 적대감 아님 |
| 경제 이해관계 | 실제 교역·투자·채권/채무·대체 가능성, 제재 시 손실의 방향 | 경제적 상호의존 = 일방적 영향력 또는 신뢰가 아님 |
| 외교적 접근성 | 승인, 공식 대표권, 소통·언론/통신/지리·의전 제약 | 대사관 존재만으로 경제 생산량 증가 아님 |
| 국내 수용성 | POP 정치성향·전쟁피해·정당/이익집단·정부권한·행정력 | 정부가 원하는 외교안이 언제나 국내 승인되는 건 아님 |
| 국제적 평판 | 제3국이 관찰한 조약 이행·중재·침공/원조, 정보 접근 | 전세계에 전지적 공통 평판수치 단방향 배포 금지 |

- 항목은 동일한 0~100으로 강제 정규화하지 않는다. 각 항목은 해당 규칙에 맞는 비율·사건 이력·범주·불확실성을 유지. UI용 요약 신뢰 등은 fixed-point view로 변환 가능하나 원본 판단을 대신할 수 없다.
- 단일 평판/우호 증가 이벤트가 H-D6 영향력에도 다시 '+5' 보너스로 더해지지 않도록 **source_event_id 단위 기여·중복 원인 제거**. 위협, 신뢰, 영향력, 종속관계, 공식 승인, 회원 가입은 다른 개념이다.
- 상대가 관세를 인상해도 A가 왜 그렇게 했는지 B는 관찰·판단할 수 있다. B의 PerceptionRecord는 실제 policy/provenance와 시차/불확실성이 다를 수 있고 잘못된 정보에서 의사결정해도 실제 금융·영토의 정본은 수정되지 않는다.
- 정권 교체, 혁명, 전쟁 뒤의 관계 연속성: 국가의 기존 조약·채무가 소멸하지 않으며 새로운 Government의 정책·공약·승인·접근은 유효 C-P 사건으로 별도 갱신. 한 정권의 비공식 개인적 신뢰는 새 정부의 국가 관계와 구분 가능.
- 역사적 특정 문화·민족·종교에 선천적 전쟁성향 또는 획일적 친소 관계를 부여하지 않는다. 관측 가능한 정책·제도·사회 여건과 검증된 역사 시나리오 자료만 사용한다.

## D1-3. 외교승인·교섭 적격성과 인식

1. **국가와 정부 승인**을 따로 관리한다. A가 국가 B의 존재를 인정하면서 B의 현 집권정부 대신 망명정부와 외교관계를 맺을 수 있다. 두 상대가 동시 주권을 보유한다고 E의 확정 sovereignty에 중복 기록하지 않는다.
2. 한 국가의 자기 선포 독립/영유권 주장은 E Claim/Dispute와 C-P 정치 사건이며 제3국 승인/대사교환과 별개. 승인은 다른 국가의 판단·조약거래 범위를 바꾸지만 POP 주민/영토/국적을 자동 재작성하지 않는다.
3. 사실상 교섭·임시 사절·비공식 대표·전시 협상은 공식 승인 전에도 법/정치규칙이 허용하면 가능. 비공식 접촉을 정식 주권승인으로 간주하지 않는다.
4. 승인의 철회는 공관·협상권·국내외 법적 요건에 영향을 줄 수 있으나 기존 외채, 회사 소유권, 유효 계약·인구를 자동 소멸시키지 않는다.
5. 시대별 국제법/국내법의 승인 기준과 열강 질서·외교 관행은 룰셋에서 달라질 수 있다. 21세기의 UN 회원제도나 법률을 1801년의 보편적 법으로 소급하지 않는다.
6. 정식 통상/군사조약의 당사자 적격성은 H-D3의 TreatyCapacity와 C-P AuthorityGrant로 검증; DiplomaticRecognition 상태만으로 조약 체결이 무조건 허용/금지되는 것은 아니다.

## D1-4. 관측·신뢰·정보 시차

- 사실(Fact): 조약 체결/경제거래/군대 이동/국경변경 등의 타 분야 확정 사건. H는 그 사건을 복제한 두 번째 사실 원장을 만들지 않는다.
- 관측(Observation): 외교사절·신문/연락·상업·군사 정보/제3국 통지로 특정 사실이 전달된 기록. 정보 도착시점·비공개 범위·신뢰도·원 출처를 가진다.
- 판단(Belief): 각국 정부가 가진 증거로 계산한 예상 의도/위협·동맹 신뢰·합의가능성. 미래 예측이며 확정 사실이 아니다.
- AI 국가가 실제로 모르는 비밀 조약의 조항을 전세계 알고 있다고 전제하지 않는다. 다만 플레이어 편의를 위해 관측 가능한 확정 정보와 추정치를 명시적으로 구분한다.
- 중립적인 정보 접근성/통신 지연은 역사적 기술(D-T), 건설 인프라(D-C), 물류/외교기관 비용에 반영. 정보 수집을 별도 추상 '첩보 포인트' 생성으로 환원하지 않는다.

## D1-5. 지역·국제협력체·위기와의 관계

- 영토 주장의 정본: E Claim/Dispute. H의 DiplomaticRelation은 상대 영유권 요구를 지지/반대하는 **입장**을 담을 수 있으나 실제 주권을 이전하지 않는다.
- 국제협력체 회원(H-D5) 여부는 접근·공동 협의와 유효 조약·공동 의무에 반영되지만 A/B 각자의 모든 양자 갈등을 삭제하지 않는다.
- 영향력(H-D6)은 객관적 경제·안보·사회 의존의 파생 뷰; 신뢰·친밀감이 낮아도 높은 영향력을 가질 수 있다.
- 국제위기(H-D7)에서 쟁점별 지지·중립·중재와 국가별 리스크 인식은 서로 달라질 수 있으며, H-D1은 사건화 이전의 상대 인식과 이해관계 입력을 제공한다.
- 군사적 위협 인식과 실제 참전/선전포고는 다른 상태. 전쟁 발생은 C-P/C-M5 사건만 허용한다.

## D1-6. 시간·인터페이스·검증

Stage0 유효 승인/공관/조약·외교적 사실과 각국의 information_as_of를 동결한다. Stage1에서 적법한 국가·공관의 접촉·예측과 자원 확보, Stage2/5는 실제 외교인력/급료 원장, Stage9에 신규 승인/비승인·외교 정책 명령과 차월 효과 예약, Stage10에 C-M/POP 사건 반영, Stage11에 관계와 관측 인과·유효기간/원장 정합성을 확인한다. Stage9 정책이 당월 stage3/7 금융·무역을 소급 변경하지 않는다.

Read API: GetRecognition(recognizer,target,month), GetDiplomaticChannels(actor,target,snapshot), ObserveBilateralRelations(actor,target,snapshot), AssessThreat(actor,target,evidence), EvaluateInterests(actor,issue,alternatives,snapshot), ExplainRelationChange(actor,target,period). Write는 ProposeRecognitionAction/ProposeMissionAction/PublishDiplomaticPosition 등 **C-D0 명령**으로만 수행.

불변식:
- HD1-INV01 Country/Government/Recognition/Claim/AuthorityGrant 분리; 정부승인으로 국가/영토 자동 생성 금지.
- HD1-INV02 A→B 신뢰/위협/인식과 B→A 대칭 강제 없음.
- HD1-INV03 인식 자료의 정보 시점·출처·공개 범위·unknown/estimated/observed 구분.
- HD1-INV04 경제·군사·법적 관계/영향력과 관계요약 캐시가 독립 쓰기 원본이 되지 않음.
- HD1-INV05 조약/보증/실제 지원·교전의 원본 참조와 단순 외교적 발표/인식의 혼동 금지.
- HD1-INV06 관계/인식 평가에서 동일 사건의 중복 강화·AI 전지성·동월 정보 역행 금지.
- HD1-INV07 정권교체·승인철회로 POP/현금/소유권·기존 계약 자동 삭제 금지.
- HD1-INV08 유효 인정/비공식 접촉·공관 권한/예산/인력 및 역사적 법적 제약 검증.
- HD1-INV09 모든 ID/기간·source refs·revision·월별 원자 커밋/멱등 검증.
- HD1-INV10 동일 seed/world/정보 접근에서 worker1/N 설명·상태 평가 결정론.

오류/거부: HD1_ERR_UNKNOWN_ACTOR, INVALID_RECOGNITION, NO_REPRESENTATION_AUTHORITY, ILLEGAL_CONTACT, STALE_INFORMATION, MISDATED_OBSERVATION, WRONG_SCOPE, DUPLICATE_RELATION_EVENT, REVISION_CONFLICT.

수락: [외교 상세 인수시험](DIPLOMACY_ACCEPTANCE_TESTS.md) HD1-T01~T12. 본 문서는 상세 설계 완료일 뿐 실제 C++ 엔진·인수시험은 미실행이다.
