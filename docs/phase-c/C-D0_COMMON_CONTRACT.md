# VIC2.5 Phase C-D0 — 경제·정치·군사 공통 설계 계약

- 상태: **설계 확정 / 구현 전**
- 버전: **phase-c-common/1.0**
- 기준일: 2026-11-03
- 의존 계약: **pop-design/1.1** ([POP-D0](../pop/D0_ARCHITECTURE.md), [POP-D1](../pop/D1_SCHEMA.md), [POP-D2](../pop/D2_TRANSITIONS.md), [POP-D3](../pop/D3_RUNTIME.md))
- 설계 범위: 경제(C-E), 정치(C-P), 군사(C-M) 사이의 공유 객체·원장·이벤트·월별 실행·저장·오류 처리 계약. **세부 정책 공식이나 작전 모델의 구현은 포함하지 않는다.**
- 관련 구현: 메인 브랜치의 S0 C++20 스캐폴드, 별도 [work/pop-phase-b의 인구·사회 엔진](https://github.com/kimjeon-il/VIC2.5/blob/work/pop-phase-b/docs/pop/PHASE_B_IMPLEMENTATION.md). Phase B 구현은 인구/고용 세계만 원자적으로 갱신하며 금융·재고·정치·군사를 아우르는 통합 엔진은 아니다.

## 1. 상위 호환성과 문서 우선순위

1. POP-D0~D3의 **PopulationSlice.count 유일한 인구 원본**, 3계층 분류, 분리된 고용·자산·현금 원장, 사건 기반 인구전이, 희소 셀·안정적 ID, 월별 12단계 및 원자적 월 커밋은 변경하지 않는다.
2. 동일 주제의 규정이 충돌하면 **POP-D0~D3의 인구·인구사건·기존 12단계 정의를 우선**한다. C-D0는 경제·정치·군사에 필요한 추가 범위만 확정한다.
3. 기존 S0 문서와 코드는 레거시 스캐폴드로 유지한다. Phase B의 유형·필드명은 구현에 사용된 어댑터이므로 개념 계약을 바꾸지 않고 매핑한다.
4. 후속 C-E/C-P/C-M 세부 설계는 C-D0의 공통 계약을 변경하지 않는 한 자유롭게 확장할 수 있다. 불가피한 변경은 별도 ADR·버전 증가·마이그레이션으로 처리한다.
5. **설계 확정과 C++ 구현·회귀 통과를 구분**한다. 본 문서 작성은 통합 런타임·회계·전투 엔진의 완성을 의미하지 않는다.

## 2. 객체와 주소의 계층

- **World**: 현재 월, 규칙 버전, 결정론 시드, 레지스트리/원장 및 커밋 체크포인트를 포함하는 하나의 원자적 실행 단위.
- **Country**: 외교적·국제법적 국가 객체. **Government**: 해당 국가의 집권 정부. 입법기관(Legislature)·행정기관(Institution)·법률(Law)·정당(Party)·이익집단(InterestGroup)·정치운동(PoliticalMovement)은 별개 객체다.
- **Region**: 인구가 거주하는 최소 시뮬레이션 지역(residence_region_id). S0에서는 지도 없는 가상 지역이며, 역사 지도 도입 시 프로빈스/기초 구역과 연결할 수 있다. **AdministrativeRegion**은 행정집계, **Market**은 시장청산, **OperationalArea/UnitPosition**은 군사 위치다. 동일 계층으로 간주하지 않는다.
- **Firm**: 고용·차입·투자·계약을 맺는 경제 법인. **ProductionSite**는 해당 기업이 운영하는 구체적 생산시설. **FinancialInstitution**은 예금·대출·지급결제의 경제주체.
- **DemographicCohort / SocioeconomicPop / HouseholdPool**: POP-D0의 세 분류. **PopulationSlice**만 headcount 소유. 경제 POP의 직업은 기업 고용·재산 소유·정치적 소속과 구분한다.
- **MilitaryUnit / Formation**은 부대·편제 객체, **ServiceAssignment**는 복무 인구를 부대에 배정한 관계 레코드. **EquipmentAssignment**는 해당 부대가 사용하는 실물 재고를 참조하는 관계 레코드다.

**금지:** Country.population, Firm.population, MilitaryUnit.manpower, Party.supporters, HouseholdPool.people 같은 독립된 쓰기 가능한 인구 원본. 이러한 값은 PopulationSlice와 유효한 관계 원장에서 산출한 뷰 또는 revision으로 검증되는 캐시로만 제공한다.

## 3. 식별자·참조·이력 계약

1. 각 객체는 **종별 네임스페이스가 구분되는 안정적 typed EntityId**를 갖는다. 내부 64-bit ID, 0은 무효값 예약을 목표로 한다. 분야마다 중복 숫자가 있어도 타입을 통해 다른 ID임을 보장한다.
2. ID는 발행 후 재사용 금지. 분할/병합/국경 변화/해산 시 tombstone과 lineage를 남긴다. 객체 이름·국가코드·표시 지역명은 ID가 아니다.
3. 서로 다른 분야는 ID를 참조할 뿐 상대 분야의 원본 데이터를 복사해서 소유하지 않는다. 모든 활성 참조는 존재하는 원본·유효기간·revision을 가리켜야 한다.
4. 관계는 **valid_from_month / valid_until_month(배타적)** 또는 동일 의미의 유효시점 구간과 source_event_id로 관리한다. 완료된 과거 사건은 덮어쓰지 않는다.
5. 미상(unknown), 추정(estimated), 실제 0, 해당없음(not_applicable)은 구별한다. 추정값에는 출처·방법·정밀도 메타데이터를 보유한다.
6. 정수 인원(People)은 signed int64 비음수, 화폐는 최소 단위 int64, 상품은 상품별 스케일을 가진 int64 고정소수점. 음수 델타는 사건에만 허용한다. 오버플로는 검증 거부하며 중간 합산은 검증된 확장 정수 연산을 사용한다.
7. 계약 체계가 동일해도 경제적 소유/고용/정치적 행동 분포가 다르면 POP-D0의 allocation_partition_id로 구별한다. 분류 이름만 같다고 무조건 병합하지 않는다.

## 4. 지역·주권·시장·행정의 관계

- PopulationSlice의 거주지는 cohort/economic/household에 일치하는 **residence_region_id**다. 국가, 행정구역, 시장, 군사 위치를 인구셀의 추가 인구 원본으로 넣지 않는다.
- **TerritorialJurisdiction** 관계는 (region_id, role, authority_country_or_government_id, 유효기간, source_event_id)로 표현한다. role은 적어도 sovereignty(주권), civil_administration(민정), fiscal_authority(과세 주체), military_control(현지 군사 통제)을 구별한다.
- 단일 역할에 동일 지역·유효시점의 **모순되는 확정 권한**을 중복 허용하지 않는다. 영유권 주장·분쟁은 Claim/Dispute 관계에 별도 기록하며 확정 주권과 혼동하지 않는다.
- **MarketAssignment**는 지역과 시장 사이의 유효기간 있는 관계다. 한 국가에 시장 하나를 강제하지 않고, 시장이 반드시 한 국가에 국한된다고 가정하지 않는다. 같은 상품에 동일 지역의 기본 시장이 두 개로 확정되면 거부한다. 상품별 특례는 C-E2/E5에서 추가할 수 있다.
- 인구 거주 지역과 법적 국적(citizenship_id)은 별개다. 영토 이전이 주민 국적·직업·가구·재산의 즉각적 자동 변경을 뜻하지 않는다. 변경이 필요하면 별도 법률/인구 사건을 기록한다.
- 지역이 실제로 분할·합병될 때는 region lineage와 PopulationEvent로 모든 인구 셀을 보존 이동하고, 해당 고용·자산·가구 관계를 유효한 새 지역에 이전한다.
- 점령군의 위치·군사 통제는 거주 인구의 주소 및 법적 주권과 동일시하지 않는다. 지도는 별도 공간 표현이며 C-D0 실행에 필수이지 않다.

## 5. 원본 데이터 소유권(Owner of Truth)

| 원본 | 유일한 소유자 / 쓰기 권한 | 타 분야의 읽기/파생값 |
|---|---|---|
| 주민 수·인구 상태 | PopulationRegistry / PopulationSlice 및 검증된 PopulationEvent | 경제노동력, 유권자, 병역적격자, 지역인구 |
| 건강·교육·직업 프로필 | POP 분류 레지스트리 및 해당 사건 | 노동/병역/투표 적격성 |
| 가구 풀·인원 배정 | HouseholdRegistry + PopulationSlice | 생활비, 가구 통계, 부양 추정 |
| 고용 인원/계약 | EmploymentLedger | 민간 취업, 기업 인건비, 노동수요 충족 |
| 법인·생산시설·설비 가동 | Firm/ProductionSite Registry | 생산능력·구인·투자계획 |
| 실제 상품·재고·이동 | InventoryLedger(+ Production/Trade 사건) | 시장 공급, 군수품 보유량, 재고가치 |
| 매수/매도 주문·체결 | Order/TradeLedger | 수요, 실제 거래, 국가간 무역 |
| 자산·토지·발행 지분 | OwnershipLedger/AssetRegistry | 배당권·재산기반 정치적 영향력 |
| 모든 금전·금융채권·채무 | FinancialLedger | 임금/세금/군비 지불, 정부·기업 잔액 |
| 조세채무/지출 의무의 발생 근거 | FiscalLedger | 정부 예산 실적. 실제 지급액은 FinancialLedger |
| 국가·정부·법률/정책·의회 | Political/Government Registry | 유효 권리·세율·노동/병역 규정 |
| 정당·운동·선거 결과 | Political/Electoral Registry | 유효 투표수·정부 구성·정책 정당성 |
| 국가·행정·시장·군사 관할 | Jurisdiction/MarketAssignment Registry | 주권·과세권·거래시장·통제지역 |
| 군 복무·부대 배정·지휘상태 | Military/ServiceAssignment Ledger | 현역/예비군·부대 유효병력(인구와 대조) |
| 군 장비 배속·보급 요청 | Military Equipment/Supply Assignment | 실물 소유·실제 수량은 InventoryLedger |

**경계:** FiscalLedger의 조세 부과나 군수조달 계약은 실제 돈의 이전을 뜻하지 않는다. FinancialLedger 지급이 일어날 때에만 돈이 이동하고, InventoryLedger 출고/인도가 있을 때에만 물건이 이동한다. POP의 평균재산/생활수준·국가 GDP/실업률/지지율은 원장으로부터 계산한 결과다.

**고용 이중 원본 금지:** 기존 S0 EmploymentLedger와 Phase B의 고용 데이터는 통합 후 동시에 쓰기 가능한 원본으로 남길 수 없다. C-E1 연동에서 하나의 정본 고용 원장과 Phase B 어댑터를 정의한다.

## 6. 공통 명령·사건·분개 계약

- **Command**: 플레이어/AI/제도의 의사결정 요청. command_id, issuer_id, submitted_month, intended_effective_month, expected_revisions, rule_version, payload. 명령 제출은 아직 실행 성공이 아니다.
- **Plan/Obligation**: 생산·채용·예산·조달·입법 등의 계획/법적 의무. 자금의 실제 지급, 상품의 실제 인도, 인구 전이를 뜻하지 않는다.
- **ProposedEvent**: 검증·자원 배정 전의 사건. event_id, command_id(선택), source_system, source_entity_id, kind, occurred_month, effective_month, stage, dependencies, expected_revisions, provenance, planned_effects.
- **CommittedEvent**: 검증되고 월간 커밋에 포함된 실제 사건. 변경된 개별 원장 batch와 transaction_id 목록, 버전/원인/계보, 확정된 수량·금액을 기록한다.
- **JournalBatch**: 동일 사건에 연결된 Population/Employment/Ownership/Financial/Inventory/Fiscal/Political/Military/Jurisdiction의 작성 묶음. 상품 인도와 대금 결제는 체결 transaction_id로 연결한다.

규칙:
1. 임의 모듈의 직접 쓰기 금지. Command → ProposedEvent → 적격성·잔액·수량·revision·의존성 검사 → JournalBatch 준비 → StagingWorld에 적용 → 전역 검증 → 월말 원자적 커밋.
2. 사건 ID·금전 transaction ID·명령 ID 각각 재적용/중복 수신 방지. 동일 command_id 재송신은 중복 실행이 아니라 기존 결과 조회 또는 동일 실패 재현으로 처리한다.
3. 하나의 경제사건에서 다른 원장으로 참조하는 기록은 **동일한 실물·화폐·인구 효과에 대한 복수 관점**이다. 예: 군수품 구매의 Fiscal 지출 사유, Financial 지급 분개, Inventory 이동은 세 번의 별개 경제가치 생성이 아니다.
4. 금전은 경제주체별 차변·대변이 맞는 복식부기로 처리한다. 은행대출은 예금부채와 대출채권을 함께 생성할 수 있다. **전 세계 예금 합계 불변을 강제하지 않는다.** 은행자산·부채 대응을 검증한다.
5. 재고 항등식: 기말 = 기초 + 생산 + 입고 − 생산 투입 − 소비 − 출고 − 폐기/전투 손실. 발송 후 미인도는 in_transit 보유자로 추적한다. 실제 재고에 없는 물품을 군사/민간에 배속하거나 인도할 수 없다.
6. 인구 이동은 출발 셀에서 차감·목적 셀로 더하는 단일 PopulationEvent로 처리한다. 출생·사망 외 원인에 따른 세계 인구 총량 변화 금지. 자산·채무를 무조건 인원 비례로 배분하지 않는다.
7. 이미 확정된 사건을 되돌릴 때는 이력 삭제가 아니라 새 보상사건/역분개를 쓴다. 미확정 월의 실패는 전체 staging을 폐기한다.

## 7. 정치·법률·재정의 효력

- Country, Government, Legislature, Law, Institution, Party, InterestGroup, PoliticalMovement, Election은 모두 별개 객체/ID다. 정권교체는 국가 재생성이나 자동 법률 초기화를 유발하지 않는다.
- **법률의 제정·폐지·개정**, **예산 승인**, **정책의 집행행위**, **실제 금전 지급**은 각각 별도의 사건/원장 기록이다.
- 법률·제도 변경은 adopted_month, effective_month, authority, legal_basis, version을 가진다. **월 m에 새로 통과·확정된 법률은 원칙적으로 m+1 월초부터 적용**한다. 기존에 예약되어 m부터 유효한 법률은 m의 단계 0 개시 스냅샷에 반영된다.
- 월 m에서 법률·정책 변경에 따른 이미 발생한 임금·거래·조세를 소급 재계산하지 않는다. 예외적 보상·추징은 후속 월의 명시적 금전 거래로 처리한다.
- 이미 유효한 법의 테두리에서 내려진 정부 구매·예산 집행·군수계약 명령은 단계 1에서 자금/권한을 검증하고 해당 월 단계 3/5/7에서 실제로 처리할 수 있다. **예산 승인 = 자금 확보 = 실제 지출**로 혼동하지 않는다.
- 선거권·정치적 영향력은 거주지·국적·법적 지위·재산/직업·법률·시점에 따른 **파생 적격성**이다. 지역주민 전원에게 자동 투표권을 부여하지 않는다. 실제 득표는 선거사건에만 확정 기록하며 같은 사람을 투표수에 이중 포함하지 않는다.
- 선거결과·연정·정권 및 법률 통과의 구체적 처리 알고리즘은 C-P에서 결정하되 위의 유효시점·원본·사건 규칙을 준수한다.

## 8. 복무·군수·전투의 인구/경제 결합

1. **병사 = 실제 PopulationSlice의 일부**. ServiceAssignment는 slice_id, unit_id, people_count, service_kind, 유효시점, source_event_id를 갖고 해당 셀의 active/reserve/pow/missing 분류와 대조한다. 부대의 manpower는 합산 뷰다.
2. 민간 고용과 현역복무의 상호배제는 시대·법률에 따라 평가하지만, 기본 전일제 고용/현역 정원은 동일한 인원에 중복 배정할 수 없다. 정확한 인원 배분을 구별해야 하면 allocation_partition_id로 셀을 세분한다.
3. 통상 거주지와 복무 위치는 별개다. 징집·제대·포로·실종·전사·부상 등 인원 변동은 POP-D2의 원자적 사건이며 군사 명부·민간 고용도 같은 JournalBatch에서 동기화한다.
4. 동원 명령은 월중 계획할 수 있지만 **월말 단계 10에 확정된 새 복무 전이는 다음 달의 노동공급·급여·편제 집계에 반영**한다. 과거 단계의 노동투입·임금지급을 소급 정정하지 않는다. 월중 동원을 지원하려면 후속 일자/서브틱 계약이 필요하다.
5. 장비는 InventoryLedger의 실제 상품/재고다. EquipmentAssignment는 군부대 사용/배정 관계일 뿐 재고수량의 독립 원본이 아니다. 연료·식량·탄약·의약품·장비 소모는 재고 사건과 대조하고, 조달은 정부재정/금융 원장에 연결한다.
6. 전투 결과·사상자는 군사사건으로 기록하고 **전사 인원 감소, 부상/장애 상태 이전, 포로·실종 별도 상태**를 POP-D2와 함께 확정한다. 자연사·전사 중복 계수는 배제하거나 경쟁위험 모형으로 결합한다.
7. 군인가족 보조금·치료비·유족연금·전쟁배상은 법적 지급 사유와 실제 지급을 분리한다. 병력 감소가 금융자산·채무·재고의 임의 소멸을 뜻하지 않는다.
8. 상세 전투 시간/위치·지휘·보급·사상률 공식은 C-M2~M5의 책임이다. C-D0에서 임의 계수로 확정하지 않는다.

## 9. 월별 12단계 실행 규약: POP-D3를 변경하지 않음

| 단계 | 기존 명칭 | C-D0 교차 분야 연계 |
|---:|---|---|
| 0 | opening_snapshot | 전월 확정 상태 + 사전 예약되어 이번 달부터 유효한 법·관할관계를 불변 개시 상태로 확정 |
| 1 | planning_and_credit | 기업 생산·정부예산 집행·군수조달/동원 계획과 자금·권한 확인 |
| 2 | labor_clearance | 실제 인구 노동 적격성, 복무·건강·법적 제한, 기업 구인을 통합 청산 |
| 3 | intermediate_goods | 기업·정부·군수 중간재의 실제 이동·대금 청산 |
| 4 | production | 사람·설비·투입재 제약에 따른 생산 및 재고 사건 |
| 5 | wages_and_transfers | 노동임금·현역급여·정부이전·원천징수의 실제 금융거래 |
| 6 | pop_consumption_plans | 당월 실수령 자금·가구정책에 따른 POP 소비 계획 |
| 7 | final_goods_market | 가구 소비·정부구매·투자재·군수 물자의 실제 인도와 대금 거래 |
| 8 | closing_accounts | 기업/은행/정부 거래결산, 채권·채무·재정 검증 |
| 9 | saving_and_investment | 저축·투자·배당 선언 및 다음 달 적용할 정치·예산·법률 결정의 확정/예약 |
| 10 | pop_demography_and_health | 군사작전의 월말 인구 충격, 사망·출생·건강·전직·징집·제대·이주 등 POP-D2 사건 |
| 11 | repricing_and_validation | 차월 가격·상태 계산, 모든 원장과 관계 제약 검증, 세계 상태 원자적 커밋 |

- 한 단계가 읽는 내용은 단계 0의 immutable OpeningSnapshot과 해당 단계 이전에 검증 완료된 staged 결과뿐이다. 뒤 단계의 매출로 앞 단계의 지출을 소급하지 않는다.
- 단계 9의 정치적 결정/정책 예약은 단계 0에 준비된 당월 법을 뒤집지 못한다. 단계 10의 인구·군사 변화는 다음 달 단계 2/5의 인력/급여 자격에 반영된다.
- 단계 10에서 복수 사상원인·의학적 사망위험을 계산할 때 동일 인원을 두 번 소비하지 않는다. PopulationEvent의 사전배분/의존순서/원자적 검사(POP-D2)가 우선한다.
- 월 내 더 촘촘한 군사시간 계산은 하위 이벤트로 수행할 수 있으나 C-D0의 12개 공개 단계와 월말 원자적 경계를 유지한다.

## 10. 실행 트랜잭션·결정론·오류 처리

- BeginMonth(committed_world, ruleset, seed)에서 **전체 세계에 대한** StagingWorld를 생성한다. Phase B의 부분 World 스냅샷만으로 전체 통합이 원자적이라고 주장하지 않는다.
- 모든 분야별 쓰기는 staging 안에서만 유효하다. 검증 순서는 typed ID/권한/유효시점 → revision/중복사건 → 적격성/인원/자금/재고 → 교차 분야 대조 → 세계 불변조건이다.
- 실패 시 AbortMonth로 인구·정책·세금·기업·군사·재고·금융 변경과 month_index 증가를 **전부 취소**한다. 일부 원장만 확정된 상태를 사용자·AI·저장기에 노출하지 않는다.
- 성공 시 WorldSnapshot + event journal + commit_id를 단 한 번 확정한다. 재시작 replay는 마지막 commit_id 이후 사건만 적용한다.
- 병렬 worker는 개인·전역 mutable 원장을 직접 갱신하지 않고 지역/분야 변경집합을 제출한다. 충돌은 동일 revision과 사건 의존관계로 처리하고 안정적 정렬 순서로 병합한다.
- 난수 스트림: (world_seed, month_index, phase, entity_id, event_kind, draw_index)로 분기; worker 수·순회 순서에 영향을 받지 않는다. 정수 확률·잔여량 및 largest-remainder(동률 ID 순) 배분을 따른다.
- 크로스플랫폼 부동소수점 재현성을 자동으로 주장하지 않는다. 보존식은 고정소수점/정수와 체크 산술로 구현하고 가격·전투의 숫자모형 재현성 범위는 C-E/C-M 수치 계약에서 정한다.

## 11. 불변조건(매월 필수)

- POP: Sum(active PopulationSlice.count)는 하나의 유일한 주민 집계. 세계 주민수 변화 = 출생 − 사망 + 외부 경계 유입 − 외부 경계 유출. 내부 이주·국적 변경·동원·부상은 인구 총합을 변경하지 않는다.
- 지역: 거주지역·국가·행정·시장·군사 위치를 여러 층으로 집계해 주민을 중복계수하지 않는다. 영토 이전 자체로 주민이 복제되지 않는다.
- 노동: 총 주된 계약 배정 인원 ≤ 해당 셀의 실제 노동 적격 인원. 서로 다른 고용주/군대가 같은 사람의 전일제 노동을 이중 사용하지 않는다.
- 군사: Unit.manpower 파생합과 유효한 ServiceAssignment 합이 일치. 서비스 상태 및 전사·부상·포로·실종과 PopulationEvent가 맞는다.
- 상품: 각 보유자/상품의 재고가 음수가 아니고 생산/운송/투입/소모/폐기 사건으로 보존식을 설명한다.
- 금융: 거래별 경제주체의 분개가 균형이고 계좌/채권/부채 및 은행 고객청구권이 대응한다. 대출 예금 창출은 정당한 자산·부채 대응이 있을 때 허용.
- 소유: 발행 지분의 전체 소유자 지분 합이 발행량과 일치. 사망이나 셀 병합으로 소유권이 소멸하지 않는다.
- 정책/재정: 세율·권리·병역 자격 등은 **해당 단계에서 유효한** 법률 버전으로 평가한다. 세입·지출 계획과 실제 분개를 구별한다.
- 사건: source_event_id/transaction_id 재적용 불가; 의존성 사이클·기한 역전·참조 유실·위법한 권한을 거부한다.
- 재현: 동일한 world snapshot·ruleset/version·seed·정렬/숫자 규칙에서 월말 상태와 사건 순서가 재현되어야 한다.

## 12. 저장 헤더·버전 및 성능

세이브의 필수 호환 정보: save_schema_version, pop_contract_version=pop-design/1.1, cross_domain_contract_version=phase-c-common/1.0, ruleset_version, month_index, calendar_id, world_seed, RNG/version, amount/quantity scales, source_provenance_hash, last_commit_id, numeric_policy_version. 사건에는 domain/type, typed ID, origin/effective time, provenance, revision, lineage를 포함한다.

계약이 달라지면 명시적 migration 또는 로드 거부. 과거 사건과 원장의 정합성을 검증하지 못한 상태에서 자동 재생성 금지.

성능 구현은 희소 셀·인덱스/SoA·변경집합·지역별 병렬 처리를 허용하되 외부 API를 가변 포인터에 묶지 않는다. 단계별 시간·최대 메모리·이벤트 처리량·인구셀 분할/병합·원장 검사 비용을 계측한다. 10만·100만 셀 및 1200개월 통합 스트레스는 **후속 구현 인수시험**이며 이 문서의 합격 결과가 아니다.

## 13. 하위 단계에 위임한 것(공통 계약 변경 아님)

- **C-E1~E5:** 기업 소유/자본·노동수요·가격시장/중간재/대체재·금융신용/세금·운송과 무역의 정확한 계산 규칙.
- **C-P1~P5:** 정부형태, 국가기관 구성, 정당과 선거수학, 정책 채택/정치적 조직화, 불안/혁명/내전의 계산 규칙.
- **C-M1~M5:** 세부 병역제/적격 연령, 편제/훈련/군종/장비, 군수망, 전투·상세 사상/포로, 점령/평화조약/전후 복구.
- **C-D4:** 모든 원장을 실제로 통합한 장기 회귀·장애 주입·교차 플랫폼·성능 시험.

이 단계가 아직 미설계라는 의미는 아니며 **C-D0에서 공통 인터페이스와 보존/시점 제약만 확정**하고 도메인별 계산식을 후속 설계에 명시적으로 위임한다.

## 14. C-D0 수락 게이트

| ID | 검사 시나리오 | 반드시 만족할 결과 |
|---|---|---|
| C0-G1 | POP 6인 fixture를 정치·경제·군사에서 동시 조회 | 세계 인구 6명, 3중/4중 인구 집계 없음 |
| C0-G2 | 기업 채용과 병력 배정이 한 셀의 같은 적격자를 요청 | 중복 인력 배정 거부 또는 사건 순서에 따라 명시적 배정 |
| C0-G3 | 정부의 군수품 매입·부대 배속 | 재고 이동·금융 분개·재정 지출 사유 한 transaction으로 교차 검증 |
| C0-G4 | 은행 대출 실행으로 기업 운전자금 확보 | 예금 증가 허용, 은행 대출채권/예금부채 및 차입자 채무 일치 |
| C0-G5 | 월 m 선거권/징병 법률 개정 | 월 m 거래·고용 소급 변경 없음; m+1부터 적용 |
| C0-G6 | 지역 주권 이전·현지 군사 점령 | 주민 인구/국적 보존, 주권·민정·군사통제 구분 |
| C0-G7 | 현역 1000명: 전사 50, 부상 180, 장기 손상 70, 나머지 700 | 주민 950, 복무 명부와 보건/재무/고용 참조 정합 |
| C0-G8 | 동일 사건 중복 적용·셀 부족·금융 지급 실패 | 검증 거부, 전체 월 staging 롤백, month_index 불변 |
| C0-G9 | 동일 초기값/시드/규칙으로 단일·다중 worker | 규정된 범위의 결정론적 월말 상태·이벤트 정렬 동일 |
| C0-G10 | save/load·버전 불일치·재시작 replay | 호환 전환/명시적 거부, 사건 이중적용 금지 |

**C-D0 판정:** 위 사항의 *설계 및 검증 기준*을 이 문서로 확정. 구현·실행·테스트 통과는 별도로 보고해야 한다.

## 15. 확정 ADR

| ADR | 확정 결정 |
|---|---|
| C0-ADR-001 | POP-D0~D3 및 월별 12단계를 공통 계약의 상위 호환 조건으로 지정 |
| C0-ADR-002 | PopulationSlice 단일 headcount와 원장별 단일 소유자 유지 |
| C0-ADR-003 | 지리 거주·주권·민정·시장·군사 통제의 유효시점 관계 분리 |
| C0-ADR-004 | Country·Government·Law·Party·Movement·Institution 분리 |
| C0-ADR-005 | Firm·ProductionSite·FinancialInstitution·MilitaryUnit 분리 |
| C0-ADR-006 | Command/Plan/ProposedEvent/CommittedEvent/JournalBatch 분리 |
| C0-ADR-007 | 금융·재고·인구·군사·정치 상태의 월 전체 원자적 커밋 |
| C0-ADR-008 | 신규 법률 기본 효력은 다음 달; 기존 법 아래 집행은 당월 가능 |
| C0-ADR-009 | 군인은 실제 인구·장비는 실제 재고를 참조하고 중복 원본 보유 금지 |
| C0-ADR-010 | 결산·정치·전투충격은 기존 12단계 내 유효시점으로 연계 |
| C0-ADR-011 | 이벤트 멱등성·인구/금융/재고 보존·결정론·명시적 버전 관리 |
| C0-ADR-012 | Phase B 고용·스테이징의 독립구현을 통합 원장/월 커밋과 동일하다고 선언하지 않음 |
