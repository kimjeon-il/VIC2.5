# H-V4 — 외교 통합 복합 시나리오·수치 오라클

> phase-h-integration/1.0 · 2026-11-04 · DESIGN_SPECIFIED / NOT_RUN
> 6개 HV-X1~X6은 복수 시스템을 동시에 검증하기 위한 **기계판독 fixture + 시나리오 오라클**이다. 테스트 파일과 C++ 실행/성공 증빙이 아니다.

## 1. 공통 실행 규칙

- 모든 사례는 world_seed/ruleset_version/contract_versions, stage 순서 0~11, source_event_id/transaction_id, CountryId/GovernmentId/ProvinceId/조약/조항/기구/위기 ID, 금융통화와 재고 상품 로트·POP 구성을 갖는 작은 World를 사용한다.
- 테스트용 숫자는 가상 단일 통화의 최소 단위/상품 개수이다. 역사적 실측값이나 Victoria 2/3 계수로 쓰지 않는다. 실제 C-E2/C-E5/관세·임금·전투 알고리즘을 우회해서 잔액을 수정하지 않는다.
- **약정·계약·발효·실제 인도/지급/전투 효과는 별개 사건**. 기존 C-M5 PeaceTreaty/C-E5 TradeAgreement를 H Treaty와 함께 두 번 집행하면 실패.
- 각 fixture는 재현 가능한 시점/선행규칙을 명시하며 경제적 파생값의 실제 변화는 다른 조건을 고정했을 때만 수치로 고정한다. 민간기업의 장기 자율 반응, 실제 전투의 확률적 손실량과 국제위기 결과를 임의의 상수로 하드코딩하지 않는다.
- 현재 독립 F 물류와 조직 명의 FinancialAccount는 상위 코드에서 미지원 가능. 실제 기능이 없으면 해당 fixture는 **BLOCKED/Unsupported**, 가상운송이나 가짜 공동계좌로 우회하지 않는다.

## HV-X1 — 관세 면제 조약 × G 무역 × 정부재정 × 영향력

자료: [h_v_x1_tariff_treaty.json](examples/h_v_x1_tariff_treaty.json)

- 월 100 stage0: A→B 상품 거래는 기존 B 수입관세 10%, 과세기준 가격 1,000, 같은 통화, 실제 통관·세액 납부 조건 충족.
- 월 100 stage9: A/B 적법한 외교권한과 비준절차에 따라 **대상 품목에 한정한 관세면제 조항** 서명·비준, 차월(101) 발효 예약.
- 월 100의 관세/실제 납부는 100(과세기준 1,000×10%). 그 달의 실효 수입물량·가격/원장 재계산/과거세수 반환 없음.
- 월 101 stage0 H TreatyTerm 발효; 월101에 적법한 **별개 통관거래**가 동일 과세기준 1,000, 품목·원산지·실효 적용조건을 충족한다면 C-E5/G/C-E4의 관세평가액은 0, 해당 거래에서 발생한 추가 관세 채무도 0. 예상 비용차 100은 다른 조건이 고정된 기준 비교이며 기업 총이익/수입량+100과 동일하지 않음.
- 해당 TradePolicyProjection은 H term_id를 1회만 읽고 같은 CustomsAssessment/Financial 지급에 H와 G가 관세를 중복 추가/면제 금지.
- 영향력(H-D6)은 실제 무역/대체시장·투자/조약 관계를 조회하되 '관세면제 완료→영향력 +10' 직접 지표 원장은 없음. 당월 Stage1 이전 AI 결정을 stage7의 신규 무역결과로 다시 계산하지 않음.

예상 고정 오라클: m100 관세채무100·납부100(입력상 유효 지급); m101 독립 거래의 관세 0; 누적 세금 순수납 100; 조약효력 m100=false, m101=true; 허구 재고/외교 포인트 변화 0.

## HV-X2 — 국제협력체 공동관세·공동기금 × 법률 × 금융

자료: [h_v_x2_organization_dues.json](examples/h_v_x2_organization_dues.json)

- A/B/C가 자격을 충족하고 유효한 Charter Treaty로 경제협력체를 결성하며, 지정 품목의 역내 관세면제와 합법적 공동대외관세 조항을 별도로 규정.
- 회원 3개국 각각 분담금 100 발생, 각 40 실제 송금. 조직계좌는 **적법한 C-E3 OwnerRef/FinancialAccount로 실제 존재**하는 fixture 전제.
- 누적 발생 분담채권/채무 300, 실제 입금 120, 남은 미수·미지급 180; 국가별 금융원장과 조직계좌의 이중 쓰기 없음.
- B는 다른 군사동맹에도 합법적 가입. 중복 조직 가입 자체가 배타적 충돌이 아니다. 경제협력체 회원이라는 이유로 A/B/C가 전쟁에 자동 참전·산업능력 +10%를 얻지 않는다.
- C의 탈퇴 후 남은 확정 채무는 Charter survival clause에 따라 보존. 조직 해체나 지도국 변경이 자산/국가·POP를 삭제하지 않는다.
- 법적 공동관세는 G/C-P/CustomsTerritory 해당 scope에서만 효력, 군사조항/주권 이전 없음.

