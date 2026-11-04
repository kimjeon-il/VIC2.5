# H-V2 — 외교 월별 실행순서·교차 사건 DAG·효력시점

> phase-h-integration/1.0 · 2026-11-04 · SPECIFIED / NOT_RUN  
> 상위: POP-D3/C-D0의 월별 0~11단계, 기존 C-P 9a~9e/군사 10a~10f, D-D0/D-V, E-D0, G-D0, H-D0. 새로운 13번째 공개 단계를 만들지 않는다.

## 1. 원자적 월 경계와 읽기 규칙

한 월 m의 확정 WorldSnapshot 및 seed/ruleset에서 BeginMonth → StagingWorld가 시작된다. 모든 분야는 동일한 월초 immutable OpeningSnapshot과 선행 stage에서 검증된 staged 결과만 읽고, 분야별 ProposedEvent/JournalBatch는 소유 원장의 승인된 쓰기 API를 거친다. Stage 11 전체 검증을 통과한 뒤에만 Commit(m)하며, 어느 단계에서든 불변식 위반이 발생하면 AbortMonth가 월 번호·국가/POP·원장·조약/회원·위기·ID allocator·이벤트 저널/캐시 revision을 전부 원상복구한다.

- **승인/계획/법률 채택 != 발효 != 실제 이행.** 조약서명과 의회 비준은 stage9 외교·정치 사건으로 확정, 유효 법률/조항은 기본 m+1 stage0부터 적용. 그 이후 돈·상품·군사/주권 효과는 각각 실제 단계와 원장 사건으로 이행.
- 외교 소식 전달·반대제안은 정보 도달/유효시점과 actor별 정보뷰를 가진다. 이미 끝난 stage3/7 거래를 stage9의 협상성공으로 소급 수정하지 않는다.
- H-D6 영향력·국제협력체 결속력/위기 위험 조회는 읽기 스냅샷에 고정된다. 같은 달 새로운 무역/정치·군사 사건을 조회해서 이미 끝난 외교 선택을 다시 최적화하지 않는다.
- 실제 군사 충돌/영토점령은 C-M5/E stage10의 유효 사건. 외교 위기 최고값 또는 조약 체결만으로 해당 사건을 생성할 수 없다.

## 2. 12단계별 책임

