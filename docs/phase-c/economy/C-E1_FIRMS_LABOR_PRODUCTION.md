# C-E1 — 기업·노동·생산 상세 설계

- 상태: **설계 확정 / 구현 전** · 경제 계약: `phase-c-economy/1.0`
- 상위: [C-D0](../C-D0_COMMON_CONTRACT.md) · [경제 색인](README.md) · POP-D0~D3
- 정본: 기업/시설 상태는 Firm·Site Registry, 인원은 PopulationSlice, 일자리·노동계약은 **단일 EmploymentLedger**, 돈은 FinancialLedger, 실물은 InventoryLedger, 물권/지분은 OwnershipLedger.

## E1-1. 핵심 객체

| 객체 | 최소 필드 | 불변조건 |
|---|---|---|
| `Firm` | firm_id, legal_form, home_jurisdiction, controller_id, lifecycle, account_refs[], revision | 생산/고용/부채/지분의 대표 책임 주체이며 수익은 잔액이 아님 |
| `ProductionSite` | site_id, operator_id, region_id, activity_id, recipe_id, capacity_asset_refs[], state, revision | 기업 1개가 다수 사업장 운영; 설비 소유와 운영자 다를 수 있음 |
| `ProductionRecipe` | recipe_id, revision, input_goods{}, output_goods{}, required_job_slots{}, machine_hours, yield_rule, waste_rule | 투입/산출/손실은 출처 있는 전환 규칙; 무제한 생산 금지 |
| `CapitalAsset` | asset_id, type, site_id, nominal_capacity, condition, installation_status, service_life, maintenance_rule | 유효 설치·가동 상태만 생산능력에 기여; 소유자는 OwnershipLedger |
| `JobOffer` | offer_id, employer_id, site_id, occupation, number_of_people, wage_rate, term, labor_rule_ref, funding_reservation_id | 구인 계획은 아직 계약이나 실제 노동인원 아님 |
| `EmploymentContract` | contract_id, slice_id, firm_or_government_id, site_id, people_count, wage_terms, effective_period, funding_ref, revision | 각 실제 배정 인원의 고유한 주된 전일제 계약 하나; 합산이 적격 인원 초과 금지 |
| `OwnAccountActivity` | activity_id, controlling_household_or_slice_partition, land/asset_refs, region_id, recipe_id, output_allocations | 자영·생계생산은 별도 공짜 산출이 아니라 노동/토지/재고를 실제 차감 |
| `FirmPlan` | firm_id, month, expected_sales, target_inventory, labor_plan, input_orders, credit_request, wage_fund, capex_requests | 의향·예측치와 실행 거래 분리 |

`CapitalAsset`는 물리적으로 설치된 설비의 자산/설비상태 원본이며 `OwnershipLedger`는 누가 소유하는지를 담는다. 기계라는 상품 재고를 설치하면 `InventoryLedger`에서 해당 물량을 투입·제거하고 동일 사건으로 설비 자산 생성/설치 상태를 기록한다. 철거·매각·파괴에는 이에 대응하는 사건이 있어야 하며 재고와 설치된 설비를 중복 계수하지 않는다.

### 소유·사업 형태

사유기업, 국가기업, 협동조합, 가계사업, 소작 생산을 `legal_form`/`operator`/`ownership` 관계로 표현한다. 기업의 소유자·경영자·노동자는 별개다. 국영기업의 생산비도 임의 정부예산에서 발생하지 않고 금융·재고·고용 원장과 연결한다. 노예·농노·부역 등 법적 강제노동은 `LaborObligation`과 유효 법률 및 실제 노동배정으로 표현하며 현금 임금을 가정하지 않는다. 비경제적 강제성을 임의의 생산성 보너스로 바꾸지 않는다.

## E1-2. 월초 기업의 수요·생산 계획 (단계 1)

1. 당월 가격 `P(t,m,g)`는 월초 고정. 전월 실매출·미충족 **지불가능한** 주문·시장 관측과 재고를 근거로 차월 수요 `Dhat` 예측.
2. 기본 수요 갱신: `Dhat[t+1]=clip(lambda*D_realized[t]+(1-lambda)*Dhat[t],0,D_cap)`. `lambda`는 업종별 ruleset 자료로 관리한다. 공급 부족 시 실현 거래량뿐 아니라 지불 가능한 미체결 주문을 관측 수요에 포함하되 주문이 실제 거래라고 기록하지 않는다.
3. 품목별 목표재고 `T=coverage_months*Dhat`; 목표 생산 `q_plan=max(0,Dhat+T-I_available)`을 출발점으로 한다. 부패성/보관한도/예상 단가·재고비용·수익률을 반영해 줄인다.
4. 레시피별 예상 매출과 원가(재료·노무·금융·운송·수수료·세금·유지비), 예상 현금흐름 및 위험을 비교한다. 기대손실이라도 공공 정책·장기계약·유지보수 등 명시적 사유가 있으면 생산계획을 승인할 수 있다.
5. 생산 계획에 맞춰 `JobOffer`, 원재료 주문, 운전자금(현금·실행된 신용), 설치/건설 투자계획을 제안한다.
6. **단계 1 확정된 자기자금 또는 실제 실행 신용 한도 내에서** 임금과 원료의 지출 예약을 각각 만든다. 같은 잔액을 두 종류 비용의 자금 원본으로 중복 예약하지 않는다. 불충분하면 계획 축소 또는 명시적 채무불이행/정책상 지원 결정.
7. 당월 생산은 실제 계약·재고·설비 조건으로 제한된다. 무조건 계획된 수량을 달성하는 것이 아니다.

