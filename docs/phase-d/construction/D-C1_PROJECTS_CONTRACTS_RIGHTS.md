# VIC2.5 D-C1 — 건설사업·주체·부지·계약

> **설계 확정 / C++ 구현·회귀 미실행** · `phase-d-construction/1.0` · 2026-11-04  
> 상위 계약: [D-D0](../D-D0_COMMON_CONTRACT.md), [C-D0](../../phase-c/C-D0_COMMON_CONTRACT.md), C-E1/E3/E4/E5 및 C-P1~P5. [건설 색인](README.md)

## C1-1. 범위와 단일 정본

건설사업은 기존 `Firm` / `ProductionSite` / `CapitalAsset` / `EmploymentLedger` / `InventoryLedger` / `FinancialLedger` / `OwnershipLedger` / `PoliticalRegistry`를 참조한다. `ConstructionProjectRegistry`만이 **사업의 승인/계획·실제 공정 상태·검수/준공의 도메인 기록**을 소유한다. 기업·토지·인구·재고·돈·법적 권리를 건설 원장에 복제하여 쓰기 금지.

**건설의 의미:** 신규·증축·개축/설비 전환·수리·복구·철거/철수·기반시설 형성을 실제 시공 노동/재료/서비스와 계약 이행으로 표현. 외관상 건물 레벨, 국가별 공짜 건설점수, 정부/민간 고정 할당을 사용하지 않는다.

## C1-2. 엔터티 계약

| 객체 | 최소 필드와 소유권/불변식 |
|---|---|
| `ConstructionProject` | `project_id, region_id, project_kind, initiator_ref, client_ref, site_ref?, output_asset_specs[], work_package_refs[], site_right_refs[], contract_refs[], permit_refs[], plan_revision, scheduled_start_month, financing_refs[], state_dimensions, revision`. 설계·공정 상태는 `ConstructionProjectRegistry`가 소유 |
| `ProjectParticipant` | `project_id, role, party_ref, effective_period, source_event_id`. 역할: `initiator`, `client`, `funder`, `general_contractor`, `subcontractor`, `supervisor`, `intended_asset_owner`, `future_operator`, `insurer`. 역할 중복 가능/필수 근거 필요 |
| `ProjectSiteRight` | `project_id, region_id, parcel_or_facility_ref, right_type, ownership_or_law_ref, valid_period, permitted_use`. 권리 **정본은 OwnershipLedger와 C-P 법/관할**; 이 객체는 검증된 참조/현장 접근 조건 |
| `ConstructionContract` | `contract_id, client_ref, contractor_ref, work_scope_ids[], price_scheme, currency_id, payment_schedule_ref, risk_allocation, delay/defect/warranty_terms, approvals, valid_period, revision` |
| `SubcontractAssignment` | `parent_contract_id, subcontract_id, work_scope_partition, effective_period`. 동일 공정·장비·인원·대금을 도급계약/하도급계약에서 중복 계산 금지 |
| `PermitRecord` | `project_id, authority_ref, permit_kind, legal_instrument_ref, effective_period, status, source_event_id`. **법적 효력의 정본은 PoliticalRegistry**; 자체 쓰기 가능한 새 법률/허가권 저장소 아님 |
| `ProjectChangeOrder` | `change_id, project_id, prior_revision, revised_scope, revised_package_refs, price/terms_delta, approval_refs, effective_month`. 이미 확정된 공사·과거 청구/권리를 소급 삭제 불가 |
| `ConstructionMilestone` | `milestone_id, project_id, required_package_ids, inspection_rule, certification_event_ref?, payment_term_ref?, accepted_fraction`. 지급/검수의 근거이며 공정량 원본 재작성 불가 |

`WorkPackage`의 상세 공정 DAG·품질·실제 진행은 D-C3에서 확정한다. `ConstructionProject`의 `output_asset_specs`는 약속된 산출물이지 가동 가능한 `CapitalAsset`가 아니다.

### 상태 축 (단일 enum으로 뭉개지 않음)

- 법/행정: `proposed -> submitted -> authorized / denied / revoked`. 현대 허가제가 없는 시대·시설은 `not_required`를 **적법 법률 룰셋**에 의해 판정.
- 금융: `unfunded -> partially_funded -> fundable -> delinquent / insolvent`. 계획·예산 승인이 실제 현금은 아님.
- 물리: `unstarted -> mobilizing -> in_progress -> suspended -> physically_complete -> inspected -> ready_for_commission -> commissioned / dismantled`. 결함/사고 시 `rework_required`.
- 계약: `negotiating -> active -> completed / terminated / breached`. 계약 종료는 공정완료와 같지 않음.

각 축의 **파생 상태/검증 캐시**는 원천 이벤트/원장 참조에서 계산한다. 부적법한 조합은 `StateTransitionGuard`가 거부(예: 권리 없이 임의 착공, 무허가·무검수 자동 `commissioned`).

## C1-3. 사업·발주·운영·소유자 구별

