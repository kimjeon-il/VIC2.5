# VIC2.5 C-P4 — 정당·선거·의회구성·정권교체

- 상태: 설계 확정, 구현 전
- 계약: `phase-c-politics/1.0`
- 근거 자료: International IDEA의 Electoral System Design Database / Voter Turnout Database (제도 유형과 투표 통계 개념만 참조)

## P4-1. 정당은 이익집단 및 정부와 다르다

| 객체 | 핵심 필드 |
|---|---|
| `PoliticalParty` | party_id, 등록/금지/해산, 시기별 `PartyPlatform`(쟁점별 공약), 조직망·재무계좌, candidate_refs[], coalition_refs[] |
| `PartyAffiliation` | party/actor/legislative_bloc 간 유효기간 있는 지지/소속. 집단의 전원 자동 당원 처리 금지 |
| `ElectoralLaw` | law_version, contest_type, electorate_rule, registration_rule, districting_rule, ballot_type, election_formula, seat_rule, threshold, term |
| `Election` | election_id, contest_id, scheduled_month, election_status, law_version_at_opening, districts[], provenance |
| `ElectoralDistrict` | district_id, affected_regions_or_pop_slices[], chamber/contest, seat_count, effective_month, boundary_revision |
| `Candidate` / `Nomination` | candidate_actor_ref or party_list_ref, valid district, registration/legal qualifications, list_rank, signed nomination event |
| `VoterEligibilityView` | district/contest/POP slice별 법적 적격 인원 파생(인구 원본 아님) |
| `VoterRegister` | district/contest별 유효 등록인원·기한/행정기록; 별도 POP 인구수 저장 금지 |
| `BallotEvent` | district/contest, registered/eligible/cast/valid/invalid/abstain, option_votes, counting_method, evidence/revision |
| `SeatAllocation` | chamber, contest, district, winner_party/actor, seats_int, allocation_method/version, source_ballot_event |
| `CoalitionAgreement` | coalition_id, member_parties/seat_blocs, cabinet_positions, policy_constraints, governing_support, validity |
| `GovernmentFormationEvent` | formation_id, nominator, constitutional_rule, confidence/appointment_refs, tenure_effective_month |

**국가원수 선출, 하원 선거, 상원 선거, 지방선거, 직능선거, 국민투표**는 각각 다른 `Election/Contest`를 가진다. 보통선거가 없는 역사시기에는 적절한 임명/승계/간접선거가 대신 존재하며, 선거를 강제로 생성하지 않는다.

## P4-2. 선거권·유권자등록·실제 투표의 세 단계

1. `legal_eligible(slice, contest, t)`는 해당 법률의 나이·시민권·거주·재산/납세/문해력/성별/신분·복무 등의 명시적 요건으로 결정한다. 어떤 조건도 직업/문화만을 이유로 자동 규칙으로 삽입하지 않는다. 제한선거·여성선거권 확대·계층별 제한 등은 *유효 법률에 규정된 경우*에만 적용한다.
2. 법적 선거권(`eligible`)과 실제 등록(`registered`), 실제 투표 접근성·기표(`ballots_cast`)를 분리한다. 법률상 투표권 600명 중 550명 등록, 420명 투표는 가능한 상태다.
3. 1인 1표 선거의 기본 보존식: `0 <= valid <= cast <= registered <= eligible <= eligible_POP_population`; `cast = valid + invalid`, `registered = cast + abstained_registered`. 단, 복수표식/선호투표식에서는 **투표용지 수/유권자 수/후보 선택 표시 수를 구별**한다.
4. 선거마다 동일인이 **한 선거구 한 유권자등록**에만 포함되어야 한다. 서로 다른 선거/기관에서 동일인이 투표하는 것은 중복인구가 아니다.
5. 선거구 획정(`DistrictAssignment`)은 인구지역과 별개 유효기간 관계다. 지역이 여러 선거구로 나뉘면 인구 집단도 할당 partition/추정자료를 통해 일의적으로 분배한다. 같은 하원선거에서 동일 사람을 두 선거구에 포함하는 것은 불가.
6. 유권자와 피선거권자의 기준, 정당 등록/출마/후보 자격은 별도로 검사. 정당 지지자가 전원 당원 또는 표를 던진 것으로 자동 처리하지 않는다.
7. 선거 비정상 행위·행정 착오 등을 역사사건으로 모델링할 때 법적 개표와 관측/공표 결과를 구분한다. 조작이 발생해도 가짜 투표가 실제 주민으로 추가되지는 않고, 부정확한 공식 보고와 실제 기표를 **별도 로그**에 남긴다.

