# C-D4-2 — 월별 실행순서·사건 의존관계·시간 계약

**상태:** 설계 확정 / 구현 전 · `phase-c-integration/1.0` · [색인](README.md)

## 1. 기존 12단계의 공개 경계

`MonthlyPipeline`의 다음 12개 이름/순서는 **변경하지 않는다.** 기존 POP-D3의 단계 0/1/.../11에서만 외부에 단계 완결 상태를 제공한다.

| 단계 | 선행 확정 / 본 단계 작성 내용 | 뒤 단계에서만 읽을 수 있는 변화 |
|---:|---|---|
| 0 `opening_snapshot` | 전월 commit 및 예약 법/공직·복무·시장/관할·도착 화물/대출기한 등을 검사·개시효력 처리하고 immutable 스냅샷 고정 | 개시 효과는 현재 월 입력; 이미 확인된 과거 사건 변경 금지 |
| 1 `planning_and_credit` | 기업/정부/군 계획·법적 권한·실제 융자/예산 재원·노무/재고/운송 예약 | 신용 **실행**만 지출가능, 승인한도/예산계획만으로 지출 불가 |
| 2 `labor_clearance` | 실제 일할 수 있는 POP/예비군·현역/직무적격·공공 인력·임금 재원으로 중복 없는 근로 배정 | 월말 신규동원 전 민간 생산/근로자 |
| 3 `intermediate_goods` | 판매가능한 실제 원재료·에너지·생산용 운송서비스 인도, 해당 금전청산 | 당월 4단계 생산품을 이전 3단계에 역송 금지 |
| 4 `production` | 실제 노동·설비·재료·자원/서비스 제한 내 생산 및 재고·서비스 확정 | 당월 제조품은 7단계 판매 가능 |
| 5 `wages_and_transfers` | 총급여/군급여·정부이전·원천징수 채무/지급을 균형 분개 | 실제 순수령액은 6단계 소비 가능 |
| 6 `pop_consumption_plans` | 가계 공동예산·실제 현물·가용 예금·대체재를 바탕으로 지불가능 주문 | 지불예약은 actual delivery/payment와 구분 |
| 7 `final_goods_market` | 정부/군/가계·기업 투자재/서비스의 실제 체결·금융정산·인도/출하 | 양(+) 운송기간 화물은 도착 전 소비/군수 사용 금지 |
| 8 `closing_accounts` | 발생/현금 결산·세금/금융/기업·정부 미수/채무·이자/부도 처리, 경제 지표 관측 확정 | 단계10의 사상자/전투 재고 변동은 뒤 11단계 최종 세계 상태에 추가 대조 |
| 9 `saving_and_investment` | 저축·투자·정치태도·선거·정부구성·법률채택·집합행동 계획을 하위 순서에 따라 실행 | 법·기관 채택이 0~8단계에 소급되지 않음 |
| 10 `pop_demography_and_health` | 작전·전투손실/민간 피해, 건강·인구·병역·가구/고용·자산 이전의 연계 batch | 월말 사망/부상/신규동원은 앞 단계 노동·임금·투표에 소급 X |
| 11 `repricing_and_validation` | 차월 가격/예약 상태, 모든 정본 원장·메타데이터·의존 관계 검사 후 월 **단 한 번 atomic Commit** | 실패면 전체 Abort, 달/커밋/ID/주문·법 등 전부 불변 |

## 2. 단계 내부의 결정론적 하위 순서

### 0단계 개시 스냅샷

`0a` 전월 커밋/미집행 예약·호환성/외부 명령 재전송 여부 확인 → `0b` 현재 월부터 유효인 법/공직·관할·시장·전월 동원/제대 상태 발효 → `0c` **실제 만기 도착/통관** 화물을 재고 보유자 이전·재정/금융 계약 조건에 맞춰 스테이징 개시 이벤트로 처리 → `0d` 인구/재무/재고/권한 검사 후 OpeningSnapshot freeze.