## E1-3. 노동시장 청산 (단계 2)

### 노동력의 원본과 적격성

`Eligible(slice, job, month)` = 나이·교육/직업자격·건강 기능 요건·복무 상태·노동법·법적 신분·지역 접근성·필수 편의제공을 모두 평가한 참/거짓 또는 실제 적격 인원이다. 장애 진단만으로 전체 취업부적격 처리 금지. 자녀·퇴직자·징집자 수를 기업별 별도 인력풀에 더하지 않는다.

최초 구현의 규약:
- 한 사람당 **주된 전일제 고용관계 또는 주된 자영·생계 노동 배정 하나**. 동일 인원의 중복 배정 금지. 자영·생계·부역 인력도 `EmploymentLedger`의 `LaborAssignment(assignment_type=employed|own_account|subsistence|obligation)`로 주된 노동점유를 등록하고 각 배정의 인원 합이 해당 slice의 적격 인원을 넘지 않아야 한다. 이는 별도 인구 원본이 아니다.
- `PopulationSlice`가 노동 적격자와 비적격자를 섞고 있으면 계약 전 slice/partition을 실제 분할하고 고용·자산·가구 참조를 동시에 이전.
- 주된 직업(profession)과 실제 고용(employment)은 별개. 실업자는 직업 정보 자체를 잃지 않는다.
- 후속 시간제·겸업은 사람 수와 `FTE`/노동시간을 분리한 새 규약에서만 활성화.

### 결정론적 매칭

1. 모든 구인에 대해 유효 법률·노동요건·접근가능 지역·기업의 확보된 임금예산을 확인한다.
2. 노동자는 적격 제안에 대해 순보수(세전 명목임금 + 실물급여의 평가가치 − 실제 출퇴근 비용), 근로조건, 계약 안정성, 위험, 문화·법적 차별 규칙 등을 반영해 선호를 산출한다. 취업하지 않을 선택과 기존 일자리 유지의 대안도 포함한다.
3. 기업은 `vacancy_count`와 우선순위/업무자격을 제안한다. (기본형) 구직자 선호와 기업 우선순위를 고정한 뒤 안정적 ID 동률 순서의 반복 제안/수락 매칭; 미충족 일자리와 실업은 그대로 남긴다. 병렬 실행은 동일 순서의 결과를 산출해야 한다.
4. 한 셀에서 서로 다른 기업이 다른 인원을 채용하려면 allocation partition이 정합되게 나뉘어야 한다. 채용 전 인원배분은 인구와 노동계약을 묶은 원자적 사건.
5. 실제 계약은 `EmploymentLedger`에 하나만 기록한다. Phase B `vic25::phaseb`의 고용테이블을 동시에 정본으로 운영하지 않고 어댑터로 전환한다.
6. 불성립 제안의 임금예산 예약은 해제되며, 확정 인력의 필요 임금예산만 이후 단계 5까지 유지한다.

### 임금 결정

노동수요·실업/구직 경쟁·생산성·이윤·법적 최저임금·단체협상·직무 위험/숙련에 따라 다음 달 임금 제안을 조정한다. 기본형:
```text
gap = (vacancies - eligible_applicants) / max(1, vacancies + eligible_applicants)
margin = (forecast_revenue - forecast_total_cost) / max(1, forecast_revenue)
proposal_next = previous_offer * (1 + clamp(alpha_w*gap + beta_w*(margin-target_margin),
                                             -max_wage_change, +max_wage_change))
offer_next = max(legal_floor_if_any, round_money(proposal_next))
```
`alpha_w/beta_w/target_margin/max_wage_change`는 유효 ruleset의 고정소수점 값. 강제노동·현물임금·공공 급여 등은 다른 계약/법률 규칙을 사용한다. 명목 임금은 기업이 현금 조달할 수 없는 수준으로 자동 확정되지 않는다.

## E1-4. 생산 청산 (단계 3→4)

