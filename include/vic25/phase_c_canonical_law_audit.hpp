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

inline const std::array<CanonicalLawRowEvidence,24>& canonical_law_row_evidence() {
    // Curated source rows covered by the named behavior comparison. Merely
    // adding a key to the old map does not establish proof here.
    static const std::array<CanonicalLawRowEvidence,24> evidence{{
        {{"law_religious_schools","state_education_access_add",0.1,false,true},
         "IntegratedWorld::phase_time_auto_demography",
         "test_politics_identity::enacted_education_changes_real_daily_schooling"},
        {{"law_public_schools","state_education_access_add",0.125,false,true},
         "IntegratedWorld::phase_time_auto_demography",
         "test_politics_identity::enacted_education_changes_real_daily_schooling"},
        {{"law_terakoya","state_education_access_add",0.25,false,false},
         "IntegratedWorld::phase_time_auto_demography",
         "test_politics_identity::enacted_education_changes_real_daily_schooling"},
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
         "test_phase_c::D4-T49"}
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
