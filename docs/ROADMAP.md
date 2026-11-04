> **D0~D3 설계 단계 완료 (코드 이식 전)**: [새 POP 설계 색인](pop/README.md)과 [인수 조건](pop/ACCEPTANCE_TESTS.md)을 참조. 구현은 기존 S0-P1 이전에 3계층 레지스트리/PopulationSlice → 상태 전이 → staging/rollback 순서로 진행합니다. 아래 S0-P1~P6은 구현 단계이며 아직 완료되지 않았습니다.

# S0-POP 구현 로드맵

| 순서 | 단계 | 작업 | 완료 정의 |
|---:|---|---|---|
| 1 | S0-P1 | POP Registry 및 코호트 전이 | 성별·연령·건강·직업·지역 이동 정합성 |
| 2 | S0-P2 | 고용·소유·금융 원장 | 임금·대출·자산 소유의 단일 원본 확보 |
| 3 | S0-P3 | 월별 12단계 및 거래 커밋 | 순환참조·이중지급·미확보 자금 사용 금지 |
| 4 | S0-P4 | 소비·재고·생계생산·생활수준 | POP 욕구가 수요·실물 소비·생활수준에 연결 |
| 5 | S0-P5 | 임금·이윤·투자·가격 동태 | POP-기업-금융-시장 간 완전한 자금순환 |
| 6 | S0-P6 | 100년 및 충격 재검증 | 회계 보존·재현성, 경기 붕괴 원인 설명 |

S0-A(금융), S0-B(노동), S0-C(가격·생산), S0-D(장기검증)의 기존 목표를 위 단계에 통합한다.

## 이번 저장소 초기 커밋의 실질적 위치

**P1/P2의 최소 자료구조 골격 + P3 단계 순서 인터페이스**까지 존재한다. 실제 기업 의사결정·시장청산·소득 분배가 연결되어 있지 않으므로 P1/P2/P3의 구현 완료를 선언하지 않는다.

## 향후 확장

정치(선거·정당·이해집단), 법률(투표권·노동규정), 교육(학교·문해율), 보건(치료·재활), 군사(징병·전투·제대·포로), 외교(무역·종속·식민지)를 같은 POP 및 원장 ID 위에 추가한다.

## Phase C 공통·경제 상세 설계(설계 완료 · 구현 전)

[Phase C-D0 공통 설계 계약](phase-c/C-D0_COMMON_CONTRACT.md)을 확정했습니다. POP-D0~D3의 단일 인구 원본과 월별 12단계를 유지하며 경제(C-E1~E5)·정치(C-P1~P5)·군사(C-M1~M5)의 영역별 세부 설계 및 C-D4 통합 검증을 후속으로 진행합니다. 또한 [경제 C-E1~E5 설계](phase-c/economy/README.md)를 확정하여 기업·생산·고용, 수요·시장·가격, 가계·은행·투자, 세금·정부예산, 운송·무역의 상세 사건/계산 및 [인수시험](phase-c/economy/ECONOMIC_ACCEPTANCE_TESTS.md)을 정했습니다. 별도 `work/pop-phase-b` 브랜치의 인구·사회 기초 엔진은 C-D0 및 C-E의 통합 구현 완료와 구분합니다.

## Phase C 군사 설계(설계 확정 · C++ 미구현)

[군사 C-M1~M5 상세 설계](phase-c/military/README.md)에서 징병·예비군/제대, 부대/편제·장비/지휘, 군수·물류, 작전/전투·사상자/회복, 전쟁/점령·종전/복구를 [C-D0](phase-c/C-D0_COMMON_CONTRACT.md) 및 [C-E1~E5](phase-c/economy/README.md)와 호환되게 확정했습니다. [군사 인수시험](phase-c/military/MILITARY_ACCEPTANCE_TESTS.md)에는 인구·복무·군수품·재정·사상자·관할권 관련 불변식과 30개 시나리오가 포함됩니다. 실제 통합 엔진/실측 역사계수/장기 C-D4 검증은 미완료입니다. 정치 C-P1~P5 설계 원본은 PR #7을 통해 GitHub main에 반영됐습니다.

## Phase C-D4 통합 검증 설계(문서 확정 · 구현 미실행)

[C-D4 통합 검증 계약](phase-c/integration/README.md)은 C-D0 및 C-E1~E5·C-P1~P5·C-M1~M5의 공통 World/12단계·인구/금융·재고/선거·동원/전투·관할 원장 정합성, [30개 불변조건](phase-c/integration/D4-3_INVARIANTS.md), [48개 통합 시험](phase-c/integration/D4-5_TEST_MATRIX.md), 결정론/세이브·롤백/100년·대규모 성능 인수 기준을 문서로 확정했습니다. 실제 C++ 통합 테스트/1,200개월 실행은 하지 않았습니다. L0 선행조건인 `phase-c-politics/1.0` 정치 설계 등록과 Phase B 인구원장 통합은 작업 브랜치에서 수행했습니다. [실제 구현/시험 보고서](phase-c/ENGINE_IMPLEMENTATION_STATUS.md)에 따른 D4 48개 합성 인수시험은 Linux/Windows PASS이지만 L0~L7 전체 게임 인수는 아직 미완료입니다.

## Phase C C++ 통합시험(2026-11-04)

[Phase C 구현·D4 최종 테스트 보고서](phase-c/ENGINE_IMPLEMENTATION_STATUS.md)에서 `D4-T46/47/48`을 포함한 48개 합성 통합시험 48 PASS/0 BLOCKED/0 FAIL과 Linux/Windows CTest 9/9 결과를 확인할 수 있습니다. C++ 엔진은 `work/phase-c-engine-integration` Draft PR #6에 있으며 완성형 역사게임 엔진의 전체 C-E/P/M 기능 구현과 정식 장기 인수는 계속 별도 상태로 관리합니다.
