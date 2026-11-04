# ADR-TIME-003 — Phase B/C/D/G/H 네이티브 일별 사건 및 호환 엔진

> 2026-11-05 · `vic25-time/2.1-native-slice` · **PARTIAL IMPLEMENTATION**, 기존 `ADR-TIME-001`/002의 일별 틱 및 Legacy MonthClose 계약을 대체하지 않고 확장.
> 코드: `work/daily-native-b-h`, `include/vic25/daily_time.hpp`, `phase_c.hpp`, `phase_d.hpp`, `phase_e.hpp`, `phase_gh.hpp`.

## 실행 계약과 일별 원본

일별 D0~D7 스케줄은 기존 12개 C/D/E/G/H *논리 단계*의 실제 source-of-truth 이벤트를 호출한다. 필수 자원·합법성·원자성을 검증한 뒤 하나의 `DayCommit`으로 발표한다. 한달의 `monthly close`는 달력상 마지막 날 **한 번**만 실행한다. 플레이어 배속은 날짜 진행 간격만 조절하고 정본 결과에 관여하지 않는다.

| 분야 | 현재 실제 일별 처리 | 유효일·보존 계약 | 미완료 |
|---|---|---|---|
| B POP | `Kind::pop_birth`, `pop_death` → C->Phase B `birth/death` | 이벤트는 특정 `DayIndex`로 저널링, 실제 인원·고용을 Phase B에서 증감 | 자연출산·사망의 독립 **자동 일별 확률** 및 코호트의 정확한 출생일 `birth_day` 미구현. 기존 POP `birth_month` 보존. C의 월간 demography가 활성이라면 월말 확률도 기존대로 1회만 적용하며 자동/명시 사건의 중복 원인은 추후 분리 |
| C/D 생산 | `production`은 실제 C 원재료 소비→완제품 생산, `manufacture`는 D Recipe/시설·실노동·재료·부산물·재고 원장 | Stage4 예약, 공유 노동·시설 예산, 월간 실행을 하루마다 자동 반복하지 않음 | 산업의 전면적 자율운영/시장청산·수익성 기반 생산 AI, 모든 월간 CAPACITY→일간 환산 미구현 |
| D 건설 | `construction`은 실제 계획·시공·지급·WIP 자본화. 일별 승인/완공 후 다음 틱 실물 시설 준공 | 공유 Phase C 금융/재고, D ConstructionProject; 동원된 노동·자재·현금 초과 사용 거부 | Package curing day/inspection/기술채택·프로젝트 자동작업계획의 포괄적 일별화 미구현 |
| G/F 물류 | `cargo_dispatch`은 실제 C 실재고를 `TRANSIT:DAY:ID`에 이동, 지리적 실제 G TransportEdge 월간 누적 한도를 일별 유효량으로 검증하고 정확한 `arrival day` Stage0에 실제 인도/확정 분실 | C Inventory 원본은 유일. G `edge_usage[month,edge]`는 기존 월간 사용량과 공유, 월간 limit 두 번 할당 금지. Day Cargo는 위치·ETA·손실 메타데이터만 소유 | F 완성형 네트워크/동적 경로·운송용량·일간 요금/보험·다중 구간 경로선택 미구현 |
| H 외교 | `treaty_offer`, `treaty_ratify` → Phase H `DailyTreaty`의 당사자·실제 정부 권한·정확한 발효일. 발효 조건 충족 시 다음 Stage0 자동 활성 | H 내부 원본 `DiplomacyState`를 사용하고 G 무역관세 면제를 동적으로 조회. H 기존 `effective_month` 조약 객체를 임의로 日로 환산하지 않음 | 완전한 다자조약·국제협력체/국제위기·세부 모든 조항의 일별 집행/정산은 추후 |
| I/J | 설계 계약의 입력·출력·상태변경 기준만 유지 | 원본 정본 추가 없음 | 실제 독립 I/J C++ 런타임과 일별 환경/자원·교육/문화 업데이트 미구현 |

