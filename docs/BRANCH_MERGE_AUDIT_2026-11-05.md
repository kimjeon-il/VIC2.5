# VIC2.5 작업 브랜치 전수 병합 감사 — 2026-11-05

## 범위와 충돌 해결
- main 외 작업 브랜치 19개(이번 조사 기준)를 검토. 합본 메인 코드 정본은 work/phase-i-j-detail의 최신 C++ C/D/E/G/H·I/J 계약·자원 유량 교체.
- 이전에 없던 Phase D 상세 D-D0, D-C1~5, D-I1~5, D-T1~5, D-V 명세/fixture 50개, Phase E EV-T10 30k GIS/벤치 파일 7개, Phase G/H 238개 검수 설정/runner/registry 3개를 합류.
- Phase E 30k 개선 중 겹치는 include/vic25/phase_e_gl.hpp, phase_e_raster.hpp, tests/test_phase_e.cpp, test_phase_e_gpu.cpp를 최신 EV-T10 검증 변경으로 선택; GH238 시험 tests/test_phase_gh_238.cpp 최신 수정 포함. CMake에 EV-T10 10k/20k/30k 성능 타깃 등록.
- 중복된 C-E/P/M 및 POP 브랜치는 최신 하류 코드와 상위 contract를 유지한다. Phase D 구 매장량·재생/채굴권 설계의 지하 'remaining'과 D-I2 confirmed_extractable_stock은 최신 J-R1로 완전히 대체하고, 기존 금융·상품·인구 원장은 보존한다. 계약 매니페스트의 변경된 D-D0/D-I2 SHA도 갱신.
- Phase I-C1 최신 개정: 언어는 문화 하위정보, 종교는 유효 법률·실제 기관/집행을 통해 조건부로 사회에 영향. 독립 언어 숙련/상시 개종 엔진 금지.
- 기존 AGENTS 개발 철학·main의 원본 파일도 함께 포함하고, 파일 충돌에서 예전 설계가 최신 코드를 덮어쓰지 않게 함.

## 확인한 19개 비-main 브랜치
work/phase-c-d4-validation, work/phase-c-design, work/phase-c-economy, work/phase-c-engine-integration, work/phase-c-m1-m5, work/phase-c-p1-p5, work/phase-d-cpp-integration, work/phase-d-design, work/phase-e-cpp-integration, work/phase-e-design, work/phase-e-ev10-30k, work/phase-e-v-remaining, work/phase-g-h-d0, work/phase-gh-238-acceptance, work/phase-i-j-d0, work/phase-i-j-detail, work/politics-contract-registration, work/pop-d0-d3, work/pop-phase-b.

## 구현/검증 정확도
- Phase I 및 J 전 부문의 상세 계약은 확정됐지만 사회·문화·교육·지질 탐사·오염·자연재해 완전 C++ 엔진은 미구현이다.
- 새 자원 흐름 엔진만 제한적 Phase D C++ 실행/원자 저장·복구로 교체. 이전 Phase D 구 세이브는 자동 마이그레이션 없이 거부.
- G/H 238개는 실행 가능한 C++ 인수시험과 명시적 BLOCKED 항목을 구분하는 매니페스트로 관리. BLOCKED를 PASS로 세지 않는다.
- EV-T10 30k 성능과 실제 전 세계 고증된 역사 국경 데이터 구축은 다른 단계다. 합성 30k 테스트만으로 GIS 역사 정확도를 확정할 수 없다.
- 최종 main 반영 전에는 임시 통합 브랜치에서 Linux/Windows 전체 CTest, 스키마/원장 검사를 실행한 결과를 확인한다.
