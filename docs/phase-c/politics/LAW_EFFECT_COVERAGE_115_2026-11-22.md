# VIC2.5 1.15 법률 효과 연결 점검: 2026-11-22 (한국시간)

기준: 코드 커밋 [`4c8e232`](https://github.com/kimjeon-il/VIC2.5/commit/4c8e232c34ee6f7b686865dbdd7d90d98e827f93). 정본 `p2_catalog_115().modifiers`는 **효과 1,117행, 고유 키 350개**다. 이 중 실제 동작을 특정 원본 행까지 대조하고 명시적으로 기록한 것은 **60행**이다. 60행은 전체 적용률이나 완료율이 아니다. [11월 10일 보고서](LAW_EFFECT_COVERAGE_115_2026-11-10.md)의 228개 미연결 키와 [11월 21일 초반 대조표](LAW_EFFECT_COVERAGE_115_2026-11-21.md)의 22개 키는 각 시점의 자료로 보존한다. 정본 행, 별도 정치 카탈로그, 이익집단 특성의 결과를 합산하지 않는다.

`include/vic25/phase_c_canonical_law_audit.hpp`는 원본 ID·키·값·자료형·기관/후원 조건을 함께 비교한다. **동작 검증**은 해당 행에만 적용한다. **실행 경로 후보**는 이름상 소비자가 있으나 조건별 검증이 남았다는 뜻이고, **근거 미기록**은 아직 이 대조표에 근거가 없다는 뜻이다. 같은 키를 쓰는 다른 행의 효과가 없다는 뜻은 아니다.

## 동작 검증

| 효과 | 행 | 실제 계산 및 검증 |
|---|---:|---|
| 교육 접근과 농민 교육, 아동노동법의 직업별 사망률 | 12 | `phase_time_auto_demography` → 실제 취학·사망. `test_politics_identity::enacted_education_changes_real_daily_schooling`, `::childrens_rights_changes_only_exact_profession_mortality`, `test_politics_domain::occupation_specific_law_changes_actual_peasant_schooling` |
| 식민지 이주 유입과 자급생산 상한 | 4 | `phase_time_auto_demography`의 확인된 미편입 도착지에 적용되는 고정 이주 효과 3행, `household_produce`의 명시적 자급생산 상한 1행. `test_politics_domain::colonial_migration_pull_requires_verified_destination`, `test_phase_c::colonial_subsistence_ceiling`. 국내 식민화의 기관 단계별 이주나 자동 생산은 입증하지 않았다. |
| 쇄국법 | 1 | `phasegh::Stage::open_crisis`에서 청구국의 신규 개시만 제한. `test_phase_gh_diplomacy::sakoku_blocks_only_new_claimant_initiation` |
| 국고 차입 이자와 복지 행정력 | 6 | `phase_time_accrue_and_settle_interest`의 실채무 이자 5행, `p1_bureaucracy`의 실제 작동하는 복지기관 단계 효과 1행. `test_p0_daily_longrun::sovereign_bank_interest_law_uses_real_principal`, `test_politics_identity::chiefs_aid_uses_only_effective_welfare_bureaucracy` |
| 내각 정당성 | 10 | `p1_legitimacy`의 기본 점수 6행, 이념 불일치 보정 4행. `test_politics_catalog::enacted_national_laws_change_only_calculated_legitimacy` |
| 협상 후원과 특정 이해집단 지지도 | 17 | `p3_group_view`. 협상으로 확인된 국내 후원 집단만 받는 10행과 법이 직접 지목하는 집단의 7행을 구분. 후원 집단의 여당 지지도 효과에는 실제 집권 연합 조건도 확인. `test_p2_advanced::negotiated_sponsor_uses_actual_same_country_group`, `::remaining_negotiated_sponsor_rows_use_actual_coalition`, `::direct_named_group_approval_tracks_law_not_the_coalition` |
| 특정 문화 수용도 및 입법 심사 성공 | 9 | `p2_acceptance`의 확인된 대상·국가 2행, `p2_review_due`의 실심사 결과 7행. `test_politics_identity::canonical_named_acceptance_changes_only_verified_pop_and_country`, `test_p2_advanced::canonical_enactment_success_changes_real_review_outcome` |
| 신선조 법의 정치운동 급진성 | 1 | `p3_movement_view`에서 고정 −5%p, `p5_risk`의 위기 압력으로 전달. POP 태도와 저장된 운동 원값은 그대로. `test_p3_p5_politics::shinsengumi_changes_movement_risk_without_altering_pop` |

## 보류: 선행 조건

| 보류 항목 | 필요한 실체와 담당 계산 |
|---|---|
| 내무기관 단계별 정치운동 효과 5행 | 기관 모델에서 일반 경찰과 내무기관을 별도 식별하고, 자국·편입지·유효 법률·실제 인력과 재원을 확인해야 한다. 내무기관이 없어도 일반 경찰 가동률은 남으므로 경찰 단계를 대신 쓸 수 없다. |
| 광저우·규슈 무역 관문 제한 | 무역 경로의 실제 관문과 법률상 허용된 광저우·규슈 관문의 식별 근거가 필요하다. 일반 항만이나 지역 태그만으로 우회·외부 무역 여부를 추정하지 않는다. |
| 운하 조건부 효과 | 무역 관문과 별개다. 지리·물류 모델이 실제 운하 시설과 운항 경로의 통과 구간·통행 주체를 연결해야 한다. |
| 부양가족 임금·수급자 이전 | 임금·가계 장부에 부양가족의 실제 임금 항목과 법정 수급자에게 지급된 이전 영수증/흐름이 필요하다. 가구 총수입이나 명목 예산을 대신 쓰지 않는다. |
| 정치운동으로 인한 POP 급진·충성 변화 | 정치운동의 실제 참가·사건이 개별 POP 태도 변화로 이어지는 기록이 필요하다. 운동의 급진성 점수를 읽는 것과 다른 경로다. |
| 직종 등 가중 투표 및 실제 과세표준 | 선거 계산은 근거가 있는 유권자·투표 단위에 해당 가중치를 연결해야 한다. 재정 장부는 각 줄의 실제 과세 대상과 징수 내역을 먼저 확인한다. 총인구·총수입으로 대체하지 않는다. |
| 세력권 영향력(레버리지) | 외교 모델에서 실제 세력권 소속·상대국별 레버리지 원장이 필요하다. 국가 외교 영향력 총량은 이 값과 다르다. |
| 군사시설 훈련 속도 | 군사 인력 모델에서 병영·해군시설의 신규 충원 속도를 기존 인원의 숙련도/훈련과 나눠야 한다. 현재 공유 모집·해산 한도나 숙련도에 바로 적용하지 않는다. |

## 재검증

```sh
python3 scripts/verify_phase_c_baseline.py
ctest --test-dir build --output-on-failure \
  -R 'vic25_(politics_catalog_tests|p2_advanced_tests|p3_p5_politics_tests)$'
```

정확한 후보의 계약 검사는 설계 테스트 48개 ID와 고정 원본 27개를 검증했다. 최신 후보의 법률 목록·고급 정치·정치운동은 네이티브로 직접 빌드해 통과했다. 자급생산 회귀는 설계 48개와 구분해 동일한 등록 Phase C 실행 파일에서 별도로 실행한다. 이전 커밋의 호스팅 전체 실행 결과는 이 후보의 전체 통과로 간주하지 않는다. 전체 CI 결과는 [GitHub Actions 통합 검사](https://github.com/kimjeon-il/VIC2.5/actions/workflows/merge-all-integration.yml)에서 같은 코드가 포함된 `main`의 해당 실행을 찾아 확인한다.
