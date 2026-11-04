# E-G3 — 동적 행정구역·행정계층·프로빈스 재편

> SPECIFIED / NOT IMPLEMENTED · E-D0 phase-e-common/1.0 · 공간 경계 자체는 E-G1 고정, 행정 편제만 동적.

## 1. 모델링 목적

빅토리아류 고정 State 정의를 회피: 다수 Province를 State/Province/County 등으로 묶거나 일부를 떼고 기존 다른 행정단위에 편입 가능. 같은 Province의 인구·시설·자산 ID는 유지하고 administration membership만 변경. 법률·행정 비용은 C-P/E/Fiscal의 별도 계산; E는 공간 편제의 정본.

## 2. AdministrationScheme, AdminUnit, Membership

AdministrationScheme {scheme_id, authority_country_id, legal_basis, is_primary_legal, active_valid_period, revision, hierarchy_policy_id}. 동일 주권국의 기본 법적 행정체계는 유효한 버전 하나; 군정/특별행정은 별도 Role/Scope로 표현하고 주권국의 legal tree를 훼손하지 않음.
AdminUnit {unit_id, scheme_id, country_id, kind_id, legal_status, optional_capital_province_id, identity_lineage, active_period, revision}. official name/short name/aliases는 language/date-specific NameRecord.
AdminParent {unit_id, parent_unit_id? or scheme_root, valid_period, event_id}. parent는 동일 scheme, active, 상위 법적 kind에 속하며 cycle 없음. scheme_root는 country jurisdiction context, 단독 owner가 아님.
AdministrativeAssignment {province_id, scheme_id, leaf_admin_unit_id, valid_from, valid_until_excl, event_id, revision}. 동일 province/scheme/month의 active leaf는 한 개. Province가 parent의 프로빈스 집합에도 속하는 것은 leaf→ancestor의 파생 조회이며 별도 직접 복제 소속이 아님.
양도/독립/점령 등으로 국가별 기본 scheme이 전환되는 경우 이전 소속 기간 종료·신규 소속 시작을 동일 JBatch에서 처리. Sovereignty absent/unknown은 행정 소속을 명시적 unresolved/temporary category로 나타내며 임의 국가 강제 배정 금지.
단위가 비연속 영토·월경지·도서를 가질 수 있음. 연속성을 요구하는 국가별 법률은 C-P policy로 검사. 수도/행정중심지는 해당 단위의 현재 프로빈스 집합 안에 있어야 하는 정책을 기본값으로 하되 법령상 외부/공동 소재지 예외는 explicit legal rule.

## 3. 계층 깊이·유형

계층은 가변. 국가→주→군, 국가→도, 연방→주→군, 국가 직할구역을 동일 schema에서 표현. 이름과 계층 단계를 동일 개념으로 두지 않음: AdminType taxonomy에는 constitutional powers와 UI style을 불필요하게 섞지 않는다. 제국의 속국·보호국 Country/Government의 독립 법적 실체는 단순 administrative leaf로 강등하지 않으며 C-P/H 정치·외교 상위 관계로 별도.

## 4. 최소 명령과 효과

CreateAdminUnit: ID 발행/명칭/법적 근거, valid parent, 실제 소속 Province 집합 포함/일정 기간까지 empty 조건 명시.
TransferProvince: 기존 leaf period 종료·새 leaf period 시작, 두 ancestor 경로를 affected cache로 표기; source/target same scheme unless G2 cross-country transfer synchronized.
SplitAdminUnit: source unit의 Province closure를 분할해서 exhaustive/disjoint partition. 법적 계보에 따라 원 단위가 유지되거나 폐지될 수 있음. 독립된 신규 unit IDs 필요.
MergeAdminUnits: 대상들의 전체 Province 집합을 새 unit에 귀속하거나 법적으로 존속하는 기존 unit에 편입; previous units retired/tombstone/lineage. child reparent를 명시.
ReparentAdminUnit: 해당 unit 및 모든 descendants의 effective ancestor path 변경. 충돌/계층 반복 및 국가권한 검증 후 원자 적용.
RenameAdminUnit: active ID 그대로 명칭 유효기간만 변경.
ChangeUnitType / AdjustAdministrativeCapital / RetireAdminUnit: 법적 의미와 후손, 인구·시설·지방정부 참조 생존 조건 검사.
ImportAdministrativeReform: 복수 leaf assignment, parent links, local government scopes 및 G2 transfer를 하나의 deterministic normalized batch로 구성.

