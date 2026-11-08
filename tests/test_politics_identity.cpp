#include "vic25/daily_snapshot.hpp"
#include "vic25/phase_c_politics_catalog.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace vic25;
using namespace vic25::phasec;

static void check(bool yes,const char* message) {
    if(!yes)throw std::runtime_error(message);
}
template<class F>static bool rejects(F&& fn) {
    try{fn();}catch(const std::exception&){return true;}
    return false;
}
static void law(IntegratedWorld& core,const std::string& country,
                const std::string& group,const std::string& choice) {
    P2BaseLaw entry;
    entry.id=country+":"+group;
    entry.country=country;
    entry.domain="lawgroup_"+group;
    entry.scope="national";
    entry.base_choice="law_"+choice;
    entry.legal_basis="scenario:constitution";
    core.p2_seed_base(entry);
}
struct Scenario {
    phasegh::World gh{24000};
    phaseb::Id primary{},foreign{},slave{},serf{},other{};
    phaseb::Id foreign_household{};
    explicit Scenario(std::string b_migration="migration_controls",
                      std::string a_citizenship="ethnostate",
                      bool abolition=true) {
        auto& geo=gh.seed_geography();
        geo.set_dataset("political-identity-fixture");
        geo.seed_province(1,"RA","A");
        geo.seed_province(2,"RB","B");
        geo.seed_unit(10,"A","A-DISTRICT");
        geo.seed_unit(20,"B","B-DISTRICT");
        geo.seed_assignment(1,10);
        geo.seed_assignment(2,20);
        auto& c=geo.seed_core().seed_core();
        c.seed_actor("A",500);
        c.seed_actor("B",400);
        c.seed_actor("EMP",500);
        c.seed_government("A","HEAD_A","PM_A",5,3);
        c.seed_government("B","HEAD_B","PM_B",5,3);
        c.seed_p1_constitution("A",{
            P1GovernancePrinciple::monarchy,
            P1PowerDistribution::autocracy,
            P1LegislatureKind::none,"scenario:constitution"});
        c.seed_p1_constitution("B",{
            P1GovernancePrinciple::monarchy,
            P1PowerDistribution::autocracy,
            P1LegislatureKind::none,"scenario:constitution"});
        c.p2_seed_culture({"AAA",{"h_aaa"},{"hgrp_aaa"},{"lang_aaa"},
            {"langgroup_aaa"},{"trad_aaa"},{"RA"}});
        c.p2_seed_culture({"BBB",{"h_bbb"},{"hgrp_bbb"},{"lang_bbb"},
            {"langgroup_bbb"},{"trad_bbb"},{"RB"}});
        c.p2_seed_religion({"FAITH_A",{"relig_a"},{"rgrp_a"}});
        c.p2_seed_religion({"FAITH_B",{"relig_b"},{"rgrp_b"}});
        c.p2_seed_national_identity({"A",{"AAA"},{"FAITH_A"}});
        c.p2_seed_national_identity({"B",{"BBB"},{"FAITH_B"}});
        auto& pop=c.seed_population();
        const auto hha=pop.household_profile({"RA","mixed","shared",50});
        foreign_household=pop.household_profile({"RB","mixed","shared",50});
        const auto eca=pop.economic_profile({"RA","worker","wage","employee",
                                            pop.default_education()});
        const auto ecb=pop.economic_profile({"RB","worker","wage","employee",
                                            pop.default_education()});
        const auto demographic=[&](std::string region,std::string culture,
                                   std::string religion,std::string status) {
            return pop.demographic_profile({region,24000-350,
                phaseb::Sex::male,culture,religion,"citizen",status,
                pop.default_health(),phaseb::Service::civilian});
        };
        primary=pop.seed_population(
            demographic("RA","AAA","FAITH_A","free"),eca,hha,30,1);
        foreign=pop.seed_population(
            demographic("RA","BBB","FAITH_B","free"),eca,hha,20,2);
        slave=pop.seed_population(
            demographic("RA","AAA","FAITH_A","slave"),eca,hha,10,3);
        serf=pop.seed_population(
            demographic("RA","AAA","FAITH_A","serf"),eca,hha,12,4);
        other=pop.seed_population(
            demographic("RB","BBB","FAITH_B","free"),ecb,foreign_household,30,5);
        c.seed_interest_group("WORKERS");
        c.p3_seed_group({"A","WORKERS","","","",P3GroupType::trade_unions,
                         800'000,0});
        law(c,"A","citizenship",a_citizenship);
        law(c,"A","church_and_state","state_religion");
        law(c,"A","migration","no_migration_controls");
        law(c,"A","land_reform","serfdom");
        if(abolition)law(c,"A","slavery","slavery_banned");
        law(c,"B","citizenship","multicultural");
        law(c,"B","migration",b_migration);
        gh.validate();
    }
};
static const IntegratedWorld& view(const daily::World& w){
    return w.engine().base().core().core();
}
static void source_ideologies_laws_traits(){
    const auto& c=politics::catalogue();
    check(c.by_group.size()==26&&c.laws.size()==148,
          "1.15 law catalogue not complete");
    check(c.ideologies.size()>=172,
          "1.13.11 verified ideology references omitted");
    check(c.stance("paternalistic","governance_principles","monarchy").value_or(0)>0,
          "imported source ideology positions missing");
    check(c.stance("feminist","rights_of_women","womens_suffrage").value_or(0)>0,
          "verified leader ideology position missing");
    std::size_t effects=0;
    for(const auto& [law,items]:c.modifiers) {
        check(c.laws.contains(law),"unmapped verified law modifier");
        effects+=items.size();
    }
    check(c.modifiers.size()==136&&effects==730,
          "verified 1.13 modifier count changed without source review");
    const auto serf=c.modifier("serfdom","state_peasants_internal_migration_disallowed_bool");
    check(serf.has_value()&&serf->boolean&&serf->value_ppm==1'000'000,
          "V3 serf mobility restriction missing from sourced modifiers");
    const auto tax=c.modifier("traditionalism","state_tax_capacity_mult");
    check(tax.has_value()&&!tax->boolean,
          "real traditionalism tax-capacity modifier not imported");
    check(politics::ig_trait_rules().size()==24,
          "eight interest groups must have all three baseline traits");
    const auto loyal=politics::active_ig_effects(P3GroupType::devout,80,100);
    bool fertility=false;
    for(const auto& x:loyal)
        if(x.modifier=="state_birth_rate_mult")
            fertility=x.value_ppm==30'000;
    check(fertility,"1.15 loyal Devout fertility trait source mismatch");
    check(politics::active_ig_effects(P3GroupType::devout,80,0).empty(),
          "fictional IG trait from zero real members");
}
static void acceptance_and_five_tier_rights() {
    Scenario s;
    const auto& c=s.gh.base().core().core();
    const auto a=c.p2_acceptance("A",s.primary);
    const auto b=c.p2_acceptance("A",s.foreign);
    check(a.has_value()&&b.has_value()&&a->score==100&&b->score<=19,
          "cultural acceptance did not read explicit primary culture and law");
    check(c.p2_citizenship_vote_allowed("A",s.primary) &&
          !c.p2_citizenship_vote_allowed("A",s.foreign),
          "ethnostate enfranchisement not tied to actual POP acceptance");
    const auto disallowed=c.p2_acceptance_effect("A",s.foreign);
    check(disallowed&&disallowed->political_strength_ppm==500'000&&
          disallowed->wage_ppm==500'000,
          "ethnostate hostility penalties not applied");
    const auto allowed=c.p2_acceptance_effect("A",s.primary);
    check(allowed&&allowed->wage_ppm==1'250'000,
          "fully accepted primary culture wage rights incorrect");
    const auto no_data=c.p2_acceptance("A",99999);
    check(!no_data.has_value(),"unknown POP acceptance falsely inferred");
    const auto existing=s.gh.base().core().core().people().population();
    auto w=daily::World::adopt(std::move(s.gh),42);
    phasegh::World::Plan contract;
    const auto worker=s.foreign;
    contract[2].push_back([worker](phasegh::Stage& stage){
        stage.phase_c().hire(worker,2,"EMP",100);
    });
    w.advance_day(contract);
    const auto& c2=view(w);
    const auto job=c2.people().job(worker);
    check(job&&job->wage_per_month==50&&
          c2.people().population()==existing,
          "discrimination didn't affect actual new wage contract exactly once");
    const auto bytes=daily::SnapshotCodec::encode(w);
    check(daily::SnapshotCodec::encode(
                daily::SnapshotCodec::decode(bytes))==bytes,
          "source-qualified cultural identities broke old daily checkpoint");
}
static void physical_migration_and_law_block() {
    Scenario closed("closed_borders");
    auto denied=daily::World::adopt(std::move(closed.gh),19);
    const auto before=daily::SnapshotCodec::encode(denied);
    phasegh::World::Plan blocked;
    const auto foreign=closed.foreign,household=closed.foreign_household;
    blocked[10].push_back([foreign,household](phasegh::Stage& stage) {
        stage.phase_c().migrate_with_explicit_property_allocation(
            foreign,4,"RB",household,"A","B",20,"",0);
    });
    check(rejects([&]{denied.advance_day(blocked);}) &&
          daily::SnapshotCodec::encode(denied)==before,
          "closed borders must roll back physical POP/cash migration");

    Scenario open("migration_controls");
    auto permitted=daily::World::adopt(std::move(open.gh),19);
    const auto count=view(permitted).people().population();
    const auto startCashA=view(permitted).cash("A");
    const auto startCashB=view(permitted).cash("B");
    phasegh::World::Plan plan;
    const auto permittedPop=open.foreign;
    const auto dest=open.foreign_household;
    plan[10].push_back([permittedPop,dest](phasegh::Stage& stage) {
        stage.phase_c().migrate_with_explicit_property_allocation(
            permittedPop,4,"RB",dest,"A","B",20,"",0);
    });
    permitted.advance_day(plan);
    check(view(permitted).people().population()==count&&
          view(permitted).cash("A")==startCashA-20&&
          view(permitted).cash("B")==startCashB+20,
          "lawful migration failed to preserve real headcount and money");
    auto snapshot=daily::SnapshotCodec::encode(permitted);
    auto replay=daily::SnapshotCodec::decode(snapshot);
    check(daily::SnapshotCodec::encode(replay)==snapshot,
          "actual permitted cross-border POP/property transfer not saved");

    Scenario bound("migration_controls");
    check(!bound.gh.base().core().core().p2_migration_permitted(
             bound.serf,"RB"),
          "serfdom not restricting actual bonded agricultural mobility");
}
static void abolition_preserves_real_people_and_membership() {
    Scenario s;
    auto world=daily::World::adopt(std::move(s.gh),22);
    const auto initialPeople=view(world).people().population();
    const auto initCash=view(world).cash("A");
    phasegh::World::Plan action;
    const auto slave=s.slave;
    action[9].push_back([slave](phasegh::Stage& stage){
        stage.phase_c().join_interest_group(slave,"WORKERS",5);
    });
    world.advance_day(action);
    const auto& c=view(world);
    bool bonded=false,freed=false,serfStill=false;
    for(const auto& [id,row]:c.people().slices()) {
        const auto& pop=c.people().demographic(row.key.demographic);
        if(pop.region!="RA")continue;
        if(pop.legal_status=="slave")bonded=true;
        if(pop.legal_status=="serf")serfStill=true;
        if(pop.legal_status=="free"&&
           c.group_members(id,"WORKERS")==5)freed=true;
    }
    check(!bonded&&freed&&serfStill&&
          c.people().population()==initialPeople&&
          c.cash("A")==initCash,
          "abolition created POP/money or lost emancipation movement membership");
    auto bytes=daily::SnapshotCodec::encode(world);
    check(daily::SnapshotCodec::encode(
        daily::SnapshotCodec::decode(bytes))==bytes,
          "new legal-status transition lost saving deterministic");
}
static void economic_law_restricts_real_company_ownership() {
    Scenario fixture;
    auto& c=fixture.gh.seed_geography().seed_core().seed_core();
    c.seed_actor("OWNER",100);
    c.seed_actor("PRIVATE",100);
    c.seed_production_facility("MILL","OWNER","RA",10);
    c.seed_shares("company:OWNER","OWNER",10);
    law(c,"A","economic_system","command_economy");
    fixture.gh.validate();
    auto world=daily::World::adopt(std::move(fixture.gh),77);
    const auto before=daily::SnapshotCodec::encode(world);
    phasegh::World::Plan forbidden;
    forbidden[7].push_back([](phasegh::Stage& stage) {
        stage.phase_c().sell_shares("company:OWNER","OWNER","PRIVATE",2);
    });
    check(rejects([&]{world.advance_day(forbidden);})&&
          daily::SnapshotCodec::encode(world)==before,
          "command economy illegally transferred physical company to private buyer");
    phasegh::World::Plan state_acquisition;
    state_acquisition[7].push_back([](phasegh::Stage& stage) {
        stage.phase_c().sell_shares("company:OWNER","OWNER","A",2);
    });
    world.advance_day(state_acquisition);
    check(view(world).shares("company:OWNER","OWNER")==8&&
          view(world).shares("company:OWNER","A")==2,
          "real state acquisition under command economy did not settle shares");
    const auto saved=daily::SnapshotCodec::encode(world);
    check(daily::SnapshotCodec::encode(
        daily::SnapshotCodec::decode(saved))==saved,
        "state-owned company title changed on checkpoint replay");
}

int main(){
    try{
        source_ideologies_laws_traits();
        acceptance_and_five_tier_rights();
        physical_migration_and_law_block();
        abolition_preserves_real_people_and_membership();
        economic_law_restricts_real_company_ownership();
        std::cout<<"VIC2.5 POLICY IDENTITY AND REFERENCE DATA PASS\n";
    }catch(const std::exception& e){
        std::cerr<<"POLITICS IDENTITY FAILURE: "<<e.what()<<"\n";
        return 1;
    }
}
