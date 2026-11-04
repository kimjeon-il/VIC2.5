# E-V — 지리·영토·행정구역 통합 검증 설계

> SPECIFIED / NOT RUN · 2026-11-04 · phase-e-validation/1.0. 총 60개 독립 EV-T01..60. 정적 문서 검사는 PASS로 세지 않는다.
> 상위 E-D0/E-G1~G5 + POP-D0~D3 + C-D0/C-D4 + D-D0/D-V. 모든 테스트는 C++ 통합 엔진/렌더러/프로파일러에서 실제 실행해야 PASS.

## 1. 범위와 책임

- E-G1 정적 프로빈스·해안·인접/자료정확도
- E-G2 주권·점령·분쟁·영토변경 트랜잭션
- E-G3 행정구역 가변계층·분할·합병·통합
- E-G4 선거·지방정부·특별관할/통계의 분리
- E-G5 래스터 큐브구면·정수-ID raster·LOD·변경분 GPU 렌더
- 크로스도메인 POP/Finance/Inventory/Ownership/Politics/Military/Construction/Industry/Technology, 장기실행, 결정론, 저장·복구, 벤치.
- C-D4 48개와 D-V 72개 및 D-D0 24 등은 E-V 독립 카운트와 합산하더라도 자동 PASS 아님. E-V 인수는 별도 요구 증빙.

## 2. 테스트 픽스처/오라클 공통 형식

모든 테스트는 {test_id, dataset_manifest_hash, scenario_id, seed, ruleset_hash, month_index, input_commands, expected_worldhash_or_explicit_fields, expected_error, committed_event_ids, expected_dirty_set, run_environment}를 기록.
기준 소규모 fictional fixture: P1..P12 12개 Province; A가 P1..6, B가 P7..12 주권, 초기 military_control=sovereignty, land adjacency는 정사각형 4열×3행의 edge-sharing (corner touch 제외), 인구는 각 Province 100명(총 1200명), 각 Province에 정규화된 기본 행정 leaf 하나, 별도 검증용 기업·재고·자산은 정식 원장으로 설치. A의 State A1={P1,P2,P3}, A2={P4,P5,P6}; B의 B1={P7,P8,P9}, B2={P10,P11,P12}. 이 수치는 실제 역사가 아니라 테스트 데이터.
오류 예상 test는 지정된 정확한 종류의 거부/Hard error에 더해 WorldHash, registry revision, last_commit_id, id-allocator-next, finance/inventory/POP state가 변하지 않아야 함. 성공 test는 full cross-domain invariant 및 settled journal validation이 필수.
렌더 golden image는 platform-specific tolerance와 CPU ProvinceId exact lookup을 구분; 역사적 정확성은 source confidence/extent 및 provenance 별도 검사.

## 3. EV-A — 지리·공간정본 (01~10)

| ID | 실행 시나리오 | 반드시 만족하는 오라클 |
|---|---|---|
| EV-T01 | 12개 valid Province 및 duplicate ID 1개를 추가한 dataset | 정상 12개 unique; 중복 1개 import hard reject, 기존 dataset 그대로 |
| EV-T02 | 4×3 격자 Province 1~12의 내부 edge 인접 구축 | interior edge 대칭, corner-only 쌍은 LandAdjacent=false |
| EV-T03 | 섬 Province·해역 접촉·해협 crossing 구분 | 섬은 육상 edge 0 가능, SeaZone/Coast만 있고 water crossing은 별도 명시 |
| EV-T04 | lon ±180, 북/남 극과 face boundary coordinate probes | 같은 위치 조회에 대해 canonical ProvinceId 정확; seam NoData 틈 없음 |
| EV-T05 | 1개 multipolygon Province+호수/내륙수면 | 동일 ProvinceId multiple components, lake NoData/SeaZone 정책과 면적중복 없음 |
| EV-T06 | raster source overlap 2개, land gap 1개 | ambiguity/coverage 검사 실패 및 source provenance error, 무작위 배정 없음 |
| EV-T07 | 같은 Province static dataset으로 twice compile | Province IDs, adjacency sorted edge IDs, hashes byte-for-byte identical |
| EV-T08 | runtime Province split/merge/geometry edit command | ED0_ERR_PROVINCE_MUTATION + zero world effects |
| EV-T09 | legacy Region 1개에 inhabited POP이 있으며 3 Province로 불명확 mapping 요청 | 명시적 allocation missing이면 거부, POP 0명 생성/유실 |
| EV-T10 | static GIS import 역사적 경계가 fixed Province 내부를 가로지름 | unsupported-boundary diagnostic+출처, gameplay Province 분할 0 |

