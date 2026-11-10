# VIC2.5 Victoria 3 1.15 — 법률 효과 실제 소비자 연결 현황

기준: 2026-11-05 공개 1.15 원본 미러의 법률·증보 수치/boolean modifier. 조사일 2026-11-10. 작업 브랜치 `work/law-effects-115-20261110`. 해당 브랜치의 2차 연결은 빌드·CTest 미실행 상태.

## 판정 규칙

- **일부 연결 (미검증)**: 법률/증보 값을 실제 C/D/E/G/H 또는 B/POP 계산에서 읽거나 조건 제한에 사용하도록 코드 경로가 존재하나, 원작 효과와 수치 동등성 또는 게임 내 동작을 전부 보증하지 않는다.
- **정의만 존재**: 1.15 자료에 식별자·값이 저장됐으나 실제 게임플레이 소비자가 없는 키이다. 특히 1.15 이벤트·시나리오·특수 스크립트의 `on_enact`, `on_activate`, `on_deactivate`, `possible`은 이 수치 목록과 **별개의 미완성 실행 요구**다.
- **전량 구현 아님**: 모든 법률이 1개 효과가 실행된다고 해당 법률의 전체 효과가 연결되었다고 표시하지 않는다. 효과가 0개인 법률도 입법 제한·해금·이벤트가 있을 수 있다.

## 전수 숫자

- 법률 **148개**, 법률군 **26개**, 증보 **71개**
- 정적 modifier 행 **1117개**, 고유 effect key **350종**
- 실행 경로 매핑된 key **98종** (부분 구현/미검증), 소비자가 아직 없는 key **252종**
- 매핑된 source 행 **468개**, 미연결 source 행 **649개**

## 법률군별 정적 modifier 매핑

| 법률군 | 법률 선택지 | 원문 직접/제도 수치 효과 행 | 코드 소비자 존재 행(부분) | 미연결 행 |
|---|---:|---:|---:|---:|
| `army_model` | 6 | 42 | 17 | 25 |
| `bureaucracy` | 6 | 13 | 7 | 6 |
| `caste_hegemony` | 4 | 51 | 2 | 49 |
| `childrens_rights` | 3 | 11 | 7 | 4 |
| `church_and_state` | 7 | 87 | 13 | 74 |
| `citizenship` | 7 | 92 | 18 | 74 |
| `colonization` | 5 | 14 | 0 | 14 |
| `distribution_of_power` | 14 | 108 | 81 | 27 |
| `economic_system` | 8 | 95 | 48 | 47 |
| `edo_social_system` | 3 | 53 | 0 | 53 |
| `education_system` | 5 | 14 | 8 | 6 |
| `free_speech` | 4 | 31 | 16 | 15 |
| `governance_principles` | 9 | 35 | 34 | 1 |
| `health_system` | 4 | 13 | 6 | 7 |
| `internal_security` | 8 | 48 | 36 | 12 |
| `labor_rights` | 3 | 1 | 1 | 0 |
| `labour_associations` | 6 | 37 | 9 | 28 |
| `land_reform` | 9 | 32 | 13 | 19 |
| `migration` | 3 | 5 | 2 | 3 |
| `navy_model` | 4 | 20 | 2 | 18 |
| `policing` | 4 | 11 | 4 | 7 |
| `rights_of_women` | 5 | 15 | 6 | 9 |
| `slavery` | 5 | 5 | 3 | 2 |
| `taxation` | 5 | 2 | 0 | 2 |
| `trade_policy` | 6 | 44 | 17 | 27 |
| `welfare` | 5 | 15 | 5 | 10 |

## 실행 경로가 존재하는 modifier 키 — 원작 동등성은 별도 검증 필요