1. **발주자/사업주체:** 자가가구, 기업, 정부/지방정부, 토지 소유자, 협동조합, 외국 법인/기관 등 법적으로 유효한 경제주체. 정권교체는 그 정부의 법인·채무를 자동 소각하지 않음.
2. **시공자:** C-E1 `Firm`/`OwnAccountActivity`로 건설 서비스를 공급. 정부 직영이라도 노동·중간재·원장을 재사용하며 시공자에게 공짜 '국가 건설력'을 부여하지 않음.
3. **금융제공자:** 은행, 투자자, 채권자, 정부 출자자; `FundingReservation/LoanContract/OwnershipLedger` 참조. 자금 제공만으로 시공계약이나 준공시설의 법적 소유권 자동 취득 금지.
4. **시설 소유자:** 기존 `OwnershipLedger`의 토지·설비·부동산 권리에 따라 준공 전후 결정. **장부의 건설중 자산(WIP) 권리**와 최종 기계/부동산 권리 귀속을 계약으로 명시.
5. **운영자:** `ProductionSite.operator_id` 또는 C-E5 시설 관리·운영관계. 소유자/발주자와 다를 수 있으며 실제 시설·허가·설비 접근 없이는 운전하지 못함.

복합 공사(공장+전용철도+변전소)는 하나의 사업이 여러 `output_asset_specs`/`WorkPackage`를 갖도록 모델링한다. **부분 준공**은 독립적으로 검사되고 실제 유효한 개별 `Asset/TransportEdge`만 단계적으로 차월 활성화 가능. '전체 사업 60%'라는 숫자만으로 설비 60% 자동 가동 금지.

## C1-4. 부지·토지·관할·역사제도

- 위치: `RegionId`와 선택적인 Parcel/Facility 참조(지도 없이 사용 가능). `RegionId`/`MarketAssignment`/`TerritorialJurisdiction`은 다른 관계이며 영토 도형은 권리 정본 아님.
- 권리: 소유권·임차권·사용권·지상권·통행권·수용권·채굴권 등의 허용 범위와 유효기간 확인. 사업 착수 및 지속 공사의 법적 권한을 월초 확정법으로 평가.
- 정부 공공사업은 C-P 유효 예산/발주권·재산취득/수용/보상 근거가 필요. 점령만으로 소유권·유효 정당한 허가 자동 이전 금지.
- 공공 통행권·행정허가가 유효해도 물리적으로 현장 접근불가 또는 전쟁·재난·수송 차단이면 공사 정지 가능.
- 시대별 농노제·부역·길드/장인조합·영지 특권/사인 계약·법인 제도·현물대금·조세특례를 **유효 법/노동/소유/계약 스키마**에 표현. 19세기와 현대에 동일 법인·허가·담보·건축규정 강제 금지.

## C1-5. 발주·입찰·시공계약

- 발주안 `ProjectProposal`: 예상 산출시설/장소·예상 수요/이익·시공 범위·소유계획·공공적 목적·비용/공기 예측. 이것만으로 시설·재고·급여·자금 증가 없음.
- `Tender/ContractOffer`(선택적): 시공 단가, 기간, 품질, 이전 실적, 운송·자재/숙련 접근, 이행보증/위험 제안. 입찰이 없는 시대·상황에는 직접도급/직영을 사용.
- `ProjectApproval`와 `ContractAward`는 법적 권한·유효 자금계획·공사범위의 중복 배정/이중 계약이 없을 때 stage1의 유효 명령으로 확정; 새 법률은 원칙적으로 차월부터.
- 가격 형태: `fixed_price`, `unit_price`, `cost_plus`, `time_and_material`, `own_account`, `in_kind/labor_obligation`. 계약금액은 미래 지급채무의 근거이지 잔액/매출을 미리 생성하지 않음.
- 분쟁·계약변경·추가 공사: `ProjectChangeOrder`에 승인/효력일·공정/자재 변동·가격·보증 조건을 명시. 이미 집행된 자재·고용/금융·공정 실적은 원장 이력으로 유지.
- 작업 범위 분할이 되어도 원도급과 하도급이 동일 자재와 공사를 두 번 산출하지 않음. 하도급업체의 서비스는 원도급의 중간투입, 전체 사업 최종 건설가치는 한 번만 계산.

## C1-6. 수락 조건

- 두 시공사에 같은 `WorkPackage` 전체 범위를 중복 발주해도 한 공정에 두 번 진척이 생성되지 않음.
- 발주자 A·시공자 B·금융제공자 C·시설 소유주 D·운영자 E를 분리한 사업의 실제 권리·지급 및 원장이 보존.
- 불법 점유/무권한 수용/유효하지 않은 허가·취소된 계약/구법 소급 적용을 거부.
- 자체 가구·공공/민간·해외투자/전통 부역에서 동일한 실물 노동/현금/현물 정본을 사용.
- 사업 변경·분할·합병/승계 시 `project_id`와 이전 이력·원장 채권·채무/소유권 참조를 보존.

**상세 시험 ID:** `DC-T01..T09`. [인수시험](CONSTRUCTION_ACCEPTANCE_TESTS.md)
