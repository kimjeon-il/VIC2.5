# Phase E C++ 통합 엔진 구현 현황

> 2026-11-04 · PARTIAL_IMPLEMENTATION / NOT_ACCEPTED
> 브랜치: work/phase-e-cpp-integration (기준: work/phase-d-cpp-integration). 상세 설계는 같은 경로의 E-D0 / E-G1~G5 / E-V.

## 구현 증빙
- include/vic25/phase_c.hpp: 기존 Phase C의 단일 sovereignty 원장에 stage0 주권 이전 adapter 추가. E에서는 중복 writable 주권 원장 금지.
- include/vic25/phase_e.hpp: Phase D World/12-stage staging과 공간 데이터 통합, 정적 Province와 인접성, 영토 이전, 군사점령, 행정구역 create/reassign/rename/reparent/merge, Claims, civil/fiscal authority, 독립 Scope, POP 원본 통계, 경계, CPU integer raster/ray sphere.
- include/vic25/phase_e_raster.hpp: CPU 래스터 마스크 중복/틈 검증, 다중 분리 프로빈스 구간, cube-sphere face/UV/타일키, 타일 pending/loaded 관리, 변경 프로빈스·인접 edge dirty set. 실제 GPU 드라이버/셰이더 구동은 미구현.
- include/vic25/phase_e_snapshot.hpp: Phase B/C/D와 E 상태의 단일 버전/체크섬 스냅샷, 복구/손상 거부.
- tests/test_phase_e.cpp: 60개 EV 번호별 47개 제한적 C++ 실행 테스트, 13개 BLOCKED 명시.
- .github/workflows/phase-e-integration.yml: Ubuntu GCC, Windows MSVC, 전체 기존 B/C/D 회귀 및 Phase E CTest 빌드·실행.

## CI
- [GitHub Actions #41795772492](https://github.com/kimjeon-il/VIC2.5/actions/runs/41795772492)
- Linux GCC: CTest 19/19 PASS; Phase E 실제 47 PASS / 13 BLOCKED / 0 FAIL.
- Windows MSVC: CTest 19/19 PASS; Phase E 실제 47 PASS / 13 BLOCKED / 0 FAIL.
- BLOCKED: EV-T10,17,32,37,39,40,43,45,49,53,57,59,60.
- 주의: 전체 CTest 19개 중 Phase E는 1개 타깃. 내부 41개 bounded check가 E-V 60개 전체 상세 오라클 PASS를 뜻하지 않는다.

## 남은 실제 구현
1. E-G1: 실물 GIS 데이터셋 importer, Province/SeaZone geometry/topology 정확성, 위경도 ±180/극지, 역사적 경계 진단.
2. E-G2: 법적 국가/정부/조약 검증, 실제 역사 시계열, 공동통치와 무주지/미확정 상태, 국가 분열/승계 전체.
3. E-G3: 모든 편제의 시간별 계보, 국별 scheme 변경, 대규모 3천 프로빈스 재편, 권한/지방기관의 실제 승계.
4. E-G4: C-P4 실제 선거구 배분/동일 contest의 1인 1회 집계, 지방자치 실체·법·세금, 비가산 통계 공식 전체.
5. E-G5: 실제 GPU cube-sphere 지구본, LOD/타일 streaming, 국경 AA, 작은 섬/월경지, GPU 복구, 저사양 PC 실측. 현재 CPU 래스터·cube-sphere 수학·타일 로딩 상태와 변경분 추적만 구현.
6. E-V: 13 BLOCKED 실제 기능 구현·테스트, 전체 60개 독립 인수 기준, 10만/100만 POP+실지도 복합 부하, worker 1/N, 저사양 FPS/VRAM, 영속 저널 crash recovery.
7. Phase C/D의 이전 잔여 상세 인수 역시 이 CTest 통과만으로 완료가 아님.

## 상태 구분
DESIGN_SPECIFIED = YES
BOUNDED_IMPLEMENTATION = YES
LINUX_CTEST = 19/19
WINDOWS_CTEST = 19/19
E_V_EXECUTED = 47/60
E_V_BLOCKED = 13/60
EV_COMPLETE = NO
