# VIC2.5 D-V6 — 결정론·저장/복구·오류주입·장기·대규모 성능

> **설계 확정 / 코드·실측 시험 미실행** · `phase-d-integration/1.0` · 2026-11-04  
> 상위: [POP-D3 Runtime](../../pop/D3_RUNTIME.md), [C-D4-6](../../phase-c/integration/D4-6_DETERMINISM_RECOVERY_PERFORMANCE.md), D-D0/DC/DI/DT. [시험 72개](D-V5_TEST_MATRIX.md).

## V6-1. 결정론 정의와 수치 정책

**동일 `WorldSnapshot+commands+ruleset/schema/provenance+world_seed/RNG_version+numeric_policy+stable ID/정렬 규칙` ⇒ 같은 월말 정본 상태/`JournalBatch` 정렬 해시**를 기본 불변식으로 한다.

- `People`은 signed 64bit nonnegative(진행 변경은 delta 사건에만 부호), `Money{currency_id,int64_minor_units}`, `Quantity{good_id,int64_scaled_units}`, `WorkQuantity`/연구서비스·지식 성과는 규칙별 단위/스케일+checked 연산. 중간 금액/비율 곱셈은 checked 확장 정수 사용; 부동소수점의 플랫폼/컴파일러 차이에 유의.
- `RNG(version,world_seed,month,stage,entity_id,event_kind,draw_index)`를 사용하여 연구 성공/실패, 공사 하자, 사고·자원/기술 접촉, 군사사상/재해의 무작위 결과를 **순회·worker 수와 독립** 생성. 각 난수 호출의 event/시점·원본/규칙 해시를 기록.
- 그룹 할당/잔여는 기존 C-D4 정렬과 `floor + largest remainder`/stable typed ID tie break. 무순서 컨테이너의 해시·쓰레드 interleaving으로 결과 달라지면 실패.
- 정본·캐시는 서로 다르며 실질 결과 비교는 캐시 메모리 배치·주소나 기초 구현의 해시 테이블 순서가 아닌 **canonical serialized normalized WorldDigest**를 사용.
- Windows/MSVC, Linux/GCC/Clang, macOS/Clang 등 실제 가능한 플랫폼을 분리 측정한다. 같은 플랫폼 내부·worker1/N 및 다른 플랫폼 정수결정론에서 기대 수준을 명시적으로 고정한다. 실측 없는 바이트동일 보장 주장 금지.
- 측정은 동일 실행환경/초기 세이브·seed/Ruleset/명령/하드웨어/컴파일러 플래그를 기록한 실제 run에서만 `PASS`로 판정.

## V6-2. 원자적 세이브·회복

### 파일/저장 헤더 필수

`save_schema_version, pop_contract_version, cross_domain_contract_version, phase_c_economy/politics/military/integration_contract_versions, phase_d_common_version, phase_d_construction_version, phase_d_industry_version, phase_d_technology_version, phase_d_integration_version, ruleset_version/hash, recipe_schema_hash, technology_schema_hash, asset_schema_hash, numeric_policy_version, RNG_version, world_seed, calendar_id, month_index, canonical_snapshot_digest, journal_commit_id, journal_cursor, source_provenance_hash`.

- 세이브의 권리/인구·금전/상품·자본재·`ConstructionProject/WorkPackage/WIP`·`IndustryActivity/ResourceSiteFlow/ExplorationState/RenewablePool` (지하 총량/잔량 제외)·`ResearchProject/KnowledgeState/SiteTechniqueAssignment`와 C-P/C-M/경제 원장의 **한 월 커밋**이 일관된 체크포인트여야 한다.
- `BeginMonth` 시 원본 상태/저널/규칙 잠금을 확인하고, 성공한 stage11 `Commit` 이후에만 원자적 `snapshot+event journal+commit marker`를 가시화한다. 트랜잭션 중 장애가 나면 이전 완전한 커밋으로 복귀.
- 동일 event/command 재전송은 `source_event_id`/`transaction_id`/revision·commit cursor 대조 후 거부/멱등 결과 조회하며 `KnowledgeEvent`·WIP 재분류·자산 추가/보조금·고용/재고 이동을 두 번 적용하지 않는다.
- 이전 계약 버전·시장/상품 스케일/기술 DAG/보유자/신규 API incompatibility는 명시적 migration/정적 원본 대조 수행 아니면 **load Reject**. 다운그레이드·임의 1:1 FX/권리·계층 강제 변환 금지.
- `CrashInjection(stage=0..11,substage)`는 `0c` 배송·`0c-commission`·`1` 예산/예약·`2` 인구/고용·`3` 재료인도·`4` 복합 공사/제조/연구·`5` 급료·`7` 투자재·`8` WIP/계좌·`9a` 지식예약·`9d` 법률·`10` 피해/인구·`11` Commit 직전/직후를 포함한다.
- 이전 단계의 모든 staged 변경(새 ID 발행/예약 포함)이 유출/부분 커밋되면 FAIL. 오류 로그의 existence 자체는 경제세계 상태가 아니다.

