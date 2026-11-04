# H-D6 — 국가 간 영향력·다면적 의존·비공식 영향권

> phase-h-diplomacy/1.0 · 2026-11-04 · SPECIFIED / C++ NOT_IMPLEMENTED
> 선행 H-D0/H-D1~D5, G-D0/G-T1~T5, C-E3/E4, C-M 군사능력/원조, D 기술/건설·투자, POP 사회/교육/이주, E 영토, H-D7 국제위기.
> **정식 시스템 명칭은 '영향력(Influence)'**. '세력권'은 지도의 파생 설명일 뿐 소속/행위/포인트 원장이나 별도 승리 자원이 아니다.

## D6-1. 개념을 정확히 분리

- **영향력** = A국이 B국의 실제 선택지·손익·안보·정치적 협상에 영향을 줄 수 있는 구조적 능력의 방향성 있는 평가. A→B와 B→A는 다름.
- **신뢰/우호·인식** = H-D1에서 평가. A가 B를 싫어하고 불신하면서도 실제 수출통제/차관·군수지원으로 B에게 강한 영향력을 행사할 수 있다. 우호와 영향력을 같은 값을 서로 복사해서는 안 된다.
- **법적 종속·보호국** = H-D4 Treaty/권한 관계. 경제적으로 우세한 A가 B의 외교권/주권을 자동 소유하지 않는다.
- **국제협력체** = H-D5 조약·헌장상 회원/기관. 소속국이라고 모두 같은 힘이나 의무를 가진 것이 아니다.
- **비공식 영향권** = 특정 나라가 분야별로 비교적 강하고 오래 지속되는 영향력을 행사한다는 **지도/보고용 파생 레이블**. 여러 국가·분야가 중첩 가능하며 편입/탈퇴/확장 버튼 없음.
- **상대의 정책 수락/협조** = H-D2의 제안·협상 결과. 영향력이 높은 것이 승낙 의무나 지배권이 아니라 상대방의 비용·대체수단·정치/군사적 저항에 영향을 줄 수 있는 한 가지 설명요인임.

영향력은 '좋은 관계'나 '정당성'과 같은 가치판단 점수가 아니다. 경제적 제재를 통한 압박, 군사보호에 의한 의존, 합의된 문화·기술협력이 서로 다른 방식으로 효과를 낼 수 있다.

## D6-2. 관측 입력·차원별 실효 영향

| 영역 | 원본 지표/관계 | 실제 영향력을 키우거나 제한하는 조건 |
|---|---|---|
| 경제·무역 | B의 상품별 A산 수입/수출·시장 접근, B기업의 A 투자·외채·은행 결제/대체시장, A기업이 소유한 실제 핵심시설 | B 전체 소비/산업에서 해당 거래의 비중, 가격/대체재·대체 공급자 가용 용량, 물류/재고, 제재 실제 집행 능력 |
| 금융/투자 | A 자금 제공자의 B 내 채권·지분/담보, B 금융 시스템의 A 결제통로 의존, 차환/외환 부족 | 자금만기·실제 채권 집행/국내 법률, 대체자금/유동성, B의 자체 수익·정부재정/은행 건전성 |
| 군사·안보 | A의 B국 군사원조·장비정비/탄약 조달, 기지·주둔·보급망, 방위보장 TreatyTerm, 적법 작전/지원 경로 | B 자체 방위능력·대체 원조국·군수 지속가능성, 유효 조약/군사지휘·거리/수송/국내 승인 |
| 정치·외교 | A가 B의 특정 요구를 국제회의에서 실제 지원/거부, H-D1 신뢰/접촉과 H-D5 협력체 의결·대표권, C-P의 실제 정책연합 | 제3국 지지/거부, B 국내 의회·선거·정치운동/집행 가능, 발언권·표결권/조약의 범위·비준 |
| 사회·문화 | 언어·교육/장학생·유학생·이주/상인/기술자·언론/학술교류, 실제 문화·지식 네트워크 | 접근성/정보·교육기관·POP의 실제 선택/언어·이주/정치권리, 효과의 장기 지속/대체 네트워크 |

내부 계산은 최소 economic, military, political_diplomatic, social_cultural 네 묶음으로 **시스템 사용자에게 구분해 제공**한다. 필요시 경제 내부에 trade/finance/ownership, 군사 내부에 procurement/security 등의 세부값을 따로 보관한 **파생 기여 분석**을 제공한다. 단지 나라 규모/생산량이 크다는 이유로 전세계 소국에 자동 영향력을 부여하지 않는다.

### 구조적 경제 의존의 유효 계산(공식 형태 예시)

해당 핵심 품목 k와 B의 일정 관측기간/실제 산업수요에서:
- exposed_quantity(A,B,k) = A가 실제로 통제 가능한 대B 공급량(단순 A기업 해외 매출과 같은 거래를 중복 합산하지 않음)
- substitutable_quantity(B,k,horizon) = 동일 기간 B가 다른 공급자/국내 대체·기존 재고·유효 운송으로 확보 가능한 추가량
- at_risk_quantity = max(0, min(exposed_quantity, required_use_quantity) − substitutable_quantity)
- at_risk_fraction = at_risk_quantity / max(required_use_quantity, minimum_valid_quantity)
- estimated_cost = 공급차질 시 필요한 실제 산업투입/소비·대체가격·재고/시간/손실 평가(미상 수치는 unknown)