| Stage | 이미 유효한 내용·실행 | 외교에 새로 허용되는 일 | 당월 소급 금지 |
|---:|---|---|---|
| 0 opening_snapshot | 유효 조약·보호국 위임/조직회원/관세·주권/군사통제, 제3국 승인, 기존 전쟁/휴전, 과거 화물 입고 및 H-D1 인식 스냅샷 확정 | H-D6 영향력/조직 결속력·위기 기초 평가를 해당 snapshot에서 동결 | stage9의 신규 법률·조약을 미리 유효하다고 읽기 |
| 1 planning_and_credit | 기업/정부/군수/외교기관의 실제 자금·재고·노동/시설/운송 예약, 기발효 TreatyObligation 이행계획 | H-D2 외교사절·교섭 계획/일반 외교행위의 적격성·예산/통신 예약, 임시 협상 초안의 검증 | 협의 제안만으로 계약상 배상 현금·군대/영토 이동 |
| 2 labor_clearance | POP 실제 고용·외교관/통역/조직 직원·군인 적격성과 노동 서비스 배정 | 이미 승인된 공관/기구 인력 활용 | 같은 공무원/예비군/노동력 중복 사용 |
| 3 intermediate_goods | 제조·군수 중간재 실제 구매, 유효 공동관세/통행권/원조계약 적용 | 외교 term은 read-only 유효 법적 조건 제공 | 이달 미도착 원자재·향후 조약특혜를 현재 공급으로 합산 |
| 4 production | 실제 기업·건설·연구/기술 산출, 시설·실물/노동 제약 | 관측 가능한 경제 사실이 사후 외교 평가에 들어갈 근거 이벤트가 됨 | 같은 달 Stage1 외교협상 결론 소급 갱신 |
| 5 wages_and_transfers | 외교관·기구 직원/군인 급료, 승인된 외교원조·공공분담금 실제 지급 | H TreatyObligation에 실제 Financial/Fiscal source refs 연결 | 승인되지 않은 회비/원조·보상금 무료 지급 |
| 6 pop_consumption_plans | 실제 POP/가계 가처분 현금·가격/공급과 유효 정책을 바탕으로 소비계획 | 외교가 POP 돈/욕구 원본을 쓰지 않고 사후 여론 근거로 읽음 | 외교위기로 명목 소비재·현금 자체를 창조/삭제 |
| 7 final_goods_market | 실제 무역·가계/정부·군수 최종재 거래와 출하·세관/관세/FX, 기발효 수출입 제한 | H treaty term/기구 정책이 G read-only policy projection으로 적용 | 당월 Stage9의 새로운 관세면제/전쟁협정을 미리 적용 |
| 8 closing_accounts | 금융·정부/기업/기구 미수·미지급/채권 및 관세·보험/배상 결산 | H 의무와 실제 이행 수량·금액/미지급·조약위반 증거 정리 | 의무/약정 100을 실제 수취현금 100으로 기록 |
| 9 saving_and_investment | 실효 월말 투자·정치/입법/외교 명령, 협상·조약서명/비준·공동조직 표결/위기 제안 처리 | 충돌·권한/기간/의회 동의 검사, 차월 제도·계약·권한 효력 예약 | 현재월의 Stage0/3/7/8의 이미 지나간 사건 소급 수정 |
| 10 pop_demography_and_health | 기존 유효 군사명령의 전투·점령/군사통제와 POP-D2 전사·부상/징병·이주, C-P 실제 정치 상태 | H가 실제 Conflict/Control/Casualty 결과를 source ref로 관측해 위기 status에 연결 | Stage9 새 전쟁계획으로 당월 과거 전투/인구 피해 생성 |
| 11 repricing_and_validation | C-E2 차월 가격/무역 기초, E 주권/행정과 H 관계·조약/기관·기구/위기, 원장·권한·캐시검증, 세계 원자 커밋 | 오라클/이력·영향력 변경원인/비밀정보 접근/중복이행 검사 | 다른 분야 실패를 묵살하고 외교 변경만 부분 commit |

Stage9a~9e(정치), Stage10a~10f(군사) 내부의 **기존 정의/순서**가 우선이다. H의 세부 하위 이벤트는 해당 stage 진입 전 공통 이벤트 의존 DAG를 제출하고, 기존 C-P/C-M 부단계별 권한·예약 순서를 위반하지 않도록 scheduling adapter로 배치한다. 아직 부단계별 정확한 슬롯 확정이 불가능한 부분은 H-V-L0에서 mapping 표를 작성하기 전까지 BLOCKED; 새로운 H-9a가 원래 정치 9a를 덮어쓰면 안 된다.

## 3. 교차 분야 이벤트·원장 의존 DAG

### A. 통상 조약 → 시장 효과

H2 NegotiationAccepted → H3 Signed → C-P Ratification → H3 TreatyTermEffective(m+1 Stage0) → G TradePolicyProjection → C-E5 CustomsAssessment → C-E4 Fiscal obligation → C-E3 Financial payment / G TradeExecution → C-E2 시장수급·Stage11 차월 가격 → H6 다음 시점 영향력 파생.

- 동일 term_id×jurisdiction×applicable shipment/transaction×effective interval은 정규화된 idempotency key로 적용 **한 번**.
- 관세채무 발생과 국고 현금수입을 구별. G가 이미 계산한 세수/수입가격에 H가 국력보너스를 더하지 않는다.

### B. 방위조약·위기 → 실제 참전

H5 OrganizationDecision or H3 DefenseTerm → H7 Crisis/trigger → H2 diplomatic invocation → C-P constitutional authorization/appropriate treaty procedure → C-M5 EntryToWar & existing MilitaryUnit/ServiceAssignment & actual Fiscal/Inventory/transport budget → C-M Combat/Occupation → POP losses + E military_control Stage10 → H7 crisis/peace status and H1 perception future snapshot.

- 집단방위 의무가 있더라도 실제 법/군사·재정의 이행 요건을 검사. 중립·지원·참전은 다른 사건.
- 무력 충돌 사실과 선전포고/동맹·조약 발효가 상호 중복 원본이 되지 않도록 C-M5 Conflict를 정본으로 사용.

