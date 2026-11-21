#include "vic25/daily_snapshot.hpp"
#include "vic25/phase_c_politics_catalog.hpp"
#include "vic25/phase_c_politics_effect_audit.hpp"
#include "vic25/phase_c_canonical_law_audit.hpp"
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace vic25;
using namespace vic25::phasec;

static void check(bool ok,const char* why) {
    if(!ok)throw std::runtime_error(why);
}
template<class F>static bool rejects(F&& f) {
    try{f();}catch(const std::exception&){return true;}
    return false;
}
static void law(IntegratedWorld& c,const std::string& domain,
                const std::string& choice) {
    P2BaseLaw l;
    l.id="test:"+domain;l.country="A";l.domain="lawgroup_"+domain;
    l.scope="national";l.base_choice="law_"+choice;
    l.legal_basis="test:constitution";
    c.p2_seed_base(l);
}
static phasegh::World make_world(const std::string& economy_choice) {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.set_dataset("political-catalogue-regression");
    geo.seed_province(1,"R","A");
    geo.seed_unit(10,"A","ADMIN");
    geo.seed_assignment(1,10);
    auto& c=geo.seed_core().seed_core();
    c.seed_actor("A",1000);
    c.seed_actor("CLINIC",1000);
    c.seed_government("A","HEAD","PRIME",5,3);
    c.seed_p1_constitution("A",{
        P1GovernancePrinciple::parliamentary_republic,
        P1PowerDistribution::universal_vote,
        P1LegislatureKind::unicameral,"test:constitution"});
    c.seed_interest_group("WORKERS");
    c.seed_interest_group("BUSINESS");
    c.seed_p1_coalition("A",{{"WORKERS","BUSINESS"},{},false});
    auto& pop=c.seed_population();
    const auto hh=pop.household_profile({"R","mixed","shared",100});
    const auto work=pop.economic_profile({
        "R","worker","wage","employee",pop.default_education()});
    const auto dem=pop.demographic_profile({
        "R",24000-350,phaseb::Sex::female,"culture","faith","citizen",
        "free",pop.default_health(),phaseb::Service::civilian});
    const auto people=pop.seed_population(dem,work,hh,100,1);
    pop.hire(people,5,"CLINIC","worker",5,{});
    c.seed_person_anchor("LEADER",people);
    c.p3_seed_group({"A","WORKERS","LEADER","","",
                     P3GroupType::trade_unions,850'000,0});
    c.p3_seed_group({"A","BUSINESS","","","",
                     P3GroupType::industrialists,850'000,0});
    c.p3_set_ideologies("A","WORKERS",{"socialist","egalitarian"});
    c.p3_set_leader_ideology("A","WORKERS","communist");
    law(c,"economic_system",economy_choice);
    law(c,"rights_of_women","no_womens_rights");
    law(c,"labor_associations","anti_strike_laws");
    law(c,"health_system","no_health_system");
    law(c,"free_speech","protected_speech");
    law(c,"taxation","consumption_based_taxation");
    law(c,"welfare","no_social_security");
    law(c,"distribution_of_power","universal_suffrage");
    c.seed_public_institution("HOSPITAL","CLINIC","R","medicine",100,3);
    c.seed_p1_institution("A","HOSPITAL",
                          P1InstitutionKind::healthcare,5);
    gh.validate();
    return gh;
}
static void catalogued_laws_and_stances() {
    const auto& c=politics::catalogue();
    check(c.by_group.size()==26&&c.laws.size()==148,
          "1.15-inspired catalogue missing law families");
    // Source completeness is not 1.15 parity: the imported reference is
    // 1.13.11 (172 ideology definitions, 136 unconditional law modifiers).
    // The extra 18 newer 1.15 ideologies and conditional effects are not
    // fabricated by assigning arbitrary default opinions.
    std::size_t stanceCount=0,modifierCount=0;
    for(const auto& [id,stances]:c.ideologies) {
        (void)id; stanceCount+=stances.size();
    }
    for(const auto& [id,mods]:c.modifiers) {
        (void)id; modifierCount+=mods.size();
    }
    check(c.ideologies.size()==190&&c.modifiers.size()==148&&
          stanceCount==4469&&modifierCount==812,
          "1.15 full ideology or unconditional law modifier dataset incomplete");
    check(c.knows_ideology("modernizer")&&c.knows_ideology("autocratic")&&
          c.knows_ideology("tsarist")&&
          c.stance("socialist","economic_system","cooperative_ownership").has_value(),
          "1.15 sourced ideology or backwards-compatible ideology migration missing");
    check(c.modifier("serfdom",
          "state_peasants_internal_migration_disallowed_bool").has_value()&&
          c.modifier("mass_conscription",
          "state_conscription_rate_add").has_value()&&
          c.modifier("cooperative_ownership",
          "building_group_bg_manufacturing_allowed_collectivization_add").has_value(),
          "source-qualified economic, social and military modifiers missing");
    check(c.has("lawgroup_economic_system","law_cooperative_ownership")&&
          c.has("rights_of_women","womens_suffrage")&&
          c.has("labor_associations","factory_councils")&&
          c.has("navy_model","capital_fleet")&&
          c.has("church_and_state","caesaropapism")&&
          c.has("citizenship","official_nationality")&&
          c.has("colonial_affairs","internal_colonization")&&
          !c.has("economic_system","womens_suffrage"),
          "catalogue group membership incorrect");
    check(c.stance("socialist","economic_system","cooperative_ownership")
             .value_or(0)>0&&
          c.stance("laissez_faire","economic_system","cooperative_ownership")
             .value_or(0)<0 &&
          c.stance("militarist","policing","militarized_police")
             .value_or(0)>0,
          "5-step ideology positions missing");
    check(politics::law_clause("labor_associations","anti_strike_laws",
              P2ClauseKind::political_association_allowed)==1&&
          politics::law_clause("labor_associations","anti_strike_laws",
              P2ClauseKind::peaceful_strike_allowed)==0,
          "anti-strike law must distinguish union association from strikes");
    const auto coverage=politics::audit_ig_traits();
    std::set<std::pair<std::string,std::string>> expected;
    std::size_t expected_supported=0,expected_unsupported=0;
    const auto& routes=politics::wired_trait_effects();
    for(const auto& rule:politics::ig_trait_rules()) {
        std::istringstream input(rule.encoded);
        for(std::string token;input>>token;) {
            const auto colon=token.rfind(':');
            check(colon!=std::string::npos,"invalid canonical trait modifier");
            const auto key=token.substr(0,colon);
            check(expected.emplace(rule.id,key).second,
                  "duplicate canonical trait modifier");
            if(routes.contains(key))++expected_supported;
            else ++expected_unsupported;
        }
    }
    std::set<std::pair<std::string,std::string>> emitted;
    std::size_t connected=0,unsupported=0;
    for(const auto& entry:coverage) {
        check(emitted.emplace(entry.trait,entry.modifier).second,
              "duplicate trait modifier audit row");
        const auto route=routes.find(entry.modifier);
        if(entry.wired) {
            ++connected;
            check(route!=routes.end() && !entry.executor.empty() &&
                  entry.executor==route->second,
                  "wired trait has no matching native executor");
        } else {
            ++unsupported;
            check(route==routes.end() && entry.executor.empty(),
                  "unsupported trait pretends to execute");
        }
    }
    check(connected>0 && connected==expected_supported &&
          unsupported==expected_unsupported && emitted==expected,
          "trait audit must exactly cover canonical trait modifiers");
    const auto discontent=politics::active_ig_effects(
        P3GroupType::trade_unions,30,100);
    const auto stable=politics::active_ig_effects(
        P3GroupType::trade_unions,60,100);
    const auto empty=politics::active_ig_effects(
        P3GroupType::trade_unions,30,0);
    check(std::any_of(discontent.begin(),discontent.end(),
            [](const auto& effect) {
                return effect.modifier=="state_construction_mult"&&
                       effect.value_ppm==-100'000;
            })&&
          std::none_of(stable.begin(),stable.end(),
            [](const auto& effect) {
                return effect.modifier=="state_construction_mult";
            })&&empty.empty(),
          "unhappy trade unions must affect real construction only with members");
    check(politics::default_ideologies(P3GroupType::trade_unions).size()>=2&&
          politics::default_ideologies(P3GroupType::custom).empty(),
          "non-historical custom IG must not be assigned invented ideology");
}
static void trait_only_hostility_does_not_claim_law_coverage() {
    const auto traits=politics::audit_ig_traits();
    for(const std::string key:{
            "state_radicalism_increases_violent_hostility_mult",
            "state_radicalism_increases_cultural_erasure_mult",
            "state_radicalism_increases_open_prejudice_mult"}) {
        check(std::any_of(p2_catalog_115().modifiers.begin(),
                          p2_catalog_115().modifiers.end(),
            [&](const auto& row) {
                return row.source_id=="law_militarized_police" &&
                       row.key==key && row.institution;
            }),"militarized-police institution modifier missing from source metadata");
        check(std::any_of(traits.begin(),traits.end(),[&](const auto& row) {
                return row.trait=="ig_trait_xenophobia" && row.modifier==key &&
                       row.wired && !row.executor.empty();
            }),"xenophobia trait lost its native hostility route");
        check(!politics::wired_law_effects().contains(key),
              "trait-only hostility route was reported as law execution");
    }
}

static void canonical_effect_audit_retains_scope_and_evidence() {
    using namespace politics;
    const auto audit=audit_canonical_law_modifiers();
    const auto& canonical=p2_catalog_115().modifiers;
    std::set<std::string_view> keys;
    check(audit.size()==canonical.size()&&audit.size()==1117,
          "canonical audit omitted or duplicated a source row");
    for(std::size_t i=0;i<audit.size();++i) {
        check(same_canonical_law_row(audit[i].source,canonical[i]),
              "canonical audit did not preserve exact row metadata");
        keys.insert(audit[i].source.key);
    }
    check(keys.size()==350,"canonical audit key inventory changed");
    for(const auto& proof:canonical_law_row_evidence())
        check(std::count_if(audit.begin(),audit.end(),[&](const auto& entry) {
            return same_canonical_law_row(entry.source,proof.row) &&
                   entry.status==CanonicalLawRowStatus::verified_row &&
                   entry.executor==proof.executor &&
                   entry.behavior_test==proof.behavior_test;
        })==1,"curated behavior evidence failed to match exactly one canonical row");

    std::size_t flat_migration=0,unrecorded_institution=0;
    for(const auto& entry:audit) {
        if(entry.source.key!="state_migration_pull_unincorporated_mult")continue;
        if(!entry.source.institution &&
           entry.status==CanonicalLawRowStatus::verified_row)++flat_migration;
        if(entry.source.source_id=="law_internal_colonization" &&
           entry.source.institution &&
           entry.status==CanonicalLawRowStatus::no_recorded_path)
            ++unrecorded_institution;
    }
    check(flat_migration==3&&unrecorded_institution==1,
          "flat colonial migration evidence leaked to the institution row");

    std::size_t sovereign_interest=0,chiefs_aid=0;
    for(const auto& entry:audit) {
        if(entry.status!=CanonicalLawRowStatus::verified_row)continue;
        if(entry.source.key=="country_loan_interest_rate_add" &&
           !entry.source.institution && !entry.source.sponsor && !entry.source.boolean &&
           entry.executor=="IntegratedWorld::phase_time_accrue_and_settle_interest" &&
           entry.behavior_test=="test_p0_daily_longrun::sovereign_bank_interest_law_uses_real_principal")
            ++sovereign_interest;
        if(entry.source.source_id=="law_chiefs_distribute_aid" &&
           entry.source.key=="country_bureaucracy_add" &&
           entry.source.institution && !entry.source.sponsor && !entry.source.boolean &&
           entry.normalized_value==10'000'000 &&
           entry.executor=="IntegratedWorld::p1_bureaucracy" &&
           entry.behavior_test=="test_politics_identity::chiefs_aid_uses_only_effective_welfare_bureaucracy")
            ++chiefs_aid;
    }
    check(sovereign_interest==5&&chiefs_aid==1,
          "scoped sovereign interest and welfare evidence missing");

    std::size_t legitimacy_base=0,legitimacy_incoherence=0;
    for(const auto& entry:audit) {
        if(!entry.source.key.starts_with("country_legitimacy_") ||
           entry.status!=CanonicalLawRowStatus::verified_row)continue;
        check(!entry.source.boolean && !entry.source.institution && !entry.source.sponsor &&
              entry.executor=="IntegratedWorld::p1_legitimacy" &&
              entry.behavior_test=="test_politics_catalog::enacted_national_laws_change_only_calculated_legitimacy",
              "legitimacy proof has the wrong scope or executor");
        if(entry.source.key=="country_legitimacy_base_add")++legitimacy_base;
        if(entry.source.key=="country_legitimacy_ideological_incoherence_mult")++legitimacy_incoherence;
        check(entry.source.key=="country_legitimacy_base_add" ||
              entry.source.key=="country_legitimacy_ideological_incoherence_mult",
              "legitimacy proof leaked to an unverified modifier");
    }
    check(legitimacy_base==6 && legitimacy_incoherence==4,
          "verified national legitimacy rows missing");

    check(std::count_if(audit.begin(),audit.end(),[](const auto& entry) {
        return entry.source.source_id=="law_colonial_exploitation" &&
               entry.source.key=="building_unincorporated_subsistence_output_mult" &&
               !entry.source.boolean && !entry.source.institution && !entry.source.sponsor &&
               entry.normalized_value==-100'000 &&
               entry.status==CanonicalLawRowStatus::verified_row &&
               entry.executor=="IntegratedWorld::household_produce (explicit Stage 4 own-account ceiling)" &&
               entry.behavior_test=="test_phase_c::colonial_subsistence_ceiling";
    })==1,"explicit Stage 4 subsistence proof missing");

    check(std::count_if(audit.begin(),audit.end(),[](const auto& entry) {
        return entry.source.source_id=="amendment_generic_sponsor_polstr_increase" &&
               entry.source.key=="interest_group_pol_str_mult" &&
               !entry.source.boolean && !entry.source.institution && entry.source.sponsor &&
               entry.normalized_value==200'000 &&
               entry.status==CanonicalLawRowStatus::verified_row &&
               entry.executor=="IntegratedWorld::p3_group_view (verified same-country negotiated sponsor)" &&
               entry.behavior_test=="test_p2_advanced::negotiated_sponsor_uses_actual_same_country_group";
    })==1,"negotiated sponsor-specific clout proof missing");
    std::size_t negotiated_strength=0,negotiated_approval=0;
    for(const auto& row:audit) {
        if(row.behavior_test!="test_p2_advanced::remaining_negotiated_sponsor_rows_use_actual_coalition")
            continue;
        check(row.source.sponsor && !row.source.boolean && !row.source.institution &&
              row.status==CanonicalLawRowStatus::verified_row,
              "negotiated witness lost the exact sponsor qualification");
        if(row.source.key=="interest_group_pol_str_mult") {
            check(row.executor=="IntegratedWorld::p3_group_view (same-country negotiated sponsor clout)",
                  "negotiated political strength witness lost exact executor scope");
            ++negotiated_strength;
        } else {
            check(row.source.key=="interest_group_in_government_approval_add" &&
                  row.executor=="IntegratedWorld::p3_group_view (same-country negotiated sponsor in actual governing coalition)",
                  "government approval witness lost actual coalition scope");
            ++negotiated_approval;
        }
        auto mismatch=row.source;
        mismatch.sponsor=false;
        check(classify_canonical_law_row(mismatch,canonical_law_row_evidence()).status==
                  CanonicalLawRowStatus::no_recorded_path,
              "proof leaked from a sponsored effect to a non-sponsored row");
    }
    check(negotiated_strength==5 && negotiated_approval==4,
          "missing precise political strength or approval sponsor evidence");

    std::size_t allowed_risk=0,restricted_risk=0;
    constexpr std::string_view mortality_fixture=
        "test_politics_identity::childrens_rights_changes_only_exact_profession_mortality";
    for(const auto& row:audit) {
        if(row.behavior_test!=mortality_fixture)continue;
        check(!row.source.boolean && !row.source.institution && !row.source.sponsor &&
              row.executor=="IntegratedWorld::phase_time_auto_demography (exact occupation mortality)" &&
              row.status==CanonicalLawRowStatus::verified_row,
              "occupation mortality witness has the wrong flat scope or executor");
        if(row.source.source_id=="law_child_labor_allowed" && row.normalized_value==50'000)
            ++allowed_risk;
        if(row.source.source_id=="law_restricted_child_labor" && row.normalized_value==20'000)
            ++restricted_risk;
    }
    check(allowed_risk==4 && restricted_risk==3,
          "exact occupation mortality witness count differs by law");
    check(classify_canonical_law_row({"law_restricted_child_labor",
              "state_machinists_mortality_mult",0.02,false,false},
              canonical_law_row_evidence()).status==CanonicalLawRowStatus::no_recorded_path &&
          classify_canonical_law_row({"law_child_labor_allowed",
              "state_workers_mortality_mult",0.05,false,false},
              canonical_law_row_evidence()).status==CanonicalLawRowStatus::no_recorded_path,
          "named law evidence leaked to an unwitnessed occupation");

    std::size_t direct_named_approval=0;
    for(const auto& row:audit) {
        if(row.behavior_test!="test_p2_advanced::direct_named_group_approval_tracks_law_not_the_coalition")continue;
        check(!row.source.boolean && !row.source.institution && !row.source.sponsor &&
              row.executor=="IntegratedWorld::p3_group_view (direct named amendment group approval)" &&
              row.status==CanonicalLawRowStatus::verified_row,
              "direct named group approval witness has the wrong flat scope");
        auto mismatch=row.source;
        mismatch.sponsor=true;
        check(classify_canonical_law_row(mismatch,canonical_law_row_evidence()).status==
                  CanonicalLawRowStatus::no_recorded_path,
              "direct named group approval proof leaked to a sponsored row");
        ++direct_named_approval;
    }
    check(direct_named_approval==7,
          "exact evidence did not preserve all direct named witnesses");
    check(canonical_law_row_evidence().size()==131,
          "expected 129 preserved witnesses and two peasant-only draft rows");
    std::size_t serfdom_internal=0,tenant_internal=0,serfdom_mass=0;
    for(const auto& row:audit) {
        if(row.source.key=="state_peasants_mass_migration_disallowed_bool") {
            check(row.status!=CanonicalLawRowStatus::verified_row&&
                  row.behavior_test.empty(),
                  "mass migration inherited internal migration evidence");
            if(row.source.source_id=="law_serfdom") {
                check(row.source.value==1&&row.source.boolean&&
                      row.normalized_value==1,
                      "unwired mass migration lost its original boolean value");
                ++serfdom_mass;
            }
        }
        if(row.source.key!="state_peasants_internal_migration_disallowed_bool")continue;
        check(row.source.value==1&&row.source.boolean&&!row.source.institution&&
              !row.source.sponsor&&row.normalized_value==1&&
              row.status==CanonicalLawRowStatus::verified_row&&
              row.executor=="IntegratedWorld::p2_migration_permitted (same-sovereign interregional peasant migration)"&&
              row.behavior_test=="test_politics_identity::land_reform_restricts_only_peasant_internal_migration",
              "internal migration evidence lost its exact boolean and flat scope");
        if(row.source.source_id=="law_serfdom")++serfdom_internal;
        if(row.source.source_id=="law_tenant_farmers")++tenant_internal;
        auto qualifier=row.source;
        qualifier.institution=true;
        check(classify_canonical_law_row(qualifier,canonical_law_row_evidence()).status!=
                  CanonicalLawRowStatus::verified_row,
              "flat internal migration proof leaked to an institutional row");
        qualifier.institution=false;
        qualifier.sponsor=true;
        check(classify_canonical_law_row(qualifier,canonical_law_row_evidence()).status!=
                  CanonicalLawRowStatus::verified_row,
              "flat internal migration proof leaked to a sponsor row");
    }
    check(serfdom_internal==1&&tenant_internal==1&&serfdom_mass==1,
          "exact sources for internal and mass migration are not individually auditable");

    std::size_t homeland=0,other_homeland_rows=0;
    for(const auto& row:audit) {
        if(row.source.key!="country_acceptance_homeland_add")continue;
        if(row.source.source_id!="law_no_colonial_affairs") {
            check(row.status!=CanonicalLawRowStatus::verified_row,
                  "different same-key source inherited the colonial homeland witness");
            ++other_homeland_rows;
            continue;
        }
        check(!row.source.boolean&&!row.source.institution&&!row.source.sponsor&&
              row.normalized_value==10'000'000&&
              row.status==CanonicalLawRowStatus::verified_row&&
              row.executor=="IntegratedWorld::p2_acceptance (explicit verified homeland)"&&
              row.behavior_test=="test_politics_identity::no_colonial_affairs_adds_only_verified_homeland_acceptance",
              "homeland evidence lost exact source, value, actor or scope");
        auto qualifier=row.source;
        qualifier.institution=true;
        check(classify_canonical_law_row(qualifier,canonical_law_row_evidence()).status!=
                  CanonicalLawRowStatus::verified_row,
              "flat homeland proof leaked to an institutional row");
        qualifier.institution=false;
        qualifier.sponsor=true;
        check(classify_canonical_law_row(qualifier,canonical_law_row_evidence()).status!=
                  CanonicalLawRowStatus::verified_row,
              "flat homeland proof leaked to a sponsor row");
        ++homeland;
    }
    check(homeland==1&&other_homeland_rows==2,
          "homeland audit did not distinguish all three canonical sources");

    constexpr std::string_view named_culture_fixture=
        "test_politics_identity::canonical_named_acceptance_changes_only_verified_pop_and_country";
    std::size_t basque=0,polish=0;
    for(const auto& row:audit) {
        if(row.behavior_test!=named_culture_fixture)continue;
        check(!row.source.boolean&&!row.source.institution&&!row.source.sponsor&&
              row.executor=="IntegratedWorld::p2_acceptance (exact verified named culture)"&&
              row.status==CanonicalLawRowStatus::verified_row,
              "named culture evidence has an incorrect scope or executor");
        if(row.source.source_id=="amendment_reinstated_fueros"&&
           row.source.key=="country_basque_cultural_acceptance_add"&&
           row.normalized_value==20'000'000)++basque;
        else if(row.source.source_id=="amendment_haiti_polish_naturalization"&&
                row.source.key=="country_polish_cultural_acceptance_add"&&
                row.normalized_value==60'000'000)++polish;
        else check(false,"another culture or source inherited the exact witness");
        auto wrong_scope=row.source;
        wrong_scope.institution=true;
        check(classify_canonical_law_row(wrong_scope,canonical_law_row_evidence()).status==
                  CanonicalLawRowStatus::no_recorded_path,
              "an institutional row inherited the flat culture witness");
    }
    check(basque==1&&polish==1,"missing exact Basque or Polish culture evidence");

    constexpr std::string_view success_fixture=
        "test_p2_advanced::canonical_enactment_success_changes_real_review_outcome";
    std::size_t national_success=0,target_success=0;
    for(const auto& row:audit) {
        if(row.behavior_test!=success_fixture)continue;
        check(!row.source.boolean&&!row.source.institution&&!row.source.sponsor&&
              row.executor=="IntegratedWorld::p2_review_due (actual staged enactment success)"&&
              row.status==CanonicalLawRowStatus::verified_row,
              "enactment success evidence has the wrong scope or executor");
        if(row.source.key=="country_law_enactment_success_add") {
            check((row.source.source_id=="amendment_geheime_staatskonferenz_metternich"&&row.normalized_value==-50'000)||
                  (row.source.source_id=="amendment_geheime_staatskonferenz_kolowrat"&&row.normalized_value==-150'000)||
                  (row.source.source_id=="amendment_geheime_staatskonferenz"&&row.normalized_value==-100'000),
                  "wrong national staged success magnitude");
            ++national_success;
        } else {
            check(row.source.source_id=="amendment_tradition_of_free_elections"&&
                  row.normalized_value==-100'000,
                  "wrong source/magnitude inherited proposal success proof");
            ++target_success;
        }
    }
    check(national_success==3&&target_success==4,
          "missing precise national or proposal-specific enactment rows");
    const auto mismatched_speed=classify_canonical_law_row(
        {"amendment_geheime_staatskonferenz","country_law_enactment_speed_mult",-0.2,false,true},
        canonical_law_row_evidence());
    check(mismatched_speed.status==CanonicalLawRowStatus::no_recorded_path,
          "flat cadence evidence leaked to an institution-qualified row");

    check(std::count_if(audit.begin(),audit.end(),[](const auto& row) {
        return row.source.source_id=="law_shinsengumi" &&
               row.source.key=="political_movement_radicalism_add" &&
               !row.source.boolean && !row.source.institution && !row.source.sponsor &&
               row.normalized_value==-50'000 &&
               row.status==CanonicalLawRowStatus::verified_row &&
               row.executor=="IntegratedWorld::p3_movement_view (flat derived radicalism)" &&
               row.behavior_test=="test_p3_p5_politics::shinsengumi_changes_movement_risk_without_altering_pop";
    })==1 && std::none_of(audit.begin(),audit.end(),[](const auto& row) {
        return row.source.key=="political_movement_radicalism_add" && row.source.institution &&
               row.status==CanonicalLawRowStatus::verified_row;
    }),"flat movement law proof missing or leaked to institutional variants");

    std::size_t austrian_speed=0,staged_review_success=0;
    for(const auto& row:audit) {
        if(row.behavior_test=="test_p2_advanced::canonical_enactment_success_changes_real_review_outcome") {
            check(row.source.key!="country_law_enactment_speed_mult",
                  "review success proof leaked to cadence");
            ++staged_review_success;
        }
        if(row.behavior_test!="test_p2_advanced::austrian_law_speed_changes_only_new_review_intervals")continue;
        check(row.source.key=="country_law_enactment_speed_mult" &&
              !row.source.boolean && !row.source.institution && !row.source.sponsor &&
              row.executor=="IntegratedWorld::p2_scaled_new_review_days (fixed checkpoint reciprocal ceiling)" &&
              row.status==CanonicalLawRowStatus::verified_row,
              "Austrian review speed witness lost precise scope");
        ++austrian_speed;
    }
    check(austrian_speed==3 && staged_review_success==7,
          "expected three review cadence witnesses separate from seven success witnesses");

    constexpr std::string_view authority_cost_fixture=
        "test_p2_advanced::political_concessions_commit_real_national_authority";
    const std::map<std::string_view,std::int64_t> authority_costs={
        {"amendment_generic_sponsor_polstr_increase",50'000'000},
        {"amendment_generic_sponsor_polstr_approval_increase",50'000'000},
        {"amendment_generic_sponsor_polstr_increase_negotiation_1",50'000'000},
        {"amendment_generic_sponsor_polstr_increase_negotiation_2",100'000'000},
        {"amendment_generic_sponsor_polstr_increase_negotiation_3",150'000'000}};
    std::set<std::string_view> authority_proven;
    for(const auto& row:audit) {
        if(row.behavior_test!=authority_cost_fixture)continue;
        check(row.source.key=="country_authority_cost_add"&&
              !row.source.boolean&&!row.source.institution&&!row.source.sponsor&&
              row.status==CanonicalLawRowStatus::verified_row&&
              row.executor=="IntegratedWorld::p1_authority_capacity (direct continuing law commitment)",
              "authority cost evidence must be flat and direct, not a sponsor benefit");
        const auto expected=authority_costs.find(row.source.source_id);
        check(expected!=authority_costs.end()&&expected->second==row.normalized_value&&
              authority_proven.insert(row.source.source_id).second,
              "wrong source or magnitude inherited continuing authority cost proof");
        auto wrong=row.source;
        wrong.sponsor=true;
        check(classify_canonical_law_row(wrong,canonical_law_row_evidence()).status==
                  CanonicalLawRowStatus::no_recorded_path,
              "flat authority cost proof leaked into a sponsor row");
    }
    check(authority_proven.size()==5,"missing exact direct continuing authority commitments");

    std::size_t institution_commitments=0;
    for(const auto& row:audit) {
        if(row.behavior_test!="test_politics_identity::institutional_bureaucracy_prices_ongoing_commitments")
            continue;
        check(row.source.key=="country_bureaucracy_investment_cost_factor_mult" &&
              !row.source.boolean && !row.source.institution && !row.source.sponsor &&
              row.executor=="IntegratedWorld::p1_bureaucracy (ongoing institutional commitments)" &&
              row.status==CanonicalLawRowStatus::verified_row &&
              ((row.source.source_id=="law_elected_bureaucrats" && row.normalized_value==-150'000) ||
               (row.source.source_id=="amendment_bourgeois_specialists" && row.normalized_value==-50'000)),
              "general institution cost witness lost exact row or scope");
        auto mismatch=row.source;
        mismatch.institution=true;
        check(classify_canonical_law_row(mismatch,canonical_law_row_evidence()).status==
                  CanonicalLawRowStatus::no_recorded_path,
              "flat institution cost evidence leaked to a per-level row");
        ++institution_commitments;
    }
    check(institution_commitments==2,"expected two exact bureaucratic institution cost witnesses");

    constexpr std::string_view direct_strength_fixture=
        "test_p2_advanced::direct_named_amendment_strength_changes_actual_clout";
    const std::map<std::pair<std::string_view,std::string_view>,std::int64_t>
        typed_strengths={
        {{"amendment_geheime_staatskonferenz_metternich","interest_group_ig_landowners_pol_str_mult"},250'000},
        {{"amendment_geheime_staatskonferenz_kolowrat","interest_group_ig_intelligentsia_pol_str_mult"},500'000},
        {{"amendment_geheime_staatskonferenz_kolowrat","interest_group_ig_armed_forces_pol_str_mult"},250'000},
        {{"amendment_geheime_staatskonferenz_kolowrat","interest_group_ig_landowners_pol_str_mult"},-250'000},
        {{"amendment_frugal_ordinance","interest_group_ig_petty_bourgeoisie_pol_str_mult"},-200'000},
        {{"amendment_corporate_state_nominating_body_tu","interest_group_ig_trade_unions_pol_str_mult"},150'000},
        {{"amendment_corporate_state_nominating_body_id","interest_group_ig_industrialists_pol_str_mult"},150'000},
        {{"amendment_corporate_state_nominating_body_it","interest_group_ig_intelligentsia_pol_str_mult"},150'000},
        {{"amendment_welfare_pro_employer_bias","interest_group_ig_industrialists_pol_str_mult"},100'000},
        {{"amendment_prussian_education","interest_group_ig_armed_forces_pol_str_mult"},100'000},
        {{"amendment_taikunate","interest_group_ig_landowners_pol_str_mult"},100'000},
        {{"amendment_american_fugitive_slaves_act","interest_group_ig_landowners_pol_str_mult"},100'000},
        {{"amendment_factory_councils","interest_group_ig_trade_unions_pol_str_mult"},250'000}};
    std::set<std::pair<std::string_view,std::string_view>> typed_strength_proven;
    for(const auto& row:audit) {
        if(row.behavior_test!=direct_strength_fixture)continue;
        const auto pair=std::make_pair(row.source.source_id,row.source.key);
        const auto expected=typed_strengths.find(pair);
        check(!row.source.boolean&&!row.source.institution&&!row.source.sponsor&&
              row.status==CanonicalLawRowStatus::verified_row&&
              row.executor=="IntegratedWorld::p2_political_strength_ppm -> p3_group_view (direct named amendment strength)"&&
              expected!=typed_strengths.end()&&expected->second==row.normalized_value&&
              typed_strength_proven.insert(pair).second,
              "direct typed strength evidence lost the exact row or executor");
        for(int alternative=0;alternative<3;++alternative) {
            auto mismatch=row.source;
            if(alternative==0)mismatch.sponsor=true;
            if(alternative==1)mismatch.institution=true;
            if(alternative==2)mismatch.key="country_bureaucrats_pol_str_mult";
            check(classify_canonical_law_row(mismatch,canonical_law_row_evidence()).status
                      !=CanonicalLawRowStatus::verified_row,
                  "direct typed strength leaked to sponsorship, institution, or profession");
        }
    }
    check(typed_strength_proven.size()==typed_strengths.size(),
          "missing exact direct typed strength witness");

    std::size_t exact_profession_strength=0;
    for(const auto& row:audit) {
        if(row.behavior_test!="test_p3_p5_politics::exact_professions_weight_actual_group_members")continue;
        check(row.source.key.starts_with("country_") && row.source.key.ends_with("_pol_str_mult") &&
              !row.source.boolean && !row.source.institution && !row.source.sponsor &&
              row.executor=="IntegratedWorld::p3_group_view (verified occupation-weighted membership)" &&
              row.status==CanonicalLawRowStatus::verified_row,
              "occupation witness mixed with a direct interest-group or qualified modifier");
        auto mismatch=row.source;
        mismatch.institution=true;
        check(classify_canonical_law_row(mismatch,canonical_law_row_evidence()).status==
                  CanonicalLawRowStatus::no_recorded_path,
              "flat occupation strength witness leaked to an institution-qualified row");
        ++exact_profession_strength;
    }
    check(exact_profession_strength==13 &&
          classify_canonical_law_row({"law_shinsengumi","country_officers_pol_str_mult",0.1,false,false},
              canonical_law_row_evidence()).status==CanonicalLawRowStatus::no_recorded_path,
          "occupation coverage missed exact receipts or claimed unverified officers");

    constexpr std::string_view base_strength_fixture=
        "test_p2_advanced::national_base_group_strength_requires_genuine_law_slot";
    const std::map<std::pair<std::string_view,std::string_view>,std::int64_t>
        base_strengths={
        {{"law_hindu_caste_enforced","interest_group_ig_landowners_pol_str_mult"},250'000},
        {{"law_hindu_caste_codified","interest_group_ig_landowners_pol_str_mult"},150'000},
        {{"law_state_religion","interest_group_ig_devout_pol_str_mult"},300'000},
        {{"law_caesaropapism","interest_group_ig_devout_pol_str_mult"},150'000},
        {{"law_millet_system","interest_group_ig_devout_pol_str_mult"},300'000},
        {{"law_people_of_the_book","interest_group_ig_devout_pol_str_mult"},300'000},
        {{"law_freedom_of_conscience","interest_group_ig_devout_pol_str_mult"},150'000},
        {{"law_state_atheism","interest_group_ig_devout_pol_str_mult"},-500'000},
        {{"law_theocracy","interest_group_ig_devout_pol_str_mult"},250'000},
        {{"law_council_republic","interest_group_ig_trade_unions_pol_str_mult"},250'000},
        {{"law_corporate_state","interest_group_ig_petty_bourgeoisie_pol_str_mult"},250'000},
        {{"law_social_monarchy","interest_group_ig_petty_bourgeoisie_pol_str_mult"},250'000},
        {{"law_industry_banned","interest_group_ig_rural_folk_pol_str_mult"},500'000},
        {{"law_serfdom","interest_group_ig_landowners_pol_str_mult"},250'000},
        {{"law_manorialism","interest_group_ig_landowners_pol_str_mult"},150'000},
        {{"law_tenant_farmers","interest_group_ig_landowners_pol_str_mult"},250'000},
        {{"law_combination_acts","interest_group_ig_trade_unions_pol_str_mult"},-250'000},
        {{"law_anti_strike_laws","interest_group_ig_trade_unions_pol_str_mult"},-100'000},
        {{"law_right_to_associate","interest_group_ig_trade_unions_pol_str_mult"},100'000},
        {{"law_factory_councils","interest_group_ig_trade_unions_pol_str_mult"},250'000},
        {{"law_debt_slavery","interest_group_ig_landowners_pol_str_mult"},250'000},
        {{"law_slave_trade","interest_group_ig_landowners_pol_str_mult"},500'000},
        {{"law_legacy_slavery","interest_group_ig_landowners_pol_str_mult"},250'000}};
    std::set<std::pair<std::string_view,std::string_view>> base_strength_proven;
    for(const auto& row:audit) {
        if(row.behavior_test!=base_strength_fixture)continue;
        const auto pair=std::make_pair(row.source.source_id,row.source.key);
        const auto expected=base_strengths.find(pair);
        check(!row.source.boolean&&!row.source.institution&&!row.source.sponsor&&
              row.status==CanonicalLawRowStatus::verified_row&&
              row.executor=="IntegratedWorld::p2_political_strength_ppm -> p3_group_view (active national base law strength)"&&
              expected!=base_strengths.end()&&expected->second==row.normalized_value&&
              base_strength_proven.insert(pair).second,
              "national base strength evidence lost the exact row or executor");
        for(int alternative=0;alternative<3;++alternative) {
            auto mismatch=row.source;
            if(alternative==0)mismatch.sponsor=true;
            if(alternative==1)mismatch.institution=true;
            if(alternative==2)mismatch.key="country_aristocrats_pol_str_mult";
            const auto classified=classify_canonical_law_row(
                mismatch,canonical_law_row_evidence());
            check(classified.behavior_test!=base_strength_fixture,
                  "national base strength witness leaked to sponsorship, institution, or profession");
            if(alternative<2)
                check(classified.status!=CanonicalLawRowStatus::verified_row,
                      "national base group strength was mistaken for a qualified row");
        }
    }
    check(base_strength_proven.size()==base_strengths.size(),
          "missing exact national base strength witness");

    std::size_t institutional_strength=0;
    for(const auto& row:audit) {
        if(row.behavior_test!="test_p3_p5_politics::funded_institutions_weight_only_their_typed_group_members")continue;
        check(row.source.key.starts_with("interest_group_ig_") &&
              row.source.key.ends_with("_pol_str_mult") &&
              !row.source.boolean && row.source.institution && !row.source.sponsor &&
              row.executor=="IntegratedWorld::p2_political_strength_ppm (funded staffed national institution)" &&
              row.status==CanonicalLawRowStatus::verified_row,
              "effective institution clout witness lost its exact qualification");
        auto mismatch=row.source;
        mismatch.institution=false;
        check(classify_canonical_law_row(mismatch,canonical_law_row_evidence()).status==
                  CanonicalLawRowStatus::no_recorded_path,
              "institutional group strength proof leaked to a flat row");
        ++institutional_strength;
    }
    check(institutional_strength==7 &&
          std::none_of(audit.begin(),audit.end(),[](const auto& row) {
              return row.source.institution && row.status==CanonicalLawRowStatus::verified_row &&
                     row.source.key.starts_with("interest_group_ig_") &&
                     row.source.key.ends_with("_pol_str_mult") &&
                     row.behavior_test!="test_p3_p5_politics::funded_institutions_weight_only_their_typed_group_members";
          }),"institution group strength evidence leaked to an unresolved owner");

    constexpr std::string_view draft_fixture="test_politics_identity::military_peasant_levy_restricts_real_conscripts";
    std::set<std::string_view> draft_sources;
    for(const auto& row:audit) {
        if(row.behavior_test!=draft_fixture)continue;
        check(row.source.key=="country_can_only_conscript_peasants_bool"&&
              row.source.boolean&&!row.source.institution&&!row.source.sponsor&&
              row.normalized_value==1&&row.status==CanonicalLawRowStatus::verified_row&&
              row.executor=="IntegratedWorld::mobilize / peacetime_military_staff (actual peasant-only draft)"&&
              draft_sources.insert(row.source.source_id).second,
              "draft evidence lost the exact flat boolean, source or real executor");
        for(int alternative=0;alternative<4;++alternative) {
            auto mismatch=row.source;
            if(alternative==0)mismatch.sponsor=true;
            if(alternative==1)mismatch.institution=true;
            if(alternative==2)mismatch.boolean=false;
            if(alternative==3)mismatch.key="country_limit_officers_qualifications_to_upper_strata";
            check(classify_canonical_law_row(mismatch,canonical_law_row_evidence()).behavior_test
                      !=draft_fixture,
                  "actual draft evidence leaked into a non-draft or qualified row");
        }
    }
    check(draft_sources==std::set<std::string_view>{"law_peasant_levies","law_warrior_caste"},
          "expected both exact national peasant-only conscription sources");

    // Synthetic evidence: three flat source rows share a key with an
    // institutional row. This does not assert unresolved migration behavior.
    const std::array<CanonicalLawRowEvidence,3> mixed{{
        {{"demo_a","mixed_key",0.5,false,false},"daily","test_a"},
        {{"demo_b","mixed_key",0.5,false,false},"daily","test_b"},
        {{"demo_c","mixed_key",0.25,false,false},"daily","test_c"}
    }};
    for(const auto& proof:mixed) {
        const auto row=classify_canonical_law_row(proof.row,mixed,"legacy");
        check(row.status==CanonicalLawRowStatus::verified_row&&
              row.executor=="daily"&&row.behavior_test==proof.behavior_test,
              "specific proven source row was not distinguished");
    }
    auto different_scope=mixed[0].row;
    different_scope.institution=true;
    const auto institution=classify_canonical_law_row(different_scope,mixed,"legacy");
    check(institution.source.institution&&institution.normalized_value==500000&&
          institution.status==CanonicalLawRowStatus::named_candidate&&
          institution.executor=="legacy"&&institution.behavior_test.empty(),
          "institution row inherited flat behavior evidence");
    different_scope.institution=false;
    different_scope.sponsor=true;
    check(classify_canonical_law_row(different_scope,mixed).status==
          CanonicalLawRowStatus::no_recorded_path,
          "sponsor row inherited flat behavior evidence");
    check(classify_canonical_law_row({"demo","boolean",1,true},{}).normalized_value==1 &&
          classify_canonical_law_row({"demo","negative",-0.125},{}).normalized_value==-125000,
          "diagnostic changed catalog numeric/boolean representation");
}

static void legal_policies_and_override_priority() {
    auto gh=make_world("laissez_faire");
    auto& c=gh.seed_geography().seed_core().seed_core();
    check(c.p2_selected_law("A","economic_system")==
                std::optional<std::string>("laissez_faire"),
          "actual legal selection lost catalogue identity");
    check(c.p2_rule("A","A",P2ClauseKind::female_suffrage_allowed)==0&&
          c.p2_rule("A","A",P2ClauseKind::peaceful_strike_allowed)==0&&
          c.p2_rule("A","A",P2ClauseKind::political_association_allowed)==1&&
          c.p2_rule("A","A",P2ClauseKind::public_healthcare_right)==0&&
          c.p2_rule("A","A",P2ClauseKind::income_tax_max_ppm)==0,
          "effective law choice not connected to legal rights");
    check(c.p2_institution_factor_ppm(
              "A",P1InstitutionKind::welfare)==0,
          "no social-security law unexpectedly funded public welfare");
    check(c.p2_institution_factor_ppm(
              "A",P1InstitutionKind::healthcare)==0&&
          c.p1_institution_capacity("HOSPITAL").usable_slots==0,
          "forbidden health system served patients through legacy facility");
    check(c.p2_political_strength_ppm("A",P3GroupType::industrialists)>1'000'000,
          "laissez-faire law not affecting actual company IG clout");
    P2Amendment amendment;
    amendment.id="Suffrage-Amendment";
    amendment.law_id="test:rights_of_women";
    amendment.legal_basis="test:statutory-override";
    amendment.clauses.push_back({
        "explicit-suffrage","A",P2ClauseKind::female_suffrage_allowed,1});
    c.p2_seed_amendment(amendment);
    check(c.p2_rule("A","A",P2ClauseKind::female_suffrage_allowed)==1,
          "enacted legal amendment did not override base law catalogue");
    P2BaseLaw invalid;
    invalid.id="invalid-group-law";invalid.country="A";
    invalid.domain="lawgroup_economic_system";
    invalid.scope="national";
    invalid.base_choice="law_womens_suffrage";
    invalid.legal_basis="test:illegal";
    check(rejects([&]{c.p2_seed_base(invalid);}),
          "wrong law/group pairing unexpectedly accepted");
}
static void observed_clout_ideology_and_legitimacy() {
    auto baseline=daily::World::adopt(make_world("laissez_faire"),81);
    auto alternative=daily::World::adopt(make_world("cooperative_ownership"),81);
    phasegh::World::Plan enroll;
    enroll[9].push_back([](phasegh::Stage& gh) {
        auto& c=gh.phase_c();
        const auto& populations=c.people().slices();
        check(populations.size()==1,"synthetic fixture has unexpected POP slices");
        const auto actual_slice=populations.begin()->first;
        c.join_interest_group(actual_slice,"WORKERS",35);
        c.join_interest_group(actual_slice,"BUSINESS",35);
    });
    baseline.advance_day(enroll);
    alternative.advance_day(enroll);
    const auto& rich=baseline.engine().base().core().core();
    const auto& cooperative=alternative.engine().base().core().core();
    check(rich.p3_group_view("A","BUSINESS").clout_ppm>
          cooperative.p3_group_view("A","BUSINESS").clout_ppm,
          "actual organized-clout failed to react to ownership-law change");
    check(cooperative.p3_law_stance("A","WORKERS",
              "economic_system","cooperative_ownership").value_or(0)>0&&
          cooperative.p3_law_stance("A","BUSINESS",
              "economic_system","cooperative_ownership").value_or(0)<0,
          "named ideology of real IG not applied to legal preference");
    check(cooperative.p1_legitimacy("A").ideological_incompatibility>0,
          "coalition policy split not reflected in derived legitimacy");
    const auto reform=cooperative.p2_political_support(
        "A","economic_system","laissez_faire");
    check(reform.observed&&reform.support_ppm>0&&
          reform.oppose_ppm>0&&reform.governing_support_ppm>0&&
          reform.support_ppm+reform.oppose_ppm+reform.neutral_ppm==1'000'000,
          "law-change support failed to compare current and proposed stances");
    const auto old_pop=cooperative.people().population();
    const auto old_cash=cooperative.cash("A");
    check(old_pop==rich.people().population()&&old_cash==rich.cash("A"),
          "politics law erroneously created population or budget cash");
    const auto bytes=daily::SnapshotCodec::encode(alternative);
    auto restored=daily::SnapshotCodec::decode(bytes);
    const auto& copy=restored.engine().base().core().core();
    check(daily::SnapshotCodec::encode(restored)==bytes&&
          copy.p3_ideologies("A","WORKERS").size()==2&&
          copy.p3_law_stance("A","WORKERS",
              "economic_system","cooperative_ownership")==
          cooperative.p3_law_stance("A","WORKERS",
              "economic_system","cooperative_ownership"),
          "P3 ideological state or named leader corrupted during save/load");
}
static void source_law_incompatibilities_are_symmetric_and_real() {
    const auto& c=politics::catalogue();
    check(c.disallowed.size()==148 &&
          c.disallowed.at("traditionalism").contains("graduated_taxation") &&
          c.disallowed.at("serfdom").contains("laissez_faire"),
          "sourced 1.15 law incompatibilities not registered");
    phasegh::World world(24000);
    auto& geo=world.seed_geography();
    geo.set_dataset("law-compatibility");
    geo.seed_province(1,"R","A");
    geo.seed_unit(1,"A","A");
    geo.seed_assignment(1,1);
    auto& core=geo.seed_core().seed_core();
    core.seed_actor("A",1000);
    core.seed_government("A","HEAD","PRIME",5,3);
    core.seed_p1_constitution("A",{P1GovernancePrinciple::monarchy,
        P1PowerDistribution::autocracy,P1LegislatureKind::none,
        "constitutional:approval"});
    law(core,"economic_system","traditionalism");
    check(rejects([&]{law(core,"taxation","graduated_taxation");}),
          "graduated taxation incorrectly coexists with Traditionalism");
    law(core,"taxation","land_based_taxation");
    check(core.p2_selected_law("A","taxation")==
          std::optional<std::string>{"land_based_taxation"},
          "compatible sourced law could not be enacted");
    world.validate();
}
static void no_new_fictional_leader_or_dynamic_ideology() {
    auto gh=make_world("interventionism");
    auto& c=gh.seed_geography().seed_core().seed_core();
    check(rejects([&]{c.p3_set_leader_ideology(
              "A","BUSINESS","socialist");}),
          "P3 created ideological leader without real PersonAnchor");
    check(rejects([&]{c.p3_set_ideologies(
              "A","WORKERS",{"socialist","socialist"});}),
          "duplicate ideologies accepted");
    check(rejects([&]{c.p3_set_ideologies(
              "A","WORKERS",{"fictional_unknown_ideology"});}),
          "unknown political ideology accepted");
}
static void enacted_national_laws_change_only_calculated_legitimacy() {
    const auto run=[](const std::string& power,const std::string& government,
                      const std::string& amendment,bool conflict,bool local_parent,
                      int flat,int penalty,bool outsiders=false) {
        phasegh::World gh{24000};
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"VIENNA","AUS");
        geo.seed_unit(1,"AUS","ADMIN");
        geo.seed_assignment(1,1);
        auto& c=geo.seed_core().seed_core();
        c.seed_actor("AUS",500);
        c.seed_government("AUS","HEAD","PRIME",5,3);
        c.seed_p1_constitution("AUS",{
            P1GovernancePrinciple::monarchy,P1PowerDistribution::autocracy,
            P1LegislatureKind::none,"fixture:constitution"});
        for(const auto* group:{"WORKERS","BUSINESS"})c.seed_interest_group(group);
        c.seed_p1_coalition("AUS",{conflict?
            std::vector<std::string>{"WORKERS","BUSINESS"}:
            std::vector<std::string>{"WORKERS"},{},false});
        c.p3_seed_group({"AUS","WORKERS","","","",P3GroupType::trade_unions,0,0});
        c.p3_seed_group({"AUS","BUSINESS","","","",P3GroupType::industrialists,0,0});
        c.p3_set_ideologies("AUS","WORKERS",{"proletarian"});
        c.p3_set_ideologies("AUS","BUSINESS",{"laissez_faire"});
        if(outsiders) {
            c.seed_interest_group("OUTSIDE");
            c.p3_seed_group({"AUS","OUTSIDE","","","",P3GroupType::rural_folk,0,0});
        }
        const auto law=[&](const std::string& id,const std::string& group,
                           const std::string& choice,bool local=false) {
            P2BaseLaw entry;
            entry.id=id;entry.country="AUS";entry.domain=group;
            entry.scope=local?"VIENNA":"national";entry.base_choice=choice;
            if(local)entry.tier=P2NormTier::local_ordinance;
            entry.legal_basis="fixture:law";
            c.p2_seed_base(entry);
        };
        law("ECONOMY","economic_system","law_cooperative_ownership");
        if(!power.empty())law("POWER","distribution_of_power",power);
        if(!government.empty())law("GOVERNMENT","governance_principles",government);
        if(local_parent)law("LOCAL","regional_rules","civic_rule",true);
        if(!amendment.empty())
            c.p2_seed_amendment({amendment,local_parent?"LOCAL":"POWER",
                "fixture:amendment","",{{"actual-clause","AUS",
                    P2ClauseKind::political_association_allowed,1}}});
        auto& pop=c.seed_population();
        const auto h=pop.household_profile({"VIENNA","mixed","shared",100});
        const auto e=pop.economic_profile({"VIENNA","worker","wage","employee",
                                           pop.default_education()});
        const auto d=pop.demographic_profile({"VIENNA",23650,phaseb::Sex::male,
            "culture","faith","citizen","free",pop.default_health(),
            phaseb::Service::civilian});
        const auto slice=pop.seed_population(d,e,h,100);
        auto world=daily::World::adopt(std::move(gh),10);
        phasegh::World::Plan join;
        join[9].push_back([slice,outsiders](phasegh::Stage& stage) {
            if(outsiders)stage.phase_c().join_interest_group(slice,"OUTSIDE",100);
            else {
                stage.phase_c().join_interest_group(slice,"WORKERS",50);
                stage.phase_c().join_interest_group(slice,"BUSINESS",50);
            }
        });
        world.advance_day(join);
        const auto& live=world.engine().base().core().core();
        const auto view=live.p1_legitimacy("AUS");
        if(!(view.observed_groups&&(outsiders?view.group_support==0:view.group_support>0)&&
              !view.observed_election&&view.electoral_support==50&&
              view.legal_compatibility==100&&live.government("AUS").quorum==0)) {
            std::cerr<<"support="<<view.group_support<<", groups="<<view.observed_groups
                     <<", election="<<view.observed_election<<", seats="
                     <<view.electoral_support<<", legal="<<view.legal_compatibility
                     <<", quorum="<<live.government("AUS").quorum<<"\n";
            throw std::runtime_error("legitimacy fixture lost actual support or voting authority");
        }
        check(live.p3_law_stance("AUS","WORKERS","economic_system",
                 "cooperative_ownership")==2&&
              live.p3_law_stance("AUS","BUSINESS","economic_system",
                 "cooperative_ownership")==-2,
              "fixture did not record actual opposing stances");
        const int baseline=(50*view.group_support+30*view.electoral_support+
                            20*view.legal_compatibility)/100;
        check(view.ideological_incompatibility==penalty&&
              view.score==std::clamp(baseline+flat-penalty,0,100),
              "adopted law did not adjust actual cabinet legitimacy in the correct units");
        check(live.people().population()==100&&live.cash("AUS")==500,
              "legitimacy calculation created a fictional POP or treasury change");
    };
    run("","","",true,false,0,20);
    run("law_autocracy","","",true,false,0,30);
    run("law_autocracy","","",false,false,0,0);
    run("","law_corporate_state","",true,false,0,15);
    run("law_elder_council","","",true,false,20,18);
    run("law_autocracy","","amendment_cult_of_personality",true,false,-5,30);
    run("law_universal_suffrage","","amendment_tradition_of_free_elections",
        true,false,10,20);
    for(const auto* variant:{"amendment_geheime_staatskonferenz",
          "amendment_geheime_staatskonferenz_metternich","amendment_geheime_staatskonferenz_kolowrat"})
        run("law_autocracy","",variant,true,false,50,30);
    run("law_autocracy","","amendment_geheime_staatskonferenz",
        false,false,50,0); // result would exceed 100 without the upper clamp
    run("law_autocracy","","amendment_cult_of_personality",true,true,0,30);
    run("law_elder_council","","amendment_cult_of_personality",true,false,20,18);
    run("law_autocracy","law_monarchy","amendment_cult_of_personality",
        true,false,-5,32,true); // negative result clamps to zero
}

int main() {
    try {
        trait_only_hostility_does_not_claim_law_coverage();
        catalogued_laws_and_stances();
        canonical_effect_audit_retains_scope_and_evidence();
        legal_policies_and_override_priority();
        observed_clout_ideology_and_legitimacy();
        enacted_national_laws_change_only_calculated_legitimacy();
        source_law_incompatibilities_are_symmetric_and_real();
        no_new_fictional_leader_or_dynamic_ideology();
        std::cout<<"VIC2.5 POLITICAL LAWS IDEOLOGIES GROUPS PASS\n";
    } catch(const std::exception& e) {
        std::cerr<<"POLITICS CATALOG FAILURE: "<<e.what()<<"\n";
        return 1;
    }
}
