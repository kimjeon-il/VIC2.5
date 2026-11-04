# VIC2.5 D-V — 건설·산업·기술 통합 검증 설계

> **D-V 설계 확정 / C++ 통합 구현·72개 교차 인수시험·장기검증 미실행**  
> 계약: `phase-d-integration/1.0` · 2026-11-04 · 브랜치: `work/phase-d-design`  
> 선행: [Phase D 공통 계약](../D-D0_COMMON_CONTRACT.md) · [건설 C1~C5](../construction/README.md) · [산업 I1~I5](../industry/README.md) · [기술 T1~T5](../technology/README.md) · [Phase C-D4 통합](../../phase-c/integration/README.md).

## 핵심 문서

| 설계 | 파일 | 확정 범위 |
|---|---|---|
| **D-V1** | [통합 아키텍처/계약 베이스라인](D-V1_ARCHITECTURE_SCOPE.md) | 하나의 World, 원장/객체별 유일한 쓰기 정본, C-D4/Phase D 코드 기준선·브랜치·I2-DISPATCH 미지원 |
| **D-V2** | [월별 12단계·사건 의존 DAG](D-V2_EXECUTION_DAG.md) | 단계0..11/9a..9e/10a..10f·자금/자재/POP·설비/연구·군수 경합과 유효 시점 |
| **D-V3** | [전역 교차 불변조건 36개](D-V3_INVARIANTS.md) | POP/노동·재고/자원/운송·금융/WIP/소유·건설·산업·연구/기술·정치/군사·결정론 |
| **D-V4** | [복합 시나리오와 수치 오라클](D-V4_SCENARIOS_ORACLES.md) | 3개월 통합생산·시공·연구/기술, 자원경합, 법/전쟁·자산손상, 전역 롤백의 4 fixture |
| **D-V5** | [추가 교차 인수시험 72개](D-V5_TEST_MATRIX.md) | `DV-T01..72`: 원본 계약, 건설↔산업, 산업↔기술, 기술↔건설, POP/경제, 정치/군사, 오류·저장, 장기/최종 게이트 |
| **D-V6** | [결정론·복구·100년·성능](D-V6_DETERMINISM_RECOVERY_PERFORMANCE.md) | 전단계 fault injection·full rollback, replay/버전 거부, 1,200개월/10만·100만 POP 대표부하 실측 |
| **D-V7** | [수락/배포 게이트·30개 ADR](D-V7_GATES_ADRS.md) | `DV-ADR001..030`, L0 소스 기준선→L1 공통 원장→L2 실제 기능→L3 시험→L4 저장/병렬→L5 장기/성능 |
| **매니페스트** | [계약·문서 해시/시험 ID](verification_manifest.json) | 문서 blob SHA/상위계약 정본 버전·36 불변식·72 시험·30 ADR·fixture 검산/NOT_RUN 상태 |

## 통합 가상 fixture

| ID | 자료 | 용도 |
|---|---|---|
| **DV-X1** | [3개월 산업→건설→연구/도입](examples/industry_construction_research_chain.json) | 제철 생산품 단계7 인도/차월 공사, WIP/준공·연구성과 차월/기업 지식 분리, 화폐 총액 검산 |
| **DV-X2** | [군수·시공·제조·연구의 공동 자원경합](examples/shared_bottleneck_allocation.json) | 철강6 로트/노동8명·기계서비스4·임금예산40, 실제 우선권·예약/실사용 정합 |
| **DV-X3** | [법 개정·전투·연구소/교량 피해](examples/war_policy_research_damage.json) | m 법률 새로 제정·m+1 효력, stage10 실물피해/WIP·장비손실, 역사 지식/채무 보존 |
| **DV-X4** | [월말 고아 소유권 주입과 전역 롤백](examples/atomic_rollback_fault.json) | 단계0부터 10까지 가동·입고·제조·연구/건설·금융/POP·피해 후 stage11 실패 시 모든 원장/ID/월 인덱스 동일 |

