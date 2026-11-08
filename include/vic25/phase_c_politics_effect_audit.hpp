#pragma once
// Audit-only mapping of sourced Victoria 3 IG trait modifier keys to native
// VIC2.5 execution paths. This does not create gameplay effects or ledgers.
// Unknown keys are UNSUPPORTED, never silently declared implemented.
#include "vic25/phase_c_politics_catalog.hpp"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace vic25::phasec::politics {
struct EffectAudit {
    std::string trait,modifier;
    bool wired{};
    std::string executor;
};
inline const std::map<std::string,std::string>& wired_trait_effects() {
    static const std::map<std::string,std::string> routes={
        {"unit_offense_mult","p0_daily::fight"},
        {"unit_defense_mult","p0_daily::fight"},
        {"country_military_tech_research_speed_mult","p0_daily::research"},
        {"country_production_tech_research_speed_mult","p0_daily::research"},
        {"country_society_tech_research_speed_mult","p0_daily::research"},
        {"country_tech_spread_mult","p0_daily::research"},
        {"state_birth_rate_mult","phase_c::phase_time_auto_demography"},
        {"state_education_access_add","phase_c::phase_time_auto_demography"},
        {"state_migration_pull_mult","phase_c::phase_time_auto_demography"},
        {"country_loan_interest_rate_mult","phase_c::phase_time_accrue_and_settle_interest"},
        {"country_bureaucracy_mult","phase_c::p1_institution_capacity"},
        {"building_group_bg_manufacturing_throughput_add","phase_d::run_recipe"},
        {"building_group_bg_agriculture_throughput_add","phase_d::run_recipe"},
        {"building_group_bg_plantations_throughput_add","phase_d::run_recipe"},
        {"building_group_bg_ranching_throughput_add","phase_d::run_recipe"},
        {"state_construction_mult","p0_daily::investment"},
        {"state_capitalists_investment_pool_efficiency_mult","p0_daily::investment"},
        {"state_aristocrats_investment_pool_efficiency_mult","p0_daily::investment"},
        {"state_infrastructure_mult","phasegh::Stage::effective_edge_capacity"},
        {"country_military_goods_cost_mult","phasegh::Stage::plan_autonomous_trade"},
        {"state_radicalism_increases_violent_hostility_mult","phasec::IntegratedWorld::p3_daily_react"},
        {"state_radicalism_increases_cultural_erasure_mult","phasec::IntegratedWorld::p3_daily_react"},
        {"state_radicalism_increases_open_prejudice_mult","phasec::IntegratedWorld::p3_daily_react"}
    };
    return routes;
}
inline std::vector<EffectAudit> audit_ig_traits() {
    std::vector<EffectAudit> out;
    for(const auto& rule:ig_trait_rules()) {
        std::istringstream input(rule.encoded);
        for(std::string token;input>>token;) {
            const auto colon=token.rfind(':');
            if(colon==std::string::npos)
                throw std::logic_error("POLITICS_INVALID_TRAIT_SOURCE");
            const auto key=token.substr(0,colon);
            const auto it=wired_trait_effects().find(key);
            out.push_back({rule.id,key,it!=wired_trait_effects().end(),
                           it==wired_trait_effects().end()?
                           std::string{}:it->second});
        }
    }
    return out;
}
} // namespace vic25::phasec::politics
