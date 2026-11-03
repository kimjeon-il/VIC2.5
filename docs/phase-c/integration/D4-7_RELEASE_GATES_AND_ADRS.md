# C-D4-7 — 구현·수락 게이트·설계 결정(ADR)

**상태:** C-D4 설계 목표 확정, 실제 통합 구현 및 테스트 실행 전 · `phase-c-integration/1.0` · [색인](README.md)

## 1. 설계와 실제 구현의 독립 상태

| 수준 | 작업 및 필수 근거 | 현재 판정 |
|---|---|---|
| D4-DESIGN | 본 명세 및 30개 불변조건·48개 테스트/수치 fixture·호환성 매니페스트의 문서 일관성 | **설계 문서 정적 검증 대상** |
| D4-L0 | C-P1~P5 정본 등록/버전 해시, `work/pop-phase-b` 실제 코드 기준 확보, C-D0/E/M 기준과 충돌 감사 | **BLOCKED: 정치 파일 아직 GitHub main 미반영, Phase B 전체 통합 전** |
| D4-L1 | World/typed ID·schema·금융/재고·POP·고용/소유 단일 원본을 새 통합 스테이징·롤백에 결합 | NOT_STARTED |
| D4-L2 | 통화/상품·기업·시장/공공·세금·운송을 실제 원장에 연결해 CE 전체 회귀 | NOT_STARTED |
| D4-L3 | 정치 C-P1~P5 법/기관/선거/운동을 POP/경제/월별 9a~9e와 연결, CP 회귀 | NOT_STARTED |
| D4-L4 | 군사 C-M1~M5 인구·병역/부대/장비/군수/전투/점령·배상과 월별10 연계 | NOT_STARTED |
| D4-L5 | [D4-T01~48](D4-5_TEST_MATRIX.md) 및 30개 불변조건/기존 POP/C-D0/E/P/M 회귀·에러주입 통과 | NOT_STARTED |
| D4-L6 | 단일/N-worker 결정론, snapshot+journal/save-replay/crash recovery, Linux/Windows 등 | NOT_STARTED |
| D4-L7 | 1,200개월, 충격 시나리오, 10만/100만 활성 slice 대표부하와 계측/문서·성능 회귀 | NOT_STARTED |
| D4-DONE | L0~L7의 증빙 포함 배포/코드 커밋 고정, 제약/미검증 공지 | NOT_STARTED |

**문서 통과는 런타임 통과가 아니다.** 더 좁은 목적으로 단일 분야를 구현 완료해도 D4 전체 `DONE`으로 승격 금지.

## 2. 선행 작업 및 구현 순서

### Gate L0 — 계약/브랜치 정본 락

1. `C-P1~P5` 사용자 전달용 `phase-c-politics/1.0` 문서/ADR/인수시험을 저장소의 `docs/phase-c/politics/`에 **동일 내용으로 기록**한다. 통합 MD SHA-256 `16ba370c017453011752a7ce7375a10e9b020231c787fb77d7885f042417be5d`와 기존 ZIP/개별문서 해시를 참조해 누락/임의 수치 변경 없이 검수. 30 ADR, 12 CP-INV, 25 CP-T 및 선거/법률 fixture 확인.
2. `work/pop-phase-b`와 main의 POP-D0~D3·기존 S0 코드 관계를 비교해 새 3계층 희소 셀/노동/교육·건강 API의 정본을 결정. 무조건 브랜치 전체를 덮어쓰지 않고 양방향 호환성/회귀 계획 확인.
3. C-D0 공통 `phase-c-common/1.0`, E `phase-c-economy/1.0`, P `phase-c-politics/1.0`, M `phase-c-military/1.0`, `pop-design/1.1`의 명시적 버전/스키마/룰셋 해시를 테스트 manifest로 고정.
4. 12단계의 외부 명칭/순서, 9a~9e와 10a~10f 하위순서 및 정치 선거인/사상자 시간경계의 충돌을 공동 리뷰. Phase B 기존 월내 인구 이벤트 순서와 통합 10단계가 달라지는 경우 migration/회귀를 명시.
5. 코드 구현이 없는 `Political/Military/Fiscal/Trade` 원장을 **실제 구현되어 있는 것처럼 표시하지 않는다**.

