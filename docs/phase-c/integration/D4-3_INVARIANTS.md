# C-D4-3 — 경제·정치·군사 교차 불변조건(검증 수식)

**상태:** 설계 확정 / 구현 전 · `phase-c-integration/1.0` · [색인](README.md)

## 1. 검증 모드

- `HARD`: commit 이전 반드시 참이어야 하는 정수/참조/권한/유효시점·자료 구조 조건. 실패 시 `AbortMonth`.
- `ACCOUNTING`: 통화/상품/인구·정확한 장부 분류에 대한 보존/복식부기 대조. 실패 시 `AbortMonth`.
- `DIAGNOSTIC`: GDP·실질임금·생활수준/정치적 선호·전쟁피로 같은 모델 산출/경제적 위험 지표. 불일치의 원인이 통계 분류/역사 입력 부족이면 경고와 근거 기록. 값 자체가 불황/패전/불만이라는 이유로 Abort하지 않는다.
- `REPRODUCIBILITY`: 동일 입력·contract/seed/numeric policy에서 hash/이벤트 순서의 재현. 스코프/플랫폼별 통과 범위가 명시되어야 한다.

## 2. 핵심 불변조건

| ID | 등급 | 정확히 검사할 규칙 |
|---|---|---|
| D4-INV-01 | HARD | `world_population = Σ active PopulationSlice.count`. Country/Market/Firm/Party/Unit/Household 캐시를 다시 더하지 않음 |
| D4-INV-02 | ACCOUNTING | `N_end = N_open + births - deaths + external_in - external_out`. 이주·직업·건강·동원/제대/투표/점령/내전 편입은 세계 주민 수 변화 0 |
| D4-INV-03 | HARD | 모든 활성 인구 셀의 demographic/economic/household/partition 참조 및 지역 일치, ID/lineage 재사용·음수 인원·중복 활성 키 금지 |
| D4-INV-04 | HARD | 실효 노동적격 인원당 주된 전일제 Employment·자영/생계 노동 배정은 각 slice 인원 상한 이내. 현역의 법적 겸직/복무 예외가 아닌 한 민간 전일제 중복 금지 |
| D4-INV-05 | HARD | `Σ active ServiceAssignment(slice).count <= service-compatible active slice population`, 실제 부대 병력은 assign 합. Reserve, POW, missing 및 부대 대기 풀까지 관계 상호대조 |
| D4-INV-06 | HARD | 상급 Formation 병력 = **유일한 하위 실부대** ServiceAssignment 재귀 조회합. 상급 인원 별도 복제/편제 계보 사이클 금지 |
| D4-INV-07 | ACCOUNTING | 품목/보유자별 `I_end = I_open + produced + arrivals - production_inputs - consumed - dispatched - documented_losses`; 내부 이체·in_transit 포함 세계 보존식에서 이중계상 금지 |
| D4-INV-08 | HARD | 주문·예산·수송계획·생산계획/보급요청·배당 선언은 실제 Fill/InventoryMovement/FinancialEntry 없이 구매·생산·자산·소득으로 확정될 수 없음 |
| D4-INV-09 | HARD | 거래의 실제 인도/대금/계약채권, 군수 조달의 장비 배속/사용, 전투의 재고/장비 소모가 `source_event_id/transaction_id`로 연결; `reservations <= spendable stock/available funds` |
| D4-INV-10 | ACCOUNTING | 모든 JournalBatch의 **경제주체/통화별** 복식분개가 균형, 은행의 예금부채·고객 청구권과 실제 대출/준비금·기관 간 결제 대응 |
| D4-INV-11 | HARD | 같은 통화가 아닌 Money끼리 무근거 합산/지불 불가. 미구현 FX 거래에 1:1 환전/자산 생성 금지 |
| D4-INV-12 | HARD | 대출승인·예상세입·국채발행계획·배당선언/투자계획을 지불가능 예금으로 중복 가산하지 않음; 실제 분개와 현재 만기/미지급 채무가 분리됨 |
| D4-INV-13 | ACCOUNTING | 발행주식·채권권리·자산 소유지분 합과 실제 발행량/금융채권·채무가 대응. POP 사망·국가승계·기업파산 후 권리/의무 고아 참조 금지 |
| D4-INV-14 | HARD | `ProductionOutput <= min(actual equipment capacity, eligible actual labor, delivered intermediate inputs, resource/legal limits)`; 투자재 설치/공공서비스 이용에도 실물·노동/공기·예산 제약 |
| D4-INV-15 | HARD | `available_unit_equipment <= actual_inventory/asset_access`; 동일 화기·수송능력·탄약/연료를 여러 부대/교전에서 중복 할당/소모 금지 |
| D4-INV-16 | HARD | 보급·수입 화물은 예정 도착일 이후 **실제 입고·통관 완료**에 한해 소비 가능. 출발·운송중·도착 보유자 상태는 동시 물량 중복 불가 |
| D4-INV-17 | HARD | 정치 권한/국체·입법기관/법률/공직·군 지휘권/군동원/재정명령은 시점에 유효한 법·기관·관할권을 통해 평가. 무권한 사건 거부 |
| D4-INV-18 | HARD | `adopted_month=m`인 신규 법의 기본 `effective_month>=m+1`. 뒤 단계 정치결정/월말 인구 변화는 이미 끝난 당월 경제·투표·교전 사건에 소급 사용 불가 |
| D4-INV-19 | HARD | 선거의 유권자/등록·유효/무효표·선거구/정당/의석 정수 보존; **1인1표일 때** `valid+invalid=cast<=registered<=eligible`, 각 투표자는 해당 선거에서 중복 투표 금지 |
| D4-INV-20 | HARD | 의석 정원=실제 충원 의석+결원, 법정 배분/임명·정족수·거부권/신임/내각 임명 준수. 선거결과만으로 무권한 정부 교체 금지 |
| D4-INV-21 | HARD | 이익집단·정당·운동의 가입·지지·표/집회/무장 실제 동원 인원을 구분. 복수 지지는 허용하나 중복 인구 합산 및 같은 기간 상호배타적 실제 참여 초과 금지 |
| D4-INV-22 | HARD | 군 전투 결과의 `KIA+temporary_wound+long_term_change+POW+MIA+unharmed = exposed_people`, 서로 배타적. 기존 중첩 질병/장애는 복합 프로필로 보존 |
| D4-INV-23 | ACCOUNTING | 단일 인구의 전사/자연사·부상/건강·포로/실종 위험집합은 과다 배정 불가. 사망자는 POP -N, 군배속/가구/유산·금융/고용과 함께 원자적 처리 |
| D4-INV-24 | HARD | `TerritorialJurisdiction`의 sovereignty/civil_administration/fiscal_authority/military_control은 독립 유효 관계. 군 점령/선전포고 자체로 주권·국적/인구/재산 변경 금지 |
| D4-INV-25 | ACCOUNTING | 전쟁배상/국채/세금·유족/군급여·복구 예산은 법적 채권·채무와 실제 지급/재고·자산·의료·복구 서비스의 대응 관계. 미지급 보상은 유령 현금/재고 아님 |
| D4-INV-26 | HARD | World/Registry/Journal/Save의 typed ID/유효기간/revision/선행 이벤트/룰셋/수량 스케일 정합, 사건 순환 참조/중복 ID·tombstone 부활 금지 |
| D4-INV-27 | HARD | 월간 실패가 있으면 모두 rollback. `month_index`, `last_commit_id`, published WorldSnapshot, 예약/지분/POP ID 발행상태가 원월 상태로 보존 |
| D4-INV-28 | REPRODUCIBILITY | 동일 초기 world·ruleset/버전·world_seed·숫자/정렬 정책 → 규정된 범위 내 event journal·end-world hash 동일(단일 vs 다중 worker 포함) |
| D4-INV-29 | DIAGNOSTIC | GDP는 실제 생산부가가치·최종지출(재고증감 포함)·소득 기준 평가가 가능한 범위에서 일치. 중간재 매출/복지 이전을 중복 GDP로 산정 금지 |
| D4-INV-30 | DIAGNOSTIC | 실업·군 동원·선거권·정치적 태도·생활수준·전쟁피로는 실제 원장/제도 관측치와 출처로 산출; 수치가 어떤 정치결과/승리를 자동 강제하지 않음 |

