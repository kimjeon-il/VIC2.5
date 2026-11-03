# POP-D1 — 자료형·인구 분포·스키마 계약

상태: **설계 확정 (구현 전)** · 계약 버전: pop-design/1.1
선행: [D0 계층 정의](D0_ARCHITECTURE.md) · 후속: [D2 전이 규약](D2_TRANSITIONS.md)

## 형식·타입 정책

- People: **signed int64** 값으로 연산하고 저장 인원은 0 이상의 정수. 음수는 사건 델타에만 허용; 체크된 가감 및 128-bit 중간 합산을 사용한다.
- MonthIndex: **signed int64**의 동일한 역법 기반 정수 월 인덱스. (연·월)과 왕복 변환 가능해야 한다. 같은 저장파일에서 달력 기준을 바꾸지 않는다.
- EntityId: 내부 저장/실행은 고유한 typed ID(64-bit) 사용 권장; 표시/외부 입력은 종류별 명시적 문자열 ID 사용. 0은 무효 식별자로 예약한다. 범주/국가/지역 코드와 객체 인스턴스 ID를 혼동하지 않는다.
- CurrencyAmount: 최소 화폐단위의 signed int64. Quantity: 상품별 스케일이 있는 int64 고정소수점. 반올림 규칙 및 상품별 scale은 데이터/세이브 헤더에 명시한다.
- Weight/Probability: 내부 사건 산출은 고정소수점 확률 및 정수 잔여량으로 결정론적 배분; 단순 표시 비율은 원본 진실값이 아니다.
- 입력에 추정치가 있으면 precision, provenance, method를 붙인다. **unknown과 0은 다르다**.

## 엔터티 정의

| 타입 | 필수 속성 | 주의 |
|---|---|---|
| DemographicCohort | id, key, schema_version | 읽기용 분류 사전, count 없음 |
| DemographicCohortKey | residence_region_id, birth_month_index, sex_recorded, culture_id, religion_id, citizenship_id, legal_status_id, health_profile_id, service_status | 하나의 속성 변경은 새/기존 분류 키로 셀 이전 |
| SocioeconomicPop | id, residence_region_id, primary_profession_id, livelihood_strategy_id, labor_market_class_id, qualification_profile_id | 직업 미배정/실직·교육/숙련 차이를 표현; 고용·자산 원본은 아님 |
| HouseholdPool | id, residence_region_id, composition_profile_id, pooling_policy_id, household_count_estimate, estimate_provenance | 실제 개별 가족이 아님; 집단거주/미상 풀 허용 |
| PopulationSlice | id, demographic_id, socioeconomic_pop_id, household_pool_id, allocation_partition_id, count, revision | 유일한 인구수 원본; 소유/계약 배분 단위가 다르면 partition으로 구분; count>0인 활성 셀만 |
| HealthProfile | id, disease_states[], injury_states[], impairment_flags, functional_support, care_dependency | 질병·부상·장애 병존; 중증도와 시작시점은 사건 이력으로 추적 |
| EducationProfile | id, literacy_level, education_stage, skill_tags[] | 학력·숙련을 별개로 다룸; 연결 분포는 필요한 경우 셀 단위로 세분화 |
| PopulationEvent | event_id, month, event_type, causes[], participants[], effects[], revision_preconditions | 검증 전 계획과 커밋된 사건 분리 |
| LedgerReference | transaction_id, source_event_id, owner_entity_id, amount/quantity, asset_id | 실제 금전·실물 원장은 별도 유지 |

### 코호트 키 해석

- residence_region_id는 등록/통상 거주지. 군부대 배치·해외 근무의 물리적 위치와 별개다.
- birth_month_index는 **내부에서 실제 배분 계산에 쓰이는 출생월**이다. 정확한 월이 없는 역사적 자료는 먼저 추정/분산 배정된 합성 월을 만들고 각 추정의 근거 및 정밀도를 보존한다. 미상 시점을 임의의 0월로 저장하지 않는다.
- sex_recorded는 female/male/another_recorded/unknown. 의료적 출산 적격성, 사회적 성역할, 법적 성별·징병 대상은 동일한 필드가 아니다. 정책/의학적 분포는 별도 모델로 계산한다.
- culture_id·religion_id는 식별/사회적 특성이지 선천적 생산성 값이 아니다. 차별·법적 권리는 별도 제도로 평가한다.
- citizenship_id와 legal_status_id를 분리해 복수 국적/비시민·농노·노예 등 제도 확대 여지를 둔다. 복수국적은 별도 CitizenshipRelation로 확장할 수 있으나 기본 키에서 중복인구를 만들지 않는다.
- service_status는 civilian/active/reserve/pow/missing 등으로 나눌 수 있으며 포로와 실종을 합치지 않는다. 군사조직별 배치는 군사 원장에 기록한다.
- health_profile_id는 배타적 단일 Health enum 대신 복합 건강 프로필 참조를 사용한다. 건강 상태·의료 필요·법적 장애 판정을 동일한 것으로 간주하지 않는다.

### 경제 POP와 가구

