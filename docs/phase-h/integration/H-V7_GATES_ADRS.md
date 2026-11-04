# H-V7 — 외교 통합 구현·인수 게이트와 ADR 30개

> phase-h-integration/1.0 · 2026-11-04 · DESIGN_SPECIFIED / NOT_RUN
> H-V 설계는 36개 전역 불변식, 72개 신규 교차시험, 6개 수치 fixture, 30개 ADR 및 총 176개 외교 시험을 포함한다.
> **실제 C++ H 엔진 구현·104개 기존 시험/72개 신규 교차시험/100년·부하·복구 측정은 수행하지 않았다.**

## 1. 선행 계약·버전 잠금

- 지정 작업 브랜치: work/phase-g-h-d0. 원본 기준선: work/phase-e-cpp-integration, Phase D 설계 정본: work/phase-d-design(별도); 현재 main에 자동 병합되었다고 가정 금지.
- 상위 버전: pop-design/1.1, phase-c-common/1.0, phase-c-economy/1.0, phase-c-politics/1.0, phase-c-military/1.0, phase-c-integration/1.0, phase-d-common/1.0, phase-d-integration/1.0, phase-e-common/1.0, phase-g-common/1.0, phase-g-trade/1.0, phase-h-common/1.0, phase-h-diplomacy/1.0, phase-h-integration/1.0. 실제 코드에서 지원하는 정확한 minor/major를 L0에서 확인하고 잠금.
- Phase F 독립 물류 모듈과 조직 계좌 typed OwnerRef, E JointAuthority, 위기 긴급 sub-tick 등은 지원 여부를 실제 코드/정본과 비교하고 미지원이면 **BLOCKED/Unsupported**로 명시. 해당 경우에 임시 fake shipment/fake treasury/instant-treaty로 테스트를 통과시키지 않는다.
- 상위 C-E5 TradeAgreement와 H TreatyTerm, C-M5 PeaceTreaty와 H 평화조항은 L0 owner map/adapter의 단일 원본·schema migration plan을 필수로 한다.

## 2. 릴리스·인수 게이트

| 게이트 | 필수 실제 증거 | 실패/미지원 판정 |
|---|---|---|
| **HV-L0 — 계약·통합 소스 기준선** | B/C/D/E/G/H 코드/설계 정본·typed IDs·원장 owner map/버전/세이브 호환, F 어댑터·C-P 9a~e/C-M 10a~f 이벤트 슬롯·기초 빌드 확정 | 미통합 코드/이중 원본·구조형 불호환은 BLOCKED_FOR_CPP |
| **HV-L1 — 단일 World/원장·효력·원자성** | 외교의 모든 사건이 C-D0 Stage0~11/원장·공통 예약/JournalBatch와 함께 원자 commit/abort, 실패가 실제 POP/돈/재고·군/주권/시간/ID까지 복구됨 | 단계별 부분 commit/조약 소급 변경이면 FAIL |
| **HV-L2 — H-D1~D7 실제 기능 연결** | 관계·서신/협상·조약 비준/실효·종속 위임·국제협력체 회원/기금/표결·실제 자원 기반 영향력·위기/중재/평화 및 국내법/군수/무역/지리 연결, 적어도 대표 인수 시나리오 실행 | 명목 객체/무료 보너스 또는 G/M5/E 어댑터 미지원이면 해당 사례 BLOCKED |
| **HV-L3 — 시험·상위 회귀** | H-D0 20 + H-D1..7 84 = 104, H-V 신규 72 = **176개 고유 외교 시험 각각 실제 PASS**. 관련 POP, C-D4 48, D-V, E-V, G-T 등 기저 회귀도 같은 코드 기준에서 실제 PASS 증빙 | 타 브랜치 제한 스모크 PASS/정적 fixture만으로 176/176 선언 금지 |
| **HV-L4 — 저장·결정론·오류 복구** | worker1/N·Linux/Windows 실제 snapshot/ledger/event hashes 동등, save/load/replay 및 계약/데이터셋 mismatch·crash recovery·한달 전역 rollback 로그 | 일부 분야만 복구·누락 참조/유령 이벤트/플랫폼별 다른 세계결과이면 FAIL |
| **HV-L5 — 장기·성능·설명·역사규칙** | 1,200개월×복수 시드/재생, 외교/무역·전쟁/정치·POP 복합 장기 부하, 국가/조약/기구·위기 수 기준 성능·메모리 실측 및 사전 지정 SLA, UI/AI 정보 접근·툴팁 설명·시대별 법률/외교제도 검증 | 사후 임의 완화된 예산, 정보유출, 현대법 무차별 소급, 성능 실측 누락은 NOT_ACCEPTED |