## 3. 정수/단위·재현의 실험적 검사

- 인구 `int64` signed 비음수, Checked/확장 중간연산. 수량 상품별 `QuantityScale`; 금액은 `{currency_id, minor_units}`; 가격은 `{currency_id, good_id, price_lot}`. `unknown`은 실제 0과 다름.
- 확률적 전이 인원 배분은 `floor + largest remainder`, 동률 안정적 ID, 잔여량 누적·재사용. 난수는 `(world_seed, month_index, phase, entity_id, event_kind, draw_index)`로 분기.
- GDP·재고 가치/자산평가는 감가상각·재고증가·해외거래·발생/현금 기준 및 price/version을 명시한 뒤 대조한다. *모형상 불황이라는 이유로 위반하지 않는* 수치가 `DIAGNOSTIC`에 해당.
- 출생/사망·동원/사상·이주/병합은 `source_event_id`를 가진다. 수정/취소는 확정 이전 전체 staging Abort, 확정 이후 별도 보상·역분개를 요구.

## 4. 검사 실행 최적화

**각 JournalBatch 사전검사:** 영향받는 entity/원장에 대한 ID, nonnegative, account balanced, reserved capacity, effective_month/revision 및 빠른 인구/금융/재고 보존 검사.

**각 단계 종료:** 단계상한·노동·수량·결제·법 권한을 검증하고 이후 읽기에 노출. 단계10은 전투 소모/사상자까지 반영.

**stage11 전체 검사:** D4-INV-01~28 강제 및 D4-INV-29~30 진단, 스냅샷 fingerprint·참조 계보/리플레이 커서, 차월 예약·통화/상품 스케일.

**성능:** 전체 원장 매 거래 전체 스캔 금지; 변경집합/인덱스 기반 delta 검사 후 월말 크로스 체크. 검증 생략으로 롤백 불가능한 최적화 금지.

## 5. 실패 예시

1. 선거구 600 적격·등록550, 투표420, 유효401 무효20 → `cast != valid+invalid` → 월 전체 거부.
2. 병력1,000 중 전사50, 부상180/70, 무손상710 → 합계1,010 → 거부.
3. 2개월 화물 60상자 구매월 즉시 부대에 60 사용 → 도착 시점 위반.
4. 정부 국채 100 **발행 계획만**으로 임금100 지급 → 금융결제 없음.
5. A군 점령만으로 B영토 주권과 주민 국적/재산권 이전 → 별도 법/조약 이벤트 없음.
6. 은행대출 실행으로 예금 100 증가: **은행 대출채권·예금부채, 차주 예금자산·채무가 정확히 일치하면 정상**. '총 예금 고정'은 잘못된 검증식이므로 사용하지 않는다.
