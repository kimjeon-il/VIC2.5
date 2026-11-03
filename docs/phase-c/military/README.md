# VIC2.5 Phase C-M1~M5 — 군사 시스템 설계 색인

- **상태: 설계 확정 / C++ 통합 구현 전**
- **계약 버전:** `phase-c-military/1.0` · 2026-11-03
- **상위:** [C-D0 공통 설계](../C-D0_COMMON_CONTRACT.md) (`phase-c-common/1.0`), [경제 C-E1~E5](../economy/README.md) (`phase-c-economy/1.0`), [POP-D0~D3](../../pop/README.md) (`pop-design/1.1`)
- **인접 설계:** C-P1~P5(정부의 전쟁/예산/병역법 권한·정치 반응; 별도 설계). 정치 문서의 GitHub 병합 여부와 무관하게 C-D0에 확정된 PoliticalRegistry·유효 법률 인터페이스만 참조한다.
- **범위:** 역사 대전략 게임의 병역·편제·군수·작전·사상자·점령·종전을 실제 POP·경제 원장과 연결. 지도 없는 S0에서도 지역/인접관계 그래프로 실행 가능.
- **구현 경계:** 본 문서는 설계 명세이며 전투 공식의 역사적 계수 보정, C++ 군사 엔진, 통합 월 커밋, 실제 역사 군대 데이터/성능/100년 테스트는 아직 수행하지 않았다.

## 세부 설계

| 단계 | 파일 | 결정할 영역 |
|---|---|---|
| C-M1 | [병역·모집·징병·예비군](C-M1_SERVICE_CONSCRIPTION_MOBILIZATION.md) | 실제 인구에서 모집, 복무 배정·현역/예비군·제대·군민 노동 중복 방지 |
| C-M2 | [편제·장비·훈련·지휘](C-M2_UNITS_EQUIPMENT_READINESS.md) | 유효 부대/상급부대, 장비 소유/사용, 상태·준비도·지휘체계 |
| C-M3 | [군수조달·보급·수송](C-M3_LOGISTICS_SUPPLY_TRANSPORT.md) | 생산·정부재정·창고·보급권·운송기간·탄약/식량/의무/정비 |
| C-M4 | [작전·전투·사상자·회복](C-M4_OPERATIONS_COMBAT_CASUALTIES.md) | 월별 작전, 병력/장비/군수 소모, 전사·부상·포로·실종, 병원·제대 |
| C-M5 | [전쟁·점령·종전·복구](C-M5_WARS_OCCUPATION_PEACE.md) | 국제/내전, 교전당사자, 점령과 주권, 협정, 인구/경제/군대 복구 |
| 검증 | [군사 인수시험](MILITARY_ACCEPTANCE_TESTS.md) | 정합 불변식·이중동원·전투/군수·전시경제 수치 사례 |
| 결정 | [군사 ADR](MILITARY_ADRS.md) | 확정 설계결정 및 수치 보정 보류 범위 |
| 예시 | [사상자 1,000명](examples/battle_1000.json), [2개월 보급수송](examples/supply_transit.json) | 기계 판독 검증 fixture; 실제 엔진 세이브 아님 |

## 원본·소유권·참조 경계

| 원본 | 단일 정본 | 군사 분야의 사용 |
|---|---|---|
| 인구수/나이·성별·건강·장애·국적/거주 | `PopulationSlice.count`와 POP 분류/사건 | 병역적격/병력·사망·부상·동원 인원은 항상 실제 slice 합산 |
| 현역·예비군·포로·실종 인구 상태 | POP Demographic service_status + PopulationEvent | 군사 ServiceAssignment의 복무·배치와 대조 |
| 부대 배치·명령·조직 상태 | MilitaryRegistry / ServiceAssignment / MilitaryEvent | `Unit.manpower`는 참조 합계의 파생 뷰 |
| 장비·식량·연료·의약품·탄약 실제 수량 | `InventoryLedger`, 설비 `AssetRegistry` | EquipmentAssignment는 사용/배치 관계로만 유지 |
| 장비/시설 소유권 | `OwnershipLedger` | 군대·정부·납품회사·보급업체의 법적 소유 분리 |
| 군비·급료·조달·국채 | `FinancialLedger`·`FiscalLedger` | 예산 승인·구매계약과 실제 지급 구분 |
| 병참로·운송 서비스·도착시점 | C-E5 Shipment/TransportEdge/TransitInventory | 출발/운송중/도착/폐기를 물리 재고 보존식으로 검증 |
| 징병법·선전포고·종전조약의 권한 | C-D0 Political/Law Registry 및 유효시점 | 법률 변경은 원칙적으로 차월 적용 |
| 영토 주권·민정·과세·군사통제 | C-D0 TerritorialJurisdiction | 점령은 주권 자동 이전이나 국적 재작성 아님 |

## 공통 모델