## 5. 검증 알고리즘

PrepareAdminChange(cmd,snapshot)
A. expected revisions; issuer authority at scheduled effective_month; law/effect; all IDs & date checks.
B. 수정을 하기 전 affected subtree 및 Province closure를 source scheme의 reverse index로 계산.
C. proposed parent graph 구성, 검증: 같은 scheme/국가, 상위 type 정책, no cycle, 유효 period, no dangling child, root coverage.
D. proposed Province→leaf membership interval overlay 생성; 해당 기본 scheme에서 각 applicable province exactly one leaf; parent closure와 sovereign G2의 유효 scheme 호환.
E. 처리 중 이전 leaf→새 leaf 변경의 집합 분할은 no loss/no duplicate; 각 기존 Region/POP/Facility/Ownership는 ID 변경 0.
F. 관련 scope가 DerivedFromAdmin이면 G4에서 affected membership을 재평가. Explicit scope는 그대로 유지(특정 법률 동시 재획정 요구 시 별도 scope event).
G. staging world cross-domain validate; all-or-nothing commit at scheduled stage0; failed transaction produces no new ID, revision, cache, country/POP monetary change.

## 6. 조회·성능

Canonical: province_to_leaf[scheme][province] valid time intervals + unit_to_parent intervals. Reverse leaf_to_provinces, unit_descendants/closure, ancestry, province/group statistics는 revision-bound indexes/derived caches. 전체 국가 계층 재귀탐색 대신 변경 subtree만 dirty invalidation; 대규모 reparent는 하위 멤버십 per-province 원본을 반복 수정할 필요 없음(derived ancestry changed). 독립 삭제/분할은 해당 개별 소속 관계를 실제 변경.
GetUnitParents(unit,t), GetUnitProvinces(unit,t), GetLeafAdministrativeUnit(province,scheme,t), GetAdminAncestors(province,t), QueryReformImpact(cmd), GetAdminHistory(unit).
명령 집합/동일 시점 충돌은 stable sorting by event priority/id, expected revisions. Caches may use compressed sorted province ID/roaring bitmaps with stable serialization; cache는 원본 아님.

## 7. 집계, 예산, 선거구

A주 P1~P5, B주 P6~P10, C주 신설에 P4,P5,P6,P7 편입 시 POP/기업/시설 모두 Province에 남고 State aggregate만 변경. Additive counts delta subtract/add; ratios=updated numerator/denominator, recompute non-additive indices. 재편 자체로 세율·법률·공공재정·local govt debt·선거구 자동 변경 없음.
Admin scheme와 지방정부 Government scope를 분리하므로 중앙집권/분권/연방 권한은 C-P 법률 및 scope-to-administration의 explicit 혹은 derived binding으로 표현.
법률상 선거구 범위가 기존 행정구역 경계와 일치할 경우 BoundToAdmin scope는 후속 G4에서 의도한 효력일부터 따라갈 수 있고, 독립 선거구는 ExplicitAssignment로 그대로 남는다.

## 8. 주의할 역사적 사례

- 월경지·해외령·고립 섬을 다른 Province를 끼워 넣어 연속으로 만들지 않는다.
- 점령지 군정은 임시 civil scope, 법적 주권국의 행정 tree 자체를 부당하게 수정하지 않는다.
- 행정구역 분할 당시 프로젝트·은행·재판·지방채 같은 기관소속은 자동 균등분할 금지; 별도 도메인 승계 계약/후속 사건.
- 이름만 바뀐 동일 행정구역과 법적으로 폐지 후 재신설한 동명 단위를 ID/lineage로 식별.

## 9. 수락 시나리오

G3-A01 P1..P10 중 P4..P7을 제3주로 이동; each Province once, total POP/stock/land area unchanged.
G3-A02 leaf 12개와 parent 3-level, child reparent by one transaction: all descendant ancestors update, original leaf population unchanged.
G3-A03 1개 Province 중복 assignment → reject/rollback.
G3-A04 cyclic parent / cross-scheme parent / missing root → reject.
G3-A05 점령 후 Sovereign A scheme 유지, B 군정 scope 따로.
G3-A06 new sovereign requires new basic assignment or legal bounded transition; no orphan in final state.
G3-A07 name rename retains ID; legal dissolution+replacement new ID and lineage.
G3-A08 discontiguous state allowed, if policy forbids then explicit legal reject.
G3-A09 massive change dirty index only + deterministic worldhash.
G3-A10 save/reload time intervals and descendants equal.