| 원본 modifier key | 해당 원본 정의 행 | 연결 소유 계산 | 비고 |
|---|---:|---|---|
| `building_group_bg_agriculture_throughput_add` | 3 | C/D 경제·재정 | 1~3단계 신규 소비자 · 조건부/게임식 단순화 · 미검증 |
| `building_group_bg_extraction_throughput_add` | 1 | C/D 경제·재정 | 1~3단계 신규 소비자 · 조건부/게임식 단순화 · 미검증 |
| `building_group_bg_plantations_throughput_add` | 2 | C/D 경제·재정 | 1~3단계 신규 소비자 · 조건부/게임식 단순화 · 미검증 |
| `building_group_bg_ranching_throughput_add` | 1 | C/D 경제·재정 | 1~3단계 신규 소비자 · 조건부/게임식 단순화 · 미검증 |
| `building_minimum_wage_mult` | 3 | C/D 경제·재정 | VIC2.5 단순화 계산·조건부 |
| `building_nationalization_cost_mult` | 4 | C/D 경제·재정 | 1~3단계 신규 소비자 · 조건부/게임식 단순화 · 미검증 |
| `building_training_rate_mult` | 5 | C/D 경제·재정 | 정적 소비자 연결 |
| `country_academics_pol_str_mult` | 3 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_aristocrats_pol_str_mult` | 10 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_authority_add` | 39 | C/P 정치 | 정적 소비자 연결 |
| `country_authority_cost_add` | 5 | C/P 정치 | 정적 소비자 연결 |
| `country_authority_mult` | 10 | C/P 정치 | 정적 소비자 연결 |
| `country_block_government_reform_bool` | 3 | P1 정부 구성 | 선택형/조건부 실행 판정 · 미검증 |
| `country_bolster_attraction_factor` | 3 | P1/P3/P5 정치 | 신규 소비자 — VIC2.5 단순화 규칙 · 미검증 |
| `country_bureaucrats_pol_str_mult` | 8 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_can_only_conscript_peasants_bool` | 2 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_capitalists_pol_str_mult` | 3 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_clergymen_pol_str_mult` | 3 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_clerks_pol_str_mult` | 1 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_coup_resistance_add` | 7 | P1/P3/P5 정치 | 신규 소비자 — VIC2.5 단순화 규칙 · 미검증 |
| `country_disable_investment_pool_bool` | 1 | C/D 경제·재정 | 1~3단계 신규 소비자 · 조건부/게임식 단순화 · 미검증 |
| `country_disable_nationalization_bool` | 1 | 별도 실행 대상 필요 | 1~3단계 신규 소비자 · 조건부/게임식 단순화 · 미검증 |
| `country_disallow_aggressive_plays_bool` | 1 | H 외교 위기 발의 | 선택형/조건부 실행 판정 · 미검증 |
| `country_disallow_trade_bool` | 1 | G/H 무역·외교 | VIC2.5 단순화 계산·조건부 |
| `country_enactment_success_chance_law_anarchy_add` | 1 | P2 입법 | 신규 소비자 — VIC2.5 단순화 규칙 · 미검증 |
| `country_enactment_success_chance_law_autocracy_add` | 1 | P2 입법 | 신규 소비자 — VIC2.5 단순화 규칙 · 미검증 |
| `country_enactment_success_chance_law_oligarchy_add` | 1 | P2 입법 | 신규 소비자 — VIC2.5 단순화 규칙 · 미검증 |
| `country_enactment_success_chance_law_technocracy_add` | 1 | P2 입법 | 신규 소비자 — VIC2.5 단순화 규칙 · 미검증 |
| `country_engineers_pol_str_mult` | 1 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_farmers_pol_str_mult` | 2 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_force_privatization_bool` | 1 | 별도 실행 대상 필요 | 1~3단계 신규 소비자 · 조건부/게임식 단순화 · 미검증 |
| `country_institution_cost_institution_health_system_mult` | 2 | B/POP·C/P1 | 1~3단계 신규 소비자 · 조건부/게임식 단순화 · 미검증 |
| `country_law_enactment_speed_mult` | 3 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_law_enactment_success_add` | 3 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_legitimacy_base_add` | 6 | C/P 정치 | VIC2.5 단순화 계산·조건부 |
| `country_legitimacy_govt_size_add` | 17 | P1/P3/P5 정치 | 신규 소비자 — VIC2.5 단순화 규칙 · 미검증 |
| `country_legitimacy_govt_total_clout_add` | 15 | C/P 정치 | VIC2.5 단순화 계산·조건부 |
| `country_legitimacy_govt_total_votes_add` | 11 | C/P 정치 | VIC2.5 단순화 계산·조건부 |
| `country_legitimacy_headofstate_add` | 13 | C/P 정치 | VIC2.5 단순화 계산·조건부 |
| `country_legitimacy_ideological_incoherence_mult` | 19 | P1/P3/P5 정치 | 신규 소비자 — VIC2.5 단순화 규칙 · 미검증 |
| `country_loan_interest_rate_add` | 5 | C/D 경제·재정 | 정적 소비자 연결 |
| `country_loan_interest_rate_mult` | 5 | C/D 경제·재정 | 정적 소비자 연결 |
| `country_migration_restrictiveness_add` | 3 | B/POP·C/P1 | VIC2.5 단순화 계산·조건부 |
| `country_officers_pol_str_mult` | 21 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_peasants_pol_str_mult` | 3 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_private_construction_allocation_mult` | 8 | P0/Phase D 민간 건설 | 신규 소비자 — VIC2.5 단순화 규칙 · 미검증 |
| `country_production_tech_research_speed_mult` | 1 | Phase D 기술·연구 | 신규 소비자 — VIC2.5 단순화 규칙 · 미검증 |
| `country_production_tech_spread_mult` | 1 | Phase D 기술·연구 | 신규 소비자 — VIC2.5 단순화 규칙 · 미검증 |
| `country_shopkeepers_pol_str_mult` | 4 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_soldiers_pol_str_mult` | 3 | 별도 실행 대상 필요 | 정적 소비자 연결 |
| `country_support_separatism_resistance_mult` | 14 | P1/P3/P5 정치 | 신규 소비자 — VIC2.5 단순화 규칙 · 미검증 |
| `country_support_separatism_strength_mult` | 9 | P1/P3/P5 정치 | 신규 소비자 — VIC2.5 단순화 규칙 · 미검증 |
| `country_suppression_attraction_factor` | 9 | P1/P3/P5 정치 | 신규 소비자 — VIC2.5 단순화 규칙 · 미검증 |
| `country_tech_research_speed_mult` | 1 | Phase D 기술·연구 | 신규 소비자 — VIC2.5 단순화 규칙 · 미검증 |
| `country_tech_spread_mult` | 6 | Phase D 기술·연구 | 신규 소비자 — VIC2.5 단순화 규칙 · 미검증 |
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
| `political_movement_radicalism_add` | 6 | P1/P3/P5 정치 | 신규 소비자 — VIC2.5 단순화 규칙 · 미검증 |
| `state_aristocrats_investment_pool_efficiency_mult` | 7 | C/D 경제·재정 | 1~3단계 신규 소비자 · 조건부/게임식 단순화 · 미검증 |
| `state_birth_rate_mult` | 4 | B/POP·C/P1 | 정적 소비자 연결 |
| `state_bureaucrats_investment_pool_efficiency_mult` | 3 | C/D 경제·재정 | 1~3단계 신규 소비자 · 조건부/게임식 단순화 · 미검증 |
| `state_capitalists_investment_pool_efficiency_mult` | 8 | C/D 경제·재정 | 1~3단계 신규 소비자 · 조건부/게임식 단순화 · 미검증 |
| `state_clergymen_investment_pool_efficiency_mult` | 3 | C/D 경제·재정 | 1~3단계 신규 소비자 · 조건부/게임식 단순화 · 미검증 |
| `state_conscription_rate_add` | 8 | C/E/H 군사 | VIC2.5 단순화 계산·조건부 |
| `state_education_access_add` | 3 | B/POP·C/P1 | 정적 소비자 연결 |
| `state_farmers_investment_pool_efficiency_mult` | 5 | C/D 경제·재정 | 1~3단계 신규 소비자 · 조건부/게임식 단순화 · 미검증 |
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
| `state_shopkeepers_investment_pool_efficiency_mult` | 6 | C/D 경제·재정 | 1~3단계 신규 소비자 · 조건부/게임식 단순화 · 미검증 |
| `state_tariff_import_add` | 5 | G/H 무역·외교 | VIC2.5 단순화 계산·조건부 |
| `state_tax_capacity_mult` | 5 | C/D 경제·재정 | 행정인력에 기반한 원천징수 실효율(단순화) |
| `state_tax_collection_mult` | 1 | C/D 경제·재정 | 행정인력에 기반한 원천징수 실효율(단순화) |
| `state_trade_capacity_mult` | 2 | G/H 무역·외교 | VIC2.5 단순화 계산·조건부 |

