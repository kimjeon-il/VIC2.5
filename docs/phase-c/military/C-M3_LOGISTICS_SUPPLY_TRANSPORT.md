# C-M3 — 군수조달·군수망·보급·수송·정비

- **상태: 설계 확정 / 구현 전** · `phase-c-military/1.0`
- 상위: [C-D0](../C-D0_COMMON_CONTRACT.md), [E1 생산](../economy/C-E1_FIRMS_LABOR_PRODUCTION.md), [E4 재정](../economy/C-E4_FISCAL_PUBLIC_SECTOR.md), [E5 운송](../economy/C-E5_TRANSPORT_INTERNATIONAL_TRADE.md), [군사 색인](README.md).
- 군수품 수량 원본은 InventoryLedger이고, 생산/구매·예산·운송·배속은 각각 원래 분야의 원장 사건으로 처리한다. 군사 UI의 보급량은 조회·캐시일 뿐 독립 쓰기 가능한 중복 재고가 아니다.

## M3-1. 군수시스템 객체

| 객체 | 필수 필드/책임 |
|---|---|
| `SupplyClass` | class_id, good_type_refs, per_role_consumption_rule, minimum_stock, perishability, compatibility |
| `MilitaryDemand` | demand_id, authority, unit_id, operation_id?, goods{}, needed_by_month, urgency/priority, authorization_ref |
| `ProcurementContract` | contract_id, buying_authority, seller, goods{}, price/currency, delivery_schedule, warranty/penalties, fiscal_ref, payment_terms |
| `SupplyDepot` | depot_id, region/node_id, operator, inventory_holder_id, capacity_by_class, service_slots, security/condition |
| `UnitSupplyPosition` | unit_id, inventory_holder_id, assigned_goods{}, available_by_class, damaged_goods{}, revision; 실물량은 InventoryLedger 참조 |
| `LogisticsRoute` | source_node, destination_node, edges[], mode, travel_months, capacity_refs, required_permissions, risk |
| `SupplyShipment` | shipment_id, demand_ref, goods_qty, from/to_inventory_holders, route_ref, dispatch/arrival_month, status; E5 Shipment의 군사 용도 관계 |
| `MaintenanceJob` | job_id, equipment_asset_ref, workshop_id, parts/skill_hours, condition_before/after, time_required, funding_ref |
| `MedicalSupplyAllocation` | care_site_id, medicine/consumables, personnel/care_slots, service_period, patient_care_event_refs |
| `SupplyConsumptionEvent` | event_id, unit_holder, operation_id?, goods/qty_used, recipe/mission_rule, losses, equipment_condition_effects |

공급물은 식량·물·피복·약품·연료·탄약·부품·예비장비·건설자재 등 실제 `Good`이며, 군사적 용도 태그만 별도로 갖는다. 군수창고·수송차량·항만·철도·야전병원은 실제 시설/설비 자산 및 고용/유지비용 제약을 받는다.

## M3-2. 조달과 경제·재정 결합

1. **단계 1:** 정부의 적법한 군사예산/기존 계약/작전 계획으로 `MilitaryDemand`를 산정. 군수기업의 `ProductionSite`가 실제 원재료·노동·설비·자금을 확보해야 공급 가능. 수요가 곧 상품 생산은 아니다.
2. 재정지출은 `BudgetAppropriation/ProcurementContract`와 `FiscalLedger`의 승인/지급 사유로 관리하고, 실제 자금 지급·채무는 `FinancialLedger`로 기록한다. 미지급 대금·연체·선급금/외상은 실제 채권/채무 관계로 남긴다.
3. 단계 3/4 군수산업에 실제 중간재가 인도/생산되어야 단계 7 완제품을 거래할 수 있다. 단계 4에 새로 생산한 탄약을 동일 월 단계 3에 과거 투입한 것으로 소급할 수 없다.
4. **단계 7:** 군수품 실제 구매/인도/출하 시 InventoryLedger가 해당 수량을 판매자→정부/보급창/운송중 보유자에게 이전한다. 해당 `transaction_id`와 대금/채권·과세·소유권을 교차 검증한다.
5. 장비를 보급창에서 부대에 지급할 때는 유효한 `EquipmentAssignment`와 재고보유/책임자를 동기화한다. 소총 100정을 예산으로 샀다고 100정의 독립 장비 숫자와 100정의 보급창 재고를 각각 소유한 것처럼 합산할 수 없다.

