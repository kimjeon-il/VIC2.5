# VIC2.5 C-P2 — 법률·정책·입법절차·행정집행

- 상태: 설계 확정, 구현 전
- 계약: `phase-c-politics/1.0`
- 상위: C-D0의 월별 12단계·법률 차월발효와 C-E4의 과세권·예산·실제 집행

## P2-1. 규범·정책·경제효과는 서로 다르다

| 객체 | 핵심 필드 / 의미 |
|---|---|
| `LegalNorm` | norm_id, 법적 종류(헌법/법률/칙령/명령/조례/조약), 규범 위계, 발의·승인 권한, 적용지역·대상, 유효기간 |
| `LawVersion` | law_id, revision, parent_version, adopted_month, promulgated_month, effective_month, expiry_month, source_event_id |
| `PolicyProvision` | provision_id, norm_version, policy_domain, 조건식, 권리/의무/금지/면제/자격/권한, 상충 해소 기준 |
| `Bill` | bill_id, proposer_office, issue/affected_provisions, proposed_version, current_stage, dependencies, status |
| `LegislativeVote` | chamber_id, motion_id, quorum, eligible_seat_ids_or_blocs, for/against/abstain/absent, vote_method, decision_id |
| `ExecutiveAction` | action_id, authorizing_provision, implementing_agency, region/recipient scope, budget/source, expected/actual outcomes |
| `RuleEvaluation` | subject, place/time, applicable provisions, interpreted result, legal_basis, unknown_source_flag |
| `EnforcementCase` | case_id, law_provision, institution, affected_entity/slice, staff/resources reserved, pending/completed/failed, remedies |

헌법 개정·조세율 변경·선거권 변경·토지제도 개혁·노동규제·교육/의료 혜택·군복무 법은 **동일 법률 규약**을 사용한다. 각 분야의 실제 변화는 `Law`가 금액이나 사람을 직접 편집하는 게 아니라 E1~E5/POP/M 모듈에 유효규칙을 노출하고 원장 거래/사건을 발생시키는 것으로 처리한다.

## P2-2. 법률 분야와 복합 적용조건

기본 정책 영역: 국체/헌정·권력/선거/시민권·소유권/토지·노동/농노/부역·상공업/면허/시장·금융/통화·세금/예산·교육/보건/복지·종교/표현/결사·치안/사법·병역/군율·지방자치·대외무역.

`PolicyProvision`의 조건은 단일 enum이 아니라 논리조건(AST나 검증된 규칙테이블)으로 표현한다. 예를 들면 노동법 적용은 `age >= threshold AND region in jurisdiction AND service_status is civilian`과 사업장 업종 예외를 함께 고려한다. 문화·종교·성별에 따른 법적 차별도 **해당 시대 실제 법률의 조건**으로만 표현하며 그 자체를 선천적 능력/정치 성향으로 취급하지 않는다. **I-C1/1.1 연계:** 종교별 사회적 차이는 종교 ID에 해당하는 실제 국교/종교의 자유·교육/공직/과세/기관 관련 규범, 관할·효력시점 및 집행/시설·재원 때문에 발생한 경우에만 허용한다. 종교 중립 법률에서 종교 자체에 고용·학력·소득·정치 성향 보너스/패널티를 부여하지 않는다.

- 여러 규범 충돌 시 헌정 질서가 정한 규범 위계·후법 우선·특별법 우선·관할 우선·법원 판단을 유효 규칙에서 결정한다. 일괄적으로 최신 법만 남기지 않는다.
- `unknown` 법령·자료는 `no law`나 `no restriction`과 다른 상태다. 실제 역사데이터 부족을 임의로 '보통선거 허용'이라 메우지 않는다.
- 법적 권리(`de_jure_right`)와 행사 가능/실효(`de_facto_access`)를 구분한다. 권리 공표만으로 학교·투표소·법원·의료공급이 즉시 충분해지지 않는다.
- 제정·폐지 후에도 해당 법의 과거 적용과 의무·채권/채무/사건 기록은 유지한다.

## P2-3. 입법 절차의 상태 기계

`draft → formally_introduced → deliberating → chamber_votes → second_chamber_if_any → assent_or_veto → promulgated → scheduled_effect → effective`, 또는 `withdrawn/rejected/expired/suspended/overridden/repealed`.

