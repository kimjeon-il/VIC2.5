# H-D2 — 외교활동·협상·중재·최후통첩·외교역량

> phase-h-diplomacy/1.0 · 2026-11-04 · SPECIFIED / NOT_IMPLEMENTED
> 선행: H-D0/H-D1, C-P1~P5(외교권한/인사·기관), C-E3/E4(지급/재정), POP-D0~D3, C-M5(전쟁), E-D0(지역·주권), H-D3(조약). 계약·외교행위·실제 집행은 분리한다.

## D2-1. 기본 목표와 플레이어 행동

외교는 단순한 포인트 소모/수치 올리기 메뉴가 아니라 국가가 유효한 정책을 **제안·요구·협상·수락/거부·중재·실행**하는 절차다. 플레이어는 소수의 의미 있는 행위를 선택하며(조약 제안, 대사 파견, 외교적 항의, 공식 승인, 보증, 중재, 경제제재 결정, 최후통첩 등), 상대국 AI는 이해관계·비용·실효성·법률·정보에 근거해 반응한다.

행동 유형은 관계 변화 자체가 아니라 외부에 전달 가능한 **원인 사건**:
- contact/establish_mission/withdraw_mission/recognize/protest/communique/consultation
- propose_treaty/propose_amendment/propose_extradition_or_transit/offer_aid/offer_guarantee
- request_concession/request_compensation/mediate/arbitrate/accept/reject/counteroffer/withdraw_offer
- sanction_policy_request/issue_ultimatum/declare_neutrality/announce_support
- invoke_existing_treaty/consult_international_organization/crisis_position_change/peace_offer

정당한 명령 제출은 곧 목표달성이나 자금지출/조약 발효/전쟁 발생이 아니다. 외교 이익·위협/기여를 직접 높이는 버튼과 무료 외교 점수는 기본값으로 두지 않는다.

## D2-2. 문서화하는 실제 객체

| 객체 | 필수 필드/의미 |
|---|---|
| DiplomaticAct | act_id, issuing country/government/office, targets, type, scope, legal_authority, command_id, source_event_id, adopted/effective/expiry, visibility, status, revision |
| DiplomaticMessage | message_id, sender/receivers, linked_act/case, content/ref, channel, send/receive month, disclosure, response_due, receipt refs; 연락기록이지 새 정책 정본 아님 |
| DiplomaticMission | H-D1 정본; 직원/공관·실제 예산/시설·접촉/협상 범위 |
| NegotiationCase | case_id, purpose/issues/claim refs, participants/roles, procedure, deadline, confidentiality, status, opened/closed period, latest_version |
| NegotiationOffer | offer_id, case_id, proposing actor, full proposed_term_set, package dependencies, concession/offering_condition, expiry, expected_revisions, submitted_month, parent_offer_id |
| NegotiationPosition | participant, acceptable policy alternatives, confidential reservation limits, stated public position, domestic mandate, validity; 공개 요구와 내부 고려를 구별 |
| DiplomaticCommitment | nonbinding declaration 또는 conditional pledge, condition/effective/expiry, actual TreatyTerm/Obligation refs if legally binding; 중복 지급 원장 아님 |
| MediationMandate | mediator (국가/적격 국제협력체/역사적 행위자), scope, consent/authority, confidentiality, proposed procedure, duration |
| DiplomaticCapacityView | effective institutions, authorized officers, staff capacity, travel/communication delay, budget/facilities, ongoing caseload; **파생 조회** |

모든 ID는 C-D0 stable typed namespace; 완료·중단 후 tombstone + 이력을 보존. 제안의 텍스트는 별도 조약·원장 효력이 아니며 제안서 버전·유효기간/선행조건을 명시한다.

## D2-3. 외교행정과 실질 자원

- 공관/사절/정상외교의 권한 = C-P StateOffice/AuthorityGrant + 정부/외교 인사·국제적 인식(H-D1) + 관계별 의전/통신 채널.
- 외교의 인력 = PopulationSlice·EmploymentLedger에 실제 존재하는 관리/통역/사절 및 필요시 임명된 C-P PoliticalActor/PersonAnchor(중복 인구 없음).
- 운영비 = Fiscal 예산 의무와 FinancialLedger 실제 지급. 시설/숙박/통신·문서/사절 이동이 필요하면 실제 C/D/E/F/C-E5 서비스·비용을 사용.
- 국가 규모가 크다는 이유로 모든 협상을 무제한 동시 처리할 수는 없으나 **일괄 외교포인트**로 제한하지 않는다. 유효 사건 처리능력은 직원·기관 전문성·통신시간·법적 권한·일정으로 결정한다.
- 시대별 기술/교통에 따른 교섭 지연(1801년 서신·사절, 후기 전신·전화·인터넷)은 기술/교통 상태에서 산출. 모든 세기에 '오늘 보냄→즉시 도착'을 기본으로 두지 않음.
- 평시 협상/위기 다자협상 모두 같은 공관·정부 역량을 활용하고, 위기 명령이 무료로 별도 외교인원을 창조하지 않는다.

