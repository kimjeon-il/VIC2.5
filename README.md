> **2026-11-07 후속 일간 자동화(검증 생략):** [가계 자동 소비·임금·은행 일별 이자·기본 국가 AI·운송경로·성능 개선 보고서](docs/time/DAILY_FOLLOWUP_2026-11-07.md). 모든 변경사항 **UNVERIFIED / 부분 구현 / main 미병합**.

> **2026-11-07 일간 엔진 단순화 작업 브랜치 (검증 생략):** [6단계 변경·잔여 작업 기록](docs/time/DAILY_SIMPLIFICATION_2026-11-07.md) — 일별 용량·국고·네이티브 화물·자원재생·기초 자율 기업 연결. **컴파일/CTest/CI 미실행, 부분 구현, main 미병합.**

> **작업 브랜치 통합(2026-11-05):** [전체 브랜치 병합 감사](docs/BRANCH_MERGE_AUDIT_2026-11-05.md) — Phase B~H C++ 최신 구현, Phase D/E 전체 설계, Phase E 30k·역사지리 시험, G/H 238개 개별 게이트, 최신 Phase I/J 및 지하자원 매장량 없는 J-R1. *완성형 전체 인수 통과를 의미하지 않으며* BLOCKED는 PASS가 아니다.

> **Phase C 통합 프로토타입 C-D4 인수 현황(2026-11-04):** [실제 구현 상태·48개 시험 결과](docs/phase-c/ENGINE_IMPLEMENTATION_STATUS.md) — T46·T47·T48 포함 **48개 합성시험 PASS**, Linux/Windows CTest 9개 PASS. **통합 코드는 `work/phase-c-engine-integration` Draft PR #6에 있고 main 미병합**, CE/CP/CM 전체 기능 구현·역사적 보정 및 D4-DONE은 미완료입니다.

> **Phase C-D4 통합 검증 설계 (phase-c-integration/1.0)**: [docs/phase-c/integration/README.md](docs/phase-c/integration/README.md)에서 POP·경제·정치·군사 통합 World, 30개 불변조건·48개 교차 시험·월별 12단계·저장/복구/100년 장기 시험과 실제 구현 게이트를 확인할 수 있습니다. **설계 문서 확정이며 C++ 통합 엔진/장기 회귀는 미실행**입니다. 정치 C-P 설계 GitHub 정본 등록·Phase B 원장 통합이 선행조건입니다.

> **Phase C-M1~M5 군사 상세 설계 (phase-c-military/1.0)**: [docs/phase-c/military/README.md](docs/phase-c/military/README.md)에서 병역·동원·예비군, 부대·장비·훈련, 군수·보급, 작전·사상자, 전쟁·점령·종전 및 [군사 인수시험](docs/phase-c/military/MILITARY_ACCEPTANCE_TESTS.md)을 확인할 수 있습니다. **설계 완료이며 전체 C++ 통합 구현·전쟁 시뮬레이션 회귀 전**입니다.

> **Phase C-E1~E5 경제 상세 설계 (phase-c-economy/1.0)**: [docs/phase-c/economy/README.md](docs/phase-c/economy/README.md)에서 기업·노동·생산, 상품·시장·가격, 가계·금융·투자, 조세·정부재정, 운송·무역의 구조·연산 규약과 [경제 인수시험](docs/phase-c/economy/ECONOMIC_ACCEPTANCE_TESTS.md)을 확인할 수 있습니다. **설계 완료이며 전체 C++20 경제 엔진 구현·검증 전**입니다.

> **Phase C-D0 공통 설계 계약 (phase-c-common/1.0)**: [docs/phase-c/README.md](docs/phase-c/README.md)에서 경제·정치·군사의 공통 객체·원장 소유권·월별 12단계·정책 효력·군사 배속·통합 사건/롤백 규격을 확인할 수 있습니다. **설계 확정이며 전체 C++ 통합 구현/장기 검증이 끝난 것은 아닙니다.**