| `country_forbid_monopoly_bool` | 1 | Phase D 실재 설비 기반 공장 확장 | 시설별 기존 생산역량으로만 점유율 추정 · 미검증 |
## 아직 연결되지 않은 modifier 키 — 실제 계산 연결 필요

| 원본 modifier key | 해당 정의 행 | 연결해야 할 영역 |
|---|---:|---|
| `building_art_academy_throughput_add` | 1 | C/D 경제·재정 |
| `building_company_government_dividends_add` | 1 | C/D 경제·재정 |
| `building_company_worker_dividends_add` | 1 | C/D 경제·재정 |
| `building_group_bg_agriculture_allowed_collectivization_add` | 4 | C/D 경제·재정 |
| `building_group_bg_agriculture_construction_efficiency_add` | 1 | C/D 경제·재정 |
| `building_group_bg_agriculture_standard_of_living_add` | 1 | C/D 경제·재정 |
| `building_group_bg_agriculture_tax_mult` | 1 | C/D 경제·재정 |
| `building_group_bg_arts_allowed_collectivization_add` | 5 | C/D 경제·재정 |
| `building_group_bg_extraction_allowed_collectivization_add` | 4 | C/D 경제·재정 |
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
| `building_group_bg_plantations_unincorporated_throughput_add` | 1 | C/D 경제·재정 |
| `building_group_bg_power_allowed_collectivization_add` | 4 | C/D 경제·재정 |
| `building_group_bg_private_infrastructure_allowed_collectivization_add` | 5 | C/D 경제·재정 |
| `building_group_bg_ranching_allowed_collectivization_add` | 4 | C/D 경제·재정 |
| `building_group_bg_ranching_standard_of_living_add` | 1 | C/D 경제·재정 |
| `building_group_bg_ranching_tax_mult` | 1 | C/D 경제·재정 |
| `building_group_bg_rubber_unincorporated_throughput_add` | 1 | C/D 경제·재정 |
| `building_group_bg_subsistence_agriculture_allowed_collectivization_add` | 1 | C/D 경제·재정 |
| `building_group_bg_subsistence_agriculture_peasants_standard_of_living_add` | 1 | C/D 경제·재정 |
| `building_group_bg_subsistence_ranching_allowed_collectivization_add` | 1 | C/D 경제·재정 |
| `building_group_bg_subsistence_ranching_peasants_standard_of_living_add` | 1 | C/D 경제·재정 |
| `building_minimum_incorporated_subsistence_employment_add` | 2 | C/D 경제·재정 |
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
| `country_dalit_acceptance_max_add` | 2 | B/POP·C/P1 |
| `country_dalit_cultural_acceptance_mult` | 2 | B/POP·C/P1 |
| `country_dalit_education_access_mult` | 1 | B/POP·C/P1 |
| `country_dalit_qualification_growth_mult` | 1 | B/POP·C/P1 |
| `country_dalit_qualification_growth_other_class_mult` | 1 | B/POP·C/P1 |
| `country_dalit_qualification_growth_same_class_mult` | 2 | B/POP·C/P1 |
| `country_disable_nationalization_without_compensation_bool` | 5 | 별도 실행 대상 필요 |
| `country_disable_non_company_privatization_bool` | 1 | 별도 실행 대상 필요 |
| `country_disallow_agitator_invites_bool` | 1 | 별도 실행 대상 필요 |
| `country_disallow_trade_outside_canton_bool` | 1 | G/H 무역·외교 |
| `country_disallow_trade_outside_kyushu_bool` | 1 | G/H 무역·외교 |
| `country_economic_dependence_on_overlord_add` | 2 | 별도 실행 대상 필요 |
| `country_education_fervor_add` | 2 | B/POP·C/P1 |
| `country_electoral_confidence_impact_mult` | 7 | 별도 실행 대상 필요 |
| `country_fervor_target_basque_add` | 1 | B/POP·C/P1 |
| `country_financial_districts_buy_farms_likelihood` | 3 | 별도 실행 대상 필요 |
| `country_forbid_electoral_fraud_bool` | 1 | 별도 실행 대상 필요 |
| `country_foreign_collectivization_bool` | 1 | 별도 실행 대상 필요 |
| `country_free_charters_add` | 8 | 별도 실행 대상 필요 |
| `country_government_dividends_efficiency_add` | 4 | C/D 경제·재정 |
| `country_government_dividends_reinvestment_add` | 6 | C/D 경제·재정 |
| `country_government_wages_mult` | 2 | C/D 경제·재정 |
| `country_influence_mult` | 4 | G/H 무역·외교 |
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
| `ship_power_projection_mult` | 2 | C/E/H 군사 |
| `state_aristocrats_investment_pool_contribution_add` | 1 | C/D 경제·재정 |
| `state_assimilation_mult` | 5 | B/POP·C/P1 |
| `state_building_barrack_max_level_add` | 6 | C/E/H 군사 |
| `state_building_conscription_center_max_level_add` | 6 | C/E/H 군사 |
| `state_bureaucracy_population_base_cost_factor_mult` | 8 | C/D 경제·재정 |
| `state_colony_growth_creation_factor` | 1 | G/H 무역·외교 |
| `state_colony_growth_speed_mult` | 2 | G/H 무역·외교 |
| `state_conscription_rate_mult` | 3 | C/E/H 군사 |
| `state_conversion_mult` | 6 | B/POP·C/P1 |
| `state_dependent_political_participation_add` | 2 | C/P 정치 |
| `state_dependent_wage_mult` | 7 | B/POP·C/P1 |
| `state_education_access_wealth_add` | 1 | B/POP·C/P1 |
| `state_expected_sol_mult` | 1 | 별도 실행 대상 필요 |
| `state_export_advantage_mult` | 1 | G/H 무역·외교 |
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

