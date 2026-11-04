# VIC2.5 D-V1 — 통합 아키텍처·범위·계약 베이스라인

> **설계 확정 / 실제 C++ 통합 구현·시험 미실행** · `phase-d-integration/1.0` · 2026-11-04  
> 상위: [D-D0](../D-D0_COMMON_CONTRACT.md), [건설 D-C1~C5](../construction/README.md), [산업 D-I1~I5](../industry/README.md), [기술 D-T1~T5](../technology/README.md), POP-D0~D3, C-D0/C-E1~E5/C-P1~P5/C-M1~M5/C-D4.

## V1-1. 통합의 정확한 목표

건설·산업·기술의 개별 작동이 아니라 **같은 실제 POP·계좌/채무·상품/운송·설비/시설·법적 권리·정부/군대**를 두고 경쟁·생산·연구·공사·변경하는 경우에도 월간 보존식·시점·결정론/원자성이 유지되는지를 검증한다.

검증 기준은 원작 게임의 건설력/기술 점수/건물 레벨이 아니라 VIC2.5의 실제 실물·고용·회계·소유·지식/설비 상태다. 역사 대전략 상황에서도 단계별 사건과 수학적 오라클을 재현할 수 있어야 한다.

## V1-2. 상위 계약과 개발 브랜치

| 소스 | 기준 계약·의미 |
|---|---|
| `POP-D0~D3` | `pop-design/1.1` — 유일 `PopulationSlice.count`, 코호트·건강/자격·인구 전이·월 원자성 |
| `C-D0` | `phase-c-common/1.0` — Country/Region/기업/정치·군사/권한·원장/시간/typed IDs |
| `C-E1~E5` | `phase-c-economy/1.0` — Firm/ProductionSite/Recipe/Asset, 고용/상품/금융/재정/무역 |
| `C-P1~P5` | `phase-c-politics/1.0` — 법률/권한/기관·선거·정치운동/경제 충격 |
| `C-M1~M5` | `phase-c-military/1.0` — 동원/군수·장비/부대·전투·점령/재건 |
| `C-D4` | `phase-c-integration/1.0` — 12단계·9a~9e/10a~10f, 전체 World 커밋·48 프로토타입 시험 |
| `D-D0` | `phase-d-common/1.0` — 건설/산업/기술 추가 계약·14 불변식/24 시험 |
| `D-C1~C5` | `phase-d-construction/1.0` — 실제 시공·WIP/계약·검수·차월 가동·25 불변식/45 시험 |
| `D-I1~I5` | `phase-d-industry/1.0` — 자원/레시피·설비·입지·기계화/기업생멸·25 불변식/45 시험 |
| `D-T1~T5` | `phase-d-technology/1.0` — 지식·연구/전파·권리/훈련·사업장 도입·25 불변식/45 시험 |

**레포지터리 운영:** Phase D 설계 정본은 `work/phase-d-design`. 2026-11-04 기준 C-D4 통합 프로토타입 소스는 `work/phase-c-engine-integration` Draft PR #6에 있으며 `main`에 병합되지 않았다. 그 브랜치의 **48 합성 인수시험 PASS** 보고서는 Phase D 기능/테스트 PASS를 의미하지 않는다. 구현 착수 전 실제 브랜치/커밋·Phase B/정치 정본/소스 배치를 확인해 **통합 소스 기준선을 명시적으로 선택·고정**한다. 다른 브랜치에서 새 World를 하나 더 생성하지 않는다.

## V1-3. Single source of truth

| 실제 원본 | 단일 소유 | Phase D의 읽기/쓰기 경계 |
|---|---|---|
| 인구수/건강/숙련/교육 | POP Registry `PopulationSlice` 및 인구·교육/건강 사건 | 연구원/공장/시공/군인 인원수 복사 금지 |
| 고용/주된 일·자영/부역 | `EmploymentLedger` | `CrewAssignment`·`ResearchWorkEvent`·`SiteActivityAssignment`는 유효 할당 참조 |
| 기업·생산사업장·자본재/시설 상태 | C-E1 `Firm/ProductionSite/CapitalAsset` | 산업/기술/건설 결과는 실제 C-E1 자산·설비에 한 번 반영 |
| 상품/재고·원료/장비 상품·운송중 | `InventoryLedger` 및 E5 Shipment | 기계 재고/설치 자산 이중계상 금지 |
| 예금/채권/채무·WIP·실제 송금 | `FinancialLedger` | 공사/연구/기계화 `FundingPlan`은 원장 참조 |
| 법적 소유·주식·특허/실시권·토지권 | `OwnershipLedger`와 유효 법적 권한 C-P | 기술 보유≠특허권, 계약자≠최종 자산 소유자 |
| 세금/예산 의무·지출 근거 | `FiscalLedger` / C-P 정책 | 정책 채택≠현금 지출 |
| 관할/선거/법/기관/군사 | C-D0/P/M 고유 정본 | 시설·연구/군수에 대해 권한 참조, 주권≠점령/사용권 |
| 실제 공사·검수/미완공 상태 | D-C `ConstructionProjectRegistry` | 돈·상품·설비/사람 원본 복제 금지 |
| 산업 분류·기술/레시피 정의 | D-I `IndustryCatalog`, D-D0 `TechniqueCatalog`/C-E1 `ProductionRecipe` | 규칙/관계이지 별도의 경제 잔액 아님 |
| 기관별 지식/연구 수행 증빙·도입 관계 | D-T `KnowledgeRegistry`/`ResearchProject`/`SiteTechniqueAssignment` | 지식은 Inventory 자산·사람·설비/즉시 전국 생산성 아님 |
| 지질 탐사·채굴 현장의 월 유량·재생 생물량 | J-R1 `ResourceSite/ExplorationState/RenewablePool`, C-E1 생산/재고 | 지하 총매장량·잔량 저장 금지, 생산된 상품은 실제 Inventory 원장 |