조건 미충족 배송은 `pending`/보관·미도착 상태로 남고 실제 지불/통관이 실패한 재고는 인도완료로 표시하지 않는다. 입고/금융결제는 새 월 원장 사건이며 전월 스냅샷을 수정하지 않는다. 개시단계에서 실패하면 월 시작 자체를 Abort.

### 1~8단계 경제·행정·군사 조달

이전 단계 확정 결과만 읽는다. 같은 단계의 다른 거래가 경쟁하는 계좌/상품/운송 용량을 요청하면 stable event key·priority·revision으로 전역 예약 충돌을 해결한다. 우선순위가 같으면 typed ID 순서, 정수 배분은 largest remainder. 각 단계의 `DomainFinalization` 후에만 다음 단계에 결과를 노출한다. 단계8은 **경제/재정 잠정 결산**이지 월간 전체 세계 Commit이 아니다.

### 9단계 정치 처리

P1~P5 `phase-c-politics/1.0`의 다음 순서를 채택(월별 외부 단계 이름 동일):
- `9a` 경제 저축·재투자·차월 재원/배당/건설 계획 (실제 결제는 E3 규정)
- `9b` 앞 단계의 실제 임금·물가·복지·세금·보급·피해 이력으로 POP 쟁점별 선호/정당·집단/운동 관계 갱신 (실물·현금/사람을 복제하지 않음)
- `9c` 유효 법으로 예정된 선거의 적격자·등록·실제 표·의석, 이후 권한에 따른 정부/내각 임명·연정/신임 사건
- `9d` 기관의 새 법률/예산/정책 표결·공포·차월 `effective_month` 예약. 같은 9단계에서 입법된 선거법을 이미 처리한 9c 선거에 적용하지 않음
- `9e` 조직화된 시위·파업·민정/치안 행위의 **차월 운영계획/당월 말 실제 인구피해 위험 사건**을 구분하여 준비. 실제 노동중단·법/징병 개정의 원칙적 효과는 차월.

단일 인원은 9c에 정상 투표하고 그 뒤 단계10 전투/사망/시위 피해를 받을 수도 있다. 이는 중복 인구/투표 오류가 **아니다**. 단 같은 사람의 하나의 선거에 두 표/두 선거구 투표는 거부한다.

### 10단계 전투/건강/동원

`10a` 월초/1단계에서 유효하던 부대·군수·정치/외교 허가로 작전/교전의 실제 참여자·재고·명령을 결정론적 선행 의존순서로 확보, 전투/민간 피해 및 자원소모 `ProposedEvent` 생성(아직 population 직접 수정 금지).

`10b` 전투 사망·자연사·질병 사망 등 **동일 인구의 경쟁위험**을 계산하고 중복 죽음/인원 초과를 제거한 실제 KIA/기타 사망자 배분을 확정. 이어 **사망자 POP 변동·부대/자산/금융 유산·고용 참조 종료**를 하나의 batch에 준비.

`10c` 기존·신규 질병/급성부상·장기 기능손상, 포로·실종/후송과 의료·재활을 실제 인구/보건·부대·군수/서비스 제약으로 이전. 한 사건의 전사·부상·포로·실종·무손상 결과는 상호배타적이며 기존 질병/장애 병존은 별도 프로필.

`10d` 출생(새 유아는 비노동), 출생월 고령화, 교육/전직·가구 재배정·거주 이주 등 비사망 전이를 처리. 원본 POP-D2 사건/이력·자산 이전 규칙 준수.

`10e` 월초 유효 병역법·1단계 동원 계획에서 **전투에 참여한 기존 현역과 별도** 실제 가용 시민/예비군에 신규 징집·제대·송환 전이를 확정, 차월 `ServiceAssignment`/노동시장 갱신. 예비군이 당월 2단계에 일한 임금/생산은 그대로; 현재 10단계의 앞 교전에는 신규 동원 인원 추가 불가.

`10f` 남은 재고/장비 손실, 군사통제/복구/정치 집합행동 인구 영향, 출생·사망·부상·동원의 lineage/expected revision·Financial/Ownership/Inventory 조정 및 후속 정책 예약의 검증. 구체 규칙의 타입별 정렬은 `ruleset`에 명시.

