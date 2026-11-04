# H-D3 — 조약·조항·비준·이행·위반·국가승계

> phase-h-diplomacy/1.0 · 2026-11-04 · SPECIFIED / C++ NOT_IMPLEMENTED
> 의존: H-D0/D1/D2, C-P1 정부/헌정기관·법, C-M5 평화/전쟁 원본, E-D0 주권, C-E3/E4 재정금융, G-D0/T4 무역, D-D0 연구/투자.
> 목표: **복수 조항을 가진 양자·다자 조약**을 독립 법적 체계로 관리하고, 실제 이행은 기존 분야 원장의 적법 사건으로 연결한다. Victoria 3의 다중 조항 협정 발상만 참고하며 고유 수치·코드를 복제하지 않는다.

## D3-1. 국가 조약 원본 및 데이터 모델

| 객체 | 최소 필드/불변 영역 |
|---|---|
| TreatyId | treaty_id, type, negotiating_case_ref, participating_actor_roles, signature_requirements, depositary/registrar, public/confidential scope, amendment/withdrawal/prolongation procedure, legal_basis, signed/ratified/effective/expired periods, revision, provenance |
| TreatyTermId | treaty_id, term_id, category, covered actors/regions/markets/goods, duties, legal_priority, conditions, effective_from/until, exceptions/reservations, dependencies, status, rule_revision, executing_system |
| TreatyRatificationId | party_id, ratifying_office/institution, constitutional_order, decision/consent source, approval_mode, instruments_deposited, decision_month, entry_into_force_condition |
| TreatyObligationId | term_ref, obligor/beneficiary, recurring/one-off, due_period, performance_requirement, compliance_evidence_refs, creditor/debtor refs if applicable, status, remedy, source_event_id |
| TreatyParticipationId | party_id, treaty_id, participant_role, reservation/withdrawal/territorial_application, legal_effective_interval, succession_event |
| TreatyAmendmentId | original_terms, modified_term_specs, accession/approval status, effective_date, supersession references |
| BreachNoticeId / DisputeProcedureId | term, observed_noncompliance, legal/evidence assessment, alleged_by, notified_to, cure_period, mediation/arbitration clauses, outcome |
| TreatyProjection | scope, month, snapshot/revisions, applicable legal terms, beneficiary/obligor, authority, effect_kind, stable term_key, assumptions/uncertainty; **read-only** |

- TreatyTerm은 **규칙/권리/의무의 정본**이다. 관세 실효세율·군사통행/방어 의무·영토 반환·연구협력·비밀 지원 등은 각각 해당 범주/시행기관이 실제로 실행하거나 조건으로 조회한다.
- H-D3은 이행 현황과 법적 의무를 기록할 수 있으나 실제 **돈·대출·재고·인구·시설·군사작전·영토 주권**은 C/D/E/G/F의 원장에 그대로 남는다.
- 모든 객체는 C-D0 typed ID/valid_from-until(exclusive), source event, revision, tombstone/lineage, unknown/estimated/0 구분을 준수한다. 개별 조항·동의서·지급의 고유 idempotency key는 같은 사건을 이중 적용하지 않는다.

## D3-2. 조약·조항의 독립 생명주기