### Gate L1 — 공통 World 및 원자적 통합

1. typed IDs, provenance·revision, Source of Truth API, registry migration facade·분기별 단일 author.
2. `StagingWorld` 안에 POP/Employment/Financial/Ownership/Inventory와 정치/군사/관할 state를 함께 담고, `JournalBatch`로 사건효과를 검증. Financial/Inventory가 `Phase B World` 바깥에 남아 단독 commit 되는 구조 금지.
3. 테스트용 fault injection으로 1개 원장에서 마지막 분개 실패 시 **다른 원장/달 인덱스·ID 발행/예약 모두 롤백**. undo-based implementation도 완전한 원자성이 성립한다면 허용; 성능 검증 전 각 단계 전체 World copy 강제하지 않음.
4. `PopulationSlice` 분할/병합과 금융/소유/고용/정치/병역 관계 원장 자동 대조.

### Gate L2~L4 — 분야 연동

- L2(경제): 기업/노동·실제 원료/생산→시장거래/소비·계좌/부채·세금/공공·운송 및 GDP 회계/정합.
- L3(정치): P1 국가/정부/헌정·P2 법률 효력/실제 집행·P3 쟁점/조직·P4 선거/의석/내각 구성·P5 집회/파업/반란의 실제 POP/금융/병역 점유.
- L4(군사): M1 징병/예비군·제대, M2 부대/편제·장비/지휘·훈련, M3 군수/수송, M4 월말 교전·상병/의료, M5 점령·조약·배상/복구.
- 도중 상태는 `ECONOMY_IMPLEMENTED`, `POLITICS_PARTIAL`, `MILITARY_PARTIAL` 등 **세분 상태**로 표시하고 완전 통합 주장 금지.

### Gate L5 — 통합 기능·오류 주입

- [D4-INV-01..30](D4-3_INVARIANTS.md) 전부, [D4-T01..48](D4-5_TEST_MATRIX.md) 및 상위 POP/CE/CP/CM 게이트 대상 실제 엔진 실행·아티팩트/실패 로그.
- 시간/권한 위반, 인구 과다 동원, 동일 현금 다중 예약, 미도착 탄약 사용, 선거구 중복 표, 법률 소급 적용, 포로·실종/사망 이중계수, 점령→주권 자동이전, 배상채권의 유령 현금 등 negative tests 필수.
- 각 실패 케이스는 **예상한 오류코드/사유와 롤백**을 정확히 출력해야 PASS. 일부 예외가 나왔다고 임의 성공 처리하지 않는다.

### Gate L6 — 재현/저장

- 동일 world snapshot·정규 명령/ID·룰셋/seed에서 worker 1/N과 무작위 순회순서 기반 일정에서 **정본 월말 해시** 및 사건 정렬 결과 비교.
- 저장 후 재시작/같은 명령 재전송·Crash injection/Partial checkpoint/Schema migration·오류 거부 시험.
- 숫자정책과 컴파일러/플랫폼 차이를 기록. 고정소수점 외의 실수경로가 남아 있다면 범위 내/범위 외 재현성을 분리해서 보고한다.

### Gate L7 — 100년/대규모/충격

- `1,200`개월 기준·농업·금융·정치·군수/전투·전후복구/복합 충격 시나리오와 stage11 매달 보존 검증, 월별 성능/메모리·event 처리량 로그.
- POP 10만/100만 셀의 대표 월 처리, 런타임·메모리·인덱스 효과 및 체제별 국가/정당/병력 수 증가 스트레스. **10만/100만 인구셀 초기화만 한 것과 100년 실행을 구별**.
- 실측 하드웨어 보고 없이 특정 ms/month·GB·fps 목표를 약속하지 않음. 게임 UI/지도의 렌더링 성능은 이 CPU 시뮬레이션 단계의 인수시험과 분리.

## 3. C-D4 설계결정 기록(20 ADR)

