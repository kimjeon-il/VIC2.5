# VIC2.5 Victoria 3 1.15 — 법률 효과 실제 소비자 연결 현황

기준: 2026-11-05 공개 1.15 원본 미러의 법률·증보 수치/boolean modifier. 조사일 2026-11-10. 작업 브랜치 `work/law-effects-115-20261110`.

## 판정 규칙

- **일부 연결 (미검증)**: 법률/증보 값을 실제 C/D/E/G/H 또는 B/POP 계산에서 읽거나 조건 제한에 사용하도록 코드 경로가 존재하나, 원작 효과와 수치 동등성 또는 게임 내 동작을 전부 보증하지 않는다.
- **정의만 존재**: 1.15 자료에 식별자·값이 저장됐으나 실제 게임플레이 소비자가 없는 키이다. 특히 1.15 이벤트·시나리오·특수 스크립트의 `on_enact`, `on_activate`, `on_deactivate`, `possible`은 이 수치 목록과 **별개의 미완성 실행 요구**다.
- **전량 구현 아님**: 모든 법률이 1개 효과가 실행된다고 해당 법률의 전체 효과가 연결되었다고 표시하지 않는다. 효과가 0개인 법률도 입법 제한·해금·이벤트가 있을 수 있다.

## 전수 숫자

- 법률 **148개**, 법률군 **26개**, 증보 **71개**
- 정적 modifier 행 **1117개**, 고유 effect key **350종**
- 실행 경로 매핑된 key **63종** (부분 구현/미검증), 소비자가 아직 없는 key **287종**
- 매핑된 source 행 **310개**, 미연결 source 행 **807개**

## 법률군별 정적 modifier 매핑

| 법률군 | 법률 선택지 | 원문 직접/제도 수치 효과 행 | 코드 소비자 존재 행(부분) | 미연결 행 |
|---|---:|---:|---:|---:|
| `army_model` | 6 | 42 | 17 | 25 |
| `bureaucracy` | 6 | 13 | 7 | 6 |
| `caste_hegemony` | 4 | 51 | 2 | 49 |
| `childrens_rights` | 3 | 11 | 7 | 4 |
| `church_and_state` | 7 | 87 | 13 | 74 |
| `citizenship` | 7 | 92 | 6 | 86 |
| `colonization` | 5 | 14 | 0 | 14 |
| `distribution_of_power` | 14 | 108 | 58 | 50 |
| `economic_system` | 8 | 95 | 6 | 89 |
| `edo_social_system` | 3 | 53 | 0 | 53 |
| `education_system` | 5 | 14 | 7 | 7 |
| `free_speech` | 4 | 31 | 3 | 28 |
| `governance_principles` | 9 | 35 | 23 | 12 |
| `health_system` | 4 | 13 | 4 | 9 |
| `internal_security` | 8 | 48 | 11 | 37 |
| `labor_rights` | 3 | 1 | 1 | 0 |
| `labour_associations` | 6 | 37 | 9 | 28 |
| `land_reform` | 9 | 32 | 11 | 21 |
| `migration` | 3 | 5 | 2 | 3 |
| `navy_model` | 4 | 20 | 2 | 18 |
| `policing` | 4 | 11 | 4 | 7 |
| `rights_of_women` | 5 | 15 | 6 | 9 |
| `slavery` | 5 | 5 | 3 | 2 |
| `taxation` | 5 | 2 | 0 | 2 |
| `trade_policy` | 6 | 44 | 12 | 32 |
| `welfare` | 5 | 15 | 5 | 10 |

## 실행 경로가 존재하는 modifier 키 — 의미와 원작 동등성은 별도 검증 필요