- 조약 문서: Draft → Negotiating(H-D2) → Signed → PendingRatification / PartlyRatified → Effective → PartlySuspended / Amended → Expired / Terminated. 국가/조항별 부분발효/가등 조건도 지원.
- **서명(Signature)**은 외교적으로 합의된 문서에 대한 행위. 단독으로 특정 국가의 의회 비준·금전 송금·주권 이동을 완료하지 않는다.
- **비준/승인(Ratification)**은 C-P 유효 헌정 권한·의회/군주/내각/임시정부의 적법 절차에 의한다. 시대별 비준이 필수인지, 서명 즉시 구속력이 있는 체제인지 등은 명시된 domestic_rule 및 조약 entry_into_force 조건으로 처리한다.
- **발효(Entry into force)**는 required_signatures/ratifications/consents/quorum/date/threshold와 해당 국가별 조건을 충족한 시점에 발생한다. 3개국 조약 중 2개국만 비준한 경우에 모든 3국에 무조건 효력이 생기지 않는다.
- **조항 적용(Term effective)**은 조약 전체보다 좁은 상품/국가/지역/조건/날짜를 가질 수 있다. 관세면제는 발효되었지만 배상금 지급은 다음 달, 군사기지 사용권은 추가 비준까지 보류하는 식으로 분리한다.
- **이행(Performance)**은 실제 관세 면제·교역·병력 철수·자금/기술 지급·시설 사용권/영토 이전/군사행동을 각 분야가 확정한 이후 해당 근거로 fulfillment 상태 갱신. 약정 금액과 실제 지급액을 같다고 가정하지 않는다.
- **중단/종료/개정**은 적법한 조약 절차·참가국 동의/고지/위반·기간을 검증한다. 특정 조항만 중단 가능; 기존 미지급 채무·권리/부속 협정은 legal_survival_clause에 따라 남을 수 있다.
- 조약문·서명/비준 사실과 실제 시행 이벤트 간 반영 시차는 C-D0 12단계 및 m+1 원칙. 법/외교적 선언이 실물 경제를 당월 역행해서 바꾸지 않는다.

## D3-3. 조항 종류와 실행 어댑터

| 조항 범주 | 유효조건·실행 | 원본 |
|---|---|---|
| 상호방위/군사원조 | 침공·보증발동 요건, 국내 승인/군사능력, 전투병력/장비·급여/무기 인도 | H 의무 → C-P/C-M5 실제 EntryToWar/군수/POP |
| 불가침·중립/군축·사찰 | 행위금지·철수/전력 제한, 검증/위반 통보 | H/C-P 법, C-M 실제 병력·이동/장비 |
| 통상/관세/시장접근·통과권 | goods scope, customs area, duty exemption, origin rules, validity | H term → G TradePolicyProjection → C-E5 Customs/Fiscal/Financial |
| 투자/차관·특허/연구 | 투자자·자산/기업/신용·지식/라이선스 제공 조건 | H obligation → C-E3/D 실제 지분·대출·KnowledgeRegistry |
| 영토/국경·주권/자치/공동관리 | 특정 E Province/Claim/Scope, 법적 권한·부속 승계 | H term → E jurisdiction + C-P 법/POP 국적·재산 별도 사건 |
| 보호국/종속·대외권한 위임 | H-D4 SovereigntyRelationship의 구체 위임·의무·종료 | H term → H-D4 관계 + C-P/E/G/C-M 실행 |
| 배상·금전/실물 원조 | 납기·수취인/통화/상품/담보, 미이행·대체지급 | H term → C-E3/E4 발생채권·실제 지급, G/F 실제 상품배송 |
| 국제협력체 헌장·회원의무 | 가입·투표·공동기금·시장/방위 조항의 발효·탈퇴 | H term → H-D5 조직/회원; 실제 예산/행동은 C/E/G |
| 중재·위기해결·평화 | 중재절차·서명국·유효 휴전/전쟁종료·사후보장 | H-D7 절차 + C-M5 PeaceTreaty/Ceasefire 원본 |
| 문화/교육·연구인력 교류 | 사람·문화제도/학교/지식·취업/통행/재정 제약 | POP/C-P/D 지식/고용·G/F 실제 자원 |

각 조항마다 executor_kind와 effect_scope를 갖되 법적 효과(영업권/시장접근 허가)와 실제 사업장·재원/인력·배송의 발생을 분리한다. **C-M5 PeaceTreaty의 정본은 명시적 ADR/마이그레이션 전까지 C-M5 그대로**이며 H TreatyTerm은 mapping/adapter를 통해 같은 peace_treaty_id/obligation_id를 참조한다. TradeAgreement도 C-E5/H 이관 전 중복 정본 금지.

