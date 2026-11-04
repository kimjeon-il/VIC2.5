# ADR-TIME-001 — B~J 월별 실행에서 일별 실시간 틱으로 전환

- 상태: **ACCEPTED FOR DESIGN; IMPLEMENTATION IS A TRANSITIONAL VERTICAL SLICE**
- 승인 기준: 일별 실시간 진행, 월별·일별 분야별 계산, 기존 B~J 원본·정산 보존
- 계약: `vic25-time/2.0` (이전 `pop-design/1.1`, `phase-c-common/1.0`, `phase-d-common/1.0`, `phase-e-common/1.0`의 **시간 관련 부분만 개정하는 상위 ADR**)
- 개발: `work/daily-tick-b-j`
- 기준: `work/merge-all-to-main`; **main 미변경**

## 1. 핵심 결정

1. 세계에서 진행 가능한 기본 시간 단위는 **게임 내 1일**. `day_index`는 Gregorian year 1-01-01 기준 일수. `GameDate(year,month,day)`와 동등 변환 가능. 연도 1 이상, 윤년 4/100/400 규칙, 각 달 실제 일수를 검증한다. 사용자에게 보이는 진행은 일시정지·배속이지만, 속도는 경제 확률/재고·인구 변화 계수와 무관하다.
2. `BeginDay → 오늘부터 유효한 예약 명령 → 시간 순서/도메인 단계에 맞는 실제 일별 사건 → 연관 원장 보존식 → (월말인 경우만) 정규 월간 정산 → CommitDay` 순서. **매일 C-D0의 월별 12단계에 포함된 자동 분개/출생·사망/시장청산/재료소비를 반복하지 않는다.**
3. 일별 원자성은 기존 Phase C/D/E/GH의 진짜 population/Financial/Inventory/Ownership 정본에 적용되며 일별 처리가 확정되지 않았을 때 날짜, 명령 큐, 금융/재고/POP, 정본 월 커밋과 daily event journal은 모두 롤백한다. 별도의 비공식 돈/물품 원본이나 "fake daily" 값 금지.
4. 일별 적용 가능한 행동은 각 원장이 실제로 일별 time semantics를 가진다고 검증한 경우만 whitelisted. 초기 인수 세로슬라이스는 `daily_payment`, `daily_trade`, `military_control` 등 단계 제한이 있는 기존 Phase C 원본 사건만 지원. 나머지는 이벤트 타입/효력 날짜/계약 전환 완료 전까지 미지원으로 정확하게 거부한다.
5. 이전 월별 엔진은 **임시 MonthClose adapter**로 남긴다. `legacy_engine.advance()`는 실제 달력의 마지막 날짜에 **한 번**만 실행된다. 이 접근은 일별 POP 출산·건설·시장·G/H 조약 전면 구현을 의미하지 않으며, 이전 숫자 계수가 하루마다 반복되지 않게 하는 정확한 호환 경로이다.
6. 월중 일별 재고·자금의 결제와 기존 월별 거래는 같은 Financial/Inventory 정본에 실제 원장 사건으로 적용. 개별 거래가 과거의 월초 snapshot으로 소급되거나 다음 날에 중복 적용되면 거부. 일별 이벤트와 월별 신규 결산의 단계별 원가·국민계정 연계는 후속 상세 구현이 필요하다.
7. Phase B/C/D/E/G/H의 과거 스냅샷·이력에서 사용하던 `month_index` 필드와 `effective_month` 필드는 legacy v1 슬롯으로 둔다. 신규 규칙·일별 효과에는 `effective_day`, `due_day`, `event_day`와 `day_commit_id`를 사용. `month_index`를 30배 곱하거나 모든 기존 조건을 매일 1회 발생시키는 자동 치환을 금지.
8. 기존 12개 월간 stage 순서는 현재 호환 엔진 내부에서 변하지 않는다. **일별 외부 엔진은 하루를 원자적으로 커밋하며 월말에만 한 번 stage 0..11을 호출.** 상세 B~J의 일별 생산·군사·행정·사회·자원연산 구현은 각 Phase의 차기 ADR에서 단계별로 이전한다.
9. 같은 게임 날짜·세계 스냅샷·명령 세트·시드이면 worker 1/N, UI 프레임률/속도와 무관하게 동등한 월간/일별 원장 결과를 생성한다. 정수/고정소수점·event-ID 정렬·월간 carry를 보존한다.
10. 세계지도의 1만~2만 프로빈스, 최대 3만 프로빈스는 매일 deep-copy 또는 전수 프로빈스·POP 재집계하지 않는다. **무변경 일별 틱은 메타데이터만 변경, 실제 상태가 바뀐 날은 필요한 부분만 수정**하도록 최적화하며, 통합 엔진의 완전한 copy-on-write는 아직 별도 구현·부하검증 게이트다.

## 2. 일별 공개 단계

| 순서 | 의존성 및 행위 | 초기 구현 범위 |
|---|---|---|
| D0 | 어제 확정 world, 유효 날짜·이벤트 정렬 | Gregorian calendar & queued actions |
| D1 | 오늘 효력 법률/정책 및 계약·명령 유효성 검사 | 실제 구현된 C 원장 일별 사건만 |
| D2 | 명령 검증/자원 예약/정본 검증 | Phase C stock/Financial/Population |
| D3 | 해당 날짜의 생산·운송·군사 등 일별 사실 처리 | 결제, 현물 거래, 점령 통제 |
| D4 | 월말이면 **한 번만** 기존 12-stage 정산 | C/D/E/G/H 기존 `advance()` |
| D5 | 전체 원장 검사·사건 멱등성·일별 확정 | 날짜, commit, queue, journal |
| D6 | 렌더·UI/AI에 확정 WorldSnapshot publish | 읽기 전용; 구현 별도 |

