# VIC2.5 D-V4 — 복합 시나리오·검증 오라클·수치 Fixture

> **설계 확정 / 정적 수치 검산 대상 / 실제 C++ 실행 전** · `phase-d-integration/1.0` · 2026-11-04  
> 연계 [V2 시간/DAG](D-V2_EXECUTION_DAG.md), [V3 불변식](D-V3_INVARIANTS.md), [V5 72개 매트릭스](D-V5_TEST_MATRIX.md), 상위 DD0/DC/DI/DT/CE/CP/CM/D4.

## V4-1. 기본 테스트 세계

지도 없이 3개 가상 `Region`(A 자원지역, B 공업/주택, C 연구·교육), 2개 `Government`, 3개 상품시장/유효 `TransportEdge/Node`, 광산/제철/기계공장·시공사/미준공 시설·대학/연구소·가계/은행/정부/군부대를 포함한다.

- `PopulationSlice` 원본에 노동자/연구원·군인·아동/고령자 등 실제 적격/비적격을 두고, `EmploymentLedger`에 근무·자가활동/부역·복무·역외 이동을 대조한다.
- 실물: 광상/농지 `ResourceStock/HarvestYield`, 광석/철강·기계상품·연구시약·소모재/연료·군수품의 개별 `InventoryLedger` 로트/보유자·`in_transit`, 설치 `CapitalAsset`/WIP.
- 금융: 기업·가계/정부/은행의 동일 통화 `FinancialLedger`, 승인/실행 대출·지출예약·무역/임금/원천징수·보험/보증/배상/특허료의 채권·채무/현금 이동.
- 권리: `OwnershipLedger`의 부지/기계·지분/특허, `PoliticalRegistry`의 법·주권/군사통제·법률 발효, `MilitaryRegistry`의 군수·포로/부대 등.
- 기술: `TechnologyDefinition`/선행관계·보유자별 `KnowledgeState`/실제 연구·교육/기술자·`SiteTechniqueAssignment`, 실제 사용가능 시설/레시피.

모든 fixture는 `fixture_type: "fictional_test_value"`, 통화·정수/상품 스케일·`ruleset_version`/`seed`/`month_index`, 필수 원장·이벤트, 예상 정상 출력/에러/롤백 상태를 포함한다. 수치는 역사통계나 Victoria 3 규칙 수치가 아니다.

## V4-2. 네 가지 대표 Fixture

| 이름 | 파일 | 실제 결합 범위 | 핵심 오라클 |
|---|---|---|---|
| **DV-X1 산업생산→건설→연구/도입** | [industry-construction-research-chain.json](examples/industry_construction_research_chain.json) | 제철소 단계4 산출→단계7 자재 구매→차월 건설·시공사 급료/WIP→차월 준공, 연구소 실제 연구/지식 검증·제3 기업 기술 접근 분리 | 세계 현금총액·원료/상품·WIP/설비/POP 보존, 신규 지식 보유≠자동 사업장 도입 |
| **DV-X2 동시 희소자원 예약** | [shared-bottleneck-allocation.json](examples/shared_bottleneck_allocation.json) | 제조·시공·군수·연구가 하나의 재고/장비·한정 POP/지급재원 경쟁 | 법적 우선권/명시적 계약 기한→stable ID 정렬, 총배정/실사용≤실재고·가용인원/장비·계좌 |
| **DV-X3 법률변경·전시손상·지식 보존** | [war-policy-research-damage.json](examples/war_policy_research_damage.json) | 연구소·철도/공장이 m stage4 생산/연구, m stage9d 새 법, m stage10 시설손상·전시/의료/연구자료 접근 | m 생산 소급 X, m+1 법적 효력, 피해후 시설 가동거부·지식/특허/채무 이력 |
| **DV-X4 월말 오류주입/전역 롤백** | [atomic-rollback-fault.json](examples/atomic_rollback_fault.json) | 한 월의 stage0 도착/준공→산업/연구/건설→급여/WIP/지식예약→전투시설 피해→stage11 권리 원장 오류 | 모든 단계 수정/예약/ID/달/원장/현장·기술·정책 전부 개시상태 해시로 롤백 |

## V4-3. X1 — 현금/재고·시간 식

예시 시작 월 `m=550`. 최초 현금 세계 합계 150 TEST_CURRENCY 최소단위(정부40 + 발주자100 + 제철소10; 연구소/연구원·제철노동자·시공사/시공노동자는0). 연구소는 유효 공공연구비20을 실제 지급받고, 연구원 급여10을 수령한다. 제철소는 실제 노동급여10을 지급하고 기보유 원료·연료를 사용해 철강4 로트를 생산, **당월 stage7**에 발주자에게 20에 판다.

~~~text
m=550:
  govt 40→20; lab 0→20→10; researchers 0→10
  steel firm 10→0 (wages 10)→20 (steel sold 20); steel workers 0→10
  client 100→80; site steel inventory 4→client steel inventory 4
  world cash end=20+10+10+20+10+80 =150
  new knowledge(m) validated at stage9a, lab usable only at m+1 stage0
