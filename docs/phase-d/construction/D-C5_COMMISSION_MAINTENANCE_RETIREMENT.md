# VIC2.5 D-C5 — 준공·가동·개축·유지보수·노후·철거·재해복구

> **설계 확정 / C++ 구현·시험 미실행** · `phase-d-construction/1.0` · 2026-11-04  
> 상위: [D-D0](../D-D0_COMMON_CONTRACT.md), D-C1~C4, C-E1/E3/E4/E5, C-M4/M5 및 C-D4. [색인](README.md)

## C5-1. 준공과 가동은 별개

**물리공사 완료**는 실제 작업/검수/시간 게이트의 결과, **법적 준공**은 관할/허가·시설의 안전·사용권 검사 결과, **경제적 가동**은 실제 설치 자산·운영자·원료/인력/에너지/시설 연결 상태를 갖춘 C-E1 `CapitalAsset` 또는 C-E5 `TransportEdge/Node`의 차월 활성이다.

- 월 m stage4 공사가 끝나도 그날 `CapitalAsset`의 생산능력이 당월 사용되지 않는다. stage11 전체 원장 검증 시 `ready_for_commission`을 예약하고, **m+1 stage0에 아래 Gate 전체 재검증** 후 가동.
- 월 m stage10 군사/재해 사건이 당월 stage4 완료된 자산·기초시설에 피해를 주면 m+1 stage0 재검증 시 손상/불량 조건에 따라 가동 거절/축소. 월 m에 이미 수행한 다른 생산/운송은 소급 무효화 금지.
- 일부 복합 프로젝트는 독립적인 `AssetSpec` 구간별 부분 준공 가능. 각 구간은 자체 검사·법률·인도/운송망 연결 조건을 모두 만족해야 하며 사업 전체 공정률만으로 임의 비례 개통 불가.

### C5-2. CommissioningGate

| 게이트 | 필수 조건 |
|---|---|
| `PHYSICAL` | 해당 자산에 필요한 WorkPackage/DAG 실적이 모두 검수·손상 반영 후 충족 |
| `QUALITY` | 시험·안전·하자보수/안정화·정해진 양생 기간 완료 |
| `AUTHORITY` | 유효 건축/사용·환경/군사·영업/시설 운영권, 권리/법적 소유 검증 |
| `DELIVERY` | 설치 재료·자본재가 실제 인도되어 InventoryLedger에서 출고/설치 사건 반영 |
| `OWNERSHIP` | 프로젝트 WIP/시설 소유·위탁 관리 계약의 적법 권리와 대상 자산 참조 |
| `TECHNIQUE` | 도입 기술/생산공정에 요구된 C-D0/D-T 지식·설비/훈련/법적 허가 |
| `NETWORK` | 운송시설은 실제 연결·용량·경로/안전·통과권 조건, 전력/상하수 등 공급망 설계는 해당 도메인의 실물 서비스와 호환 |
| `FINANCE` | 지급 및 분개 정합·자산 인수 의무/보험/담보의 이력. **미지급 계약채무만 있다는 이유로 모든 시설의 기술적 가동을 무조건 거부하지 않음**; 계약의 소유권 보류/금지 조항이 있으면 해당 법률·권리를 따른다 |

`CommissioningEvent`는 `project_id, target_asset_spec_id, ready_verification_refs[], new_asset_id_or_existing_asset_ref, physical_asset_changes[], WIP_transfer_refs[], ownership_refs[], effective_month, source_event_id`. 새 자산 등록/기존 자산 갱신·WIP 금액 재분류/소유권 정합이 하나의 `JournalBatch`에 들어간다.

## C5-3. 시설 생애주기와 상태

기존 `CapitalAsset`의 `installation_status`를 단일 정본으로 유지한다. 필요 시 `asset_lifecycle`는 기존 상태에서 계산/어댑터로 확장하며 별도 경쟁하는 두 가동 정본 금지.

권장 상태:
`planned -> under_construction -> installed_not_commissioned -> active -> idle / under_maintenance -> impaired -> retired -> dismantled`

