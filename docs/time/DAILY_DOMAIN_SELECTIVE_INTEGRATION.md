# DAILY-DOMAIN-SELECTIVE — work/daily-domain-conversion × main 기능별 차등 통합

> 상태: CODE INTEGRATED / INDEPENDENT CI PENDING · 2026-11-05
> 비교 기준: 현재 main 3c095d37ee740b8904e122eba136314085b64075 (통합 Linux/Windows CI 성공)
> 대상: work/daily-domain-conversion (기준 분기 2e387eb1c3696ceb8aae0a14f975316f940a3a46)
> 목적: 기존 영역의 구현과 테스트를 후퇴시키지 않고 **한쪽에만 존재하는 기능만 추가**. 전체 B~H 자동 일별 시뮬레이션 완성 주장 아님.

## 1. 기능별 판정

| 기능 | main의 정본 | daily-domain-conversion의 구현 | 이번 통합 |
|---|---|---|---|
| POP 출생/사망 | 실제 Phase B PopulationSlice 일별 변경·명령/저널 | 전염병으로 질병·사망 동시 전이 | **epidemic 명령만 추가**. 출생·사망 원본 유지 |
| POP 건강/역학 | Phase C epidemic(source, ill, deaths) 함수는 이미 존재 | Kind::epidemic 경로로 실제 이벤트 호출 | stage10 기존 원본 연결, 중복 보건 원장 없음 |
| 생산·제조 | C 실물 투입/산출, D 제조공정의 월간 설비용량을 일일 누적 한도로 집행 | C produce, D manufacture 원시 매개 | **중복 제외**, main의 유량/설비 및 노동 한도 유지 |
| 실물 소비 | C consume(owner, region, good, qty)는 존재 | Kind::consumption 명령으로 매일 재고 차감 | **명령 진입점만 추가**, 기존 재고 원장 검산 |
| 건설 | 공사·실제 자금/자재·노동·WIP 및 다음날 준공 | construction_work 명령·공정 | **중복 제외**, main의 준공/검수 경로 보존 |
| 지역간 물류 | G transport edge/월간 누적 수용력, C TRANSIT:DAY, 도착일/손실 | 무경로 DAYTRANSIT 발송/도착 예약 | **후퇴 경로 제외**. route_id 없는 수송은 복원하지 않음 |
| 조약 외교 | 실제 날짜별 H DailyTreaty 승인/발효·관세 면제 | 해당 기능 없음 | main 유지 |
| 관세 직접 변경 | H 월별 tariff 변경만 존재 | stage0 지정일 법적 권한 확인 후 tariff 변경 | **일별 H tariff 반영 추가**; 법적 권한·외교권 위임 검사 및 이전 거래 소급 금지 |
| 저장 형식 | vic25-daily-save/2.0 기록, 명시적 v1.0 이주 | v1.1(추가 destination, 다른 enum Kind 값) | **v1.1의 안전한 subset decode 추가**, v2.0 출력형식/기존 enum 태그 유지 |
| 데이터 규모·안정성 | idle day fast path, 30k synthetic, Linux/Windows 회귀 | 별도 고하중 구현 없음 | main 유지 |

## 2. 일별 명령·계약

- 새 Kind는 기존 코드/세이브에서 사용된 숫자 1..11을 바꾸지 않고 `epidemic=12`, `consumption=13`, `tariff=14`로만 **append-only** 추가한다.
- epidemic은 C `epidemic()`을 stage10에서 호출하며 실제 POP 사망/건강 전이 보존식과 계약·이미 아픈 집단 여부를 확인한다. 월간 자연 출생·사망률을 매일 재실행하지 않는다.
- consumption은 C `consume()`을 stage7에서 호출하며 실제 상품 재고만 차감한다. 소비를 새 시장/가격/가구 원장으로 재구현하지 않는다.
- tariff는 H `adopt_tariff_daily()`를 지정된 날짜 stage0에서 실행. 명령의 `from=importer country`, `to=authorized government head`, `good`, `amount=ppm`을 사용한다. H 권한/국가 정부 책임자/종속국 외교권 제약·기존 관세 원장을 검증하고 **명시적 유효일 이전**에는 효력이 발생하지 않는다. 기존 월별 adopt_tariff와 조약 면제 규칙은 유지한다.
- 모든 변경은 한 날의 C/D/E/G/H 전원장 staged commit으로 원자 확정. 실패 시 날짜·명령 큐·현물/현금·POP·외교 상태를 함께 롤백하고 이벤트 ID 재사용을 거부한다.