## 2026-11-10 후속 연결 (2차)

- P5: 기존 실인구 기반 운동 위험에 치안기관의 실제 운영 능력에 따른 쿠데타 저항력, 분리주의 저항·강도, 정치운동 급진성 보정을 적용. 혁명군·국가·POP를 새로 생성하지 않는다.
- P1: 권위를 사용하는 실제 운동 지원·억압에 언론법의 attraction factor를 적용한다. 정부의 실제 IG/법률 불일치를 정통성의 이념 불일치로 반영하고, 법률상 정부 규모 가중을 적용한다.
- P2: 실제 개정 대상 법률과 정확히 일치하는 성공률 보정만 활성 법률/증보에서 조회한다. 다른 선택지에 패널티를 일반화하지 않는다.
- Phase D/P0: 법률상 연구 속도 및 전파 속도는 유료 기술 연구의 **필요 증빙량**을 변경하며 검증된 작업량과 실제 지출은 그대로 유지한다. 전파에는 실제 기술이전 수업·자금만 사용한다. 기존 프로젝트의 요구량은 시행법 변경 시 다시 계산하되 POP·돈·재료를 재생성하지 않는다.
- P0/Phase D: 민간 건설 배분율은 완료된 건설을 생성하지 않고 일일 **유효 발주 상한**만 조정한다. 현금, 자재, 직원, 실제 시공 실적, 시설 완공은 계속 기존 원장에 귀속한다.
- 2차 추가 키 **17종**. 이전 연결 키 포함 총 **80종**, 미연결 **270종**. 이는 내용적 완성률이 아니라 정적 modifier 키별 코드 소비자 존재 여부다.