## D3-4. 조약 충돌·중첩·우선순위

- 충돌 판정은 (actor, scope, goods/Province, legal_kind, legal_hierarchy, time, exceptions, earlier_term_refs, third_party_rights)에서 수행한다.
- **다중 조약은 원칙적으로 공존**한다. A-B 방위동맹, A-C 통상협정, B-C 중립보장은 자동 상충이 아니다. 실효 의무가 정면으로 충돌할 때만 conflict.
- 충돌한 조항을 '최근 서명했으니 무조건 우선'하거나 강대국의 영향력 점수로 임의 무효화하지 않는다. 국가별 국내법·조약상의 법적 우선순위·예외·시간과 권한·해결절차로 판단한다. 법적 결론 불가능하면 incompatible_pending_resolution/reject로 표시.
- 각 조항이 다른 조항에 종속될 수 있음: 관세면제는 공동관세영역 발효 조건, 상호방위는 적법한 집단방위 발동 조건, 영토이양은 당사자의 법적 승인/시점 등. DAG 선행 관계, 사이클 금지.
- 공개/비밀 조약: 효력/의무는 법률상 비밀 유지가 가능하더라도 실제 계약 정본에 존재해야 한다. 다른 국가/AI의 H-D1 information_view에는 접근권 없이 누출하지 않는다.
- 위반은 실효 의무의 실제 불이행·권한/비용·면책·불가항력/고의 여부를 분리해 평가. 자동 전쟁·전세계 악평 수치가 아니라 통지·시정기회·재교섭·제재·중재·위기화 등 가능한 다음 외교행동으로 연결한다.

## D3-5. 이행·지급·영토 변경의 원자성

- **조약 m월 stage9 승인 → 원칙적으로 m+1 stage0 법적 효력.** 현재 월의 관세·전투·취업/선거에 소급 적용 금지.
- 이행계획은 유효 조항과 필요한 금융/상품·시설·병력/통과권·시간/투표권의 **실제 제약**을 검증하여 C-D0 Command→ProposedEvent→JournalBatch→StagingWorld→Commit.
- 하나의 조약이 금전배상·영토이양·군축을 같은 시점의 단일 조건부 패키지로 명시한다면, **동시 이행 조건**을 가진 변경집합이 전원장 원자적 검증에 성공해야만 시행. 반대로 비동시·분할 이행은 각 마일스톤별 채무/주권 상태를 명시해 정상적인 부분 이행을 허용한다.
- 조약 배상 100이 발생하고 실제 지급 20이면 의무 100, 기지급 20, 미지급 80(명시된 변경/이자 없는 가정). 타국 자금이 공중에서 발행되지 않는다.
- 영토 할양은 E sovereignty change에 특정 ProvinceId 집합, 권리/세금/민정·군사통제 분리, 주민 POP·국적·재산·공공채무 승계의 별도 명시 사건을 동반. 정권 교체/조약발효가 주민수·국적을 자동 변경하지 않는다.
- 조약 폐기/전쟁발생으로 기존 은행대출/민간 계약이 자동 소멸하는 것은 아니다. 실효 제재/채무조정/전시금융 조건이 있을 때 실제 원장 변동.
- 실제 기구 헌장 개정/회원국 탈퇴는 H-D5 membership와 공동기금/시설/채권 등에 대한 조건을 명시. 조직 ID나 가입국 자체를 자동 재생성하지 않는다.

## D3-6. 역사적 법제도·국가 승계

