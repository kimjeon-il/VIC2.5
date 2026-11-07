# VIC2.5 — Phase C-P1~P5 정치 시스템 세부 설계

**상태: P1의 헌정·내각·정통성·공공기관 연결 코드 일부 구현(2026-11-07, 개발 브랜치·미검증). P2~P5 전체 및 P1 세부 설계는 미완성.**

**실제 P1 코드/한계:** [P1 네이티브 구현 기록](P1_IMPLEMENTATION_2026-11-07.md).

**P4 정당·선거 부분 구현:** [다대다 정당 관계·독립 선거일정·해산/개헌 코드 기록](P4_PARTIES_ELECTIONS_IMPLEMENTATION_2026-11-07.md) — 개발 브랜치만, 빌드·검증 전.

**P2 법률·증보·정책 일부 구현:** [기본법·수치 조항·증보·선출직 임기법의 실제 코드와 한계](P2_LAWS_AMENDMENTS_IMPLEMENTATION_2026-11-07.md).

**2026-11-08 후속 구현:** [분야별 실제 법적 효과, P3 증보 자동 협상, P1/P4 양원제 의결, POP-귀속 개인별 선출직 임기](P2_ADVANCED_POLITICS_2026-11-08.md).

**P3·P5 2026-11-08 실행 연결:** [실제 POP 정치태도·이익집단·운동, 청원·시위·익일 파업·위기·실제 군사교전·합의](P3_P5_IMPLEMENTATION_2026-11-08.md). 개발 브랜치에서 코드·집중 검사는 작성했으나, 컴파일·CI·대규모 회귀 전이며 전체 정치시뮬레이터의 기능 완성을 주장하지 않음.

**P5-G 2026-11-08 유격전 보강:** [지역 유격전 일지·실제 무장부대·익일 경제 접근 차질·정규전 전환/종결](P5_GUERRILLA_IMPLEMENTATION_2026-11-08.md). 별도 브랜치에서 코드와 회귀 시나리오를 작성했으며 실행 검증 전.  
계약: **`phase-c-politics/1.0`** · 2026-11-03

이 문서는 기존 POP·경제·군사 공통 설계에 연결되는 **정치 시스템의 규범·데이터/이벤트 계약**이다. 다른 브랜치의 인구·사회 기초 구현이 전체 정치엔진을 의미하지 않으며, 실제 GitHub 병합이나 C++ 실행 검증 결과를 주장하지 않는다.

## 설계문서 색인

| 단계 | 문서 | 핵심 영역 |
|---|---|---|
| P1 | [국가·정부·의회](C-P1_STATE_GOVERNMENT_INSTITUTIONS.md) | 헌정 권한·국가/내각/국가원수·단원/양원·행정과 중앙/지방 |
| P2 | [법률·정책·집행](C-P2_LAW_POLICY_ENFORCEMENT.md) | 입법절차·의결·법적 권리·집행병목·차월 발효 |
| P3 | [POP·이익집단·정치운동](C-P3_POP_INTEREST_GROUPS_MOVEMENTS.md) | 쟁점별 선호, 회원/지지/참가 구분, 다중 조직화 |
| P4 | [정당·선거·정부 구성](C-P4_PARTIES_ELECTIONS_GOVERNMENT_FORMATION.md) | 선거권·등록·투표·FPTP/PR·의석·연정·정부 교체 |
| P5 | [불안정·시위·반란·내전](C-P5_UNREST_REBELLION_CIVIL_WAR.md) | 실제 불만/조직·동원·치안·쿠데타·C-M 무장/점령 경계 |
| 보조 | [정치 ADR](POLITICAL_ADRS.md) | 30개 설계결정, 역사 자료와 수치 보정 경계 |
| 보조 | [인수검증](POLITICAL_ACCEPTANCE_TESTS.md) | 불변조건 12개·통합 시험 시나리오 25개 |
| 예시 | [3석 선거](examples/election_three_seats.json), [선거권 개정](examples/law_effective_next_month.json) | 가상 수치·명시적 예상결과(실제 엔진 저장형식 아님) |

## 상위 계약의 불변 원칙

