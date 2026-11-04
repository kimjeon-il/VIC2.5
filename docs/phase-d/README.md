# VIC2.5 Phase D — 건설·산업·기술 설계 색인

> **D-D0 공통·D-C1~C5 건설·D-I1~I5 산업·D-T1~T5 기술 및 D-V 통합 검증 설계 확정 / 전체 Phase D C++ 구현·검증 전**  
> 2026-11-04 · 공통 계약 `phase-d-common/1.0` · 브랜치 `work/phase-d-design`

## 설계 정본

| 문서 | 상태 | 범위 |
|---|---|---|
| [D-D0 공통 설계 계약](D-D0_COMMON_CONTRACT.md) | **설계 확정** | 건설·산업·기술의 객체/원본 소유권, 이벤트, 월별 12단계, C-E/P/M/D4 호환성, 지식과 기술 채택 구분, 14개 불변조건/20 ADR |
| [D-D0 인수시험](D-D0_ACCEPTANCE_TESTS.md) | **24개 시험 명세 확정/실행 전** | 공사 자재·자금/시공·WIP/준공, 산업·자원·연구/기술 도입, 법률·군사·결정론, 실패·롤백 |
| [기계 판독 계약 매니페스트](contract_manifest.json) | **정적 번호·blob SHA 잠금** | 14개 불변조건, 24개 인수시험, 20개 ADR, 상위 계약 버전/구현 상태 |

## Phase D-C1~C5 건설 상세 설계

- [**건설 설계 색인**](construction/README.md): 프로젝트·자금·시공/재료·공정/품질·준공/보수·철거 및 시간계약.
- [**건설 인수시험**](construction/CONSTRUCTION_ACCEPTANCE_TESTS.md): 25개 불변조건, 45개 개별 수락 시나리오(명세 확정/실제 C++ 실행 전).
- [**건설 ADR**](construction/CONSTRUCTION_ADRS.md): 35개 결정. [검증 매니페스트](construction/verification_manifest.json)와 [3개월 공장 공사 fixture](construction/examples/three_month_factory.json), [전시피해 복구 fixture](construction/examples/war_damage_repair.json).
- **상태:** `phase-d-construction/1.0` 설계 확정; `work/phase-d-design`에서만 문서 추가; C++ 건설 엔진/인수시험 `NOT_RUN`, `main` 미병합.

## Phase D-I1~I5 산업 상세 설계

- [**산업 설계 색인**](industry/README.md): 산업분류·기업/다업종 사업장, 자원·에너지/레시피·설비/기계화, 입지/공급망·산업화/기업생멸/산업정책.
- [**산업 인수시험**](industry/INDUSTRY_ACCEPTANCE_TESTS.md): 25개 불변조건, 45개 검증 시나리오(명세 확정/실제 C++ 미실행).
- [**산업 ADR**](industry/INDUSTRY_ADRS.md): 35개 결정. [수치 예제 — 광산→제철 공급망](industry/examples/ore_steel_supply_chain.json) 및 [기계화 개축](industry/examples/mechanization_retrofit.json).
- **중요:** 비저장 실시간 전력의 동월 생산·사용 기능 `I2-DISPATCH`는 C-E1/C-D4의 명시적 상위계약 확장 전 `UNSUPPORTED`. 기존 공개 월별 12단계는 그대로.
- **상태:** `phase-d-industry/1.0` 설계 확정, 산업 C++ 엔진 및 45개 시험 `NOT_RUN`. 작업 브랜치에 문서만 등록.

## Phase D-T1~T5 기술 상세 설계

- [**기술 설계 색인**](technology/README.md): 기술/지식 분류·선행관계·주체별 보유상태, 실제 연구인력/연구비·실험검증, 교류/확산·특허/면허, 사업장별 기계·훈련/기술 도입, 경제·교육/보건·정치·군사 효과.
- [**기술 인수시험**](technology/TECHNOLOGY_ACCEPTANCE_TESTS.md): 25개 불변조건·45개 검증 시나리오(명세 확정/실제 C++ 미실행).
- [**기술 ADR**](technology/TECHNOLOGY_ADRS.md): 35개 결정. [검증 매니페스트](technology/verification_manifest.json)와 [연구 성과 예제](technology/examples/two_month_research.json)·[라이선스/지식확산 예제](technology/examples/licensed_diffusion_three_holders.json)·[실제 공장 도입/POP 훈련 예제](technology/examples/site_retrofit_training.json).
- **중요:** `ResearchWorkEvent`(실제 연구실적)와 `KnowledgeState`(지식 보유) 및 `SiteTechniqueAssignment`(사업장 적용)을 분리하며, 기술 확보 즉시 전국의 생산성/군사/의료 능력이 상승하지 않음. `I2-DISPATCH` 미지원/상위계약 확장 대기.
- **상태:** `phase-d-technology/1.0` 설계 확정, 실제 기술 C++ 엔진 및 45개 시험 `NOT_RUN`. 현재 작업 브랜치에 문서만 등록.

## Phase D-V 건설·산업·기술 통합 검증 설계