## 3. v1.1 스냅샷 호환 한계 (중요)

변환 브랜치 v1.1은 `Command.destination`만 저장했으며, 현재 v2.0은 `destination+auxiliary+route_id`와 `DailyCargo`를 저장한다. 새 스냅샷 저장은 무조건 v2.0이며 기존 바이트 형식과 엔진 정본은 변경하지 않는다.

v1.1 독립 Kind 숫자 해석은 다음과 같이 **명시적으로 변환**한다.

| 변환 v1.1 값 | 의미 | 현재 |
|---|---|---|
| 1,2,3 | payment, trade, military_control | 기존 값 유지 |
| 4 | epidemic | 12 |
| 5 | production | 6 |
| 6 | consumption | 13 |
| 7 | construction_work | 7 (main construction) |
| 8 | manufacture | 8 |
| 9 | tariff | 14 |
| 10,11 | 경로/한도 검증 없는 dispatch/arrival | **자동 마이그레이션 금지**, 명시적 오류 |

과거 10/11번을 무심코 새 treaty_offer/ratify로 읽으면 국제관계가 잘못 변경될 수 있으므로 예약 명령뿐 아니라 완료 저널에서도 거부한다. 무경로 운송의 법적 정본·운송 간선·손실/ETA를 확인할 수 있을 때만 별도 수동 변환이 가능하며, 이번 작업에서는 원본 세이브를 덮어쓰지 않는다. v1.0 디코드 및 v2.0 현재 코덱은 계속 지원한다. v1.1 형식과 독립적인 다른 Phase B~H snapshot 마이그레이션은 변경하지 않는다.

## 4. 병합 대상 제외·잔여 범위

- `dispatch_daily_lot()/arrive_daily_lot()`, `enqueue_shipment()` (무경로 수송), 단순 `manufacture()` 및 `construction_work` 단계 재구현은 현재 main의 상위 계약과 충돌하거나 기능 중복이므로 복사하지 않는다.
- Conversion 브랜치의 `advance_daily()` 단순 오버로드를 덮어쓰지 않음. main의 정확한 일자별 조약 발효·다음날 준공·물류 유효일, 설비 일간 처리량 및 day-copy fast path 유지.
- 기존 B~H 월별 자동 계산이 MonthClose에 남아 있는 상태는 그대로이며, **모든 분야 자동 계산을 실제 일별 연산으로 바꾼 것이 아님**. Phase I/J 전체 엔진은 계속 설계 중심.

## 5. 검증 게이트

- 동일 날짜 소비·전염병 사건으로 POP/재고 실제 값 변화, 월간 자동경제 중복 없음.
- 지정일 전후 관세/정부 책임자 권한·거부 및 전원장 날짜 롤백.
- v1.1에서 열거형 4·6·9 보존 재매핑과 v2.0 재저장/재시작; 10·11 무경로 화물 명시 거부.
- 기존 Phase B/C/D/E/G/H native daily 테스트, 238개 G/H 명시적인 BLOCKED vs PASS, 전체 CTest Linux/Windows, D4 source-lock 검증이 그대로 유지되어야 함.

**성능 게이트:** 기존 30k 합성 지리 테스트를 유지하며 이번 3개 명령 추가만으로 전체 1m POP 자동 일별 모형 성능을 입증했다고 주장하지 않는다.
