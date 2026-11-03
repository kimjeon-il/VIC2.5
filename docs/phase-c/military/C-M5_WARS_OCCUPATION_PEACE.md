# C-M5 — 전쟁·점령·종전·전후 복구 상세 설계

- **상태: 설계 확정 / 구현 전** · `phase-c-military/1.0`
- 상위: [C-D0](../C-D0_COMMON_CONTRACT.md), [E4 정부재정](../economy/C-E4_FISCAL_PUBLIC_SECTOR.md), [E5 무역·지역](../economy/C-E5_TRANSPORT_INTERNATIONAL_TRADE.md), [M4](C-M4_OPERATIONS_COMBAT_CASUALTIES.md), [군사 색인](README.md).
- **전쟁 상태·교전·종전·점령·주권 이전은 서로 다른 사건**이다. 전선 지도/전쟁점수만으로 인구·상품·국가채무/재산권을 자동 조작하지 않는다.

## M5-1. 전쟁/교전/동맹의 독립 모델

| 객체 | 최소 필드/책임 |
|---|---|
| `Conflict` | conflict_id, conflict_kind(interstate/intrastate/etc), incompatibility(government/territory/other), initiated_month, status, provenance |
| `ConflictParty` | party_id, conflict_id, state/government/faction/organized_group_ref, coalition_side, role, entry/exit_month, affiliation/command_refs |
| `WarGoal` | goal_id, claimant_ref, target_government_or_region, demanded_change, authority/claim_refs, settlement_preconditions, progress |
| `HostilityRelation` | actor_pair, hostility_state, valid_period, source_event_id, legal_status |
| `MilitaryCampaign` | campaign_id, conflict_id, operational_areas, participating_unit_refs, start/termination, objectives/actual_events |
| `OccupationAdministration` | occupation_id, region_id, controlling_force_ref, civil_authority_ref, extent/effective_control, population_services_plan, period |
| `CeasefireAgreement` | agreement_id, parties, covered_regions/units, monitoring, agreed/effective_period, exceptions/violation_event_refs |
| `PeaceTreaty` | treaty_id, signatories, law/ratification_requirements, effective_month, obligations[], territorial_terms[], implementation_status |
| `SettlementObligation` | obligation_id, debtor/creditor/beneficiary, currency/good/service/region_right, amount/quantity, schedule, legal_basis, linked_financial/ownership refs |
| `PostwarRecoveryProgram` | program_id, government/aid_actor, beneficiaries, damaged_assets/people/regions, resource_budget, labor, schedule, progress |
| `ConflictStatistics` | conflict_id, casualty/territorial/economic aggregations from authoritative ledgers; **only derived, not writable population/wealth** |

`Conflict`는 정치/외교상 실제 교전 관계의 진실값, `Engagement`는 실물 전투, `WarGoal`은 당사자 요구, `PeaceTreaty`는 종전·변경의 법적 근거다. 정전·휴전·강화·종전은 독립 상태/효력 시점을 가진다.

참고: UCDP의 25명/연간 또는 1,000명/연간 전투관련 사망자 기준은 *연구상 관측/분류*의 참고값으로만 다룬다. 이 게임의 국가 간 선전포고·내전 생성·휴전이 반드시 그 인명수 기준을 충족해야만 가능하다는 뜻이 아니다.

## M5-2. 전쟁 발생과 참여

1. 선전포고·국가 간 적대행위·정당한/비정당한 국내 무장충돌 등은 `Political/ConflictEvent`로 제안된다. 전쟁발생의 정부/의회/군주의 권한, 지역적 교전상태, 외교협약/동맹, 기간은 유효한 제도·조약을 참조.
2. 실제 교전에는 자격 있는 `ConflictParty`, 적대행위 관계, 실제 동원 가능한 부대·재정·군수 능력이 필요하다. 전쟁선언과 군대 증강은 동일하지 않다.
3. 제3국 동맹/지원/참전은 별도 `EntryToWar` 사건이다. 군대/예산/무역을 제공하려면 실제 정치적 권한·계약·원장 거래를 수반한다.
4. 국가/정부와 반란세력은 구별. 내전의 `InsurgentOrganization`은 C-P5의 실제 조직과 참여 인구에 근거해야 하며 인원·화기·군수는 민간인→복무/무장조직 편입, Inventory/Ownership 이동으로 설명한다. 추상 불만/반란 진행도에 비례한 '무료 반란군 10,000명' 생성 금지.
5. 정권교체/독립·분리/국가 해체는 유효한 정치·국가·영토 계보와 인구·소유권/채무승계 사건을 동반한다. 교전의 종료만으로 국가 또는 정부가 자동 삭제되지 않는다.