## V6-3. 오류주입 매트릭스

| 주입 지점 | 의도적 오류 | 예상 결과 |
|---|---|---|
| `0c` | 실제 미도착 광석/실험재를 선입고로 표시 | `D4_ERR_TIMING`/Inventory 오류 + 전체 Abort |
| `0c-commission` | 검사 누락된 설비/철도 간선을 활성 | `DC_ERR_COMMISSION_EARLY` → 전체 Abort |
| `1` | 같은 정부/기업 예금 100으로 임금·자재/연구 각100 예약 | `D4_ERR_FINANCIAL_UNBALANCED` 또는 합법 축소/초과요구 명시 실패 |
| `2` | 4 POP을 연구·군수/건설에 합계 7 배정 | `D4_ERR_POP_OVERALLOC` 또는 적법 부분축소 |
| `3` | 철강 4 로트를 시공3·산업3에 모두 소비 예약 | `D4_ERR_INVENTORY_OVERDRAWN` 또는 합법 부분축소 |
| `4` | 공사 시공 WorkEvent만 기록·재료 소비 누락 | DV-INV-09/19 위반 → HARD ERROR |
| `4` | 연구량/실험 성공 이벤트 생성하되 연구원·연구재료/증빙 없음 | `DT_ERR_RESEARCH_RESOURCE`·`DT_ERR_KNOWLEDGE_PROVENANCE` |
| `5` | 임금 10 지급 후 노동자·정부/기업 계좌 분개 누락 | `D4_ERR_FINANCIAL_UNBALANCED` |
| `7` | 재고기계 동일 로트를 인도/설치 후 창고에서 중복 판매 | `DC_ERR_ASSET_DOUBLE_COUNT`/Inventory 충돌 |
| `8` | WIP 증가120·청구권120이지만 대응 외상·실적/원가 없음 | `D4_ERR_FINANCIAL_UNBALANCED`/commit 검증 |
| `9a` | 같은 월 m 새 지식을 m stage4 또는 동월 9a 다른 프로젝트 입력으로 적용 | `D4_ERR_TIMING` |
| `9d` | 방금 입법된 특허/교육/관세정책을 m stage1~8에 소급 | `D4_ERR_TIMING`/AUTHORITY |
| `10` | 사망·부상 인구 중복, 철도/연구소 전쟁 피해 원장 누락 | `D4_ERR_POP_OVERALLOC`/`D4_ERR_COMMIT_VALIDATION` |
| `11b` | 법적 소유자 없는 설치자산, 저장상태 고아 참조 | `D4_ERR_OWNERSHIP_ORPHAN` + 전체 World Abort |
| `11d` | Commit marker write 전/후 프로세스 중단 | 이전 완전 월 또는 새 완전 월 **정확히 하나**만 복구; 반쪽 커밋·double replay 0 |

각 주입은 `before_world_digest, staged_operations, expected_domain_error, expected_D4_error, actual_error, rollback_digest, ID_allocator_before/after, last_committed_event`를 보관한다. **예상된 오류코드를 정확히 발생시켜 거부·롤백한 테스트가 PASS**이며 일부 오류를 catch/무시하거나 ‘모델이 정상 침체했다’로 위장하면 실패.

## V6-4. 1,200개월/중장기 회귀 프로파일