## P4-3. 투표·기권·무효·후보 선택

- 단계 9c에서 당월 확정된 쟁점/정당 입장·유권자 분포·현실적 접근성·공약/정보의 인지·정치제도에 대한 인식을 입력받는다.
- POP별 `turnout_prob`과 조건부 `choice_weight`는 이전 선거 경험, 해당 쟁점 중요도, 실제 정당/후보 대안, 참여비용, 법률 및 관측 가능한 정보에 따라 **버전 관리되는 함수**로 결정한다. 이것은 게임 내부 모형의 입력·결과 규칙이며, 특정 실제 사회집단을 설득하기 위한 전략 도구가 아니다.
- 한 PopulationSlice의 적격자 중 **등록/투표한 실제 정수 인원**만 계산한다. 확률은 ppm 고정소수점, 부족/초과·반올림은 POP-D3의 largest remainder(동률 안정적 ID) 방식. 이미 다른 선거구에 배분된 인원은 재사용 금지.
- 기본 투표 배분 순서: 법적 적격 → 등록/투표 접근 → 실제 투표/기권 → 무효표 발생 → 후보/정당 선택 → 선거제도별 의석 변환. `party_support` 인구를 그대로 표로 대체하지 않는다.
- 후보 명부의 비실존 인물도 당선되면 `OfficeTenure`가 필요하고 역사적으로 특정 이름이 필요한 경우만 P1의 `PersonAnchor`로 묶는다. 의석을 주민 한 명처럼 총인구에 새로 더하지 않는다.
- 조직 회비·선거 비용·정부 선거관리비는 E3/E4 금융 원장/실제 서비스 구매로 결제한다. 공약 채택이나 홍보 결제만으로 자동 표가 추가되지 않는다.

## P4-4. 의석 배분 체계

선거 방식(`electoral_formula`)과 투표용지 구조(`ballot_structure`)를 분리한다.

**초기 구현의 필수 두 방식**:
- `FPTP`: 단일 선거구 의석 1석. 유효 후보별 정수 표가 가장 큰 적격 후보가 1석을 얻는다(동률시 법률에서 정한 재투표/추첨·제비 또는 결정론 ID 정책 명시). 상대다수와 과반수를 혼동하지 않는다.
- `ListPR_DHondt`: 다인 선거구. 후보정당별 투표수로 정수 몫 `votes_i / (seats_i+1)` 비교하여 1석씩 배정. **분수 비교는 정수 교차곱과 안정적 동률 규칙**으로 처리하고 유효 법률상 최소득표율/명부 후보정원/결원 규칙을 적용한다.

**확장 가능한 방식 인터페이스**: Sainte-Laguë, largest remainder/Hare/Droop, 2회투표, 선호투표, SNTV/Block vote, 혼합(MMP/parallel), 간접선거·직능/종교/신분별 임명 의석. 방식마다 별도 단위 테스트 및 선거법·선거구 정원 계약이 필요하다. `MMP`와 단순 병립을 같은 계산으로 처리하지 않는다.

**의석 불변식**: 각 `Chamber.seat_capacity` = 법이 지정한 정원(총선출+임명+예약+결원). `sum(occupied_seats) + vacant_seats = capacity`. 같은 의원 의석/정당 블록이 정원의 두 배로 기록되지 않는다. 의석수와 유효표 수/실제 유권자 수는 다른 단위다.

