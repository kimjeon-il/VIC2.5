# VIC2.5 D-V2 — 월별 12단계·의존 DAG·자원경합·시간 경계

> **설계 확정 / C++ 통합 실행 미검증** · `phase-d-integration/1.0` · 2026-11-04  
> 상위: [POP-D3](../../pop/D3_RUNTIME.md), [C-D4-2](../../phase-c/integration/D4-2_MONTHLY_EXECUTION_ORDER.md), [D-D0](../D-D0_COMMON_CONTRACT.md). [색인](README.md)

## V2-1. 기존 파이프라인을 절대로 변경하지 않음

| 공개 Stage | 개시조건/기존 연산 | D-C·D-I·D-T 추가 연산/검증 |
|---:|---|---|
| `0 opening_snapshot` | 0a 이전 commit/예약·중복 명령 확인, 0b 법/관할·복무/시장 효력, 0c 실제 화물 도착/통관, 0d freeze | 유효 지식/면허·POP 훈련자격/연구자료 접근 갱신, 지난달 `ready` 자산/철도 간선의 준공 게이트 재검증과 WIP→CapitalAsset, 기술공정 배정 활성; 모든 조건 확인 후 동결 |
| `1 planning_and_credit` | 유효 정부/법인 권한·실제 인출/유동성·예약 | 공사/투자·제조/에너지/운송·연구/교육·군수 계획 통합, 금전/상품·장비/서비스·운송/작업자 예약 |
| `2 labor_clearance` | POP 적격/ServiceAssignment/실제 자금·고용·자영/부역 | 시공·제조/농업·연구/교육/군수 인력 충돌 해결, 현장별 적격인력·직무/서비스 배분 |
| `3 intermediate_goods` | 단계3까지 실제 인도되는 재고·운송 및 대금 | 공사재료·제조원료/저장 연료·연구시약·설비유지보수의 실물 청산/예약; `in_transit`/단계4 계획산출 투입 금지 |
| `4 production` | 유효 실제 설비·인력·재료의 생산/폐기 | C-E1 생산/채취·I2 부산물/에너지 SAFE, D-C `WorkPackage` 시공서비스·하자/검수, D-T `ResearchWorkEvent`·교육서비스/실적. 같은 stage의 공정별 유효 DAG·자원 잠금 준수 |
| `5 wages_and_transfers` | 총급여/원천징수·정부이전 실제 지급 | 건설·생산·연구/교육/군수 종사자의 임금/이연채무·현물지급 구분, 가계 계좌/정부세액 정합 |
| `6 pop_consumption_plans` | 가계 현금·현물·서비스와 유효 수요 | 연구성공/미판매기계·공사 WIP를 현금/소비재로 생성하지 않음 |
| `7 final_goods_market` | 완제품/투자재·정부/군수/가계 실제 판매·운송·결제 | 신규 기계·공사자재/연구기기 구매·수송·미판매 제조재, 로열티 서비스/사용권 계약. 해당 단계 인도품을 종료된 4단계에 소급 사용 금지 |
| `8 closing_accounts` | 기업·가계/국고/은행 발생·현금 결산 | 시공기성·WIP·시공/하청원가·연구개발비/실시료·기계투자/부도/세금·지급 미이행 검산 |
| `9 saving_and_investment` | 9a 저축·투자, 9b 정치선호, 9c 선거, 9d 법률, 9e 집합행동 | **9a** 연구완료 증거 검증/지식 이벤트 차월 예약, 기술도입/공사·투자계획; **9b** 이미 발생한 실제 고용·임금·부족/오염·교육 서비스에 대한 반응; **9c~9e** 기존 선거/법률/집합행동·차월 효력 변경 없이 유지 |
| `10 pop_demography_and_health` | 10a 작전제안, 10b 경쟁 사망위험, 10c 상병/포로, 10d 출생·교육/이주, 10e 징집·제대, 10f 마감/정합 | 사고·실제 교육/직무훈련 성과→10d POP 적격/학력 전이, 전투·재해의 실제 공사/연구소·기계·철도/기록 접근 피해→10a/10f 자산·권리 사건, 이달 이미 근무/시공·연구한 결과 소급 변경 금지 |
| `11 repricing_and_validation` | 11a 원장 대조, 11b 교차 보존, 11c 차월 예약/해시, 11d 단일 atomic commit | WIP/설비 설치·실적, 지식/연구량·산업 생산/채취·운송/군수/금융/POP·권한/시점/중복 원장 전체 검증, `ready`/지식의 차월 적용 예약 후 한 번 커밋 |

