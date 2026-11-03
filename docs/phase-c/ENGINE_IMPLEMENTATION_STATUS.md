# Phase C C++ 통합 엔진 — D4 48개 인수시험 결과 (2026-11-04)

> **검증 상태: 프로토타입 범위 C-D4 상세 인수시험 48 PASS / BLOCKED 0 / FAIL 0.** `D4-DONE`(전체 역사 대전략 엔진 최종 인수)과는 별개다.  
> 최종 코드 검증 커밋: `a686dfffe6dc10eb5953e909169918f1f12ea655`  
> [GitHub Actions #41725230435](https://github.com/kimjeon-il/VIC2.5/actions/runs/41725230435) — Linux/GCC 및 Windows/MSVC 모두 CTest **9/9 PASS**.

## 1. BLOCKED로 남았던 세 항목

| D4 시험 | 현재 | 실제 실행한 검증 범위 |
|---|---|---|
| **D4-T46** | PASS | 동일 가상 세계에서 1,200개월 × 2회, POP 내생 출생·사망·질병·경제생산/시장부족·은행부실·선거/정부·동원/전투·점령·배상/복구 교차 원장 검사 |
| **D4-T47** | PASS | 활성 희소 POP 10만/100만 셀·4개 지역/기업/시장/군부대·월 1,000건 실거래·8개월 반복, 단계별 수량/금융·현역/장비 정합, P50/P95/P99·CPU·RSS 계측 |
| **D4-T48** | PASS | 선행 **47개가 모두 PASS**여야 통과하는 최종 집계 시험 + POP/C-D0/E/P/M/D4 **27개 기준 파일의 Git blob 해시** 및 P 정본 등록·설계 ADR/불변조건/시험 번호 검사 + 저장파일 계약/수치정책 버전·타버전 거부 검증 |

상세 시험 [`tests/test_phase_c.cpp`](../../tests/test_phase_c.cpp)는 **48개 모두 `test.run`**이며 `test.skip` 항목이 없다. 이 48개는 각각 완성형 시스템의 모든 요구사항을 완전하게 입증한다는 뜻이 아니라, 개별 범위의 **실행 가능한 합성 수치·거부/보존 회귀**로 간주한다.

## 2. 실물 코드·출처

- 통합 원자적 월연산: [`include/vic25/phase_c.hpp`](../../include/vic25/phase_c.hpp) — Phase B 인구와 S0 금융·재고·소유권의 단일 월간 staging, 실제 경제·정치·군사 이벤트와 롤백.
- 버전 있는 저장·복구: [`include/vic25/phase_c_snapshot.hpp`](../../include/vic25/phase_c_snapshot.hpp) — 6개 계약 버전과 현재 숫자정책/가상 규칙 버전, 체크섬 검증, 버전 불일치 및 손상된 세이브 거부. 이전 형식의 자동 마이그레이션/저널 영속 리플레이는 아직 없음.
- 실제 정치 정본: [`docs/phase-c/politics/`](politics/README.md) 8개 원문 및 선거·법률 fixture 2개는 [PR #7](https://github.com/kimjeon-il/VIC2.5/pull/7)로 이미 `main`에 병합; 통합 작업 브랜치에도 동일 Git blob을 가져옴.
- 독립 소스/계약 감사: [`contract_baseline.json`](integration/contract_baseline.json), [`verify_phase_c_baseline.py`](../../scripts/verify_phase_c_baseline.py). Windows checkout의 정상 LF→CRLF 변환은 정본 해시 계산 전에 정규화하되, 실제 내용 변화는 실패 처리.
- 장기회귀: [`test_phase_c_population_crisis.cpp`](../../tests/test_phase_c_population_crisis.cpp), [`phase_c_t46_crisis.hpp`](../../tests/phase_c_t46_crisis.hpp). 성능 측정: [`phase_c_t47_scaling.hpp`](../../tests/phase_c_t47_scaling.hpp).
- 통합 엔진 코드는 [Draft PR #6](https://github.com/kimjeon-il/VIC2.5/pull/6) 및 `work/phase-c-engine-integration`에 있으며 **아직 `main`에 병합하지 않음**.

## 3. 실측 성능 예시

최종 코드 검증 환경의 Linux CI:
- CTest **9/9 PASS** (기존 POP+S0, 경제/정치/군사 인수, 1,200개월 회귀 3종, 계약 감사).
- D4 단위 시험 결과 **48 PASS, 0 BLOCKED, 0 FAIL**.
- D4-T47 10만 셀 / 8개월: P50 약 154ms, P95 약 231ms, 이벤트 16,006건, 메모리 최대 약 96,448 KiB.
- D4-T47 100만 셀 / 8개월: P50 약 1.94초, P95 약 2.56초, 이벤트 16,006건, 메모리 최대 약 919,228 KiB.
- CPU/메모리·시간은 GitHub Actions 실행의 특정 자원과 **합성 부하** 기준이며 모든 역사적 제도·대규모 교전·정치적 조직/무역 거래가 섞인 100만 POP의 100년 게임 성능을 증명하지 않는다.
- Windows/MSVC는 CMake/C++20 빌드, 등록 CTest 9/9를 통과했다. macOS/Clang, 멀티워커 전역 원장의 크로스플랫폼 바이트 동일·실제 전원차단 후 영속 저장복구는 이번 인수 범위 밖이다.

## 4. 전체 게임 최종 인수와의 차이

**D4-T48 PASS ≠ D4-DONE.** T48은 *설계 정본 등록·계약 버전 잠금 및 앞선 47개 합성 시험의 증빙*이 완료됐다는 뜻이다. 초기 설계의 모든 C-E1~E5, C-P1~P5, C-M1~M5를 실제로 구현하고 역사 자료로 검증했다는 판정이 아니다.

추가 구현·검증이 남아 있는 대표 영역:
- 경제: 다양한 생산 레시피/가격시장 완전청산, 역사적 금융제도·부도/유동성·대출, 국가별 관세·다중통화와 결제·운송망, 국민계정/기업 투자·도산.
- 정치: 실제 선거구·FPTP/양원제/다양한 헌법·사법/행정·선호 분포/정당과 정치운동·혁명/내전 등 범국가 모델.
- 군사: 시대별 무기·부대 편제·훈련·전투확률/작전·포로/의료·전체 군수망/국제전쟁·평화조약/민간 피해 등의 고도화.
- 인수/운영: 모든 CE 24, CP 25, CM 30 개별 인수항목의 정식 실행 구현, 전원차단·규칙/세이브 마이그레이션, 병렬 worker 결정론, 역사적 모수·대규모 장기 성능.

따라서 **현재 완료 판정은 `D4 48개 프로토타입 인수시험 PASS`이며 `D4-DONE/완성형 전략게임`은 미완료**다. 이를 숨기지 않기 위해 기능 PR은 Draft를 유지한다.