### 수치 예시 (가상 선거)

- 인구 1,000명 중 법적 유권자 600명, 등록 550명, 실제 투표 420명, 유효 400표, 무효 20표, 등록했으나 투표하지 않은 사람 130명.
- 유효표는 A정당 220, B정당 130, C정당 50, 합계 400.
- **3석 비례대표 D'Hondt**: 몫 상위 순서 A 220, B 130, A 110 → A 2석/B 1석/C 0석. 같은 표를 **1석 FPTP 가상선거**에 사용한다면 후보선거구에서 A후보 1석(정당명 자체에 투표하는 방식과 후보 선거는 별개의 예시).
- **총 600명 모두가 투표했다**고 기록하거나 유효 400표와 무효 20표를 420명에 더해 새 인구를 만들면 실패.

## P4-5. 정부 구성과 임기/연립정부

1. 선거결과는 의회의 정수 의석 배분일 뿐 즉시 정부교체가 아니다. 해당 헌정질서의 정부 구성 방식(직접 대통령선거, 의회 신임투표, 군주 총리임명, 군정 내부 선임, 군소 연립, 국무위원 선출 등)에 따라 `GovernmentFormationEvent`를 시도한다.
2. 정당별 연정 제안/정책 협약은 `CoalitionAgreement`로 표현하고 예산과 실제 의원/기관의 찬성 조건을 검사한다. 연정 구성비·집권 정당·정책 합의를 별도 기록한다.
3. 의회 과반이 필요한 제도에서 정부 신임투표가 실패하면 정권을 성공 처리하지 않는다. 과도정부·재선거·재구성·의회 해산은 법적 절차에 따라 예약한다.
4. 불신임·총리 사퇴·정당 분열·각료 교체는 정부 및 의석·정당 소속 사건이며, 국가 인구·토지·국채·법률·군수품의 별도 원본 변경을 일으키지 않는다.
5. 법률상 임기 종료와 비정상적인 조기해산/비상 임기연장은 별도 사유와 유효 법률을 가진다. 총선 주기는 고정된 '4년마다'로 모든 나라에 강제하지 않는다.
6. 단계 9c에서 선거가 확정되어도 새 의회의 표결·내각·법률은 **월 m+1 이후 유효한 권한**으로 검증한다. 당월 이전 단계의 생산·세금·투표권을 소급 바꾸지 않는다.

## P4-6. 선거/정치 지표

법적 선거권 인구, 등록률, 실제 투표자/투표용지/유효·무효, 선거구별 득표·의석·결원, 선거법 버전, 선거구 할당, 정당·무소속 의원, 집권/야당·연정·입법 통과 여부를 기록한다. 무선거제 국가는 선거권률을 근거 없이 100%라고 설정하지 않고 적절한 `not_applicable`을 제공한다.

## P4 검증 게이트

- P4-G1: 1,000명 중 법적 적격 600/등록 550/투표 420/유효 400/무효 20의 항등식이 맞는다.
- P4-G2: 220·130·50표의 D'Hondt 3석은 2/1/0석, 유효표는 400; 총인구는 변하지 않는다.
- P4-G3: 두 선거구에서 동일인 중복 등록/투표 금지. 선거구 재획정은 POP/지역 이력과 연동.
- P4-G4: 선거권 제한·정당금지·무선거 상태는 실제 유효법과 일치하고 단순 POP 직업/문화로 자동 지정되지 않는다.
- P4-G5: 의회 의석 변동이 자동으로 정부교체·국채 소멸·법률 변경을 발생시키지 않는다.
- P4-G6: 동일 월 총선/정권구성이 완료돼도 새 권한이 이전 단계의 임금·정책·세금에 소급 적용되지 않는다.
- P4-G7: 유효법률/투표/등록/의석 규정이 다른 선거제도에 일괄 적용되어 계산을 망치지 않는다.
- P4-G8: 정당·선거기관의 선거비가 실제 금융원장에 지급되며 지지자/당원이 자동 복제되지 않는다.