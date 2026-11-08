#include "vic25/daily_snapshot.hpp"
#include "vic25/phase_c_politics_catalog.hpp"
#include <iostream>
#include <memory>
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
    law(c,"distribution_of_power","universal_suffrage");
    c.seed_public_institution("HOSPITAL","CLINIC","R","medicine",100,3);
    c.seed_p1_institution("A","HOSPITAL",
                          P1InstitutionKind::healthcare,5);
    gh.validate();
    return gh;
}
static void catalogued_laws_and_stances() {
    const auto& c=politics::catalogue();
    check(c.by_group.size()==26&&c.laws.size()>=140,
          "1.15-inspired catalogue missing law families");
    check(c.has("lawgroup_economic_system","law_cooperative_ownership")&&
          c.has("rights_of_women","womens_suffrage")&&
          c.has("labor_associations","factory_councils")&&
          c.has("navy_model","capital_fleet")&&
          !c.has("economic_system","womens_suffrage"),
          "catalogue group membership incorrect");
    check(c.stance("socialist","economic_system","cooperative_ownership")
             .value_or(0)>0&&
          c.stance("laissez_faire","economic_system","cooperative_ownership")
             .value_or(0)<0 &&
          c.stance("militarist","policing","militarized_police")
             .value_or(0)>0,
          "5-step ideology positions missing");
    check(politics::default_ideologies(P3GroupType::trade_unions).size()>=2&&
          politics::default_ideologies(P3GroupType::custom).empty(),
          "non-historical custom IG must not be assigned invented ideology");
}
static void legal_policies_and_override_priority() {
    auto gh=make_world("laissez_faire");
    auto& c=gh.seed_geography().seed_core().seed_core();
    check(c.p2_selected_law("A","economic_system")==
                std::optional<std::string>("laissez_faire"),
          "actual legal selection lost catalogue identity");
    check(c.p2_rule("A","A",P2ClauseKind::female_suffrage_allowed)==0&&
          c.p2_rule("A","A",P2ClauseKind::peaceful_strike_allowed)==0&&
          c.p2_rule("A","A",P2ClauseKind::political_association_allowed)==0&&
          c.p2_rule("A","A",P2ClauseKind::public_healthcare_right)==0,
          "effective law choice not connected to legal rights");
    check(c.p2_institution_factor_ppm(
              "A",P1InstitutionKind::healthcare)==0&&
          c.p1_institution_capacity("HOSPITAL").usable_slots==0,
          "forbidden health system served patients through legacy facility");
    check(c.p2_political_strength_ppm("A",P3GroupType::industrialists)>1'000'000,
          "laissez-faire law not affecting actual company IG clout");
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
        c.join_interest_group(1,"WORKERS",35);
        c.join_interest_group(1,"BUSINESS",35);
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
int main() {
    try {
        catalogued_laws_and_stances();
        legal_policies_and_override_priority();
        observed_clout_ideology_and_legitimacy();
        no_new_fictional_leader_or_dynamic_ideology();
        std::cout<<"VIC2.5 POLITICAL LAWS IDEOLOGIES GROUPS PASS\n";
    } catch(const std::exception& e) {
        std::cerr<<"POLITICS CATALOG FAILURE: "<<e.what()<<"\n";
        return 1;
    }
}