**기존 Phase B**의 stage10 인구 연산 순서와 통합 M4 경쟁위험 단계가 다를 수 있다. 구조/결과가 호환되지 않는 경우 성급한 기존 세이브 해석을 하지 않고 `integration_numeric_policy_version`·migration/기존 회귀를 명시하여 전이한다. POP-D3의 외부 단계 10 이름/단일 인구원본은 유지한다.

### 11단계 세계 결산

`11a` 단계10 모든 인구/상품·자산·재정·정치·군사 차월 상태 반영 → `11b` **전역 교차 불변식**/가격·스케일/난수·이벤트 순서·경제 GDP 항등식 검사 → `11c` 차월 가격/법률/사건 예약 해시와 `commit_id`, replay cursor 만들기 → `11d` 성공 시 atomically publish, 실패 시 Abort. 부분 저장/외부 메시지/월 인덱스 전진 금지.

## 3. 의존관계 DAG 및 동시 충돌

```text
Effective Law m, Opening Finance/POP/Inventory
  ├─► credit/funding reservation ─► labor ─► intermediate inputs ─► production
  │                                  └─────────► wages ─► consumption order
  ├─► authorized procurement / supply ─► actual trade/delivery ─► available stocks
  │
  ├─► month-m politics preferences ─► election(m) ─► law(m+1)
  │                                   └───────────► political action events
  └─► valid orders and active soldiers ─► engagement ─► casualties/equipment losses
                                          │
                                          └─► POP-D2 population and ledger transitions
All staged postings ─► full-world validation ─► ONE month commit
```

- DAG는 `depends_on_event_ids`와 유효단계 및 expected revision으로 검증. 의존사이클·역방향 참조·아직 발표되지 않은 이벤트 사용은 거부.
- 명령 2개가 같은 사람/예금/장비/재고를 넘게 요구하면 (a) 합법적 우선순위·사전예약 차감에 따른 부분확정, (b) 명시적 수정/재배분, (c) 사건 거부 중 하나로 처리. 조용한 음수/중복 허용 금지.
- 한 사람이 투표/일하고/이후 죽는 것과 한 사람이 동일한 시간·임무로 두 부대/두 공장에 복무하는 것을 구별한다.
- 병렬 작업자는 독립 `JournalProposal`만 작성, 전역 순서·reservation table을 거친 뒤 적용한다. 결과는 worker 수와 순회 순서에 의존하지 않아야 한다.
- stage10의 전투 재고·자산 손실을 이미 끝난 stage8 현금거래에 소급 넣지 않고, 단계11에서 월말 재고·자산과 사건별 발생손실을 최종 대조한다.

## 4. 단계별 오류 코드/보고 계약

- `D4_ERR_AUTHORITY`: 유효 권한 없는 법/징병/예산/지휘/점령 명령
- `D4_ERR_TIMING`: 미래 단계/미도착/다음달 법·병력 사용, 금지된 소급
- `D4_ERR_POP_OVERALLOC`: 사망/동원/취업/선거 인구 중복·초과
- `D4_ERR_FINANCIAL_UNBALANCED`: 분개 또는 지급/채무 불일치, 통화 오류
- `D4_ERR_INVENTORY_OVERDRAWN`: 판매·운송·군수 소비가 실제 재고 초과
- `D4_ERR_OWNERSHIP_ORPHAN`: 사망·합병/분할로 자산·지분/채무 소유주 참조 유실
- `D4_ERR_EVENT_REPLAY`: command/event/transaction 중복
- `D4_ERR_REVISION_CONFLICT`: stale revision/중복 예약·종속사건 충돌
- `D4_ERR_RULESET_COMPAT`: 불일치 스케일·버전/세이브·정렬/numeric policy
- `D4_ERR_COMMIT_VALIDATION`: 일부 영역의 실패, 전체 staging 폐기

오류 보고서 필수: `world_id, month_index, stage/subphase, event/command_ids, affected_entities, reason_code, expected_revisions/observed_revisions, ruleset_hash, seed, rollback_commit_id`. 오류 메타데이터 출력 자체를 확정 월간 세계 변경으로 취급하지 않는다.
