# VIC2.5 일별 시간 전환 — 실제 구현 현황

> 기준: `work/daily-tick-b-j` · 계약 `vic25-time/2.0` · **PARTIAL_DAILY_IMPLEMENTATION** · B~J 전체 완성판 아님

## 구현 파일

- `include/vic25/daily_time.hpp`: Gregorian day index, 일별 이벤트 명령, 하루 원자적 커밋, 월말 legacy 정산 1회 호출, 무변경 일별 O(1) 메타데이터 fast path.
- `include/vic25/daily_snapshot.hpp`: `vic25-daily-save/1.0` 명시적 세이브 헤더, nested GH snapshot/checksum, 일별 큐·journal·world seed. 구형 C/D/E/GH 실제 codec별 `Migrator` 및 Phase B 메모리 인구 상태 이전.
- `phase_c.hpp`: `advance_daily`는 원본 C Financial/Inventory/Jurisdiction의 허용된 사건만 처리, 기존 월별 자동 출생·시장·임금·계약 정산은 실행하지 않음.
- `phase_d.hpp` / `phase_e.hpp` / `phase_gh.hpp`: 단계별 일별 C ledger 트랜잭션 전달과 기존 바깥 sidecar 불변식 검사. 월간 기존 동작은 그대로 유지.
- `tests/test_daily_time.cpp` Gregorian 윤년·월말/연말·실제 현금·재고·점령·idempotency·월말 롤백·구형 checkpoint 및 새 일별 저장·복원·재현성·365일 주기 증명.
- `.github/workflows/daily-tick-integration.yml`: Linux GCC 및 Windows MSVC 실제 빌드·테스트.

## 명시적 비지원/향후 구현

1. Phase B의 출생·사망·노화·교육/이동의 **일별 확률계수·carry** 모델 (현재 monthly ppm만 월말 1회).
2. Phase C/E/G/H의 모든 정치·군사·행정·외교·무역 명령을 일별 효력으로 이식 (현재 C payment, trade, military_control만 whitelisted).
3. Phase D의 일별 시공·생산·기술 발견·WIP/준공/고용 정산 (현재 12단계 월말 호환).
4. Phase F 실제 일별 logistics engine 및 독립 snapshot은 미구현. `docs/phase-f/F-TIME_INTERFACE.md`는 계약만.
5. Phase I/J는 현재 **설계 중심**. 실제 일별 문화·교육·환경·자원 시뮬레이션 및 세이브 migration은 없으며, J 지하자원 총매장량 모델은 사용하지 않음.
6. 과거 Phase B 단독의 표준 serialized save format은 존재하지 않아 binary migration은 미지원. 메모리상 Phase B registry는 POP 인구를 바꾸지 않고 이주할 수 있음.
7. 월별 기존 실제 시뮬레이션 결과를 30배/31배 발생시키거나, 과거 월간 checkpoint를 월중 임의 날짜로 분할해 정확하다고 주장하지 않는다.
8. 실물 30k Province와 1m POP, 일별 COW·full parallelism/GUI FPS, 모든 역사 시나리오에 대한 전세계 부하는 아직 검증되지 않음.

## 계약 버전과 마이그레이션

- `ADR-TIME-001`: 일별 시간의 정의/기존 원장과의 관계 및 각 Phase B~J 도입 범위.
- `ADR-TIME-002`: 구형 인코딩된 월별 C/D/E/G/H snapshot을 날짜가 있는 일별 wrapper로 승격, 최초 날짜는 `month_index`에 대응하는 다음 처리월의 1일. 기존 GH payload 재인코딩 hash 보존.
- 신규 저장 `VIC25_DAILY_SNAPSHOT_1` + `vic25-daily-save/1.0` + Gregorian contract hash; 새로운 스냅샷과 구형 파일 혼용 금지.
- Legacy B~H core snapshot contract 번호 자체를 임의 재기록하지 않음. 시간 부분은 v2 상위 ADR이 우선하고 정기 결산 호환 경로로 보호.
- 테스트 PASS는 기존 월간 원장 및 신규 일별 사건에 대한 **한정된 합성 fixture**이며 일별 full-game 완료가 아니다.

## 검증

- [Daily Tick CI](https://github.com/kimjeon-il/VIC2.5/actions/workflows/daily-tick-integration.yml)
- Linux/Windows 모두 일별 초기 6개 시험 그룹 빌드·실행 PASS (실제 CI run 41859929268).
- 범분야 POP/C/D/E/G/H 연동 회귀는 별도 CTest와 실측 증거를 확인해 추가할 것.
 
## 오프라인 마이그레이션 CLI

`vic25_daily_migrate <phase-c|phase-d|phase-e|phase-gh> <legacy-input> <new-daily-output> <world-seed>` 명령을 제공한다. 구 파일은 덮어쓰지 않고 새 경로에만 `VIC25_DAILY_SNAPSHOT_1`로 저장하며 저장 직후 재읽기·체크섬·출처/날짜를 검증한다. 비지원 포맷 또는 손상 파일은 정확하게 거부한다.