1. 발의권·기본 적격성을 C-P1 권한에서 조회하고, 발의 당시의 법령 버전을 고정한다.
2. 의회 정족수, 출석, 무효/기권 표, 찬반 표, 특별다수(헌법개정/조약) 등 의결 규칙을 세부 설정한다. 당의석수와 실제 찬성표를 동일시하지 않는다.
3. 상·하원 또는 자문회의·군주/대통령 재가·주민투표·사법심사 등 **존재하는** 선행 절차만 실행한다. 거부권·재의결·시한·관할권 위반은 명시적 상태를 남긴다.
4. 법안 통과, 공포, 실제 발효는 구별한다. **월 m에 새로 채택된 법은 원칙적으로 m+1 단계 0부터 효력 발생**하며, 기존 월 m 지출/임금/납세/징병을 소급 변경하지 않는다.
5. 월 m 이전 이미 유효한 법률에 근거해 승인한 정부 집행 명령은 m 단계 1부터 실제 집행할 수 있다. 예산승인과 실제 자금확보/상품 인도는 C-E4에 따른다.
6. 긴급명령/계엄/비상권력도 `AuthorityGrant`·`EmergencyProvision`·기간/통제 절차가 있어야 한다. 월중 예외적 즉시효력을 원한다면 C-D0의 일자/서브틱 계약을 정식 확장하기 전에는 예약 월초만을 지원하고 소급 계산은 금지한다.
7. 회의 절차·표결 순서와 법적 논란은 사건 ID와 provenance로 기록한다. 이미 확정된 법적 행위를 수정할 때는 새 시정 사건/법원의 결정으로 처리한다.

## P2-4. 집행과 실제 효과

법률이 정한 **권리·의무**가 존재하는 것과 법률이 정한 **실제 결과**가 달라지는 것은 별개다. 

- 조세: `TaxRule/TaxAssessment` 발생 → 실제 징수인력과 납세협조·유효 재원 → `FinancialLedger` 실제 납부. 세수가 예상과 달라질 수 있다.
- 노동법/최저임금: 적격 노동제약/기준임금 적용 → 단계 2 고용·임금청산. 기업의 법 위반/미지급이 적발되면 별도 법집행 사건; 모든 사업장이 무조건 즉시 준수한다고 가정하지 않는다.
- 교육/의료·복지: 자격/권리 설정 → 기관 실제 고용/시설/재고/국고지급 → 서비스 슬롯/수급. 예산 부족·접근 불가능 시 미충족 기록.
- 재산·토지: 법개정이 소유권을 바꾸도록 명시한 경우에만 해당 `OwnershipLedger` 이전·보상·분쟁·기한이 있는 사건을 발행한다. 단순 조세정책 변화로 토지가 새로 생기지 않는다.
- 형사·경찰/징병: 적용법/권한/실제 치안/군/법원 자원, 절차 요건, POP의 법적 상태/군복무 전이를 통해 처리한다. 구금·부상·사망은 POP-D2 유효 사건만 허용.

집행량의 기본 한계: `min(법적 대상 사건, 담당기관의 실제 인력·행정처리 능력, 통신/도달 가능성, 집행비용 충당량, 필요한 재고/시설)`. 법 준수는 법률·조직 인센티브·집행·자발적 협조 등에서 나오는 관측 결과로 둔다. 모델 속 제도집행 능력이 0이라도 권리 자체가 없었다고 역기록하지 않는다.

## P2-5. 정책변경 효과와 피드백

정책의 예상효과/실제효과/정치적 수용은 별도 필드다. 정책의 결과가 임금·세금·교육/건강·물가·재산·군사 복무를 변경할 때 **이전/새 원장 계약**에 맞춰 후속 월 결과를 만들며 이를 C-P3의 정치적 신뢰·불만·조직 동기에 입력한다. 반드시 동일한 정책이 모든 집단에 동일 만족/불만을 유발하도록 고정하지 않는다.

## P2 검증 게이트

- P2-G1: 의회 미승인·거부권 미처리·권한 외 지역 법률은 확정/발효 거부.
- P2-G2: 월 100에 제정된 선거권 확대법은 기본적으로 월 101부터 유권자 자격을 변경하고 월 100의 선거·임금을 소급하지 않는다.
- P2-G3: 부과세 100, 실제 징수 70이면 미수/체납 30을 기록하며 정부 현금이 100 증가하지 않는다.
- P2-G4: 교육·복지법 통과만으로 실제 서비스와 현금이 생성되지 않는다.
- P2-G5: 헌법·법률·조례/지방 권한의 상충은 우선규범과 관할 근거로 해결하거나 명시적으로 오류 처리.
- P2-G6: 폐지된 법에 의거한 과거 사건의 회계/법적 근거가 사라지지 않는다.
- P2-G7: 국적/법적 지위·소유권 변경에 필요한 POP/Ownership 전이를 빠뜨리면 월 커밋 거부.