**군수예산만 늘면 군사력이 즉시 증가한다는 모델을 금지.** 산업 생산능력·물적 재고·운송/통관·훈련·정비, 실제 군부대 흡수 역량을 모두 확인한다.

## M3-3. 보급망과 수송

- 운송망은 E5의 `TransportNode/TransportEdge` 및 `Shipment`을 그대로 사용. 육로·철도·해상·강·항공 등의 시대별 운송수단은 `mode`/기술·접근 권한으로 구분하고 시기상 존재하지 않는 수단을 임의 적용하지 않는다.
- 여러 노선을 지나면 각 구간의 **실제 용량·운송기간·연료·수송인력·운임·금수/봉쇄/점령 통과 허가**가 필요. `reserved_capacity <= available_capacity`. 민간/군사 수요가 같은 운송 서비스를 요구하면 별도 무료 군용차로 수송한 것처럼 취급하지 않는다.
- 보급권/보관권/소유권 분리: 정부가 소유한 물자라도 운송 중 `in_transit` 보유자는 운송업자일 수 있으며, 부대가 실제 도착 전 물자를 소비할 수 없다. 같은 상품은 출고→운송중→입고의 각 시점에 정확히 하나의 물리 보유 상태로 기록.
- `travel_months > 0`: 출하월과 그다음 월의 수입지역에서 아직 `available`이 아니며, `scheduled_arrival_month`에 단계 0 입고 검증된 이후 사용 가능. 국경통과/항만적체·봉쇄/손상·도난/폐기는 별도의 통관/운송 사건으로 실제 수량을 줄이거나 지연한다.
- `travel_months=0`: 같은 지역/그달 실제 수송이 입증된 경로에서만 허용. 단계 7 부대 입고가 확인되었으면 단계 10 해당 월 전투에 사용 가능하되 병력이 단계 10에 새로 동원된 경우 소급 투입 불가.
- 사용 가능 재고량은 구분: `on_hand` 보유, `reserved` 다른 작업에 배정, `available=on_hand-reserved-damaged`, `in_transit` 도착 전 사용금지. 같은 탄약을 두 전투가 각각 전부 소모하지 못하게 단계별 예약한다.

## M3-4. 보급 소요와 우선순위

단위별/월별 소요 `D[unit,good,role,m]`는 실제 병력·훈련·작전 종류·장비 사용·기상/지역 조건·정비·의료 필요와 **버전 있는 소비규칙**으로 산정. 미래 예상 소요와 실제 사용량을 분리한다.

- 식량/물/의료 등 생존·보건 필요, 연료·탄약/무기 부품, 정비·숙영·운송서비스는 서로 대체 불가능한 카테고리로 명시한다. 대체재가 가능하면 `SubstitutionSet` 및 법적/기술적 호환성에 따름.
- 보급요청은 중요도/법적 우선순위/계약/작전/부대별 `SupplyPriority`로 안정적 정렬/정수 비례 배분. 우선권의 정치·군 지휘권은 실제 제도/명령에서 온다.
- 보급 부족은 즉시 병력·장비를 삭제하지 않는다. `days_of_supply`, 유효 임무 수행 가능성, 기력·건강, 정비 중단/고장, 피로/사기 등에 차월 후속 효과를 부여한다.
- 병력/장비의 전투 작동률은 가용량과 필요한 필수 군수품을 바탕으로 **상한** 계산. 공급이 0인 탄약을 임의 소비하며 전투를 진행하지 않는다.
- 부대별 현장 재고는 유효 보유량, 후방 창고 재고는 공급가능성, 생산예정량은 미래 공급계획으로 분리해 사용자에게 표시한다.

