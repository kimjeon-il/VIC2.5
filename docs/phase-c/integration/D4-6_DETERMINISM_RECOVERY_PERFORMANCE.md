# C-D4-6 — 결정론·저장/복구·성능·장기검증 계약

**상태:** 설계 확정 / 실행·측정 미실시 · `phase-c-integration/1.0` · [색인](README.md)

## 1. 결정론적 실행의 우선순위

- **인구·통화·상품·소유권·표·의석의 보존 결과는 정수/체크산술로 완전 일치.** `People=int64`, `Money={currency_id,int64_minor_units}`, `Quantity={good_id,int64_scaled_units}` 및 체크된 확장 중간연산 사용.
- 가격/환율·확률·전투효과·사기/교육·수요 예측·정치선호/동원비율에서 실제 인구/돈/재고를 바꿀 때는 버전 있는 고정소수점 숫자계약과 반올림 방식(최대/최소 tick, floor + largest remainder, 안정적 ID)을 사용한다.
- 월간 결과가 단일 worker와 N worker·엔터티 순회순서와 무관하게 같아야 한다. 영역별 worker가 전역 금융원장·인구셀을 순서 없이 직접 수정하는 최적화 금지.
- 난수: `seed_stream = derive(world_seed, month_index, stage, source_entity_id, event_kind, draw_index, rng_algorithm_version)`. 전역 mutable PRNG 순차 호출 금지. `draw_index`는 엔터티별 논리 사건/정렬에 기반, 스레드 생성 순서로 증가시키지 않음.
- Event ordering: 의존 `DAG` 위상정렬, 동률은 `(month, stage, subphase, event_priority, entity_type, entity_id, event_kind, event_id)`의 버전 있는 stable key. `event_id`는 순서 결정을 위해 생성되며 병렬 실행 순간마다 순서가 변하지 않는 **결정론적 할당/재생성 전략** 필요.
- 자산·계정/인구셀의 신 ID 발행은 최종 deterministic journal order에서만 확정한다. aborted staging의 ID 증가가 다음달 실행에 남지 않아야 하며, 이미 외부에 노출한 명령 ID는 중복 수신 테이블로 처리한다.
- 반올림 잔차(인구 출생 기대값/금융·가격 단위 잔여)는 사건별 잔차 계정/상태로 누적하며 월중 아무 이유 없이 버리지 않는다.
- **크로스플랫폼:** 모든 커밋에 영향을 주는 core numeric path는 명시적 고정소수점 구현을 목표로 하며, 아직 부동소수점 모델이 사용되는 경우 **Windows/Linux 간 바이트 동일**을 달성했다고 선언하지 않는다. 실제 동일 규칙·셋업의 cross-platform hash 비교 후 범위를 확정한다.

## 2. 월별 세계 fingerprint

`WorldFingerprint`는 다음 내용의 **정규화된 직렬화 바이트**를 hash한 값이다:
- 활동 PopulationSlice·분류/health/profile/partition·lineage 및 revision(정렬);
- 실제 Employment/Ownership/Financial/Inventory/Production/Trade/Fiscal 원장·모든 잔여 예약/계약 상태;
- Country/Government/Law/Office/Election/Party/Movement·정치적 태도/회원/시위 원장;
- MilitaryUnit/Formation/ServiceAssignment/EquipmentAssignment/War/Engagement/Jurisdiction/Shipment;
- 이월할 주문/효력예약/운송중 상품/법률 변경, RNG 잔차·새 ID·이벤트 dedupe cursor, month_index와 핵심 rule/data hashes.

**검증 로그 출력 시간·CPU 소요·메모리주소·스레드 ID 같은 비결정적 운영 메타데이터는 world fingerprint에서 제외하고 별도 측정 로그에 넣는다.** `hash_algorithm_version`과 정규 직렬화 순서를 세이브 헤더에 고정. 정책상 외부 환율/실측 역사 데이터는 먼저 특정 월의 입력으로 고정된 뒤 계산한다.

## 3. save/load·replay 계약