오라클: confirmed members=3(탈퇴 전), 실제 총 현금120, 미수금180, 별도 무상 현금=0, 복수 회원권 유효, 무조항 자동참전=0.

## HV-X3 — 무역 의존·제재 × 산업 공급망 × 영향력·위기

자료: [h_v_x3_influence_trade_shock.json](examples/h_v_x3_influence_trade_shock.json)

- B의 철강 월 필요량 100. A는 60을 유효하게 공급하고, B의 국내 재고/다른 공급자/기술적 대체로 **같은 기간 추가 35**를 확보 가능; 나머지 비A 기존 공급40은 이미 포함되어 있다고 가정.
- B의 수요량100과 A산60, 추가 대체35만 주어져서는 25 부족이라는 결론을 낼 수 없다. **정확한 계산은 비A 기존 공급40을 제외한 A산60의 대체 가능량35**이므로 공급 중단 후 B의 가용량=40+35=75, 부족=25라는 보존식으로 설명한다.
- 월 m에 A가 적법한 수출금지를 새로 결정했다면 기본 m+1부터 효력. m에는 이미 계약/통관·생산에 발생한 결과를 소급 삭제하지 않는다.
- m+1에 A산 실제 배송 0, 비A 기존 공급40+합법 대체35가 실효 수급일 때 B의 최대 공급75·미충족25, 다만 실제 물류/재정/시장체결이 불가능하면 더 낮을 수 있다. 조업손실은 D/C-E1 레시피·재고·노동에 따라 **추가로** 계산한다.
- A의 기존 대B 매출이 60단위×10이면 기존 기준 매출600이며, 전량 차단 시 대B 매출0. 다른 시장으로의 재판매/기업 이윤·세수는 별도 실제 거래가 있어야 계산 가능.
- H6 경제 영향력 평가는 조달노출/대체능력/자국 역손실 등 조건부 위험을 읽을 뿐 +25 influence points/자동 외교양보 없음. H7 국제위기는 적격 분쟁·항의/요구/협상 이벤트가 있을 때만 발생하고 긴장도 자동 전쟁 없음.

오라클: B 공급 가능75, 부족25, 대B A 공급량0, A 대B 기존 기준매출600→0(정확히 동일 조건의 거래에 한정), 원본 무역물자/돈 자동생성0, H 파생 값에 쓰기 원장0, 자동전쟁0.

## HV-X4 — 국제위기 중립 중재 × 영토 공동관리 × 배상 × 강화

자료: [h_v_x4_crisis_mediation.json](examples/h_v_x4_crisis_mediation.json)

- 유효 Claim/Dispute: A가 B 주권의 Province P를 요구. C는 중립국으로 외교권과 중재의 통신/기관능력이 있음.
- 분쟁이 실제 외교 항의·거부로 국제화되면 H7 Crisis 개시. C는 mediator로 참여, A/B는 복수 제안·조건부 양보를 할 수 있고 A·B 외 다른 두 진영을 강제하지 않음.
- 월 m stage9, A/B가 C 중재의 'P 공동관리 + B→A 배상100(가상) + 군철수' 패키지를 적법 서명/비준한다고 가정. 이행은 m+1 stage0 이후 E JointAuthority·C-M5 troop withdrawal 및 실제 금융 지급에 **각각 합법적 이행 사건 필요**.
- m+1에 유효한 지급약정100 중 20 실제 납부 → 미지급80; 공동관리의 P sovereignty/military_control/civil/fiscal authority는 E-D0의 legal JointAuthority/별개 role 구조를 통해서만 변경. POP 거주/국적·기업 소유권 자동 이전 금지.
- 해당 합의가 전쟁 전 평화적 해결이면 C-M5에 실제 Conflict/EntryToWar가 없고 군사적 휴전/강화의 가짜 전쟁 기록을 생성하지 않는다. 이미 전쟁 중이라면 C-M5의 existing PeaceTreaty/Ceasefire 원본을 mapping해야 한다.
- H Crisis는 적법한 조약이행에 따라 Settled/PartiallySettled 등으로 변하고, 장래 미지급80/조약불이행의 새 분쟁 가능성은 남는다.

오라클: pre-war Conflict 생성0, 배상약정100/납부20/미납80, 중재국 자동 참전0, 기존 POP·국적/시설 수량 변화0, 주권/관할은 적법 명시 JointAuthority 사건에서만 변함. E JointAuthority 미지원시 시험 BLOCKED이지 가짜 PASS가 아니다.