**HV-COMPLETE**는 모든 HV-L0~L5=PASS일 때만 가능. 법적·회계적 불변식을 실제로 깨는 사례가 하나라도 있으면 사후 PASS 선언 불가. 월간 시뮬레이션이 경제적 적자/전쟁 패배/조약 무효/국제위기 장기화로 끝났다는 사유만으로 FAIL 처리하지 않는다.

## 3. 시험 번호와 합계·메타게이트

- 기존 H-D0 시험 HD0-T01..20 = 20 고유 ID.
- 기존 H-D1..D7 시험 HD1-T01..12 등 = 7 × 12 = 84 고유 ID.
- 신규 HV-T01..HV-T71 = 71 **독립 교차 인수시험**.
- **HV-T72 = 통합 증거 메타게이트**: 71개 교차 + 기존104 + 요구된 동일코드 기준 상위 POP/C/D/E/G 회귀의 실제 로그/ID·계약 해시/정합을 확인. 이 메타게이트의 PASS는 위 필수 증거의 존재/정확성 검사이고, **HV-COMPLETE는 추가로 HV-L4~L5까지 완료**해야 한다.
- 통계: 신규 71 + 메타 1 = 72, 기존 104, 총 **176 distinct IDs**. 상위 C/D/E/G 개별 회귀 시험 개수는 외교 176에 합산하지 않는다.
- 기존 D-V 279개, E-V 60개 등 타 Phase의 '설계된 시험'은 실제 실행·PASS 증빙으로 대체되지 않는다. 현재 E-V는 47 bounded PASS/13 BLOCKED의 부분 구현 기록을 유지; H-V 개발이 이를 자동 해결하지 않는다.
- 시험 하나가 정상 거절/해외기업 파산/위기 동결/국제기구 탈퇴를 의도한 경우는 그 실제 상태와 원장/권한을 검증했다면 PASS 가능. 결함 오류는 적법한 Reject와 구분한다.

## 4. 설계 결정 기록(HV-ADR001..030)