- [**D-V 통합 검증 색인**](integration/README.md): 단일 World/원장, 12단계·자원경합, 36개 불변식, 72개 교차시험, 30 ADR, 저장·장기/성능·구현 게이트.
- [**D-V 72개 실제 검증 명세**](integration/D-V5_TEST_MATRIX.md)와 [**36개 불변조건**](integration/D-V3_INVARIANTS.md): 기존 DD0 24·건설45·산업45·기술45·C-D4 48에 추가되는 교차 수락 기준. **279개 정의된 ID, Phase D 단일 C++ 엔진 실행은 미실시**.
- [**D-V 수락/구현 게이트**](integration/D-V7_GATES_ADRS.md): 30개 ADR, L0 공통 코드/브랜치 호환 → L1/L2 실제 기능 → L3 시험 → L4 저장/결정론 → L5 1,200개월/대표부하 실측.
- [**D-V 검증 매니페스트**](integration/verification_manifest.json)와 [**4개 복합 시나리오/오라클**](integration/D-V4_SCENARIOS_ORACLES.md): 실제 금전·인구/자산·상품·지식·법률/군사 영향과 전역 Abort 수치정합.
- **상태:** `phase-d-integration/1.0` 설계 확정. 실제 C++ 기반 L0는 `work/phase-c-engine-integration`/Phase D 브랜치 미결합으로 `BLOCKED_FOR_CPP`; L1~L5/72개 실행·정상 PASS `NOT_RUN`.

## D-D0 확정 원칙

1. **인구/고용/재고/금융/소유권은 기존 원장을 재사용**한다. C-E1의 Firm/ProductionSite/CapitalAsset/Recipe를 다시 만들지 않는다.
2. 공사는 실제 시공 서비스, 인력, 장비, 원료, 계약, 자금에 의해 진행한다. 전국 통합 건설력이나 무료 건설점수는 기본 규칙이 아니다.
3. 산업은 분류와 생산레시피의 추가 모델이며, 실제 생산·판매·가동률·설비는 C-E1/E2의 정본에 남는다.
4. 기술은 **지식 발견·이전**과 **사업장별 설비/숙련/법률에 근거한 도입**으로 구분하며, 국가 수준의 기술 즉시 보너스는 부여하지 않는다.
5. 월별 0~11단계/9a~9e/10a~10f는 변경하지 않고, 공사·연구 성과/시설 가동은 해당 유효 시점에만 반영한다.
6. 기존 Phase C-D4 48개 프로토타입 테스트 PASS는 Phase D의 24개 시험 PASS를 의미하지 않는다.

## Phase D 후속 세부 설계 순서

| 단계 | 분야 | 설계 범위 | 상태 |
|---|---|---|---|
| D-C1 | 건설 | 건설사업·주체·시공·토지/소유/계약 | **설계 확정** |
| D-C2 | 건설 | 건설인력·자재·건설업/시공장비·가용용량 | **설계 확정** |
| D-C3 | 건설 | 공정·공사기간·실제 작업·하자/재시공 | **설계 확정** |
| D-C4 | 건설 | 공사 금융·민간/공공투자·WIP/기성고 | **설계 확정** |
| D-C5 | 건설 | 준공·개축·보수·철거·노후·피해복구 | **설계 확정** |
| D-I1 | 산업 | 산업분류·기업/사업장·산업구조 | **설계 확정** |
| D-I2 | 산업 | 자원/원료·에너지·중간재·공정/산출 | **설계 확정** |
| D-I3 | 산업 | 기계화·설비·생산방식·숙련/생산성 | **설계 확정** |
| D-I4 | 산업 | 입지·집적·수송/시장·구조변화 | **설계 확정** |
| D-I5 | 산업 | 산업화·기업 성장/쇠퇴·정책 | **설계 확정** |
| D-T1 | 기술 | 기술/지식 분류·선행관계·이력 | **설계 확정** |
| D-T2 | 기술 | 연구주체·연구원·연구비·실험 | **설계 확정** |
| D-T3 | 기술 | 발명·연구실패·기술이전·확산 | **설계 확정** |
| D-T4 | 기술 | 기술/공정 채택·개축/훈련·현장 보급 | **설계 확정** |
| D-T5 | 기술 | 경제·사회·정치·군사 피드백 | **설계 확정** |
| D-V | 통합 | 36 불변식·72 교차시험/30 ADR·279 상위 포함 ID + D4 회귀·100년/재현성/성능 | **설계 확정** |

## 상위 계약·현재 엔진의 정확한 상태

- [POP-D0~D3](../pop/README.md): 인구·교육/건강·희소 셀/월간 원자성 기준.
- [C-D0 공통 계약](../phase-c/C-D0_COMMON_CONTRACT.md): 국가/정부/지역/기업/전 분야 단일 원장 및 시간 계약.
- [C-E1~E5 경제](../phase-c/economy/README.md): 생산/고용·자본재 설치, 상품/시장·금융/투자·운송.
- [C-P1~P5 정치](../phase-c/politics/README.md): 법률/인허가·정부·자치·정책 효력.
- [C-M1~M5 군사](../phase-c/military/README.md): 군수/동원·시설피해·전시경제.
- [C-D4 통합](../phase-c/integration/README.md): 30개 불변조건·48개 프로토타입 시험.
- [Phase C 구현 보고서](https://github.com/kimjeon-il/VIC2.5/blob/work/phase-c-engine-integration/docs/phase-c/ENGINE_IMPLEMENTATION_STATUS.md): 2026-11-04 기준 48개 프로토타입 회귀 PASS, 통합 C++ 코드는 별도 `work/phase-c-engine-integration`, `main` 미병합, 완성형 게임 구현 전.

**건설·산업·기술의 상세 설계와 D-V 통합 검증 설계까지 확정했으며, 실제 단일 Phase D C++ 구현·전체 인수시험·100년/대규모 검증은 수행하지 않았다.**