## HV-X5 — 집단방위 발동 × 실제 참전/군수 × POP 피해 × 점령/주권 구분

자료: [h_v_x5_defense_occupation.json](examples/h_v_x5_defense_occupation.json)

- A/B 전쟁; D가 B와 적법 방위 TreatyTerm을 유지하고 있어도 **참전은 D 정부의 C-P/C-M5 적격 사건이 있어야 가능**.
- 이미 편성된 D 부대의 실제 복무자 100, 장비 보유20, 유효 재고/수송/급여가 있다고 가정. D가 참전을 선언하고 실제 보급 10을 이송하면 D 보유20→10·운송/수취 보유가 대응되어야 하며 B에 장비20이 공중 생성되지 않는다.
- 전투피해를 검증하기 위해 C-M5/POP의 **외부에서 주어진 확정 fixture 사건**(전사12, 부상8)을 사용한다. 이 수치는 전투 승패 공식이 아닌 보존식 검증 입력이다. 전사로 세계인구/해당군 복무 적격 -12, 부상 8은 POP 건강상태 전이이며 세계인구에서 추가 -8을 하지 않는다.
- 전투로 B가 A주권 Province P의 military_control을 확보한 사건이 있다면 E sovereignty=A 유지, E military_control=B로 전이. 장기 점령·세금/민정은 별도 제도/권한·인력/재정 사건이 필요.
- D가 단순 지지 선언만 하고 EntryToWar 사건을 제출하지 않으면 교전당사자가 아니며, 총장비/병력은 사전에 존재한 원장 범위를 초과하지 않는다.

오라클: 전사12에 해당하는 실제 POP headcount 감소12, 부상8 인구 총수 영향0, 장비 출발보유 10·수취/운송중10(손실이 없는 가정), P 주권=A/군사통제=B, D 교전당사자 포함은 적법 EntryToWar 이후에만.

## HV-X6 — 조약·재정·영토 공동 이행 × 최종 단계 전역 롤백

자료: [h_v_x6_atomic_rollback.json](examples/h_v_x6_atomic_rollback.json)

- 기존 A/B 정치·경제/군사·무역이 정상인 World에서 새 H 협정: B가 A에 현금100 즉시 지급하고 Province P 주권을 이전해야 하는 **동시 이행(all-or-nothing)** 패키지.
- B의 실효 지불가능 잔고=60, 추가 유효 대출/후급/보증 없음. 명시된 계약은 부분 지급/이행을 **허용하지 않는다**.
- Stage1 사전 예약 실패는 정상 Reject이며, stage11에 인위적인 잘못된 영토 권한/고아 참조를 주입한 경로는 전체 월 StagingWorld에서 HARD ERROR/Abort를 검증.
- 어느 경로에서도 B 잔고 -100, A 잔고 +100, P 주권변경이 부분적으로 성공해서는 안 된다. Stage0 이전 original world의 money/inventory/POP/sovereignty/Treaty/Organization/Crisis/ID allocator/commit_id/event journal hashes가 월 전체 abort 뒤 정확히 일치.
- 다른 유효 독립 명령의 처리 실패 시 월 원자성이 깨지지 않도록 **이벤트종속 DAG와 all-or-nothing 그룹**을 검증한다. 월 중 발생한 정상 명령 자체도 월 전체 abort를 선택한 통합 규약에서는 커밋되지 않는다.

오라클: A/B 현금·재고/국가·E 주권 변경0, B 지불부족40, 성공한 TreatyTerm 조항적용0, month_index/last_commit_id/next_entity_id unchanged, H/C/POP/D/E/G의 정본 hash unchanged, 반복/worker1/N도 동일.

## 8. 판정·수치 오라클의 한계

- HV-X1~X6의 물리적 보존식/법적 상태·수령액 등은 fixed fixture의 정확한 오라클이다. 가격·성장률·POP 정치적 성향·국제적 타협/전쟁위험 수치는 실제 유효 모델과 원장을 통해서만 계산하고 별도 고정 '마법 효과'를 넣지 않는다.
- 단일 통화·단기 운송·작은 조직 fixture에 대한 PASS는 장거리 물류·다중통화·역사적 파라미터/대규모 POP/국가 인수의 충분조건이 아니다.
- F 독립 물류·E JointAuthority·국제기구 재무 OwnerRef·일 단위 긴급전쟁 등 요구 기능이 실제 구현되지 않았다면 시험별 BLOCKED/Unsupported와 선행 게이트를 기록한다. fixture만 검산한 뒤 런타임 PASS라고 부르지 않는다.
- 각 fixture는 [H-V5 72시험](H-V5_TEST_MATRIX.md)의 관련 세부 검사에 연결되며, 단일 fixture의 테스트 수만큼 중복 PASS를 집계하지 않는다.

**현재: 6개 복합 시나리오/오라클을 문서화한 상태. 실제 게임 엔진에 실행하지 않았다.**
