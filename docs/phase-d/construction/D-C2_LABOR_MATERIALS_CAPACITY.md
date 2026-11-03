# VIC2.5 D-C2 — 건설 노동력·자재·시공장비·생산능력

> **설계 확정 / 구현·시험 미실행** · `phase-d-construction/1.0` · 2026-11-04  
> 상위: [D-D0](../D-D0_COMMON_CONTRACT.md), C-E1(기업·노동·생산), C-E2(시장), C-E3(자금), C-E5(운송), POP-D0~D3. [색인](README.md)

## C2-1. 건설공급 주체

건설서비스의 생산자는 기존 `Firm`(민간·국영·협동), `OwnAccountActivity`(가계/자영·현물), 유효한 `LaborObligation`에 의한 직영/부역 인력, 공공기관 직영팀 등이다. `ConstructionFirm` 또는 독립 `ConstructionSectorPopulation`를 만들지 않는다. 생산 단위는 C-E1 `ProductionSite`의 활동 `construction_service`와 공정별 `ConstructionTechnique` 레시피.

시공기업 공급능력은 해당 월의 **실제로 고용된 적격 인구 + 유효 장비 서비스 + 선행 인도된 재료 + 현장 접근/법률/환경조건**에서 결정된다. 전국/국가 단위 공짜 건설점수, 건설사 존재만으로 생기는 무조건 노동·자재·공정 증가를 금지한다.

### C2-2. 신규 관계 객체

| 관계/뷰 | 최소 정보·원본 경계 |
|---|---|
| `CrewAssignment` | `project_id, work_package_id, contractor_site_id, employment_assignment_refs[], occupation, qualification_req, period, revision`. **인원 원본 아님**; C-E1 고용을 참조하여 월별 현장·공정에 배치 |
| `SiteEquipmentUse` | `work_package_id, capital_asset_id, service_requirement, reserved_service, used_service, service_month, rental_or_ownership_ref`. 설치자산은 C-E1, 소유는 Ownership |
| `MaterialAllocation` | `project_id, work_package_id, good_id, required_quantity, stocked_lot_refs[], reserved, consumed, wastage, revision`. 실제 물량은 오직 InventoryLedger |
| `SiteStorageAccount` | `custodian_ref, region, storage_site_ref, inventory_account_ref, handling/condition_rule`. 별도 물량 원본이 아닌 실제 창고·보관자 참조 |
| `ContractorCapacityView` | 활동중인 `ProductionSite/Employment/CapitalAsset`로부터 계산하는 월간/공정종류별 서비스 상한; 쓰기 가능한 국력·레벨 원본 금지 |
| `SupplyConstraintTrace` | 작업 의향량·확보 노동/자재/기계/운송/자금·법적 조건별 병목 원인/수치, 진단 조회값 |

## C2-3. 노동 공급과 직무 적격성

- C-E1의 `Eligible(slice,job,month)`와 건강/연령·교육·기능요건·편의제공·법적 노동권한을 그대로 사용한다. `PopulationSlice.count` 유일한 인구 정본. 장애 진단 자체를 일반적인 고용불능/전 분야 노동능력 0으로 간주하지 않는다.
- 최초 계약 `phase-d-construction/1.0`은 C-E1과 동일하게 **한 인원당 한 달에 주된 전일제 고용/자영/부역 배정 하나**로 제한한다. 일용직·겸업·시공 하청업체 간 동시근무 등은 C-E1 FTE/시간제 계약이 명시적으로 승인된 뒤에만 확장.
- 한 기업에 근무하는 노동자를 여러 공사로 사용할 경우 첫 버전은 `CrewAssignment`에서 인원 partition별 **한 달 한 현장·주된 프로젝트**만 사용한다. 프로젝트 내부에서 서로 다른 공정을 순서대로 수행할 수 있으나 같은 인원 노동서비스 시간을 이중 사용하지 않는다.
- 직무별 필수 숙련: 일반시공, 토목/측량, 목수·석공, 철공·기계설치, 감독/검수, 운송·조달, 전기/철도 등 **시기·공법에 따른 다중 직무 분류**. 명칭·수치·숙련도는 규칙 파일. 한 직무의 인원이 다른 모든 직무를 자동 대체하지 않는다.
- 근무 가능한 인력은 지역·합법적 이주/통근·교통비·현실 접근/전시동원을 고려. 원거리 대규모 인력은 단일 국가 건설 대기열 때문에 공짜 순간이동하지 않음.
- 기존 현역 인구는 월초 복무/고용 적격성에 따라 제외. stage10에 신규 동원·사상 발생 시 그달 stage2 노동계약·stage4 작업/임금을 소급 변경하지 않고 다음 달 인력 감소에 반영.

### 노동력 기여

`CrewOutputCapacity`는 최소한 다음을 반영한다:

~~~text
available_labor_work_units =
    sum_over_eligible_active_crews(
        verified_headcount
        × contractual_work_period_units
        × task_skill_factor
        × effective_site_access_factor
        × health_and_accommodation_factor
        × effective_labor_rules_factor
    )
~~~