- `planned`는 약속된 사양이며 실물 자산이 아님.
- `under_construction`는 `ConstructionProject/WIP`가 관리하는 미완성 상태; 이미 설치된 일부 자산은 재고에서 제거되어 설치미가동 상태로 표현할 수 있으나 **생산능력 0**.
- `installed_not_commissioned`는 인도·설치는 완료했으나 가동검수/권리 미충족.
- `active`는 생산/운송 서비스 사용 가능. 당월 생산량은 추가로 E1의 실제 노동·원료·법/자원 상한에 제한.
- `idle`는 기술상 사용 가능하지만 운영계획/시장수요/직원·원료/자금 부족으로 실제 가동이 없을 수 있음. 무조건 철거로 처리하지 않는다.
- `under_maintenance/ impaired`는 명시적 수리·고장·피해 이력과 자산 `condition`을 동반. 손상과 가동불가를 반드시 동일하게 간주하지 않고 규칙에서 부분용량·안전 제한.
- `retired`/`dismantled`는 실제 법적 폐기/철거 계약과 소유권·고정자산 잔액 이전을 요구. 유효 ID 재사용 불가.

## C5-4. 마모와 정비의 분리

- **경제적 감가상각**(FinancialLedger 회계상 자산가치 변화)과 **물리적 상태 열화**(CapitalAsset.condition)를 구분한다. 가동한 시간/부하·환경·유지보수/연령, 물리적 파손·전쟁 피해를 `AssetWearEvent/AssetDamageEvent`로 기록.
- 상태는 예컨대 단위 ppm으로 `0..1_000_000`. 규칙 형식 예:

~~~text
condition_next = clamp(
    condition_open
  - wear_from_actual_use
  - age_and_environment_loss
  - disaster_or_combat_damage
  + realized_maintenance_restoration,
  0, 1_000_000
)
usable_capacity_next =
    rated_capacity
  × condition_performance_factor(condition_next)
  × legal_and_safety_availability
~~~

- 이는 공식 **형식**이고 수치계수는 `versioned Ruleset`에서 산업/자산/역사시기별 설정. 손상이 발생한 월 stage10 이전에 이미 실현한 stage4 생산이나 배송을 소급 되돌리지 않는다.
- 예방 정비(일반 유지비): C-E1 사업장 유지보수 `maintenance_rule`의 실제 수리 노동/부품·서비스·자금 사용. 통상 기능 유지에 해당하면 자본형성으로 중복처리하지 않는다.
- 대규모 개량/증축/수명 연장: 별도 `ConstructionProject` 또는 `WorkPackage`로 기존 자산 수정/교체, 적법한 자본적 지출·WIP·준공/차월 효력을 거침. 단순 유지보수 비용을 투자재로 자동 중복 계산 금지.
- 정비/개축 중 운전 중단은 실제 `AssetAvailabilityWindow`와 C-E1 생산능력/운송용량 제약. 공사 중에도 전체를 멈출 필요가 없는 단계적 교체·운영은 실제 독립 설비/구간별 가용성을 검증할 때만 허용.

## C5-5. 보수·대수선·교체·확장

| 유형 | 실물 처리 | 자산/회계 |
|---|---|---|
| `routine_maintenance` | 기존 시설 유지 목적의 소량 재료/서비스, 적격 노동 투입 | 실제 비용·재고 이동; 원칙적 비용 처리(세부 회계 룰셋) |
| `repair` | 경미/중대 하자 보수, 상태/안전 회복 | 기존 자산 condition 반영, 자본적 개선 해당 시에만 WIP |
| `major_overhaul` | 대수선·수명 연장/분해 수리, 사용중단/시험 포함 | 별도 공사와 명시적 장부/실물 변경 |
| `retrofit` | 신기술 적용/설비 교체·노동훈련 필요 | C-D0 지식/현장 `SiteTechniqueAssignment`, 자재/부품·공사비 |
| `expansion` | 생산시설/운송망 확장 | 증설된 독립 `CapitalAsset` 또는 기존 자산 유효 capacity delta·차월 발효 |
| `replacement` | 기존 자산 철수/폐기 후 신규 자산 인수 | 구자산 잔존가치/회수 재료·신규 물권 동시 정합 |
| `decommission/demolition` | 법적 운영중단/철거·구조물 해체/폐기 | 폐기·재활용·소유·금융·환경의무 등 처리 |