## 스케줄·우선순위

- 일반 명령: `Command{id,kind,from,to,region,good,amount,unit_price,destination,auxiliary,route_id}`. 유일 ID, `effective_day>=today`만 등록. 이 스키마의 `unit_price`는 종류별로 단가/필요 노동/운송 소요일, `auxiliary`는 실제 사망 고용·수송 손실·조약 효력 지연 등 명시적으로 해석한다. 향후 도메인별 타입 안전 variant 분리 필요.
- 분산된 사건은 **stage + command ID** 순으로 정렬, 자원 예약과 일간 원장 검증 실패 시 C/D/E/G/H/당일 날짜·큐·cargo 전부 롤백.
- Stage0 이전일 도착 화물을 실물 재고로 반영; stage1 건설 자금 예약, stage4 실노동·자재·설비 이용, stage5 시공 지급, stage7 운송 dispatch, stage8 WIP, stage9 H 의결, stage10 POP 전이; stage11 cross-domain invariant.
- H 합의 발효·D 시설 완공이 다음 날 예정인 경우, **명령이 없는 날에도 해당 due 이벤트를 처리**. 평시 무변경일은 고액 POP/30k Province deep-copy 없이 일별 날짜·커밋만 갱신한다.
- 월말의 legacy C/D/E/GH 정산이 동일 날짜 일별 사건과 함께 실패하면 **전체 당일 롤백**. 실제 월간 경제·출생·무역 계산을 28~31회 일괄 반복하지 않는다.

## 저장·호환성

- 신규 일별 저장 `vic25-daily-save/2.0`: 추가 `Command.destination/auxiliary/route_id`, 실물 C 재고를 참조하는 `DailyCargo` 인도 메타·도착일·손실·원본 edge. `vic25-daily-save/1.0`는 **명시적 decode**로 읽고 새 버전으로 재저장 가능. 근거 없는 개별 월중 날짜 생성 금지.
- H `phase-gh-cpp/0.3`: `DiplomacyState.daily_treaties`가 실제 존재할 때만 사용. 구 `phase-gh-cpp/0.2`를 그대로 읽고 legacy reencode는 v0.2 byte-identical. 기존 GH/H Treaty는 월별 효력을 유지.
- C/D/E의 원본 인구·회계·주권/정적 영토를 이중으로 직렬화하지 않는다. 중간 Stage 저장은 금지.
- 소프트웨어 버전/자료셋/GPU 안티앨리어싱 등은 세계 원장 해시와 분리; 실제 당일 사건·정본 수치의 결론은 결정론적으로 일치해야 한다.

## 수락 게이트 (합성 세로슬라이스)

1. 같은 날짜의 POP 실제 인구 증감과 날짜별 원본 사건 ID.
2. C 및 D 실제 생산의 원자적 원자재·시설·고용 보존.
3. D 프로젝트의 일별 누적 progress·시공대금·WIP 보존, 완공→다음 날 준공.
4. 실제 수송 네트워크 G 간선, G 월간 물류한도와 같은 자원 예산, in_transit 고유 ID, 월경계 도착/사고, 세이브 복구.
5. H 정확한 발효일·적법 정부 비준, 동일 조약 중복 거부, 월간 G 관세 면제 연계.
6. 각 분야에서 부정한 사건 발생 시 day/월말 전체 commit rollback.
7. Daily save/1→/2 업그레이드, GH save 0.2→0.3 비파괴 decode, 동일 날짜 replay.
8. Linux GCC/Windows MSVC CTest 및 기존 B/C/D/E/GH 대표 회귀 유지.

통합 수락이 8개 항목의 합성 fixture를 통과하더라도 **FULL_B_J_NATIVE_DAILY=NO**: 30k 프로빈스 고부하·1m POP 일별 자동 코호트·일별 가격/공급망/사회/환경·실제 역사 시나리오 검증은 별도.