## 4. EV-B — 주권·점령·국경 (11~20)

| ID | 실행 시나리오 | 반드시 만족하는 오라클 |
|---|---|---|
| EV-T11 | A주권 P1의 military_control B로 변경 | sovereign A/POP 100/citizenship unchanged; control B |
| EV-T12 | P1 B국 Claim 추가·삭제 | 주권 A/POP·Finance 동일, claim history/ID 유효 |
| EV-T13 | 같은 P2/기간 역할 sovereignty A+B 이중 확정 | ED0_ERR_JURISDICTION_CONFLICT, 전월 state/ID/clock 완전 롤백 |
| EV-T14 | P2,P3을 A→B cede, B basic admin assignment 함께 제출 | A4/B8 provinces, total 12; POP A400/B800 (초기100씩); world1200 |
| EV-T15 | P2,P3 2개 cede에 P999 invalid 포함 | 전부 reject, A6/B6 및 전원장/ID allocator 동일 |
| EV-T16 | P1↔P7 영토 교환 | A6/B6 유지; 해당 Province 주권 교환, 남은 모든 status unchanged |
| EV-T17 | A가 P4,P5 독립 C 설립+정부/기본 admin 준비 | A4/B6/C2; total POP1200; 기존 시민권·자산/채무 변화 없음(별도 명령 없는 경우) |
| EV-T18 | 점령군 B가 P3을 장악하고 tax/civil authority 없이 자동 세수 요청 | sovereignty A, military B, fiscal authority unchanged; 무근거 세금 인출 거부 |
| EV-T19 | 다른 Province P1에 두 경쟁 event 같은 expected revision | 1개만 규약대로 commit, stale revision event conflict; 결과 결정론 |
| EV-T20 | P1 A→B 인접 지도 경계갱신 | P1 접경 edge 및 관련 국가 색상만 dirty; G1 static adjacency/hash unchanged |

## 5. EV-C — 동적 행정구역 (21~30)

| ID | 실행 시나리오 | 반드시 만족하는 오라클 |
|---|---|---|
| EV-T21 | P3 A1→A2 소속 이전 | P1,2=A1; P3,4,5,6=A2; A 영역/POP 총량 불변 |
| EV-T22 | A1={P1,2,3}을 {P1}과 {P2,3}으로 완전 분할 | 각 P 정확한 1 leaf; new ID/lineage; POP unchanged |
| EV-T23 | A1,A2 법적 합병 신설 A3 | A3={P1..P6}, 이전 units retired/lineage, ProvinceIds 유지 |
| EV-T24 | A1의 child unit을 다른 parent로 재배치 | child/descendant Province leaf unchanged, ancestor queries revised |
| EV-T25 | A1→A2→A1 parent cycle 시도 | ED0_ERR_ADMIN_CYCLE + revisions/worldhash unchanged |
| EV-T26 | 같은 A scheme P1을 두 active leaf에 배치 | ED0_ERR_ADMIN_ASSIGNMENT, rollback |
| EV-T27 | B영토 P7을 A 기본 scheme에 행정 소속 단독 할당 | sovereign/scheme mismatch reject; 전쟁 군정 scope와 구별 |
| EV-T28 | 정부 행정개편 m stage9 신청 | m 경제·인구/선거 snapshot 유지, m+1 stage0 행정 갱신 |
| EV-T29 | A2 명칭 변경만 수행 vs A2 폐지·동명 재생성 | rename keeps ID; recreated has new ID and predecessor links |
| EV-T30 | 3000개 Province 행정 재편·법적 월경지 | valid discontiguous units allowed, membership completeness, no full world POP rewrite |

## 6. EV-D — 선거구·지방정부·관할 통계 (31~40)

