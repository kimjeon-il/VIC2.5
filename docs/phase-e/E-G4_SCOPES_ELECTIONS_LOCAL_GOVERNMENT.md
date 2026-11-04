# E-G4 — 목적별 관할·선거구·지방자치 확장·지역 집계

> SPECIFIED / NOT IMPLEMENTED · E-D0 phase-e-common/1.0 / C-P4 ElectoralDistrict와 C-D0 원본 일원화 유지.

## 1. 관할의 층위

Province는 고정 물리적 주소, AdministrativeUnit은 국별 basic scheme 편제, TerritorialScope는 목적별 공간 범위. 정부(Government), 지방정부 기관, 선거 이벤트, 시장, 지분·채무는 각각 기존 분야의 원본. 기본 행정구역의 경계와 하원 선거구·지방의회 선거구·세무서 관할·군관구 등은 일치할 필요 없음.

## 2. Scope 정의와 관계

TerritorialScope {id, kind:Electoral|LocalGovernment|Fiscal|Judicial|MilitaryDistrict|Special, owner_or_authority_typed_id, parent_scope_id?, legal_basis_id, effective_period, membership_mode, coverage_policy, exclusivity_policy, revision}.
ScopeAssignment {scope_id, province_id, valid_period, source_event_id, revision}. 기본은 whole Province membership; 같은 scope/Province/기간 중복 레코드 금지.
MembershipMode:
- ExplicitProvinceSet: 법률 또는 명령으로 선거구·군관구·특례지역의 Province membership을 독립 고정.
- DerivedFromAdminUnit: 특정 AdminUnit의 현재 Province closure에서 유효시점에 scope를 도출 (지방정부 관할 등), 원본은 참조 하나뿐; duplicate writable Province list 금지. 행정개혁의 scope 효과는 stage0 전 검증.
- SelectiveLegalOverlay: 명시된 예외 Province set을 additive/subtractive operation으로 조합; source refs와 priority 필요. 임의 JSON query로 월마다 전체 주권 공간 스캔은 피함.

scope는 법률상 자치권 보유자/기구와 1:1일 수도 1:n/n:1일 수도 있음. Scope의 생성만으로 지방정부·의회·세금 계좌가 자동 생성되지는 않음.

## 3. 범위·배타성

CoverageRule: CompleteWithinJurisdiction / PartialAllowed. ExclusivityRule: OnePerPartition / OverlapAllowed. 같은 목적이라도 contest_id, house_id, level, fiscal_tax_kind 등 partition_key별로 배타성을 검사.
Electoral Contest에서 한 유권자는 주어진 투표/의석 구분의 valid district assignment에 1회만 포함. 서로 다른 대통령·하원·지방 선거에서 같은 사람이 각각 투표하는 것은 정상. C-P4 VoterEligibilityView/VoterRegister/BallotEvent가 사람·투표수·의석의 유일한 계산 원본.
고정 Province를 기하적으로 분할하지 않기 때문에 기본 GeographicDistrict는 Province 완전 집합. Province 내부 여러 선거구는 현 E-G4 지리 raster로 정확한 경계를 표시할 수 없음. C-P4가 허용한 PopulationSlice allocation_partition_id를 통해 인구수 분할을 명시/검증하여 정치학적 선거 계산을 하는 것은 별도 future extension; 인구분할을 공간 경계처럼 표시하거나 근거 없는 비율로 임의 배분 금지.

## 4. 지방정부 인터페이스

LocalGovernmentAuthority { government/institution id from C-P, scope_id:E-G4, policy_power_grants from C-P, ledger_account_refs from C-E4, delegation/legal basis, effective period }는 교차 분야 관계. E-G4는 관할 위치만 원본이며 자치권/조세부과권/선출방식/예산/채무/정치성향을 중복 저장하지 않는다.
시·도·군 기본 행정구역이 있어도 지방자치 정부 없는 중앙임명 관리 가능. 입법권·재정권 없는 행정출장소도 별도.
연방 구성주 권한은 C-P의 헌법·법률/정부형태, 영토 scope는 E의 공간, constituent state 자체 법적 지위/하위·속국 관계는 국가/외교 E-G2/H에 연결.

## 5. 선거구/특별관할 변경

CreateScope, AssignProvinceToScope, RemoveScopeAssignment, RebaseScopeToAdmin, ChangeScopeRule, RetireScope. 선거법 개정 후 contest-boundary-revision으로 elections snapshot freeze; 진행/확정 선거의 district boundaries와 유권자 적격성·등록/투표를 소급 변경하지 않음.
한 Province의 administrative leaf가 변경되어도 Explicit electoral district 그대로 유지. DerivedFromAdmin의 범위는 새 admin snapshot effective month에 따라 이동하되, 이전 선거/청구/채무 원본 변경 금지. scope 변경이 정부 관할·세수·부채 승계 필요시 C-P/E의 explicit event batch가 동반.

## 6. 통계 집계

GetScopePopulation(scope,snapshot), GetEligibleVoters(contest,district,snapshot), GetScopeSites, GetScopeEmployment, GetScopeTaxableBase, GetScopeOutput, GetScopeInstitutionCapacity 등은 권한이 있는 C/B/D 원본 원장을 조회하는 읽기 전용 계층. 각각 source_registry_revision+scope_membership_revision+ruleset_version+effective_month+snapshot_id 키 사용.
가산 가능: headcount, real output, firm/site counts (site ID 유일성), inventory by holder/location, physical area once. 단순 합산 금지: 실업률, 평균임금, 지지율, 물가수준, 주민의 재산(중복소유), 다중 scope 합계. 비율은 sum numerator/sum denominator, 비가산 값은 각 도메인 메트릭의 정확한 집계식.
하나의 Province가 서로 다른 overlapping scopes에서 각자 포함되는 것은 정상이나 겹친 scopes의 총계를 하나의 unique national total로 더하면 안 됨. 필요하면 union ProvinceIds로 다시 집계.

## 7. 정치·경제·군사 API 분리

E에서의 GetScopes(province,kind), ResolveScopeMembers(scope,month), GetJurisdictionAndAdministrativeContext(province,month). C-P electoral formula, candidate/voter eligibility, local elections, law/authority; C-E fiscal budget/bonds; C-M mobilization/military districts; F transport assignment; H treaty/sovereignty 외교는 소유하지 않음.

## 8. 인수 시나리오

G4-A01 같은 12 Province를 A/B주(세로)와 3개 national electoral districts(가로)로 분리; 주 변경과 electoral explicit 소속 독립.
G4-A02 같은 하원 contest에서 투표자 중복 district assignment reject, 전국 적격 합 불변.
G4-A03 하원/지방의회/국가원수 각각 다른 scopes 중첩은 적법, 같은 contest 중복만 거부.
G4-A04 derived local scope follows legal admin change effective m+1; explicit parliamentary district unchanged.
G4-A05 local scope를 생성해도 money/tax/debt/institution 자동 생성 0.
G4-A06 평균임금/실업률 분자·분모 집계, 비율평균 금지.
G4-A07 주민·시설의 프로빈스 주소는 scope 편제로 변경되지 않음.
G4-A08 선거 snapshot 법/지리 revision freeze, 개표 이후 임의 재획정 소급 거부.
G4-A09 overlapping scope union 집계의 중복계수 방지.
G4-A10 save/replay + admin membership change before scope derived cross consistency.