m=551:
  client prepays constructor 40 (80→40); builder 0→40→20 after worker wage 20
  build workers 0→20; client-owned steel4 consumed into WorkPackage
  certified construction service40 + previously purchased steel cost20
  client WIP=60; no new money, ready_for_commission stage11
  after actual authorized teaching contact: client observed knowledge effective m+1, not usable
m=552:
  inspected project stage0 commissioned: WIP60→capital asset book_value60
  no new money/GDP from pure WIP reclassification; nominal site service-capacity becomes available
  client still observed technology; new technique not automatically adopted;
  plant cannot output goods without its own separately delivered inputs/qualified labor.
~~~

이 수치 예시는 기존 C-E 회계/기성고·중간재/설비/물권의 실제 JournalBatch가 필요하다는 **정적 수치 계약**이다. 실제 시험에서 상대 분개·현물 인도/자원 출처·시점·자격이 검증되지 않으면 PASS 불가.

## V4-4. X2 — 공유 자원의 합법적 경합

실제 현장 철강6 로트를 건설3, 제조3, 군수2, 연구1 요구(총9)하면 **실제 배정합계 최대6**. 시험 세계의 유효 법적 우선권(군수2), 마감 기한과 계약상 우선권(건설2), 제조 남은2, 연구0 등 **명시적 규칙**으로 부분배정. 정부가 군수라는 이름만으로 항상 최우선이라는 보편규칙을 만들지 않는다.

재고 외에도 실제 POP·장비/시간·금융예약이 각각 독립적인 병목을 형성한다. 근로자와 설비 서비스는 한 월에서 구매/예약→실사용 변환을 **이중 집계하지 않는다**.

## V4-5. X3 — 연구/준공과 전투/정책 효력 충돌

월 m에 연구소가 실제 연구하고 공장·철도가 공정 완료해도, `stage9d` 신규 법 효력은 기본 `m+1`이고 `stage10` 군사/재해 시설 손상·연구원 사고는 `stage11` 다음 달 `CommissioningGate`와 `KnowledgeAccess`를 재검증한다. 안전한 다른 기관의 공개 기술문서·검증된 연구성적/특허권은 근거 없는 세계적 소각 금지.

법률이 과거 월 m의 급여·공사/제조·연구 성과를 소급 변경하거나, 과거 출하된 화물이 손상으로 갑자기 지워지면 HARD 실패. 완공 지연·자산손실·교육/군수 위축은 정상 결과.

## V4-6. X4 — 오류주입의 진짜 원자성

단계11의 고아 자산권/지분 원장 실패를 의도적으로 주입. 이미 작성된 `stage0` 새 도착품/가동, `stage2` 고용, `stage3` 재고 인도, `stage4` 연구/시공/제조, `stage5` 급여, `stage8` WIP/채무, `stage9a` 지식 이벤트 예약, `stage10` POP/군사·피해 사건, 부차 ID 발행 등 **모두** 폐기되고 이전 월 snapshot/해시·ID allocator/최종 commit·month_index/예약·통화별 분개 그대로여야 한다.

음수 재고/권리 고아·잘못된 기술 적용을 자동 수정해 나중에 성공시키지 않는다. 정확한 `DV_ERR_OWNERSHIP` 또는 대응 `D4_ERR_OWNERSHIP_ORPHAN`과 offending source_event_id를 기록. 기대된 오류를 실제로 발생시켜야 **expected-failure 테스트가 PASS**다.

## V4-7. 복합 테스트 오라클·리포트 인터페이스

- `ConservationOracle`: 인구·복무/고용·건설인력/연구원 수; 실제 상품·소모·재생/생산·운송중·설비상품/설치자산; 금융 통화별 금액·채무/소유지분.
- `TimelineOracle`: 주문/돈 예약 ≠ 확정 지급, stage7 부품은 stage4 소급 불가, 지식 `m+1`/준공 `m+1`, stage10 전사·훈련/사고는 이전 근무·투표를 소급 취소하지 않음.
- `CausalOracle`: 실제 근로·금전/실물·연구/배우기/설비/지식/권리·환경/군사 인과관계의 source_event_id·depends_on·원본 revision.
- `NationalAccountOracle`: 산출−중간재=부가가치, 시공 WIP/완공 자산의 추가 중복 GDP/화폐 발생 금지, 현물·무형자산/서비스의 정당한 평가/시점.
- `DeterminismOracle`: 같은 세이브/규칙/seed·정렬·명령·worker 1/N의 사건과 월말 상태 fingerprint 동일.
- `RollbackOracle`: 실패 시 월 시작한 이전 stable World 전체 hash, ID counter/전역 reservation·지식/공사·외부 전자상태/계좌의 흔적 없는 복구.

자세한 테스트 매핑은 [V5](D-V5_TEST_MATRIX.md), 릴리스 판정은 [V7](D-V7_GATES_ADRS.md).

**현재:** 정적 시나리오 명세 작성만 완료, C++ 인수시험/100년 통합 실행·성능/장기회귀는 `NOT_RUN`.
