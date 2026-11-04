# E-G2 — 주권·영토 이동·군사통제·국경 파생

> 상태: SPECIFIED / NOT IMPLEMENTED · 상위 E-D0, C-D0(관할), C-M5(점령·종전)

## 1. 정본

JurisdictionRegistry는 기존 C-D0 TerritorialJurisdiction의 공간 표준 구현. 별도 E Sovereignty 복제본은 금지. 각 관계:
{ id, province_id, role:[sovereignty,civil_administration,fiscal_authority,military_control], authority_typed_id, valid_from_month, valid_until_month_exclusive|null, legal_basis_id|null, source_event_id, revision, certainty_status }.
- authority는 role에 맞는 country/government/institution/force/JointAuthority만 허용.
- Unknown, none/no sovereign, disputed claims, joint administration은 구별하는 tagged status. 복수 country의 확정 독점 sovereign 상태는 거부. 합법적 공동통치는 1개 JointAuthority와 참여 주체의 계약으로 표현.
- Claim/Dispute Registry는 {claimant,candidate_provinces,source,time,scope,claim_status}를 사용하며 확정 sovereignty를 바꾸지 않는다.
- 일시적 military_control change로 population citizenship/legal_status/ownership/market/financial stock 자동 변경 금지.

## 2. 명령/사건

TransferSovereignty, ExchangeProvinces, AnnexTerritory, RecognizeSeparation, UnifyCountries, ChangeMilitaryControl, SetCivilAdministration, SetFiscalAuthority, RegisterClaim, WithdrawClaim, CreateJointAuthority.
입력: command/event IDs, issuer, affected ProvinceIds(sorted unique), expected_role_revisions, legal_basis (treaty/war/politics), effective_month, source, dependencies. 등록 = 전후 권한 근거의 정확한 의미가 있고 단순 게임 UI 선택이 확정 효력과 동일하지 않음.

## 3. 검증·원자 이벤트 알고리즘

1. source event, issuer authority, ruleset effective law/war/settlement authority, role-specific type 검사.
2. 모든 ProvinceId 존재·unique·not duplicate, effective date와 expected revisions 유효성 검사.
3. 앞선 jurisdiction 기간 종료와 새 기간 시작을 충돌 없이 구성; concurrent claim/occupation 관계는 의미에 따라 별도 유지.
4. 기본 주권이 바뀐 Province의 새 국가 basic administration assignment/과도 state를 G3에서 합법적으로 구성. 점령 군정은 별도 관할.
5. MarketAssignment/Transport restrictions/financial obligations/pop citizenship/ownership 등은 자체 사건이 명시된 경우에만 해당 분야 staging에 반영; 빠진 필수 법적 승계가 있으면 commit reject 또는 transitional flag with legal basis.
6. Jurisdiction + G3 + C-P/M + 기타 분야의 JournalBatch를 하나의 StagingWorld에 원자적으로 적용, 글로벌 INV 검사, 월말 한 번 커밋. 부분 변경 불가.
7. 유효 commit 이후 Province→sovereign index 및 국경 관련 adjacency edge 캐시만 invalidation.

## 4. 시간 경계

일반 정책/영토 조약은 월 m stage9 채택→m+1 stage0 유효. stage10 war/control 결과는 당월말 군사 사건으로 확정하여 m+1 opening에 실효 상태로 사용; 이전의 m 단계4/5 생산·임금·세금·영토소유를 소급 정정하지 않음. 추가 일/서브틱 도입 전까지 월중 점령에 근거한 당월 재정 부과/인구 강제이동을 자동 수행하지 않는다.

## 5. 영토 사건별 승계

- Cession: old sovereign jurisdiction 종료, new sovereign 시작, 새 basic admin scheme assignment, 국가·행정 집계 갱신. 군사 통제, 국적·시장·소유권/채권 자동 승계는 금지.
- Occupation: military_control만 변경; 적법 군정 설치는 civil/fiscal의 독립 사건, 실제 예산·보급·치안 필요.
- Independence: C-P 신규 Country/Government entity + territories + G3 scheme; 인구·회사·채무·재산·상표·산업시설 명시 승계/존속. 군 지휘/군 보급 별도.
- Unification: country lineage/status/valid period, 분할/흡수 구분, 행정 통합은 별도 G3 사건; 이전 국가의 금융·인구 원본 삭제 금지.
- War/Peace: C-M5 settlement의 WarGoal/OccupationAdministration/SettlementObligation과 E events linked, 기간 및 분쟁 claims 재평가. 강제적 '전쟁 종료 즉시 모든 점령 해제' 금지.
- Country may temporarily have zero sovereign provinces (e.g., exile); 자동 국가 폐지 판단은 C-P/H 소관.
- 수역/영해·조차지·주둔권·속국 관계는 land sovereignty의 단순 대체 아님; scope/right 및 향후 H 조약 객체로 확장.

## 6. 국경 파생

CountryBoundaryEdge(a,b): LandBoundaryContact(a,b) AND resolved sovereignty(a) != sovereignty(b). Unknown/joint state는 별도 border style/status; 단순 국경 확정 금지.
MilitaryFrontierEdge: control(a)!=control(b), 군사 이동 가능성과 전선 발생 여부는 C-M 별도 판정.
AdminBoundaryEdge(scheme,level,a,b): 해당 시점 정규화 ancestor at requested level 다르면 경계. Level 미존재/국가 직할의 경우 explicit display rule.
ChangeSet으로 affected Province IDs와 각 인접 edge의 dirty bit만 갱신. 국가 외곽 전체 polygon을 매번 재생성하지 않음. 해상 국경은 E-G1 topology와 별개의 향후 maritime jurisdiction.

## 7. 교차 규약·오류·검증

- Jurisdiction ID stable; 관계의 기간 변경은 lineage/역사 기록과 source_event; no in-place overwrite of committed history.
- 안건이 법적으로 거부되면 normal rejected command; 임의 두 sovereign 중복·revision conflict·고아행정·부적법 월 소급·분개 불균형은 hard validation error/atomic abort.
- Territory(P,t) = sorted ProvinceIds with valid effective sovereign=P at t. Area/pop/GDP/industrial-site sums는 각각 spatial static values와 POP/Finance/Industry 원본으로 조회.
- 핵심 fixture: A: P1~4, B:P5~8; B가 P2~3 점령(sovereign A 유지) 후 조약으로 P2만 cede. military P3는 B 통제, P2 sovereignty B, POP 총량 및 국적 불변, 별도 국적 사건 없이는 citizenship unchanged.
- 1000 Province bulk transfer with one invalid entry must result zero changes and no leaked revisions/ID allocations.
- 서로 다른 두 변경 명령이 같은 source role/revision를 변경하려 하면 revision-conflict, order-stable.
- Restore/replay same jurisdiction intervals, derived border IDs and hashes.