| ID | 실행 시나리오 | 반드시 만족하는 오라클 |
|---|---|---|
| EV-T31 | A1/A2 지리 편제와 별도로 가로형 의회 선거구 구성 | electoral membership independent, admin same |
| EV-T32 | 같은 하원 ContestId에서 P2 eligible POP 중복 두 district에 배분 | duplicate-voter/district error, eligible registered/cast conservation |
| EV-T33 | 같은 주민이 하원과 지방의회에 각각 적법 등록 | 서로 다른 contest 각각 가능, 세계 headcount 여전히1200 |
| EV-T34 | A1 지역 자치정부 derived admin scope로 생성 후 P3 이관 | local jurisdiction P3 m+1 새 A2에 반영, independent electoral district unchanged |
| EV-T35 | explicit military district와 fiscal district가 동일 P2 포함 | overlap allowed by separate kind; authority domains independent |
| EV-T36 | local-government scope만 만들고 세금/현금/채무 자동 생성 주장 | no fiscal/FinancialLedger entries without C-P/E event |
| EV-T37 | P1 임금 평균10/인구100, P2 평균20/인구300(별도 fixture) | combined average = (10×100+20×300)/400=17.5, 단순 평균15 금지 |
| EV-T38 | 겹치는 Scope A={P1,P2}, B={P2,P3} 합산 시도 | unique union={P1,P2,P3}, POP=300 not 400 |
| EV-T39 | 선거 진행 중 admin/derived scope 재획정 예약 | 진행 중 Election snapshot boundary revision frozen; 과거 투표 결과 불변 |
| EV-T40 | 한 Province 내부 선거구를 지리적 polygon으로 두 개 분할 요구 | E raster 기본으로 불가 명시; 합법 population partition 시 인구 분배 보존, 부정확 지도 경계 생성 금지 |

## 7. EV-E — 래스터 지구본/LOD (41~50)

| ID | 실행 시나리오 | 반드시 만족하는 오라클 |
|---|---|---|
| EV-T41 | cube face/LOD tile 변환과 기본 camera ray picking | canonical world coordinate→ProvinceId exact, 실패 시 NoData |
| EV-T42 | ID integer raster를 bilinear color sampling 방식으로 처리하려는 코드 | invalid sampler/pipeline 검증 실패, ID 혼합 금지 |
| EV-T43 | 작은 섬 Province zoom-out coarse tile 소실 | canonical Province 여전히 존재, locator/fallback selection으로 접근 가능 |
| EV-T44 | P1 Sovereign A→B 변경 뒤 static tile hashes 비교 | static ID/terrain digest unchanged, dynamic LUT/border changed |
| EV-T45 | 행정구역 표시 계층 전환 후 sovereign 모드로 복귀 | 동일 WorldSnapshot 주권 참조 보존, 행정 colors가 국가 원장 변경 0 |
| EV-T46 | 군사 점령 border와 국가 border 색/패턴 출력 | 군사통제와 주권 각각 독립 표시 및 레전드 구분 |
| EV-T47 | ±180 seam와 북·남극 tile UV 경계 samples | 경계 찢어짐/잘못된 ID 없음, canonical ID lookup exact |
| EV-T48 | tile 미로딩 중 클릭/렌더 전환 | NoData/pending 명시, 이전 tile stale ID 오표기 금지 |
| EV-T49 | GPU device loss + recreation | simulation world hash/commit unchanged, render assets reproducibly regenerated |
| EV-T50 | P1만 changed + render instrumentation | GPU LUT patch + neighbor border dirty, static full-world raster regenerated=0 |

## 8. EV-F — 범분야 원장·복구·결정론·장기 성능 (51~60)