예: B의 월 철강 필요 100 중 A산 60, A가 완전 수출금지할 수 있고 **같은 달 실제 대체/비축 능력이 추가 35뿐**이라면 최대 미충족 25라는 **경제적 위험 시나리오**가 산출된다. 이것은 B가 A의 외교 요구를 25% 확률로 수용한다거나 영향력 포인트가 25라는 뜻은 아니다. B는 물자배급·산업구조조정·제3국 원조·정치적 저항 등 다른 선택을 할 수 있다.

- **계약상 제재 가능성**과 **실제로 손실을 발생시킬 수 있는 수단/비용**을 따로 계산. A가 공급을 끊으면 A의 기업도 매출 손실을 본다. A→B 효과와 B→A의 역방향 효과가 비대칭적으로 발생한다.
- 기업 소유·은행 대출·무역금융 등 동일 핵심 자산/거래에 겹친 exposure를 같은 위험원천 계약/lot/asset ID로 dedupe하거나 조건부 의존으로 결합. 실제 상관된 위험을 독립 합계로 더하는 방식 금지.
- 조약/경제관계에 대한 장기 **긍정적 접근/협력** 능력도 평가. 강압적 제재능력만 영향력이라고 정의하지 않는다. 다만 '한 국가가 요구를 관철하는 확률'로 단일 환원하지 않는다.
- 관측정보 unknown을 0으로 치환하지 않는다. 역사자료가 부정확하면 추정범위/출처와 기준시점을 기록하고 AI는 자기 정보뷰에 따라 의사결정한다.

## D6-3. InfluenceAssessment 데이터 계약(쓰기 원본 아님)

InfluenceAssessmentKey:
(A CountryId, B CountryId, observed_month, WorldSnapshotId, ruleset_version, economic_revision, financial_revision, trade_revision, military_revision, government_treaty_revision, pop_culture_revision, market_access_revision)

출력:
- directional: A→B, B→A를 각각 반환
- dimensions: economic/trade/finance/military/political/social contribution summaries, independent uncertainty
- observed_evidence: actual trade/funding/shipments/defense/terms/education event refs
- options_and_countermeasures: B의 대체경로·차환·방위지원·정치적 대안
- source_history: 변화의 원인 사건/적용기간·캐시 생성/무효화 revision
- optional_display_bands: '낮음/중간/높음' 같은 **UI 해석구간** (model_version, 기준/기한 명시; 실제 권한/외교수락의 문턱값으로 사용 금지)
- unknown/estimated/supported confidence flags

**영향력 값 직접 수정 API 없음.** 외부 명령에서 SetInfluence/IncreaseInfluence/JoinSphere와 같은 명령을 정의하지 않는다. 업데이트는 투자/무역/지원·조약/정권·POP 교류 등 실제 원장/사건 변경 후 다음 조회 시 자동 재평가한다.

## D6-4. 다국가 중첩·비공식 영향권 표시

- 각 B 대상 국가에 대해 A,C,D 등 여러 주체의 influence vector를 독립 조회. A가 B에 경제적으로, C가 군사적으로, D가 사회·문화적으로 동시에 강력한 영향력을 가질 수 있다.
- 지도의 influence overlay는 경제/군사/정치/문화 등 선택된 분야에 대한 **평가/계약 시점 표기**다. 범위/대상이 확정되어 있지 않은 데이터에서는 '영향 미상'으로 표기하고 색칠 범위를 단정하지 않는다.
- 비공식 영향권 표시는 일정 관측기간 이상의 지속적·상대적 우위/신뢰도·자료 충분성·히스테리시스 조건을 만족한 경우에 한정하고, 격차가 작거나 복수 영향이 비슷하면 contested/overlapping/no_dominant_actor를 허용한다.
- 위 UI 범주/히스테리시스는 **표시용 파생 분류**이며 실제 경제·조약·군사/정치 결과를 다시 바꾸지 않는다. 어떤 지도색도 새 소속관계나 강제통상권을 만들지 않음.
- 국가의 국제협력체 다중가입 여부와 조약상 보호국 지위는 별도 레이어. H-D6은 이들 체제의 실효성·경제적 파급을 조회하지만 별도의 자동 회원/종속국 변경을 수행하지 않는다.

## D6-5. 정치·외교상 영향력의 사용

