#ifdef NDEBUG
#undef NDEBUG
#endif
#include "vic25/phase_c_p2_catalog_115.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <string_view>

// Original Victoria 3 1.15 law definitions: 41 previously disconnected
// source rows across 20 distinct keys. Every row stays available after
// integration, even for modifiers whose actual game-level semantics differ
// from Victoria 3. See the law-effect coverage contract for consumer owners.
int main() {
    using namespace vic25::phasec;
    struct Row {std::string_view law,key;double value;bool boolean;};
    constexpr std::array<Row,41> required{{
    {"law_traditionalism","state_bureaucracy_population_base_cost_factor_mult",-0.25,false},
    {"law_traditionalism","country_government_dividends_reinvestment_add",0.25,false},
    {"law_traditionalism","building_nationalization_investment_return_add",0.1,false},
    {"law_traditionalism","country_leverage_resistance_mult",0.25,false},
    {"law_traditionalism","state_market_access_price_impact",-0.15,false},
    {"law_traditionalism","building_self_investment_chance_add",0.25,false},
    {"law_traditionalism","country_disable_nationalization_without_compensation_bool",1,true},
    {"law_interventionism","country_disable_nationalization_without_compensation_bool",1,true},
    {"law_interventionism","building_nationalization_investment_return_add",0.5,false},
    {"law_interventionism","country_government_dividends_reinvestment_add",0.5,false},
    {"law_interventionism","country_free_charters_add",1,false},
    {"law_agrarianism","building_nationalization_investment_return_add",0.5,false},
    {"law_agrarianism","country_government_dividends_reinvestment_add",0.5,false},
    {"law_agrarianism","country_free_charters_add",1,false},
    {"law_agrarianism","country_disable_nationalization_without_compensation_bool",1,true},
    {"law_industry_banned","state_expected_sol_mult",-0.1,false},
    {"law_industry_banned","state_aristocrats_investment_pool_contribution_add",0.25,false},
    {"law_industry_banned","building_nationalization_investment_return_add",0.5,false},
    {"law_industry_banned","country_government_dividends_reinvestment_add",0.25,false},
    {"law_industry_banned","country_disable_nationalization_without_compensation_bool",1,true},
    {"law_industry_banned","country_economic_dependence_on_overlord_add",-0.5,false},
    {"law_industry_banned","country_free_charters_add",1,false},
    {"law_extraction_economy","building_nationalization_investment_return_add",0.5,false},
    {"law_extraction_economy","country_disable_nationalization_without_compensation_bool",1,true},
    {"law_extraction_economy","country_economic_dependence_on_overlord_add",0.5,false},
    {"law_laissez_faire","country_government_dividends_reinvestment_add",1,false},
    {"law_laissez_faire","country_free_charters_add",2,false},
    {"law_cooperative_ownership","country_government_dividends_reinvestment_add",1,false},
    {"law_cooperative_ownership","country_free_charters_add",1,false},
    {"law_cooperative_ownership","building_group_bg_manufacturing_allowed_collectivization_add",1,false},
    {"law_cooperative_ownership","building_group_bg_extraction_allowed_collectivization_add",1,false},
    {"law_cooperative_ownership","building_group_bg_power_allowed_collectivization_add",1,false},
    {"law_cooperative_ownership","building_group_bg_private_infrastructure_allowed_collectivization_add",1,false},
    {"law_cooperative_ownership","building_group_bg_arts_allowed_collectivization_add",1,false},
    {"law_cooperative_ownership","country_other_country_socialist_economy_hostility_impact_add",50,false},
    {"law_cooperative_ownership","country_foreign_collectivization_bool",1,true},
    {"law_command_economy","building_nationalization_radicals_mult",-0.75,false},
    {"law_command_economy","country_free_charters_add",1,false},
    {"law_command_economy","country_leverage_resistance_mult",0.25,false},
    {"law_command_economy","country_other_country_socialist_economy_hostility_impact_add",50,false},
    {"law_command_economy","country_disable_non_company_privatization_bool",1,true}
    }};
    for(const auto& row:required) {
        const auto* law=p2_catalog_law(row.law);
        assert(law&&law->group=="economic_system");
        const auto got=p2_catalog_raw_modifier(row.law,row.key);
        if(row.boolean)assert(got==1.0);
        else assert(std::abs(got-row.value)<0.0000001);
    }
    assert(required.size()==41);
}