- `contractual_work_period_units`는 실제 인원/계약에 포함된 월간 기준 근로 서비스로, 추가 고용 인원/시간제 계약 원본이 아니다. 수치 정책·정수 로트·근무일수 변화는 ruleset으로 관리.
- 무단 노동생산 보너스는 금지; 법정근로·숙련 부족·안전/산재·공사 환경의 영향은 명시한 계수/원본으로 추적.
- 노동부문을 별도로 복제하지 않으며 정규 `EmploymentLedger`의 인건비 지급근거를 단계5에 검증.

## C2-4. 시공설비

- `CapitalAsset`의 장비는 콘크리트 혼합/양중/굴착/철도부설·동력 등 시대별 종류를 규칙 파일로 표현. 설치자산과 상품 재고를 한 물건으로 이중 계상 금지.
- 실제 보유/임차·운행 가능한 시간, 정비상태·에너지/연료·운영 숙련/접근성을 평가. `EquipmentUseEvent`는 해당 월 서비스 실제 사용에 대한 참조이고 기계가 공짜로 복제되지 않는다.
- 유효 장비 서비스는 작업 종류/장비 `service_unit_id`별 고정소수점으로 관리하고, 장비가 여러 현장에 분산되면 장소 이동·설치/운송기간·장비 이동 서비스/비용이 필요하다.
- `reserved <= physical_service_capacity`, `used <= reserved` (비예약 현장 직영 작업은 같은 단계의 유효 즉시예약 사건을 요구).
- 설비의 실제 마모·수리·파손은 C-E1 `CapitalAsset.condition`과 D-C5 유지보수로 연결; 가상 `ConstructionCapacity` 재고 축적 불가. 사용하지 않은 **비저장 서비스 용량은 월말 소멸**.

## C2-5. 자재·원료·서비스와 운송

1. 기본 시공 재료는 C-E2 `GoodId/Quantity`와 C-E1 `ProductionRecipe` 투입, 시장 `Order/Fill/Delivery/Settlement`로 구입한다. 도급 계약이 있다고 자재는 지급되지 않는다.
2. 특정 공정에서 `required_quantity`를 정한 뒤 stage1 자금·실효 주문/재고 예약을 요청. stage3에 **정말 인도된 중간재**만 stage4에 투입 가능. stage4 이후 시장에서 생산/도착한 자재는 후속 월의 공정에 사용.
3. 시장의 창고·운송중·현장 자재는 `InventoryLedger`의 실제 `holder/custodian`로 구분. E5에 따라 운송예정이 아닌 실제 `scheduled_arrival_month <= m`와 통관 허가를 stage0에 확인. 남는 현장자재는 재고로 다음 달 이월할 수 있다.
4. 자재 대체(석재/목재, 수입 자재 등)는 버전 있는 `ConstructionTechnique/WorkRecipe`의 승인된 대체비율·품질/구조제약만 허용. 같은 자재를 경제/건설/군수에서 중복 인출 불가.
5. 폐기·재작업 손실·자재 부패, 보관비·운송파손은 `InventoryLedger` 손실/비용/채권 사건으로 처리. 표면상 계획공정 대비 실제 투입으로 품질·낭비를 계측한다.

## C2-6. 실제 할당과 공급 경쟁

- 건설 프로젝트/광업·군수·일반 제조업/연구가 **공통 노동/상품/장비/자금/운송** 자원에 경쟁한다. 건설을 특별 대기열에 담았다는 이유로 선취권이 자동 발생하지 않는다.
- 예약 우선순위는 유효 법령·계약상 선순위권/기한·실제 대금/운송계약·미착착공 위험과 deterministic stable ID로 결정. 정부/민간이라는 속성만으로 절대 우선권을 주지 않음.
- 부분할당은 로트·계약/공법 허용범위 내에서만 가능; 한 공정이 일정한 최소 투입 묶음을 필요로 하면 재료·노무·설비를 모두 유효 확보하지 못한 상태로 생산 불가.
- 동률 시 C-D4의 고정 정렬, 반올림은 largest remainder·잔여 추적을 따른다. 자원 하나를 건설과 일반 생산에 동시에 예약하면 하나만 확정하거나 명시적 재배정/부분 거절.
- 여러 현장의 자재/장비 용량을 전국 합산해 특정 현장에 순간 배정 금지. C-E5 실제 경로·수송·시간을 거쳐야 한다.

## C2-7. 보존·진단·수락

- 인원: `sum(active CrewAssignment.actual_headcount) <= sum(valid EmploymentLedger contractor workers)`; 같은 인원 partition이 여러 현장·주된 일을 중복 점유할 수 없음.
- 자재: `opening_inventory + arrivals + production − consumed − dispatch − loss = ending_inventory`, 한 물품 로트가 `in_transit` 및 현장 보관에 중복 표시 금지.
- 장비 서비스: 월별 `sum(reserved_use) <= available_service_units`; 노동/설비 서비스의 비사용 잔량은 새로운 창고 상품이 아님.
- 시공 실제 진행은 `min(material_feasibility, labor_feasibility, equipment_feasibility, site_access, funding/permission)`를 넘지 않음.
- 산출 조회: 시공사별 계약수·활성 현장/직무 노동·임금/장비 서비스, 자재 부족·지역 조달단가/운임, 미실현 작업량, 공정별 병목지표.

**세부 시험:** `DC-T10..T18`. [인수시험](CONSTRUCTION_ACCEPTANCE_TESTS.md)
