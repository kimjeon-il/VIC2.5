# J-D1 — 자연재해·노출·피해·복구 상세 설계

> phase-j-disasters/1.0 · SPECIFIED / NOT IMPLEMENTED · 상위: J-D0/J-N1, E-D0, C-M/C-E, D-C5, POP-D2

## D1-1. 범위
가뭄·홍수·폭풍·극심한 기온/산불·지진 등 재해를 **자연환경·위험노출·시설 취약성**과 결합한다. 전쟁 피해, 일반 산업재해, 전염병의 실제 역학·의료 상세 계산은 C-M/D/POP 보건과 책임을 분리한다. 자연환경이 병원/정수/전염 위험에 미치는 영향은 명시적 보건 입력 사건으로만 전달한다. 강제 재해 '이벤트 카드'·추상 GDP -X% 버튼은 기본 엔진이 아니다.

## D1-2. 최소 모델
| 객체 | 필드/의미 |
|---|---|
| HazardDefinition | kind, seasonal/tectonic/hydrologic inputs, intensity units, duration, severity_ruleset_version, allowed_provinces/sea_zones |
| HazardForecast / OpeningExposure | province_id, month, conditional risk/notice, uncertainty, stage0 operational_constraints; 재해 확정 피해와 구별 |
| HazardEvent | event_id, kind, start_month/submonth_offset, affected_province_set, severity_by_province, causal_weather/geology_refs, seed_stream, event_revision |
| ExposureView | site/assets/PopulationSlice/transport_refs, physical_location, protective_infrastructure, vulnerability_rule_version; 사람/건물의 중복 소유 원본 금지 |
| DamageBatch | hazard_id, region, target_entity_id, damage_type, measurable_loss, mitigation_refs, source_event_id; C/D/POP에 제출 |
| RecoveryAction | actual repair/construction/relief/temporary relocation/insurance payout refs, government/owner, effective_month; 해당 분야 원장만 정본 |

## D1-3. 발생·공간·피해
1. 기후재해 위험은 J-N1의 당월 계절/강수/토양수분·하천/해안/고도 환경 및 실제 산림·방재시설에서 계산. 지진은 별도 지질 위험·발생 규칙에 따라 결정; 특정 국가마다 고정 '재해 운' 포인트를 부여하지 않는다.
2. 동일 기상 원인 가뭄이 농업 생산 제약으로 이미 반영됐으면 J-D1이 농업 재고를 추가로 임의 소각하지 않는다. **노출된 특정 시설/수확물이 실제 손상된 부분**만 추가 피해로 기록한다.
3. 단일 HazardEvent는 여러 프로빈스에 걸쳐도 **안정적 event_id 하나**와 지역별 severity·피해 엔터티의 독립 기록을 가진다. 인접성/하천·해안 연결 근거 없이 범위를 확장하지 않는다.
4. 환경 위험 자체와 실제 피해는 다르다. 부실한 방재시설·토목·위생·주거/인구밀도·기술·자산상태가 손실/피난 규모에 작용. 법령·정책 발표만으로 내구성 보너스를 즉시 부여하지 않으며 실제 건설/운영을 거친다.
5. 주민 사망·부상·이주는 POP-D2 사건으로만 반영하고, 시설 손상은 D-C5 CapitalAsset 상태, 재고·물류 손실은 Inventory/Shipment·생산 원장으로만 반영한다. 하나의 사상 원인+인구를 C-M 전투/POP 보건과 중복 소비하지 않는다.
6. 기업·정부의 복구는 실제 인력·재료·금전·운송/법적 권한이 충족될 때만 공사로 진행. 수재 손실을 고정 국가 GDP 감소나 근거 없는 무료 복구로 환산하지 않는다.

## D1-4. 시간 정합과 의외의 재해
- 월초 stage0에 **예견 가능한** 계절/방재·정상운영 제약을 동결하여 stage1~4 조업·건설 계획에 반영한다. stage0 기상 위험을 알고도 합리적으로 피하기 어려운 손실을 발생시킬 수 있다.
- 단계10에 처음 발생/확정된 예기치 않은 재해는 Stage10의 사람·재고·시설 피해를 기록하고 차월 stage0의 운영능력을 낮춘다. 이미 끝난 stage4 생산/월급/시장 거래를 소급 취소하지 않는다.
- 월중 재해를 시간 단위로 세분하고 싶다면 기존 12단계 바깥 독립 커밋을 만들지 않고 한 월 내부 하위 Event DAG로만 재생한다.
- 기존 C-M 전투가 같은 시설에 입힌 손실과 재해 피해는 (target_id, loss_kind, effective_month, source_event_id)의 원인/가용한 자산 잔액을 대조하여 실제 잔존 설비/재고 이상을 중복 파괴하지 않는다.
- 구호비·보험·부채 탕감은 Finance/Fiscal/계약을 근거로 실제 지급. 구호 선언만으로 POP 현금 증가나 시장 상품 재고 생성 금지.

## D1-5. API·성능
GetHazardRisk(scope, period, snapshot) → 가능성·범위·자료불확실성; RealizeHazard(month, ruleset, seed, opening_state) → 유일 사건과 지리 피해 범위; EvaluateExposure(event_id, current_facilities/pops, snapshot) → 중복 없는 대상; SubmitDamageBatch(... stage10) → 원본 분야 이벤트; PlanRecovery(actor, region, budget/crews, effective_month) → 후속 실제 공사/이전 사건; ExplainLoss(event, affected_entities) → 발생 근거·피해 규모/실제 방재와 회복.

보통 월에는 활성 위험/변화 프로빈스만 평가, 지진·대형 홍수의 영향을 받는 지역군은 중복 없이 순회. 전 세계 30k 프로빈스의 모든 건물×전체 POP 셀 조합 전수 스캔을 금지하고 E 공간 인덱스/행정·시설 지역 인덱스를 재사용.

## D1-6. 필수 시험 J-D1-T01~T12
T01 남반구 계절별 폭풍 위험; T02 홍수 지형/하천 연결 범위; T03 하나의 태풍 복수 프로빈스 이벤트 ID 유일; T04 한 시설 동일 손실 중복 반영 금지; T05 전투 피해와 재해 피해 같은 잔존량 2번 차감 금지; T06 예견된 가뭄이 당월 생산 한 번만 제약; T07 Stage10 신규 재해가 Stage4 산출 소급 취소하지 않음; T08 POP 사망/부상·이주 단일 인구 원본; T09 실제 방재 시설 완공 전 보너스 없음; T10 실제 복구비/보험·예산 회계; T11 동일 시드/명령 다른 worker 및 저장·재개 동치; T12 대형 재해/30k Province/100만 POP 실제 성능.

**상태:** 상세 설계 확정, 자연재해 피해 통합 C++ 미구현.