## M5-3. 전쟁 진행과 전쟁 수행 능력

- 지역/기간별 실제 지휘·병력·장비·보급·교전 결과와 세입·부채/무역·공공서비스·주거·가계 피해를 누적한다.
- `WarEffort`는 목표 대비 실제 확보된 영토 통제·군대의 지속가능성·외교/경제 상태·인구 피해/정치적 의지 등을 **다차원 관측값**으로 제공한다. 전쟁점수/소진도를 UI 표시용으로 계산할 수 있지만 독립 원장/마법적 종전 조건이 아니다.
- 전쟁피로와 전후 정책선호는 C-P3/C-P5의 POP 태도/실제 가계피해·유족·실업/징병, 재정압박·공급 부족을 토대로 산정한다. 한 인구셀에 여러 번 동일 전사/전쟁비용을 중복 평가하지 않는다.
- 전시경제에서는 유효 전시법률/예산·조달권한으로 기업/시장·노동/배급 규칙이 바뀌지만, 동일 POP·현금·재고·노동·설비 제약은 유지한다.
- 전투가 종료되거나 지역을 점령해도 상대 전투부대/군수·영토 주민이 자동으로 사라지지 않는다. 유효 후퇴/항복/포로/이주/행정이양 사건으로 처리.

## M5-4. 점령·군사 통제·주권의 분리

국가의 주권 `sovereignty`, 민정 `civil_administration`, 과세 `fiscal_authority`, 군사 실효통제 `military_control`를 **서로 다른 유효기간 관계**로 관리한다.

- `CombatResult/control_event`는 지역에 대한 실제 군사통제 전이 가능성을 제공하지만 유효한 `TerritorialJurisdiction` 갱신/지역의 장악 사실을 검증해야 한다. 군대가 지나가기만 했다는 이유로 행정·과세·주권이 자동 넘어가지 않는다.
- 점령지 주민의 `residence_region_id`, citizenship/legal_status, 자산 소유, 가구 풀은 동일하다. 주권/국적 변동은 새 `Treaty/Legal/PopulationEvent` 또는 근거 있는 제도적 변경 필요.
- `OccupationAdministration`은 실제 군사통제 상황·행정인력·치안·식량·병원·공공서비스/정비와 자금·실물 재고를 소비한다. 점령지는 무료 세수/생산/노동력을 공급하는 가상 매스가 아니다.
- 군정 징발·납세·노동 동원은 별도의 법률·국제법·시대별 관할/강제성 제약 및 실제 Inventory/Financial/POP 사건을 요구한다. 특정 시대에 적용되는 전쟁법/점령법의 위반 여부도 분리 표기한다.
- 20세기 이후의 현대 국제인도법에서 점령은 주권 이전이 아니며, 강제입대/민간인 강제이송에 제한이 존재한다. 1801년과 같은 다른 시대에는 해당 시점 유효했던 법·관습·외교/정치 인식 차이를 **자료 출처와 함께** 표시하고 1949년 조약을 자동 소급 적용하지 않는다.
- 점령지 민간인·부상자·포로의 보호·생계·의료·식량 부족·폭력/징발 피해/피난·이주를 실제 POP-D2 사건·가구와 재정·서비스/창고 재고로 측정한다. 보호대상을 무한한 보급원이 되게 만드는 설계 금지.

## M5-5. 정전·휴전·종전·평화협정

- `CeasefireAgreement`는 교전의 중단, 기간, 당사자·지역별 범위/위반관계. 법적으로 전쟁이 완전히 끝났다고 자동 처리하지 않는다.
- 정전·휴전·종전·평화조약은 `adopted_month`, `effective_month`, `authority/signatories`, `implementation_conditions`, `source_event_id`가 필요. 월 m에 새로 채택된 조약의 기본 효과는 **m+1 단계 0부터** 반영하며, 별도 유효시점 약속/서브틱은 명시해야 한다.
- 조약은 영토통제 재편, 군축/제대, 병력 철수·포로 송환, 재산 반환·배상·부채승계·교역/관세 변경을 별도 `SettlementObligation`/Political/Financial/Inventory/Population/Ownership/Jurisdiction 이벤트로 발행한다.
- 조약에 배상금 100이 적혀 있어도 **당월 현금 100 자동 지급은 금지**. 해당 정부의 새 채무·수령 정부/피해자의 채권을 발생시키고 실제 지급일의 FinancialLedger 이체로 결제한다. 물자배상은 실제 재고/운송과 결제 방식에 맞춰 인도한다.
- 영토할양·독립·편입은 `TerritorialJurisdiction`/Region lineage와 국적/거주·행정/세금/재산 관련 **별도 사건**으로 처리. 국경이 변경돼도 거주 POP 인원이 세계에서 중복되거나 사라져서는 안 된다.
- 전쟁 종료가 모든 현역 군인의 동시 제대를 뜻하지 않는다. 군축/지속경계/철수·예비군 재배치/실제 노동 복귀는 M1의 별도 복무·노동 사건을 따른다.

