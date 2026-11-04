# H-D4 — 종속관계·보호국·주권 위임·자치·국가승계

> phase-h-diplomacy/1.0 · 2026-11-04 · SPECIFIED / NOT_IMPLEMENTED
> 상위: H-D0/H-D1~D3, C-P1/C-P4/C-P5, C-M5, E-D0/E-G2~G4, C-E3/E4, G-D0.
> 핵심: **국제법적 국가·통치정부·영토 주권·군사점령·권한 위임·자치권·비공식 영향력은 서로 다른 객체와 관계**다.

## D4-1. 정의·적용 범위

'종속국'을 단일 overlord와 autonomy slider로만 표현하지 않는다. 역사적 속국·보호국·종주/조공 관계·피보호 지역·자치령·식민지 행정·위임통치·인적 동군연합·연방/국가연합을 실제 **권리와 권한의 조합**으로 설명한다. 하나의 역사적 지위 이름이 같아도 시기·지역/조약에 따라 세금·외교·군사·내정권한이 다를 수 있다.

- **Country**: C-P1 국제적 국가 실체, Government는 현재 집권 정부. 보호관계 성립·종료만으로 CountryId/인구가 자동 생성·삭제되지 않는다.
- **SovereigntyRelationship**: H-D0 정본. principal_country_id, dependent_country_id/eligible_actor, relationship_kind, treaty_ref, rights_matrix_ref, covered_scope, effective_period, revision, provenance.
- **TerritorialJurisdiction**: E-D0/C-D0 정본으로 Province별 sovereignty, civil_administration, fiscal_authority, military_control을 독립적으로 유지. **주권 보유 = 관세·행정·군사권 전부 일치**는 아님.
- **DomesticAutonomyGrant**: 동일 Country 내부의 지방/자치권은 C-P/Phase E TerritorialScope/AuthorityGrant가 정본이고, H는 다른 Country 간 대외위임·조약/보호관계만 소유. 지방자치권을 별도의 H 종속국 객체로 중복 생성하지 않는다.
- **InformalInfluence**: H-D6 파생 관계. A국이 B국 경제를 지배적으로 통제해도 법적 protectorate/sovereignty 자동 성립 없음.
- **MilitaryOccupation**: C-M5/E military_control 및 occupation administration. 군사 점령은 적법한 외교적 주권이양/보호관계와 별개의 사건.
- **PersonalUnion/DynasticLink**: C-P1 국가원수·인적 승계와 H 조약적 대외 의무를 링크. 같은 국왕이라도 Country/국고·외채·POP를 단일 국가로 합산하지 않는다.

## D4-2. 최소 권리·권한 행렬

권한은 named right를 (holder, grantor, scope, territory/market, legal_basis, valid_period, delegation_rule, veto/consent, implementation_custodian, revisions)로 명시한다.

| Domain | 대표 권리/의무 | 실제 원본 및 처리 |
|---|---|---|
| foreign_policy | 대사파견, 승인, 외교입장, 특정 조약 협상 | H-D1/D2 + C-P 외교권한 |
| treaty_capacity | 독자적 조약/참가/비준, 종주국 동의 필요 여부 | H-D3 + C-P 법/비준 |
| war_declaration / peace | 선전포고·참전·강화 권한, 보호국 군사개입 | C-P1/C-M5 + H 조약 |
| defense/security | 방위보장, 현지 주둔, 무기 조달/배치·군정 | H TreatyTerm + C-M/POP/C-E actual |
| customs/trade | 관세·시장접근·무역제재/조약 통상권 | C-P/E 권한 + G/C-E5 실제 통관/무역 |
| taxation/tribute | 과세권, 조공·보호비/금전 납부/현물 제공 | E fiscal_authority + C-E4/C-E3/Inventory |
| internal_legislation | 민법·경제법·형법·인사/교육/지방제도 결정권 | C-P Law/AuthorityGrant |
| appointments / administration | 총독/장관 임명권, 자치 의회·기관 구성, 민정 | C-P/E civil_administration |
| territorial_disposition | 국경·영토할양·주권 이전/조약 적격성 | E-D0 sovereignty/Claim + H-D3 |
| judicial/financial | 상소·사법권, 차관/채무보증·중앙은행/통화권 | C-P/C-E3/E4 법·금융 원장 |

표는 요구 가능한 권한명이지 '보호국은 모두 foreign_policy=false' 같은 보편 역사법칙이 아니다. 특정 관계의 **권리 행렬**이 실제 관할 범위를 결정한다. 명목권·실제 집행가능성·현재 영토 통제를 별도 평가한다.

## D4-3. 관계 유형과 상태

