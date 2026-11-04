# H-D7 — 국제위기·다자간 협상·전쟁 확대·평화체결

> phase-h-diplomacy/1.0 · 2026-11-04 · SPECIFIED / C++ NOT_IMPLEMENTED
> 상위: H-D0~D6, E-D0 Claim/Dispute/Province/Sovereignty, C-P5 정치운동/내전·C-P1 전쟁/비준권한, C-M5 Conflict/WarGoal/PeaceTreaty/Ceasefire, G 통상·금융, D 산업/기술/인프라·POP.
> 방향: Victoria 2의 **국제위기 속 제3국 참여/타협/강대국 압박** 개념을 참고하되 열강독점·두 진영 강제·긴장도 100 자동전쟁을 제거한다. **발생부터 종결까지 국가 간 실제 이해관계·행위·조약으로 결정한다.**

## D7-1. 개념과 원인 분류

**잠재분쟁(Dispute)**은 E Claim/Dispute, C-P 사회·정치운동/독립·자치 주장, H TreatyTerm 위반, G 통상·제재/부채 분쟁, C-M 군사충돌 가능성 등 다른 시스템의 원본 사건이다. 모든 잠재분쟁이 자동으로 국제위기가 되는 것이 아니다.

**국제위기(InternationalCrisis)**는 둘 이상의 적격 국가·정치 주체가 특정 분쟁에 대해 **공식 입장·요구·외교적 압박·외부 지원·중재/제3국 개입** 등으로 상충하는 외교 선택을 시작하면서 국제화된 절차 상태다. 역사적 '강대국' 자격을 의무화하지 않는다.

쟁점 issue_kind: territorial_claim, independence/autonomy, protectorate/suzerainty, treaty_enforcement, economic_sanctions/trade_access, sovereign_debt/reparations, military_access/base, colonial_dispute, recognition/government_legitimacy, border_incident, civilian_protection/humanitarian, alliance_obligation, mixed_package. 쟁점은 E Province/Claim/Scope/국가·정부·원장 계약/법적 근거를 참조하며 가상의 영토·반란군/인구를 생성하지 않는다.

**국제위기 긴장도/전쟁위험**은 실제 전력배치·동원·양보/지원·제재·국내여론·신뢰·종전 보장 등에서 도출하는 파생 평가다. 임계값 '100'은 단지 표시용 상한일 수 있으며 자동 전쟁 스위치가 아니다.

## D7-2. 최소 엔터티·구분

| 객체 | 핵심 필드 |
|---|---|
| InternationalCrisisId | crisis_id, initiating_dispute_refs, issue_ids, affected_province/territorial scope, start/status/last_action_month, instigating parties, visibility, revision, provenance |
| CrisisIssueId | type, underlying claim/term/debt/region ref, claimant/respondent, demanded change, legal/evidence basis, scope, priority/linked issues, resolved_status |
| CrisisParticipationId | crisis_id, actor_id(country/government/eligible insurgent/organization mediator), role, legal_capacity, joined/exited dates, public/private commitments, source_event |
| CrisisPositionId | stance(support claimant, support respondent, independent, conditional, neutral, mediation, guarantor, observer), affected_issue_ids, offered material/diplomatic measures, conditions, expiry, revision |
| CrisisProposalId | negotiation_case_id/offer_id H-D2, issue_ref set, treaty clause package, conditions, supporter commitments, signature/ratification requirements |
| CrisisRiskAssessment | snapshot/revisions, military mobilization, issue stakes, credible support, economic exposure/alternatives, diplomatic deadlock, confidence, horizon, explanation_refs — **read-only** |
| CrisisAction | protest, ultimatum, sanctions_notice, actual_sanction_policy, withdrawal, troop_mobilization, mediate, arbitrate, consent, refusal, invoke_treaty, ceasefire_offer, issue_link |
| CrisisSettlementId | H-D2 accepted negotiation/H-D3 draft Treaty and relevant C-M5 PeaceTreaty/SettlementObligation refs, terms/conditions/entry/effective, actual fulfillment/status |
| CrisisLinkId | causal/related/competing issues and crises; no implicit merger across identical geographic regions without source events |

각 객체는 C-D0 typed EntityId·source_event/revision/valid_from-until, known/estimated/unknown을 따른다. 무력 교전의 진실값은 **C-M5 Conflict/EntryToWar**, 전장/주권관할은 E/C-M5, POP·재정/재고는 기존 원장이다. InternationalCrisis는 전쟁과 동의어가 아니다.

## D7-3. 국제위기 발생 조건과 상태 전이