- H-D2 협상/중재에서 특정 제안이 B에 초래할 실제 비용/보상·대체수단/정책 가능성을 조회한다. 영향력 지표를 단순 '+' 보너스·전쟁 가능성 강제값으로 전달하는 대신 **평가의 근거인 원본 위험노출·행사수단을 제공**한다.
- H-D7 국제위기에서 A의 실제 군사지원·제재 실행능력은 B의 손실/위기 확대·제3국 개입 의사에 영향을 줄 수 있다. 실제 위기 참여/전쟁 선언은 적법한 H/C-P/C-M 사건이며 영향권 표시는 자동 의무가 아니다.
- A 기업의 B 해외투자↑ → B 고용/무역·채무 의존과 A 정치적 접근 기회 변화(자동 파생) → 차월 외교적 협상 후보 평가로 반영. **A가 투자한 순간 B의 법적 주권·정치성향이 1단위씩 자동 변하는 것은 아님**.
- C-P/POP의 친A 성향 정치집단이 집권하면 외교협력 가능한 정책대안이 바뀔 수 있으나, 각 개별 POP이 무조건 A를 좋아하게 되는 초자연적 변형 금지.
- 강제제재·외교압박으로 B의 산업/가계에 피해가 실제로 발생하면 C-P POP 정책/여론·신뢰에 반작용할 수 있다. 동일 피해를 무역→영향력→정치의 과정에서 두 번 비용으로 과금하지 않는다.

## D6-6. 시간·성능·캐시

1. Stage0 유효 원장/조약·시장에서 국가간 노출·정보 접근의 관측 스냅샷을 고정.
2. Stage1 외교제안의 기대 손익·대체수단 조회에 **같은 Stage0 평가**를 사용. Stage3/7/8 당월 거래가 Stage1 이전 판단을 다시 바꾸지 않는다.
3. Stage3/7의 실제 무역·stage5/8 재정/채권·stage10 군사/인구 및 stage9 조약의 후속 영향은 원본 사건으로 남고, **다음달 Stage0** 또는 완료된 snapshot 조회 시 영향력 캐시 갱신.
4. Stage11 전역 사건/계약·영향력 설명 provenance, 관련 원본 revisions/결정론을 검증. 캐시가 stale이면 폐기 후 재생성하되 그 캐시의 값만으로 국제사건을 다시 실행하지 않는다.
5. 성능: 변경된 무역·투자·군수·조약·문화 네트워크의 bilateral country-pair changed-set/희소 인덱스 위주. 모든 국가쌍·모든 상품·모든 프로빈스를 매월 무조건 완전 전수 재평가하지 않는다. 사용자가 선택한 국가/세계지도 보기에서 요약 조회·사전 집계.
6. 동일 1/N worker/seed/경제 원장·외교 사건에서 동일 view/설명/요약. 비결정적 hash unordered iteration/부동소수 누적 순서 의존 금지.

API: AssessInfluence(actor_A,actor_B,snapshot), GetInfluenceContributions(actor_A,actor_B,domain,snapshot), ListObservedInfluenceTargets(actor_A,scope,month), CompareInfluenceOnTarget(target,domain,snapshot), ExplainInfluenceChange(A,B,month_range), QueryInformalInfluenceOverlay(scope,domain,confidence), EstimateSanctionLeverage(A,B,policy,scenario). **쓰기형 AddInfluencePoints/EnterSphere는 존재하지 않는다.**

## D6-7. 불변식·오류·검증

- HD6-INV01 외교 영향력은 원본 경제/군사/정치/문화 자료의 read-only 파생 평가. 임의 증가·감소/별도 원장 없음.
- HD6-INV02 A→B≠B→A, 차원별 분리·상반된 우호도/법적 관계/중첩 영향권 허용.
- HD6-INV03 영향력 평가/정치적 수락은 실제 대체수단·집행능력·비용·국내 조건을 반영하고 고정 수치 문턱값으로 강제하지 않음.
- HD6-INV04 같은 원천 상업거래/투자/대출/군지원/제재의 중복 기여·부채/채권/상품 double count 금지.
- HD6-INV05 영향권 지도 표시로 법적 조약·종속/소속·국민/영토·재고/돈의 새 사건을 발생시키지 않음.
- HD6-INV06 unknown/estimated/observed/zero, 정보 접근 권한과 유효시점·출처/설명 보존.
- HD6-INV07 source revision 갱신·월별 시점에 따른 영향력 캐시 무효화/읽기 정합성.
- HD6-INV08 월내 G↔H recursive self-amplification·동월 가격/협상 반복 계산 금지.
- HD6-INV09 국가·프로빈스·영토 세분과 무관한 정확한 대상/분야 scope를 명시.
- HD6-INV10 같은 world/seed/snapshot/worker 구성에서 파생 평가·관측 정보·설명 해시 결정론.

오류: HD6_ERR_STALE_SNAPSHOT, UNKNOWN_EVIDENCE, INVALID_ACTOR_PAIR, INCOMPATIBLE_VALUE_BASIS, SOURCE_DOUBLE_COUNT, INVALID_DISPLAY_SCOPE, MISSING_REVISION. 입력 불명은 unknown 결과; 원본 훼손은 HARD ERROR.

시험: [DIPLOMACY_ACCEPTANCE_TESTS](DIPLOMACY_ACCEPTANCE_TESTS.md)의 HD6-T01~T12. 본 문서는 정교한 영향력 평가의 설계이며 구현/역사적 수치 보정·회귀는 미수행.