- 관계 예: suzerainty, vassalage, tributary, protectorate, dominion_like, externally_represented_entity, mandated_territory, associated_state, customs_dependency, defense_dependency, personal_union. **표시용 명칭**이고 구성요소는 treaty + rights_matrix + jurisdiction + fiscal/defense obligations.
- 관계 상태: proposed → agreed_treaty → effective → amended/contested → suspended/terminated/succeeded. 관계 성립은 적법한 서명·비준/발효·권한/관할·시점과 일치해야 함.
- 종속관계는 다층체계(예: 제국→왕국→공국), 여러 권한영역의 서로 다른 보호국을 가질 수 있다. **같은 범위·시간의 배타적 단일 권한을 양립 불가능하게 이중 위임하는 것은 금지**, 비배타적 조언/보증 등은 다중 허용. 명시적 공동주권/공동행정은 E-D0 JointAuthority 및 법적 근거로만 처리한다.
- 위임 권한 그래프의 같은 영역·시점에서 무한 루프/상충 우선권 방지: parent/authority chain cycle 검출, 필요 시 scope-specific precedence. 재위임할 법적 근거가 없는 종속국이 제3국에 배타권을 넘길 수 없다.
- 국가가 완전한 독립을 얻더라도 일정 기간 종주국 차관·투자·군사기지·영토 사용권을 유지할 수 있다. **법적 종속관계 종료와 경제·군사 영향력 해소는 별개의 사건**.
- A의 형식상 주권과 B의 사실상 군사점령·C의 세금 징수권이 특정 프로빈스에서 다를 수 있으며 E의 동적 관할 원장이 실제 역할을 관리한다.

## D4-4. 종속관계의 경제·군사·사회적 실효성

1. 조공·세금·수출특혜/보호비·군사원조 의무는 H-D3 TreatyObligation으로 정의하지만, **현금과 상품**의 출납은 C-E3/E4/G/F/C-E5에서 실제 실행한다. 종속관계만으로 종주국 세수 자동 증가·병력 자동 지원 금지.
2. 종속국 시장은 자동으로 종주국 국내시장에 흡수되지 않는다. 관세동맹·공동시장·통상특권은 명시적 H-D3 조항 → G PolicyProjection 및 C-D0 MarketAssignment에 따름.
3. 종속국 군대는 C-M5의 별도 실제 부대·지휘체계/POP 복무할당. 군사적 종주권이 있어도 군인 수를 종주국 현역으로 이중 합산/무상 이전하지 않는다.
4. 영토 주권·민정·과세권·군사통제를 E-D0에 분리하므로 현지 학교/경찰/공무원 급여, 과세, 입법을 실효기관/재원 없이 무료 시행할 수 없다.
5. 국내 반종속 운동, 자치 요구, 독립 지지 등은 C-P5 Grievance/MobilizationCampaign과 실제 POP/기관·조약 위반·경제 피해의 결과. H가 자동 생성한 가상의 '독립 POP'이나 계급 보너스로 처리하지 않는다.
6. 보호관계가 해체되어도 기존 경제 의존도/무역계약·해외투자·군사지원이 남아 있을 수 있으며 H-D6의 경제·군사 영향력은 실제 관계를 기준으로 계속 평가한다.

## D4-5. 설립·협상·자치 변경·독립/병합

**보호관계 생성**
1. 기존 Country/actor의 적격성과 권한을 검증하고 H-D2에서 보호국 조약 및 구체 권리/의무/승계·퇴출 조건을 협상한다.
2. H-D3 서명·비준·효력 확인 후 H-D4의 SovereigntyRelationship과 AuthorityGrant/필요시 E Jurisdiction 변경을 동일 effective_month로 예약한다.
3. 실제 재정·군사·무역/행정 집행은 해당 C/E/G/F 의무와 원장으로 진행. **계약이 성립했다고 중앙정권이 지방 모든 직업과 국민 정체성을 자동 변경하지 않는다.**

**자치권 확대/축소**
- 전권 일괄 자동 변경이 아니라 treaty term/권한 행렬 변경(예: 대외조약권 일부 복구, 독자관세권 회복, 군사동원 위임 해지)을 한다.
- 비준/조약상 승인과 국내 입법/행정기구 교체·직위 임명, 재정/군수 계약 재협상을 따로 처리. 자치율 0→100 값 변경만으로 모든 법적 효과 강제 금지.
- 정치적 동의 없는 일방적 행위는 treaty breach/contested authority/국제위기·독립전쟁 원인일 수 있으나, 효력/실효 통제·강제능력은 별도로 평가한다.