- 1801년 이후의 다양한 왕조조약·속국조약·무역특허·협정·전쟁배상/비밀 의정서, 현대 다자조약을 지원하되 1969년 비엔나 협약 등의 현대 법률을 **모든 시대에 기계적으로 소급 적용하지 않는다**.
- 국가와 정권의 연속성, 분리·합병·점령·독립·정부승계는 C-P1/E-D0의 실제 Country/Government/sovereignty 사건과 연결. TreatyParticipation의 승계·재확인·탈퇴·권리/채무 배분은 treaty_succession_rule 및 당시 법적 상황과 계약조항에 따라 평가한다.
- 정부가 바뀌어도 동일 CountryId이면 유효 외교조약을 기본 유지하되 해당 조약 조건/권한·정권 관련 예외가 있을 수 있다. 국가 자체의 분열/병합 시 자동 일괄 승계나 자동 소멸을 보장하지 않는다.
- 실제 사료의 불확실한 비준일/발효일은 unknown/estimated·출처를 기록하고 정확하지 않은 날짜를 0/게임 시작월로 가정하지 않는다.

## D3-7. 월별 단계·API·불변식

Stage0: 확정/유효 조약·조항/만기·권한·국내 비준 상태와 이행조건 동결; Stage1: 이행 계획·재원/군사·거래 예약 검증; Stage2~8: 이미 발효된 조항의 효과를 해당 C/D/E/G/F가 수행하고 H는 compliance refs만 연결; Stage9: 새 서명/비준/수정/탈퇴/종결 결정과 차월 발효 예약; Stage10: C-M 군사·POP/관할 충격, 종전 정합성; Stage11: 조약/의무/전 분야 원장·동일조항 중복/효력 검증 후 Commit.

API: ProposeTreatyDraft(case,parties,term_specs), SignTreaty(office,treaty,revision), RatifyTreaty(country,authority,legal_basis), EvaluateEntryIntoForce(treaty,month), GetApplicableTreatyTerms(actor,scope,month,snapshot), ProposeTreatyImplementation(term,source_command), RecordObligationFulfillment(obligation,upstream_event), EvaluateTermConflict(term_set,snapshot), ReportBreach(term,evidence), TerminateOrAmendTreaty(treaty,procedure), ExplainTreatyImpacts(treaty,scenario).

- HD3-INV01 조약/조항/참가자/서명·비준·발효/이행의 독립 시점과 유효한 출처.
- HD3-INV02 법/국제적 비준 적격성·다자 발효조건 충족 전 조항 강제 금지.
- HD3-INV03 관련 분야 거래/군사/영토/금전 사건 없이 물품·돈·인구·주권 이동/창출 금지.
- HD3-INV04 TreatyTerm 중복 실행 및 C-M5 PeaceTreaty, C-E5 TradeAgreement 이중 원본 금지.
- HD3-INV05 조약 조항의 충돌·DAG·법적 우선순위와 개정/탈퇴·이력 정합.
- HD3-INV06 미지급 의무와 실제 지급·수송/군사적 이행의 분리 및 채권·채무 보존.
- HD3-INV07 유효 기간·비밀/접근권·역사적 법률/조약 적용 시점 오류 금지.
- HD3-INV08 국가/정부·영토·주권/군사통제/국적/소유권 관계 분리.
- HD3-INV09 조약 종료·국가 승계의 잔존권리/의무·계약 계보 보존.
- HD3-INV10 멱등·원자 전원장 rollback·worker1/N·세이브 재현.

거부코드: HD3_ERR_BAD_SIGNATORY, RATIFICATION_MISSING, ENTRY_CONDITIONS_UNMET, TERM_CONFLICT, INVALID_BENEFICIARY, DUPLICATE_TERM_EXECUTION, INVALID_JURISDICTION, OBLIGATION_UNFUNDED, HISTORY_CONFLICT, ILLEGAL_RETROACTIVITY.

수락: [DIPLOMACY_ACCEPTANCE_TESTS](DIPLOMACY_ACCEPTANCE_TESTS.md)의 HD3-T01~T12. 문서 정본 등록과 실제 C++ 시험/이행 엔진 구현은 구분한다.
