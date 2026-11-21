# 1.15 법률 효과 연결 대조: 2026-11-21

기준: `ebf570c77be6422c84b5a26015716dd73b00d503`. [11월 10일 보고서](LAW_EFFECT_COVERAGE_115_2026-11-10.md)는 당시 기록으로 보존한다.

`p2_catalog_115().modifiers`의 네 원본 파일에는 법률 148개, 증보 71개, 효과 1,117행 및 고유 키 350개가 있다. 11월 10일 문서의 미연결 목록 228개와 당시 코드의 `wired_law_effects()` 48개를 대조했다. 코드에 등록된 키 중 39개가 정본에도 있으며, 그중 **22개 키(122행)**가 문서에서 미연결로 분류됐다. 아래는 이 22개만의 대조 결과이지 전체 350개의 적용률이 아니다.

| 키 | 정본 행 | 실제 경로 |
|---|---:|---|
| `country_acceptance_homeland_add` | 3 | 별도 정치 카탈로그 → 수용도 |
| `country_acceptance_no_shared_heritage_trait_add` | 9 | 별도 정치 카탈로그 → 수용도 |
| `country_acceptance_no_shared_language_trait_add` | 7 | 별도 정치 카탈로그 → 수용도 |
| `country_acceptance_no_shared_religious_trait_add` | 8 | 별도 정치 카탈로그 → 수용도 |
| `country_acceptance_no_shared_tradition_trait_add` | 7 | 별도 정치 카탈로그 → 수용도 |
| `country_acceptance_primary_culture_add` | 7 | 별도 정치 카탈로그 → 수용도 |
| `country_acceptance_shared_heritage_trait_add` | 8 | 별도 정치 카탈로그 → 수용도 |
| `country_acceptance_shared_heritage_trait_group_add` | 8 | 별도 정치 카탈로그 → 수용도 |
| `country_acceptance_shared_language_trait_add` | 7 | 별도 정치 카탈로그 → 수용도 |
| `country_acceptance_shared_language_trait_group_add` | 7 | 별도 정치 카탈로그 → 수용도 |
| `country_acceptance_shared_religious_trait_add` | 8 | 별도 정치 카탈로그 → 수용도 |
| `country_acceptance_shared_religious_trait_group_add` | 8 | 별도 정치 카탈로그 → 수용도 |
| `country_acceptance_shared_tradition_trait_add` | 7 | 별도 정치 카탈로그 → 수용도 |
| `country_acceptance_state_religion_add` | 7 | 별도 정치 카탈로그 → 수용도 |
| `country_bureaucracy_mult` | 2 | 정본 → 행정력 생산 |
| `country_influence_mult` | 4 | 정본 → 외교 영향력 |
| `country_institution_cost_institution_home_affairs_mult` | 2 | 정본 → 치안 기관 행정비 |
| `country_institution_cost_institution_schools_mult` | 8 | 정본 → 교육 기관 행정비 |
| `country_military_goods_cost_mult` | 2 | 정본 → 군수품 조달가 |
| `state_radicalism_increases_cultural_erasure_mult` | 1 | 법률 미연결, 이익집단 특성만 실행 |
| `state_radicalism_increases_open_prejudice_mult` | 1 | 법률 미연결, 이익집단 특성만 실행 |
| `state_radicalism_increases_violent_hostility_mult` | 1 | 법률 미연결, 이익집단 특성만 실행 |

