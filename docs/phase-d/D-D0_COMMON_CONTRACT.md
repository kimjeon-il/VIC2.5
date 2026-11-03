# VIC2.5 Phase D-D0 — 건설·산업·기술 공통 설계 계약

> **상태: 공통 설계 계약 확정 / C++ 구현·통합 인수시험 미실행**  
> 버전: `phase-d-common/1.0` · 기준일: 2026-11-04 · 개발 브랜치: `work/phase-d-design`  
> 세부 계약: D-C1~C5(건설), D-I1~I5(산업), D-T1~T5(기술), D-V(검증)  
> 상위: `pop-design/1.1`, `phase-c-common/1.0`, `phase-c-economy/1.0`, `phase-c-politics/1.0`, `phase-c-military/1.0`, `phase-c-integration/1.0`.

## 0. 목적·근거·독자성

역사 대전략형 POP 중심 세계에서 **건설은 실제 시공 서비스와 투입자원을 사용해 자산의 상태를 전환하고**, **산업은 기존 C-E1의 생산 활동을 세부화하며**, **기술은 연구·접촉·채택을 통해 생산방식/공법의 사용 가능성을 바꾸도록** 연결한다.

참고한 Victoria 3의 상위 개념은 건설부문/건설 대기열, 생산방식에 따른 투입·산출·고용 변화, 연구 및 기술확산, 정부·민간 투자 주체의 구분이다. 이 게임의 **건설점수·국가별 무료 건설력·건물 레벨·자동 생산방식 보너스·국가 전체 기술 즉시 적용·고정된 공공/민간 대기열 비율·숫자 계수·게임 데이터**는 이식하지 않는다. 2026-11-04 조사 시 Paradox 공식 위키 웹페이지는 직접 열람 제한(401)이 있어, 위키 관련 공개 해설과 Paradox 개발자 공개 설명을 교차 참조했다. 시간에 따라 원작 패치별 수치·방식이 달라지므로 특정 버전을 VIC2.5 규칙으로 고정하지 않는다.

