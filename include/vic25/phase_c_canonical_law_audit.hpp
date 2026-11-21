#pragma once
#include "vic25/phase_c_politics_effect_audit.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace vic25::phasec::politics {

// This diagnostic describes evidence, never applies effects. An unknown row
// is not a claim that the simulation lacks a consumer for it.
enum class CanonicalLawRowStatus {
    no_recorded_path,
    named_candidate,
    verified_row
};

struct CanonicalLawRowEvidence {
    P2CatalogModifier row;
    std::string_view executor,behavior_test;
};

struct CanonicalLawEffectAudit {
    P2CatalogModifier source;
    // Numeric values are signed millionths of the authored unit (including
    // absolute bonuses); boolean values use 0/1, as in p2_active_modifiers.
    std::int64_t normalized_value{};
    CanonicalLawRowStatus status{};
    std::string executor,behavior_test;
};

inline bool same_canonical_law_row(
        const P2CatalogModifier& a,const P2CatalogModifier& b) {
    return a.source_id==b.source_id && a.key==b.key && a.value==b.value &&
           a.boolean==b.boolean && a.institution==b.institution &&
           a.sponsor==b.sponsor;
}

inline CanonicalLawEffectAudit classify_canonical_law_row(
        const P2CatalogModifier& row,
        std::span<const CanonicalLawRowEvidence> evidence,
        std::string_view named_executor={}) {
    CanonicalLawEffectAudit result{
        row,row.boolean?static_cast<std::int64_t>(row.value!=0):
            static_cast<std::int64_t>(std::llround(row.value*1'000'000.0)),
        CanonicalLawRowStatus::no_recorded_path,{},{}};
    for(const auto& proof:evidence)
        if(same_canonical_law_row(row,proof.row) &&
           !proof.executor.empty() && !proof.behavior_test.empty()) {
            result.status=CanonicalLawRowStatus::verified_row;
            result.executor=proof.executor;
            result.behavior_test=proof.behavior_test;
            return result;
        }
    if(!named_executor.empty()) {
        result.status=CanonicalLawRowStatus::named_candidate;
        result.executor=named_executor;
    }
    return result;
}