권장 전이:
- LatentDispute (실제 E/C-P/H/G 원본에서 존재; 필요시 H는 조회로 표현)
- Internationalized (분쟁을 실제 국가 간 외교행위·제3국 이해관계가 국제화)
- ActiveNegotiation (기한 있는 외교/중재·지원·제재 제안이 존재)
- Settled(전체 또는 일부) / Frozen(일시 정지) / Withdrawn(요구 철회) / EscalatedArmedConflict(실제 C-M 참전/전쟁) / ExpiredAsInactive(전투/조약 없이 시효 종료)
- Reopened 또는 LinkedCrisis는 **새 근거 사건/조건**을 요구. 이전 정본을 지우고 초기화하지 않음.

**발생 평가 순서**
1. 원인 분쟁(ClaimId/Policy/Term/Contract/Event)과 대상 지역/국가·정부 유효성을 검사.
2. 분쟁 당사자의 공식 요구·거부·외교행위/실제 동원 또는 조약상 분쟁중재·제3국 지원 사실을 관측. 개인적 불만만으로 세계 위기를 자동 생성하지 않음.
3. 외교/군사·재정/상업 피해의 범위, 관련국의 실제 권익·참여 가능성·정보 접근·국내 정치의제를 평가. 직접 당사자만 있어도 국제적 사안이면 성립할 수 있고, 강대국 참석이 조건은 아님.
4. CrisisIssue·CrisisParticipation/Position의 초기 이력을 발행. same Claim/Term/issue의 반복 평가로 동일 위기를 중복 발행하지 않도록 stable trigger key/source events.
5. 협상·중재 사건은 H-D2 NegotiationCase를 참조해 실제 절차를 진행한다.
6. 새 자료/전쟁·조약 종결은 유효 권한과 해당 원본에서 처리하며, 위기 status는 그 결과를 반영한다.

잠재분쟁을 억지로 수치 하나로 누적하다가 threshold에서 전쟁을 생성하는 방식 금지. 국제위기 발생 가능성을 hazard로 추정할 수 있어도 **실제 외교행위·참여·충돌의 적격성과 사건 증거가 있어야 위기를 확정한다**.

## D7-4. 참여·중립·보증·다자 협상

- 참여 역할: claimant/respondent, supporter, conditional supporter, independent proposer, mediator, guarantor, observer, neutral. 한 나라가 동일 issue에서 논리적으로 양립 불가능한 지위를 동시에 취하지 않도록 role-specific constraints 사용.
- 소국·중견국·종속국·조직/반란조직은 모두 법적 capacity·실제 이해관계·영향·군사/경제 능력·접근성·정보에 따라 참가/비참가 가능. 종속국의 외교권 제한은 H-D4에서 확인.
- 국가의 입장은 **쟁점별**이다. A국이 B의 영유권에는 반대하지만 다른 조약의 배상요구에는 찬성하는 등 혼합 입장 가능.
- 지원의 세분: 외교적 지지 선언, 중재 보증, 제재 공동결의, 실물 군수품/은행자금/무역 지원, 실제 참전. 선언·약정·실제 재고/군사행동은 서로 다르고 각 유효 재원/정치/법률을 거친다.
- 국제협력체(H-D5)의 회원/공동의결은 참가 명분/의무·공식 중재 가능성을 제공하지만 모든 회원을 자동 동원하거나 동일 진영으로 강제 배정하지 않는다.
- **중립국은 중재**하거나 교역/난민·인도지원/통과권 제안을 할 수 있다. 중립이 관여 불능·발언 금지와 동일하지 않음.
- 참가국을 반드시 청색 vs 적색 양측으로 고정할 필요가 없다. 위기 입장 그래프/issue별 제안으로 표현하되 UI는 주요 요구·지원국·중재자를 간단히 집계해 보여준다.
- 참여·탈퇴/지원 조건의 변경은 source event/월별 시점·신뢰·조약 의무 변경을 기록. 후원선언 하나로 군사동맹 조약 자동 체결 없음.

## D7-5. 위기의 실제 역학과 예상 전쟁 위험

입력 항목:
- 쟁점에서 각국의 실제 정책이익/손실·법적 양보 가능성·제3국 대안 (H-D1/H-D2)
- 동원군·보급 가능한 전력/위협의 신빙성·국경/해상 접근성 (C-M5/F/E)
- 중재국·군사동맹·실제 군사원조·상호방위 조약의 적격성 (H-D3/H-D5)
- 자국 POP의 징병/전쟁부담·정부재정·채무/경제제재·관세/통상 관계 (POP/C-P/C-E/G)
- 조약 신뢰·과거 위반/시정·정보 비대칭과 의사전달 지연 (H-D1)
- 위기 체류기간, 제안 수정/협상결렬 및 최후통첩 시점 (H-D2/H-D3)