| 프로파일 | 주어진 충격 | 필수 증거 |
|---|---|---|
| `DV-LR1_baseline` | 3+ Region, 공사/제철·기계·농업·연구/기술·POP 정치/군수, 1,200개월 | 월별 36 DV 불변식·기존 전체 보존/회계·연구·준공·가계/고용/가격·seeds, zero silent skip |
| `DV-LR2_resource_finance` | 흉작·광상고갈·상인/운송 지연·은행 위기·공공 건설 중단/파산 | 조달/생산·월별 부채/청산·고용/기술 R&D/투자 감소의 합법 경로 및 보존 |
| `DV-LR3_war_reconstruction` | 동원/군수 연구·산업 전환/전투·연구시설/철도파괴·전후 재건 | POP 사상자·고용/서비스·재고/가동/WIP·승계/권리/지식 손실·보험/금융 재건 정합 |
| `DV-LR4_multi_society_tech` | 정부/법률·교육·기술 전파, 다국/다지역 연구기관·특허법/제도 변경·이주/문화·언어 접근 | 유효 법/권리/숙련·지식 확산의 원인·차월 효력·국가 시장·연구/부의 분포 보존 |
| `DV-LR5_deterministic_twice` | 동일 World/seed·월별 명령을 직렬과 N worker로 1,200개월 각 2회 재생 | 월말/저널 해시·관측 결과 동일, 다름시 첫 월/이벤트 및 수치 정책 보고 |

- 정상 생산량 악화/산업붕괴·연구실패·금융위기·내전/패전은 불변식만 준수하면 **합격 가능한 모형 결과**다. 무조건 성장률 >0, 연구성공/군사승리를 수락기준으로 강제하지 않는다.
- 세이브 후 재생 시 서로 다른 seed/ruleset/historical coefficients가 사용됐다면 결과가 같지 않아도 버전 불일치 오류·명시적 비교경계를 보고한다. 어중간한 과거 세이브를 최신 규칙으로 조용히 덮어쓰기 금지.

## V6-5. 규모·실제 실측과 회귀

| 규모 | 부하 조건 | 판정 기준 |
|---|---|---|
| `S-small` | 3개 지역, 2개 정부/복수 시장, 모든 D-D0/DC/DI/DT 기능이 실제 실행되는 수십~수백 객체 | 고정 오라클·72+상위 시험/지표 검산 |
| `S-100k` | 100,000 활성 `PopulationSlice` 및 복수 지역/산업별 사업장·시장·공정/자원·연구소·군부대·월 1,000+ 이상 실제 관련 사건을 **가진 대표 월 8개월+** | 실제 월/단계 P50/P95/P99 wall/CPU·최대 RSS·캐시 miss/alloc·journal/commit/invariant 검증, 보존·재현성 |
| `S-1m` | 1,000,000 활성 `PopulationSlice` 및 유효 산업·공사·연구·전투/법·교역/설비 로드의 복수 월 | 메모리/시간·이벤트 처리량·병목/최대 사례·회귀/정본 보존. 초기화만 수행한 벤치는 불인정 |
| `S-1,200` | 100년 동안 생산/연구·건설/승계/설비·POP/국가·전쟁/위기 실제 이벤트 누적 | 1,200번 commit/기록·장기 drift/메모리 누수·인구/금융/상품/자산/기술 이력/수치정책 보존 |

- **C-D4 프로토타입의 10만/100만 셀 실측 성능 수치나 48개 합성 테스트 PASS를 Phase D 성능/72개 테스트로 복사하지 않는다**. 실제 Phase D 분야별 활성 객체 수/사건량이 다르며 비교는 같은 하드웨어·입력·코드·반복/컴파일러 기준으로만.
- 절대적인 목표 ms/월·RSS/GB는 실측 전에 '통과'나 '실패'를 임의 선언하지 않는다. `baseline_hardware`에서 최적화 전 기준선과 이후 허용 퇴보율/기능별 시간 예산을 **후속 성능 ADR**로 정하고 시험 결과·기종/OS를 함께 기록해야 수용 가능.
- 성능 수치를 얻으면서 기능을 비활성화·캐시를 미검증 상태로 바꿔 빠르게 만든 경우 `profile_hash/active_feature_set`을 보관하고 **대표 부하 성능 PASS로 사용할 수 없다**.

## V6-6. 저장·로그·운영 보고 형식

최소 테스트 결과: `run_id, git_commit/source_branch, source_baseline, feature_gate/I2-SAFE, contract_hashes, scenario_id, test_id, fixture_hash, seed, ruleset_schema_hash, compiler/OS/hardware/worker_count, start_world_digest, month/stage/subphase, first_error_code, mismatched_entities, journal_digests, rollback_digest, save_replay_digest, elapsed_p50/p95/p99, peak_rss, invariant_failures, final_verdict`.

테스트 누락·skipped/blocked 항목은 완료 보고서에 보존하고 전체 `D-V COMPLETE`나 `DV-T72 PASS`를 선언하지 않는다.

**현재:** 결정론·저장/복구·1,200개월·대표부하의 **계약/검사기준만 확정**, 실제 측정·C++ 재현성 검증 **NOT_RUN**.
