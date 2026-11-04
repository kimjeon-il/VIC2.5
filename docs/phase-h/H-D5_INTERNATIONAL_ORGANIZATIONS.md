# H-D5 — 국제협력체·헌장·회원국·공동정책·조직변화

> phase-h-diplomacy/1.0 · 2026-11-04 · SPECIFIED / C++ NOT_IMPLEMENTED
> 상위: H-D0/H-D1~D4, C-P1 정치적/입법 권한, C-E3/E4 재정·기관, C-M5 동맹/참전, G-D0/G-T4 관세/무역, D-D0 해외 연구/기술, E-D0 주권/관할.
> 목표: Victoria 3의 '세력블록'을 **국제협력체(InternationalOrganization)**라는 조약·헌장 기반 현실적 공동 제도로 변용. 경제·군사·정치·이념·문화 분야를 조합할 수 있으나 고정 분류·무료 능력치·단일 세력 소속·강제참전은 사용하지 않는다.

## D5-1. 상위 원칙

- 국제협력체는 **공식·준공식 국가 간 협력 구조**다. 일시적 국가 협상(H-D2)과 상설/한시적 조직(H-D5)은 조직 헌장/회원권/의결 규칙의 존재 여부로 구분한다.
- 기본 성립에는 적법한 참가 주체 **최소 2곳**이 필요하다. 망명정부·속국·비국가 행위자의 정회원/참관 적격은 H-D1/D4의 법적 지위와 헌장으로 결정한다. 참가 주체가 1국만 남으면 헌장상 정지/정리/해체 절차를 적용하고 이미 축적된 조직기금/채무를 삭제하지 않는다.
- 한 국가는 **여러 국제협력체에 동시 가입** 가능하다. 배타성은 일부 조항의 의무 충돌(예: 서로 다른 대외공동관세 체제)로만 제한하며, '소속 슬롯 1개'는 금지.
- 공동 정책은 **헌장·조약 및 실제 기관 의결**로 생성하고, 실행은 해당 분야 C/D/E/G/F/군사 시스템에 위임한다. 기구의 단순 회원국이라는 이유로 산업/군사/무역 수치를 무상 가산하지 않는다.
- 조직의 주도국·지도방식은 헌장으로 규정. 조직이 자동으로 가장 GDP가 높은 국가에 귀속되거나, 강대국 영향력 구매로 바뀌지 않는다.
- 공식 조직 **국제협력체(H-D5)**와 비공식적 **국가 간 영향력(H-D6)**을 분리. 한 국가가 조직에 회원국으로 가입했어도 다른 국가의 경제·군사 영향력 아래 있을 수 있고, 조직 미가입국도 매우 밀접한 영향을 받을 수 있다.

## D5-2. 조직·회원·기관·예산 객체

| 객체 | 필수 정보 |
|---|---|
| InternationalOrganizationId | organization_id, charter_treaty_id, foundation/effective/termination, membership_rule, admitted_actors, mandate_domains, institutional_model, location, legal_personality, status, revision, provenance |
| OrganizationMembershipId | organization_id, member_actor_id, role(full/associate/observer/guarantor/conditional), entry/exit, rights, vote/delegate rules, obligations, legal_basis/term refs, revision |
| OrganizationCharterId | TreatyId와 유효 조항·헌장 개정 이력/권리/조직 정족수/가입·제명·해체 조건, 분야별 권한 한계 |
| OrganizationOrganId | org_id, assembly/council/secretariat/commission/court/standing_committee etc., representation/appointment, jurisdiction, budget authorization, meeting rules |
| OrganizationVoteId | session, proposal/term ref, valid member representatives and ballots, quorum/veto/consensus/weight rules, result, timestamp, source_event |
| OrganizationDecisionId | competent organ, subject/term refs, actual votes/mandate, adopted/effective, action_items, obligors, implementation_refs, status |
| MembershipAdmissionCaseId | candidate actor, application, legal capacity, existing treaties/exclusivity, approval rule, domestic ratification/consent, status |
| OrganizationContributionId | member, charter/decision obligation, currency/goods/service, due_period, status, Financial/Fiscal/Inventory refs |
| OrganizationAssetBindingId | actual account/asset/office/facility/employee owner references (C-E3/Ownership/C-P/Employment), legal title/beneficiary, source |
| CohesionAssessmentView | snapshot, observed treaty compliance, joint policy alignment, mutual trade/security exposure, arrears, institutional disputes, reason refs; **read-only** |

OrganizationId는 소속국 통합 CountryId가 아니다. 국제협력체가 별도 법인격·자산/직원/은행계좌를 갖는 시기/조직은 **C-E3의 적격 경제행위자 계좌**, OwnershipLedger, EmploymentLedger에 실제 관계로 등록한다. 현행 C-E3가 국제기구를 account owner로 지원하지 않으면 무조건 없는 돈/공동은행을 만들지 않고, 후속 typed OwnerRef 계약 확장 및 인증 전 그 기능은 UNSUPPORTED. 기구의 회계는 새 H 화폐원장이 아니다.

## D5-3. 회원국의 가입·탈퇴·제명