| ADR | 분야 | 확정 결정 | 배제한 대안/위험 | 적용 게이트 | 증거 |
|---|---|---|---|---|---|
| HV-ADR001 | 단일 세계/원장 | H 외교 모듈을 C-D0의 단일 StagingWorld에 병합하고 독립 월 커밋/자금원장을 만들지 않음 | 전 분야 롤백/거래 일원화와 실제 시점 | HV-L1 | World stage/commit hash |
| HV-ADR002 | 원본 우선순위 | POP/C-D0/C-E/C-P/C-M/D/E/G/H-D0 원본 의미가 상충하면 명시 ADR/버전업·이관 승인 전 상위 원본 유지 | 조용한 이중 원장·정치/영토 충돌 방지 | HV-L0 | owner map + migrations |
| HV-ADR003 | 조약 이관 | C-M5 PeaceTreaty·C-E5 TradeAgreement와 H TreatyTerm은 adapter로 1회 연계, 대체 시 버전/저장 마이그레이션 필수 | 배상·관세/종전의 중복 적용 방지 | HV-L0/L2 | unique term/event refs |
| HV-ADR004 | 국가/정부 | Country의 존속·정부 교체·인정/승인·법적 대외권한을 서로 다른 정본으로 유지 | 정권교체 즉시 영토/조약/외채 사라짐 방지 | HV-L2 | C-P IDs/authority |
| HV-ADR005 | 공간·주권 | E 고정 Province와 Claim/Dispute·sovereignty/민정/과세/군사통제 역할을 분리 | 외교적 분쟁/점령이 자동 주권 이동하는 오류 방지 | HV-L2 | E jurisdiction queries |
| HV-ADR006 | 월간 시점 | C-D0 0~11단계 및 9a~9e/10a~10f 규약을 변경하지 않고 m stage9 → m+1 stage0 효력 | 이미 종료된 경제/전투 소급반영 차단 | HV-L1 | event DAG/month snapshots |
| HV-ADR007 | 단계내 순서 | H의 하위 stage 이벤트는 기존 C-P/C-M에 정합한 dependency DAG로 매핑 | H가 정치·군사 하위단계를 덮어씀 방지 | HV-L0/L1 | substage compatibility table |
| HV-ADR008 | 조약 생명주기 | 협상/서명·비준·발효/지급·종료/위반을 다른 상태/유효시점으로 유지 | 명목 조약·확정 돈·영토 효과 혼합 방지 | HV-L2 | Term/Obligation states |
| HV-ADR009 | 정보 도착 | 외교서신·비밀조항은 국가별 정보 접근/통신·타임스탬프에 구속 | AI 전지성·비밀누출 차단 | HV-L2/L5 | information-set traces |
| HV-ADR010 | 복합 원자성 | 동시 이행 패키지와 단계별 의무는 명시 분리, 필수 동시 그룹 중 일부 실패하면 전체 rollback | 돈 지급만 성공하고 주권 이전 실패한 부분 커밋 방지 | HV-L1/L3 | transaction batches |
| HV-ADR011 | 국제협력체 | 조직은 Treaty Charter·Membership/Organ/Decision을 가지는 국가 간 공식 제도 | 단일 세력블록 슬롯/무근거 보너스 방지 | HV-L2 | Org charter/member records |
| HV-ADR012 | 중복 가입 | 국가의 국제협력체 다중 가입 허용; 실제 배타적 의무만 충돌 검증 | 상호 다른 경제·군사 조직의 현실적 공존 | HV-L2 | charter compatibility |
| HV-ADR013 | 조직 기금 | 기구 법적 경제주체는 C-E3 OwnerRef/FinancialAccount가 지원할 때만 사용, 회비는 Fiscal 의무와 금융 지급 분리 | 조직 설립만으로 현금/시설 공짜 생성 금지 | HV-L0/L2 | Financial/Contribution ledgers |
| HV-ADR014 | 공동 정책 | 조직 의결은 조약/국내 권한을 통해 G/군사·재정/기술 원장이 따로 실행 | 투표 찬성=자동 참전/기술 개발 방지 | HV-L2 | OrgDecision→domain refs |
| HV-ADR015 | 결속력 | 국제협력체 결속력은 투표·조약이행·실제 무역/안보 의존에서 파생 | 조직 경험치 직접 버튼 금지 | HV-L2/L5 | read-only contributions |
| HV-ADR016 | 국가 간 영향력 | Influence(A→B)는 다차원/비대칭·read-only 평가, 세력권은 UI 설명만 | 관계 수치 강제 동조/세력권 법적 지위화 금지 | HV-L2 | InfluenceAssessment cache |
| HV-ADR017 | 영향력 원인 | 무역·투자·차관/군지원·교류 중 동일 underlying 사건은 중복 평가하지 않음 | 위험/협상압박 이중계상 제거 | HV-L2/L3 | source event dedupe |
| HV-ADR018 | 영향력 지연 | 관측 snapshot/revision과 changed-set으로 캐시를 갱신, 당월 G↔H self-loop 금지 | 결정론·성능 및 인과 시간 보존 | HV-L4/L5 | cache invalidation log |
| HV-ADR019 | 압박·협상 | 영향력은 상대의 비용/대체수단·선택권을 평가하는 설명 입력, 일정 문턱에서 자동 수락 금지 | 내부 정교함·게임 조작 단순화 유지 | HV-L2/L5 | AI offer evaluations |
| HV-ADR020 | 영향권 중첩 | 경제/안보/문화 중첩 지도는 영토/국제협력체 소속 원장이 아닌 파생 레이어 | 여러 국가가 같은 국가에 다른 방식 영향력 행사 가능 | HV-L5 | UI overlay + E hash |
| HV-ADR021 | 위기 발생 | H 국제위기는 E Claim/C-P 정치·G 경제/H 조약위반/CM 군사 사실에 근거해 생성 | 없는 국가·지역/반란군·전쟁 사건 생성 방지 | HV-L2 | crisis cause refs |
| HV-ADR022 | 위기 참여 | 열강 독점이나 두 진영 강제를 폐기, 소국·중립·중재·조건부/다자 참가 허용 | 복합적 외교관계와 평화적 결과 지원 | HV-L2/L3 | CrisisPosition rights |
| HV-ADR023 | 위기 자동전쟁 금지 | 위험/긴장도는 파생 추정, 실제 전쟁·참전은 C-P/C-M5 권한·원장 사건에 한정 | 무근거 자동 병력·사망/전시경제 효과 제거 | HV-L2 | C-M5 Conflict checks |
| HV-ADR024 | 평화 정본 | 조약상 평화·배상·영토·군축은 H 법적 조항과 C-M5/E/금융 실제 이행을 단일 참조로 결합 | PeaceTreaty/Financial/Jurisdiction 이중 생성 방지 | HV-L2 | peace adapter + obligations |
| HV-ADR025 | 단일회계 | 모든 금전·재고·POP 인구/군인 및 기업 설비 자원은 기존 원장의 잔액/예약/실제 사건으로 검증 | 외교·기구/군사·무역 자원 마법 생성 금지 | HV-L1 | Financial/Inventory/POP balances |
| HV-ADR026 | 결정론 | 정수/고정소수점·stable event order/seed stream, worker1/N·OS별 결과 해시 동일 | 병렬 외교·위기/시장 경쟁에서 임의 진행 방지 | HV-L4 | canonical world/journal hashes |
| HV-ADR027 | 저장·복구 | H 상태를 C/D/E/G 동일 commit_id로 보관, 비호환 version/data hash 명시 거부 | 정본 상실·old save 조용한 초기화 방지 | HV-L4 | save/restore/crash fixtures |
| HV-ADR028 | 장기 부하 | 국가/시장/기구/조약/위기·POP 복합 1,200개월 및 대표 대규모 부하를 실제 측정 | 문서 설계=완성형 게임 검증이라는 착시 방지 | HV-L5 | run logs CPU/RSS/transactions |
| HV-ADR029 | 설명가능성 | 사전 확정/예상·실현, 불확실성과 비밀정보, 인과/경제·군사 파급의 툴팁 조회를 엔진과 공유 | 복잡한 연산을 UI 중복 수치 조작으로 전가하지 않음 | HV-L5 | Explain calls/assertions |
| HV-ADR030 | 릴리스 기준 | 104 외교 기존 시험+72 신규 교차, 상위 POP/C-D4/D-V/E-V/G 회귀, 저장·성능 증빙이 없으면 HV-DONE 금지 | 부분 프로토타입·정적 문서의 합격 과장 방지 | HV-L3/L5 | test manifest + proofs |

