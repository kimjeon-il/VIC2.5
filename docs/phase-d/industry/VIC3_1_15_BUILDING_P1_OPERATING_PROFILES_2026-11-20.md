# VIC2.5 P1 — 건물 52종 운영·상품·설비 연결 (2026-11-20)

> 실행할 수 있는 건물은 C/D의 실제 `ProductionFacility`·`ConstructionProject`를 사용한다. 이 명세는 원작 Victoria 3 1.15의 건물 분류를 참고한 VIC2.5 독자적 운영 계약이며, **원작의 생산 수율·임금·생산능력 수치를 복제하지 않는다**. 빌드/CTEST 미실행.

## 전수 프로필 (52개)

| 건물 원형 | 운영 방식 | 설치 조건 | 산출 상품 | 필수 투입 상품 | 노동 직무 참고 | 공사 자재 예시 |
|---|---|---|---|---|---|---|
| 무기공장 | 제조·실물생산 | 도시 시설 | small_arms | steel, tools | `machinist` | `steel` |
| 화포주조소 | 제조·실물생산 | 도시 시설 | artillery | steel, tools | `machinist` | `steel` |
| 예술원 | 제조·실물생산 | 도시 시설 | fine_art | fabric, dye | `artist` | `wood` |
| 자동차공장 | 제조·실물생산 | 도시 시설 | automobiles | engines, steel | `machinist` | `steel` |
| 전기기기공장 | 제조·실물생산 | 도시 시설 | telephones, radios | tools, steel | `engineer` | `steel` |
| 폭약공장 | 제조·실물생산 | 도시 시설 | explosives | fertilizer, sulfur | `chemical_worker` | `steel` |
| 비료공장 | 제조·실물생산 | 도시 시설 | fertilizer | sulfur | `chemical_worker` | `steel` |
| 식품공장 | 제조·실물생산 | 도시 시설 | groceries | grain | `factory_worker` | `wood` |
| 가구공장 | 제조·실물생산 | 도시 시설 | furniture | wood | `craftsperson` | `wood` |
| 유리·도자기공장 | 제조·실물생산 | 도시 시설 | glass | coal, sand | `factory_worker` | `stone` |
| 동력기관공장 | 제조·실물생산 | 도시 시설 | engines | steel, tools | `machinist` | `steel` |
| 탄약공장 | 제조·실물생산 | 도시 시설 | ammunition | lead, explosives | `factory_worker` | `steel` |
| 제지공장 | 제조·실물생산 | 도시 시설 | paper | wood | `factory_worker` | `wood` |
| 조선소 | 제조·실물생산 | 해안 | clippers, steamers, ironclads | wood, tools | `shipwright` | `wood` |
| 제철소 | 제조·실물생산 | 도시 시설 | steel | iron, coal | `factory_worker` | `steel` |
| 합성물공장 | 제조·실물생산 | 도시 시설 | dye, silk | coal, sulfur | `chemical_worker` | `steel` |
| 방직공장 | 제조·실물생산 | 도시 시설 | clothes | fabric | `weaver` | `wood` |
| 공구공장 | 제조·실물생산 | 도시 시설 | tools | iron, wood | `craftsperson` | `wood` |
| 바나나 농장 | 현장 수확·채취 | 경작권 | fruit | 현장 자원·재생 생물량/실물 계약 | `farm_worker` | `wood` |
| 석탄광산 | 현장 수확·채취 | 채굴권 | coal | 현장 자원·재생 생물량/실물 계약 | `miner` | `wood` |
| 커피 농장 | 현장 수확·채취 | 경작권 | coffee | 현장 자원·재생 생물량/실물 계약 | `farm_worker` | `wood` |
| 구리광산 | 현장 수확·채취 | 채굴권 | copper | 현장 자원·재생 생물량/실물 계약 | `miner` | `wood` |
| 목화 농장 | 현장 수확·채취 | 경작권 | fabric, cotton | 현장 자원·재생 생물량/실물 계약 | `farm_worker` | `wood` |
| 염료 농장 | 현장 수확·채취 | 경작권 | dye | 현장 자원·재생 생물량/실물 계약 | `farm_worker` | `wood` |
| 어항·어업시설 | 현장 수확·채취 | 해안 | fish | 현장 자원·재생 생물량/실물 계약 | `fisher` | `wood` |
| 금광산 | 현장 수확·채취 | 채굴권 | gold | 현장 자원·재생 생물량/실물 계약 | `miner` | `wood` |
| 철광산 | 현장 수확·채취 | 채굴권 | iron | 현장 자원·재생 생물량/실물 계약 | `miner` | `wood` |
| 납광산 | 현장 수확·채취 | 채굴권 | lead | 현장 자원·재생 생물량/실물 계약 | `miner` | `wood` |
| 목장 | 현장 수확·채취 | 목축지 권리 | meat, fabric | 현장 자원·재생 생물량/실물 계약 | `rancher` | `wood` |
| 벌목장 | 현장 수확·채취 | 산림권 | wood | 현장 자원·재생 생물량/실물 계약 | `lumberjack` | `wood` |
| 옥수수 농장 | 현장 수확·채취 | 경작권 | grain | 현장 자원·재생 생물량/실물 계약 | `farm_worker` | `wood` |
| 잡곡 농장 | 현장 수확·채취 | 경작권 | grain | 현장 자원·재생 생물량/실물 계약 | `farm_worker` | `wood` |
| 유정 | 현장 수확·채취 | 채굴권·유정 | oil | 현장 자원·재생 생물량/실물 계약 | `driller` | `steel` |
| 아편 농장 | 현장 수확·채취 | 경작권 | opium | 현장 자원·재생 생물량/실물 계약 | `farm_worker` | `wood` |
| 벼 농장 | 현장 수확·채취 | 경작권 | grain | 현장 자원·재생 생물량/실물 계약 | `farm_worker` | `wood` |
| 고무 농장 | 현장 수확·채취 | 경작권 | rubber | 현장 자원·재생 생물량/실물 계약 | `farm_worker` | `wood` |
| 호밀 농장 | 현장 수확·채취 | 경작권 | grain | 현장 자원·재생 생물량/실물 계약 | `farm_worker` | `wood` |
| 양잠 농장 | 현장 수확·채취 | 경작권 | silk | 현장 자원·재생 생물량/실물 계약 | `farm_worker` | `wood` |
| 설탕 농장 | 현장 수확·채취 | 경작권 | sugar | 현장 자원·재생 생물량/실물 계약 | `farm_worker` | `wood` |
| 유황광산 | 현장 수확·채취 | 채굴권 | sulfur | 현장 자원·재생 생물량/실물 계약 | `miner` | `wood` |
| 차 농장 | 현장 수확·채취 | 경작권 | tea | 현장 자원·재생 생물량/실물 계약 | `farm_worker` | `wood` |
| 담배 농장 | 현장 수확·채취 | 경작권 | tobacco | 현장 자원·재생 생물량/실물 계약 | `farm_worker` | `wood` |
| 포도원 | 현장 수확·채취 | 경작권 | fruit | 현장 자원·재생 생물량/실물 계약 | `farm_worker` | `wood` |
| 포경기지 | 현장 수확·채취 | 해안 | oil | 현장 자원·재생 생물량/실물 계약 | `fisher` | `wood` |
| 밀 농장 | 현장 수확·채취 | 경작권 | grain | 현장 자원·재생 생물량/실물 계약 | `farm_worker` | `wood` |
| 정부청사 | 서비스 | 도시 시설 | 비저장/실제 서비스 | 현장 자원·재생 생물량/실물 계약 | `clerk` | `stone` |
| 항만 | 운송망 | 해안 | 비저장/실제 서비스 | 현장 자원·재생 생물량/실물 계약 | `dockworker` | `wood` |
| 발전소 | 비저장 전력 | 도시 시설 | 비저장/실제 서비스 | coal | `engineer` | `steel` |
| 철도 | 운송망 | 실재 철도 링크 | 비저장/실제 서비스 | 현장 자원·재생 생물량/실물 계약 | `rail_worker` | `steel` |
| 마천루 | 서비스 | 도시 시설 | 비저장/실제 서비스 | 현장 자원·재생 생물량/실물 계약 | `clerk` | `steel` |
| 무역센터 | 상업 | 도시 시설 | 비저장/실제 서비스 | 현장 자원·재생 생물량/실물 계약 | `merchant` | `wood` |
| 대학 | 연구·교육 | 도시 시설 | 비저장/실제 서비스 | paper | `academic` | `stone` |

