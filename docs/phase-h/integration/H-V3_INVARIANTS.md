# H-V3 — 외교·전 분야 교차 불변조건 36개

> phase-h-integration/1.0 · 2026-11-04 · SPECIFIED / NOT_RUN  
> HV-INV01..HV-INV36은 기존 HD0-INV01..20, HD1~HD7의 세부 불변식과 상위 C-D4/D-V/E-V/G-D0 조건을 약화시키지 않는다.
> 각 항목은 해당 원본 원장/계약의 사건과 실제 WorldSnapshot 변화로 판정한다. UI 값만 확인하고 PASS로 처리하지 않는다.

## 0. 검증 오라클 정책

- 하위 명령 거절(무권한, 예산/FX·관세/조약 권한 부족)은 도메인 Reject/Unsupported가 기대 결과다. 정본 불변식 위반·고아참조·중복 돈/상품/인원/주권은 HARD ERROR + AbortMonth.
- Money/Quantity/People의 정수/고정소수점·checked intermediate arithmetic, 각 통화별 복식부기, 실제 물량/인구 보존 및 계약의 미이행 잔액을 검증한다.
- 검증 기준: 최초 snapshot hash, 단계별 ProposedEvent 및 CommittedEvent, source_event/term/obligation/transaction ID, 각 원장 delta, 최종 snapshot/id allocator/commit_id, 관련 cache revisions.
- UI read-only 파생 데이터의 잘못된 값은 state invariant와 별개로 오라클 실패지만 해당 cache 자체가 원본 진실값이 되어서는 안 된다.

## 1. 국가·외교권한·국제법적 적격성 (HV-INV01..HV-INV06)

| ID | 전역 불변조건 | 검증해야 할 정본/오라클 |
|---|---|---|
| HV-INV01 | 국가와 정부·기관/공인 주체의 typed ID와 생명주기 분리 | Country/Government/StateOffice/RecognizedActor/InternationalOrganization; 정부교체 전후 Country/기존 의무 ID 동일 |
| HV-INV02 | 외교명령/조약비준·전쟁승인 권한의 지역·시점·헌정 절차 검증 | C-P AuthorityGrant, H-D4 rights; 무권한 결정은 정상 Reject, 원본 불변 |
| HV-INV03 | 외교승인/비공식 접촉과 확정 영토주권/정부집권 분리 | Recognition과 E Province sovereignty, C-P Government 별도 |
| HV-INV04 | 법률·조약/관세·공동의결의 범위·예외·중복/충돌 검증 | H Term/C-P Law/G PolicyProjection, 동일 겹친 scope에서 상충된 effect 무단 집행 없음 |
| HV-INV05 | 다중 종속/조직 가입은 허용하되 배타적 권한·헌장상 충돌과 위임 사이클 거부 | H-D4 rights matrix·H-D5 charter/authority graph; 적법 JointAuthority 예외 |
| HV-INV06 | 정보 접근·외교통신/비밀/시기별 법률제약으로 국가 AI가 전지적 상태를 사용하지 않음 | H-D1 actor information_as_of 및 공개범위, C-P/POP 외교기관 자원 |

## 2. 조약·협상·효력시점·의무 이행 (HV-INV07..HV-INV12)

| ID | 전역 불변조건 | 검증해야 할 정본/오라클 |
|---|---|---|
| HV-INV07 | 협상 제안·수락과 서명/비준·발효/부분정지·이행/위반을 독립 사건으로 기록 | H Negotiation/Treaty/Term/Obligation + C-P, 조항별 effective_interval |
| HV-INV08 | m stage9 신규 외교조약/입법은 원칙 m+1 stage0 발효하며 당월 과거 거래/전투 변경 금지 | C-D0 0~11, C-P9a~e/C-M10a~f, snapshot hashes |
| HV-INV09 | 다국가·다조항 발효선행조건/동의율·조항 DAG/법률상 잔존의무를 검증 | H-D3 participation/term dep, orphan term·cycle 금지 |
| HV-INV10 | 이미 유효한 조약의 경제·군사·영토 적용을 domain/term/source event마다 한 번만 집행 | C-E5 TradeAgreement/C-M5 PeaceTreaty와 H 간 단일 source effect key |
| HV-INV11 | 조약 만료/탈퇴·정부교체/국가승계 뒤 채무·부속 계약/조항의 법정 존속·계보 보존 | H-D3/D4 succession, C-E3 Financial/Ownership, C-P Country lineage |
| HV-INV12 | 정보 도착/협상 기한/위기 시한 경과는 자동 전쟁·주권/재정 이전이 아님 | H-D2/H-D7 messages/ultimatum, C-M5 EntryToWar & E changes only authorized |