## D2-4. 협상 상태와 절차

**NegotiationCase**: open → collecting_positions → active_bargaining → agreed_in_principle / deadlocked / suspended / withdrawn / expired. agreed_in_principle은 Treaty가 아니다.

1. **적격성/의제**: 실제 Claim/Dispute·경제계약·종속 관계·유효 조약을 대상으로 적법한 정부/기관이 사건을 개시한다. 법적 위임이 없는 사절이 주권 양도를 확약할 수 없다.
2. **통신/정보**: 참가국에 제안이 유효 통신경로로 도달했는지 검사. secret_offer는 해당 당사자만 인지, 공표된 내용과 협상자 인식을 구별.
3. **제안 제출**: Offer는 기간·내용·양자/다자 참가자·필수/선택 조항·조건/비용·상호 조항 의존을 포함. 무효 기간·권한 충돌은 Reject.
4. **계산(동결된 스냅샷)**: 각 국가의 기대실익·법적 가능성·재정/군사 실행능력·제3국 반응·국내정치·조약 충돌·정보/신뢰(H-D1)·영향력(H-D6)의 관련 원천 등을 평가. 영향력 값이 수락률 보너스가 되는 것은 금지.
5. **반응/교섭**: accept, reject, counter, conditional acceptance, request guarantee, defer, withdraw. 참가자별 비준/의회 승인과 공약의 불확실성을 보존. 협상 마감일 경과는 전쟁의 자동 발동조건이 아니다.
6. **합의 문서화**: 수락된 내용은 H-D3의 Draft Treaty(또는 유효한 비구속 공동성명/정책 변경)로 제출, 적법 서명/비준 후 개별 조항 발효. 합의된 협상도 정부가 실제 자금을 지급하거나 군대를 이동시키기 전까지 이행이 아님.
7. **실패와 이력**: 협상 결렬/시한 경과·강경조치/합의 불이행은 사후 외교적 신뢰·정치/경제 위험에 영향을 줄 수 있으나, 단순 거부가 반드시 전쟁·기본 우호도 급락을 유발하지는 않는다. 근거 사건에 연결.

협상 시 대안을 조합할 수 있다: 영토 공동관리+배상, 관세 특혜+군사통행권, 자치 보장+국제중재, 군축+안보보장 등. 각 조항은 비대칭적 당사자·유효기간·국내 비준과 집행주체를 가진다.

## D2-5. 수락 의사결정(국가 AI와 플레이어)

AI가 평가하는 것은 (A) 현실적 법적 승인권, (B) 정책별 실제 예상 경제·군사·영토/정치적 결과, (C) 다른 국가/국제기구 약속의 충돌, (D) 상대국의 실행 가능 능력 및 위험, (E) 국내 정치상 승인 가능성과 선거/군사·재정 부담, (F) 정보 접근과 시한이다.

- 수락 가능성 자체를 고정 'A 호감도 70이면 수락'으로 판단하지 않음. 상대 군사력이 강해도 국내 반발, 제3국 방위보장, 제재의 실제 손실·대체 무역시장, 조약의 집행가능성에 따라 거절 가능.
- 상대가 아무 자원도 없는데 "매월 금 10만 지급"을 제안하면 무조건 협상 거부할 필요는 없지만 **현재 재정/금융으로 이행 불가능**하다는 위험·조건부 이행/보증을 표시해야 한다.
- 모델은 완전한 미래를 아는 전지적 AI가 아니며, 행위자의 H-D1 information_as_of로 관측 가능한 수치와 불확실한 추정을 구분한다.
- 필요한 역사적/정치적 성향/협상비용 계수는 Ruleset에; 하나의 조작에 무조건 정해진 '외교 성공률' 고정값 금지. 동일 세계/시드/행위·정보에서 동일 결과를 생성하는 결정론 준수.
- 위기 외교(H-D7)는 참가자·중재자/보증·다수 쟁점별 별도 제안이 같은 NegotiationCase를 사용하며 자동 승전/정해진 2진영 구도는 강제하지 않는다.

## D2-6. 최후통첩과 압박