> **신규 POP-D0~D3 설계 계약 (pop-design/1.1)**: [docs/pop/README.md](docs/pop/README.md)에서 인구 코호트·경제 POP·가구 풀/희소 PopulationSlice, 전이 이벤트, 월별 원자적 실행 규칙과 구현 인수조건을 확인할 수 있습니다. **설계 문서 완료이며 C++20 실물 엔진 이식/장기 검증은 아직 미실행입니다.** 기존 README 아래의 자료구조 설명은 초기 골격 기준입니다.

# VIC2.5 — POP 중심 역사 대전략 시뮬레이션

빅토리아 3의 POP 중심 *설계 원칙*을 참고하되, 실물 재고·복식부기·다양한 인구 상태를 독자적으로 구현하는 C++20 역사 대전략 게임 프로젝트입니다. 빅토리아 2는 제한적인 비교 자료로만 활용합니다. Paradox의 게임 코드 또는 데이터 파일을 복제하지 않습니다.

## 현재 상태 (2026-11-03)

**S0-POP v1.0 설계 규약 확정 / 신규 검증용 코드 골격**입니다. 완성된 경제 엔진이 아닙니다.

- 설계: POP 기본 모델, 성별·출생월·건강·장애/부상·복무 코호트, 고용·소유·재고·회계 원장, 월별 12단계 규약.
- 신규 코드: 정수 인구 코호트 분할/이전, 전투 사상자 상태 전이, 간이 복식부기 원장, 실물 재고 원장, 소유지분·고용 기록, 12단계 파이프라인 인터페이스.
- 테스트: 인구 보존 및 전투 피해, 금융 분개 균형, 재고·지분 보존, 처리 단계 순서에 대한 **단위/스모크 테스트**.
- 미구현: 실제 기업 생산·상품 거래 청산·가격·대출 의사결정·소비·생활수준·인구 증감·정치/군사 시뮬레이션. 이 단계의 코드는 연산 규약을 위한 시작점입니다.
- **주의:** 이전 대화에서 언급한 S0 v0.1/v0.2 ZIP과 100년 실행 결과 원본은 이 저장소 생성 당시 접근 가능한 파일로 제공되지 않아 업로드하지 않았습니다. 신규 코드를 기존 v0.2 복원본으로 해석하면 안 됩니다.

## 빌드 및 테스트

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/vic25_demo
```

CMake 3.20+, C++20 컴파일러가 필요합니다. Linux/GCC 14 환경에서 빌드·CTest 1개가 통과했습니다. 다른 운영체제의 빌드/성능 검증은 별도입니다.

## 문서

- [S0 범위와 확인된 이력](docs/S0_STATUS.md)
- [구조 및 소스 오브 트루스](docs/ARCHITECTURE.md)
- [POP·성별·연령·건강·장애·부상](docs/POP_MODEL.md)
- [관계 원장·회계·재고](docs/LEDGERS.md)
- [월별 12단계 및 계산식](docs/MONTHLY_RULES.md)
- [테스트 및 불변조건](docs/TEST_PLAN.md)
- [구현 단계](docs/ROADMAP.md)
- [참고 문헌과 인용 한계](docs/REFERENCES.md)

## 개발 원칙

1. POP는 노동·소비·소유·납세·사회·정치의 기반이며, 기업/시장/금융/정부는 독립적인 상태와 책임을 가진다.
2. 금전 거래는 경제주체별 복식부기로, 재고 이동은 물리적 수량 원장으로 기록한다.
3. 직업·소유자·경제적 계급·법적 지위는 같은 개념이 아니다.
4. 성별·연령·건강·장애·부상은 POP의 실제 인구 구성에 반영한다. 장애 자체를 일률적 실업·무능력으로 처리하지 않는다.
5. 동일 초기 상태·규칙·시드라면 같은 결과를 얻어야 한다. 장기 침체와 회계 오류는 구별해서 검사한다.

저장소 이름 `VIC2.5`는 프로젝트의 명칭이며, Victoria 2 게임 코드 사용이나 공식 Paradox 프로젝트를 뜻하지 않습니다.