RiskAssessment는 '현재 근거 하의 위험·예상 피해·불확실성'을 제공하되, **실제 전쟁 결정은 해당 Country의 유효 Government/C-P 권한과 C-M5 Conflict/EntryToWar 사건**. 공격·기습/실제 군사충돌이 전쟁으로 확대되는 경우에도 전투와 양국 교전 관계를 C-M에 기록, H가 가상 부대나 사상자를 생성하지 않음.

위기 강도가 높아도 제재의 피해가 너무 크거나 제3국 중재가 성립하면 평화 해결 가능. 반대로 불완전한 정보·동원력/국내 정치와 실제 전쟁결정이 있으면 작은 국경분쟁이 확대될 수 있음. 수치가 아니라 **기존 시스템의 실제 이해관계**가 결과를 만든다.

전쟁 발발 시 위기의 모든 참가국이 자동 참전하지 않는다. 비참전 지원·중립·보증/중재·무역 중단은 개별 선택과 조약/법적 의무에 따른다.

## D7-6. 해결안·평화·이행

해결 결과는 H-D2 NegotiationOffer 패키지와 H-D3 TreatyTerm/의무 및 C-M5 PeaceTreaty/CeasefireAgreement를 통해 실현:
1. claim의 일부 철회/확정, E Province 국경변경/공동관리(실제 jurisdiction/주민·국적 등 독립 이력).
2. 적격 정치/헌정·행정에 근거한 자치권 확대, 독립/보호국·종속권한 변경(H-D4/C-P/E).
3. 통상/관세·시장 접근권/금수해제·투자/차관·배상(G/C-E3/E4).
4. 군축·군사기지 철수/상호방위·비무장지대·보장/검증(C-M/E/H-D3).
5. 주민투표/중재/조정절차가 필요한 경우 실제 유권자 적격성·행정처리/정보/법적 승인(C-P/POP/E)을 충족.
6. 이해관계가 복잡하면 일부 쟁점만 해결, 일부는 Frozen/다음 회의·장기 보증. 전체 분쟁을 강제 정리하지 않는다.
7. 합의 후 불이행/위반/재개 시 원 사건과 조약 이행·군사/경제 상태에서 다시 위기화될 수 있다. 같은 사건을 다시 금전/영토로 이중 실행하지 않음.

**중대한 상위 호환:** C-M5에 이미 PeaceTreaty/CeasefireAgreement/SettlementObligation이 정의되어 있다. H의 CrisisSettlement는 이들과 동일 권리/금전/주권 이전에 대해 두번째 정본이 되지 않는다. H TreatyTerm/H-D7은 평화조약의 **교섭·법적 조항/추적 어댑터**, C-M5는 전쟁/휴전·실제 전투·군사통제/종전 정본이다. 근본적 이관은 두 계약 ADR/버전/저장 마이그레이션을 거친다.

배상금 100을 합의하고 실제 20만 납부하면 미지급 채권/채무 80을 Financial/Fiscal에 보존. 영토 요구에서 A가 분쟁지역을 군사점령해도 E의 확정 sovereignty는 조약/법적 주권이양과 분리. POP 주민수·국적·재산권을 자동 이동/재설정하지 않는다.

## D7-7. 대표 위기 흐름(가상)

A는 B의 Province P에 영유권을 요구한다(E Claim). C는 B와 경제협력체 회원, D는 B와 상호방위조약 체결, E는 중립 중재국이라고 가정.

- 월 m: A의 적법한 요구/국제적 항의가 전달되고 B가 거절, 국제위기 개시. C는 무역손실이 커서 A의 요구에는 반대하지만 군사 지원은 거부.
- 월 m+1: D가 방위조약의 실제 발동 조건을 검토하고 일부 방어 장비 지원에 동의한다. 장비가 실제 도착하기 전에는 B의 재고/군전력이 증가하지 않음.
- 월 m+2: 중재국 E가 'P의 공동관리 + 배상금 100(분할) + 군사 철수' 조항 패키지를 제안. B의 동의·A의 반대제안·각국의 국내 승인/비준을 절차적으로 처리.
- 월 m+3 이후: 적법한 합의/비준이 이루어지면 각 조항의 effective_month에 따른 E/C-M/Financial 실행이 일어나고 위기는 Settled; 협상 단절이면 Frozen/Withdrawn 가능. 실제 C-M 전쟁 선포가 있을 경우에만 EscalatedArmedConflict.