1. **설립**: H-D2 다자협상에서 Charter Treaty의 목적·기관·회원권·의결 절차·공동정책·재정/본부·발효 조건을 논의. H-D3 서명·비준·유효시점에 따라 InternationalOrganization 및 적법 최초 회원관계 성립.
2. **가입 신청**: candidate는 C-P 유효 권한, H-D4 treaty_capacity(속국/자치국 등), 기존 유효조약/타 조직과의 충돌, charter의 자격을 검사. 회원국 승인/참가비 납부/국내 비준이 요구되면 각각 독립 사건.
3. **회원 권리**: 의석·투표권·통상 혜택/군사 지원 권리·부담금·거부권/참관권의 적용범위를 명시. 참관국이 자동으로 정회원 표·공동시장 접근권을 받는 것 금지.
4. **탈퇴**: 통지 기한/청산·회비/채무·기지/시설/공동투자 권리·국내법/조약 효력에 따라 처리. 회원명부 삭제만으로 유효 채권/민간 계약·군인/재고가 소멸하지 않는다.
5. **제명·자격정지**: 헌장상의 정족수·법적 사유·절차·효력월을 요구. 지도국 마음대로 임의 강제퇴출 금지, 적법한 특정 권한이 있다면 행사 가능.
6. **분열·통합·해산**: 원 조직의 권리/채무/시설과 각각의 OrganizationId·successor/predecessor lineage, 남은 조약 조건을 보존. 의장국 교체만으로 새 OrganizationId를 발행하지 않음.
7. **중복 가입**: 여러 조직에 소속 가능하나 incompatible exclusive obligations가 있으면 가입금지·조건부 유보/조약 개정·기존 탈퇴 등 **현실적인 선택지**를 요구한다.

## D5-4. 헌장에 의해 조합 가능한 기능

| 분야/실제 조항 | 현실의 협력 방식 | 타 시스템에 필요한 실효 사건 |
|---|---|---|
| 경제·상업 | 공동시장, 역내 특별관세, 공동대외관세, 투자보장·통화협의 | H-D3 terms → C-P/G PolicyProjection·MarketAssignment/FX, 실제 거래·세수 |
| 군사·안보 | 상호방위, 군사협의, 군사기지/통행권, 공동훈련·방위조달 | H-D3 trigger → C-P/C-M5 참전/군수/기지·실제 병력/예산 |
| 외교·정치 | 공동 성명, 상호 중재, 국제회의, 관측·제재 | H-D2 NegotiationCase/Decision, 법적 이행·집행능력 |
| 공동 행정 | 조약감독위, 공동사무국, 분쟁중재/재판기관 | 법적 AuthorityGrant/유효 직원·예산·시설/실제 사건 |
| 기술·교육·문화 | 교환학생, 학술/인력교류, 연구기금, 특허·지식 이전 | POP 교육·이동/기술 지식 이전/사용권, D 연구·고용·소유 |
| 사회·금융 지원 | 재정지원기금, 재해구호, 금융안정·상호차관 | C-E3/E4 실제 계좌·채무/은행, Inventory/물류 실제 납품 |

**기능 중첩 허용:** 경제협력체가 추후 공동방위조항을 추가해도 기존 OrganizationId와 유효 헌장·다른 회원권을 유지. 군사조항 해제 후 경제협력은 남을 수 있다. **공동시장과 관세동맹의 차이**, 방위협의와 자동 참전의 차이를 treaty_term 단위로 명확히 표시.

## D5-5. 의사결정·지휘구조

- 조직기관은 회원국 대표/사무국/위원회·재판·의장국·공동지도부 등 조합가능. chair_selection은 선출·윤번·지명·창립지도국 고정 등 헌장 규칙에 따른다.
- 의결: 단순다수/가중투표/만장일치/거부권/특정 소수필수동의/참석정족수/서면동의 등. 실제 규칙·의석/가중치·시점의 회원명부를 조회해 각 ballot을 **한 번**만 계산. 국가 원조액/GDP나 외교 영향력 크기로 자동 표 배증 금지(헌장에 명시된 가중투표는 허용).
- 의결안은 policy package(조약 개정·공동 제재·분담금 증액·공동기금 지출·새 회원 승인·국제위기 중재안)이며, 안건별 적법성·발효조건·표결권·비준 필수 여부를 검사.
- 조직 표결 결과와 **회원국의 국내법·국제조약 실제 집행/군사 참전·은행 지출**은 독립 사건이다. 조약상 의무가 있으면 불이행으로 처리할 수는 있으나 조직 결의만으로 없는 군대/예금·투표권을 복제하지 않는다.
- 조직의 외교적 대표권/독립 협상 자격도 Charter Treaty의 범위에 한정. 사무국장이 모든 회원국 영토·예산을 임의 처분할 수 없다.
- 공동기금: 유효 기여금 미납·지불능력 부족·회계감사·자산 소유/집행권·기구 법인격을 검사. 회비 '약정액 100'은 실제 현금 100이 아니다.

## D5-6. 결속력·지도권·경쟁