검토 참고:
- [Victoria 3 Wiki — Buildings](https://vic3.paradoxwikis.com/Buildings) 및 [Production methods](https://vic3.paradoxwikis.com/Production_methods) (직접 접근 제한)
- [Paradox Dev Diary #36 — Construction](https://store.steampowered.com/news/posts/?enddate=1646329240&feed=steam_community_announcements)
- [Paradox Dev Diary #5 — Production Methods](https://store.steampowered.com/news/posts/?enddate=1624550727&feed=steam_community_announcements)
- [Paradox Dev Diary #71 — Autonomous Investment](https://www.paradoxinteractive.com/games/victoria-3/news/dev-diary-71-autonomous-investment-in-1-2)
- [Victoria 3 technology reference (fan-maintained, version caveats)](https://github.com/jakeOmega/Vic3TimelineExtended/blob/main/docs/vanilla/vanilla_technology_reference.md)

## 1. 상위 계약, 원본 소유권, 변경 경계

1. POP-D0~D3의 `PopulationSlice.count`가 유일한 인구수 원본이다. 건설·공장·연구소가 별도 인구수/노동인구를 소유하지 않는다.
2. 기존 C-D0의 typed EntityId, immutable 월초 스냅샷, 0~11단계, `StagingWorld`/`JournalBatch`/원자적 월 커밋, stable ID, 결정론·재현성·전역 롤백을 유지한다.
3. 기존 C-E1의 `Firm`·`ProductionSite`·`ProductionRecipe`·`CapitalAsset`는 재사용·확장한다. 독립된 D-기업/공장/시설/기계 원본을 만들지 않는다.
4. 기존 C-E2~E5의 원장·시장·가격·투자·세금·운송/무역은 그대로 정본이다. 건설대금·연구비·특허/사용료가 새 화폐 원본이 될 수 없다.
5. 기존 C-P의 권한·법률·국가/정부 객체와 `9a~9e` 정치 하위 단계, C-M의 동원/군수/전투·시설 피해, C-D4의 30개 전역 불변조건과 48개 교차 시험은 삭제·약화하지 않는다.
6. D-D0는 세부 산업 분류, 생산레시피와 공법의 실측 계수, 지식 네트워크 알고리즘, 시설별 요구자재·노무/공사기간을 **규칙 파일 및 후속 상세설계로 위임**한다. 임의 원작 수치를 역사적 실측값으로 사용하지 않는다.
7. 충돌 시 POP-D0~D3 → C-D0/C-D4 시간·원본 계약 → 해당 분야의 C-E/C-P/C-M 정본 → D-D0 추가 계약 순으로 해석한다. 상위 의미 변경 시 별도 ADR·버전 증가·마이그레이션이 필수다.

### 1.1 소유권 매트릭스

| 상태/정보 | 유일한 쓰기 정본 | Phase D에서의 역할 |
|---|---|---|
| 주민 수, 연령, 건강, 교육/숙련 | POP Registry, `PopulationSlice` 및 교육/건강 관계 | 실물 노동 적격성, 학습/보급 역량 조회 |
| 고용·자영/부역·주된 노동배정 | `EmploymentLedger` | 건설·연구·일반 생산 간 중복 점유 금지 |
| 기업/사업장·설비 생애주기·가동능력 | C-E1 `Firm/ProductionSite/CapitalAsset` Registry | 신규 산업·건설·연구 사업장은 기존 객체 사용 |
| 상품/원료/설비상품·운송 중 실물 | `InventoryLedger` | 자재·기계의 실제 이동/소비·폐기·회수 |
| 돈·대출·외상·자본화된 공사비 | `FinancialLedger` | 건설계약·연구비·WIP 회계 |
| 부동산/시설·기업/지분·특허 권리 | `OwnershipLedger` | 소유주, 이용권·담보·실시권 관계 |
| 세금·예산·보조금·공공조달 의무 | `FiscalLedger` | 재정권한과 실제 지출의 원장 대조 |
| 거래·가격·선적/통관·경로/용량 | C-E2/E5 시장·운송 원장 | 공장·건설현장·기술자재의 공급 가능성 |
| 공사 상태·분할 공정·검사/인수 | **`ConstructionProjectRegistry`** (신규) | 실제 공정량·기술적 완성도·행정 상태; 돈/상품/인구 복제 금지 |
| 산업분류/공정·기술 요구조건 | **`IndustryCatalog` / `TechniqueCatalog`** (신규 규칙·분류) | 스키마/레시피 참조; 실제 생산/산출 원본 아님 |
| 지식 확보·공개/이전·연구성과 | **`KnowledgeRegistry`** (신규) | 주체별 지식 상태·연구/이전 사건; 새 가용 생산능력 원본 아님 |
| 개별 시설의 기술 채택·전환 예약 | **`SiteTechniqueAssignment`** (신규 관계) | 현장별 유효 공정/도입 기간; 실제 설비는 C-E1 |
| 자연자원 매장/재생/채굴권 | C-E1 `ResourceStock/HarvestYield` 확장 | 지하/미채취 자원과 채굴된 상품재고 분리 |
| 법률·건축허가·환경규제·주권/과세 | C-P `PoliticalRegistry` 및 C-D0 `TerritorialJurisdiction` | 투자·건설·연구/지식재산 권한 검증 |
| 군사 시설 사용/손상·점령 | C-M `MilitaryRegistry` 및 `EquipmentAssignment` | 실제 시설·장비·관할 참조, 중복 생성 금지 |

## 2. 식별자·관계·상태형식

- 신규 안정적 typed ID: `ConstructionProjectId`, `WorkPackageId`, `ConstructionContractId`, `IndustryClassId`, `TechniqueId`, `KnowledgeHolderId`(국가/정부기관/기업 등 기존 typed ID의 참조 래퍼), `ResearchProjectId`, `KnowledgeEventId`, `SiteTechniqueAssignmentId`. `WorkPackageId`는 `ConstructionProjectId`의 하위 작업 객체다.
- 신규 객체는 `source_event_id`, `revision`, `created_month`, `valid_from_month`/`valid_until_month`, `ruleset_version`, provenance를 원칙적으로 갖는다. 신설 객체/상태 해산 이후 ID 재사용 금지.
- 인원 `int64` 비음수, 돈 `Money{currency_id,int64_minor_units}`, 상품 `Quantity{good_id,int64_scaled_units}`, 비율 fixed-point 정책. `WorkQuantity`(해당 공정 전용 고정소수점 실공정량)는 저장 가능한 거래상품이나 '국가 건설력'이 아니며, `ResearchWork`는 연구서비스 실적이고 은행 잔액·거래 가능 상품이 아니다.
- 수량 변환·비율/원가 곱셈에 checked 128-bit 중간연산 사용. signed overflow, 유효하지 않은 상품 스케일, 역행하는 이벤트 의존관계, 다른 통화 단순 합산은 hard error.
- 국가/지역별 산업규모·기술수준·건설능력 등 합계는 정본들에서 파생된 **조회 뷰** 또는 revision 기반 캐시. 독립적인 가변 `country_industry_level`, `country_tech_bonus`, `country_construction_points` 원본 금지.
- 지리적 위치는 기존 `Region`, `MarketAssignment`, `TransportNode/Edge`, `TerritorialJurisdiction`로 표현. 지도/지형 도형 자체는 정본이 아니다.

## 3. 건설 공동 규약 (D-C1~C5의 상한 경계)

### 3.1 객체와 주체

- `ConstructionProject`: project_id, region_id, project_type(new/expand/retrofit/repair/demolish/infrastructure), initiator, client, future_operator, target_site/asset_refs, project_plan_revision, contract_refs, work_package_refs, funding_plan, permit_refs, state, completion_month, commissioning_effective_month, revision.
- `WorkPackage`: work_id, project_id, precedence_refs, required_work_by_recipe, done_work, site/crew_provider_refs, material_requirements, machine_service_requirements, labor_requirements, quality_state, inspection_state. 공정은 선행 공정과 장소·계절적 제약을 갖는다.
- `ConstructionContract`: 발주자/시공 법인 또는 자영 활동자, 작업 범위·단가·검수/인도·지급/지체/보증 조건. 계약의 약속·청구·지급은 각각 별도 사건.
- 건설업체는 기존 `Firm`이며 실제 현장은 `ProductionSite`/건설 서비스업 활동으로 표현한다. 전통 가구의 자가 건설도 `OwnAccountActivity` + 실제 노동배정/자재 소비로 허용한다. 국가가 '무료 노동'을 생성하지 않는다.
- 자금을 제공한 투자자, 법적 시설 소유자, 발주자, 시공자, 실제 운영자는 같을 수도 다를 수도 있다. 영업권/임차권·토지권/사용허가는 소유·관할 원장에서 검증한다.

### 3.2 실제 공정/작업 제약

- 계획 공정량은 `WorkPackage`별 버전 있는 기술적 작업 단위로 정한다. 프로젝트/공정 간 임의 합산하여 세계 공통 건설 점수를 생성하지 않는다.
- `actual_work = min(remaining_work, crew/skill_capacity, machine_service_capacity, delivered_material_capacity, usable_site_capacity, access/legal/weather_limit)`에서 각 상한은 **실제 확보된 자원**으로부터 도출하고, 원재료·노무·기계 서비스의 투입과 `ProjectWorkEvent`를 같은 JournalBatch에 작성한다.
- 동일 노동자/설비의 당월 노동·장비 가용 용량은 여러 사업과 생산활동을 합친 예약량을 초과하지 않는다. `EmploymentLedger`가 인원 정본이며, 근무 일정·현장 배분은 인원 원본이 아닌 유효 계약의 사용 배분이다. 시간제/FTE 정식 확장은 C-E1의 후속 규약이 제공되기 전까지 임의 활성화하지 않는다.
- 철강/목재 등 실제 인도·고정된 공정 레시피에 근거한 소비만 진행량을 창출한다. 예정 배송·미지급 확약·생산 예정분은 이미 확보된 재고가 아니다.
- 불량, 재시공, 사고, 일시중단, 계약 해지, 금융 문제, 전쟁·재난 피해는 진행도/품질/잔존물/채권·채무를 별도 사건으로 조정한다. 이미 투입한 물품·급여를 사건 없이 복구하지 않는다.
- 공사 중인 물리적 성과는 `ConstructionProjectRegistry`의 진행·품질 상태와 금융원장의 `WIP`(건설중 자산) 가치/채권·채무로 추적한다. **WIP의 장부 가치 ≠ 재고 상품량 ≠ 사용 가능한 생산능력**이다. 공사 진행률 자체에 시장 판매 가능한 독립 상품을 만들지 않는다.

### 3.3 완료·승인·가동 전환

- 공사 진척만 100%가 됐다고 자동 가동되지 않는다. 선행 공정/안전검사/인허가/설치된 물품/기술/운영권/시험가동 등 `CommissioningGate`를 검증한다.
- 월 m에 실공정이 완료되면 stage11에 `ready_for_commission` 가능. 원칙적으로 `m+1` stage0에서 최종 재검증 후 기존 C-E1 `CapitalAsset`의 설치/가동 상태를 활성화하고 WIP를 시설자산 계정으로 대체한다. 기존 공장 증축도 기존 `ProductionSite`의 capacity delta가 차월부터 효력 발생.
- 같은 기계 로트가 창고 재고와 설치된 자산 양쪽에 동시에 존재하면 안 된다. 설치 사건은 재고 소비/인수 → `CapitalAsset` 등록/상태전환 → `OwnershipLedger` 권리의 원자적 연결이다.
- 준공은 투자자·시공자에 자동 현금이 지급되는 사건이 아니다. 지급은 계약에 따른 금융 분개가 별도로 존재해야 한다. 설치·준공이 기업 지분·토지권을 임의 이전시키지도 않는다.
- 도로·철도·항만 등 기반시설은 준공·가동 이후 기존 C-E5 `TransportEdge/Node/Capacity`의 유효 관계를 갱신한다. 물리적 연결이 곧 무료·무제한 운송 또는 관세 면제를 뜻하지 않는다.
- 전쟁/재해가 stage10에 완료 예정 자산을 파괴·손상한 경우 stage0의 준공 재검증은 손상 상태를 고려한다. `ready`가 자동 영구 `active` 상태를 보장하지 않는다.

### 3.4 투자 배분

- 정부·기업·협동조합·가구/자가 활동·국외 투자주체의 공사를 같은 프로젝트 모델로 표현한다. 정부/민간 고정 슬롯, 법률별 고정 건설력 할당, 국가별 무상 건설력은 기본모형에 없다.
- `ProjectProposal → Authority/Permit → Financing/Contract → Reservation → ActualWork → Inspection → Commissioning` 순서를 분리한다. 경기예측·투자명령은 물리적 건설 실적과 다르다.
- 공적 예산 승인이나 투자기금의 장부는 지급 가능 잔액과 구분한다. 금융원장에서 실제 인출/대출 실행/예약이 확인돼야 동일 자금의 이중 지출을 막을 수 있다.
- 공사 금융·부도·보험·공익사업·사업승계·국유화·점령지 복구는 기존 E3/E4/P/M 분야에서 경제/법적 사건으로 기록한다.

## 4. 산업 공동 규약 (D-I1~I5의 상한 경계)

1. 산업은 `IndustryClassId`의 분류체계(시대/지역에 따라 확장 가능한 다단계 taxonomy)이고, 실제 생산/고용/이익은 C-E1의 `Firm/ProductionSite/ProductionRecipe`와 각 원장에서 발생한다. 하나의 기업은 여러 업종의 사업장을 운영할 수 있고, 하나의 사업장은 규칙상 복수 생산 활동을 수행할 수 있다.
2. 산업분류와 실제 생산공정은 다르다. `TechniqueDefinition` 및 레시피는 input_goods, output_goods, byproducts/waste, workforce/qualification, capital_services, energy, resource_rights, physical_capacity, quality, environmental/legal constraints를 버전 있게 연결한다.
3. 원작의 고정된 생산방식 슬롯·소유권 생산방식을 복제하지 않는다. VIC2.5의 생산방식은 **현장별 실제 장비·숙련·투입·권한**에 맞는 호환 가능한 공정 구성이다. 자본가 지분은 `OwnershipLedger`가 소유하고 공정 선택이 지분을 자동 변경하지 않는다.
4. 설비 능력은 실제 `CapitalAsset`의 가동 가능용량·상태·유지보수·전력/연료 공급과 노동·재료를 함께 만족한 경우에만 산출된다. 설비 가동률·병목·기업 손익은 기존 C-E 계산 규약을 따른다.
5. 기술적 대체/기계화/공정변경에는 자본재·이전비용·노동 재배치·재훈련·설비 적합성·법률/지식 접근이 필요할 수 있다. 더 새 기술도 원료 조달·노동비용에 따라 경제적으로 불리할 수 있으며 무조건 단계 상승/상시 상위 보너스로 취급하지 않는다.
6. 광물 매장량·경작가능지·재생가능 자원은 `ResourceStock/HarvestYield` 근거 없이 상품을 창출하지 않는다. 지하/미채취 자원과 `InventoryLedger`의 채굴/수확 이후 재고는 구분한다.
7. 산업집적·입지효과·운송병목·산업정책은 실제 공장 위치, 유효 운송 서비스, 시장, 숙련 인구, 규제, 공공 인프라에서 도출한다. 국가 전체 무제한 즉시 이동·추상 전국 생산 보너스는 기본 가정이 아니다.
8. 역사적 가내수공업·소농·부역·조합·국영 사업·민간 법인·대기업은 기존 `OwnAccountActivity` 및 Firm/법률/소유 관계의 변형으로 지원한다. 특정 시대에 현대적 법인/은행/기계화를 의무화하지 않는다.

## 5. 기술 공동 규약 (D-T1~T5의 상한 경계)

### 5.1 지식과 기술 채택 분리

- `TechnologyDefinition`: tech_id, domain_tags(산업/군사/사회 등 다중 태그), prerequisites(지식/실험/기반), empirical_requirements, discoverability/transfer_rules, valid_historical_metadata. 방향 비순환 선행관계가 필요하면 해당 `ruleset`이 명시하고, 모든 기술을 고정된 3개 트리·5개 시대로 제한하지 않는다.
- `KnowledgeState`: holder_id(`Country`/`Institution`/`Firm`/연구 조직 등 유효 주체), tech_id, status(unknown/observed/understood/usable), research_progress, provenance, effective_month. 연구 진척은 기술 적용의 상시 성능 보너스나 판매 가능한 상품 재고가 아니다.
- `KnowledgeHolder`의 '국가 전체 기술 보유'는 기관/기업·정부가 갖는 지식 및 공개 접근권을 집계한 뷰다. 공유/비공개/기술이전·특허/독점권/면허를 분리하며, 단일 국가 전체에 모든 지식이 자동·즉시 확산되지는 않는다.
- `SiteTechniqueAssignment`: site_id, technique_id, status(proposed/training/retrofit_ready/installed/active/obsolete), effective_period, permit/license_refs, required_capital_refs, workforce_training_refs, source_event_id. **지식 보유 ≠ 기술 도입 ≠ 충분한 숙련 ≠ 실제 생산량**이다.
- 지식·기술 목록, 인적 숙련, 설비 소유, 법적 독점권/실시권은 각각 다른 정본이다. `KnowledgeRegistry`가 사람 수·학력이나 특허 소유권을 중복 보유하지 않는다.

### 5.2 연구·발명·확산

1. 연구주체는 기존 법인·정부기관·연구 현장(`ProductionSite`)을 사용하고 실제 `EmploymentLedger`의 적격 연구인력, 장비·시설·연구비와 조달된 중간재를 사용한다. 연구소 설치만으로 무료 혁신점수 생성 금지.
2. `ResearchProject`는 목표/탐색영역, 수행주체, 연구장소, 전제지식, 지불 가능 예산, 연구 공정, 누적 검증된 연구성과, 발표/공개 상태를 기록한다.
3. 실제 연구 생산은 stage4의 연구서비스/`ResearchWorkEvent`로 기록하고, stage9a에 기술적 조건/자금·기록을 검증해 `KnowledgeEvent`를 예약한다. 새로운 지식의 다른 공정 효력은 **다음 달 stage0 이후**다. 연구 결과와 즉시 산업 생산을 동일 월에 소급 연결하지 않는다.
4. 기술 이전·교육·무역/인적 왕래·설비 수입·역공학/모방은 별개의 `KnowledgeTransferEvent`로 다룬다. 지식 접근성이 있는 전파 경로와 수용 주체의 역량·규칙/권한이 필요하다. 모든 미연구 기술이 전세계에 무료·무조건 보급되는 규칙은 두지 않는다.
5. 연구 실패, 부분 성공, 누적·공동 연구, 비밀 연구, 기술 유실/단절, 재발견, 병행 발명 등은 기술별/시대별 정책 및 실제 사건으로 지원할 수 있어야 한다. 확률적 사건은 시드·사건 ID·수치정책으로 결정론적 재생 가능해야 한다.
6. 기술을 실제 사업장에 적용하려면 지식 접근/유효 면허, 관련 법률, 설비/작업자 숙련, 공정 변경의 비용·기한/설치가 충족되어야 한다. `SiteTechniqueAssignment`의 `active`는 유효 시설 상태로부터 산출되며 다른 사이트에 자동 확산되지 않는다.
7. 자금·교육·학습·연구 인프라가 기술 확산 속도에 영향을 줄 수 있지만, 효과는 규칙 파일의 명시적 경로와 입력 변수로 표현한다. 기술 연구 완료 시 전 POP의 문해율·숙련도·정치 태도를 직접 덮어쓰지 않는다.
8. 기술과 특허 권리의 소유·사용료는 C-P 법률 및 `OwnershipLedger/FinancialLedger`, 외국 기술이전은 C-E5 운송·무역/계약·국제결제와 연결한다. 특허제도나 보편적 연구기관이 존재하지 않는 역사시대도 허용한다.

## 6. 기존 월별 12단계에 끼워 넣는 실행 경계

**공개 stage 수·순서·이름을 절대로 추가/변경하지 않는다.** 기존 D4 `9a~9e`·`10a~10f` 및 분야별 확정 결과의 조회 시점 우선.

| Stage | 건설 | 산업 | 기술 |
|---|---|---|---|
| 0 `opening_snapshot` | 전월 완료·검수된 프로젝트의 준공/가동/운송 간선 유효화, 미도착자재 제외 | 가동 설비/시설 상태·재고·가격 freeze | 전월 확정 지식·현장 도입·라이선스 효력 반영 |
| 1 `planning_and_credit` | 발주·시공·공정 계획, 허가/재원·장비/재고·운송 예약 | 생산/투자·원료 계획 및 합법적 조달 재원 | 연구 목표·연구비/인력/장비 계획 |
| 2 `labor_clearance` | 시공인력 중복 없는 계약·적격 배정 | 제조/농업/광업·연구와 노동 경쟁 | 실제 연구·교육 관련 고용 배정 |
| 3 `intermediate_goods` | 당월 공사용 중간재 실인도/예약 해제 | 제조·정비용 실제 재료·에너지 실인도 | 실험 재료·현장 가동용 실제 투입 확보 |
| 4 `production` | 실제 시공 서비스·자재 소모·공정량/품질 기록 | 실제 생산·재고 생성/소모, 설비 마모 | 실제 연구·훈련 서비스/실적 기록 |
| 5 `wages_and_transfers` | 시공·감리 등 임금/보조금 실지급 | 노동자 임금/납세 원천징수 | 연구원 임금·유효 공공 지원 지급 |
| 6 `pop_consumption_plans` | 가구 소득 결과 반영, 직접 공사비 소급 금지 | 임금/상품에 따른 실효 소비 수요 | 연구 지식만으로 가구 현금 창출 금지 |
| 7 `final_goods_market` | 시설 설치용 투자재·최종재 인도 및 조달; 당월 stage4 공정에 소급 투입 불가 | 완제품/설비상품 판매·수송/소비 | 라이선스/서비스의 실제 계약·지급/인도, 지식 자체는 재고 아님 |
| 8 `closing_accounts` | 공사비/WIP·기성청구·채권·채무·감가/손상 결산 | 기업 원가·재고자산·매출/손익/이자 | 연구비·사용료·성과보상 회계 |
| 9 `saving_and_investment` | `9a` 차월 자금계획/건설계획·확장·보수 신청 | `9a` 차월 신규 투자/구조조정 예약 | `9a` 검증 연구성과/지식사건 및 차월 도입계획. `9d` 새 법률은 차월 효력 |
| 10 `pop_demography_and_health` | 전쟁·재난/사고로 인한 현장·미준공시설 손상/인력 사고 반영 | 전투·재난으로 설비/생산시설 손상, 인구 건강·복무 원장 연결 | 연구 인력의 실제 부상/사망/이주·교육 전이; 앞 단계 연구 실적 소급 취소 금지 |
| 11 `repricing_and_validation` | 공정/재고/WIP/소유·준공 대기/차월 상태 검증 | 생산·설비·시장/재무·자원 보존/차월 가격 검증 | 지식/라이선스·연구 이벤트·현장채택/규칙버전 대조 후 전체 월 ONE commit |

- stage4에 진행량이 완공되어도 같은 달 stage4 일반 공장의 신규 생산능력으로 사용하지 않으며, stage7에 구매한 물품은 이미 종료된 stage3/4에 사용 불가. 도착이 미래인 자재는 E5 운송원장의 `in_transit`로 남는다.
- stage9a에서 제안한 공장 개축·새 공정·기술 개량은 차월부터 적법한 공사/훈련/채택 절차를 거친다. stage9d의 새 법률은 같은 달의 9a/9c/0~8 결과에 적용되지 않는다.
- stage10 인명피해는 기존 POP-D2/C-M 사상 처리로만 발생하며, 현장 사고 때문에 건설 분야가 독립 인구 원본에 사망자를 쓰지 않는다.
- 실패 시 `StagingWorld` 전체 Abort. 건설 공정/지식 연구만 완료된 부분 커밋, 다른 원장보다 앞선 별도 월말 커밋은 허용하지 않는다.

## 7. 교차분야 의존과 영향 경로

- **건설 → 산업:** 확인된 신설·증축·보수 자산의 가용 능력 변경(차월) → C-E1 생산·노동·자재·시장 가격/투자에 반영.
- **산업 → 건설:** 철강·목재·기계 등 생산과 재고/가격·운송 서비스 → 건설업자의 실효 중간재 공급/공사비·기간 상한. 자재 부족/인플레이션 시 실제 지연.
- **기술 → 산업/건설:** 지식 확보·공정 승인·기술 도입/교육/설비 개축 → 현장별 생산 레시피와 실제 건설 효율·요구자재 변경. 기존 공정은 규칙상 병존 가능.
- **산업/건설 → 기술:** 실제 연구 시설·장비·숙련/교육·연구비와 산업 현장 피드백이 연구 가능한 범위/성과에 영향. 공장 규모가 연구 완료를 자동 보장하지 않음.
- **정치/재정 → D:** 인허가/계약·노동·교육·특허·공공투자/보조금/자유무역의 유효 권한을 확인; 정책 발표=실제 공사/재료/연구 효과 아님.
- **군사/전쟁 → D:** 군수공장/기지/요새의 생산·건설을 C-M 보급 및 실제 자산/전투손실과 연결. 점령은 시설의 주권/소유를 자동 이전하지 않음. 복구에 실제 재료/인력/금융 필요.
- **지역/시장/교통 → D:** 프로젝트와 사업장의 `RegionId`는 지도 없이 유효해야 하고, 지리·수로·철도·항만은 C-E5 실제 간선 및 수송용량 계약으로 연결한다. 기술 이전 접촉도 무조건 글로벌 효과로 처리하지 않는다.

## 8. 정합성 불변조건 (DD0-INV)

| ID | 계약 불변조건 |
|---|---|
| DD0-INV-01 | 인구 원본은 `PopulationSlice.count` 하나; 시공·연구·공장 인원 합은 유효 실제 할당/조회이며 중복 창출 금지 |
| DD0-INV-02 | 실제 노동·설비/서비스 자원과 예약된 자금/재고/운송용량의 사용 총합은 각 원본의 가용량 초과 금지 |
| DD0-INV-03 | 건설 중간재는 `opening_stock+actual_arrival+realized_purchases−uses−dispatch−loss=closing_stock` 검산에 포함 |
| DD0-INV-04 | `done_work <= required_work`, 완료공정 선행조건/실제 투입 증빙 존재, 무단 진척·음수·오버플로 금지 |
| DD0-INV-05 | 실제 건설/연구비·임금·상품 결제는 통화별 균형 분개·정당한 채권/채무와 일치 |
| DD0-INV-06 | `WIP` 비용 계정, 인도/설치된 실물·`CapitalAsset`, `OwnershipLedger`가 이중 재고/중복 자본/이중 소유권을 만들지 않음 |
| DD0-INV-07 | 신규/증축·개축 자산과 운송간선 가동은 검수·효력월 이전 생산/수송능력을 소급 증가시키지 않음 |
| DD0-INV-08 | 자연자원 매장·재생/채취 사건 없이 원료 생산을 허용하지 않음; 미채취 자원 ≠ 상품 재고 |
| DD0-INV-09 | 지식 확보는 정당한 연구/이전/역사 초기상태 출처를 가지며 같은 지식을 타 보유자에게 무근거 복제하지 않음 |
| DD0-INV-10 | 사업장 `active` 생산공정은 유효 지식접근·법/허가·설비·훈련·선행조건을 모두 만족해야 함 |
| DD0-INV-11 | 계획/승인/기술발견/건축허가는 실제 자금 지급·재고·시설/생산량/기술보급·시민 숙련의 즉시 추가와 구별 |
| DD0-INV-12 | D 분야 사건도 C-D0/D4의 source_event_id 멱등성·revision/시점·전월 replay·월간 원자성을 준수 |
| DD0-INV-13 | 기존 정치·군사·영토 관할 및 지분 관계와 모순되는 공사/지식 이전/시설 소유 확정은 거부 |
| DD0-INV-14 | 확정된 월말 WorldSnapshot과 해당 ruleset/seed/정렬 정책이 같으면 직렬/허용 병렬 계산의 결과 해시 동일 |

기본 hard error: `DD0_ERR_WORK_OVERALLOC`, `DD0_ERR_CONSTRUCTION_INPUT`, `DD0_ERR_FINANCE_RESERVATION`, `DD0_ERR_COMMISSION_EARLY`, `DD0_ERR_ASSET_DOUBLE_COUNT`, `DD0_ERR_RESEARCH_PROVENANCE`, `DD0_ERR_TECH_UNAVAILABLE`, `DD0_ERR_AUTHORITY`, `DD0_ERR_TIME_TRAVEL`, `DD0_ERR_CONTRACT_CONFLICT`. 해당 원인에서 기존 D4 오류코드로 매핑하는 표는 구현 시 유지한다. 정상적인 공사 지연·연구 실패·기술 보급 지체·경기침체·기업도산은 hard error가 아니다.

## 9. 기록·저장·성능·검증 계약

- 기존 save_header의 `pop_contract_version`/`cross_domain_contract_version`/`ruleset_version` 등은 유지하고, Phase D 추가 `phase_d_contract_version=phase-d-common/1.0` 및 기술/공법/건설 레시피 schema/hash·numeric policy로 확장한다.
- 저널: `World/Month/Stage/Subphase/EntityIds/JournalBatchId/SourceEventId/DependsOn/ExpectedRevision/EffectiveMonth/RulesetHash`를 저장. 지식/건설 투입 사건을 중복 replay하면 거부한다.
- 분할/합병·회사 청산·정부/국경 변경 시 진행중 프로젝트, 시설·지식보유, 소유권·채무·인허가·연구팀의 유효 참조는 이전/승계 사건 또는 종료 사건으로 처리한다.
- 최소 통합 fixture: 3개 Region, 2개 Government, 건설 법인/광산/제철소/연구 현장, 자가 건설 가구, 금융기관, 시공 중인 시설과 장거리 운송 중인 공사용품, 복수 기술/숙련 상태를 포함한다.
- 인수 시험은 [`D-D0_ACCEPTANCE_TESTS.md`](D-D0_ACCEPTANCE_TESTS.md)의 `DD0-G01~G24`를 따르고 기존 POP/CE/CP/CM/D4 검증을 추가로 실행한다. 장기 1,200개월, 10만/100만 희소 인구셀 대표부하·월별 메모리/CPU, 단일/복수 worker 재현성과 저장·복구는 후속 구현 결과를 로그로 검증한다.
- **설계 확정은 구현·실측·회귀시험 완료가 아니다.** 24개 시험은 현재 실행 목록/합격 기준만 결정되어 있으며 PASS 판정이 없다.

## 10. 확정 ADR

| 결정 ID | 결정 내용 |
|---|---|
| DD0-ADR-001 | POP-D0~D3, C-D0/C-D4, C-E/P/M 원본과 월별 12단계에 상위 호환 |
| DD0-ADR-002 | 기존 Firm·ProductionSite·CapitalAsset/레시피를 재사용하며 D-영역 복제 원본 금지 |
| DD0-ADR-003 | 건설을 노동·자재·설비·금융에 기반한 실제 시공 서비스/공정으로 모델링 |
| DD0-ADR-004 | 공공/민간/자가 건설은 발주·시공·금융·소유/운영 관계로 분리, 고정 건설력 배당 없음 |
| DD0-ADR-005 | 공사 진행 실적과 금융 WIP 및 기계 상품재고·설치자산을 구별 |
| DD0-ADR-006 | 공사 완료·검수·차월 설치/가동/인프라 효력 분리 |
| DD0-ADR-007 | 산업은 분류 객체이며 생산/실물/고용은 C-E 원장을 유지 |
| DD0-ADR-008 | 생산방식은 독립 공정 조합·설비/자원/지식·숙련의 실제 제약으로 결정 |
| DD0-ADR-009 | 자영·소농·가내수공업·공영/민영/협동·부역 건설/산업 모두 표현 |
| DD0-ADR-010 | 자연자원 매장/회복과 채취된 상품 재고는 분리 |
| DD0-ADR-011 | 지식 발견·확보, 공개/이전, 기술 실용화/현장 채택, 숙련 정착을 분리 |
| DD0-ADR-012 | 연구기관·연구인력·재료·재정·검증된 연구활동 없이는 무료 연구성과 없음 |
| DD0-ADR-013 | 해당 월 연구결과/공법변경을 종료한 생산·공사로 소급 적용 금지 |
| DD0-ADR-014 | 기술 확산은 접촉·교육/거래·권리/행정·수용 역량에 근거, 전세계 자동 공짜 확산 금지 |
| DD0-ADR-015 | 생산·산업 기술과 민군/사회 지식을 단일 선형 세대/고정 3계열/국가 전역 보너스로 환원하지 않음 |
| DD0-ADR-016 | 기존 E5 운송/시장 및 P/M/관할을 D의 실제 권한·물리 병목 입력으로 활용 |
| DD0-ADR-017 | 단일 StagingWorld, 멱등 사건, 대조 불변식과 전원장 롤백 |
| DD0-ADR-018 | 24개 상세 수락 게이트·장기/결정론 시험은 완료 전까지 미실행으로 명시 |
| DD0-ADR-019 | 수치/역사별 계수는 외부 ruleset과 출처/스케일/버전 관리로 분리 |
| DD0-ADR-020 | 상세 D-C/D-I/D-T는 공통 계약을 변경하려면 ADR+버전·마이그레이션 필요 |

## 11. 후속 설계의 고정 책임

- D-C1~C5: `ConstructionProject/WorkPackage` 상세 스키마, 시공 노동/장비·공정 레시피, 공사 계약·분납/WIP/감리·허가, 준공/설비 설치·노후·철거·복구 계산.
- D-I1~I5: 업종 분류, 원료/자원·에너지·공정조합, 현장 가동률·설비·투입대체, 산업 입지·집적/정책, 구조전환·기업 생멸과 산업화.
- D-T1~T5: 지식 그래프, 연구/혁신·실험/우발적 발명, 이전·확산/모방·국제 접근성, 실용화·교육/현장채택, 역사·사회·경제·군사 연동.
- D-V: `DD0-INV` 14개 + `DD0-G` 24개, 기존 D4 회귀, 반례·예상 오류코드/전체 롤백, 직렬/병렬 동일 결과, snapshot/save/replay, 100년·규모별 계측.

**계약 판정:** `D-D0 DESIGN CONFIRMED`. 신규 C++ 엔진/24개 검증 실행 및 성능 합격 판정은 **NOT_RUN**.
