# VIC2.5 D-V7 — 릴리스 게이트·통합 구현 순서·ADR

> **설계 확정 / 실제 통합 C++ 구현·72개 인수시험 미실행** · `phase-d-integration/1.0` · 2026-11-04
> 상위: [V1 아키텍처](D-V1_ARCHITECTURE_SCOPE.md)·[V2 시간/DAG](D-V2_EXECUTION_DAG.md)·[V3 36 불변식](D-V3_INVARIANTS.md)·[V4 예제](D-V4_SCENARIOS_ORACLES.md)·[V5 72개](D-V5_TEST_MATRIX.md)·[V6 복구/성능](D-V6_DETERMINISM_RECOVERY_PERFORMANCE.md).

## V7-1. 설계 완료와 실제 엔진 통과 구분

| Gate | 합격에 반드시 필요한 증빙 | 현 상태 |
|---|---|---|
| **DV-DESIGN** | V1~V7, 원본·단계·36 불변식/72 시험/30 ADR, 합성 fixture와 문서/계약 해시의 정적 검증 | **DESIGN CONFIRMED** (문서/정적 검산에 한정) |
| **DV-L0 기준선/계약** | `work/phase-c-engine-integration` Draft PR #6과 `main`·`work/phase-d-design`의 실제 코드·Phase B/정치 정본/버전/ABI/source hash 감사; 단일 World/원장 owner/기능 프로파일 락·작동 C++ baseline 선택 | **BLOCKED_FOR_CPP** (현재 단일 코드 베이스가 병합/고정되지 않음) |
| **DV-L1 원장/타입 통합** | D-C/D-I/D-T 객체/관계/JournalBatch가 Phase C World와 같은 staging/commit/abort·stable IDs/원장 참조로 연동. 강제 실패시 모든 수정 롤백 | NOT_RUN |
| **DV-L2 기능 연동** | 실제 시공·WIP/준공 → 산업/채취·시장/기계화 → 지식연구/전파·시설도입/POP 자격·정치/군수·환경 순으로 교차 엔진 구현. 미지원 I2-SAFE 정확히 Reject | NOT_RUN |
| **DV-L3 인수/오류 시험** | `DV-T01..72`(72), `DV-INV01..36`, 상위 `DD0-G01..24`, `DC-T01..45`, `DI-T01..45`, `DT-T01..45`, `D4-T01..48`, POP/CE/CP/CM 해당 회귀·모든 expected-failure 코드/rollback PASS | NOT_RUN |
| **DV-L4 재현성·세이브/복구** | worker1/N·동률/순회순서/RNG·Windows/Linux/가능 macOS, snapshot/journal/save/replay·실제 crash/부분 commit/계약 버전 거부 및 오류 추적 PASS | NOT_RUN |
| **DV-L5 장기·성능** | baseline·흉작/금융/산업/전쟁·재건/기술유실·다국 제도 1,200개월, 10만/100만 활성 POP+복수 산업·연구/시공·국방 대표 실제 이벤트/메모리·실측 P50/P95/P99, 무 silent skip | NOT_RUN |
| **DV-COMPLETE** | L0..L5와 DV-T01..72·상위 의무 테스트 전부 PASS, 코드 commit/ruleset/seed/OS/하드웨어/로그/성능·미지원 기능/알려진 제약 보고 | NOT_RUN |

**구분:** `DV-DESIGN CONFIRMED ≠ DV-T72 PASS ≠ DV-COMPLETE ≠ C-D4-DONE ≠ 완성형 게임`. L0의 현재 `BLOCKED_FOR_CPP`는 정적 설계 작업의 실패가 아니라 실제 C++ 소스 기준선을 결합해야 한다는 명시적 구현 선행조건이다. 사용자 승인 없는 `main` 병합은 수행하지 않는다.

## V7-2. 구현 순서·리스크 대응