## D-V의 고정 설계 원칙

1. **단일 `StagingWorld`/월 1회 Commit/Abort**. POP·고용·금융/세금·상품·자산/권리·공사·연구/지식·산업/기술·정치·군사·관할의 유일한 원본을 보존.
2. 자금·자재·연구원/근로·자본재/설비서비스·수송/토지·법/특허·군수 자원의 **실제 경쟁과 정합적 예약·할당/사용**을 검증.
3. 월 `m` stage4 산업생산→stage7 판매·수송(미래 도착은 나중), 같은 stage4 다른 기업의 stage3로 소급 사용 불가. `m` 공사·기술 연구/훈련은 `m+1 stage0/2`에만 효력.
4. 기술의 `KnowledgeState`와 특허권·설비 설치/시공·사업장 공정 활성·POP 자격/군수효과를 분리. 발명/대학 설치만으로 국가 전체 공장·군대/의료·교육·현금/재고 성능 상승 금지.
5. `I2-SAFE` 기본 프로파일에서 비저장 실시간 전력의 동월 stage4 생산→소비는 **예상 Unsupported Reject + 전역 롤백**으로 검증. `I2-DISPATCH`를 지원하려면 기존 C-E1/C-D4의 명시적 계약·ADR/버전업·실제 회귀 통과가 필요.
6. 경제위기·파산/전쟁/공사 지연·연구실패/기술 보급 지체는 원장 정합성을 보존한 정상 모델 결과 가능. 유령 자재·인구·돈/지식/자산과 시점·권한 위반은 HARD 실패.

## 상위 시험/릴리스 상태

| 테스트 계열 | 정의된 ID 수 | 실행·합격 상태 |
|---|---:|---|
| D-D0 공통 | 24 | 새 단일 통합 C++에 **NOT_RUN** |
| D-C 건설 | 45 | NOT_RUN |
| D-I 산업 | 45 | NOT_RUN |
| D-T 기술 | 45 | NOT_RUN |
| C-D4 경제·정치·군사 | 48 | **별도 Phase C 프로토타입 범위 48 PASS**; Phase D 통합 브랜치 재회귀 NOT_RUN |
| **D-V 신규 교차** | **72** | **NOT_RUN** |
| **총 정의된 고유 ID** | **279** | **279개 실제 Phase D 단일 엔진 PASS는 주장하지 않음** |

추가로 기존 POP/CE/CP/CM의 개별 인수시험/정본 회귀도 필수(위 279 이외에 해당 계열의 시험 포함).

Phase C의 2026-11-04 프로토타입 48 PASS 근거는 [work/phase-c-engine-integration 구현 보고서](https://github.com/kimjeon-il/VIC2.5/blob/work/phase-c-engine-integration/docs/phase-c/ENGINE_IMPLEMENTATION_STATUS.md). 이 코드는 `main` 미병합이므로 D-V-L0의 **단일 코드 기준선 결합/호환 감사**가 필요. 문서만 병합했다고 엔진 구현 PASS를 주장하지 않는다.

## 현재 명시적 판정

- `DV-DESIGN CONFIRMED`: 정적 계약/72개/36개/30 ADR·4 fixture 명세.
- `DV-L0 BLOCKED_FOR_CPP`: C-D4 구현 브랜치·D-D0/DC/DI/DT 추가 타입/원장·정치/Phase B 소스의 단일 구현 기준선 미연결.
- `DV-L1..L5 NOT_RUN`: 실제 구현, 72개 인수·상위 207 시험/POP·경제·정치·군사 회귀, 결정론/저장·성능/장기시험 모두 미실행.
- `DV-COMPLETE NOT_RUN`: 전체 게임·완성형 기능/배포 판정 아님.

**D-V 설계는 이 단계에서 완료했으나 실제 통합 C++ 엔진의 인수 완료를 뜻하지 않는다.**
