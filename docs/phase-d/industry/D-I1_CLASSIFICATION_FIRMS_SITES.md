# VIC2.5 D-I1 — 산업분류·기업·사업장·생산활동

> **설계 확정 / C++ 구현·자동시험 미실행** · \`phase-d-industry/1.0\` · 2026-11-04  
> 상위: [D-D0](../D-D0_COMMON_CONTRACT.md), [C-E1](../../phase-c/economy/C-E1_FIRMS_LABOR_PRODUCTION.md), POP-D0~D3, C-P1~P5, D-C1~C5. [산업 색인](README.md)

## I1-1. 개념 경계

**산업(IndustryClass)은 경제활동의 분류, 기업(Firm)은 경제·법적 계약주체, 사업장(ProductionSite)은 특정 지역에서 생산하는 운영단위, 활동(SiteActivity)은 특정 사업장에서 실행하는 공정·생산의 할당 관계**이다. 상품(Good)은 거래되는 산출물, 레시피(ProductionRecipe)는 기술적 실물 전환, 기술(Technique/Knowledge)은 공정을 사용할 수 있는 조건이다. 같은 개념으로 합치거나 독립된 생산·자산/노동·현금 원본을 추가하지 않는다.

- \`IndustryCatalog\`: 분류 구조와 코드·유효기간·범위의 **버전 있는 규칙 데이터**. 특정 산업 분류 노드를 생성해도 경제주체·공장·생산능력이 생기지 않는다.
- \`SiteActivityAssignment\`: \`site_id, activity_id, industry_class_id, recipe_id, technique_assignment_ref?, effective_period, site_operating_window, revision\` 의 **관계**. 매출·생산실적/인원·재고는 C-E 원장에서 파생한다.
- \`IndustryObservationView\`: 산업/지역/기업 규모/사업장별 생산·중간소비·부가가치·고용·투자·수출입/부도의 시간별 조회 캐시; \`revision\` 불일치 시 무효화.
- 기업 하나가 복수 사업장을, 사업장 하나가 복수 활동을 가질 수 있으나 **활동별 실물·노무·설비 사용은 공통 CapacityReservationTable**에 묶어야 하며 같은 로트를 여러 활동의 산출로 세지 않는다.

## I1-2. 산업 분류 방식

권장 상위 13분류(고정된 업종 상한이 아니라 시작 taxonomy):

| 코드 예시 | 상위 분류 | 활동 예시·중요 구분 |
|---|---|---|
| \`AGRI\` | 농경·원예 | 작물/원예, 관개·농지·수확·자가소비, 단순 토지 보유와 다름 |
| \`LIVE\` | 축산·목축 | 사육·도축·유제품 원료, 사료/개체·번식·분뇨/질병 |
| \`FORE\` | 임업·벌목 | 벌채/조림·지속가능 생장·목재가공과 구분 |
| \`FISH\` | 어업·양식 | 수역 자원, 선박/장비, 야생 어획과 양식 구분 |
| \`EXTR\` | 광업·채굴 | 광석·석탄·석재/염, 미채굴 자원과 상품 재고 분리 |
| \`MANU\` | 제조·가공 | 식품·직물·목재·제철·화학·기계·군수품 등 하위 트리 |
| \`ENER\` | 연료전환·전력·열 | 1차 연료/비저장 에너지 서비스와 저장 가능 연료 분리 |
| \`UTIL\` | 상하수·폐기물·환경 | 물 공급/위생·폐기물 회수·환경정화 서비스 |
| \`CONS\` | 건설·수리/토목 | D-C1~C5의 실공정/검수·시설 형성, 공사 중간투입 중복 합산 금지 |
| \`LOGI\` | 운송·창고·통신 | C-E5 유효 노드·간선/수송 서비스와 관련 서비스 생산 |
| \`TRAD\` | 도소매·중개·숙박 | 상품 소유권 이전/유통 마진·서비스 실제 제공 구분 |
| \`FINA\` | 금융·보험·부동산 | E3 실금융·서비스/부동산 이용서비스, 금융자산 거래액을 GDP 산출로 간주하지 않음 |
| \`SERV\` | 전문·교육·의료·행정/기타 서비스 | 실제 시설·고용/비저장 서비스·공적 서비스; 정부 지출=자동 서비스 아님 |

시대별 길드·가내수공업·수공업 조합·동인도형 상업회사·장원/소농·공장제·현대 다국적 기업·국영기업/협동조합은 **법형태와 규모/소유·노동구조의 표현**이며 업종 자체가 아니다. 동일 활동을 다른 법·사회체제로 생산할 수 있어야 한다.

- \`IndustryClassId\`는 stable typed ID, \`parent_id\`, \`canonical_code\`, \`historical_labels[]\`, \`valid_period\`, \`compatible_recipe_tags[]\`, \`source_provenance\`, \`version\`를 가진다. 상하위 관계는 순환 불가.
- 동일 산업코드의 시대별 재정의/통계 재분류는 \`IndustryCrosswalk(from_version,to_version,weighted_class_mappings,provenance)\`로 **조회/통계 변환**한다. \`IndustryClassId\`의 기존 사건을 소급 재태깅해 생산량/임금을 다시 쓰지 않는다.
- 국제통계 대조는 UN ISIC Rev.5(2024 채택)를 통계적 참고로 사용하되, 1801년 등 과거의 경제주체가 현대 ISIC 표준을 직접 적용했다고 가정하지 않는다. 엄격한 1:1 대응이 불가능하면 \`unmapped/estimated\`와 근거를 보존. ISIC 코드·텍스트 전체를 복제하지 않는다.
- 산업 분류와 상품 분류(C-E2 \`GoodId\`)를 구분. 같은 원료/상품은 여러 산업에서 생산될 수 있고, 한 산업은 여러 품목 및 부산물을 생산할 수 있다.

## I1-3. 사업장·활동/주사업 분류

\`Firm\`는 법적 형태, 본거지, 소유주·계좌·투자/채무 원본을 C-E1/E3에 보유한다. \`ProductionSite\`는 운영자·지역·설치 \`CapitalAsset\`·산출 활동을 C-E1에 보유한다.

- 복수 생산활동이 같은 사이트·자본·노동을 사용하면 \`ActivityAllocation\`은 **해당 월 공정 사용/성과의 참조 관계**이지 두 번째 생산/고용 레지스트리가 아니다. 한 단위 실물 노동·장비/시간·재료를 활동 A와 B에 이중 배정 금지.
- 기업의 \`primary_industry_class_id\`는 해당 기간 지배적인 부가가치(값 유효 시), 불가능하면 검증된 매출/고용 대리변수 + provenance를 포함하는 **파생 조회값**; 회사법상 업종·허가의 법적 등록은 C-P와 분리. 회사가 제조·유통을 둘 다 하면 GDP/고용을 한 부문에 통째로 몰아 넣지 않고 실제 활동별 기록에서 배분.
- 판매 자회사/타 지역 사업장/위탁·합자·임대 공장은 법인 경계에 따라 소유/운영을 구분. 동일 시공·제조 \`ProductionSite\` 시설을 타 소유자에게 위탁했어도 \`OwnershipLedger\` 권리를 복사하지 않는다.
- 전통 자영업·생계생산은 C-E1 \`OwnAccountActivity\`에 활동/산업 라벨을 붙인다. 현물 생산/소비는 \`InventoryLedger\`로 기록하고 판매·현금 임금/이익으로 임의 환산하여 계좌에 가산하지 않는다.
- 정부·군사·민간 및 비영리 공급은 실제 생산주체/법적·회계처리 경계를 명시. 사회·공공 생산 서비스는 E4의 고용·실제 시설/투입·서비스 슬롯을 필요로 한다.

## I1-4. 산업별 조회와 소유/노동 통계

- 업종별 월간 \`gross_output\`, \`intermediate_consumption\`, \`gross_value_added\`, \`employee_compensation\`, \`capital_formation\`, \`imports/exports\`, \`capacity_utilization\`, \`unmet_orders\`, \`firm_births/exits\`를 유효 통화/가격/물량/월 기준으로 관측.
- 기업 차원의 재무상태표(자산/채무/자본)는 산업이 중복되어도 **하나**. 기업 총고용은 \`EmploymentLedger\` 실제 계약을 한 번만 집계하고, 산업별 고용은 유효한 활동/현장 배정 합계가 총고용과 조정 가능한 범위여야 한다.
- 계열사 내부 서비스/상품 거래도 실제 거래/원장 사건일 때만 인식; 세계 GDP 합계는 SNA 생산/중간소비 원칙에 맞춰 이중계상하지 않는다. 국가·시장 경계가 변경돼도 과거 생산/소유·인구 사건이 지워지지 않는다.
- 대표 사업장은 생산통계/산업 분류를 위한 집계 단위이고, 전체 기업의 복수사업장 재무를 덧셈할 때 내부거래 상계 정책을 명시.

## I1-5. 고정 경계 및 위임 사항

- 산업의 다층 분류/통계·\`SiteActivityAssignment\` 관계만 D-I1에서 소유한다. 산출/재료·자원/전력 제약(D-I2), 기계화·레시피 전환(D-I3), 지역산업/집적(D-I4), 산업정책·생멸(D-I5)로 상세 계산을 위임한다.
- 산업 도메인 고유 사건/카탈로그는 \`source_event_id, revision, valid_from_month, valid_until_month, ruleset_hash\`를 보존. 분류 개정에 따른 원장 재기록 금지.
- C-E1의 \`ProductionSite.activity_id/recipe_id\` 및 복수활동 확장을 위해 **읽기 호환 어댑터**를 먼저 정의한다. 단일활동 레거시 세이브를 새 다중활동 원본과 동시에 운영하지 않고 스키마 버전 전환/검증.
- 수치·산업군/분류 목록은 보정 가능 규칙 데이터이며 위 13개는 영구적 종수 제한이 아니다.

**인수시험:** \`DI-T01..T09\` (산업분류·사업장·통계). [검증 매트릭스](INDUSTRY_ACCEPTANCE_TESTS.md).

## 참고

- [UNSD ISIC Revision 5 classification](https://unstats.un.org/unsd/classifications/Family/Detail/2095) — 2024 채택 산업활동 통계 분류/계층의 개념 참고.
- [UNSD System of National Accounts 2025](https://unstats.un.org/unsd/Nationalaccount/sna.asp) — 생산·산업별 부가가치·중간소비/자본 형성의 계정 일치.
- [Victoria 3 Production Methods 개발자 설명](https://store.steampowered.com/news/posts/?enddate=1624550727&feed=steam_community_announcements) — 투입·산출·고용/자동화의 문제영역 참고. **원작 슬롯·수치/코드는 복제하지 않음**.