| 원본 modifier key | 해당 원본 정의 행 | 연결 소유 계산 | 비고 |
|---|---:|---|---|
| `building_minimum_wage_mult` | 3 | C/D 경제·재정 | VIC2.5 단순화 계산·조건부 |
| `building_training_rate_mult` | 5 | C/D 경제·재정 | 정적 소비자 연결 |
| `country_academics_pol_str_mult` | 3 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_aristocrats_pol_str_mult` | 10 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_authority_add` | 39 | C/P 정치 | 정적 소비자 연결 |
| `country_authority_cost_add` | 5 | C/P 정치 | 정적 소비자 연결 |
| `country_authority_mult` | 10 | C/P 정치 | 정적 소비자 연결 |
| `country_bureaucrats_pol_str_mult` | 8 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_can_only_conscript_peasants_bool` | 2 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_capitalists_pol_str_mult` | 3 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_clergymen_pol_str_mult` | 3 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_clerks_pol_str_mult` | 1 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_disallow_trade_bool` | 1 | G/H 무역·외교 | VIC2.5 단순화 계산·조건부 |
| `country_engineers_pol_str_mult` | 1 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_farmers_pol_str_mult` | 2 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_law_enactment_speed_mult` | 3 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_law_enactment_success_add` | 3 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_legitimacy_base_add` | 6 | C/P 정치 | VIC2.5 단순화 계산·조건부 |
| `country_legitimacy_govt_total_clout_add` | 15 | C/P 정치 | VIC2.5 단순화 계산·조건부 |
| `country_legitimacy_govt_total_votes_add` | 11 | C/P 정치 | VIC2.5 단순화 계산·조건부 |
| `country_legitimacy_headofstate_add` | 13 | C/P 정치 | VIC2.5 단순화 계산·조건부 |
| `country_loan_interest_rate_add` | 5 | C/D 경제·재정 | 정적 소비자 연결 |
| `country_loan_interest_rate_mult` | 5 | C/D 경제·재정 | 정적 소비자 연결 |
| `country_migration_restrictiveness_add` | 3 | B/POP·C/P1 | VIC2.5 단순화 계산·조건부 |
| `country_officers_pol_str_mult` | 21 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_peasants_pol_str_mult` | 3 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_shopkeepers_pol_str_mult` | 4 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_soldiers_pol_str_mult` | 3 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `interest_group_ig_armed_forces_approval_add` | 1 | C/P 정치 | 정적 소비자 연결 |
| `interest_group_ig_armed_forces_pol_str_mult` | 4 | C/P 정치 | 정적 소비자 연결 |
| `interest_group_ig_devout_approval_add` | 1 | C/P 정치 | 정적 소비자 연결 |
| `interest_group_ig_devout_pol_str_mult` | 10 | C/P 정치 | 정적 소비자 연결 |
| `interest_group_ig_industrialists_pol_str_mult` | 2 | C/P 정치 | 정적 소비자 연결 |
| `interest_group_ig_intelligentsia_pol_str_mult` | 3 | C/P 정치 | 정적 소비자 연결 |
| `interest_group_ig_landowners_approval_add` | 3 | C/P 정치 | 정적 소비자 연결 |
| `interest_group_ig_landowners_pol_str_mult` | 14 | C/P 정치 | 정적 소비자 연결 |
| `interest_group_ig_petty_bourgeoisie_approval_add` | 1 | C/P 정치 | 정적 소비자 연결 |
| `interest_group_ig_petty_bourgeoisie_pol_str_mult` | 5 | C/P 정치 | 정적 소비자 연결 |
| `interest_group_ig_rural_folk_approval_add` | 1 | C/P 정치 | 정적 소비자 연결 |
| `interest_group_ig_rural_folk_pol_str_mult` | 1 | C/P 정치 | 정적 소비자 연결 |
| `interest_group_ig_trade_unions_pol_str_mult` | 7 | C/P 정치 | VIC2.5 단순화 계산·조건부 |
| `interest_group_in_government_approval_add` | 4 | C/P 정치 | 정적 소비자 연결 |
| `interest_group_pol_str_mult` | 6 | C/P 정치 | 정적 소비자 연결 |
| `state_birth_rate_mult` | 4 | B/POP·C/P1 | 정적 소비자 연결 |
| `state_conscription_rate_add` | 8 | C/E/H 군사 | VIC2.5 단순화 계산·조건부 |
| `state_education_access_add` | 3 | B/POP·C/P1 | 정적 소비자 연결 |
| `state_farmers_mortality_mult` | 2 | B/POP·C/P1 | 정적 소비자 연결 |
| `state_laborers_mortality_mult` | 2 | B/POP·C/P1 | 정적 소비자 연결 |
| `state_literacy_growth_add` | 1 | B/POP·C/P1 | 정적 소비자 연결 |
| `state_lower_strata_birth_rate_mult` | 1 | B/POP·C/P1 | 정적 소비자 연결 |
| `state_loyalists_from_political_movements_mult` | 1 | C/P 정치 | 정적 소비자 연결 |
| `state_machinists_mortality_mult` | 1 | B/POP·C/P1 | 정적 소비자 연결 |
| `state_migration_pull_mult` | 1 | B/POP·C/P1 | VIC2.5 단순화 계산·조건부 |
| `state_mortality_mult` | 3 | B/POP·C/P1 | 정적 소비자 연결 |
| `state_peasants_education_access_add` | 2 | B/POP·C/P1 | 정적 소비자 연결 |
| `state_peasants_internal_migration_disallowed_bool` | 2 | B/POP·C/P1 | VIC2.5 단순화 계산·조건부 |
| `state_peasants_mortality_mult` | 2 | B/POP·C/P1 | 정적 소비자 연결 |
| `state_pop_qualifications_mult` | 1 | B/POP·C/P1 | VIC2.5 단순화 계산·조건부 |
| `state_radicals_from_political_movements_mult` | 9 | C/P 정치 | 정적 소비자 연결 |
| `state_tariff_import_add` | 5 | G/H 무역·외교 | VIC2.5 단순화 계산·조건부 |
| `state_trade_capacity_mult` | 2 | G/H 무역·외교 | VIC2.5 단순화 계산·조건부 |

