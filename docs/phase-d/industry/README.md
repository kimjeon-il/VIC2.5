# VIC2.5 Phase D-I1~I5 — 산업 시스템 상세 설계 색인

> **상태: 산업 D-I1~I5 세부 설계 확정 / C++ 산업 엔진·45개 자동 인수시험 미실행**  
> **계약:** `phase-d-industry/1.0` · 2026-11-04 · 브랜치 `work/phase-d-design`  
> 상위: `phase-d-common/1.0`, `pop-design/1.1`, `phase-c-common/economy/politics/military/integration/1.0`, `phase-d-construction/1.0`.  
> 기존 C-E1 기업·생산·원장·C-E2 상품·시장·E3 금융/E5 운송/정치·군사 및 D-D0 단일 월간 규칙을 유지한다.

## 설계 정본

| 단계 | 상세 문서 | 설계 결정 |
|---|---|---|
| **D-I1** | [산업분류·기업·사업장](D-I1_CLASSIFICATION_FIRMS_SITES.md) | 산업활동 다단계 taxonomy, 기존 기업/사업장 분리, 복수산업·활동 배분, 통계 ISIC 대응 |
| **D-I2** | [자원·에너지·생산공정](D-I2_RESOURCES_ENERGY_RECIPES.md) | ResourceStock·채취·재생/고갈, 원료·중간재/연료·부산물/폐기·물리 생산, 비저장 에너지 기능 게이트 |
| **D-I3** | [설비·기계화·생산방식](D-I3_CAPITAL_AUTOMATION_PRODUCTIVITY.md) | 설치 설비·공정 조합/용량 공유, 실제 기계화/훈련·기술·소유권 분리, 전환비용/가동시점 |
| **D-I4** | [입지·집적·공급망](D-I4_LOCATION_CLUSTERS_LOGISTICS.md) | 지역/시장·권리/자원·노동·기반시설·운송/무역의 현장 제약, 집적/혼잡 파생효과 |
| **D-I5** | [산업화·기업생멸·산업정책](D-I5_INDUSTRIALIZATION_POLICY_TRANSITION.md) | 자본축적·설비투자/생멸·도시화/직업전이·법률·정부·전시산업/산업쇠퇴 |
| **ADR** | [35개 산업 결정](INDUSTRY_ADRS.md) | `DI-ADR-001..035` |
| **수락** | [25 불변조건·45 인수시험](INDUSTRY_ACCEPTANCE_TESTS.md) | `DI-INV-01..25`, `DI-T01..45` (명세 완료/코드 미실행) |
| **매니페스트** | [계약 버전·정본 해시](verification_manifest.json) | 문서 blob SHA·정적 결과/릴리스 게이트 |
| **fixture** | [광산→제철 공급망](examples/ore_steel_supply_chain.json) | 광석 매장량 감소/생산, 2개월 철광석 운송, 합법적 제조 투입/결합산출·돈·로트 보존 |
| **fixture** | [공장 기계화·노동전환](examples/mechanization_retrofit.json) | 투자/실행신용·실제 장비 구매/개축/검수, 차월 기술·설비 효력, POP 고용 재배정 |

## 핵심 산업 설계 결정

1. **산업은 경제활동의 분류이지 공장·기업·자산/인구의 별도 원본이 아니다.** `Firm/ProductionSite/ProductionRecipe/CapitalAsset/EmploymentLedger/Inventory/Financial/Ownership`는 기존 C-E1~E5 정본 유지.
2. **산업구조는 실제 기업/공장·노동·시설·실물 재고와 거래에서 산출된다.** 국가별 산업화 레벨·건물 레벨이나 수치만으로 공장·기계/생산을 만들지 않는다.
3. **생산 방식은 레시피·실제 사용 가능한 시설·적격 노동·원료/에너지/자원·기술접근·법률의 조합**이다. 기술 발견만으로 전 공장의 공정/회계 지분 자동 변경 없음.
4. 매장/채취·상품 생산·보관/운송·폐기를 명확히 분리하고, 공정별 인도된 실제 원료·설비/노동 사용량 내에서만 stage4 생산. stage4 산출품은 stage7 매매 가능하나 같은 월 stage3 중간재 사용 불가.
5. 전력/열 같은 비저장 서비스의 **동월 stage4 생산→소비**는 기존 C-E1 단계3 투입 계약과 충돌하므로 `I2-SAFE` 기본, `I2-DISPATCH`는 상위 CE/D4 명시적 ADR·버전 변경/회귀 통과 뒤에만 활성화.
6. 지역/산업 집적·기술 확산·산업화/도시화/정치적 반응은 실제 기업/POP·운송·연구·투자/법·국방 사건에서 파생하며 무료 전국 점수 버프는 금지.
7. 매월 0~11단계/9a~9e 및 `StagingWorld` 단일 commit/abort·source_event_id/typed ID, 정수·고정소수점/결정론·저장버전 계약 유지.

## 원작·외부통계 참고 범위

- [Victoria 3 Production Methods 개발자 설명](https://store.steampowered.com/news/posts/?enddate=1624550727&feed=steam_community_announcements): 공장 선택의 투입·산출/자동화 개념 참고. **고정 생산방식 그룹/수치·원본 코드·게임 파일을 복제하지 않는다**.
- [UN ISIC Revision 5](https://unstats.un.org/unsd/classifications/Family/Detail/2095): 산업활동의 계층 분류/국제 비교. 역사의 과거 산업 활동에 현대 분류를 그대로 강제하지 않는다.
- [System of National Accounts 2025](https://unstats.un.org/unsd/Nationalaccount/sna.asp): 산출·중간소비·부가가치, 기업/정부/가계/외국의 생산/소득·투자 계정 정합.

## 구현 게이트

| Gate | 필수 수행 | 현재 상태 |
|---|---|---|
| DI-DESIGN | I1~I5 세부 구조·35 ADR/25 불변조건·45 시험·수치 fixture/버전 매니페스트 | **설계 확정** |
| DI-L0 | 실제 Phase C 통합 엔진 및 D-D0/D-C 계약 호환·schema/recipe/domain ID 감사 | NOT_RUN |
| DI-L1~L2 | C++ `IndustryCatalog`/사업장 활동, 자원·레시피, 설비/기계화, 공급망·산업정책 실제 기능 | NOT_RUN |
| DI-L3 | DI 45·DD0 24·DC 45 + 기존 D4 48, POP/CE/CP/CM 합성/반례·롤백 | NOT_RUN |
| DI-L4 | 1,200개월/10만·100만 POP 실제 대표부하, 저장·병렬/Windows 등 환경별 계측 | NOT_RUN |
| DI-COMPLETE | 코드·commit/ruleset/seed·테스트 결과·실측 증빙/알려진 제약 모두 PASS | NOT_RUN |

**최종 판정:** `D-I1~I5 DESIGN CONFIRMED` / `INDUSTRY_CPP NOT_RUN`. D-D0·D-C1~C5의 상위 정본 및 Phase C 프로토타입 48개 합성 시험 PASS는 산업의 45개 시험 PASS와 별개이다.