이 표는 논리적 시간 계층. 분야별 실제 세부 단계/의존 DAG 및 J/I 계산은 아직 상세 버전 개정이 필요.

## 3. B~J 변경 매트릭스

| Phase | 일별 필수 동작 | 일별/월말 분리 및 미완료 상태 |
|---|---|---|
| B POP | 출생·사망·부상·이주 등 실제 일자·지연 상태 | 기존 `DemographyRules`는 월간 ppm. 일별 누적 확률/age 전이·carry 마이그레이션 미완료, **월말 한 번만** 기존 모델 |
| C 경제·정치·군사 | 실제 일별 현물/금전 거래·군 통제·정책 효력 | 제한된 C 진짜 ledger 사건만 일별 허용. 고용/출생·군수/정치 일별 전체는 차기 단계 |
| D 건설·산업·기술 | 일별 생산/공정·준공/교육/기술이전의 날짜별 효력 | 기존 월간 capacity/WorkPackage/Research는 월말 한 번. 일별 작업량·설비 예약·WIP/공정비 전체 이전 미완료 |
| E 지리·영토·행정 | 점령/통제·국경 변경일, 시장/행정 소속의 날짜별 효력 | C의 실제 군사통제 일별 접근, 유효기간 월 단위인 E 행정재편은 우선 월간 경계 호환 |
| F 교통·물류 | shipment eta day, 운송 혼잡/수송량 | F 완성형 엔진 미등록. G/H의 월간 transit_months를 임의 일수로 나누지 않음 |
| G 무역 | 선적·계약·보험·금융 및 배송 시각 | GH 정본 월간 계약/시장청산은 MonthClose. 모든 일별 계약 스케줄 마이그레이션 후 활성화 |
| H 외교 | 조약 비준·효력 날짜·위기 전개 | 기존 `effective_month` 정본은 MonthClose. 새 일별 조약의 유효화는 H2 명시 schema 버전 이후 |
| I 사회·교육 | 일별 교육서비스 발생과 학교/사회 제도 | **설계 전용**: 아직 전체 C++ I engine 없음, 일별 상태를 허위로 생성하지 않음 |
| J 환경·자원 | 날씨/재해/생산성 변화/탐사 | **설계 전용**: 아직 전체 C++ J engine 없음. 지하 전체 매장량 모델은 포함하지 않음 |

## 4. 호환/데이터 이주

- 정상적인 구 월별 snapshot의 `legacy.month()`는 **다음에 처리할 월의 0-base month ordinal**. 새 일별 시작점은 이 월의 1일 00:00 (그레고리력). 마지막 완전 월말이 이미 끝난 상태로 취급하며 추가 POP/금융/상품 지급·인원 전이를 만들지 않는다.
- `year=month_index/12`, `month=(month_index%12)+1` (`year>=1`). 2000-01 ordinal은 24000. 월별 snapshot에서 명시적 `calendar_id`가 없던 사실을 기록하고 **proleptic Gregorian**이 시나리오/계약에서 승인된 경우에만 변환.
- 신규 daily checkpoint에는 `vic25-time/2.0`, Gregorian calendar version, day_index/date, day_commit_id, base legacy GH payload exact checksum, pending event queue/event IDs, 정본 nested snapshot schema, ruleset migration provenance를 기록. 완전 일별 상태가 없는 월중 임의 날짜로의 "그럴듯한" 재구성 금지.
- 월간 checkpoint와 일별 checkpoint를 무조건 상호 호환하지 않는다. 명시적 `migrate_legacy_*()` 인터페이스에서만 원본 snapshot의 고유 checksum/version을 검증한 뒤 **처음 날짜**로 wrapping. 포맷 불일치·고아참조·손상은 거부하고 원본 파일을 덮어쓰지 않는다.
- 실제로 등록되어 있는 C/D/E/GH 버전별 codec만 승인; B 단독의 공식 snapshot codec 및 I/J runtime snapshot은 현재 없음. 존재하지 않는 raw 자료 포맷을 추측하여 migration 성공으로 선언하지 않음.
- Snapshot migration은 rollback 가능하며 `old_world_hash`, `new_world_hash` 및 정본 POP 인구·현금·재고/시설·정부/군사 상태 동일성을 검증. 저장은 파일명 임시 작성→원자 rename; 이전 데이터 백업 유지.
- 사용자 시간 가속/일시정지/렌더 성능은 시뮬레이션 결과에 영향을 주지 않는다.

## 5. 필수 회귀·릴리스 게이트

T-A Gregorian leap/day/month transitions; T-B 월 경계 1회만 legacy monthly; T-C 날짜별 실제 현금·재고·군사통제 갱신; T-D 잘못된 명령 전체 하루 롤백, command-ID 재적용 거부; T-E 월말 원장 실패 시 일별 사건까지 함께 롤백; T-F 구형 C/D/E/GH snapshot 정확한 migration 및 손상·버전 거부; T-G 직렬화 왕복·재현·시드/순서; T-H 10k~30k Province × 100k/1m POP 고빈도/무변경 일별 부하, UI 성능 독립; T-I 기존 POP/C/D/E/G/H 회귀 및 GH 238 Gate 미완료 구분.

## ADR 확정 판정

`ADR-TIME-001 ACCEPTED`: 일별 외부 시간 계약 확정.
`FULL_B_J_DAILY_ENGINE ACCEPTED`: **NO**. 일별 전환은 기존 구현된 B~H에 한정된 호환 실행기와 검증에서 시작하며, I/J full runtime과 법률·국민계정·물류의 완전 일별 계산은 별도 단계에서 구현.