완공/설치 과정에서 `InventoryLedger`의 설비 상품 1을 줄이고 `CapitalAsset` 1을 등록하는 사건은 `WIP` 자본화 금액 재분류와 별개로 잔액/권리를 대응한다. 공사비 지출만으로 대상 `rated_capacity`가 올라가지 않음.

## C5-6. 철거·재활용·손실

1. `DecommissionOrder`는 유효 소유/영업중단·행정/환경법 허가, 철거 공사의 현장·인력·장비/운송·보관·폐기·안전비용을 검증한다.
2. `SalvageEvent`는 실제 회수 가능한 특정 재료·자본재 종류/로트/상태·회수 노동/작업·재고 이동을 기록. **철거만 선언해서 전량 원래 자재를 무료 복구하지 않음.**
3. 철거·가동중지 자산의 C-E1 생산능력 또는 C-E5 운송용량은 유효일에 실제 변경. 이미 출하/예약된 화물은 E5 보류/우회/손실·권리사건으로 처리해야 하며 선적 물량을 자동 삭제하지 않는다.
4. 자산의 미상각잔액·철거충당 의무·이행보증/보험/환경복구 책임은 C-E3/E4와 C-P 권한에 따라 별도 발생·지급/손실. 소유권은 적법 법률/계약 없이 정부나 철거 시공자로 자동 전이되지 않음.
5. 군사·자연재해로 파괴된 시설은 `AssetDamageEvent`와 `Inventory/Financial/Ownership` 및 C-M4 사상·군수 사건에 연결. 전후복구는 재료/고용/자금/시공기간이 필요한 **신규 복구 공사**이며 즉시 건물 레벨 자동 복구 금지.

## C5-7. 기타 시설·서비스의 연결

- 철도·도로·항만·교량·운하·창고 등은 C-E5 `TransportNode/Edge/Capacity`와 수송서비스 생산에 연동. 공사 준공은 법적 통행·관세·운송업 종사자/차량·수송재고를 자동 제공하지 않는다.
- 학교·병원·주거는 향후 C-E4 공공서비스, POP의 실제 주거 수요/가구 서비스와 연결할 수 있는 **실물 시설 수용력**만 제공. 시설 준공만으로 해당 인구의 문해율/건강·생활수준을 직접 증가시키지 않음.
- 군사시설·조선소·병참창고는 C-M의 실제 유효 장비·물자/군인·군비를 참조. 외형상 요새를 완공했다고 무기·주둔병을 자동 생산하지 않음.
- 산업별·시대별 생산자산의 기능·마모·유지비/공법은 `Ruleset`. 미정 실측 수치를 임의 0 또는 '원작 수치'로 채우지 않음.

## C5-8. 수락 조건

- `stage4` 완공과 `stage11` ready, `m+1 stage0` 실제 효력 순서를 유지하고 불완전 검수/권리·설비/자재 부족은 commissioning 거부.
- stage10 전투로 직전 완공 시설이 손상됐으면 차월 검수 실패/수리 경로로 전환, 이전 월 생산 소급 무효화 없음.
- 설비 재고→설치자산→감가·수리→철거/회수 재고의 수량/금융·소유권 정합.
- 감가상각만으로 물리 condition 일괄 감소 또는 시설 상태 개선만으로 회계 장부에 새 현금 추가 금지.
- 폐기된 철도 간선을 통한 무제한 수송/이전 시점 배송 소급 완료 금지, 부분 개통의 단절 구간 검사.
- 실제 수리/복구 비용·재고/인력과 시설 상태개선이 연결돼야 함.

**상세 시험:** `DC-T37..T45`.
