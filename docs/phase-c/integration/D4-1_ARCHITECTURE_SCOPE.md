# C-D4-1 — 통합 아키텍처·범위·원본 계약

**상태:** 설계 확정 / 구현 전 · `phase-c-integration/1.0` · [C-D4 색인](README.md)

## 1. 시스템 경계와 정본

| 분야 | 쓰기 가능한 정본 | 다른 분야의 처리 |
|---|---|---|
| POP | `PopulationSlice.count`, Demographic/Economic/Household Registry와 PopulationEvent | 고용/소득/유권자/병역자격 인구의 파생 조회, 중복 인구 원본 금지 |
| 고용/생산 | `EmploymentLedger`, Firm/Site/Capital Registry, ProductionEvent | 정치권리·군사동원·경제 생산 사용 인력의 실제 적격성 |
| 상품/시장 | `InventoryLedger`, Order/Trade/ProductionLedger | 군수품·정부구매·가계 소비 모두 실제 수량과 거래를 요구 |
| 재무/소유 | `FinancialLedger`, `OwnershipLedger` | 세금·임금·국채·조달·배상 실제 지급, 금융 채무·물권 |
| 재정 | `FiscalLedger`, Budget/Appropriation | 납세/복지/군사 재정 법적 의무(현금은 FinancialLedger) |
| 국가정치 | Country/Government/Constitution/Office/Law/Party/Election/Movement Registry | 법률·권한·투표/시위 상태만, 실제 사람·돈/병력 중복 금지 |
| 군사 | MilitaryUnit/Formation/ServiceAssignment/Engagement Registry | 실제 POP에 대한 복무 배정; 무기는 Inventory 원본 |
| 지역/운송 | `TerritorialJurisdiction`, MarketAssignment, E5 Shipment | 주권/민정/과세/군사 통제 분리, in_transit 재고 단일 보유자 |

**핵심 충돌을 명시적으로 해소한다.**
- Phase B의 임시 고용 상태 및 기존 S0 `EmploymentLedger`는 **하나의 통합 고용원장**에 이식한다. 임시 어댑터는 정본을 변경하지 않는 read-only view 또는 트랜잭션을 위임하는 facade로 제한.
- `MilitaryUnit.manpower`, `Party.supporters`, `Country.population`는 원본 필드가 아니라 같은 시점·revision으로 계산 가능한 파생 뷰.
- `LoanContract.principal_outstanding`와 은행/차주의 대출 장부에 별도 쓰기 가능한 잔액 두 개를 유지하지 않는다. 통화별 FinancialLedger가 정본이고 계약원금은 검증 캐시/스케줄로 유지한다.
- `EquipmentAssignment`는 보유/사용 권리 관계, `InventoryLedger`가 실제 물량 원본이다. `ShippingManifest.qty`는 보존해야 할 화물 상태에 대한 참조지 독립 물량이 아니다.
- `FiscalAssessment`는 발생 채권·채무와 법적 사유를 관리한다. 세금·배상·군급료의 지급 여부는 FinancialLedger의 실제 분개로 판단.

## 2. 통합 API 계약

논리 API(구현 인터페이스 가이드, 실제 코드 아님):

```text
WorldSnapshot committed;                // immutable committed state
StagingWorld begin_month(committed, ruleset, seed);
StageView view(stage, opening, earlier_confirmed_effects);
Result<EventId, DomainError> propose(Command, StageView);
Result<JournalBatch, DomainError> prepare(ProposedEvent, StageView, Reservations);
Result<void, DomainError> validate(batch, staging);
Result<void, DomainError> apply(staging, batch);   // staging ONLY
Result<CommitId, DomainError> close_month(staging); // verify + atomic publish
void abort_month(staging);                         // no committed changes
```

`JournalBatch`는 사건 ID와 `source_system/source_entity/ruleset/effective_month/stage/dependency_ids/expected_revisions`를 가진다. 각 효과는 `PopulationDelta/EmploymentDelta/FinancialEntries/InventoryDeltas/OwnershipTransfers/FiscalObligations/PoliticalDecision/MilitaryAssignment/JurisdictionChange` 중 해당하는 자료형으로만 추가 가능. 없는 원장에 직접 패치 불가.

각 단계는 phase0 immutable snapshot + 이미 확정된 이전 단계/선행 하위사건만 읽는다. 변경은 **모든 영향을 함께 준비하고 검증한 뒤** staging에만 반영. 전역 stage11 성공 전에는 관찰자/저장기/AI 의사결정에서 월말 확정 상태로 읽지 않는다.

## 3. 명령·법률·자원 예약