1. `PopulationSlice.count`만이 인구수 원본. 코호트/경제POP/가구/선거권자/당원·정치운동/군대에서 별도 인구를 쓰기 가능하게 복제하지 않는다.
2. 국가(Country), 정부(Government), 헌정질서(ConstitutionalOrder), 입법기관(Legislature), 공직(StateOffice), 법률(LegalNorm/LawVersion), 정당(Party), 이익집단(InterestGroup), 운동(Movement)은 별도 ID와 유효기간을 가진다.
3. 정치적 지지(선호), 조직 가입(실제 소속), 참여(실제 투표/시위/파업·복무)를 구분. 중복회원/동시 쟁점 지지는 가능하나 동일인의 인구·참여 자원 이중 배정은 불가.
4. 선거권자 적격/유권자 등록/투표율/유효·무효/정당·후보별 득표/의석·정부구성은 서로 다른 원장 기록. 선거가 없거나 간접/임명기관인 시기에도 모형 작동.
5. 정책은 권한·의결/승인 절차·공포·월별 발효/실제 집행을 분리한다. **월 m에서 제정한 법률의 기본 효력은 m+1의 단계 0**. 실제 세금·예산/고용·상품·금융·인구 결과는 각 단일 원장과 사건으로 처리.
6. 민원/치안/교육·보건/선거운영/입법의 실효 처리량은 실제 인력·예산·물자·행정 접근성에 종속. 국가나 정당이 추상 점수를 소비해 돈·인구를 임의 생성하지 않는다.
7. 반란·내전은 실제 POP/군 장비·조직·군사통제(C-M)와 연결. 군사 상세 엔진 구현 전에는 정치적 사건의 준비·조직·정권 효과만 설계하고 전투 승패를 허위 구현하지 않는다.
8. 전체 세계의 월간 staging/rollback·결정론적 정수 배분·사건 멱등성·계약 버전 저장은 C-D0와 POP-D3의 12단계를 변경하지 않는다.

## 기존 경제·POP과의 연계

```
PopulationSlice ──────▶ VoterEligibility / PoliticalAttitude / OrganizationMembership
        │                             │
        ▼                             ▼
Demographic/Economic/Household    Party / InterestGroup / Movement
        │                             │
        ▼                             ▼
Employment / Financial / Ownership / Fiscal / Inventory
        │                             │
        └──────── actual economic outcomes ──▶ issue preferences / participation
                                               │
State + Constitution + Law ◀── Legislature ◀── Election / appointment
               │
               └── rules effective next month ──▶ economic / military / POP ledgers
```

## 월별 실행 규약

기존 C-D0·POP-D3 공개 12단계 변경 금지. 정치 전용 주요 단계: `0 opening_snapshot`(유효 법/공직), `1 planning_and_credit`(행정 집행 예산·권한), `2~8`(실제 경제·공공서비스·법 집행 결과), `9 saving_and_investment`의 하위순서 **9a 경제 저축/투자 → 9b POP 쟁점·운동 갱신 → 9c 선거·정부구성 → 9d 의회·법률 개정 → 9e 시위·치안·폭력 사건 준비/차월 예약**, `10 pop_demography_and_health`(실제 인구·사상/복무/법적 상태 전이), `11 repricing_and_validation`(모든 원장/권한/재현성 확인·원자 커밋).

이 시퀀스는 같은 달 정치 결정이 이미 끝난 4단계 생산·5단계 급여·8단계 정부결산을 소급 변경하는 것을 금한다. 실효법과 새 기관은 다음 달의 계획 단계부터 영향을 미친다.

## 예시·검증 및 근거

- 인구 1,000명, 유권자 자격 600명, 등록 550명, 투표 420명, 유효 400표, 무효 20표. A 220/B 130/C 50표, 3석 D'Hondt: A 2/B 1/C 0석.
- 선거연령 21→18세 법률 월 240 제정, 월 241 발효. 18~20세 40명·21세 이상 60명의 법적 적격자는 월 240에 60명, 월 241에 100명. 등록·투표는 별도 계산.
- 참고: [International IDEA 선거제도 데이터](https://www.idea.int/data-tools/data/electoral-system-design-database), [International IDEA 투표율 정의](https://www.idea.int/data-tools/data/voter-turnout-database), [V-Dem 정치 제도 변수](https://v-dem.net/data/the-v-dem-dataset/), [World Bank 거버넌스 차원](https://www.worldbank.org/en/publication/worldwide-governance-indicators).
- 수치와 사례는 **fictional_test_value**이며 역사적 사실/전망/정당 가치평가가 아니다.

## 다음 구현 순서

1. Phase B 인구/경제 원장과 C-D0의 정치 이벤트/기관·법률 스키마를 결합하는 데이터 어댑터·인구 공유 조회 작성.
2. P1 공직/헌정권한, P2 법률 상태/법해석과 월별 발효 구현 후 재정·노동 제약으로 회귀.
3. P3 정치적 선호·소속/조직 원장과 actual POP 인구·계약/자산 이전을 동기화.
4. P4 선거/의석·연정·정권교체 구현. 독립된 선거구·입법 절차 테스트.
5. P5 조직 동원/파업·치안/민정·실제 C-M 병력/무기·POP-D2 사상자 경계 구현.
6. 경제/군사와 통합된 C-D4에서 1,200개월·결정론·세이브·성능 검증.

**주의:** 문서 설계 완료는 GitHub 반영·실제 C++ 구현/실행·경제/군사 통합검증 완료와 다르다.