- **군인은 POP에서 별도로 생성된 사람이 아니다.** `ServiceAssignment`는 실제 `PopulationSlice`에 인원별로 연결하며, 배치 부대·복무형태·상태의 유효기간을 기록한다.
- `Formation`의 병력은 하위 `MilitaryUnit` 합계를 재귀적으로 조회하되 **부대 원본 인원과 다시 합산하지 않는다**.
- **예비군**은 POP의 복무 프로필/동원 가능 인구로 관리하되, 현역 전환 사건 이전에는 민간 직업·취업상태가 유지될 수 있다. 신규 현역 동원은 월말 확정되며 다음 달 노동공급/급료에 반영된다.
- 장비 배정은 실제 보급 재고/설비/소유권과 교차 검증한다. 전투 중 탄약/연료·장비 소모와 포획·폐기는 `InventoryLedger` 사건으로 처리한다.
- 전투는 `CombatOrder`라는 허가된 명령과 실제 유효병력·장비·보급·지형/기상 등 게임 규칙을 이용해 결과를 계산한다. **추상 전투 점수만으로 사상자·무기·재고를 생성하지 않는다.**
- 상병 중첩은 `HealthProfile`로 표현. 전사/일시 부상/장기 기능변화/포로/실종을 중복하지 않고, 실종·포로를 사망자로 자동 처리하지 않는다.
- 전쟁·군사통제·점령·병합·정권교체를 각각 별도 사건으로 처리하며 민간인·포로·부상자의 피해/지원·수용 자원도 실제 POP/재고/예산에 연결한다.
- 시대별 무기·군제·교통/통신·병역법·전투 양상은 버전 있는 Ruleset/역사 자료로 표현한다. 고정된 현대식 군조직·보편징병·무기 기술을 모든 세기에 강제하지 않는다.

## 월별 12단계(POP-D3, C-D0 동일)

| 단계 | 군사 연동 |
|---:|---|
| 0 opening_snapshot | 전월말 변경된 복무상태·명령·조약·관할 및 실제 도착한 군수품/예약상태를 월초 확정 |
| 1 planning_and_credit | 유효 정부·법률에 따른 예산·조달·작전·징집/전역/이동 계획과 자금/재고/운송용량 예약 |
| 2 labor_clearance | 실제 현역 인구를 민간 노동에서 제외; 예비군의 민간 고용 유지, 부대별 유효 인원 대조 |
| 3 intermediate_goods | 군수산업 원료/보급 조달에 필요한 중간재 거래·물품 인도 |
| 4 production | 군수공장·수리시설/의료 서비스의 실제 생산과 물량 생성·소모 |
| 5 wages_and_transfers | 현역 급료·정부부담·군인/가족 급여 실제 금융분개 |
| 6 pop_consumption_plans | 군인가족의 실제 소득/현물·부양 인구를 반영한 소비 계획 |
| 7 final_goods_market | 완성 군수품·식량/의약품·운송 서비스의 조달·출고·납품·배치 |
| 8 closing_accounts | 군사조달/급여·대금·계약채무·정부재정 잠정 결산 |
| 9 saving_and_investment | 군사예산·병역법/전쟁 결정을 다음 달 이후 발효하도록 정치·경제 원장에 예약 |
| 10 pop_demography_and_health | 월초/단계1 확정 명령의 작전 결과·전투 소모·사상자·동원·제대·포로/실종·이주 등 원자적 사건 |
| 11 repricing_and_validation | 단계10 전투 재고/자산 변동까지 포함해 원장/국가·부대 정합 재검증, 차월 상태/가격 계산, 월간 통합 커밋 |

**시간 규칙:** 단계10의 전투는 이전에 확정된 전투가능 인구/장비/보급만 이용한다. 단계9에서 새로 채택한 정책/징집은 과거의 전투능력으로 소급하지 않는다. 단계8 이후 전투소모는 단계10 사건으로 새로 분개한 뒤 단계11에서 월말 결산 정합성을 검사한다.

## 참고 범위

- 미군 육군 [Logistical Impact](https://www.army.mil/article/267699/logistical_impact_operation_success_requires_understanding_sustainment): 보급을 물자·금융·인사·의무지원과 결합할 필요가 있다는 구조 참고(현실 전술 매뉴얼이나 전투 수치 복제 아님).
- [ICRC: 전쟁포로](https://www.icrc.org/en/document/prisoners-war-what-you-need-know), [부상자·환자](https://www.icrc.org/en/law-and-policy/protected-persons-wounded-sick-and-health-care-providers), [점령](https://www.icrc.org/en/law-and-policy/occupation): 신분과 법적 보호·점령/주권 구분에 관한 20세기 이후 규범 참고. 게임의 1801년 등 다른 시기에 1949년 조약을 그대로 소급 적용하지 않고 시기별 법·관습을 따로 표시한다.
- [UCDP definitions](https://www.uu.se/en/department/peace-and-conflict-research/research/ucdp/ucdp-definitions): 내전·전쟁·피해 집계의 연구상 구분 참고. UCDP의 관측상 25/1000명 기준을 **게임 내 전쟁의 발생 요건으로 사용하지 않는다**.

## 구현/검증 구분

완료: C-M1~M5 자료형·이벤트·단계·규칙 경계·인수 검증의 문서화. 미완료: C++ 전투/병참/동원/점령/평화 엔진과 경제·정치 원장의 전체 통합, 역사 모수, 장기 시나리오 성능.