### C. 평화·종속권한/영토 → 관할·재정 변화

H7 SettlementProposal → H2 NegotiationAccepted → H3 TreatyTerm / C-M5 PeaceTreaty adapter → C-P Ratification/Authority → E Province sovereignty or JointAuthority and C-P government/rights succession at scheduled effective_month → C-E3/4 debt/compensation, C-M troop withdrawal, G customs territory/market rules → future POP citizenship/legal events if explicitly adopted.

- Claim/Dispute나 군사 점령 자체는 주권/국적/토지 소유권을 자동 변경하지 않는다.
- 법률상 조항 패키지가 **동시 이행**을 요구하는 경우 동일 월 staging의 모든 필수 이벤트가 성공해야만 commit. 다중월 분할 이행은 각 별도 조건/채무상태를 보존한다.

### D. 투자·문화 교류 → 영향력·위기 평가

C-E3/D/G confirmed foreign investment, trade & credit / C-M actual military support / POP education/cultural exchange → source events (m) → H6 normalized directional observation at future snapshot (m or m+1 state view, original decision frozen) → H2 future offer evaluation/H7 future risk.

- 영향력 자체가 또 G 주문/기업 재고/국제위기 사건을 만들어 동월 G→H→G 순환하지 않는다. 신뢰/결속/위기위험도 read-only.
- 국가 AI는 information_as_of에 접근 가능한 값으로만 평가. 실제 비밀 조약의 공개를 캐시 누출로 해결하면 안 된다.

## 4. 중복 적용·예약·의존 관리

- **원본 적합성**: 사건 key = (source_command/event/term/obligation ID, executing domain, target entity/scope, effective_month, transaction/shipment if applicable, ruleset_version). 하나의 원인 조항을 여러 분야가 참조할 수 있지만 실제 금전/상품/영토 변경은 해당 정본에 한 번만 발생.
- **권한 우선**: Entity ID 존재→C-P AuthorityGrant / H-D4 delegated scope→H3 Treaty valid/ratified/terms→E spatial/market/Customs/CM rights→실제 인구/자금/재고/수송 용량→분야별 ledger writes→global invariants.
- **실제 제약 공통 예약**: 노동/설비/군수/상품·화물/FX/세관·조직기금/정부재정/은행 자금은 C/D의 공통 예약자를 통해 배분. H가 허구의 별도 외교재정·영향력 포인트로 우회 금지.
- **동률·충돌**: 상위 계약/법적 우선권 → 명시된 승인/유효일→사건 의존성 → 안정적인 typed EntityId/command_id 총정렬. 서로 충돌하는 필수 이벤트는 원자 거래 거부 또는 명시적 Partial(법률상 허용)로 처리.
- **구현미지원**: 당월 긴급 교전/합의·F 미연결 수송/국제협력체 전용 재무법인 owner 미지원은 UNSUPPORTED / BLOCKED로 남기며, 마법적 즉시 효과 또는 실패 무시로 처리하지 않는다.

## 5. 시뮬레이션·UI 시간 규약

툴팁 응답은 (world_snapshot, current_month, event/command id, information_view, known law/terms, forecast_horizon, source revisions)에서 생성한다. 사전 툴팁에는 확정 법적 효과와 아직 불확실한 실물/정치 반응, 전쟁위험을 다른 표기로 제시한다. 사후 툴팁은 실제 원장/조약·위기 사건의 provenance/constraint와 예상 차이를 반환. 정보 없는 미래 예측은 "unknown/조건부"이지 확정 효과 아님.

- 실시간 추적을 위해 내부에 UI-only Stage12를 추가하지 않는다. 저장·재생/멀티스레드 수치가 같아야 한다.
- Stage11 검증에서 snapshot/effective_month/history/ID, treaty term → policy → final ledger 및 H6/H7 read-only cache의 revision coherence를 함께 검사한다.
- 본 절차는 **설계된 검증 순서**이며 지금 실행된 C++ 월 통합 경로가 아니다.

인수 근거: [H-V3 36 불변식](H-V3_INVARIANTS.md), [H-V5 72 교차시험](H-V5_TEST_MATRIX.md), [H-V6 결정론·저장·부하](H-V6_DETERMINISM_RECOVERY_PERFORMANCE.md).