Cohesion은 회원의 공동이해와 이행 역량에서 계산되는 **관측지표**로, 구성요소는:
- 유효 TreatyTerm의 이행/체납, 공동투표 일치/정책 충돌
- 실제 무역/투자·방위 상호의존과 대체 가능성
- 회원 간 위기·주권/역사적 갈등·탈퇴 요구
- 공동 예산/사무국 처리 역량, 조약집행의 법적 안정성
- 외부 위협·중재 성과·기관개편에 따른 이해관계 차이

이 지표를 별도 가변 cohesion_point 원장으로 저장·증가시키지 않는다. H-D6 국가 간 영향력도 별개의 read-only 평가이며, 강력한 영향국이 있어도 헌장 규칙 없이 기구의 법적 지도권을 자동 얻지 못한다. 지도권 변경은 실제 의결/승계·헌장 조항과 권한 범위에 의한다.

회원국은 특정 위기에서 서로 다른 입장·중립·중재·지원/비지원 선택을 할 수 있다. 상호방위조항이 있으면 발동 요건·법적 의무/위반과 실제 이행 능력을 H-D3/C-M5로 확인한다.

## D5-7. 사례(설명용)

A/B/C국이 3개국 경제협력체를 결성하여 **상호 수입관세 0** 조항과 **공동대외관세 5%** 조항, 연회비 각 100의 조약을 발효한다고 가정:
- A/B/C국의 특정 상품과 CustomsTerritory에 대해서만 G-T4가 유효 관세율을 적용. 공동시장/관세동맹 자체로 생산·재고/현금/주권이 증가하지 않는다.
- 각 회원의 연회비 발생채무 100, 실제 송금 40인 경우 해당 회원 미납 60이 남는다(약정금액만으로 공동기금 현금 100이 아님). 3개국 모두 각 40을 냈다면 공동계좌 실제 입금은 총 120이고, 총 약정 300/미납 180이라는 각각 다른 값이 존재한다.
- C가 B와 영토분쟁을 벌여도 경제협력체 회원권이 바로 종료되거나 A가 자동 참전하지 않는다. 헌장에 군사 조항이 없다면 각국의 C-P/H-D2 외교행위를 따름.

## D5-8. 시간·API·불변식

Stage0 회원·헌장/조약·정회원 투표권·공동 정책 효력동결; Stage1 회비·기관/공동사업·실제 자원 예약; Stage2/5 실제 직원 채용·급료; Stage3/7 G 공동관세/시장접근·공동조달의 실효 거래; Stage8 회비 채권/실수납·자산회계; Stage9 가입·탈퇴/선거/위원회 의결·헌장 개정 차월 예약; Stage10 군사·위기/인구 사건을 유효 헌장 의무와 대조; Stage11 국제협력체·원장·이력·법률 검증/커밋.

API: ProposeOrganizationCharter(case,terms), ApplyOrganizationMembership(actor,org), VoteOrganizationDecision(org,proposal,authorized_representatives), AmendOrganizationCharter(org,terms), GetActiveOrganizations(actor,snapshot), GetMembershipRights(org,actor,scope), GetOrganizationFinances(org,period), AssessOrganizationCohesion(org,snapshot), ExplainOrganizationDecision(org,decision,snapshot).

- HD5-INV01 적법한 조직/헌장·최초 2개국, OrganizationId/MemberId 안정성·유효기간.
- HD5-INV02 같은 국가 다중 가입 허용, 명시적 헌장상 배타성/의무 상충만 거부.
- HD5-INV03 기관별 의결/투표권·정족수/거부권·회의 시점 명부 정합, 동일 표 이중계수 금지.
- HD5-INV04 헌장/회원/결의만으로 군대·상품·금전·시장수급·기술 생성/무료 보너스 없음.
- HD5-INV05 공동기금의 약정/실수납·채권/채무·자산·직원 원본 C/Fiscal/Employment 일원화.
- HD5-INV06 회원 탈퇴/기구해산·지도권교체의 조약 잔존의무·채무·시설/소유권 계보 보존.
- HD5-INV07 시장·관세동맹·집단방위는 조항별 scope/법적 효력·타 분야 실행 적격성 검증.
- HD5-INV08 결속력/영향력은 실제 관계의 파생 조회이며 임의 원본 포인트/조작 금지.
- HD5-INV09 기구 회원국은 위기 중 같은 편·자동 참전·자동 외교/군사 권한 이전 강제 안 함.
- HD5-INV10 H/C-P/G/M/E 사건 idempotence·전원장 rollback·worker1/N/save 결정론.

거부: HD5_ERR_CHARTER_INVALID, TOO_FEW_FOUNDERS, INELIGIBLE_MEMBER, EXCLUSIVE_CONFLICT, NO_VOTING_RIGHT, QUORUM_NOT_MET, MEMBER_NOT_RATIFIED, INVALID_LEADERSHIP, UNFUNDED_CONTRIBUTION, ORG_OWNER_UNSUPPORTED, STALE_MEMBERSHIP_REVISION.

수락: [DIPLOMACY_ACCEPTANCE_TESTS](DIPLOMACY_ACCEPTANCE_TESTS.md) HD5-T01~T12. 문서 완료만 의미, 실제 C++ 구현·시험 미실행.
