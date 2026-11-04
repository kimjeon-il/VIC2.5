# VIC2.5 Phase E — 지리·영토·행정구역

- 설계 계약: phase-e-common/1.0 (E-D0)
- 통합 개발 브랜치: work/phase-e-cpp-integration
- 현재 구현: PARTIAL_IMPLEMENTATION, 실제 E-V 41 bounded PASS / 19 BLOCKED (Linux 2026-11-04)
- 전체 인수 60/60 / 실제 GPU 3D 지구본: NOT_IMPLEMENTED

## 정본 설계
- [E-D0 공통 계약](E-D0_COMMON_CONTRACT.md)
- [E-G1 고정 프로빈스·정적 지리·토폴로지](E-G1_PROVINCES_GEOGRAPHY_TOPOLOGY.md)
- [E-G2 주권·통제·국경](E-G2_SOVEREIGNTY_CONTROL_BORDERS.md)
- [E-G3 동적 행정구역](E-G3_DYNAMIC_ADMINISTRATIVE_DIVISIONS.md)
- [E-G4 목적별 관할·선거구·지방자치](E-G4_SCOPES_ELECTIONS_LOCAL_GOVERNMENT.md)
- [E-G5 래스터 지구본](E-G5_RASTER_GLOBE_RENDERING.md)
- [E-V 60개 통합시험](E-V_INTEGRATION_VALIDATION.md)
- [실제 C++ 구현·CI·미구현 현황](ENGINE_IMPLEMENTATION_STATUS.md)

## 실제 C++
- include/vic25/phase_e.hpp
- include/vic25/phase_e_snapshot.hpp
- include/vic25/phase_c.hpp (stage0 sovereignty adapter)
- tests/test_phase_e.cpp

Phase B POP/Phase C 경제·정치·군사/Phase D 건설·산업·기술이 소유하는 원본은 Phase E에서 복제하지 않는다. E 공간 객체는 C-D0 12단계 월간 원자적 커밋과 D-D0 자원원장을 존중한다.
설계문서의 E-V 60개 전항목을 구현했다고 오인하지 말 것. 이 브랜치는 이전 Phase D C++ 구현 브랜치 위에 정본 E 설계문서를 이식한 부분 구현이다.