*공사 자재 및 노동 직무는 분류·기술 보정의 힌트이며 자동 원장 지급·소비 계수가 아니다. 농장·광산의 투입은 유효 토지/자원·기자재 노동 서비스와 시나리오에 지정한 실제 소비재로 처리한다. 필수 투입 상품은 최소 가공 공정의 구조적 요건이며, 시대별 추가 재료·품질·대체공정은 별도 룰셋에서 보강한다.*

## 실제 실행

- `building_operating_profiles.hpp`: 산업별 활동/산출/필수 중간재/직무/입지 및 공사자재 참조. 기존 99개 분류 중 52개 물리시설에 정확히 1:1 정적 매칭.
- `World::seed_building_recipe`: 활동·산출상품·필수 투입상품·기술·고용·시설서비스 계수를 실제 `IndustryRecipe`로 등록. 원료가 없거나 근로자가 없거나 건물이 미준공이면 기존 `Stage::manufacture`가 생산을 거부한다.
- `World::seed_building_resource_site`: 농장·목장·벌목·어업·광산·유정의 실제 `ResourceSite`와 산출품·재생여부 검사. 운영 소모품 `operational_inputs_per_unit`은 생산시 기존 `InventoryLedger`에서 차감(실물 생성과 단일 단계에서 원자적 처리).
- `World::building_production_capacity` / `building_resource_capacity`: 사용 가능한 공장 설비·노동·기존 인도 재료·월 생산실적·현장 월 유량·재생 생물량의 최소 제약으로 계산하는 **읽기 전용 생산 상한**. 실제 단위량은 `IndustryRecipe`·`ResourceSite`의 수치와 현장 상태에서 결정하며 별도 '건물 레벨 생산력' 원장 없음.
- `phasee::World::seed_building_in_province` / `seed_building_construction_in_province`: 기록된 실제 Province·해안·물리적 철도링크 및 기존 소유권 원장의 `land:`/`mining_right:` 자산 보유를 검사. 임의 생성된 역사적 지형/자원 증거로 간주하지 않음.
- 기존 D-C: 사업주·시공자·자재·실제 노동·대금·WIP·준공을 통해 `ProductionFacility.capacity`을 부여. 자재/노동/지급 없는 건설, 무료 설비, 무근거 지하 총매장량을 생성하지 않음.
- 스냅샷: `VIC25_PHASED_SNAPSHOT_8`/`phase-d-cpp/0.8`로 자원현장 실제 소모품 보존. v0.7 저장은 의도적으로 지원하지 않음.