**외부 공개 stage는 정확히 0~11의 12개.** 본 문서의 `4d`, `0d-pre` 등은 기존 단계의 내부 의존순서/로컬 연산 식별자일 뿐 제13단계가 아니다.

## V2-2. 세밀한 내부 순서와 `stage4` 처리

- `0b`: 현재 월부터 유효한 법·주권/사용권·연구공개/특허·POP 자격/기존 부대·기술의 승인상태를 먼저 적용.
- `0c`: 현재 월에 실제 입고/통관·대금·재고보유자 이전이 가능한 운송물만 확인.
- `0c-commission`: 위 개시 법/권리·검사/품질·인도된 설치기계·`KnowledgeState`/숙련·필요 운송망 등을 재검사한 뒤, 전월 `ready` 공사/시설만 `CapitalAsset`/`TransportEdge`를 가동화하고 WIP/소유/재고 대응 분개. 필요한 검사 실패시 시설은 `uncommissioned/repair_required`로 남으며 이전 월의 공사를 취소·자동 환불하지 않음. `0d`에 개시상태 동결.
- `4d`(논리 연산 분류): 실제 제조/채취, 건설 시공, 연구/교육 서비스. **stage4 새로 생산한 제조품을 다른 공장의 stage3 원료로 역송하지 않음**. 건설 현장 내부 같은 stage4의 공정 선후 `WorkPackage`는 D-C3의 결정론적 순서와 같은 노동/자재/장비의 단일 사용조건에 한해 허용.
- `4d-research`: 연구의 실제 실적만 확정, `KnowledgeState` 새 효력 생성 금지. `4d-training`: 교육서비스 실적만 기록, POP 자격은 stage10d의 실제 사건이 필요.
- `9a-research-review`: stage4 검증된 증거·재료/원가/계약의 유효성을 보고 지식 검증/전파 승인을 제안, `effective_month>=m+1`. 같은 stage9a의 새 지식을 다른 연구 프로젝트가 즉시 재사용하여 무한 혁신 연쇄를 만드는 것 금지.
- `10a/10f`: 군사·재해가 그달 `stage4`에 완공한 기초/연구소/철도/공장을 손상시킬 수 있음. stage11 예약은 피해 후 상태로 재검증되어 차월 준공/기술 도입이 취소·연기될 수 있다. 실제 노동자 피해는 10b/10c/10d/10e에 따라 사람 원본 갱신.
- `9b~9e`: 연구기관 설립·법률/예산·선거/정치 반응은 C-P 원본/유효 월의 실제 사건. 새 법 `m` 채택은 기본 `m+1`부터, 이미 진행한 `m` 연구·생산·임금/교육·선거를 소급 변경하지 않음.

## V2-3. 자원 예약/청산 계약

| 경쟁 자원 | 정본 | 동시에 경쟁하는 영역 | 실패시 조치 |
|---|---|---|---|
| 주민 노동·연구/시공/군사복무 | `PopulationSlice`/`EmploymentLedger`/`ServiceAssignment` | 건설·산업·연구·군수/의료·행정 | 하나의 유효 주된 노동 배정, 적격 partition/계약 확인 |
| 기업/정부/은행·가계 지급재원 | `FinancialLedger/FundingReservation` | 자재·임금·연구·건설·군수·세금 | 실제 인출 자금/통화별 예약내 부분확정 또는 명시적 거절 |
| 상품·자재/연료·실험재·군수품 | `InventoryLedger` | 시공/생산·교육/연구·군수/소비·수송 | 재고 보유자·월내 인도/수량·최소 로트 검증, 초과소비 불가 |
| 설치자산·운송/연구설비 서비스 | `CapitalAsset`/C-E5 capacity | 공장라인·건설/연구/군수·수송 | 서비스/실제 사용량 합계<=가능용량, 이중 장비 사용 금지 |
| 토지·광상·공장/특허 사용권 | `OwnershipLedger`/`TerritorialJurisdiction`/`PoliticalRegistry` | 건설·자원개발·기술 실시·정부/군사 | 유효한 법·권리/실제 공간·시점·권한 없는 명령 거부 |
| 지식/훈련·면허/연구시설 | `KnowledgeRegistry`/POP/소유·법 | 연구 프로젝트·기업/군수 기술 도입 | 공개·전파/교육 근거 없으면 자동 보급·성능 상승 금지 |