## M3-5. 보급·정비·의료의 실제 사용

- **단계 3/7:** 정부 군수조달 거래 및 보급 출고·운송, 부대 재고 인도. 수송 서비스 자체는 해당 기간 운송업체의 고용·시설·연료 소비를 발생시킨다.
- **단계 4:** 군수공장/정비공장/군병원 서비스 용량 및 재고·인력 계산. `MaintenanceJob`의 부품·정비 노동은 실제 이용되어야 장비 상태가 복구된다.
- **단계 10:** 실제 작전/전투/훈련 사건이 부대가 이미 소유·수령한 물자의 소비를 InventoryLedger에 기록. 모든 소모는 `SupplyConsumptionEvent`로 버전/원인을 보유한다. 단계 10 전투 중 새로운 재고 손실·유실과 장비 파괴는 단계 11 전 세계 계정·자산가치 검증에 포함.
- **보건:** 실제 의무·병원 인력, 약품·치료시설·병상 및 수송 접근이 있어야 병상/부상자 치료·회복의 처리량을 부여한다. 치료비 청구/실제 지급과 의료서비스 제공/회복은 별개.
- 부상자 전원에게 치료확률 하나만 적용해 장애/질병을 무조건 제거하지 않는다. 치료 가능한 상병과 기존 만성 상태를 HealthProfile로 보존·변환한다.
- 의료시설/수송/포로 수용 등 보호대상 인원은 정치·국제규범에 따른 제약과 실제 인도적 지원 필요를 별도 관계로 둔다.

## M3-6. 보급 작전의 비용·원장 검증

```text
UnitAvailable[g,m] = InventoryLedger(unit_holder,g).on_hand
                   - inventory_reserved[g,m] - damaged_unusable[g,m]
ShipInTransit[g,m] is NOT part of UnitAvailable.
ConsumptionActual[g] <= UnitAvailableBeforeConsumption[g].
TotalCost = payment_goods + freight + storage + insurance + tariff
            + repair + manpower_cost (do not double-count included costs)
```

보급 부족이 외생적으로 해소되는 `instant_refill`/예산만으로 재고 생성 금지. 역사 시나리오에서 기존 재고를 초기 입력할 때에는 source_provenance, quantity_scale, owner/custodian 및 초기 계정/재산권 일관성 필요.

## M3 검증 게이트

- M3-G1: 소총 100정 구매 → 실제 Inventory 100, 지급된 금액/채무도 Financial에 대응, `UnitAssignment`는 그중 **해당 현장 재고에 인도된** 수량만.
- M3-G2: 탄약 40발 부대 보유·전투 2건 각 30발 요청 → 총 40발 이하 소비/예약.
- M3-G3: 2개월 운송되는 의약품 60상자 → 출발월·다음월 목표지 부대는 해당 물량으로 치료 불가.
- M3-G4: 운송 중 의약품 5상자 손실 시 도착 55/손실 5/운송중 0을 원장으로 설명.
- M3-G5: 군수예산 1,000 증가하지만 제조원료·운송능력 0 → 자동 장비 1,000 증가 금지.
- M3-G6: 정부 예산 승인 100, 계약금만 지급 30이면 현금지출 30·미지급의무 구분.
- M3-G7: 정비부품 또는 정비사 부족 시 장비 상태를 무비용으로 `serviceable`로 바꾸지 않음.
- M3-G8: 단계 10에서 탄약 소비 후 단계 8 잠정 결산을 소급 수정하지 않고, 단계 11 최종 검증에 전투 자산변동을 포함.
- M3-G9: 민간 수송과 군수수송이 같은 노선용량을 신청해도 총 배정량은 실제 운송용량 이하.