- **별도 정치 카탈로그 14개(101행):** `phase_c_politics_identity_impl.hpp`의 `p2_acceptance`가 `politics::catalogue().modifier`를 읽는다. 이 입력은 812행 별도 정치 카탈로그이며, `p2_catalog_115()`의 1,117행을 직접 소비하지 않는다. `tests/test_politics_identity.cpp`의 `acceptance_and_five_tier_rights`는 민족국가의 주류/외부 문화 점수, 투표권, 임금·정치력 차이와 근거 없는 POP 조회를 검증한다. 위 14개의 키를 각각 바꿔 본 시험은 아니다.
- **정본 계산 5개(18행):** `phase_c_p1_bureaucracy_impl.hpp`의 `p1_bureaucracy` (행정력 및 학교·치안 기관 행정비), `phase_gh_political_power.hpp`의 `diplomatic_capacity` (영향력), `phase_gh_extensions.hpp`의 `military_procurement_price`와 `phase_gh.hpp`의 사용 경로(군수품 조달가)가 `p2_modifier_ppm`으로 법률 값을 읽는다. 대조 시점에는 이 다섯 키별 채택 전후 결과를 직접 비교하는 단독 시험을 확인하지 못했다. 기존 외교 시험의 군수품 인도량 검사는 조달가 효과 검증이 아니다.
- **법률 미연결 3개(3행):** 세 효과는 모두 `law_militarized_police`의 기관 수준 조건부 행이다. `phase_c_p3_impl.hpp`의 `p3_daily_react`는 현재 이 키들을 이익집단 특성에서만 읽는다. 따라서 법률 실행 목록에서 제거하고, 이익집단 실행 목록과 원본 행은 유지한다. `tests/test_politics_catalog.cpp`의 회귀는 이 구분을 검사하며 해당 법률의 게임 효과를 추가하거나 입증하지 않는다.

이익집단 쪽 원본은 24개 특성, 32개 특성·효과 조합이다. 조사 당시 32개 모두 실행 경로 목록에 있어 ‘미연결 특성이 반드시 있어야 한다’는 이전 시험의 전제는 맞지 않았다. 시험은 대신 원본의 조합 목록과 각 조합의 연결 표시·실행 경로가 빠짐없이 일치하는지 확인한다.

두 목록에 같은 키가 있어도 같은 수치·조건을 소비한다는 뜻은 아니다. 나머지 키를 감사할 때는 실행 경로와 원본 계열을 확인하고, 법률별 동작 비교를 별도로 표시한다.

## 이어서 연결한 효과

- 복지기관 행정비: 종교 구빈원·구빈원 규정 개정이 발효된 뒤에만 복지기관의 행정비가 10% 줄어든다. 복지 2단계 기준 비용 20→18, 같은 지역의 치안기관 비용 10은 그대로다. 테스트는 두 개정을 각각 실제 발효시켜 비교한다.
- 미편입 지역의 산업 생산량: **명시적으로 미편입인 지역**의 실제 생산 레시피에서만 식민지 착취법이 제조업 −10%, 플랜테이션·고무·벌목·광업 +20%로 반영된다. 편입 상태를 모르는 지역, 해당 산업이 아닌 일반 추출·임업 분류에는 적용하지 않는다. 원료 사용량·임금·배치 수는 그대로다. 산출량은 배치 정수 단위로 계산한다. 다른 제한이 산출량을 0으로 만들지 않은 상황에서 이 법의 감소분만으로 최소 양이 0으로 내림되는 경우에는 기존 생산 변환의 최소 단위 1을 유지한다. 별도 자원 추출 경로는 이 효과 검증에 포함하지 않았다.

이 항목은 위 22개 불일치 대조와 별도의 새 작업이다. 11월 10일 집계에서 숫자를 곧바로 차감하지 않는다.

### 일간 인구 사망률

- 채택된 의료 법률의 기관 단계별 사망률 효과는 실제 운영 가능한 의료기관이 확인되고, 지역이 명시적으로 편입돼 있을 때만 적용한다. 같은 국가에 의료기관이 여러 개 있어도 단계를 합산하지 않는다. 다른 편입 지역에는 해당 국가의 유효 단계가 적용되지만, 미편입 및 편입 미확인 지역에는 적용하지 않는다.
- 별도 신성모독 개정의 평면 효과는 의료기관 없이도 해당 주권 지역에 발효일부터 적용한다. 두 효과를 분리해 합산한다. 회귀 테스트에서 자선 의료 2단계 −6%, 공공 의료 2단계 −10%, 신성모독 개정 +2%, 공공 의료 2단계와 그 개정의 조합 −8%를 실제 일일 사망자 수로 대조했다.