| ID | 결정 |
|---|---|
| D4-ADR-001 | C-D0와 POP-D0~D3가 경제/정치/군사 상위 데이터·시간 계약이며 C-D4는 검증 범위를 확장 |
| D4-ADR-002 | 통합 WorldSnapshot/월간 StagingWorld·JournalBatch를 분야별 정본의 유일한 commit 경로로 지정 |
| D4-ADR-003 | Phase B의 별도 고용·인구/구형 S0 고용은 이식하여 단일 정본으로 전환 |
| D4-ADR-004 | 실물(Inventory), 인구(POP), 화폐/채무(Financial), 소유(Ownership), 제도/선거(Political), 배속(Military), 지역관할(Jurisdiction) 소유를 분리 |
| D4-ADR-005 | 명령/계획/승인/법률/예상 피해와 실제 금융/상품/인구 효과를 엄격히 구분 |
| D4-ADR-006 | 공개 12단계 유지, 단계0 개시 정합·9a~9e 정치 순서·10a~10f 인구/전투 위험집합/동원 순서를 명시 |
| D4-ADR-007 | 월 m 정치채택→기본 m+1 법 효력, 월말 징집/제대→차월 고용·전투인원 |
| D4-ADR-008 | 당월 이미 유효하게 투표/급여 지급/노동한 사람이 뒤 단계 전사/동원해도 이전 사건을 소급 무효화하지 않음 |
| D4-ADR-009 | 동일 인원의 주된 노동/복무·같은 계정 예약·실물/장비·선거중복 등 **자원별 실제 상호배제 규칙**을 별도로 평가 |
| D4-ADR-010 | 인구·화폐·실물의 보존조건 위반은 HARD 실패, 경제위기/정부교체/패전 자체는 오류가 아님 |
| D4-ADR-011 | 군사통제/주권/민정/과세권 및 국가/정권/부채 승계는 별도 사건·유효기간/권한 |
| D4-ADR-012 | 금융·재고·군수·정치사건은 source_event_id/typed transaction_id로 원장 교차참조 |
| D4-ADR-013 | 병렬 worker는 사건제안만 독립작성, 전역 정렬·자원예약/원자적 commit은 단일 정본 경로 |
| D4-ADR-014 | 고정소수점 정수·checked arithmetic·versioned RNG/반올림·stable ID로 결정론 범위를 명시 |
| D4-ADR-015 | 저장은 원자 snapshot+event journal+commit marker+version/hash, 실패한 중간월 재생 금지 |
| D4-ADR-016 | 실제 C++ 통과/벤치마크 전까지 문서 설계 검증과 통합 엔진 검증 결과를 명시적으로 분리 |
| D4-ADR-017 | 사용자가 전달받은 정치 설계는 해시/버전 관리 후 GitHub 정본 등록을 L0 필수 선행조건으로 지정 |
| D4-ADR-018 | 기준 48개 교차 시험과 30개 전역 불변식 외에도 기존 POP/CE/CP/CM 회귀를 유지 |
| D4-ADR-019 | 성능 수치/장기·실측 역사 보정은 컴파일러/플랫폼·하드웨어/출처/수치 규칙 확인 후 결론 |
| D4-ADR-020 | 기능·회계·재현/저장·성능·정본 호환 L0~L7 증빙이 모여야 D4-DONE을 선언 |

## 4. 인수보고서 최소 형식

```text
ReportId, date, repo_url, commit_sha, source_branch
contract_versions + hashes, ruleset_hash/numeric_policy_version
toolchain(os/compiler/compiler flags/dependencies), hardware(cpu/ram), seed
d4_design_check: verified/unverified
L0..L7 status: PASS / FAIL / BLOCKED / NOT_RUN
POP/CE/CP/CM/D4 test results: passed/failed/skipped/blocked and log links
invariant failures, first failing month/stage/event, rollback trace
monthly performance distribution, maximum RSS/alloc bytes
save/load/cross-platform fingerprint comparisons
migration/known limitations, explicit unresolved blockers
final recommendation: implemented_accepted or needs_work
```

**중요:** C-D4가 제공하는 것은 완료기준과 검증 계획이다. 현재 실제 통합 엔진/전 분야 1,200개월 실측 로그가 없으므로 `D4-DONE`은 NOT_STARTED이며, L0는 미충족이다.