**연결 범위 한계:** 원작에서는 별도의 이념/정치운동/건설·연구 엔진에서 계산하는 효과를 VIC2.5의 단순화된 P1/P5/P0/D 규칙에 맞춰 사용한다. 엄밀한 1.15 수치 동등성을 주장하지 않는다. 본 회차 컴파일 및 실제 시나리오 재생은 실행하지 않았다.

## 추가: 헌정·외교법 효과

- `country_block_government_reform_bool`: 실제 발효된 법률 증보가 있을 때 재직 정부의 재량적 연정 재편을 제한한다. 관리정부의 승계나 실제 새 총선 직후의 합법적 정부 구성은 예외로 유지한다.
- `country_disallow_aggressive_plays_bool`: 실제 대외 위기를 새로 발의할 때 금지한다. 기존 위기의 협상·방어전쟁·진행 중 전쟁을 자동 폐기하지 않는다.
- 이 두 항목도 전체 빅토리아 3 정치·외교 스크립트를 복원한다는 의미가 아닌 VIC2.5의 구체적 법률 소비자 연결이다.

## 2026-11-10 후속작업: 경제 1~3단계

**1. 산업 처리량.** Phase D 제조업 생산배치와 채굴/채취의 기존 물적 입력·직원·시설 용량은 그대로 소모한다. 산업 분류 클래스 및 상위 분류가 확인될 때만 농업·플랜테이션·채굴·목축 업종의 법률상 처리량을 생산 산출량에 연결한다. 일간 소량 생산은 한 달 누적량의 차분으로 정수 보너스를 반영해 절삭 손실을 줄인다. 확률이나 임의 화폐·POP 생성은 하지 않는다. 공장 수익성 의사결정 역시 생산량 개선을 고려한다.