- `Command`는 플레이어/AI/법률·기관이 제안한 의도. `ProposedEvent`는 자원/법/참조 적격검사 전 결과. `CommittedEvent`는 stage11 포함 월 commit 후에만 외부 확정.
- 정당/의회의 법률 제안, 선거결과, 모집계획, 정부예산, 수출입 주문은 **실제 실물/돈/병력과 분리**. 계획 실패는 계획을 미수행으로 기록하고 예산/재고 원장을 임의 수정하지 않는다.
- `FundingReservation(account_id,currency_id,amount,priority,expiry)`와 `InventoryReservation(holder,good,qty,stage,expiry)`, `Labor/ServiceAllocation(slice_id,count,period)`, `TransportReservation(edge,capacity,period)`, `Seat/VoteUniqueConstraint(election/constituency/anchor)`가 서로 충돌하지 않는지 검증.
- 같은 돈을 임금/원료/군수 구매 두 군데 동시에 확정 예약할 수 없으며, 현역과 민간 전일제 노동, 같은 사람의 이중 군 배치, 같은 장비/탄약의 두 전투 예약이 모두 거부/재배분 대상.
- 다른 선거의 표, 회원/지지/선거참여와 노동은 **항상 상호배타적이지 않다**. 예를 들어 한 사람이 월 초에 일하고 단계9 선거에 투표하는 것은 허용; 투표가 POP를 '소모'하지 않는다. 시위·파업으로 실제 근로시간/근로관계가 점유될 때에만 지정 기간의 노동·행동 배분 충돌을 검사한다.
- 법률 채택월 m의 단순 기본 발효는 m+1 stage0. `existing_authority`에 따른 m의 지출/명령·민간 고용, 실제 이미 완료된 투표는 소급 수정 금지. m 말에 사망한 주민이 m의 이전 단계에 투표했다면 그 표는 자동 취소되지 않는다.
- `ServiceAssignment`로 복무를 월말 새로 할당한 사람은 m 전투에 참여할 수 없으며 다음 월의 고용/급여·군 전투 가능 요건을 재계산한다.

## 4. 출처·권한·유효시점·소유권

- 모든 domain ID는 안정적 typed EntityId, 0 예약. 임의 문자열 이름이나 국명/도시명/선거구명을 ID로 재활용하지 않는다.
- `Country`와 `Government`, 선거결과와 정부구성, 군사 통제와 주권, 세금 권한과 군사적 점령은 분리. 정부가 바뀌었다고 국채·법률·기업/재산·국적을 자동 초기화하지 않는다.
- 지역에는 `sovereignty`, `civil_administration`, `fiscal_authority`, `military_control`의 기간/권한 원본이 유효하며, 필요시 분쟁·청구권을 별도로 표현한다. 범위가 겹치는 모순되는 확정 권한은 검증 오류.
- 개별 지휘관/정치인용 `PersonAnchor`는 실제 `PopulationSlice`에서 배정한 정확히 1명의 대표를 참조하거나 추상 직책만 사용. 별개의 인구 원본 생성 금지.
- 역사 자료에서 미상/추정/실제0·법적 권한/실효 가능성, 계약 형식/실제 비용을 분리. 외생 초기조건도 provenance와 계정의 정합성을 요구.

## 5. 호환성 차단조건

- 현재 `main`의 POP-D0~D3는 설계이고, Phase B의 실물 인구사회 구현은 `work/pop-phase-b`에 존재한다. 구형 인구 모델과 Phase B에 독립 인구 원본이 남아 있는 동안 '완전 통합'을 선언할 수 없다.
- C-P1~P5는 2026-11-03 생성한 사용자 전달용 아티팩트(계약 `phase-c-politics/1.0`, 통합 MD SHA-256 `16ba370c017453011752a7ce7375a10e9b020231c787fb77d7885f042417be5d`)가 있으나 GitHub main의 정본은 아직 아니다. 전체 테스트 구현 전에 원문 동일성·ADR·CP-INV/CP-T 수량/링크 검증 후 별도 PR/커밋으로 등록해야 한다.
- D4는 이 문제를 설계 이슈로 등록하며 **GitHub에 존재하지 않는 정치 구현을 존재한다고 가정하거나 다른 브랜치의 Phase B 테스트 결과를 전체 엔진 통과로 전용하지 않는다.**

## 6. 검증 레이어

**층 A(자료형/스키마):** typed ID·단위·소스·법·유효기간·버전 필드 검사.

**층 B(도메인 원장):** POP, Employment, Financial, Inventory, Fiscal, Political, Military, Jurisdiction 각각 단위 보존/거부.

**층 C(교차 이벤트):** 하나의 TradeFill/Payroll/Mobilization/Combat/Tax/Treaty 등으로 여러 원장의 정합성 검증.

**층 D(월간 통합):** 12단계·의존 DAG·정해진 법률/병역/선거/전투/운송 시점, 실패 주입, atomic commit/abort.

**층 E(장기/운영):** 복합 국가/지역·세이브·정확 재현성·병렬/최적화·100년·성능·충격.

이 레이어의 성공은 별도 로그·시드·ruleset·스키마 SHA·커밋으로 증명한다. '비고 없는 PASS'나 비유효 로그는 인정하지 않는다.