이 사례는 시간·수치의 **예시일 뿐 발생확률/게임 승리 규칙이 아니다**.

## D7-8. 월별 12단계·정보·지연

- Stage0: 전월까지 확정된 Claim/Dispute·조약/승인/관할·군사배치·금융/무역 상태/위기 협상의 유효 스냅샷 동결.
- Stage1: 적법한 위기 대응(외교/중재/원조·제재/동원/전쟁 계획), 실제 예산/군수/행정 자원 예약.
- Stage2~8: 기발효 무역·정부재정·군사 지원/실제 선적/관세·대금 집행. 협상 제안만으로 당월 생산·재고 소급 변동 금지.
- Stage9: 위기 참가·요구안·교섭/양보/중재·기구 공동결정/서명/비준·제재/정책 변경/선전포고 승인 등을 차월 효력으로 예약.
- Stage10: 기존 유효 전투/점령·사상자/POP 변동을 C-M/POP/E 사건으로 기록; 위기의 실제 군사적 확대는 C-M5 Conflict ref와 연결.
- Stage11: 독립 위기 ID/issue 중복, 조건부 조약·부분해결·원장/정권/군사·원자 rollback·시차 검증 후 Commit.

월 내부에서 당월 긴급 외교/전쟁을 지시하는 기능이 필요하면 C-D0/C-M5가 모두 지원하는 sub-tick 계약/transaction boundary를 먼저 공식 확장한다. **당월 stage9 협의가 이미 끝난 stage4 생산이나 과거 stage10 전투를 소급 수정할 수 없다.**

## D7-9. 인터페이스·불변식

API: EvaluateCrisisTriggers(dispute_event_refs,snapshot), OpenInternationalCrisis(source_issue_ids,eligible_actors), JoinOrLeaveCrisis(actor,position,legal_basis), SubmitCrisisOffer(crisis,issue_set,package), ProposeMediation(crisis,mandate), AssessCrisisRisk(crisis,actor,information_view), ProposeCrisisSettlement(crisis,term_set), LinkConflictToCrisis(crisis,conflict_id), ObserveCrisisStatus(crisis,snapshot), ExplainCrisisEscalationOrResolution(crisis,event_history).

- HD7-INV01 국제위기의 생성은 실제 쟁점/분쟁 사건·적격 주체와 유효 근거에 연결.
- HD7-INV02 열강만 개입 가능한 고정 규칙/이분 진영 강제/중립·중재 금지 없음.
- HD7-INV03 위기 위험/긴장도는 read-only 평가, 임계값 자동 전쟁·군대·재고/인구 생성 없음.
- HD7-INV04 전쟁/참전/군사통제/사상자 원본 C-P/C-M5/E/POP 정합.
- HD7-INV05 지원선언/실제 무기·자금/외교조약·동맹의 구별.
- HD7-INV06 위기 해결 협상/조약 비준/평화협정·영토/배상 실제 이행을 독립 사건으로 관리.
- HD7-INV07 여러 위기가 동일 권리/쟁점에 얽힐 때 중복 Sovereignty/Financial/Treaty obligation 적용 금지.
- HD7-INV08 Frozen/Withdrawn/PartiallySettled/Settled/War의 비단선·시간/재개 이력 보존.
- HD7-INV09 위기 파급의 무역/POP/군사/금융 사건 이중 손실·동월 G↔H 무한 피드백 금지.
- HD7-INV10 외교채널·정보시차/기한·법적/군사적 능력과 AI/툴팁 설명 정합.
- HD7-INV11 위기 진행·종결에서 법적 Claim/Dispute와 확정 sovereignty·독립국 CountryId·정부 교체를 혼동하지 않음.
- HD7-INV12 command/event/위기문서·원장 멱등·전원장 rollback·세이브/worker 결정론.

거부: HD7_ERR_MISSING_UNDERLYING_DISPUTE, INELIGIBLE_PARTICIPANT, INVALID_CRISIS_POSITION, STALE_NEGOTIATION_REVISION, MISSING_RATIFICATION, DUPLICATE_ISSUE_EFFECT, UNKNOWN_TERRITORY_SCOPE, UNAUTHORISED_WAR_ENTRY, INVALID_M5_LINK, ILLEGAL_RETROACTIVITY.

검증: HD7-T01~T12([외교 상세 인수시험](DIPLOMACY_ACCEPTANCE_TESTS.md)). 설계이며 실제 외교 C++ 엔진/실측 위기·전쟁 시험은 미진행.