사업장별 레시피 `r`의 실제 산출은:
```text
q_possible = min(
    capacity_from_installed_assets_and_condition,
    feasible_output_from_confirmed_labor_and_skill,
    min_for_each_required_input(available_input_quantity / input_per_output),
    legal/environmental/resource/transport operating limit
)
q_actual <= min(q_plan, q_possible)
```
- 대체 원료·결합생산·부산물·폐기·열/에너지·생산물 품질 등은 versioned Recipe와 정확한 input/output 계수로 정의한다. 어떤 투입품도 유령 생성되지 않는다.
- 원재료는 **단계 3까지 실제 재고로 인도**되어야 단계 4 생산에 사용 가능. 미래 운송 예정분은 사용 불가.
- 생산량 확정 시 `ProductionEvent`와 `InventoryLedger`의 실제 투입·산출·손실을 원자적으로 작성한다. 채굴/벌목·농업수확의 자연자원은 `ResourceStock/HarvestYield` 등 출처가 있는 재생·채취 사건이 필요하다. 게임의 외생 자원 생성도 사건/규칙 출처를 명시한다.
- 운송·의료·교육 등 비저장 서비스는 해당 월 제공량·유효 슬롯을 추적하고 다음 달로 무제한 이월할 수 없다. 미사용 서비스 용량은 명시적 만료로 정리한다.
- 자영업/소농/자급농은 해당 Slice 인원과 토지·도구·근로 배정을 사용한다. 수확은 현물소비/실제 판매 가능 물량으로 나뉘며, 가계에 현금소득을 임의 생성하지 않는다.
- 시설의 감가상각은 장부 가치·물리적 마모를 구분한다. 기계 고장·파괴·보수는 자산 상태와 실제 비용·원료/장비를 연계한다.

## E1-5. 기업 결산·투자·퇴출

- `Revenue`는 실제 인도에 대한 매출·매출채권으로 분개한다. `CashReceived`는 실제 현금/예금 수취. **매출/이익과 현금은 서로 다른 값**이다.
- 영업손익 = 매출 − 판매된 생산물의 원가 − 임금/유지비 − 해당기간 비용. 순이익은 이자·세금·감가상각 등에 대한 회계처리 결과; 미판매 완제품은 재고자산으로 남고 즉시 매출이 되지 않는다.
- 기업은 자기자본(지분)·이익유보·차입·보조금·국유출자 등 **명시적 거래**만으로 자본을 축적한다. 지분의 새로운 발행/이전도 `OwnershipLedger`에 등록한다.
- 투자는 단계 1·9에서 계획하며 실제 투자재 구매/대금 지급·운송·설비 설치/건설 기간을 거친 후 **다음 달 이후** 생산능력에 반영된다. 자금이 없어도 설비가 자동 증축되지 않는다.
- 현금흐름 위기·회생·법정관리·청산을 구분한다. 폐업은 일자리 소멸(Employment), 재고/설비 처분(Inventory/Ownership), 채권자 청산(Financial), 실업과 가구 수입 변화(POP)로 이어지는 사건 배치다. 부실자산을 무상으로 없애지 않는다.
- 이윤이 적립돼도 자동으로 주주 가계로 지급되지 않는다. 배당은 `C-E3`에서 별도 선언·지급한다.

## E1-6. 산출되는 관측값

지역/업종별 기업 수, 사업장 수, 공급능력, 가동률, 적격 노동 공급, 실업(구직자 대비 고용 안 됨), 명목/실질 임금, 총산출, 중간투입, 부가가치, 기업 이윤/손실, 자영/생계 생산량, 임금체불/부도 건수. 모든 값은 원장·거래·시장가격에서 재계산 가능해야 한다.

## E1 검증 게이트

- E1-G1: 같은 사람 3명을 2개 사업장이 동시에 3명씩 채용 요청해도 총 고용은 3명을 넘지 않는다.
- E1-G2: 건강·연령·복무 변화/직업 전환에도 인구와 계약 참조가 보존된다.
- E1-G3: 자금 예약이 부족한 기업은 임금을 지급한 것처럼 생산하지 않는다.
- E1-G4: 원자재·설비·노동 가운데 하나라도 병목이면 생산량 상한을 넘지 않는다.
- E1-G5: 2개 상품을 결합생산해도 각 산출·부산물·폐기량의 레시피 규약을 지킨다.
- E1-G6: 농가 현물 생산·판매는 별개의 회계 처리이고 현물가치를 현금에 이중 추가하지 않는다.
- E1-G7: 설비 설치/매각 시 재고/자본재/소유권 및 생산능력이 정합하다.
- E1-G8: 기업 부도/청산 중 인구·자산·채무가 사라지지 않고 전체 월 원자성이 유지된다.