필수 세이브 헤더:
```text
save_schema_version
pop_contract_version = pop-design/1.1
common_contract_version = phase-c-common/1.0
economic_contract_version = phase-c-economy/1.0
political_contract_version = phase-c-politics/1.0
military_contract_version = phase-c-military/1.0
integration_contract_version = phase-c-integration/1.0
ruleset_version, ruleset_hash, source_provenance_hash
calendar_id, month_index, world_seed, rng_algorithm_version
money_scale_by_currency, quantity_scale_by_good, numeric_policy_version
id_allocator_state, last_commit_id, last_journal_offset
snapshot_hash, journal_hash, world_fingerprint
```

정치 GitHub 정본 미등록/스키마 부재 시 `political_contract_version`를 근거 없이 실제 엔진 세이브에서 완전 지원으로 광고하지 않는다. 사전 설계/테스트 manifest에는 `baseline_reference_sha256`과 `status=UNREGISTERED`를 명시한다.

파일/영속화 모델: **immutable committed snapshot + ordered append-only event journal + atomic commit marker**. 파일 기록 시 스테이징 완료 후 검증된 checkpoint/snapshot+hash를 임시 위치에 기록, 내구성·원자적 참조 교체를 통해 성공한 커밋만 공개. 크래시/부분 기록 후 로드할 때 마지막 정상 checkpoint 및 journal commit marker까지만 복구하고 손상된/중간 월은 폐기. 저장시스템별 fsync/rename/트랜잭션 DB 원자성 차이는 실제 운영 환경별 시험으로 확인한다.

Replay 규칙:
1. last_commit_id/last_journal_offset와 journal hash 대조.
2. 저장월 이후의 확정 사건만 적용. 동일 source_event_id/transaction_id 재적용 금지.
3. 규칙·스키마 버전이 불일치하면 확정된 migration 경로로만 변환하고 migration 후 단계별 잔액/주민/재고/법 효력·fingerprint를 검증. 검증할 수 없는 변환은 명시적 `UNSUPPORTED_SCHEMA` 거부.
4. 프로세스 비정상 종료/부분 저장·전원 중단/디스크 부족을 장애 주입으로 시험. 실패하면 **월초 또는 마지막 확정 월 상태**로만 회복.
5. 다른 플랫폼/컴파일러로 로드 시 숫자·정렬·직렬화/encoding 정책을 대조하고 해당 형식 호환 범위를 보고.

## 4. 장애 주입(fault injection) 표준

| 주입점 | 실패 모드 | 요구 결과 |
|---|---|---|
| 월초 법 발효/도착화물 준비 중 | invalid law revision/ customs payment 실패 | OpeningSnapshot은 미확정, 전월 commit 그대로 |
| 단계1 은행대출/지출예약 | 중복 reserve 또는 현금 부족 | 예약/대출 양쪽 원장 부분반영 금지 |
| 단계3/7 상품 거래 | 결제되었으나 Inventory movement 실패 | 같은 trade batch 전체 rollback, 품목/금액 불변 |
| 단계5 급료/원천징수 | Financial 분개 한쪽 누락 | 노동자+국고+기업 합산 검증 실패/abort |
| 단계9 선거·입법 | 2중 투표/무권한 법·연정 권한 누락 | 정당/정부·법률상태·예금 동시롤백 |
| 단계10 피해·징집 | 이미 사망한 사람 동원/두 전투 초과피해 | POP/Service/Finance/Inventory 일괄rollback |
| 단계11 검증 | 고의 잔액1 불균형·유효기간 중복 | 월 index/commit ID/차월 가격/법 예약 모두 원래대로 |
| 체크포인트 기록 중 | 디스크 부족·중단·손상된 journal | 검증 가능한 마지막 확정 상태만 복구 |
| 동일 외부 메시지 재발행 | duplicated command_id / event_id | 기존 결과 재조회, 두 번째 효과 없음 |

장애발생 지점별 seed/command trace/commit hash를 보존하되, 실제 `committed_world`에 로그/예외 객체를 기록하지 않는다.

## 5. 장기 회귀(1200개월) 매트릭스