## 5. 정적 설계 수락 조건과 실제 구현 상태를 분리

**설계 수락(HV-DESIGN):** H-V1~V7 문서가 존재하고, HV-INV01..36/HV-T01..72/HV-ADR001..030의 번호·하위 계약 링크가 중복 없고, 6개의 기계판독 JSON fixture에 기대 원장/유효월/금액·영토·보존 오라클이 정의되어야 한다. 정적 JSON 검산/링크·문서해시를 verification_manifest.json에 보존.

**구현 수락(HV-DONE):** H-V-L0..L5와 상위 회귀·저장/오류 복구·1,200개월/성능/설명 검증의 실제 실행 결과가 필요. 설계만 GitHub에 등록한 상태에서는 HV-L0는 코드기준 감사가 미완이므로 BLOCKED_FOR_CPP 또는 NOT_RUN, HV-L1..L5는 NOT_RUN이다.

- 실행/결과 증빙: branch/commit & contract version/hash, 시험 fixture SHA, OS/compiler/CPU/memory, seed, 인원/국가/시장·기관/조약/위기 규모, 월/stage별 time/Finance/Inventory/POP/Military/Jurisdiction/Treaty/Organization/Crisis event hashes, assert PASS/FAIL/BLOCKED/NOT_RUN, 오류/미지원 사유.
- 실패가 발생한 시험을 단순 '생략' 처리하고 통과율만 제시하면 인수 무효. BLOCKED/NOT_RUN의 선행조건·후속 구현 과제·수량을 명시한다.
- 위험한 기능 지름길을 허용하지 않는다: 문서만으로 완성형 외교 AI 주장, H가 C-M 군사/Finance E 주권 직접 쓰기, 영향력 포인트/세력권 소속 원본, 위기 100 자동 전쟁, 조직 무료 공동예산/무역 보너스 금지.

**현재 판정:** HV-DESIGN은 문서/매니페스트 정적 확인 후 확정 가능, HV-L0 BLOCKED_FOR_CPP, HV-L1~L5 NOT_RUN, HV-COMPLETE NOT_DONE.