| `state_tax_capacity_mult` | 5 | C/D 경제·재정 | 행정인력에 기반한 원천징수 실효율(단순화) |
| `state_tax_collection_mult` | 1 | C/D 경제·재정 | 행정인력에 기반한 원천징수 실효율(단순화) |

## 아직 연결되지 않은 modifier 키 — 모두 실제 계산 연결이 필요함

| 원본 modifier key | 해당 정의 행 | 연결해야 할 영역 |
|---|---:|---|
| `building_art_academy_throughput_add` | 1 | C/D 경제·재정 |
| `building_company_government_dividends_add` | 1 | C/D 경제·재정 |
| `building_company_worker_dividends_add` | 1 | C/D 경제·재정 |
| `building_group_bg_agriculture_allowed_collectivization_add` | 4 | C/D 경제·재정 |
| `building_group_bg_agriculture_construction_efficiency_add` | 1 | C/D 경제·재정 |
| `building_group_bg_agriculture_standard_of_living_add` | 1 | C/D 경제·재정 |
| `building_group_bg_agriculture_tax_mult` | 1 | C/D 경제·재정 |
| `building_group_bg_agriculture_throughput_add` | 3 | C/D 경제·재정 |
| `building_group_bg_arts_allowed_collectivization_add` | 5 | C/D 경제·재정 |
| `building_group_bg_extraction_allowed_collectivization_add` | 4 | C/D 경제·재정 |
| `building_group_bg_extraction_throughput_add` | 1 | C/D 경제·재정 |
| `building_group_bg_heavy_industry_allowed_collectivization_add` | 1 | C/D 경제·재정 |
| `building_group_bg_light_industry_allowed_collectivization_add` | 1 | C/D 경제·재정 |
| `building_group_bg_logging_unincorporated_throughput_add` | 1 | C/D 경제·재정 |
| `building_group_bg_manor_houses_aristocrats_standard_of_living_add` | 1 | C/D 경제·재정 |
| `building_group_bg_manufacturing_allowed_collectivization_add` | 4 | C/D 경제·재정 |
| `building_group_bg_manufacturing_unincorporated_throughput_add` | 1 | C/D 경제·재정 |
| `building_group_bg_military_industry_allowed_collectivization_add` | 1 | C/E/H 군사 |
| `building_group_bg_mining_allowed_collectivization_add` | 1 | C/D 경제·재정 |
| `building_group_bg_mining_unincorporated_throughput_add` | 1 | C/D 경제·재정 |
| `building_group_bg_plantations_allowed_collectivization_add` | 1 | C/D 경제·재정 |
| `building_group_bg_plantations_tax_mult` | 1 | C/D 경제·재정 |
| `building_group_bg_plantations_throughput_add` | 2 | C/D 경제·재정 |
| `building_group_bg_plantations_unincorporated_throughput_add` | 1 | C/D 경제·재정 |
| `building_group_bg_power_allowed_collectivization_add` | 4 | C/D 경제·재정 |
| `building_group_bg_private_infrastructure_allowed_collectivization_add` | 5 | C/D 경제·재정 |
| `building_group_bg_ranching_allowed_collectivization_add` | 4 | C/D 경제·재정 |
| `building_group_bg_ranching_standard_of_living_add` | 1 | C/D 경제·재정 |
| `building_group_bg_ranching_tax_mult` | 1 | C/D 경제·재정 |
| `building_group_bg_ranching_throughput_add` | 1 | C/D 경제·재정 |
| `building_group_bg_rubber_unincorporated_throughput_add` | 1 | C/D 경제·재정 |
| `building_group_bg_subsistence_agriculture_allowed_collectivization_add` | 1 | C/D 경제·재정 |
| `building_group_bg_subsistence_agriculture_peasants_standard_of_living_add` | 1 | C/D 경제·재정 |
| `building_group_bg_subsistence_ranching_allowed_collectivization_add` | 1 | C/D 경제·재정 |
| `building_group_bg_subsistence_ranching_peasants_standard_of_living_add` | 1 | C/D 경제·재정 |
| `building_minimum_incorporated_subsistence_employment_add` | 2 | C/D 경제·재정 |
| `building_nationalization_cost_mult` | 4 | C/D 경제·재정 |
| `building_nationalization_investment_return_add` | 5 | C/D 경제·재정 |
| `building_nationalization_radicals_mult` | 1 | C/D 경제·재정 |
| `building_naval_administration_hiring_rate_mult` | 2 | C/D 경제·재정 |
| `building_self_investment_chance_add` | 2 | C/D 경제·재정 |
| `building_unincorporated_subsistence_output_mult` | 1 | C/D 경제·재정 |
| `building_unincorporated_throughput_add` | 1 | C/D 경제·재정 |
| `building_working_conditions_mult` | 1 | C/D 경제·재정 |
| `country_academics_voting_power_add` | 1 | C/P 정치 |
| `country_acceptance_homeland_add` | 3 | B/POP·C/P1 |
| `country_acceptance_no_shared_heritage_trait_add` | 9 | B/POP·C/P1 |
| `country_acceptance_no_shared_language_trait_add` | 7 | B/POP·C/P1 |
| `country_acceptance_no_shared_religious_trait_add` | 8 | B/POP·C/P1 |
| `country_acceptance_no_shared_tradition_trait_add` | 7 | B/POP·C/P1 |
| `country_acceptance_not_homeland_add` | 2 | B/POP·C/P1 |
| `country_acceptance_primary_culture_add` | 7 | B/POP·C/P1 |
| `country_acceptance_shared_heritage_trait_add` | 8 | B/POP·C/P1 |
| `country_acceptance_shared_heritage_trait_group_add` | 8 | B/POP·C/P1 |
| `country_acceptance_shared_language_trait_add` | 7 | B/POP·C/P1 |
| `country_acceptance_shared_language_trait_group_add` | 7 | B/POP·C/P1 |
| `country_acceptance_shared_religious_trait_add` | 8 | B/POP·C/P1 |
| `country_acceptance_shared_religious_trait_group_add` | 8 | B/POP·C/P1 |
| `country_acceptance_shared_tradition_trait_add` | 7 | B/POP·C/P1 |
| `country_acceptance_state_religion_add` | 7 | B/POP·C/P1 |
| `country_allow_conversion_cultural_erasure_bool` | 7 | B/POP·C/P1 |
| `country_allow_conversion_full_acceptance_bool` | 7 | B/POP·C/P1 |
| `country_allow_conversion_open_prejudice_bool` | 7 | B/POP·C/P1 |
| `country_allow_conversion_second_rate_citizen_bool` | 7 | B/POP·C/P1 |
| `country_allow_conversion_violent_hostility_bool` | 7 | B/POP·C/P1 |
| `country_amenability_add` | 4 | 별도 실행 대상 필요 |
| `country_aristocrats_voting_power_add` | 3 | C/P 정치 |
| `country_basque_cultural_acceptance_add` | 1 | B/POP·C/P1 |
| `country_block_government_reform_bool` | 3 | 별도 실행 대상 필요 |
| `country_bolster_attraction_factor` | 3 | 별도 실행 대상 필요 |
| `country_brahmins_acceptance_min_add` | 3 | B/POP·C/P1 |
| `country_brahmins_qualification_growth_other_class_mult` | 1 | B/POP·C/P1 |
| `country_brahmins_qualification_growth_same_class_mult` | 2 | B/POP·C/P1 |
| `country_bureaucracy_add` | 1 | C/D 경제·재정 |
| `country_bureaucracy_investment_cost_factor_mult` | 2 | C/D 경제·재정 |
| `country_bureaucracy_mult` | 2 | C/D 경제·재정 |
| `country_bureaucrats_voting_power_add` | 3 | C/P 정치 |
| `country_can_found_cossack_host_bool` | 1 | 별도 실행 대상 필요 |
| `country_capitalists_voting_power_add` | 1 | C/P 정치 |
| `country_clergymen_voting_power_add` | 3 | C/P 정치 |
| `country_clerks_voting_power_add` | 1 | C/P 정치 |
| `country_company_construction_efficiency_bonus_add` | 1 | C/D 경제·재정 |
| `country_consumption_tax_cost_mult` | 1 | C/D 경제·재정 |
| `country_coup_resistance_add` | 7 | C/P 정치 |
| `country_dalit_acceptance_max_add` | 2 | B/POP·C/P1 |
| `country_dalit_cultural_acceptance_mult` | 2 | B/POP·C/P1 |
| `country_dalit_education_access_mult` | 1 | B/POP·C/P1 |
| `country_dalit_qualification_growth_mult` | 1 | B/POP·C/P1 |
| `country_dalit_qualification_growth_other_class_mult` | 1 | B/POP·C/P1 |
| `country_dalit_qualification_growth_same_class_mult` | 2 | B/POP·C/P1 |
| `country_disable_investment_pool_bool` | 1 | C/D 경제·재정 |
| `country_disable_nationalization_bool` | 1 | 별도 실행 대상 필요 |
| `country_disable_nationalization_without_compensation_bool` | 5 | 별도 실행 대상 필요 |
| `country_disable_non_company_privatization_bool` | 1 | 별도 실행 대상 필요 |
| `country_disallow_aggressive_plays_bool` | 1 | 별도 실행 대상 필요 |
| `country_disallow_agitator_invites_bool` | 1 | 별도 실행 대상 필요 |
| `country_disallow_trade_outside_canton_bool` | 1 | G/H 무역·외교 |
| `country_disallow_trade_outside_kyushu_bool` | 1 | G/H 무역·외교 |
| `country_economic_dependence_on_overlord_add` | 2 | 별도 실행 대상 필요 |
| `country_education_fervor_add` | 2 | B/POP·C/P1 |
| `country_electoral_confidence_impact_mult` | 7 | 별도 실행 대상 필요 |
| `country_enactment_success_chance_law_anarchy_add` | 1 | 별도 실행 대상 필요 |
| `country_enactment_success_chance_law_autocracy_add` | 1 | 별도 실행 대상 필요 |
| `country_enactment_success_chance_law_oligarchy_add` | 1 | 별도 실행 대상 필요 |
| `country_enactment_success_chance_law_technocracy_add` | 1 | 별도 실행 대상 필요 |
| `country_fervor_target_basque_add` | 1 | B/POP·C/P1 |
| `country_financial_districts_buy_farms_likelihood` | 3 | 별도 실행 대상 필요 |
| `country_forbid_electoral_fraud_bool` | 1 | 별도 실행 대상 필요 |
| `country_forbid_monopoly_bool` | 1 | 별도 실행 대상 필요 |
| `country_force_privatization_bool` | 1 | 별도 실행 대상 필요 |
| `country_foreign_collectivization_bool` | 1 | 별도 실행 대상 필요 |
| `country_free_charters_add` | 8 | 별도 실행 대상 필요 |
| `country_government_dividends_efficiency_add` | 4 | C/D 경제·재정 |
| `country_government_dividends_reinvestment_add` | 6 | C/D 경제·재정 |
| `country_government_wages_mult` | 2 | C/D 경제·재정 |
| `country_influence_mult` | 4 | G/H 무역·외교 |
| `country_institution_cost_institution_health_system_mult` | 2 | B/POP·C/P1 |
| `country_institution_cost_institution_home_affairs_mult` | 2 | 별도 실행 대상 필요 |
| `country_institution_cost_institution_schools_mult` | 8 | 별도 실행 대상 필요 |
| `country_institution_cost_institution_social_security_mult` | 2 | 별도 실행 대상 필요 |
| `country_institution_home_affairs_max_investment_add` | 1 | C/D 경제·재정 |
| `country_institution_police_max_investment_add` | 2 | C/D 경제·재정 |
| `country_institution_schools_max_investment_add` | 2 | C/D 경제·재정 |
| `country_institution_social_security_max_investment_add` | 1 | C/D 경제·재정 |
| `country_kshatriyas_acceptance_max_add` | 2 | B/POP·C/P1 |
| `country_kshatriyas_acceptance_min_add` | 2 | B/POP·C/P1 |
| `country_kshatriyas_cultural_acceptance_mult` | 2 | B/POP·C/P1 |
| `country_kshatriyas_education_access_mult` | 1 | B/POP·C/P1 |
| `country_kshatriyas_qualification_growth_mult` | 1 | B/POP·C/P1 |
| `country_kshatriyas_qualification_growth_other_class_mult` | 1 | B/POP·C/P1 |
| `country_kshatriyas_qualification_growth_same_class_mult` | 2 | B/POP·C/P1 |
| `country_laborers_voting_power_add` | 1 | C/P 정치 |
| `country_legitimacy_govt_size_add` | 17 | C/P 정치 |
| `country_legitimacy_ideological_incoherence_mult` | 19 | C/P 정치 |
| `country_leverage_resistance_mult` | 15 | G/H 무역·외교 |
| `country_limit_officers_qualifications_to_upper_strata` | 1 | B/POP·C/P1 |
| `country_machinists_voting_power_add` | 1 | C/P 정치 |
| `country_max_companies_add` | 1 | 별도 실행 대상 필요 |
| `country_military_goods_cost_mult` | 2 | C/E/H 군사 |
| `country_military_wages_mult` | 1 | C/E/H 군사 |
| `country_nationalization_cost_non_members_mult` | 1 | 별도 실행 대상 필요 |
| `country_navy_goods_cost_mult` | 1 | C/E/H 군사 |
| `country_non_state_religion_wages_mult` | 3 | C/D 경제·재정 |
| `country_officers_voting_power_add` | 4 | C/P 정치 |
| `country_other_country_socialist_economy_hostility_impact_add` | 2 | 별도 실행 대상 필요 |
| `country_other_country_socialist_government_hostility_impact_add` | 1 | 별도 실행 대상 필요 |
| `country_peasants_voting_power_add` | 1 | C/P 정치 |
| `country_polish_cultural_acceptance_add` | 1 | B/POP·C/P1 |
| `country_prestige_from_navy_power_projection_mult` | 1 | C/E/H 군사 |
| `country_primary_culture_fervor_from_laws_add` | 5 | B/POP·C/P1 |
| `country_private_construction_allocation_mult` | 8 | C/D 경제·재정 |
| `country_production_tech_research_speed_mult` | 1 | C/D 경제·재정 |
| `country_production_tech_spread_mult` | 1 | C/D 경제·재정 |
| `country_radicalism_increases_full_acceptance_mult` | 1 | B/POP·C/P1 |
| `country_ship_construction_efficiency_add` | 1 | C/D 경제·재정 |
| `country_ship_crew_starting_veterancy_experience_add` | 2 | C/E/H 군사 |
| `country_ship_group_capital_ships_construction_efficiency_add` | 1 | C/D 경제·재정 |
| `country_ship_group_cruisers_construction_efficiency_add` | 1 | C/D 경제·재정 |
| `country_ship_group_supply_ships_construction_efficiency_add` | 1 | C/D 경제·재정 |
| `country_ship_group_torpedo_craft_construction_efficiency_add` | 1 | C/D 경제·재정 |
| `country_ship_type_armored_cruiser_construction_efficiency_add` | 1 | C/D 경제·재정 |
| `country_ship_type_early_troop_ship_construction_efficiency_add` | 1 | C/D 경제·재정 |
| `country_ship_type_light_cruiser_construction_efficiency_add` | 1 | C/D 경제·재정 |
| `country_ship_type_protected_cruiser_construction_efficiency_add` | 1 | C/D 경제·재정 |
| `country_ship_type_troop_ship_construction_efficiency_add` | 1 | C/D 경제·재정 |
| `country_shopkeepers_voting_power_add` | 2 | C/P 정치 |
| `country_shudras_acceptance_max_add` | 2 | B/POP·C/P1 |
| `country_shudras_cultural_acceptance_mult` | 2 | B/POP·C/P1 |
| `country_shudras_education_access_mult` | 1 | B/POP·C/P1 |
| `country_shudras_qualification_growth_mult` | 1 | B/POP·C/P1 |
| `country_shudras_qualification_growth_other_class_mult` | 1 | B/POP·C/P1 |
| `country_shudras_qualification_growth_same_class_mult` | 2 | B/POP·C/P1 |
| `country_soldiers_voting_power_add` | 1 | C/P 정치 |
| `country_state_religion_wages_mult` | 3 | C/D 경제·재정 |
| `country_strata_commoners_peasants_acceptance_max_add` | 3 | B/POP·C/P1 |
| `country_strata_commoners_peasants_acceptance_min_add` | 3 | B/POP·C/P1 |
| `country_strata_commoners_peasants_qualification_growth_other_class_mult` | 3 | B/POP·C/P1 |
| `country_strata_commoners_peasants_qualification_growth_same_class_mult` | 3 | B/POP·C/P1 |
| `country_strata_commoners_townspeople_acceptance_max_add` | 3 | B/POP·C/P1 |
| `country_strata_commoners_townspeople_acceptance_min_add` | 3 | B/POP·C/P1 |
| `country_strata_commoners_townspeople_qualification_growth_other_class_mult` | 3 | B/POP·C/P1 |
| `country_strata_commoners_townspeople_qualification_growth_same_class_mult` | 3 | B/POP·C/P1 |
| `country_strata_outcastes_acceptance_max_add` | 3 | B/POP·C/P1 |
| `country_strata_outcastes_qualification_growth_other_class_mult` | 3 | B/POP·C/P1 |
| `country_strata_outcastes_qualification_growth_same_class_mult` | 3 | B/POP·C/P1 |
| `country_strata_religious_acceptance_max_add` | 1 | B/POP·C/P1 |
| `country_strata_religious_acceptance_min_add` | 3 | B/POP·C/P1 |
| `country_strata_religious_qualification_growth_other_class_mult` | 3 | B/POP·C/P1 |
| `country_strata_religious_qualification_growth_same_class_mult` | 1 | B/POP·C/P1 |
| `country_strata_samurai_high_acceptance_min_add` | 3 | B/POP·C/P1 |
| `country_strata_samurai_high_qualification_growth_same_class_mult` | 2 | B/POP·C/P1 |
| `country_strata_samurai_low_acceptance_max_add` | 1 | B/POP·C/P1 |
| `country_strata_samurai_low_acceptance_min_add` | 3 | B/POP·C/P1 |
| `country_strata_samurai_low_qualification_growth_same_class_mult` | 3 | B/POP·C/P1 |
| `country_support_separatism_resistance_mult` | 14 | C/P 정치 |
| `country_support_separatism_strength_mult` | 9 | C/P 정치 |
| `country_suppression_attraction_factor` | 9 | C/P 정치 |
| `country_tech_research_speed_mult` | 1 | 별도 실행 대상 필요 |
| `country_tech_spread_mult` | 6 | 별도 실행 대상 필요 |
| `country_tension_decay_mult` | 1 | 별도 실행 대상 필요 |
| `country_two_spains_conservative_drift_add` | 1 | 별도 실행 대상 필요 |
| `country_vaishyas_acceptance_max_add` | 2 | B/POP·C/P1 |
| `country_vaishyas_acceptance_min_add` | 2 | B/POP·C/P1 |
| `country_vaishyas_cultural_acceptance_mult` | 2 | B/POP·C/P1 |
| `country_vaishyas_education_access_mult` | 1 | B/POP·C/P1 |
| `country_vaishyas_qualification_growth_mult` | 1 | B/POP·C/P1 |
| `country_vaishyas_qualification_growth_other_class_mult` | 1 | B/POP·C/P1 |
| `country_vaishyas_qualification_growth_same_class_mult` | 2 | B/POP·C/P1 |
| `country_voting_power_base_add` | 8 | C/P 정치 |
| `country_voting_power_from_literacy_add` | 2 | C/P 정치 |
| `country_voting_power_mult` | 2 | C/P 정치 |
| `country_voting_power_wealth_threshold_add` | 3 | C/P 정치 |
| `country_wage_cultural_erasure_mult` | 1 | C/D 경제·재정 |
| `country_wage_open_prejudice_mult` | 1 | C/D 경제·재정 |
| `country_wage_second_rate_citizen_mult` | 1 | C/D 경제·재정 |
| `country_wage_violent_hostility_mult` | 1 | C/D 경제·재정 |
| `country_war_support_battles_decrease_mult` | 4 | C/E/H 군사 |
| `country_war_support_battles_increase_mult` | 4 | C/E/H 군사 |
| `country_war_support_casualties_mult` | 4 | C/E/H 군사 |
| `interest_group_ig_devout_pop_attraction_mult` | 3 | C/P 정치 |
| `interest_group_in_government_attraction_mult` | 1 | C/P 정치 |
| `political_movement_radicalism_add` | 6 | C/P 정치 |
| `ship_power_projection_mult` | 2 | C/E/H 군사 |
| `state_aristocrats_investment_pool_contribution_add` | 1 | C/D 경제·재정 |
| `state_aristocrats_investment_pool_efficiency_mult` | 7 | C/D 경제·재정 |
| `state_assimilation_mult` | 5 | B/POP·C/P1 |
| `state_building_barrack_max_level_add` | 6 | C/E/H 군사 |
| `state_building_conscription_center_max_level_add` | 6 | C/E/H 군사 |
| `state_bureaucracy_population_base_cost_factor_mult` | 8 | C/D 경제·재정 |
| `state_bureaucrats_investment_pool_efficiency_mult` | 3 | C/D 경제·재정 |
| `state_capitalists_investment_pool_efficiency_mult` | 8 | C/D 경제·재정 |
| `state_clergymen_investment_pool_efficiency_mult` | 3 | C/D 경제·재정 |
| `state_colony_growth_creation_factor` | 1 | G/H 무역·외교 |
| `state_colony_growth_speed_mult` | 2 | G/H 무역·외교 |
| `state_conscription_rate_mult` | 3 | C/E/H 군사 |
| `state_conversion_mult` | 6 | B/POP·C/P1 |
| `state_dependent_political_participation_add` | 2 | C/P 정치 |
| `state_dependent_wage_mult` | 7 | B/POP·C/P1 |
| `state_education_access_wealth_add` | 1 | B/POP·C/P1 |
| `state_expected_sol_mult` | 1 | 별도 실행 대상 필요 |
| `state_export_advantage_mult` | 1 | G/H 무역·외교 |
| `state_farmers_investment_pool_efficiency_mult` | 5 | C/D 경제·재정 |
| `state_food_security_add` | 2 | B/POP·C/P1 |
| `state_free_state_pop_support_movement_anti_slavery_mult` | 1 | B/POP·C/P1 |
| `state_harvest_condition_drought_impact_mult` | 1 | 별도 실행 대상 필요 |
| `state_harvest_condition_extreme_winds_impact_mult` | 1 | 별도 실행 대상 필요 |
| `state_harvest_condition_flood_impact_mult` | 1 | 별도 실행 대상 필요 |
| `state_harvest_condition_wildfire_impact_mult` | 1 | 별도 실행 대상 필요 |
| `state_import_advantage_mult` | 1 | G/H 무역·외교 |
| `state_lower_strata_expected_sol_add` | 2 | B/POP·C/P1 |
| `state_lower_strata_standard_of_living_add` | 1 | B/POP·C/P1 |
| `state_market_access_price_impact` | 1 | C/D 경제·재정 |
| `state_middle_strata_expected_sol_add` | 1 | B/POP·C/P1 |
| `state_middle_strata_standard_of_living_add` | 1 | B/POP·C/P1 |
| `state_migration_pull_unincorporated_mult` | 4 | B/POP·C/P1 |
| `state_minimum_incorporated_subsistence_arable_land_add` | 5 | 별도 실행 대상 필요 |
| `state_mortality_turmoil_mult` | 2 | B/POP·C/P1 |
| `state_mortality_wealth_mult` | 2 | B/POP·C/P1 |
| `state_non_homeland_mortality_mult` | 1 | B/POP·C/P1 |
| `state_orthodox_lower_strata_conversion_rate_add` | 1 | B/POP·C/P1 |
| `state_peasants_mass_migration_disallowed_bool` | 1 | B/POP·C/P1 |
| `state_political_strength_from_wealth_mult` | 2 | C/P 정치 |
| `state_political_strength_from_welfare_mult` | 3 | C/P 정치 |
| `state_pollution_reduction_health_mult` | 3 | B/POP·C/P1 |
| `state_pop_pol_str_add` | 1 | B/POP·C/P1 |
| `state_pop_support_movement_anarchist_mult` | 5 | B/POP·C/P1 |
| `state_pop_support_movement_communist_mult` | 5 | B/POP·C/P1 |
| `state_pop_support_movement_labor_mult` | 5 | B/POP·C/P1 |
| `state_pop_support_movement_socialist_mult` | 5 | B/POP·C/P1 |
| `state_radicalism_increases_cultural_erasure_mult` | 1 | 별도 실행 대상 필요 |
| `state_radicalism_increases_open_prejudice_mult` | 1 | 별도 실행 대상 필요 |
| `state_radicalism_increases_violent_hostility_mult` | 1 | 별도 실행 대상 필요 |
| `state_radicals_and_loyalists_from_sol_change_mult` | 1 | 별도 실행 대상 필요 |
| `state_shopkeepers_investment_pool_efficiency_mult` | 6 | C/D 경제·재정 |
| `state_standard_of_living_add` | 1 | 별도 실행 대상 필요 |
| `state_subvention_export_add` | 5 | G/H 무역·외교 |
| `state_subvention_import_add` | 5 | G/H 무역·외교 |
| `state_tariff_export_add` | 4 | G/H 무역·외교 |
| `state_trade_advantage_mult` | 1 | G/H 무역·외교 |
| `state_trade_center_max_limit_add` | 2 | G/H 무역·외교 |
| `state_turmoil_effects_mult` | 1 | 별도 실행 대상 필요 |
| `state_unincorporated_starting_wages_mult` | 1 | C/D 경제·재정 |
| `state_upper_strata_standard_of_living_add` | 1 | B/POP·C/P1 |
| `state_welfare_payments_add` | 4 | B/POP·C/P1 |
| `state_working_adult_ratio_add` | 5 | B/POP·C/P1 |
| `unit_experience_gain_mult` | 2 | C/E/H 군사 |
| `unit_morale_loss_mult` | 5 | C/E/H 군사 |