**독립/국가분열/병합**
- 이미 존재하는 Country가 보호관계에서 벗어나면 H 관계 종료/변경이 기본. 원래 C-P/E에 없던 신규 Country의 독립이 성립할 경우, C-P/E의 역사적 계보·적법성·주권/정부/법률·관할 사건으로 **새 ID 발행**. 인구·예금·재산·시설의 물리적 복제 금지.
- 국가분할/영토할양은 대상 ProvinceId 집합에 E sovereign_jurisdiction 전이, 새로운 정부/헌정·채권/채무·군대/외교조약 승계, POP 국적·거주/고용의 **독립적인 정책/분배 사건**이 필요. 고정 프로빈스를 무분별하게 분할하지 않는다.
- 국가 자체의 소멸/통합/독립을 단순 국제승인 수준과 혼동하지 않는다. C-P1 Country lineage와 tombstone·유효기간을 보존.
- 공동주권/공동관리·무주지/미확정·사실상 통치의 역사적 사례는 E-D0의 별도 권한 상태를 존중한다.

## D4-6. 주권권한 충돌·종속관계 경쟁

- B가 A에 외교권을 위임한 상태에서 C와 군사동맹을 체결하려고 한다면, **B의 독자적 treaty_capacity**와 A의 veto/approval·예외범위를 확인한다. 위임되지 않은 교육·지방자치 조항은 별도로 유효할 수 있다.
- 두 종주국이 같은 범위의 유일한 방위권을 주장하면 자동으로 양쪽을 합법자로 인정하지 않는다. Claim/Dispute·조약 유효성·관할 불일치/법적 분쟁을 기록하고, E의 배타적 권한/JointAuthority 규칙을 따른다.
- 종속관계/민정 법적 권한과 실제 사회·경제적 의존(영향력)은 별개. 법률상 종속국이라도 비공식적으로 제3국의 경제·문화 영향이 강할 수 있고, 경제 의존만으로 종속국이 되지 않는다.
- 점령·전쟁상태라면 C-M5/ E military_control과 종전·강화 조약 및 법적 유효 권한의 실행순서를 검증한다. 전투승리 하나만으로 영유권·외교권/국적을 묵시 이전하는 것 금지.

## D4-7. 공개 인터페이스·단계·불변식

API: GetSovereigntyRelationships(country,snapshot), GetDelegatedAuthority(principal,dependent,domain,scope,month), CheckTreatyCapacity(actor,proposed_term,month), ProposeDependentRelationship(terms), AmendAutonomyGrant(relationship,rights_matrix_revision), ResolveMultiplePrincipals(scope,domain), ProposeIndependenceSettlement(province_refs,terms), ExplainSubjectRights(country,snapshot).

시간: Stage0 관계/행정권한·법률·조약 효력 동결; Stage1 승인된 보호비/원조·세금/관세·군사통행 계획 검증; Stage3~8 해당 실제 경제·무역/군사 자원 이전; Stage9 자치 개정·독립합의·조약/권한 변경 차월 예약; Stage10 군사통제/인구·국가 존속 관련 실제 사건 참조; Stage11 H/E/C-P/C-E/C-M의 권한/원장·이력 검증 및 단일 Commit.

- HD4-INV01 Country/Government/Province/JointAuthority/Claim/Occupation/Subordination/Influence 분리.
- HD4-INV02 법적 권한 행렬과 실제 경제/민정/군사 수행 상태 분리.
- HD4-INV03 동일 시점/배타적 영역의 모순 위임, 무근거 재위임, 권한 사이클 금지.
- HD4-INV04 보호/종속 성립·해소로 POP/재고/자금/부채·군대의 자동 생성·흡수 금지.
- HD4-INV05 조공·보호비/원조·군사방어·관세권은 명시 TreatyTerm+실제 재정/물류·군사사건으로만 이행.
- HD4-INV06 종속·자치권 수정 및 주권이전은 E/C-P 유효 jurisdiction/authority와 일치.
- HD4-INV07 법적 주권·군사점령·민정·과세권·국적·재산권을 동일시하지 않음.
- HD4-INV08 국가 독립·분열·병합의 lineage/tombstone·채무/소유/조약 승계 보존.
- HD4-INV09 복수 종주국·법적 분쟁/공동관리의 명시적 적격성·JointAuthority와 Claim 구분.
- HD4-INV10 같은 원인 의무/영토·재정 적용의 멱등·전원장 롤백/결정론 유지.

오류: HD4_ERR_INVALID_PRINCIPAL, NO_DELEGATION_AUTHORITY, DELEGATION_CYCLE, OVERLAPPING_EXCLUSIVE_RIGHT, ILLEGAL_TREATY_CAPACITY, ILLEGAL_SOVEREIGNTY_CHANGE, MISSING_SUCCESSION, AUTOMATIC_POP_TRANSFER, REVISION_CONFLICT.

시험: HD4-T01~T12([인수시험](DIPLOMACY_ACCEPTANCE_TESTS.md)). 상세 설계 문서이며 실제 C++ 구현·회귀 미실행.