## M5-6. 전후 복구·인구/금융/군수

1. **인구:** 전사·전염병·장기 부상/장애·민간 피해·인구이동 및 이산가족/포로의 행방은 월별 실제 인구/건강/거주·복무 사건의 합계로 유지. 전투 종료 즉시 전쟁 인구손실 복원 금지.
2. **노동:** 제대/송환·재활·교육·실업/고용·가구 부양 변화는 노동시장/행정제도와 결합. 군인 전역 다음 달 민간 일자리 자동 제공 금지.
3. **경제:** 공장/교량·철도·창고·농지·주택의 파괴/손상은 `Asset/Inventory/ProductionSite` 상태로 남는다. 실제 건설재·인력·돈·운송 기간을 통해 복구해야 생산능력 회복. 단일 `reconstruction +20%`로 자산 창출 불가.
4. **재정·금융:** 국가채무·배상채무/채권·금융기관 부실·가계 재산과 유족지원의 실제 지급을 추적. 정부 변경/독립/병합 후 채무소멸 가정 금지; 별도 승계/재조정/상각 사건 필요.
5. **사회·정치:** 전후 식량·주택·치료·교육·안전/참정권의 실제 상태가 C-P3/C-P5에 반영되고, 정치적 합의/갈등의 반복은 고정/보너스가 아니라 사건과 제도로 결정된다.
6. **보급:** 점령군·귀환자/재건 현장·난민/전쟁포로 수용시설의 실제 식량·의약품·운송수요는 군민 공통 상품/서비스에 대한 수요로 집계하며 군수/가계/공공 예산의 결제를 구별한다.

## M5-7. 전쟁 유형과 역사 데이터

- 국제전·내전·독립전쟁·식민지/종속지역 무장갈등·동맹 개입 등은 `ConflictKind`와 법적 권력의 실체/교전당사자·요구 대상(정부/영토)을 분리해 표현한다.
- `historical_conflict`의 시작/종료 날짜, 참여 국가/정부/조직, 단계별 국경·군사통제 자료와 실제 부대/인구 수치는 provenance/정확도/추정 방법 필요. 사건이 불확실하면 `unknown`, `estimated` 및 범위 표시.
- 연구데이터 집계의 '전쟁'과 게임 안의 법적 선전포고/교전상태/전투 참가는 다르게 구분한다. 사상자 비교 시 군인/민간인, 전투/비전투 사망, 포로/실종, 부상/질환을 중복 없이 구별.
- 조약/국제관례·국가승계의 구체 법적 타당성은 시기별 정책/역사 자료를 근거로 하며 국가/정권을 단순히 '점령하면 영유'로 처리하지 않는다.

## M5 검증 게이트

- M5-G1: 국가 A가 B의 한 지역 military_control을 점령해도 B sovereignty/기존 주민수·국적/재산권을 자동 이전하지 않는다.
- M5-G2: 5,000명 불만 인구의 내전 발생 제안은 즉시 반란군 5,000명을 생성하지 않으며, 실제 조직/군복무 전이·장비/재정으로 소속인원을 설명.
- M5-G3: 휴전 조약 월 m 합의·m+1 발효 → 월 m 이미 실행한 전투/전사자 소급 제거 금지.
- M5-G4: 배상금 100 약정·실제 납부 20 → 현금 수령 20, 미지급 채권/채무 80 보존.
- M5-G5: 국가 분리·편입/행정관할 전환에도 세계 주민수와 기존 금융/소유권/고용 명부를 보존.
- M5-G6: 포로·실종 송환/행방확인은 인구 전이/제대/가구 정보와 연결되고 이중 사망/추가 인구 생성 금지.
- M5-G7: 전투로 시설 10개 파괴됐으면 복구 예산만으로 시설이 원상회복되지 않으며 재료·노동·공기가 필요.
- M5-G8: 내전의 정권교체도 국가 기존 대외채무/국내 재산·가계 인구를 자동 초기화하지 않는다.
- M5-G9: 점령지 보급·민간 서비스가 실제 예산/인력/재고에 연결되어 있고 없는 자원을 자동 소비/생성하지 않는다.
- M5-G10: 종전/점령/주권/과세권/행정/시장관계의 유효기간·국가 계보·서브틱 적용 여부가 정합하다.