1. **L0 계약 기준선 고정:** C-D4 통합 소스/프로토타입 48 PASS 보고서의 실제 SHA/커밋·시나리오 범위를 확인하고 D-D0/DC/DI/DT 스키마·원장 owner/Phase B adapter 정합성 검수. 정치·군사 실제 미구현 부분을 목업/가짜 PASS로 덮지 않음.
2. **L1 단일 원자성:** 새 타입/레지스트리/관계는 C-E1/POP/원장 소유자가 쓰는 실제 `JournalBatch`로 반영. ID allocator 포함 장애주입 롤백·중복 이벤트/파생 캐시 교차 검증.
3. **L2 기능 세로슬라이스:** `철강→자재 인도→실제 시공/기성WIP→차월 시설가동→산업 생산`을 먼저 연결. `실제 연구/검증→지식 차월 효력→교육·면허→기계구입/개축→사업장 도입`과 결합. 이후 정치법/관세·교육/의료·군수/전시충격 순으로 확장.
4. **L3 수락:** 4개 대표 fixture와 72개 DV-T를 실제 자동 테스트로 작성해 개별 예상 Journal/지갑·자재/자산·POP/지식/법/군수·rollback 검증. 기존 207개 정수 테스트 ID(DD0 24 + DC45 + DI45 + DT45 + D4 48) 외에도 해당 POP/CE/CP/CM 회귀 전부 유지. 전체 지정 ID는 **72+207=279**, 모두 실제 통과 전까지 PASS로 표시 불가.
5. **L4 저장/병렬:** 동일 snapshot/명령/seed의 worker1/2/4/8·RNG/정렬 결과 fingerprint, save/load/checkpoint/replay·버전 거부·오류/중단 후 원자복구를 실제 플랫폼에서 반복.
6. **L5 성능/장기:** 1,200개월 장기 여러 프로파일 + 10만/100만 실활성 POP와 공사·생산·지식이전·연구/정치/군사 복합 사건 성능. 성능 기준을 실측 하드웨어/feature coverage에 따라 문서로 정한다.

## V7-3. 정본·시험 보고 최소 형식

~~~text
report_id, date, repo, branch, source_commit_sha, merged_baseline_sha
contract_versions/hash + manifests, ruleset/numeric_policy/RNG/save_schema hash
os/compiler/build flags/hardware/worker_count/seed, feature profile(I2-SAFE)
DV-DESIGN, L0..L5 status: PASS|FAIL|BLOCKED|NOT_RUN
DD0/DC/DI/DT/C-D4/POP/CE/CP/CM test counts and direct logs
DV-T01..72 individual results: expected/actual, no silent SKIP
DV-INV01..36 monthly failures, first bad event/month/stage, whole-world rollback hash
journal/commit/save-load/replay digests, cross-platform deterministic comparisons
1,200 months statistics, 100k/1m measured active workloads, P50/P95/P99/RSS
known unsupported/omitted features & migration, final recommendation
~~~

## V7-4. D-V 확정 ADR 30개