## 3. 금융·산업·무역·재정과 외교의 접속 (HV-INV13..HV-INV18)

| ID | 전역 불변조건 | 검증해야 할 정본/오라클 |
|---|---|---|
| HV-INV13 | 외교·국제기구 비용/조약 배상은 재정 발생채권·금융 실제 지급을 구분 | FiscalAssessment/FinancialLedger; commitment != cash; 복식부기 수지 |
| HV-INV14 | 조약/제재·통상특혜는 G TradePolicyProjection 한 번 적용하며 C-E5 세관/원장 중복 금지 | term×jurisdiction×shipment×period에서 unique application |
| HV-INV15 | 실물 군사/인도·상품원조와 외교 계약 약속을 분리하고 Inventory/Shipment 보존 | InventoryLedger 원본; 물리적 운송 중 재고 소유/보관자 일관 |
| HV-INV16 | 무역·투자/기구기금·공사/기술 협약이 실제 금융/노동/설비·국제결제와 제약/예약을 공유 | C/D cross reservation; 한 원자 자원 두 분야 이중 사용 금지 |
| HV-INV17 | 환율·다중 통화·금융/보험·배상으로 이중 화폐 창출·무상 채무 면제 금지 | Money currency_id, FXTrade, Ownership debt, FinancialJournal |
| HV-INV18 | 경제 충격/관세·공급망/세수·영향력 보고에서 동일 거래/피해의 이중계상 금지 | G-C-E3/D source_event lineage & evaluation basis; no country GDP duplication |

## 4. 영토·인구·군사·국제적 지위 (HV-INV19..HV-INV24)

| ID | 전역 불변조건 | 검증해야 할 정본/오라클 |
|---|---|---|
| HV-INV19 | ProvinceId/정적 형상 유지, Claim/Dispute와 확정 sovereignty의 동시 이중정본 금지 | E registry/Claim; 영토이양은 유효 Province/role/time로 별도 사건 |
| HV-INV20 | 주권·민정·과세권·군사통제와 법적 종속/보호국·실제 점령을 구분 | E-D0 4 jurisdiction roles, H-D4 rights, C-M occupation |
| HV-INV21 | 영토할양/독립·국가분열에서 실제 POP/국적·소유/부채·고용/군 배정을 명시적 사건 없이 이전/복제 금지 | PopulationSlice, Ownership/Financial/Employment/ServiceAssignment |
| HV-INV22 | 국제위기·동맹/조약 선언과 실제 전쟁·참전/철수·전투 피해/포로는 C-M5 원본 참조 | Conflict/EntryToWar/ServiceAssignment/POP events |
| HV-INV23 | 병력·군수품·지휘/수송·국제 원조는 실제 인구·재고·예산·경로 용량 범위 내에서만 실행 | POP/Inventory/Financial/Military/F/C-E5 reservation & consumption |
| HV-INV24 | 평화/배상·영토조항은 C-M5 PeaceTreaty/E jurisdiction/Financial와 동일 효과 1회, 원자적·분할 실행 조건 준수 | PeaceTreaty/SettlementObligation/Term/Province/Journal linkage |

## 5. 국제협력체·영향력·국제위기 (HV-INV25..HV-INV30)