## 수치 목록 밖의 미구현 내용

1. 법률별 활성화·폐지·제정·특수 국가 이벤트와 `on_enact`, `on_activate`, `on_deactivate`, DLC/저널 등 깊은 조건부 스크립트 실행.
2. 원작 법률 효과를 토대로 한 경제별 생산방식·국가 세금 종류(소비/토지/인두/비례/누진), 투자 지분·소유권, 특수 회사 및 주별 관세/수출세.
3. 종교·언어·문화의 원작식 수용도·동화·개종과 POP 요구 및 전국 정책; 해군 모델·선박 설계와 전투 경험, 전쟁지지도·함대력과 건설 효율.
4. 빅토리아 3 모드 API와 다른 VIC2.5의 계산단위에 대한 수치 검정. 위의 '일부 연결'은 C++ 빌드·CTest를 실행하지 않은 상태다.

## 다음 처리 기준

각 미연결 키는 (a) 이미 존재하는 원장 소비자에 연결하거나 (b) 게임적으로 통합된 명시 규칙을 만들거나 (c) 해당 게임 기능이 없어 `UNSUPPORTED`로 남긴다는 상태를 별도로 기록한다. **미지원 항목을 0으로 조용히 적용하고 '모든 효과 구현'으로 보고하지 않는다.** 기존 C/D/E/G/H 실제 현금·재고·인구·의석·병력 원장은 중복 생성하지 않는다.

원자료: https://github.com/jink47787/Historical_MP_Series_Vic3/tree/master/basegame-ver/common/laws 및 https://github.com/jink47787/Historical_MP_Series_Vic3/tree/master/basegame-ver/common/amendments
