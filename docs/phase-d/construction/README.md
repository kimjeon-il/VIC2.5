# VIC2.5 Phase D-C1~C5 — 건설 시스템 상세 설계

> **상태: 세부 설계 확정 / C++ 전체 구현·자동 인수시험 미실행**  
> **계약 버전:** `phase-d-construction/1.0` · 작성/확정: 2026-11-04  
> 상위 D-D0 `phase-d-common/1.0` · 개발 브랜치 `work/phase-d-design`  
> 본 계약은 `POP-D0~D3` 및 C-D0/C-E1~E5/C-P1~P5/C-M1~M5/C-D4의 단일 원본/12단계/원자성 규약을 변경하지 않는다.

## 상세 정본

| 범위 | 문서 | 완료 범위 |
|---|---|---|
| **D-C1** | [건설사업·주체·부지·계약](D-C1_PROJECTS_CONTRACTS_RIGHTS.md) | 건설 프로젝트, 계약/역할·소유/사용권·법적 허가, 직영/도급/자가건설, 공사 변경 |
| **D-C2** | [건설인력·자재·장비·생산능력](D-C2_LABOR_MATERIALS_CAPACITY.md) | 기존 POP/고용 및 C-E 생산·재고·운송 원장을 통한 건설서비스, 전문인력·장비·재료 병목 |
| **D-C3** | [공정·공사기간·검수·품질](D-C3_WORKFLOW_TIME_QUALITY.md) | WorkPackage DAG, 단위별 실제 작업량, 병목계산·최소 공기·검수/불량/재시공·결정론 |
| **D-C4** | [건설비·투자·금융·WIP](D-C4_FINANCE_INVESTMENT_WIP.md) | 발주/도급/민간·공공 투자, 실행신용/예약·지급과 발생주의, 계약 기성/WIP·GDP 정합, 파산 |
| **D-C5** | [준공·증축·개축·노후·철거·복구](D-C5_COMMISSION_MAINTENANCE_RETIREMENT.md) | 안전·권리·검사/시설 게이트, 차월 가동·자산 마모/유지·개축/교체·재활용·군사 피해 |
| **ADR** | [35개 건설 설계결정](CONSTRUCTION_ADRS.md) | `DC-ADR-001..035` 상위 계약/유효시점/역사적 제도와 수치 보정 경계 |
| **수락** | [25개 불변식·45개 시험](CONSTRUCTION_ACCEPTANCE_TESTS.md) | `DC-INV-01..25` 및 `DC-T01..45`; 전체 C++ 실행 게이트는 **NOT_RUN** |
| **매니페스트** | [검증 기준·소스 해시](verification_manifest.json) | 계약 버전·문서 Git blob·시험 번호·실행 상태 고정 |
| **fixture** | [3개월 신설 공장](examples/three_month_factory.json) | 2개월 실제 공정·기성/WIP·차월 준공·확정 원가/현금 |
| **fixture** | [전쟁 피해와 재시공](examples/war_damage_repair.json) | stage10 손상·보험/손실·재시공·차월 준공 |

## 설계 결정 개요

1. 건설은 **시공 생산서비스/실제 노동·자재·장비**를 요구한다. 건설사·자영업/부역·직영은 기존 C-E1 원장 주체이며, 무상 국가건설력·국가고정 건설순위/무료 건설 포인트 금지.
2. `ConstructionProjectRegistry`는 공사/검수의 원본만 소유. 인구 `PopulationSlice`, 고용 `EmploymentLedger`, 현금/채권·WIP `FinancialLedger`, 상품 `InventoryLedger`, 장비/설비 `CapitalAsset`, 권리 `OwnershipLedger`, 법/관할 C-P를 다시 만들지 않는다.
3. 공사는 주체/현장/공정별로 진행하고, `WorkPackage`는 물리 단위의 실제 작업/DAG/최소 기간·품질을 가진다. 공사 지연·실패·비용초과가 실제 모형 결과가 될 수 있다.
4. 계약 승인·예상 자금·건설 중 자산의 회계가치·실제 지급·준공 후 시설 능력을 분리한다. WIP 자본화와 원재료/하도급 매출을 최종투자/GDP에 이중 반영하지 않는다.
5. 월 m stage4 공사가 완공되더라도 m stage11 `ready_for_commission` 검증 후 **m+1 stage0에 시설/교통망 가동**한다. stage10 재난/전쟁 손상은 그 효력에 반영한다.
6. 기존 stage0~11, 정치 9a~9e, 인구/전투 10a~10f, 전체 World 1회 atomic commit/rollback을 유지한다.