## 직접 연결하지 않는 기능

- `power_plants`: 비저장 전기는 I2-DISPATCH가 준비되기 전까지 실제 공급전력 생산/동월 소비를 만들어내지 않음.
- `port`, `railway`: 실제 E 지역 링크·G 운송사·실측 서비스/용량이 있어야 운송 발생. 물리시설만으로 무료 전국 수송량 생성 안 함.
- 대학·행정·상업·고층건물: 실제 연구·정부/상업·부동산 사업장에 연결 가능한 **시설 프로필**만 제공. 개별 서비스 산출/수요와 사업장 고용·재정의 전체 실행 경로는 아직 각 서비스 도메인에서 연동 필요.
- `occupation_hint`는 UI·향후 직업별 적격성 검사용이며 **현재 직무 전문성을 검증하는 코드가 아님**. 고용인원의 총수와 12단계 노동 사용은 기존 POP/Employment 원장이 실제 처리.
- 1801년 이후 역사적 세계 작물생산량·자원 위치·노선/기술 등장 시기·지역 용적률/농지 생산성을 자동 생성하지 않음. 실제 측정·문헌 데이터는 다음 단계 자료 입력.
- 산출 수율과 각 입력재 소모량, 고용 1인당 생산성은 시나리오가 제공해야 하며, 위 표의 이름만으로 통화·재고·실노동이 생기지 않는다.

검증 파일: `tests/test_phase_d_buildings.cpp`. 52개 프로필 등록, 농업 투입물 차감·정본 저장·원료 부족 롤백, 공장 생산·준공, 지리·토지·철도 링크 거부 등을 포함. **소스와 등록만 완료 / C++ 빌드·실행 결과 미검증**.