inline const std::array<CanonicalLawRowEvidence,99>& canonical_law_row_evidence() {
    // Curated source rows covered by the named behavior comparison. Merely
    // adding a key to the old map does not establish proof here.
    static const std::array<CanonicalLawRowEvidence,99> evidence{{
        {{"law_religious_schools","state_education_access_add",0.1,false,true},
         "IntegratedWorld::phase_time_auto_demography",
         "test_politics_identity::enacted_education_changes_real_daily_schooling"},
        {{"law_public_schools","state_education_access_add",0.125,false,true},
         "IntegratedWorld::phase_time_auto_demography",
         "test_politics_identity::enacted_education_changes_real_daily_schooling"},
        {{"law_terakoya","state_education_access_add",0.25,false,false},
         "IntegratedWorld::phase_time_auto_demography",
         "test_politics_identity::enacted_education_changes_real_daily_schooling"},
        {{"law_industry_banned","state_peasants_education_access_add",-0.1,false,false},
         "IntegratedWorld::phase_time_auto_demography",
         "test_politics_domain::occupation_specific_law_changes_actual_peasant_schooling"},
        {{"law_terakoya","state_peasants_education_access_add",0.2,false,false},
         "IntegratedWorld::phase_time_auto_demography",
         "test_politics_domain::occupation_specific_law_changes_actual_peasant_schooling"},
        {{"law_frontier_colonization","state_migration_pull_unincorporated_mult",0.5,false,false},
         "IntegratedWorld::phase_time_auto_demography",
         "test_politics_domain::colonial_migration_pull_requires_verified_destination"},
        {{"law_colonial_resettlement","state_migration_pull_unincorporated_mult",0.5,false,false},
         "IntegratedWorld::phase_time_auto_demography",
         "test_politics_domain::colonial_migration_pull_requires_verified_destination"},
        {{"amendment_homestead_acts","state_migration_pull_unincorporated_mult",0.25,false,false},
         "IntegratedWorld::phase_time_auto_demography",
         "test_politics_domain::colonial_migration_pull_requires_verified_destination"},
        {{"law_sakoku","country_disallow_aggressive_plays_bool",1,true,false},
         "phasegh::Stage::open_crisis (claimant initiation)",
         "test_phase_gh_diplomacy::sakoku_blocks_only_new_claimant_initiation"},
        {{"amendment_draconian_blasphemy_laws","country_loan_interest_rate_add",0.02,false,false},
         "IntegratedWorld::phase_time_accrue_and_settle_interest",
         "test_p0_daily_longrun::sovereign_bank_interest_law_uses_real_principal"},
        {{"amendment_open_legal_questions","country_loan_interest_rate_add",0.02,false,false},
         "IntegratedWorld::phase_time_accrue_and_settle_interest",
         "test_p0_daily_longrun::sovereign_bank_interest_law_uses_real_principal"},
        {{"amendment_industrial_socialization","country_loan_interest_rate_add",0.03,false,false},
         "IntegratedWorld::phase_time_accrue_and_settle_interest",
         "test_p0_daily_longrun::sovereign_bank_interest_law_uses_real_principal"},
        {{"amendment_shop_councils","country_loan_interest_rate_add",0.01,false,false},
         "IntegratedWorld::phase_time_accrue_and_settle_interest",
         "test_p0_daily_longrun::sovereign_bank_interest_law_uses_real_principal"},
        {{"amendment_foreign_investment_seizures","country_loan_interest_rate_add",0.05,false,false},
         "IntegratedWorld::phase_time_accrue_and_settle_interest",
         "test_p0_daily_longrun::sovereign_bank_interest_law_uses_real_principal"},
        {{"law_chiefs_distribute_aid","country_bureaucracy_add",10,false,true},
         "IntegratedWorld::p1_bureaucracy",
         "test_politics_identity::chiefs_aid_uses_only_effective_welfare_bureaucracy"},
        {{"law_elder_council","country_legitimacy_base_add",20,false,false},
         "IntegratedWorld::p1_legitimacy",
         "test_politics_catalog::enacted_national_laws_change_only_calculated_legitimacy"},
        {{"amendment_cult_of_personality","country_legitimacy_base_add",-5,false,false},
         "IntegratedWorld::p1_legitimacy",
         "test_politics_catalog::enacted_national_laws_change_only_calculated_legitimacy"},
        {{"amendment_tradition_of_free_elections","country_legitimacy_base_add",10,false,false},
         "IntegratedWorld::p1_legitimacy",
         "test_politics_catalog::enacted_national_laws_change_only_calculated_legitimacy"},
        {{"amendment_geheime_staatskonferenz","country_legitimacy_base_add",50,false,false},
         "IntegratedWorld::p1_legitimacy",
         "test_politics_catalog::enacted_national_laws_change_only_calculated_legitimacy"},
        {{"amendment_geheime_staatskonferenz_metternich","country_legitimacy_base_add",50,false,false},
         "IntegratedWorld::p1_legitimacy",
         "test_politics_catalog::enacted_national_laws_change_only_calculated_legitimacy"},
        {{"amendment_geheime_staatskonferenz_kolowrat","country_legitimacy_base_add",50,false,false},
         "IntegratedWorld::p1_legitimacy",
         "test_politics_catalog::enacted_national_laws_change_only_calculated_legitimacy"},
        {{"law_autocracy","country_legitimacy_ideological_incoherence_mult",0.5,false,false},
         "IntegratedWorld::p1_legitimacy",
         "test_politics_catalog::enacted_national_laws_change_only_calculated_legitimacy"},
        {{"law_corporate_state","country_legitimacy_ideological_incoherence_mult",-0.25,false,false},
         "IntegratedWorld::p1_legitimacy",
         "test_politics_catalog::enacted_national_laws_change_only_calculated_legitimacy"},
        {{"law_elder_council","country_legitimacy_ideological_incoherence_mult",-0.1,false,false},
         "IntegratedWorld::p1_legitimacy",
         "test_politics_catalog::enacted_national_laws_change_only_calculated_legitimacy"},
        {{"law_monarchy","country_legitimacy_ideological_incoherence_mult",0.1,false,false},
         "IntegratedWorld::p1_legitimacy",
         "test_politics_catalog::enacted_national_laws_change_only_calculated_legitimacy"},
        {{"law_colonial_exploitation","building_unincorporated_subsistence_output_mult",-0.1,false,false},
         "IntegratedWorld::household_produce (explicit Stage 4 own-account ceiling)",
         "test_phase_c::colonial_subsistence_ceiling"},
        {{"amendment_generic_sponsor_polstr_increase","interest_group_pol_str_mult",0.2,false,false,true},
         "IntegratedWorld::p3_group_view (verified same-country negotiated sponsor)",
         "test_p2_advanced::negotiated_sponsor_uses_actual_same_country_group"},
        {{"amendment_preserved_bureaucratic_caste","interest_group_pol_str_mult",0.15,false,false,true},
         "IntegratedWorld::p3_group_view (same-country negotiated sponsor clout)",
         "test_p2_advanced::remaining_negotiated_sponsor_rows_use_actual_coalition"},
        {{"amendment_generic_sponsor_polstr_approval_increase","interest_group_pol_str_mult",0.2,false,false,true},
         "IntegratedWorld::p3_group_view (same-country negotiated sponsor clout)",
         "test_p2_advanced::remaining_negotiated_sponsor_rows_use_actual_coalition"},
        {{"amendment_generic_sponsor_polstr_approval_increase","interest_group_in_government_approval_add",2,false,false,true},
         "IntegratedWorld::p3_group_view (same-country negotiated sponsor in actual governing coalition)",
         "test_p2_advanced::remaining_negotiated_sponsor_rows_use_actual_coalition"},
        {{"amendment_generic_sponsor_polstr_increase_negotiation_1","interest_group_pol_str_mult",0.2,false,false,true},
         "IntegratedWorld::p3_group_view (same-country negotiated sponsor clout)",
         "test_p2_advanced::remaining_negotiated_sponsor_rows_use_actual_coalition"},
        {{"amendment_generic_sponsor_polstr_increase_negotiation_1","interest_group_in_government_approval_add",1,false,false,true},
         "IntegratedWorld::p3_group_view (same-country negotiated sponsor in actual governing coalition)",
         "test_p2_advanced::remaining_negotiated_sponsor_rows_use_actual_coalition"},
        {{"amendment_generic_sponsor_polstr_increase_negotiation_2","interest_group_pol_str_mult",0.3,false,false,true},
         "IntegratedWorld::p3_group_view (same-country negotiated sponsor clout)",
         "test_p2_advanced::remaining_negotiated_sponsor_rows_use_actual_coalition"},
        {{"amendment_generic_sponsor_polstr_increase_negotiation_2","interest_group_in_government_approval_add",2,false,false,true},
         "IntegratedWorld::p3_group_view (same-country negotiated sponsor in actual governing coalition)",
         "test_p2_advanced::remaining_negotiated_sponsor_rows_use_actual_coalition"},
        {{"amendment_generic_sponsor_polstr_increase_negotiation_3","interest_group_pol_str_mult",0.4,false,false,true},
         "IntegratedWorld::p3_group_view (same-country negotiated sponsor clout)",
         "test_p2_advanced::remaining_negotiated_sponsor_rows_use_actual_coalition"},
        {{"amendment_generic_sponsor_polstr_increase_negotiation_3","interest_group_in_government_approval_add",3,false,false,true},
         "IntegratedWorld::p3_group_view (same-country negotiated sponsor in actual governing coalition)",
         "test_p2_advanced::remaining_negotiated_sponsor_rows_use_actual_coalition"},
        {{"law_child_labor_allowed","state_laborers_mortality_mult",0.05,false,false},
         "IntegratedWorld::phase_time_auto_demography (exact occupation mortality)",
         "test_politics_identity::childrens_rights_changes_only_exact_profession_mortality"},
        {{"law_child_labor_allowed","state_machinists_mortality_mult",0.05,false,false},
         "IntegratedWorld::phase_time_auto_demography (exact occupation mortality)",
         "test_politics_identity::childrens_rights_changes_only_exact_profession_mortality"},
        {{"law_child_labor_allowed","state_farmers_mortality_mult",0.05,false,false},
         "IntegratedWorld::phase_time_auto_demography (exact occupation mortality)",
         "test_politics_identity::childrens_rights_changes_only_exact_profession_mortality"},
        {{"law_child_labor_allowed","state_peasants_mortality_mult",0.05,false,false},
         "IntegratedWorld::phase_time_auto_demography (exact occupation mortality)",
         "test_politics_identity::childrens_rights_changes_only_exact_profession_mortality"},
        {{"law_restricted_child_labor","state_laborers_mortality_mult",0.02,false,false},
         "IntegratedWorld::phase_time_auto_demography (exact occupation mortality)",
         "test_politics_identity::childrens_rights_changes_only_exact_profession_mortality"},
        {{"law_restricted_child_labor","state_farmers_mortality_mult",0.02,false,false},
         "IntegratedWorld::phase_time_auto_demography (exact occupation mortality)",
         "test_politics_identity::childrens_rights_changes_only_exact_profession_mortality"},
        {{"law_restricted_child_labor","state_peasants_mortality_mult",0.02,false,false},
         "IntegratedWorld::phase_time_auto_demography (exact occupation mortality)",
         "test_politics_identity::childrens_rights_changes_only_exact_profession_mortality"},
        {{"law_serfdom","state_peasants_internal_migration_disallowed_bool",1,true,false},
         "IntegratedWorld::p2_migration_permitted (same-sovereign interregional peasant migration)",
         "test_politics_identity::land_reform_restricts_only_peasant_internal_migration"},
        {{"law_tenant_farmers","state_peasants_internal_migration_disallowed_bool",1,true,false},
         "IntegratedWorld::p2_migration_permitted (same-sovereign interregional peasant migration)",
         "test_politics_identity::land_reform_restricts_only_peasant_internal_migration"},
        {{"law_no_colonial_affairs","country_acceptance_homeland_add",10,false,false},
         "IntegratedWorld::p2_acceptance (explicit verified homeland)",
         "test_politics_identity::no_colonial_affairs_adds_only_verified_homeland_acceptance"},
        {{"amendment_reinstated_fueros","country_basque_cultural_acceptance_add",20,false,false},
         "IntegratedWorld::p2_acceptance (exact verified named culture)",
         "test_politics_identity::canonical_named_acceptance_changes_only_verified_pop_and_country"},
        {{"amendment_haiti_polish_naturalization","country_polish_cultural_acceptance_add",60,false,false},
         "IntegratedWorld::p2_acceptance (exact verified named culture)",
         "test_politics_identity::canonical_named_acceptance_changes_only_verified_pop_and_country"},
        {{"amendment_reinstated_fueros","interest_group_ig_landowners_approval_add",1,false,false},
         "IntegratedWorld::p3_group_view (direct named amendment group approval)",
         "test_p2_advanced::direct_named_group_approval_tracks_law_not_the_coalition"},
        {{"amendment_reinstated_fueros","interest_group_ig_rural_folk_approval_add",1,false,false},
         "IntegratedWorld::p3_group_view (direct named amendment group approval)",
         "test_p2_advanced::direct_named_group_approval_tracks_law_not_the_coalition"},
        {{"amendment_frugal_ordinance","interest_group_ig_armed_forces_approval_add",2,false,false},
         "IntegratedWorld::p3_group_view (direct named amendment group approval)",
         "test_p2_advanced::direct_named_group_approval_tracks_law_not_the_coalition"},
        {{"amendment_frugal_ordinance","interest_group_ig_landowners_approval_add",2,false,false},
         "IntegratedWorld::p3_group_view (direct named amendment group approval)",
         "test_p2_advanced::direct_named_group_approval_tracks_law_not_the_coalition"},
        {{"amendment_frugal_ordinance","interest_group_ig_petty_bourgeoisie_approval_add",-2,false,false},
         "IntegratedWorld::p3_group_view (direct named amendment group approval)",
         "test_p2_advanced::direct_named_group_approval_tracks_law_not_the_coalition"},
        {{"amendment_sovereign_of_the_nation","interest_group_ig_devout_approval_add",-1,false,false},
         "IntegratedWorld::p3_group_view (direct named amendment group approval)",
         "test_p2_advanced::direct_named_group_approval_tracks_law_not_the_coalition"},
        {{"amendment_american_fugitive_slaves_act","interest_group_ig_landowners_approval_add",1,false,false},
         "IntegratedWorld::p3_group_view (direct named amendment group approval)",
         "test_p2_advanced::direct_named_group_approval_tracks_law_not_the_coalition"},
        {{"amendment_geheime_staatskonferenz_metternich","country_law_enactment_success_add",-0.05,false,false},
         "IntegratedWorld::p2_review_due (actual staged enactment success)",
         "test_p2_advanced::canonical_enactment_success_changes_real_review_outcome"},
        {{"amendment_geheime_staatskonferenz_kolowrat","country_law_enactment_success_add",-0.15,false,false},
         "IntegratedWorld::p2_review_due (actual staged enactment success)",
         "test_p2_advanced::canonical_enactment_success_changes_real_review_outcome"},
        {{"amendment_geheime_staatskonferenz","country_law_enactment_success_add",-0.1,false,false},
         "IntegratedWorld::p2_review_due (actual staged enactment success)",
         "test_p2_advanced::canonical_enactment_success_changes_real_review_outcome"},
        {{"amendment_tradition_of_free_elections","country_enactment_success_chance_law_autocracy_add",-0.1,false,false},
         "IntegratedWorld::p2_review_due (actual staged enactment success)",
         "test_p2_advanced::canonical_enactment_success_changes_real_review_outcome"},
        {{"amendment_tradition_of_free_elections","country_enactment_success_chance_law_oligarchy_add",-0.1,false,false},
         "IntegratedWorld::p2_review_due (actual staged enactment success)",
         "test_p2_advanced::canonical_enactment_success_changes_real_review_outcome"},
        {{"amendment_tradition_of_free_elections","country_enactment_success_chance_law_technocracy_add",-0.1,false,false},
         "IntegratedWorld::p2_review_due (actual staged enactment success)",
         "test_p2_advanced::canonical_enactment_success_changes_real_review_outcome"},
        {{"amendment_tradition_of_free_elections","country_enactment_success_chance_law_anarchy_add",-0.1,false,false},
         "IntegratedWorld::p2_review_due (actual staged enactment success)",
         "test_p2_advanced::canonical_enactment_success_changes_real_review_outcome"},
        {{"law_shinsengumi","political_movement_radicalism_add",-0.05,false,false},
         "IntegratedWorld::p3_movement_view (flat derived radicalism)",
         "test_p3_p5_politics::shinsengumi_changes_movement_risk_without_altering_pop"},
        {{"amendment_geheime_staatskonferenz_metternich","country_law_enactment_speed_mult",-0.1,false,false},
         "IntegratedWorld::p2_scaled_new_review_days (fixed checkpoint reciprocal ceiling)",
         "test_p2_advanced::austrian_law_speed_changes_only_new_review_intervals"},
        {{"amendment_geheime_staatskonferenz_kolowrat","country_law_enactment_speed_mult",-0.25,false,false},
         "IntegratedWorld::p2_scaled_new_review_days (fixed checkpoint reciprocal ceiling)",
         "test_p2_advanced::austrian_law_speed_changes_only_new_review_intervals"},
        {{"amendment_geheime_staatskonferenz","country_law_enactment_speed_mult",-0.2,false,false},
         "IntegratedWorld::p2_scaled_new_review_days (fixed checkpoint reciprocal ceiling)",
         "test_p2_advanced::austrian_law_speed_changes_only_new_review_intervals"},
        {{"amendment_generic_sponsor_polstr_increase","country_authority_cost_add",50,false,false},
         "IntegratedWorld::p1_authority_capacity (direct continuing law commitment)",
         "test_p2_advanced::political_concessions_commit_real_national_authority"},
        {{"amendment_generic_sponsor_polstr_approval_increase","country_authority_cost_add",50,false,false},
         "IntegratedWorld::p1_authority_capacity (direct continuing law commitment)",
         "test_p2_advanced::political_concessions_commit_real_national_authority"},
        {{"amendment_generic_sponsor_polstr_increase_negotiation_1","country_authority_cost_add",50,false,false},
         "IntegratedWorld::p1_authority_capacity (direct continuing law commitment)",
         "test_p2_advanced::political_concessions_commit_real_national_authority"},
        {{"amendment_generic_sponsor_polstr_increase_negotiation_2","country_authority_cost_add",100,false,false},
         "IntegratedWorld::p1_authority_capacity (direct continuing law commitment)",
         "test_p2_advanced::political_concessions_commit_real_national_authority"},
        {{"amendment_generic_sponsor_polstr_increase_negotiation_3","country_authority_cost_add",150,false,false},
         "IntegratedWorld::p1_authority_capacity (direct continuing law commitment)",
         "test_p2_advanced::political_concessions_commit_real_national_authority"},
        {{"law_elected_bureaucrats","country_bureaucracy_investment_cost_factor_mult",-0.15,false,false},
         "IntegratedWorld::p1_bureaucracy (ongoing institutional commitments)",
         "test_politics_identity::institutional_bureaucracy_prices_ongoing_commitments"},
        {{"amendment_bourgeois_specialists","country_bureaucracy_investment_cost_factor_mult",-0.05,false,false},
         "IntegratedWorld::p1_bureaucracy (ongoing institutional commitments)",
         "test_politics_identity::institutional_bureaucracy_prices_ongoing_commitments"},
        {{"amendment_geheime_staatskonferenz_metternich","interest_group_ig_landowners_pol_str_mult",0.25,false,false},
         "IntegratedWorld::p2_political_strength_ppm -> p3_group_view (direct named amendment strength)",
         "test_p2_advanced::direct_named_amendment_strength_changes_actual_clout"},
        {{"amendment_geheime_staatskonferenz_kolowrat","interest_group_ig_intelligentsia_pol_str_mult",0.5,false,false},
         "IntegratedWorld::p2_political_strength_ppm -> p3_group_view (direct named amendment strength)",
         "test_p2_advanced::direct_named_amendment_strength_changes_actual_clout"},
        {{"amendment_geheime_staatskonferenz_kolowrat","interest_group_ig_armed_forces_pol_str_mult",0.25,false,false},
         "IntegratedWorld::p2_political_strength_ppm -> p3_group_view (direct named amendment strength)",
         "test_p2_advanced::direct_named_amendment_strength_changes_actual_clout"},
        {{"amendment_geheime_staatskonferenz_kolowrat","interest_group_ig_landowners_pol_str_mult",-0.25,false,false},
         "IntegratedWorld::p2_political_strength_ppm -> p3_group_view (direct named amendment strength)",
         "test_p2_advanced::direct_named_amendment_strength_changes_actual_clout"},
        {{"amendment_frugal_ordinance","interest_group_ig_petty_bourgeoisie_pol_str_mult",-0.2,false,false},
         "IntegratedWorld::p2_political_strength_ppm -> p3_group_view (direct named amendment strength)",
         "test_p2_advanced::direct_named_amendment_strength_changes_actual_clout"},
        {{"amendment_corporate_state_nominating_body_tu","interest_group_ig_trade_unions_pol_str_mult",0.15,false,false},
         "IntegratedWorld::p2_political_strength_ppm -> p3_group_view (direct named amendment strength)",
         "test_p2_advanced::direct_named_amendment_strength_changes_actual_clout"},
        {{"amendment_corporate_state_nominating_body_id","interest_group_ig_industrialists_pol_str_mult",0.15,false,false},
         "IntegratedWorld::p2_political_strength_ppm -> p3_group_view (direct named amendment strength)",
         "test_p2_advanced::direct_named_amendment_strength_changes_actual_clout"},
        {{"amendment_corporate_state_nominating_body_it","interest_group_ig_intelligentsia_pol_str_mult",0.15,false,false},
         "IntegratedWorld::p2_political_strength_ppm -> p3_group_view (direct named amendment strength)",
         "test_p2_advanced::direct_named_amendment_strength_changes_actual_clout"},
        {{"amendment_welfare_pro_employer_bias","interest_group_ig_industrialists_pol_str_mult",0.1,false,false},
         "IntegratedWorld::p2_political_strength_ppm -> p3_group_view (direct named amendment strength)",
         "test_p2_advanced::direct_named_amendment_strength_changes_actual_clout"},
        {{"amendment_prussian_education","interest_group_ig_armed_forces_pol_str_mult",0.1,false,false},
         "IntegratedWorld::p2_political_strength_ppm -> p3_group_view (direct named amendment strength)",
         "test_p2_advanced::direct_named_amendment_strength_changes_actual_clout"},
        {{"amendment_taikunate","interest_group_ig_landowners_pol_str_mult",0.1,false,false},
         "IntegratedWorld::p2_political_strength_ppm -> p3_group_view (direct named amendment strength)",
         "test_p2_advanced::direct_named_amendment_strength_changes_actual_clout"},
        {{"amendment_american_fugitive_slaves_act","interest_group_ig_landowners_pol_str_mult",0.1,false,false},
         "IntegratedWorld::p2_political_strength_ppm -> p3_group_view (direct named amendment strength)",
         "test_p2_advanced::direct_named_amendment_strength_changes_actual_clout"},
        {{"amendment_factory_councils","interest_group_ig_trade_unions_pol_str_mult",0.25,false,false},
         "IntegratedWorld::p2_political_strength_ppm -> p3_group_view (direct named amendment strength)",
         "test_p2_advanced::direct_named_amendment_strength_changes_actual_clout"},
        {{"law_latifundias","country_peasants_pol_str_mult",-0.2,false,false},
         "IntegratedWorld::p3_group_view (verified occupation-weighted membership)",
         "test_p3_p5_politics::exact_professions_weight_actual_group_members"},
        {{"law_appointed_bureaucrats","country_bureaucrats_pol_str_mult",0.5,false,false},
         "IntegratedWorld::p3_group_view (verified occupation-weighted membership)",
         "test_p3_p5_politics::exact_professions_weight_actual_group_members"},
        {{"law_communal_administration","country_bureaucrats_pol_str_mult",-0.25,false,false},
         "IntegratedWorld::p3_group_view (verified occupation-weighted membership)",
         "test_p3_p5_politics::exact_professions_weight_actual_group_members"},
        {{"amendment_bourgeois_specialists","country_bureaucrats_pol_str_mult",0.15,false,false},
         "IntegratedWorld::p3_group_view (verified occupation-weighted membership)",
         "test_p3_p5_politics::exact_professions_weight_actual_group_members"},
        {{"law_technocracy","country_academics_pol_str_mult",0.33,false,false},
         "IntegratedWorld::p3_group_view (verified occupation-weighted membership)",
         "test_p3_p5_politics::exact_professions_weight_actual_group_members"},
        {{"law_guild_system","country_shopkeepers_pol_str_mult",0.25,false,false},
         "IntegratedWorld::p3_group_view (verified occupation-weighted membership)",
         "test_p3_p5_politics::exact_professions_weight_actual_group_members"},
        {{"law_latifundias","country_aristocrats_pol_str_mult",0.4,false,false},
         "IntegratedWorld::p3_group_view (verified occupation-weighted membership)",
         "test_p3_p5_politics::exact_professions_weight_actual_group_members"},
        {{"law_oligarchy","country_capitalists_pol_str_mult",0.25,false,false},
         "IntegratedWorld::p3_group_view (verified occupation-weighted membership)",
         "test_p3_p5_politics::exact_professions_weight_actual_group_members"},
        {{"law_elder_council","country_clergymen_pol_str_mult",0.5,false,false},
         "IntegratedWorld::p3_group_view (verified occupation-weighted membership)",
         "test_p3_p5_politics::exact_professions_weight_actual_group_members"},
        {{"law_organic_regulation","country_clerks_pol_str_mult",0.25,false,false},
         "IntegratedWorld::p3_group_view (verified occupation-weighted membership)",
         "test_p3_p5_politics::exact_professions_weight_actual_group_members"},
        {{"law_technocracy","country_engineers_pol_str_mult",0.33,false,false},
         "IntegratedWorld::p3_group_view (verified occupation-weighted membership)",
         "test_p3_p5_politics::exact_professions_weight_actual_group_members"},
        {{"law_homesteading","country_farmers_pol_str_mult",0.25,false,false},
         "IntegratedWorld::p3_group_view (verified occupation-weighted membership)",
         "test_p3_p5_politics::exact_professions_weight_actual_group_members"},
        {{"law_extraordinary_commission","country_soldiers_pol_str_mult",0.15,false,false},
         "IntegratedWorld::p3_group_view (verified occupation-weighted membership)",
         "test_p3_p5_politics::exact_professions_weight_actual_group_members"}
    }};
    return evidence;
}

inline std::vector<CanonicalLawEffectAudit> audit_canonical_law_modifiers() {
    const auto& rows=p2_catalog_115().modifiers;
    const auto& named=wired_law_effects();
    std::vector<CanonicalLawEffectAudit> audit;
    audit.reserve(rows.size());
    for(const auto& row:rows) {
        const auto it=named.find(std::string(row.key));
        audit.push_back(classify_canonical_law_row(row,canonical_law_row_evidence(),
            it==named.end()?std::string_view{}:std::string_view(it->second)));
    }
    return audit;
}

} // namespace vic25::phasec::politics
