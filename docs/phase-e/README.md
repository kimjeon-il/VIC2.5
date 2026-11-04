# VIC2.5 Phase E — 지리·영토·행정구역

- 설계 계약: phase-e-common/1.0 (E-D0).
- 최신 C++ 구현 브랜치: work/phase-e-cpp-integration (후속 검증 work/phase-e-v-remaining에서 수행).
- 최신 합성 실행 상태: EV-T01~60 세부 **56 PASS / 4 BLOCKED / 0 FAIL**. EV-COMPLETE=NO.
- Linux GCC CTest **22/22**, Windows MSVC CTest **20/20**. 실제 GPU GL3.3/context 재생성 시험은 Linux에서만 수행.
- 실제 약 10,000개의 역사적 프로빈스 데이터셋과 국경선 교차는 **설계만**. EV-T10 DESIGN_ONLY.

## 설계 정본
- [E-D0 계약](E-D0_COMMON_CONTRACT.md)
- [E-G1 고정 프로빈스](E-G1_PROVINCES_GEOGRAPHY_TOPOLOGY.md)
- [E-G2 주권·통제](E-G2_SOVEREIGNTY_CONTROL_BORDERS.md)
- [E-G3 동적 행정구역](E-G3_DYNAMIC_ADMINISTRATIVE_DIVISIONS.md)
- [E-G4 선거·자치 공간](E-G4_SCOPES_ELECTIONS_LOCAL_GOVERNMENT.md)
- [E-G5 래스터 지구본](E-G5_RASTER_GLOBE_RENDERING.md)
- [E-V 60개 통합 인수 명세](E-V_INTEGRATION_VALIDATION.md)

## 구현·검증
- [EV-T10 약 1만 프로빈스 역사 국경 분할 설계](EV-T10_PROVINCE_10K_HISTORICAL_BORDER_PLAN.md)
- [EV-T10~T60 후속 구현/범위/합성 성능 결과](EV_REMAINING_IMPLEMENTATION_STATUS.md)
- [E 실제 C++ 개발 현황](ENGINE_IMPLEMENTATION_STATUS.md)
- [Linux/Windows 최신 CI](https://github.com/kimjeon-il/VIC2.5/actions/runs/41814202292)

## 30k 프로빈스 성능 및 GIS 추가 실험

- [EV-T10 실제 역사 경계 소스 검사·30,000개 합성 인수 계약](EV-T10_30K_GEODATA_AND_PERFORMANCE.md)
- `include/vic25/phase_e_compact.hpp`와 `bench/phase_e_30k.cpp`: 압축된 실시간 조회, 국가변경 증분 patch, 불변 렌더 Snapshot. 10k·20k·30k 규모 CTest 지원.
- `scripts/ev10_historical_overlay.py`, `scripts/ev10_history_polygon_import.py`: 별도 Shapely/PROJ 오프라인 GIS 검수. 실제 세계 역사 소스의 전체 정확도와 합성 성능은 다른 인수 기준이다.

## 주요 코드
- include/vic25/phase_e.hpp + phase_e_snapshot.hpp (C/D 단일 월별 스테이징에 통합)
- include/vic25/phase_e_raster.hpp + phase_e_view.hpp + phase_e_gl.hpp (CPU+실제 선택형 GL3.3)
- include/vic25/phase_e_parallel.hpp (다중 worker 공간명령 사전검증)
- tests/test_phase_e.cpp + tests/test_phase_e_gpu.cpp + bench/phase_e_scaling.cpp
- scripts/verify_phase_e_gate.py (T60 최종 수락 거부/통과 판정기)

기존 Phase B POP 및 C/D 분야별 원본은 유지됨. E-V 실제 60/60 완료나 GPU 전체 게임 지구본 구현이 완료되었다고 주장하지 않음.