| 결정 ID | 고정 결정 | 근거 |
|---|---|---|
| **DV-ADR-001** | 기존 POP-D0~D3·C-D0/E/P/M/D4·D-D0/DC/DI/DT의 데이터 원본·시간 계약을 상위 고정, 새 월 단계/독립 시뮬레이션 World 금지 | V1/V2 |
| **DV-ADR-002** | Phase D 구현은 하나의 C-D4 통합 StagingWorld에 부가하고 장기 main/작업 브랜치의 이중 원본을 병행 운영하지 않음 | V1/V7-L0 |
| **DV-ADR-003** | PopulationSlice/Employment/Financial/Inventory/Ownership/Fiscal/CapitalAsset/Knowledge/Construction/Political/Military의 영역별 유일한 writer 보장 | V1/V3 |
| **DV-ADR-004** | 계획·허가·계약/예약과 실제 시공·인도/임금 지급·연구·검증/설비 활성 상태 구분 | V1/V2 |
| **DV-ADR-005** | 계좌·노동/군역·상품·장비서비스/운송·토지/특허 권한의 공통 예약 우선순위와 stable ID tie break | V2 |
| **DV-ADR-006** | 기존 C-D4 Stage0..11/9a..9e/10a..10f 공개 순서, stage4 내부에만 유효 공정 DAG 인정 | V2 |
| **DV-ADR-007** | 당월 생산된 중간재를 이미 끝난 Stage3에 소급 입력하지 못하며 Stage7 신구매는 이전 Stage4를 늘리지 못함 | V2 |
| **DV-ADR-008** | 공사 완료 m→Stage11 ready→m+1 Stage0 설치/권리/WIP/운송용량 효력 게이트 유지 | V2/V4 |
| **DV-ADR-009** | 연구 Stage4 실제 실험→Stage9a 검증→m+1 Stage0 지식 효력, 같은 달 9a 새 지식 무한 연쇄 금지 | V2 |
| **DV-ADR-010** | 훈련/문해/인력상태 전이는 POP Stage10 확정 후 m+1 고용/병역 적격부터 적용 | V2 |
| **DV-ADR-011** | 국가 기술보유·특허권·연구실적·사업장 실제 적용/설비·개별 인적 숙련을 다른 정본으로 검증 | V1/V3 |
| **DV-ADR-012** | 제품/건설 서비스/연구 실적·기성고/WIP/설비재고/고정자산을 이중 부가가치·현금으로 계상하지 않음 | V3/V4 |
| **DV-ADR-013** | 광상/농지 등 실제 ResourceStock과 상품재고 분리 및 부산물/폐기·재활용·에너지 서비스 추적 | V3 |
| **DV-ADR-014** | 새로운 연구/건설·군수·제조 자원 경합은 동일 금융·상품·POP/장비 보존식으로 검사 | V2/V3 |
| **DV-ADR-015** | 산업·교육/의료/행정·군수/환경/기술의 경제/사회효과는 실제 원장/시설·연구/의료/교육서비스 이벤트 필요 | V3/V4 |
| **DV-ADR-016** | 정부 예산·새 법/정권교체/군사점령·관세/특허/건축허가는 유효 권한·효력월/원장 기록으로만 사용 | V3 |
| **DV-ADR-017** | 전투·재난/실험 사고는 Stage10 실제 인구/설비·WIP/철도/재고 손실로 처리, 과거 생산/투표/지식 역사 소급 변경 금지 | V2/V4 |
| **DV-ADR-018** | I2-SAFE 프로파일의 비저장 동월 Stage4 전력 생산→다른 공장 소비는 명시적 UNSUPPORTED; 상위 CE/D4 ADR·버전/회귀 전 I2-DISPATCH 사용 금지 | V1/V2/V5 |
| **DV-ADR-019** | 정상 공사 지연·연구 실패·경제위기/광산고갈·기업도산·전쟁 패전과 HARD 원장/권한/시간 오류를 구분 | V3/V6 |
| **DV-ADR-020** | 사건 멱등·revision/타입 ID·계보, 단일 World 원자적 Commit/Abort 및 모든 도메인·ID allocator/예약 전체 롤백 | V1/V6 |
| **DV-ADR-021** | 월말 정본·이벤트 순서의 직렬/다중 worker 결정론, 고정소수점·RNG/정렬·반올림 규칙과 세이브 버전 고정 | V6 |
| **DV-ADR-022** | 세이브/중간 장애/체크포인트·저널 커서/마이그레이션/미지원 스키마 거부의 독립 시험 요구 | V6 |
| **DV-ADR-023** | DV-INV-01..36과 DV-T01..72는 기존 DD0 24·DC45·DI45·DT45·D4 48 및 POP/CE/CP/CM을 대체하지 않음 | V3/V5 |
| **DV-ADR-024** | 검증 fixture는 가상 수치/통화·단위/시드·명시적 예측·법/실물 효과를 갖고 정적 검산과 실제 C++ 실행을 엄격히 구분 | V4 |
| **DV-ADR-025** | D-V 실제 구현 선행조건은 Phase C 통합 코드/정치·Phase B의 단일 원본·브랜치 정본 락 및 새 Phase D 계약 해시/ABI 마이그레이션 | V7-L0 |
| **DV-ADR-026** | D-V 코드 구현은 공통 원장/월 사건 정합→건설·산업·기술 단계별 연결→72시험/회귀→저장/병렬→장기/성능 순 | V7-L1..L5 |
| **DV-ADR-027** | 공공/민간·국가별 건설/기술연구 결과 및 산업 성장률을 무료 전역 수치 버프로 직접 생성하지 않음 | V1/V3 |
| **DV-ADR-028** | 100년×다중 충격과 10만/100만 활성 POP/시설·연구/산업/군수 대표부하를 실제 계측, Phase C 수치의 Phase D 실측 대체 금지 | V6/V7-L5 |
| **DV-ADR-029** | 성능 PASS 임계는 하드웨어/feature coverage·회귀기준 실측 후 합의하며 미측정 절대 ms/GB 목표·무단 skip 금지 | V6 |
| **DV-ADR-030** | DV-T72와 D-V COMPLETE는 L0..L5 실제 PASS 및 정확한 전체 테스트·회귀/성능 증거만 인정, 설계 확정을 구현 완료로 승격 금지 | V7 |

## V7-5. 현재 완료 판정

**확정:** 단일 월간 구조·D-V 독립 36 불변식·72 교차 테스트 및 상위 207개 필수 회귀/30 ADR의 명세와 정적 fixture.

**미실행:** D-V 실제 통합 C++ 코드, 72개 인수·207개 상위 시험을 결합한 같은 엔진의 재실행, 1,200개월/10만·100만/세이브·worker·OS 실측.

**현재 선언:** `DV-DESIGN CONFIRMED` / `DV-L0 BLOCKED_FOR_CPP` / `DV-L1..L5 NOT_RUN` / `DV-COMPLETE NOT_RUN`.