- primary_profession_id는 주된 직업/직업 자격 분류이며 실업 후에도 기존 직업을 보유할 수 있다. qualification_profile_id는 EducationProfile의 자격·교육·숙련 분류를 참조하며, 자격이 다른 사람을 직종 동일성만으로 혼합하지 않도록 한다. 유아·학교 재학생 등은 not_assigned로 기록한다.
- livelihood_strategy_id는 wage/self_employed/subsistence/inactive 등의 생계 전략을 표현한다. 실제 근로시간·계약은 EmploymentLedger에서 계산한다.
- wealth_band를 POP 식별 키에서 제외한다. 분포를 무시한다는 뜻이 아니라, 금융원장과 명시적 자산 분포(구간, 표본 정보, 불확실성)를 별도로 둔다. 극단적인 분포 차이는 정책 기준에 따라 경제 POP/셀을 추가 세분한다.
- allocation_partition_id는 같은 분류에 속하더라도 서로 다른 고용·자산·부채·정확도 분포를 가진 사람들을 구별할 때 사용한다. 단순 임의 할당으로 분할을 폭증시키지 않으며, partition 병합은 관련 원장이 동등하게 이전될 때에만 허용한다.
- HouseholdPool.household_count_estimate는 통계적 **가구 수**이고 셀의 인구 합계와 동일하지 않다. 가구당 인원 분포/구성 프로필과 일치하는지 검사하며 자료 부족 시 estimate_provenance=unknown으로 관리한다.
- household_pool_id는 모든 셀에 필수; 부양 관계를 알 수 없더라도 명시적인 unassigned/institutional 풀에 넣는다. 소비 계산에서 공동 예산을 가정할지 여부는 pooling_policy_id가 정한다.
- HouseholdPool이 소유하는 계정과 경제 POP가 소유하는 예금은 별개다. 가구 공유 소득은 실제 자금 이체가 있을 때에만 이동한다.

## 인구 셀: 유일한 계수 원본

PopulationSlice는 ID가 유일한 인구 배분 단위다. 세 분류 ID가 같아도 소유·계약·분포 배정에 중요한 차이가 있으면 서로 다른 allocation_partition_id를 가지며 별개 활성 셀로 유지할 수 있다. 반대로 네 가지 (demographic_id, socioeconomic_pop_id, household_pool_id, allocation_partition_id)가 모두 같으면 같은 월말에 중복 활성 셀을 허용하지 않는다. 레지스트리에서 분류(코호트/경제 POP/가구 풀) ID는 재사용하지만 **cell id는 영구 재사용 금지**. 활성 테이블은 nonzero count만 보유한다. 국가/지역·문화·연령별 집계는 모든 활성 셀을 필터링하여 계산한다.

- SUM(PopulationSlice.count) = 세계 주민 인구수(모형 범위의 실제 집계).
- SUM(count WHERE cohort_id = X) = X 코호트의 인구수.
- SUM(count WHERE socioeconomic_pop_id = Y) = Y 경제 POP의 인구수.
- SUM(count WHERE household_pool_id = Z) = Z 가구 풀의 인구수.
- 해당 세 뷰는 별개 합계일 뿐 합산하여 전체 인구의 3배를 만들면 안 된다.

경제 POP의 구성원이 여러 다른 연령·성별·가구에 속해도 고용자는 **노동 적격 인원**만 고용해야 한다. 초기 구현에서는 한 사람이 동일 월에 단 하나의 주된 고용계약을 가진다고 가정한다. 미래의 시간제·겸업 확대 시 인원과 FTE/근로시간을 별도 부호화하고 중복 계산을 방지한다.

## 필수 참조 조건

1. 모든 active slice의 세 외래 ID가 존재하고 동일 거주지역을 가리킨다.
2. 모든 count는 양의 정수이며, active 셀의 (demographic_id, socioeconomic_pop_id, household_pool_id, allocation_partition_id) 조합은 유일하다. 같은 세 분류가 같더라도 서로 다른 partition은 독립된 사람의 집합이며 합산 인원은 중복되지 않는다.
3. health_profile_id 및 service_status의 참조 대상은 유효하며, 이력 삭제는 active 참조 종료 뒤에만 허용한다.
4. 사람을 이동시킬 때 소유·계약·가구예산·교육 분포 등 **다른 원본 원장과의 관계가 끊어지지 않아야 한다**. 금액을 인구수에 따라 무조건 비례 배분하는 것은 금지한다.
5. 군 복무 중 민간 취업 금지와 같은 자격 규칙은 시대·법률·직무에 따라 평가하되, 집계의 참조 제약과는 별개로 검증한다.
6. education/skill/health 상태를 평균으로만 제공해 개별 적격자 수를 식별할 수 없다면 고용·징병 계산 전에 해당 셀을 필요한 만큼 세분해야 한다.

## 예시: 한 지역 6명

| Slice | 코호트 | 경제 POP | 가구 풀 | 인원 |
|---|---|---|---|---:|
| S1 | 성인 여성 건강 | 농민 | H1 | 2 |
| S2 | 성인 남성 건강 | 농민 | H1 | 1 |
| S3 | 소아 여성 건강 | 미배정 | H1 | 2 |
| S4 | 소아 남성 질병 | 미배정 | H1 | 1 |

지역 총인구는 **6명**이며 농민 경제 POP의 인구는 3명, H1 가구 풀의 인구는 6명이다. 위의 숫자 세 종류를 서로 더할 수 없다.

## 호환성

현재 코드의 PopKey(wealth_band 포함), CohortKey(Health 단일 열거형), Pop.cohorts(count 보유)는 **legacy 스캐폴드**다. 신규 코드의 목표는 세 분류와 PopulationSlice 단일 인구 원본이다. 기존 자료는 [MIGRATION](MIGRATION.md) 규약으로 옮긴다. 기존 public header를 사전 고지 없이 삭제하지 않는다.

## 설계 게이트 D1

D1 통과: 각 필드가 타입·단위·허용 상태·원본 소유자를 가진다. 누락·추정치의 출처가 0과 구별된다. 실제 셀 6명을 연령/경제/가구로 각각 합산해도 6명이 보존된다. 경제 POP에 재산원본이나 어린이 노동계약이 자동 생성되지 않는다.
