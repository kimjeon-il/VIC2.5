#pragma once
// Exact direct *unconditional* modifier definitions, 1.15 source SHA 9fab6db71edc7df473aea00cd3ac81092b6cef20.
// 12 source law files, final 58 laws.
inline void install_vic3_115_law_effects_b(Catalogue& out) {
    static constexpr std::array<std::pair<const char*,const char*>,58> rows{{
        {"serfdom","interest_group_ig_landowners_pol_str_mult:250000:n state_peasants_internal_migration_disallowed_bool:1000000:b state_peasants_mass_migration_disallowed_bool:1000000:b state_minimum_incorporated_subsistence_arable_land_add:750000:n building_minimum_incorporated_subsistence_employment_add:500000:n"},
        {"manorialism","interest_group_ig_landowners_pol_str_mult:150000:n state_minimum_incorporated_subsistence_arable_land_add:500000:n building_minimum_incorporated_subsistence_employment_add:500000:n"},
        {"tenant_farmers","interest_group_ig_landowners_pol_str_mult:250000:n state_peasants_internal_migration_disallowed_bool:1000000:b state_minimum_incorporated_subsistence_arable_land_add:250000:n"},
        {"latifundias","country_aristocrats_pol_str_mult:400000:n state_aristocrats_investment_pool_efficiency_mult:-200000:n country_peasants_pol_str_mult:-200000:n"},
        {"expanded_latifundias","country_aristocrats_pol_str_mult:400000:n state_aristocrats_investment_pool_efficiency_mult:200000:n country_peasants_pol_str_mult:-400000:n"},
        {"commercialized_agriculture","country_financial_districts_buy_farms_likelihood:750000:n"},
        {"peasant_proprietorship","country_farmers_pol_str_mult:150000:n building_group_bg_subsistence_agriculture_allowed_collectivization_add:250000:n building_group_bg_subsistence_ranching_allowed_collectivization_add:250000:n building_group_bg_agriculture_allowed_collectivization_add:250000:n building_group_bg_ranching_allowed_collectivization_add:250000:n country_financial_districts_buy_farms_likelihood:250000:n state_minimum_incorporated_subsistence_arable_land_add:350000:n"},
        {"homesteading","country_farmers_pol_str_mult:250000:n building_group_bg_agriculture_allowed_collectivization_add:500000:n building_group_bg_ranching_allowed_collectivization_add:500000:n state_minimum_incorporated_subsistence_arable_land_add:350000:n"},
        {"collectivized_agriculture","building_group_bg_agriculture_allowed_collectivization_add:1000000:n building_group_bg_ranching_allowed_collectivization_add:1000000:n building_group_bg_plantations_allowed_collectivization_add:1000000:n"},
        {"no_police",""},
        {"local_police",""},
        {"dedicated_police","country_institution_police_max_investment_add:2000000:n"},
        {"militarized_police","country_institution_police_max_investment_add:2000000:n"},
        {"consumption_based_taxation","state_bureaucracy_population_base_cost_factor_mult:-250000:n country_consumption_tax_cost_mult:-500000:n"},
        {"land_based_taxation",""},
        {"per_capita_based_taxation",""},
        {"proportional_taxation",""},
        {"graduated_taxation",""},
        {"mercantilism","state_tariff_import_add:500000:n state_tariff_export_add:200000:n state_subvention_export_add:500000:n state_subvention_import_add:200000:n state_import_advantage_mult:-250000:n state_export_advantage_mult:250000:n"},
        {"protectionism","state_tariff_import_add:500000:n state_tariff_export_add:500000:n state_subvention_export_add:500000:n state_subvention_import_add:500000:n country_leverage_resistance_mult:250000:n"},
        {"free_trade","state_subvention_export_add:500000:n state_subvention_import_add:500000:n state_trade_advantage_mult:250000:n country_leverage_resistance_mult:-250000:n"},
        {"isolationism","country_authority_mult:500000:n state_tax_capacity_mult:250000:n country_tech_spread_mult:-150000:n country_leverage_resistance_mult:1000000:n country_support_separatism_resistance_mult:500000:n country_disallow_trade_bool:1000000:b"},
        {"canton_system","country_authority_mult:250000:n state_tax_capacity_mult:150000:n country_tech_spread_mult:-100000:n state_trade_capacity_mult:-250000:n state_tariff_import_add:500000:n state_tariff_export_add:500000:n state_subvention_export_add:200000:n state_subvention_import_add:200000:n country_leverage_resistance_mult:750000:n country_disallow_trade_outside_canton_bool:1000000:b state_trade_center_max_limit_add:50000000:n"},
        {"sakoku","country_authority_mult:250000:n country_influence_mult:-500000:n country_tech_spread_mult:-200000:n state_trade_capacity_mult:-750000:n state_tariff_import_add:500000:n state_tariff_export_add:500000:n state_subvention_export_add:200000:n state_subvention_import_add:200000:n country_leverage_resistance_mult:750000:n country_disallow_trade_outside_kyushu_bool:1000000:b state_trade_center_max_limit_add:5000000:n country_disallow_aggressive_plays_bool:1000000:b"},
        {"child_labor_allowed","state_laborers_mortality_mult:50000:n state_machinists_mortality_mult:50000:n state_farmers_mortality_mult:50000:n state_peasants_mortality_mult:50000:n state_dependent_wage_mult:300000:n"},
        {"restricted_child_labor","state_laborers_mortality_mult:20000:n state_farmers_mortality_mult:20000:n state_peasants_mortality_mult:20000:n state_dependent_wage_mult:150000:n country_institution_schools_max_investment_add:1000000:n"},
        {"compulsory_primary_school","country_institution_schools_max_investment_add:2000000:n"},
        {"outlawed_dissent","country_authority_add:200000000:n country_bolster_attraction_factor:500000:n country_suppression_attraction_factor:-500000:n country_tech_spread_mult:-150000:n country_leverage_resistance_mult:500000:n country_support_separatism_resistance_mult:500000:n country_war_support_battles_increase_mult:-200000:n country_war_support_battles_decrease_mult:-400000:n country_war_support_casualties_mult:-250000:n"},
        {"censorship","country_authority_add:100000000:n country_bolster_attraction_factor:400000:n country_suppression_attraction_factor:-400000:n country_tech_spread_mult:-100000:n country_leverage_resistance_mult:250000:n country_support_separatism_resistance_mult:250000:n country_war_support_battles_increase_mult:-150000:n country_war_support_battles_decrease_mult:-250000:n country_war_support_casualties_mult:-150000:n"},
        {"right_of_assembly","country_authority_add:50000000:n country_bolster_attraction_factor:300000:n country_suppression_attraction_factor:-300000:n country_support_separatism_resistance_mult:-100000:n country_war_support_battles_increase_mult:150000:n country_war_support_battles_decrease_mult:150000:n country_war_support_casualties_mult:100000:n"},
        {"protected_speech","country_tech_spread_mult:250000:n country_leverage_resistance_mult:-250000:n country_support_separatism_resistance_mult:-250000:n country_war_support_battles_increase_mult:300000:n country_war_support_battles_decrease_mult:300000:n country_war_support_casualties_mult:200000:n"},
        {"no_workers_rights",""},
        {"regulatory_bodies",""},
        {"worker_protections",""},
        {"guild_system","building_group_bg_manufacturing_allowed_collectivization_add:250000:n building_group_bg_extraction_allowed_collectivization_add:250000:n building_group_bg_power_allowed_collectivization_add:250000:n building_group_bg_private_infrastructure_allowed_collectivization_add:250000:n building_group_bg_arts_allowed_collectivization_add:250000:n country_shopkeepers_pol_str_mult:250000:n building_self_investment_chance_add:250000:n state_pop_qualifications_mult:-200000:n"},
        {"combination_acts","interest_group_ig_trade_unions_pol_str_mult:-250000:n state_radicals_from_political_movements_mult:250000:n state_pop_support_movement_labor_mult:-100000:n state_pop_support_movement_communist_mult:100000:n state_pop_support_movement_anarchist_mult:100000:n state_pop_support_movement_socialist_mult:100000:n"},
        {"anti_strike_laws","interest_group_ig_trade_unions_pol_str_mult:-100000:n state_radicals_from_political_movements_mult:100000:n state_pop_support_movement_labor_mult:-50000:n state_pop_support_movement_communist_mult:50000:n state_pop_support_movement_anarchist_mult:50000:n state_pop_support_movement_socialist_mult:50000:n"},
        {"right_to_associate","interest_group_ig_trade_unions_pol_str_mult:100000:n state_radicals_from_political_movements_mult:-100000:n state_pop_support_movement_labor_mult:100000:n"},
        {"corporatized_unions","state_radicals_and_loyalists_from_sol_change_mult:150000:n state_pop_support_movement_communist_mult:-50000:n state_pop_support_movement_anarchist_mult:-50000:n state_pop_support_movement_socialist_mult:-50000:n"},
        {"factory_councils","interest_group_ig_trade_unions_pol_str_mult:250000:n state_pop_support_movement_labor_mult:-100000:n state_pop_support_movement_communist_mult:250000:n state_pop_support_movement_socialist_mult:250000:n state_pop_support_movement_anarchist_mult:250000:n building_group_bg_manufacturing_allowed_collectivization_add:500000:n building_group_bg_extraction_allowed_collectivization_add:500000:n building_group_bg_power_allowed_collectivization_add:500000:n building_group_bg_private_infrastructure_allowed_collectivization_add:500000:n building_group_bg_arts_allowed_collectivization_add:500000:n"},
        {"no_migration_controls","country_leverage_resistance_mult:-250000:n"},
        {"migration_controls","country_migration_restrictiveness_add:60000000:n"},
        {"closed_borders","country_migration_restrictiveness_add:101000000:n country_disallow_agitator_invites_bool:1000000:b country_leverage_resistance_mult:250000:n"},
        {"no_womens_rights","state_birth_rate_mult:50000:n"},
        {"women_in_the_fields","state_birth_rate_mult:-100000:n state_working_adult_ratio_add:300000:n state_conscription_rate_add:400000:n building_training_rate_mult:1000000:n state_middle_strata_standard_of_living_add:-8000000:n state_lower_strata_standard_of_living_add:-4000000:n state_middle_strata_expected_sol_add:-4000000:n state_lower_strata_expected_sol_add:-2000000:n"},
        {"women_own_property","state_working_adult_ratio_add:50000:n"},
        {"women_in_the_workplace","state_working_adult_ratio_add:100000:n state_birth_rate_mult:-50000:n"},
        {"womens_suffrage","state_working_adult_ratio_add:150000:n state_birth_rate_mult:-50000:n state_dependent_political_participation_add:330000:n"},
        {"slavery_banned",""},
        {"colonial_slavery",""},
        {"debt_slavery","interest_group_ig_landowners_pol_str_mult:250000:n"},
        {"slave_trade","interest_group_ig_landowners_pol_str_mult:500000:n country_influence_mult:-250000:n"},
        {"legacy_slavery","interest_group_ig_landowners_pol_str_mult:250000:n country_influence_mult:-100000:n"},
        {"no_social_security",""},
        {"chiefs_distribute_aid","country_institution_social_security_max_investment_add:4000000:n"},
        {"poor_laws",""},
        {"wage_subsidies",""},
        {"old_age_pension",""},
    }};
    for(const auto& [id,encoded]:rows) {
        if(!out.laws.contains(id)||out.modifiers.contains(id))
            throw std::logic_error("POLITICS_115_DUPLICATE_OR_MISSING_LAW");
        auto& effects=out.modifiers[id];
        std::istringstream terms(encoded);
        for(std::string term;terms>>term;) {
            const auto kind=term.rfind(':');
            const auto value=term.rfind(':',kind-1);
            if(value==std::string::npos||kind==std::string::npos)
                throw std::logic_error("POLITICS_115_BAD_EFFECT");
            const auto key=term.substr(0,value);
            const auto val=std::stoll(term.substr(value+1,kind-value-1));
            const auto tag=term.substr(kind+1);
            if(tag!="b"&&tag!="n")throw std::logic_error("POLITICS_115_BAD_TYPE");
            if(!effects.emplace(key,LawModifier{val,tag=="b"}).second)
                throw std::logic_error("POLITICS_115_DUPLICATE_LAW_EFFECT");
        }
    }
    if(out.modifiers.size()!=148)throw std::logic_error("POLITICS_115_LAW_EFFECT_COVERAGE");
}