| ID | 전역 불변조건 | 검증해야 할 정본/오라클 |
|---|---|---|
| HV-INV25 | 국제협력체 헌장·가입/탈퇴/투표권·정족수/거부권/지도권은 H 정본 조약과 일치 | Organization/Member/Vote/Decision; 회원 하나 1투표권 규칙이면 이중표 없음 |
| HV-INV26 | 국제협력체 예산/시설·직원·공동기금은 실제 금융/소유권·POP 고용 원본만 사용 | Org account OwnerRef supported required; 없으면 Unsupported / no free money |
| HV-INV27 | 국제협력체 결속력·A→B 영향력은 근거 사건에서 파생, 직접 증감 포인트·가입/세력권 간 자동 관계 없음 | H-D5 cohesion/H-D6 influence read-only, origin refs/revisions |
| HV-INV28 | 영향력의 경제·군사·정치·문화 방향성·중첩/unknown·대체수단은 독립 차원으로 검증 | Influence(A→B) ≠ Influence(B→A), overlap allowed, no automatic concession |
| HV-INV29 | 국제위기의 개시/진행/부분합의/동결/중립·중재는 실제 분쟁/조약·정치·경제 사건에 근거 | H-D7 crisis issue refs, C-P/E/G/M source events, no fake state/army |
| HV-INV30 | 위기 위험/긴장도 임계치 및 공동 방위 조항만으로 자동 전쟁/일률 진영 동원 금지 | C-P authority+C-M conflict event, organization members can abstain |

## 6. 원자성·결정론·세이브·성능·설명 (HV-INV31..HV-INV36)

| ID | 전역 불변조건 | 검증해야 할 정본/오라클 |
|---|---|---|
| HV-INV31 | 모든 참여 분야는 하나의 월 StagingWorld/JournalBatch와 commit/abort; 부분 commit 불가 | WorldSnapshot/commit_id, H/C/E/G/D/POP/군사 분야별 hash |
| HV-INV32 | 무효 요청·중복 Command/Event/Term 이행은 멱등, 실패 후 ID allocator/월·저널/예약 완전 복구 | same replay, no duplicate obligation, refunded reservation |
| HV-INV33 | 월초 스냅샷/원본 revision·DAG 위반, stale 영향력/결속·위기 위험 캐시가 실효 거래를 바꾸지 않음 | snapshot+revisions/causal trace; no stage back-edge |
| HV-INV34 | 동일 입력·seed·규칙/actor information_set에서 worker1/N, 재시작 replay의 정수 상태/사건 해시 결정론 | deterministic ordering, fixed-point, no nondeterministic hash iteration |
| HV-INV35 | 설명·UI 툴팁의 예상/실현/미상과 영향 원인·실제 비용/시간차·정보권한이 엔진 조회와 일치 | forecast snapshot vs committed event trace; secrecy not leaked |
| HV-INV36 | 장기/성능 부하에서 불변식 유지·계측/병목·메모리 누수 감시; 단순 경제불황/전쟁패배를 엔진 실패로 오인 안 함 | 1,200개월/국가·조약·조직·위기/POP/프로빈스 대표부하, measured budget predeclared |

## 7. 검증 시점과 수락 경계

1. Stage0은 유효 국가·정부/조약·보호권/기구·영토관할·시장/군사/정치 법률을 freeze하고 key/revision consistency를 확인한다.
2. Stage1은 예상 능력·법적 자격/자금·상품·행정·군수/통신·운송의 유효 예약과 G/H 이중 예약 거부를 확인한다.
3. Stage3/5/7/8은 실제 Trade/Inventory/Financial/Fiscal/Employment/Ownership/Shipment의 대응 사건 및 조약 의무의 이행량을 대조한다.
4. Stage9은 서명/비준/국제협력체 의결·위기 협상이 과거 stage 효과를 바꾸지 않았는지, m+1 유효예약만 생성하는지 검증한다.
5. Stage10은 C-M/POP/E 점령/인구/부대 사건과 H 위기·조약의 인과 링크를 확인한다.
6. Stage11 전원장 재대조 후 commit. 실패 시 모든 원장, stage별 외교/기구/위기 상태, 월/ID/캐시/저장 journal까지 사전 hash와 동일해야 한다.

**완료 기준:** 정본 36개 불변식은 실제 H-V5 교차시험(72개) 및 상위 104개/POP/C/D/E/G 회귀에서 반복 검사해야 한다. 이 문서 작성이 36개 불변조건의 런타임 검증 수행을 뜻하지 않는다.