## V1-4. 한 월의 공통 실행·원자성

~~~text
WorldSnapshot(m opening)
  + immutable effective Law / POP / inventories / financial / skills / knowledge / assets
  + validated commands / seed / ruleset_version
     -> BeginMonth -> single StagingWorld
         -> stage 0..11 ordered event proposals, locks and JournalBatches
         -> per-stage domain finalization
         -> cross-domain reconciliation in stage11
            -> ALL PASS: one World commit + stable event journal/checkpoint
            -> ANY HARD FAIL: full Abort (all domains, next IDs, reservations, month index)
~~~

- 한 프로젝트가 상품·돈·사람·소유/권한·산출/지식에 동시에 영향을 줄 때 `depends_on_event_ids`·typed ID·revision을 가진 **원자적 JournalBatch**와 부문별 원장 분개가 필요.
- `ConstructionProject` 진척·`ResearchWorkEvent`·`ProductionEvent`는 서로가 다른 원본을 곧바로 수정하는 API가 아니라 공통 `JournalProposal`을 생성하고 각 원장 owner가 적용·검증한다.
- `planned/forecast/approved/contracted/reserved` 및 `committed/physically_delivered/actually_paid/accepted/commissioned` 상태·효력월을 구분한다.
- 불변식/테스트는 [V3](D-V3_INVARIANTS.md) 및 [V5](D-V5_TEST_MATRIX.md). 수용 게이트는 [V7](D-V7_GATES_ADRS.md).

## V1-5. 입력 명령과 공통 조회/오라클

`DVCommandEnvelope` (논리 계약): `command_id, actor_ref, authority_ref, issue_month, requested_effective_month, domain, action_kind, affected_refs, expected_revisions, ruleset_hash`. 중복 명령/시점 역전은 거부.

`DVJournalEnvelope`: `event_id, source_command_id?, world/month/stage/subphase, type, subject_refs, depends_on_event_ids, resource_reservations, postings[], effective_month, ruleset_hash, rng_draw_ids?, provenance, expected_revisions`.

검증은 특정 구현 클래스명/메모리 주소에 의존하지 않고 다음 정본 조회를 사용: `PopulationSlice.count`, C-E1 `EmploymentLedger/CapitalAsset/ProductionRecipe`, `InventoryLedger`, `FinancialLedger`, `OwnershipLedger`, `FiscalLedger`, E5 `Shipment/TransportEdge`, C-P/M 상태, D-C `ProjectWork/Commission`, D-I/J-R1 `IndustryActivity/ResourceSiteFlow/RenewablePool`, D-T `KnowledgeState/ResearchWork/Adoption`.

상태 조회 캐시는 revision/hash/효력월의 정합성이 입증되지 않으면 버리고 정본에서 재구축한다. 플랫폼별 허용되는 난수·정수/고정소수점 수치정책도 저장 헤더에 포함한다.

## V1-6. 명시적 미지원/호환성 관문

**D-I2 `I2-DISPATCH`:** 현재 C-E1/C-D4의 stage3 실제 인도 → stage4 생산 계약과 달리, 같은 stage4의 비저장 전력/열 생산·즉시 소비는 **상위 계약 개정(ADR·버전업·이전 세이브 마이그레이션/명시 거부·회귀)이 있기 전 미지원**이다. D-V의 SAFE 시험은 이런 요청을 정확히 `Unsupported`/`DI_ERR_ENERGY_UNSUPPORTED`로 거부하는지를 검사한다. 기능 미지원 표시를 무시하고 당월 전력을 창고 상품처럼 만들거나, 미지원 시험을 조용히 PASS 처리 금지.

사용하지 않는 기능을 구현 완료라 주장하지 않으며, 상위 계약이 나중에 업데이트되면 선택형 `I2-DISPATCH` 시험 프로파일을 추가하고 기존 SAFE 결과를 보존한다.

## V1-7. 설계/구현 상태

- **설계:** D-V1~V7 + 36 불변식/72 추가 교차 시험/30 ADR/fixture와 기존 상위 시험의 의존 관계.
- **미구현:** Phase D C++ 기능의 단일 통합, 대규모/1200개월 시험·저장/리플레이·다중 worker 및 Windows/macOS/Linux 실측.
- **릴리스 조건:** 본 문서·매니페스트 정적 감사만으로 D-V 실제 합격을 선언할 수 없다. 단계별 PASS/BLOCKED/NOT_RUN 근거는 [V7](D-V7_GATES_ADRS.md).

**판정:** `DV-DESIGN CONFIRMED`. `DV-INTEGRATED_CPP NOT_RUN`.