**2. 투자·소유권.** 계획경제의 투자풀 금지법은 기업의 자율 확장 제안을 차단하되, 이미 자금·시공계약이 있는 명시적 공공 건설을 자동 삭제하지 않는다. 실재 기업 주식의 가계주주와 실제 직업 분포가 모두 확인될 때만 직업별 투자효율을 P0의 일간 **발주한도**에 연결한다. 국유화는 실제 국고 현금으로 기존 완공시설을 보상 매입하며 원래 사업주·시설·유가증권·장부자산을 동일 Stage7 거래에서 변경한다. 자유방임의 강제 민영화는 입찰자와 실제 매입 대금이 존재할 때에만 허용하며 매수자가 없는 경우 임의로 처분하지 않는다. 다른 자산 소유권이나 외부 투자자를 자동 창작하지 않는다.

**3. 세제·국가재정.** 법률군 `taxation`의 5개 선택지는 이제 각기 다른 과세 대상을 사용한다: 비례세는 기존 급여, 누진세는 실제 급여·지급 인원과 관측된 전국 월급여, 소비세는 실제 가계 상품구매 및 공공국고로의 징수, 인두세는 실존 성인 POP/가계현금·관측 급여, 토지세는 실제 완공 농업시설의 지불된 장부가액과 현금. 이는 빅토리아 3의 완전한 세수 함수가 아닌 보수적 게임식 근사이며 실재 과세 대상이 없으면 세수 0이다. 기존 국고 부채·현금 원장만 사용한다. 실재 공공의료 운영서비스 보조 지출은 건강보험제도 비용 보정치를 적용하며, 돈 없는 국가는 보조금이 생성되지 않는다.

**별도 완료해야 할 항목:** 대다수 건물별 집산화·회사 배당·국영기업 재투자·중립/외국 투자권·시장 접근성·민간기업 구조조정, 건설 효율의 *원작식* 전환, 모든 전문직 투자자 분포가 관측되지 않는 국가, 지역별 부동산/인구 과세의 정밀한 월별 회계, 교육·치안·복지기관의 실제 법정 예산과 모든 원작 조건부 세제 트리거. 신규 등록된 P0/D/C/P2 집중 검사도 **컴파일 및 CTest를 아직 실행하지 않았다.**

**이번 누적 정적 연결:** 98/350 고유 키, 468/1117 정적 modifier 행. 분모는 원본 효과 키 행수이지 결과 정확도, 실제 사용량, 동등성 또는 전 법률 구현 완료율이 아니다.

경제 후속 수정: `country_forbid_monopoly_bool`은 기존에 완공·가동 가능한 실제 제조 시설들의 지역별 설비용량을 비교하여, 해당 기업의 명확한 절대다수(50% 초과) 설비 지배 시 같은 품목 자동 증설을 거부한다. 미완공·잠재 시설을 경쟁사로 허위 계산하지 않는다. 공식 빅토리아 3 시장점유율 공식과 동일하다고 가정하지 않는다. 인두세 국가 임금기준은 일간 가계당 반복 계산하지 않고 국가당 1회만 계산한다.
