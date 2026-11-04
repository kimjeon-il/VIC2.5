# Phase D C++ 통합 엔진 — 구현 및 실측 인수 현황

> **2026-11-04 신규 I/J 작업 브랜치 주의:** 아래는 과거 Phase D 원본 구현/검증 보고서다. `work/phase-i-j-detail`에서는 자원 모델 `ResourceDeposit.remaining`을 `ResourceSite.monthly_capacity/cumulative_output` 및 재생 생물량으로 **교체**하고 Phase D 세이브를 `phase-d-cpp/0.6`으로 올렸다. 아래의 이전 CI 실행 결과는 이 신규 자원 모델을 검증하지 않았으며, 새로운 회귀는 별도 CI가 필요하다. 근거: [J-R1](../phase-j/J-R1_RESOURCE_EXPLORATION_FLOW.md) / [교체 내역](../phase-j/RESOURCE_MODEL_REPLACEMENT.md).

> **상태: PARTIAL_IMPLEMENTATION — 제한적 C++ 통합 세로슬라이스 구현 및 실제 CI 검증 완료. 전체 D-V 인수 미완료.**  
> 2026-11-04 · 실제 기능 검증 커밋: `d624f2014d8b5cf4f87b56b371c749eabab07e05`  
> 개발 브랜치: `work/phase-d-cpp-integration` · 기준 코드: `work/phase-c-engine-integration` (Draft PR #6)  
> Phase D 상세 설계 정본: [`work/phase-d-design`](https://github.com/kimjeon-il/VIC2.5/tree/work/phase-d-design/docs/phase-d)  
> 구현 PR: [#8 — Draft](https://github.com/kimjeon-il/VIC2.5/pull/8)

## 1. 검증된 실행 결과

[GitHub Actions #41762505400](https://github.com/kimjeon-il/VIC2.5/actions/runs/41762505400) — 구현 커밋 `d624f201...`에서 **Linux/GCC 및 Windows/MSVC 모두 CTest 12/12 PASS, 0 FAIL**, C++ 빌드 PASS.

| 시험 | Linux | Windows | 실제 검증 영역 |
|---|---|---|---|
| 기존 Phase C 및 POP/S0 CTest 9개 | 9/9 PASS | 9/9 PASS | 기존 C-D4 48 합성시험 집계 및 POP/금융·재고·정치/군사·장기·소스 baseline 회귀 |
| `vic25_phase_d_tests` | PASS | PASS | 11개의 한정적 건설·산업·연구/기술도입·오류 거부/전역 롤백·소규모 결정론 시나리오 |
| `vic25_phase_d_snapshot_tests` | PASS | PASS | 실제 Phase C+Phase D 스냅샷 인코딩·디코딩, 파일 저장/재로드, 동일 재생, 손상/계약 불일치 거부, 오류 후 불변 |
| `vic25_phase_d_longrun_tests` | PASS | PASS | 소규모 1,200개월×2회, 500회 산업 실제 생산, 공사·연구/지식·차월 준공/군 시설 피해·복구·중간 저장재생 |
| **CTest 합계** | **12/12** | **12/12** | 9개 기존 + 3개 신규 CTest 타깃 |

이 결과는 해당 커밋의 **합성 테스트 범위**에 한정된다. `D-V 72개 신설 인수시험 전부` 및 `DD0 24 + DC45 + DI45 + DT45 + C-D4 48 = 207`을 **모두 같은 완성형 Phase D 엔진으로 전수 재구현/재검증**한 것이 아니다. `279/279 PASS`나 `D-V COMPLETE`를 주장하지 않는다.

## 2. 실제 구현된 C++ 모듈

### 공통 C++ 실행 경로

- [`include/vic25/phase_d.hpp`](../../include/vic25/phase_d.hpp): Phase D의 건설·산업·연구·지식/도입 데이터와 `Stage` API, 기존 Phase C의 12단계 `IntegratedWorld::advance`에 직접 묶은 `World::advance`. 이 함수는 Phase C/Phase D 상태를 모두 같은 월 단위로 스테이징한 뒤 성공시에만 함께 커밋한다. 오류시 인구·고용/금융/재고/시설/소유·연구/공사/예약 상태를 함께 롤백.
- [`include/vic25/phase_c.hpp`](../../include/vic25/phase_c.hpp): C-D4 실제 금융·재고/고용·소유 원장에 대한 단계 제한 API 추가. 실제 자재/노동을 요구하는 제조/시공/연구 투입, 건설대금의 실제 지급 비용→`WIP` 자본화, 전월 말 `ready` 시설의 다음 달 0단계 설치/가동·물권·고정자산 재분류.
- [`include/vic25/phase_d_snapshot.hpp`](../../include/vic25/phase_d_snapshot.hpp): 실제 Phase C core snapshot과 Phase D sidecar state를 함께 기록하는 버전 잠금/체크섬 기반 바이너리 포맷. 중간 파일→원자 파일명 교체 방식, 손상/다른 계약 거부. 실제 복구·재생/결정론 일부 검증. **영속 이벤트 저널의 전원차단 후 전면 crash replay/옛 schema migration은 구현되지 않음.**
- [`tests/test_phase_d.cpp`](../../tests/test_phase_d.cpp), [`tests/test_phase_d_snapshot.cpp`](../../tests/test_phase_d_snapshot.cpp), [`tests/test_phase_d_longrun.cpp`](../../tests/test_phase_d_longrun.cpp) 및 [`CMakeLists.txt`](../../CMakeLists.txt): CI에서 실제 컴파일·실행한 신규 테스트 3개.

### 실제 연결된 분야

1. **건설:** 공사계획→실제 `PopulationSlice` 취업 근로자/시공자 보유 자재→`stage4` 작업 실적→`stage5` 확정 공사대금 지급→`stage8` 이미 지급된 비용을 `WIP`로 자본화→`stage11` 준비/`m+1 stage0` 시설·물권/고정자산 가동. 계약 중간미지급/설계품질/토지권·복잡 계약은 후속 과제.
2. **산업:** 유효 설치 사업장·실제 노동과 원료에 기반한 단순 다중 투입·산출 레시피, 공장 가용 서비스/장비·상품중복 소비 거부, 다음달 건설에 투입 가능한 실제 구매자 재고. `ResourceStock`·부산물/환경·다중통화 시장·완전 C-E1 시설모델은 미구현.
3. **기술:** 실제 연구인력/연구소·실험재와 확보된 공공 연구비로 연구서비스를 생산, 검증된 증거 누적→`stage9` 연구검토→`m+1 stage0` 해당 지식 보유자의 `usable` 활성. 다른 기업/현장에는 자동 확산하지 않으며 지식과 `SiteTechniqueAssignment`을 분리한다. 기존 호환 시설에 대해서만 승인된 다음달 도입. 설비개축·특허/기술이전·POP 훈련 전체는 미구현.
4. **미지원 기능의 안전한 거부:** 실제 공사가 필요한 `needs_retrofit` 기술, 기존 C-E1/C-D4 계약 개정 없이는 사용할 수 없는 동월 비저장 `I2-DISPATCH` 공정은 성공으로 처리하지 않는다.

## 3. 프로토타입의 의미·남은 구조적 격차

| D-V 게이트 | 현재 | 구체적 부족분 |
|---|---|---|
| `DV-L0` | **PARTIAL** | Phase C 코드 기준선 선정/브랜치 생성·기존 9회귀 통과는 완료, Phase D 별도 설계 브랜치와 정본 해시/완전 자료형 통합·E/P/M 전체 호환 감사 미완료 |
| `DV-L1` | **PARTIAL** | Phase C 원장을 실제 사용하는 원자 staging/고용·재고·금융·WIP 연결 구현. 다만 부문별/기존 C 전체를 관통하는 단일 범용 자금/장비/인력 예약 계약, 모든 typed ID·자원권한, 통합 journal 영속화 미완료 |
| `DV-L2` | **PARTIAL** | 제한적 제조/시공·재정/연구/지식 차월 효력·설비가동은 구현. 전체 D-C1~C5, D-I1~I5, D-T1~T5 공정/법/경제/금융·외교/사회/군사/특허·훈련 모든 기능은 미구현 |
| `DV-L3` | **PARTIAL, NOT_ACCEPTED** | 신규 3개 CTest(기능11 시나리오/스냅샷/저밀도 장기) 실제 PASS. D-V 신규 72개·상위 279개 고유 ID / POP·CE/CP/CM 전체 규격을 독립 코드 시험으로 모두 PASS한 것 아님 |
| `DV-L4` | **PARTIAL** | 실제 스냅샷 직렬화/파일 왕복·재생·체크섬/버전 오류 거부, Linux/Windows CI는 PASS. 정규 명령/worker1/N 바이트 동일·저널 crash 복구/세이브 마이그레이션 추가 증거 미확보 |
| `DV-L5` | **PARTIAL, NOT_ACCEPTED** | 실제 작은 세계 1,200개월×2회 PASS. 10만/100만 활성 POP와 복수 공사/산업·연구·군사/금융 충격 대표 복합 부하, 상세 성능 P50/P95/P99·메모리 실측/역사 계수 보정 미실시 |
| `DV-COMPLETE` | **NOT_DONE** | L0~L5에 모두 완료 증빙이 없으므로 279/279 전수 합격과 완성형 엔진은 선언 불가 |

## 4. 남은 우선과제

- 기존 Phase C `ProductionFacility(capacity,damaged)`는 제한적 프로토타입이며 완전한 `Firm/ProductionSite/CapitalAsset/ProductionRecipe`와 시대별 역사적 산업 모델이 아직 아니다.
- **모든 영역 공유 자원예약/원장·법률·소유권과 스키마**를 강화하고 Phase C 핵심 내부 전체에서 source_event/typed ID·단일 재원/노동·설비 서비스 예약을 강제해야 한다.
- D-C 미지급 기성·도급/토지권/검수·품질·보험/철거·설비투자, D-I 물리적 ResourceStock/재생·레시피/품질·시장/운송·에너지, D-T 지식접촉/특허·기술자 훈련/POP 교육·군수/보건·연구 불확실성의 진짜 사건 구현 필요.
- R&D는 작은 fixture의 **결정론적 연구증빙 충족 규칙**이며 현실 발명확률/지식 확산·역사계수의 보정이 완료됐다는 뜻이 아니다.
- D-V 72개 교차시험과 DD0/DC/DI/DT 159개, 기존 C-D4 48개, 추가 POP/CE/CP/CM 모든 인수/회귀를 실제 단일 Phase D 엔진에서 구현·수락해야 한다.

## 5. 출처·범위 고정

- 소스 기준선: `work/phase-c-engine-integration` Draft PR #6. 이 브랜치의 기존 Phase C 48 PASS는 제한적 합성 시험이며 `D4-DONE`과 다름.
- Phase D 설계 정본은 `work/phase-d-design`에 있으며 이 구현 브랜치에 자동 병합되지 않음.
- 새 구현은 `work/phase-d-cpp-integration` Draft [PR #8](https://github.com/kimjeon-il/VIC2.5/pull/8). `main` 미병합·기존 Phase C PR 미병합.
- 검증 커밋과 CI: [`d624f2014d8b5cf4f87b56b371c749eabab07e05`](https://github.com/kimjeon-il/VIC2.5/commit/d624f2014d8b5cf4f87b56b371c749eabab07e05), [GitHub Actions #41762505400](https://github.com/kimjeon-il/VIC2.5/actions/runs/41762505400).

**결론:** 제한적 세 분야 C++ 세로슬라이스 및 재현/저장·작은 100년 테스트는 실제 두 플랫폼에서 통과했다. 그러나 `DV-COMPLETE`/279개 전수 인수·실물 역사 엔진 전체 구현은 **미완료**다.