**T-Base:** 3지역·2국가/정부, 제한적 기업·농가·가계·금융기관·정당/정치운동·정규군/예비군. 평시 출생·노화·교육·노동/가격·국가재정·선거/법률·기본 장비 유지비. 1,200개월 중 모든 월에서 30개 불변식/단일 커밋. 인구 감소/기업 파산도 원인이 설명되면 허용.

**T-Agri:** 인구 급증/흉작·식량부족, 품목별 가격·소비와 건강/사망의 연쇄·정치 반응. 가격/만족의 동학은 규칙·계수에 따라 다르므로 단일 예측값을 강제하지 않는다.

**T-Finance:** 기업적자·실업·자산 담보·은행연체/대출상각·정부국채, 복식부기와 실제 소비·세수의 인과.

**T-Politics:** 선거권 확대/개정·양원제/군주거부권/정부 교체·시위/파업/반란 조직 전이·법 채택/실제 집행 분리.

**T-War:** 징집/군수품 부족·여러 전투·사상/건강/장비 손실·점령·배상·제대·의료·전후 재건과 정치·경제 복구.

**T-Combined:** 같은 기간 흉작→전쟁동원→금융/물가 위기→정치 개혁/점령→종전/복구. 결과가 불황/갈등/패전이어도 인구·자금/재고·권한·이력 정합이 유지되면 성공 가능.

단계별(0..11) 원장/수량·선행 상태를 수시 점검하고 월말마다 full-world 검증 및 fingerprint 기록. 위험집합·소유주 변경·멀티부대·선거구 변경에 대한 high-stress fixture도 포함.

## 6. 계측·프로파일링

매월 기록: `population_slices`, 분류/활성 법·조직/정당/군부대/지역 수, 노동계약, 시장주문/체결, 계정/금전거래/재고 이동, 군 수송중 화물/전투 사건, per-stage events·rollback·revision conflict/branch count, 월별 wall/CPU time, RSS, allocations, stage11 full invariant cost, journal IO, save/replay time.

시험 규모:
- `B0`: 6명·3지역 미니 fixture, 모든 분야 연동.
- `B1`: 수천~수만 희소 셀, 실제 거래/단일·다중 국가 관계, 1,200개월.
- `B2`: **10만 활성 희소 셀**, 거래·정당/군관계의 대표 부하를 일정 개월 반복.
- `B3`: **100만 활성 희소 셀** 실제 생성·원장 검증/대표 월 단계 처리; 데이터 메모리/성능 상한 탐색. 100만 셀을 100년 돌려보지 않고 100년 안정성을 이미 확인했다고 하지 않는다.
- `B4`: 충돌 반복(다중 worker 및 12단계 stage10 전투/인구 분할 스트레스).
- `B5`: Linux/GCC, Windows/MSVC, macOS/Clang 적용 가능한 플랫폼별 동일 입력 컴파일/정수 결과/세이브 비교 및 실패 로그.

P50/P95/P99(월·단계별), throughput(event/s), peak RSS/alloc bytes, shadow state 증가량, 전역 commit 시간, save/load 크기/시간·CPU를 실제 하드웨어/컴파일 옵션과 함께 기록. **임의 목표 ms/month·GB/월 사용량을 검증 전 성능 약속으로 제시하지 않는다.** 시뮬레이션용 최소 하드웨어/목표 성능은 기준 벤치마크를 확인하고 별도 ADR로 확정.

## 7. 수치 모형 보정과 게임성 검증의 분리

- 검증의 첫째 목표는 **내부 회계/논리·재현성**. '20년 후 GDP' '전쟁 승률' '정당 지지율'의 역사예측 정확도와 구별한다.
- 역사적 인구/경제/정치/군사 실제 모수는 시대·국가별 연구자료 출처, 측정/추정 범위, 시기별 calibration revision 및 confidence를 가져야 한다. 추정값을 실제 기록으로 표기 금지.
- 가격반응 `alpha/beta/delta`, 고용/투자·금융부실 탄력도, 정치동원·투표·병역 적격과 전투 손실계수는 각 영역의 버전 규칙파일에 고정한다.
- 기초 거시지표가 현실적으로 어색해도 곧장 arbitrary correction으로 POP/돈/상품을 더하지 말고 **원장·수식·매개변수/충격 기여 분해**를 남긴다.