- **Ultimatum** = 구체적 요구·시한·협상경로·거부 시 발표된 조건부 조치(예: 제재/군사행동 검토). 법률상 유효 명령과 실제 도달 여부, 시한은 외교적으로 기록한다.
- 시한 경과로 할 수 있는 조치는 C-P의 적법한 승인 후 시행. 즉시 자동 전쟁을 발생시키지 않는다. 유효 미발효 정책(제재)과 실제 군사 동원/선전포고는 C-P/C-M5 원본.
- 압박의 협상효과는 채무·무역손실·군사보급·국제동맹 실행능력/대체 수단 등에서 발생한다. 경고문 하나만으로 상대 인구·물자를 삭제하거나 종속관계를 부여하지 않는다.
- 다자 중재자는 중립 혹은 이해관계가 있을 수 있으며, 중재 의결이 주권/군사력을 마법적으로 이전하지 않는다.
- H-D5 국제협력체의 공식 결의가 있어도 회원정부가 합법적으로 집행할 수 있는지를 C-P/H-D3에서 별도 검증한다.

## D2-7. 월별 파이프라인

Stage0: 지난 월 확정 외교권한·공관/계약·유효 협상/전달된 서신과 법적 상태를 동결.
Stage1: 새 외교명령의 권한/재정/공약자원 확인 및 통신/사절·이동 서비스 예약.
Stage2/5: 실제 외교 공무원·임금 및 운영 비용을 C 원장에 기록.
Stage3/7: 이미 유효한 제재·원조/통상조항은 타 분야가 거래에 적용; 신규 제안만으로 당월 무역을 바꾸지 않는다.
Stage9: 통지 도달/제안·대응·합의/서명/비준 절차·기구 협의·국제위기 제안을 처리하여 차월 유효 사건 예약.
Stage10: 기존 유효 군사·인구 충격에 대한 사실을 조회/사후 판단, 같은 월 과거 거래를 되돌리지 않음.
Stage11: 시한·제안 수정경쟁/revision·원장·상위 계약 검증 후 월말 단일 Commit.

동월 일 단위 외교적 급변을 실제 게임플레이로 지원하려면 C-D0의 공개 12단계와 모순 없는 **명시적 하위 sub-tick 계약/전 분야 동기화**를 먼저 확장해야 한다.

## D2-8. 인터페이스·불변식

API: ProposeDiplomaticAct(issuer,act_type,targets,legal_basis,expected_revisions), OpenNegotiation(issues,participants), SendDiplomaticMessage(channel,content,receipt_condition), SubmitOffer(case_id,terms,expiry), EvaluateOfferForActor(actor,offer,snapshot,info_view), RecordNegotiationResponse(case,offer,response), ProposeMediation(case,mediator,terms), ProposeUltimatum(demands,deadline,conditional_actions), ExplainNegotiationDecision(actor,offer,snapshot).

- HD2-INV01 command/offer/response의 권한·시점/전달·revision·멱등 일치.
- HD2-INV02 협상 제안·비구속 공약은 금융/군사/영토·무역의 실제 효과와 구별.
- HD2-INV03 실제 외교기관 고용/예산·사절/통신 및 단계 순서를 우회한 무상 행동 없음.
- HD2-INV04 공개/비공개 제안·정보 도착시점에 따른 행위자 인식과 AI 선택의 정합.
- HD2-INV05 협상수락과 Treaty 서명/비준/발효/이행은 서로 별개.
- HD2-INV06 압박·최후통첩·위기 데드라인으로 자동 전쟁/영토 양도 없음.
- HD2-INV07 다자 협상·조건부 조항/중재자의 비동일 입장과 여러 경로 허용.
- HD2-INV08 같은 자금/군사약속·인력 이중 예약 금지, 실패는 C-D0 전역 롤백.
- HD2-INV09 AI/툴팁 수락 설명은 실제 비용·법률/불확실성·근거 자료에서 도출.
- HD2-INV10 정보/조건/상위 revision 동일 시 worker1/N 결정론·세이브 재현.

거부: HD2_ERR_NO_LEGAL_MANDATE, UNREACHABLE_COUNTERPARTY, MESSAGE_NOT_RECEIVED, NEGOTIATION_EXPIRED, UNFUNDED_OFFER, CONFLICTING_OBLIGATION, INVALID_RESPONSE, INVALID_DEADLINE, PROPOSAL_REVISION_MISMATCH. 한 나라가 거절하는 것은 정상적 외교 결과이며 HARD ERROR가 아님.

수락: [DIPLOMACY_ACCEPTANCE_TESTS](DIPLOMACY_ACCEPTANCE_TESTS.md) HD2-T01~T12. 문서 작성은 실제 C++ 구현/실행을 의미하지 않는다.
