#pragma once
// Original 8 IG x 3 traits (24 rows), source:
// https://github.com/alcaras/v3ref/blob/main/src/data/interest_groups.json
// Updated 1.15 Devout loyal fertility trait +3% from:
// https://vic3tabula.com/en/interest-groups
// Approval thresholds in VIC2.5's 0-100 scale are adapted to loyal>=75,
// happy>=60 and unhappy<=35. These are explicit game balance conversions,
// NOT the original Victoria 3 approval points.
//
// Included after P3GroupType and Catalogue declaration inside politics.
struct IGEffect {
    std::string id;
    std::string modifier;
    std::int64_t value_ppm{};
};
struct IGRule {
    P3GroupType type;
    const char* id;
    int min_approval;
    int max_approval;
    const char* encoded;
};
inline const std::vector<IGRule>& ig_trait_rules() {
    static const std::vector<IGRule> rules={
        {P3GroupType::armed_forces,"ig_trait_patriotic_fervor",75,101,"unit_offense_mult:100000 unit_defense_mult:100000"},
        {P3GroupType::armed_forces,"ig_trait_veteran_consultation",60,101,"country_military_tech_research_speed_mult:100000"},
        {P3GroupType::armed_forces,"ig_trait_materiel_waste",-1,35,"country_military_goods_cost_mult:150000"},
        {P3GroupType::devout,"ig_trait_be_fruitful_and_multiply",75,101,"state_birth_rate_mult:30000"},
        {P3GroupType::devout,"ig_trait_divine_right",60,101,"country_authority_mult:100000"},
        {P3GroupType::devout,"ig_trait_pious_fiction",-1,35,"state_education_access_add:-100000"},
        {P3GroupType::industrialists,"ig_trait_job_creators",75,101,"state_capitalists_investment_pool_efficiency_mult:100000"},
        {P3GroupType::industrialists,"ig_trait_engines_of_progress",60,101,"country_production_tech_research_speed_mult:100000"},
        {P3GroupType::industrialists,"ig_trait_tax_avoidance",-1,35,"building_group_bg_manufacturing_tax_mult:-100000"},
        {P3GroupType::intelligentsia,"ig_trait_propagandists",75,101,"state_migration_pull_mult:150000"},
        {P3GroupType::intelligentsia,"ig_trait_avant_garde",60,101,"country_society_tech_research_speed_mult:100000"},
        {P3GroupType::intelligentsia,"ig_trait_social_criticism",-1,35,"country_prestige_mult:-100000"},
        {P3GroupType::landowners,"ig_trait_noblesse_oblige",75,101,"state_aristocrats_investment_pool_efficiency_mult:100000"},
        {P3GroupType::landowners,"ig_trait_family_ties",60,101,"country_influence_mult:100000"},
        {P3GroupType::landowners,"ig_trait_noble_privileges",-1,35,"building_group_bg_agriculture_tax_mult:-100000 building_group_bg_ranching_tax_mult:-100000 building_group_bg_plantations_tax_mult:-100000"},
        {P3GroupType::petite_bourgeoisie,"ig_trait_treasury_bonds",75,101,"country_loan_interest_rate_mult:-100000"},
        {P3GroupType::petite_bourgeoisie,"ig_trait_middle_managers",60,101,"country_bureaucracy_mult:100000"},
        {P3GroupType::petite_bourgeoisie,"ig_trait_xenophobia",-1,35,"state_radicalism_increases_violent_hostility_mult:100000 state_radicalism_increases_cultural_erasure_mult:100000 state_radicalism_increases_open_prejudice_mult:100000 country_influence_mult:-100000"},
        {P3GroupType::rural_folk,"ig_trait_farmers_markets",75,101,"state_infrastructure_mult:100000"},
        {P3GroupType::rural_folk,"ig_trait_honest_work",60,101,"building_group_bg_agriculture_throughput_add:100000 building_group_bg_plantations_throughput_add:100000 building_group_bg_ranching_throughput_add:100000"},
        {P3GroupType::rural_folk,"ig_trait_old_ways",-1,35,"country_tech_spread_mult:-100000"},
        {P3GroupType::trade_unions,"ig_trait_solidarity",75,101,"state_working_adult_ratio_add:50000"},
        {P3GroupType::trade_unions,"ig_trait_industrial_organizers",60,101,"building_group_bg_manufacturing_throughput_add:100000"},
        {P3GroupType::trade_unions,"ig_trait_work_to_rule",-1,35,"state_construction_mult:-100000"},
    };
    return rules;
}
inline std::vector<IGEffect> active_ig_effects(
        P3GroupType type,int approval,std::int64_t members) {
    std::vector<IGEffect> effects;
    if(members<=0)return effects;
    for(const auto& rule:ig_trait_rules()) {
        if(rule.type!=type||approval<rule.min_approval||
           approval>rule.max_approval)continue;
        std::istringstream input(rule.encoded);
        for(std::string token;input>>token;) {
            const auto split=token.rfind(':');
            if(split==std::string::npos)
                throw std::logic_error("POLITICS_BAD_TRAIT_EFFECT");
            effects.push_back({rule.id,token.substr(0,split),
                               std::stoll(token.substr(split+1))});
        }
    }
    return effects;
}