공통 예약은 `(resource_type, typed_resource_id, month, capacity_slot_or_quantity, holder, law_priority, contract_deadline, stable_command_id)`로 대조. 법적으로 유효한 우선권 → 계약상 우선순위/기한 → 안정적 ID 순서로 정렬; 정수부분 배분은 C-D4의 `largest remainder`와 수치규약을 따른다. 같은 자금/광석/노동·장비를 두 건설사업/공장/연구에 무조건 우선 배정하지 않으며, 정상 부족은 `unmet_order/unassigned/pending_project`로 남긴다.

한 도메인 단계에서 자원이 부족하면 `계획 축소·합법적인 부분 실행·명시적 자금/재료 부족 실패`로 처리. **정상 재고부족/손실·연구 실패를 전역 HARD ERROR로 오인하지 않음.** 반대로 확정된 중복 소비·시간 역행·고아 권리·불균형 분개는 즉시 HARD ERROR/전체 월 Abort.

## V2-4. 의존 그래프

~~~text
m Opening Law/POP/Inventory/Finance/Assets/Knowledge
      │
      ├── funding/credit/rights ─┬─> labor clearance ─> actual intermediate inputs
      │                          │                             │
      │                          └─> planned research/construction/industry services
      │                                                        │
      └─> transports arrived ───────────────────────────────────┤
                                                               v
                              actual industry production + construction work + research
                                      │                  │                  │
                                      ├─ stage5 wages ───┴── stage8 WIP/expense/revenue
                                      ├─ stage7 product delivery/next input purchases
                                      └─ stage9a validated knowledge → next month
                                                               │
                              stage9b political response; 9c election; 9d law → next month
                                                               │
                              stage10 POP skill/health/service + war/disaster asset loss
                                                               │
                              stage11 full conservation / commit / ready reservations
                                                               │
                              (m+1) stage0 validated site commissioning + knowledge
~~~

`depends_on_event_ids`는 같은 stage 내부의 승인된 하위 DAG만 순서를 부여할 수 있으며 다른 stage 완료결과를 역으로 가리키면 실패. 국가·산업·프로젝트별 병렬 `JournalProposal`는 전역 잠금/결정론적 순서로 합쳐져야 하며 worker 수·순회 순서에 독립적인 결과를 생성한다.

## V2-5. 특례와 명시적 거부

- `I2-SAFE`: 확인되지 않은 동월 stage4 비저장 전력 생산/동월 다른 제조기업 사용은 `DI_ERR_ENERGY_UNSUPPORTED` 및 `D4_ERR_RULESET_COMPAT` 매핑으로 명시적 거부. `I2-DISPATCH` 기능은 C-E1/C-D4 상위 ADR·세이브 버전과 독립 회귀 승인 뒤에만 활성화. 기존 stage3~stage4 계약을 문서 하나로 무단 개정하지 않는다.
- 기계상품 stage7 구매는 stage4 이미 종료한 제조·공사의 능력을 올리지 않는다. 건설/개축 `m stage11 ready` → `m+1 stage0` 인수·가동.
- stage10 새로운 인구 자격·징집·사망은 stage2 고용, stage4 생산/연구/시공, stage5 임금·9c 선거 결과를 소급 취소할 수 없음.
- 월별 경제·사회 실제 충격에 따른 정치·군사 영향은 C-P/C-M의 기존 법/복무/작전 시간과 교차 검증하고 주권·군사통제·토지권·세수 권한을 구분.

**테스트:** `DV-T10..T36`(세 분야 교차), `DV-T37..T54`(POP/경제·정치/군사), `DV-T55..T72`(에러/장기/호환). [V5](D-V5_TEST_MATRIX.md).
