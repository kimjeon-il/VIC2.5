# Phase E — EV-T10~T60 잔여 구현 보고서

> 2026-11-04 · work/phase-e-v-remaining · PARTIAL_IMPLEMENTATION / EV-COMPLETE=NO

## 범위와 제한

- 프로빈스 약 10,000개를 역사적 국경·행정경계 기준으로 나누는 것은 **설계만** 작성. 실제 전 세계 GIS 지오메트리 구축 및 국경 교차 검증(EV-T10)은 BLOCKED.
- 설계: [EV-T10 약 1만 프로빈스 역사 경계 계획](EV-T10_PROVINCE_10K_HISTORICAL_BORDER_PLAN.md)
- 기존 C/D 쓰기 원장 및 월별 12단계·롤백을 재사용, 프로빈스 ID와 정적 관계만 유지. Phase B 인구/Phase C 금융·재고·정치/Phase D 건설·산업·기술 원본 복제 금지.

## 잔여 항목을 실제 코드와 연결한 내용

| ID | 기능 | 적용 |
|---|---|---|
| EV-T17 | 독립국 성립 | Phase C GovernmentState 생성과 E 주권·기본 행정구역을 stage0 하나의 atomic 변경으로 연결 |
| EV-T32 | 선거구 중복 | C real POP voter eligibility/anchor를 E의 정수 선거구 POP 배분과 연결, 중복등록 및 초과거부 |
| EV-T37 | 평균임금 | Phase B 고용계약 원본에서 근로인구 가중평균 반환 (실지급 금융임금과 구별) |
| EV-T39 | 선거중 재획정 | 선거 경계 snapshot 동결 및 후속 행정구역 변경으로 기존 투표 데이터 소급 변경 불허 |
| EV-T40 | 프로빈스 내부 선거구 | 개별 프로빈스를 지도에서 나누지 않고 POP integer partition만 지원; 없는 지리경계 조작 금지 |
| EV-T43 | 작은 섬 | 낮은 LOD에서 소실된 Province의 지리좌표 마커·정확 ID 기반 선택 보조 |
| EV-T45 | 표시 레이어 | 국가·행정·점령 지도 모드 읽기 전용 전환 및 실제 GL GPU palette test |
| EV-T49 | GPU 리소스 복구 | 실제 OpenGL3.3 context destroy/recreate + GLSL/VBO/정수ID raster/LUT 재구성; OS TDR 미실행 |
| EV-T53 | 시장·수송 | C Shipment·Inventory·MarketQuote를 territorial cession와 분리; E 지리 연결 및 실물 재고 이전 오작동 거부 |
| EV-T57 | 결정론 | 1/4 actual std::thread immutable 공간명령 preflight + canonical stage9 병합, same C/D/E snapshot hash |
| EV-T59 | 대규모 부하 | 1만/10만 Province×10만/100만 POP 합성 워크로드, P50/P95/P99, peak RSS, 단계 경계 시간 측정 |
| EV-T60 | 최종 인수 | scripts/verify_phase_e_gate.py + CMake vic25_phase_e_release_gate, 불완전한 선행시험·자료증거이면 명시 BLOCKED |

## Linux 실제 증빙

- [GitHub Actions #41814202292](https://github.com/kimjeon-il/VIC2.5/actions/runs/41814202292) — Ubuntu/GCC CTest **22/22 PASS** (기존 C/D 회귀 포함), Phase E 기본 **56 PASS / 4 BLOCKED / 0 FAIL**.
- 기본 BLOCKED: EV-T10(역사 GIS 설계만), EV-T49(GPU 별도 실행 증명), EV-T59(부하 별도 실행/실저사양 기준 미완료), EV-T60(엄격 집계 게이트).
- 별도 GL/Mesa 실행: GPU_ACTUAL_GL33_RENDER PASS, EV-T45_GPU_LAYERS PASS, EV-T49_CONTEXT_RECREATE PASS (OS-level 강제 GPU reset은 포함하지 않음).
- 10,000 Provinces + 100,000 active POP cells / 4개월: **P50 342.403ms**, **P95 374.828ms**, **P99 378.319ms**, peak RSS **79.387 MiB**.
- 100,000 Provinces + 1,000,000 active POP cells / 3개월: **P50 4052.010ms**, **P95 4370.145ms**, **P99 4398.423ms**, peak RSS **756.938 MiB**.
- 표본 기간과 GitHub runner는 실제 역사 데이터나 목표 저사양 PC가 아니며 Stage 시간은 callback 경계 근사값. GPU memory·시스템 전체 장기 1200개월 복합 부하/실제 선거·국제 무역/외교도 검증되지 않았음.

## Windows

- 최초 CI에서 windows.h min macro가 std::min과 충돌했으나 NOMINMAX 적용 후 재빌드 통과.
- Windows Server 2022 / MSVC CTest **20/20 PASS**. Phase E 세부 56 PASS / 4 BLOCKED / 0 FAIL.
- 10,000 Province + 100,000 POP slice / 4개월: P50 **576.197ms**, P95 **584.756ms**, P99 **585.828ms**, peak RSS **83.727 MiB**. GPU test와 100만 POP case는 Windows job 범위에 포함되지 않음.

## 완료 판정과 남은 책임

- 56개 기본 PASS는 한정된 실행 코드 오라클이며 E-V 전 60개/완전 역사 엔진의 검증을 뜻하지 않음.
- T49는 실제 OpenGL context 재생성을 별도 GPU 시험으로 확인했으나 운영체제 GPU 장치 중단/재시작 전체는 미구현.
- T59는 합성 부하를 검증했으나 명시된 하드웨어 기준 및 실제 GPU FPS·VRAM/역사 시나리오 미완료.
- T60을 완전히 통과시키려면 T10 역사적 실자료 검증이 필요하므로 사용자의 현재 보류 결정과 양립할 수 없음.
- 이전 E-D0·C-D0·D-D0 원본 계약 변경 없이 확장하였으나 E snapshot schema는 phase-e-cpp/0.4로 변경되어 예전 snapshot 자동 마이그레이션은 지원하지 않음.

**상태: 구현 세로슬라이스 확장 및 합성 인수검증, EV-COMPLETE=NO.**