| ID | 실행 시나리오 | 반드시 만족하는 오라클 |
|---|---|---|
| EV-T51 | 행정구역 20→50 재편, 동일 POP/기업/시설 재집계 | world headcount/money/inventory/ownership/site IDs unchanged, aggregates correct |
| EV-T52 | 공장·건설프로젝트·연구소가 있는 P1을 A→B 이전 | 시설/진행중 WIP/지식·보유권/현금 무단 이전 0, 새 유효 jurisdiction만 |
| EV-T53 | P1 주권 변경과 시장·수송·무역경로가 연계 | MarketAssignment/Shipment/Transport 변동은 해당 별도 정당한 event만; 상품 위치 보존 |
| EV-T54 | 전투 stage10 P1 군사통제 B 변경, 단계4 생산/단계5 임금 완료 | 당월 생산·임금 소급 수정 0, 차월 stage0 control B, POP casualties only valid POP events |
| EV-T55 | 금융 재고 reserve 충돌과 동시에 행정/주권 승인 시도 | shared reservation overload reject, C/D/E 전원장 all-or-nothing rollback |
| EV-T56 | world save/load + Province dataset hash mismatch + corrupt journal | matching save exact equality; mismatch migration/reject, journal corruption rejected |
| EV-T57 | 동일 명령/월 1 worker vs 4 worker, 반복 실행 | bit-exact WorldHash, committed events order, journal, financial/pop/admin results |
| EV-T58 | 1200개월 × 2 iteration, 반복 독립·독립취소·행정개혁·전쟁·산업활동 | 매월 invariant check, no ID reuse, checksum, no leaks, deterministic final state |
| EV-T59 | sparse POP 100k/1M + Province 10k/100k synthetic workload | actual peak RSS/CPU P50/P95/P99, per-stage CPU+memory+eventcount 측정; 부하 중 완전 보존, 사전 합의한 장치/메모리 성능 게이트를 적용 |
| EV-T60 | EV-T01~T59 실제 실행+자료해시/메트릭/OS/compiler/GPU 매트릭스 | 59개가 모두 코드상 PASS이고 회귀 포함, final acceptance artifact 존재 시에만 EV-COMPLETE. 문서만으로 PASS 금지 |

## 9. 독립 수락 게이트

1. G1 static-data integrity/accuracy and provenance. No magic generation of historical countries. Missing border fidelity must be documented.
2. G2 sovereign/occupation/admin claim role split. Province transfer cross-domain atomicity.
3. G3 administrative arbitrary recompose, no Province mutation, valid lineage.
4. G4 district/Government scopes and POP voter partition invariant.
5. G5 functional globe on target hardware; stable integer ID pixels + correct picking and dynamic country/admin visualization.
6. POP/C/D regression (whatever has real executed PASS in same branch) still pass after E adapters; existing outstanding D 279 acceptance cases remain separate BLOCKED/NOT_RUN until independently proven.
7. Stress and profiling: choose at least 1 named low-end hardware profile + 1 reference GPU, resolution, driver and RAM budgets *before testing*. Do not silently tune thresholds after measuring results; record GPU frametime and camera/input latency, cache misses, render full regeneration count.
8. Fault injection: refuse invalid role/time/revision/ownership and prove that world stage/commit hash/ID generator/all domain ledgers are unchanged. Multi-command conflicts deterministic.
9. Persistence and migration: geography static dataset hashed; no silent remapping of ProvinceId; loader rejects mismatched history and invalid refs.

## 10. 품질/결정론/성능 규약

EV-F performance benchmark is not a pass by its existence: record real OS/CPU/GPU/VRAM/compiler/driver, expected workload sizes, shader backend, total number of active POP slices, firms, sites, countries, provinces, journal events; compare with baseline time/memory limits chosen in advance. Dynamic changes should scale with modified Province+boundary set, but aggregated dependencies can make the dirty set larger (reparent large subtree), which must be measured honestly.
Rollback oracle: check balances and ledgers, no new finalized events, month/commit ID unchanged, no ID allocator increment; game-rejected commands are expected successful validation of a rejection, not engine panic.
Deterministic world content: canonical serialization of ID/revision/map relation, int arithmetic, fixed tiebreak; GPU screenshot anti-aliasing can vary by renderer but GPU display cannot change canonical world hash.

## 11. Release states

DESIGN_SPECIFIED: 현재 이 문서의 상태.
IMPLEMENTATION_PARTIAL: one or more C++ tests code verified, not the full 60.
EV-TESTS-60-PASS: EV-T01..T60 executable, all oracle and regression evidence.
EV-COMPLETE: upstream POP/C/D contract integration, E realtime render/longrun/low-end benchmarks, persistence/worker determinism and 60 tests all accepted with no unresolved blocker.
현재 상태: DESIGN_SPECIFIED; EV-T01..60=NOT_RUN, EV-COMPLETE=NO.