## 월간 Stage별 건설 실행 경계

| Stage | 의미 |
|---:|---|
| 0 | 유효법/부지/계약, 전월 공사재료 실제 도착, 준공/시설 활성 및 기존 설비/공사 상태 스냅샷 |
| 1 | 신축·개축·보수/철거 발주·착공/시공계획·투자/은행 실제 자금·임금/재료/장비/운송 예약 |
| 2 | 건설노동/장인·직영/부역 적격성과 중복 없는 취업/현장 배정 |
| 3 | 건설 공정에 필요한 중간재·에너지/유효 현장 보유자 인도/결제 |
| 4 | 실제 노동·장비·자재로 공정 서비스 생산/소비, 진행량/품질·검수/재작업 생성 |
| 5 | 시공·감리/자영 노동의 유효 임금/현물 보상·원천징수/보조금 실제 결제 |
| 6 | POP 가계의 실제 수령액/현물·주택 서비스와 소비 계획(공사 생산/현금 소급 X) |
| 7 | 차월 공사용 투자재·설치자산·공공조달/운송 인도. 과거 4단계에 소급 재료 공급 X |
| 8 | 시공업체/발주자 기성고·청구/정산·WIP/원가/미지급/법적 손실 회계·부도 |
| 9a | 차월 신규 투자/증축·대체·연구/기술·건설계획, 기존 정치 9b~9e 보존 |
| 10 | 자연재해·전투/산재 인구·시설/미준공 공사 손실, 장비/재고·금융/권리 동시 사건 |
| 11 | 재고/인구/임금/자산/WIP·계약/허가/미완성 공정 정합, ready/차월 예약·가격·World 단 한 번 Commit |

## 외부 게임 참고의 한계

Victoria 3의 국가 건설대기열/건설 부문·재료/고용·생산방식 및 민간투자 개념을 **문제 설정 참고**에만 사용한다.
- [Paradox Victoria 3 Dev Diary #36 — Construction](https://store.steampowered.com/news/posts/?enddate=1646328679&feed=steam_community_announcements)
- [Paradox Victoria 3 Dev Diary #71 — Autonomous Investment](https://www.paradoxinteractive.com/games/victoria-3/news/dev-diary-71-autonomous-investment-in-1-2)
- [Victoria 3 Wiki — Buildings](https://vic3.paradoxwikis.com/Buildings) / [Production methods](https://vic3.paradoxwikis.com/Production_methods) (접근제한/버전별 내용변경 가능)

VIC2.5는 실제 기업·노동·소유/채무·재고/운송·기성고/WIP를 기준으로 독자 규칙을 확정한다. 원작의 게임 파일/수치/코드·텍스트를 복제하지 않는다.

## 구현 및 검증 상태

- **확정:** D-C1~C5 문서, `DC-INV01..25`, `DC-T01..45`, `DC-ADR001..035`, 2개 명시적 `fictional_test_value` fixture, 버전 있는 명세.
- **미수행:** 실제 C++ `ConstructionProjectRegistry/WorkPackage`·시공 월연산/원장 통합, 45개 실제 자동화 인수시험, 기존 D4 48개/상위 CE/CP/CM/POP 재회귀, 저장·병렬·100년 및 대규모 성능.
- 기존 Phase C 프로토타입 48 PASS 보고서는 [별도 통합 작업 브랜치](https://github.com/kimjeon-il/VIC2.5/blob/work/phase-c-engine-integration/docs/phase-c/ENGINE_IMPLEMENTATION_STATUS.md)에 있으며, **Phase D 건설 구현·검증이 됐다는 뜻이 아니다.**

**판정:** `D-C1~C5 DESIGN CONFIRMED` / `CONSTRUCTION_CPP NOT_RUN`.
