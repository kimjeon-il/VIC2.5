#include "vic25/daily_snapshot.hpp"
#include "vic25/phase_c_politics_catalog.hpp"
#include "vic25/phase_c_politics_effect_audit.hpp"
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
                      bool abolition=true,bool allow_serfdom=true) {
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
        law(c,"A","land_reform",allow_serfdom?"serfdom":"tenant_farmers");
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
    check(c.ideologies.size()==190,
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
    check(c.modifiers.size()==148&&effects==812,
          "verified 1.15 modifier count changed without source review");
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
    // Source 1.15 forbids Command Economy while serfdom remains enacted.
    // Use lawful Tenant Farmers instead of an impossible mixed-law scenario.
    Scenario fixture("migration_controls","ethnostate",true,false);
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

static void military_peasant_levy_restricts_real_conscripts() {
    Scenario fixture;
    auto& core=fixture.gh.seed_geography().seed_core().seed_core();
    core.seed_unit("LEVY");
    law(core,"A","army_model","peasant_levies");
    P2BaseLaw conscription;
    conscription.id="A:lawful_conscription";
    conscription.country="A";conscription.domain="conscription_fixture";
    conscription.scope="national";conscription.base_choice="military-law";
    conscription.legal_basis="fixture:conscription";
    conscription.clauses.push_back({
        "conscription-permitted","A",
        P2ClauseKind::conscription_authorized,1});
    core.p2_seed_base(conscription);
    auto& pop=core.seed_population();
    const auto row=pop.slice(fixture.primary);
    const auto economic=pop.economic_profile({
        "RA","peasant","subsistence","self_employed",
        pop.default_education()});
    const auto peasant=pop.seed_population(
        row.key.demographic,economic,row.key.household,100,7001);
    fixture.gh.validate();
    auto world=daily::World::adopt(std::move(fixture.gh),12);
    const auto before=daily::SnapshotCodec::encode(world);
    phasegh::World::Plan blocked;
    const auto civilian=fixture.primary;
    blocked[10].push_back([civilian](phasegh::Stage& gh) {
        gh.phase_c().mobilize(civilian,1,"LEVY");
    });
    check(rejects([&]{world.advance_day(blocked);})&&
          daily::SnapshotCodec::encode(world)==before,
          "peasant levies conscripted non-peasant workers");
    const auto total=view(world).people().population();
    phasegh::World::Plan actual;
    actual[10].push_back([peasant](phasegh::Stage& gh) {
        gh.phase_c().mobilize(peasant,3,"LEVY");
    });
    world.advance_day(actual);
    check(view(world).people().population()==total&&
          view(world).active_strength("LEVY")==3,
          "actual lawful militia failed POP/army conservation");
    const auto saved=daily::SnapshotCodec::encode(world);
    check(daily::SnapshotCodec::encode(
        daily::SnapshotCodec::decode(saved))==saved,
          "military law actual conscripts failed checkpoint replay");
}

static void effective_workhouse_amendment_reduces_only_welfare_cost(
        const std::string& amendment_id) {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"R","A");
    geo.seed_unit(1,"A","ADMIN");
    geo.seed_assignment(1,1);
    auto& c=geo.seed_core().seed_core();
    c.seed_actor("A",1000);
    c.seed_actor("SERVICES",1000);
    c.seed_government("A","HEAD","PRIME",5,3);
    c.seed_p1_constitution("A",{P1GovernancePrinciple::monarchy,
        P1PowerDistribution::autocracy,P1LegislatureKind::none,
        "test:welfare-authority"});
    c.p1_set_incorporation("R",true);
    auto& pop=c.seed_population();
    const auto hh=pop.household_profile({"R","mixed","shared",25'000});
    const auto worker=pop.economic_profile({
        "R","worker","wage","employee",pop.default_education()});
    const auto dem=pop.demographic_profile({
        "R",23650,phaseb::Sex::female,"culture","faith","citizen",
        "free",pop.default_health(),phaseb::Service::civilian});
    pop.seed_population(dem,worker,hh,100'000);
    c.seed_public_institution("WELFARE","SERVICES","R","food",100,1);
    c.seed_p1_institution("A","WELFARE",P1InstitutionKind::welfare,2);
    c.seed_public_institution("POLICE","SERVICES","R","paper",100,1);
    c.seed_p1_institution("A","POLICE",P1InstitutionKind::policing,1);
    law(c,"A","welfare","poor_laws");
    if(amendment_id=="amendment_religious_workhouses")
        law(c,"A","health_system","charitable_health_system");
    else
        law(c,"A","labor_rights","regulatory_bodies");
    auto world=daily::World::adopt(std::move(gh),42);
    const auto state=[&]() -> const IntegratedWorld& {
        return world.engine().base().core().core();
    };
    check(state().p1_bureaucracy("A").institution_cost==30,
          "base welfare (20) and police (10) must be charged normally");
    phasegh::World::Plan enact;
    const auto start=world.day_index();
    enact[9].push_back([&](phasegh::Stage& stage) {
        auto& live=stage.phase_c();
        P2Bill bill;
        bill.id="WORKHOUSE-BILL";
        bill.country="A";bill.proposer="PRIME";bill.legal_basis="test:workhouse";
        bill.action=P2BillAction::attach_amendment;
        bill.effective_day=start+2;
        bill.proposed_amendment={amendment_id,"A:welfare",
            "test:workhouse","",{{"welfare-level","WELFARE",
                P2ClauseKind::public_institution_level,2}}};
        live.p2_submit_bill(std::move(bill),start);
        live.p2_approve_bill("WORKHOUSE-BILL",{});
        live.p2_promulgate_bill("WORKHOUSE-BILL","HEAD",start);
    });
    world.advance_day(enact);
    world.advance_day();
    check(!state().p2_amendment_active(amendment_id)&&
          state().p1_bureaucracy("A").institution_cost==30,
          "not-yet-effective workhouse amendment changed bureaucracy");
    world.advance_day();
    static const std::string key=
        "country_institution_cost_institution_social_security_mult";
    check(state().p2_amendment_active(amendment_id)&&
          state().p2_modifier_ppm("A",key)==-100'000,
          "canonical amendment modifier did not become effective");
    check(state().p1_bureaucracy("A").institution_cost==28,
          "effective workhouse law must lower welfare cost to 18 and leave police at 10");
    check(politics::wired_law_effects().at(key)==
              "phasec::IntegratedWorld::p1_bureaucracy",
          "welfare cost consumer missing from law coverage audit");
}

static daily::World mortality_fixture(
        const std::string& health_law,int legal_level,bool funded,
        bool staffed,bool blasphemy=false,bool empty_scope=false,
        bool staff_outside_clinic=false) {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    const std::vector<std::string> regions={"CLINIC","CORE","DEPENDENCY","UNKNOWN"};
    geo.seed_unit(1,"A","ADMIN");
    for(std::size_t i=0;i<regions.size();++i) {
        geo.seed_province(i+1,regions[i],"A");
        geo.seed_assignment(i+1,1);
    }
    auto& c=geo.seed_core().seed_core();
    c.seed_actor("A",funded?100:0);
    c.seed_actor("HOSPITAL",funded?100:0);
    c.seed_government("A","HEAD","PRIME",5,3);
    c.seed_p1_constitution("A",{P1GovernancePrinciple::theocracy,
        P1PowerDistribution::autocracy,P1LegislatureKind::none,"scenario:law"});
    c.p1_set_incorporation("CLINIC",true);
    c.p1_set_incorporation("CORE",true);
    c.p1_set_incorporation("DEPENDENCY",false);
    auto& pop=c.seed_population();
    for(const auto& region:regions) {
        const auto h=pop.household_profile({region,"mixed","shared",25'000});
        const auto e=pop.economic_profile({region,"worker","wage","employee",
                                         pop.default_education()});
        const auto d=pop.demographic_profile({region,23650,phaseb::Sex::male,
            "culture","faith","citizen","free",pop.default_health(),
            phaseb::Service::civilian});
        const auto slice=pop.seed_population(d,e,h,100'000);
        if(staffed&&region==(staff_outside_clinic?"CORE":"CLINIC"))
            pop.hire(slice,10,"HOSPITAL","worker",10,{});
    }
    law(c,"A","governance_principles","theocracy");
    law(c,"A","church_and_state","state_religion");
    if(!health_law.empty()) {
        if(empty_scope)
            c.p2_seed_base({"A:health_system","A","lawgroup_health_system","",
                "law_"+health_law,"scenario:constitution"});
        else law(c,"A","health_system",health_law);
    }
    if(legal_level>=0) {
        c.seed_public_institution("HEALTH","HOSPITAL","CLINIC","medicine",50,5);
        c.seed_p1_institution("A","HEALTH",P1InstitutionKind::healthcare,5);
        if(health_law!="")
            c.p2_seed_amendment({"effective-health-funding","A:health_system",
                "scenario:budget","",{{"approved-health-level","HEALTH",
                    P2ClauseKind::public_institution_level,legal_level}}});
    }
    if(blasphemy)
        c.p2_seed_amendment({"amendment_draconian_blasphemy_laws",
            "A:church_and_state","scenario:law","",{{"civil-law","A",
                P2ClauseKind::political_association_allowed,1}}});
    phaseb::DemographyRules rates;
    rates.fertility_ppm=0;
    rates.adult_death_ppm=310'000; // January: exactly 10,000 ppm/day.
    rates.illness_incidence_ppm=0;
    rates.accident_ppm=0;
    rates.migration_ppm=0;
    c.enable_demography(rates);
    return daily::World::adopt(std::move(gh),42);
}
static phaseb::People residents(const daily::World& world,const std::string& region) {
    const auto& pop=world.engine().base().core().core().people();
    phaseb::People total=0;
    for(const auto& [id,slice]:pop.slices()) {
        (void)id;
        if(pop.demographic(slice.key.demographic).region==region)
            total+=slice.count;
    }
    return total;
}
static void childrens_rights_changes_only_exact_profession_mortality() {
    const auto run=[](const std::string& choice,bool stacked,
                      const std::array<phaseb::People,7>& expected) {
        phasegh::World gh(24000);
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"HOME","A");geo.seed_province(2,"FOREIGN","B");
        geo.seed_unit(1,"A","ROOT");geo.seed_unit(2,"B","ROOT");
        geo.seed_assignment(1,1);geo.seed_assignment(2,2);
        auto& c=geo.seed_core().seed_core();
        for(const auto* owner:{"A","B"}) {
            c.seed_actor(owner,100);
            c.seed_government(owner,"HEAD","PRIME",5,3);
            c.seed_p1_constitution(owner,{P1GovernancePrinciple::theocracy,
                P1PowerDistribution::autocracy,P1LegislatureKind::none,"scenario:law"});
        }
        c.seed_actor("EMPLOYER",101);
        if(!choice.empty())law(c,"A","childrens_rights",choice);
        if(stacked) {
            law(c,"A","governance_principles","theocracy");
            law(c,"A","church_and_state","state_religion");
            c.p2_seed_amendment({"amendment_draconian_blasphemy_laws",
                "A:church_and_state","scenario:law","",{{"civil-law","A",
                    P2ClauseKind::political_association_allowed,1}}});
        }
        const std::array<std::string,7> professions{
            "laborer","machinist","farmer","peasant","clerk","worker","labourer"};
        auto& pop=c.seed_population();
        for(const auto* region:{"HOME","FOREIGN"}) {
            const auto d=pop.demographic_profile({region,23650,phaseb::Sex::male,
                "culture","faith","citizen","free",pop.default_health(),
                phaseb::Service::civilian});
            const auto h=pop.household_profile({region,"mixed","shared",25'000});
            for(const auto& profession:professions) {
                const auto e=pop.economic_profile({region,profession,"wage","employee",
                                                   pop.default_education()});
                const auto slice=pop.seed_population(d,e,h,100'000);
                pop.hire(slice,1,"EMPLOYER",profession,1,{});
            }
        }
        phaseb::DemographyRules rules;
        rules.fertility_ppm=0;rules.adult_death_ppm=310'000;
        rules.illness_incidence_ppm=0;rules.accident_ppm=0;rules.migration_ppm=0;
        c.enable_demography(rules);
        auto world=daily::World::adopt(std::move(gh),43);
        world.advance_day();
        const auto& live=world.engine().base().core().core();
        const auto& people=live.people();
        for(const auto* region:{"HOME","FOREIGN"}) {
            std::size_t seen=0;
            for(const auto& [id,slice]:people.slices()) {
                if(people.demographic(slice.key.demographic).region!=region)continue;
                const auto& profession=people.economic(slice.key.economic).profession;
                const auto found=std::find(professions.begin(),professions.end(),profession);
                check(found!=professions.end(),"mortality invented/reassigned profession");
                const auto idx=static_cast<std::size_t>(found-professions.begin());
                const auto deaths=std::string_view(region)=="HOME"?expected[idx]:1000;
                check(slice.count==100'000-deaths && people.employed(id)==1,
                      "occupation or jurisdiction mortality differs from exact effective law");
                ++seen;
            }
            check(seen==professions.size(),"occupation test lost a real POP cohort");
        }
        check(live.cash("EMPLOYER")==101 && live.cash("A")==100 && live.cash("B")==100,
              "mortality changed employer or treasury cash");
    };
    run("",false,{1000,1000,1000,1000,1000,1000,1000});
    run("child_labor_allowed",false,{1050,1050,1050,1050,1000,1000,1000});
    run("restricted_child_labor",false,{1020,1000,1020,1020,1000,1000,1000});
    run("child_labor_allowed",true,{1070,1070,1070,1070,1020,1020,1020});
}

static void effective_health_institution_and_flat_law_change_daily_mortality() {
    const auto fixture=[](const std::string& law,int level,bool funded,bool staffed,
                          bool flat,phaseb::People expected_core,
                          phaseb::People expected_outside) {
        auto world=mortality_fixture(law,level,funded,staffed,flat);
        world.advance_day();
        check(residents(world,"DEPENDENCY")==100'000-expected_outside &&
              residents(world,"UNKNOWN")==100'000-expected_outside,
              "health institution leaked outside known incorporated territory or flat law was lost");
        check(residents(world,"CLINIC")==100'000-expected_core &&
              residents(world,"CORE")==100'000-expected_core,
              "effective national health law did not change actual mortality at its legal level");
    };
    fixture("no_health_system",2,true,true,false,1000,1000);
    fixture("charitable_health_system",2,true,true,false,940,1000);
    fixture("public_health_insurance",2,true,true,false,900,1000);
    fixture("public_health_insurance",0,true,true,false,1000,1000);
    fixture("public_health_insurance",2,false,true,false,1000,1000);
    fixture("public_health_insurance",2,true,false,false,1000,1000);
    fixture("public_health_insurance",-1,true,true,false,1000,1000);
    fixture("",-1,false,false,true,1020,1020);
    fixture("public_health_insurance",2,true,true,true,920,1020);
    auto implicit_national=mortality_fixture("public_health_insurance",2,
        true,true,false,true);
    implicit_national.advance_day();
    check(residents(implicit_national,"CORE")==99'100,
          "supported empty national scope lost healthcare mortality benefit");
    auto nonlocal_staff=mortality_fixture("public_health_insurance",2,
        true,true,false,false,true);
    nonlocal_staff.advance_day();
    check(residents(nonlocal_staff,"CORE")==99'000 &&
          residents(nonlocal_staff,"CLINIC")==99'000,
          "a clinic without local effective staff certified national healthcare");

    auto world=mortality_fixture("",-1,false,false);
    const auto start=world.day_index();
    phasegh::World::Plan enact;
    enact[9].push_back([start](phasegh::Stage& stage) {
        auto& c=stage.phase_c();
        P2Bill bill;
        bill.id="BLASPHEMY";bill.country="A";bill.proposer="PRIME";
        bill.legal_basis="scenario:act";bill.action=P2BillAction::attach_amendment;
        bill.effective_day=start+2;
        bill.proposed_amendment={"amendment_draconian_blasphemy_laws",
            "A:church_and_state","scenario:act","",{{"civil-law","A",
                P2ClauseKind::political_association_allowed,1}}};
        c.p2_submit_bill(std::move(bill),start);
        c.p2_approve_bill("BLASPHEMY",{});
        c.p2_promulgate_bill("BLASPHEMY","HEAD",start);
    });
    world.advance_day(enact);
    world.advance_day();
    check(residents(world,"UNKNOWN")==98'010,
          "pending flat amendment changed mortality before legal effect");
    auto replay=daily::SnapshotCodec::decode(daily::SnapshotCodec::encode(world));
    world.advance_day();replay.advance_day();
    check(residents(world,"UNKNOWN")==97'011 &&
          daily::SnapshotCodec::encode(world)==daily::SnapshotCodec::encode(replay),
          "dated flat mortality effect did not start on actual effective day or survive replay");
}

static void enacted_education_changes_real_daily_schooling() {
    auto scenario=[](const std::string& law_choice,int legal_level,
                     bool funded,const std::string& staff_region,
                     phaseb::People expected_core,phaseb::People expected_other) {
        phasegh::World gh(24000);
        auto& geo=gh.seed_geography();
        const std::vector<std::string> regions={"SCHOOL","CORE","DEPENDENCY","UNKNOWN"};
        geo.seed_unit(1,"A","ADMIN");
        for(std::size_t i=0;i<regions.size();++i) {
            geo.seed_province(i+1,regions[i],"A");
            geo.seed_assignment(i+1,1);
        }
        auto& c=geo.seed_core().seed_core();
        c.seed_actor("A",funded?100:0);
        c.seed_actor("TEACHERS",funded?100:0);
        c.seed_government("A","HEAD","PRIME",5,3);
        c.seed_p1_constitution("A",{P1GovernancePrinciple::monarchy,
            P1PowerDistribution::autocracy,P1LegislatureKind::none,"scenario:school"});
        c.p1_set_incorporation("SCHOOL",true);
        c.p1_set_incorporation("CORE",true);
        c.p1_set_incorporation("DEPENDENCY",false);
        auto& pop=c.seed_population();
        for(const auto& region:regions) {
            const auto h=pop.household_profile({region,"mixed","shared",25'000});
            const auto e=pop.economic_profile({region,"not_assigned","inactive",
                                              "ineligible",pop.default_education()});
            const auto d=pop.demographic_profile({region,23904,phaseb::Sex::male,
                "culture","faith","citizen","free",pop.default_health(),
                phaseb::Service::civilian});
            pop.seed_population(d,e,h,100'000);
            if(region==staff_region) {
                const auto paid=pop.economic_profile({region,"teacher","wage","employee",
                                                     pop.default_education()});
                const auto adult=pop.demographic_profile({region,23651,phaseb::Sex::male,
                    "culture","faith","citizen","free",pop.default_health(),
                    phaseb::Service::civilian});
                const auto staff=pop.seed_population(adult,paid,h,10);
                pop.hire(staff,10,"TEACHERS","teacher",10,{});
            }
        }
        law(c,"A","education_system",law_choice);
        if(legal_level>=0) {
            c.seed_public_institution("SCHOOLS","TEACHERS","SCHOOL","paper",50,5);
            c.seed_p1_institution("A","SCHOOLS",P1InstitutionKind::education,5);
            c.p2_seed_amendment({"effective-school-budget","A:education_system",
                "scenario:budget","",{{"approved-school-level","SCHOOLS",
                    P2ClauseKind::public_institution_level,legal_level}}});
        }
        phaseb::DemographyRules rates;
        rates.fertility_ppm=0;
        rates.child_death_ppm=0;
        rates.adult_death_ppm=0;
        rates.illness_incidence_ppm=0;
        rates.accident_ppm=0;
        rates.training_ppm=0;
        rates.primary_access_ppm=310'000;
        c.enable_demography(rates);
        auto world=daily::World::adopt(std::move(gh),43);
        auto replay=daily::SnapshotCodec::decode(daily::SnapshotCodec::encode(world));
        world.advance_day();
        replay.advance_day();
        check(daily::SnapshotCodec::encode(world)==daily::SnapshotCodec::encode(replay),
              "education law hazard was not reproducible after reload");
        const auto& people=world.engine().base().core().core().people();
        std::map<std::string,phaseb::People> primary;
        for(const auto& [id,row]:people.slices()) {
            (void)id;
            const auto& dem=people.demographic(row.key.demographic);
            const auto& economic=people.economic(row.key.economic);
            if(dem.birth_month==23904&&
               people.education(economic.education).school==phaseb::School::primary)
                primary[dem.region]+=row.count;
        }
        check(primary["DEPENDENCY"]==expected_other&&primary["UNKNOWN"]==expected_other,
              "education institution leaked outside incorporated areas or lost existing flat access");
        check(primary["SCHOOL"]==expected_core&&primary["CORE"]==expected_core,
              "effective national education law did not advance actual cohorts at its legal level");
    };
    scenario("no_schools",2,true,"SCHOOL",0,0);
    scenario("religious_schools",2,true,"SCHOOL",1645,1000);
    scenario("public_schools",2,true,"SCHOOL",1806,1000);
    scenario("public_schools",0,true,"SCHOOL",1000,1000);
    scenario("public_schools",-1,true,"SCHOOL",1000,1000);
    scenario("public_schools",2,false,"SCHOOL",1000,1000);
    scenario("public_schools",2,true,"",1000,1000);
    scenario("public_schools",2,true,"CORE",1000,1000);
    scenario("terakoya",2,true,"SCHOOL",1806,1806);
}

static void chiefs_aid_uses_only_effective_welfare_bureaucracy() {
    const auto scenario=[](const std::string& choice,P1InstitutionKind kind,
                           const std::string& service_region,
                           const std::string& staff_region,
                           bool funded,bool dated,std::int64_t expected) {
        phasegh::World gh(24000);
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"CORE","A");
        geo.seed_province(2,"DEPENDENCY","A");
        geo.seed_province(3,"FOREIGN","B");
        geo.seed_unit(1,"A","A-ADMIN");geo.seed_unit(2,"B","B-ADMIN");
        geo.seed_assignment(1,1);geo.seed_assignment(2,1);geo.seed_assignment(3,2);
        auto& c=geo.seed_core().seed_core();
        c.seed_actor("A",0);c.seed_actor("B",0);
        c.seed_actor("SERVICES",funded?100:0);
        c.seed_government("A","HEAD","PRIME",5,3);
        c.seed_government("B","HEAD-B","PRIME-B",5,3);
        c.seed_p1_constitution("A",{P1GovernancePrinciple::monarchy,
            P1PowerDistribution::autocracy,P1LegislatureKind::none,
            "fixture:executive"});
        c.seed_p1_constitution("B",{P1GovernancePrinciple::monarchy,
            P1PowerDistribution::autocracy,P1LegislatureKind::none,
            "fixture:foreign-executive"});
        c.p1_set_incorporation("CORE",true);
        c.p1_set_incorporation("DEPENDENCY",false);
        c.p1_set_incorporation("FOREIGN",true);
        auto& pop=c.seed_population();
        for(const auto& region:{"CORE","DEPENDENCY","FOREIGN"}) {
            const auto hh=pop.household_profile({region,"mixed","shared",25'000});
            const auto econ=pop.economic_profile({region,"worker","wage","employee",
                                                 pop.default_education()});
            const auto dem=pop.demographic_profile({region,23650,phaseb::Sex::female,
                "culture","faith","citizen","free",pop.default_health(),
                phaseb::Service::civilian});
            const auto id=pop.seed_population(dem,econ,hh,100'000);
            if(region==staff_region)pop.hire(id,10,"SERVICES","worker",10,{});
        }
        const auto country=service_region=="FOREIGN"?"B":"A";
        c.seed_public_institution("AID-ONE","SERVICES",service_region,"food",100,1);
        c.seed_public_institution("AID-TWO","SERVICES",service_region,"food",100,1);
        c.seed_p1_institution(country,"AID-ONE",kind,1);
        c.seed_p1_institution(country,"AID-TWO",kind,2);
        if(!choice.empty())law(c,"A","welfare",dated?"poor_laws":choice);
        auto world=daily::World::adopt(std::move(gh),42);
        const auto& init=world.engine().base().core().core();
        const auto opening_cost=init.p1_bureaucracy("A").institution_cost;
        if(dated) {
            const auto day=world.day_index();
            phasegh::World::Plan plan;
            plan[9].push_back([&](phasegh::Stage& stage) {
                auto& live=stage.phase_c();
                phasec::P2Bill bill;
                bill.id="CHIEFS-BILL";bill.country="A";bill.proposer="PRIME";
                bill.legal_basis="fixture:chiefs";bill.effective_day=day+2;
                bill.proposed_base={"CHIEFS-LAW","A","lawgroup_welfare","national",
                    "law_chiefs_distribute_aid","fixture:chiefs"};
                live.p2_submit_bill(std::move(bill),day);
                live.p2_approve_bill("CHIEFS-BILL",{});
                live.p2_promulgate_bill("CHIEFS-BILL","HEAD",day);
            });
            world.advance_day(plan);
            world.advance_day();
            check(world.engine().base().core().core().p1_bureaucracy("A")
                      .capacity.generated==100,
                  "pending chiefs law created administrative output early");
            world.advance_day();
        }
        const auto& live=world.engine().base().core().core();
        const auto value=live.p1_bureaucracy("A");
        if(value.capacity.generated!=expected)
            throw std::runtime_error("wrong chiefs aid administration output for "+
                choice+"/"+service_region+" (dated="+std::to_string(dated)+
                "): expected "+std::to_string(expected)+", got "+
                std::to_string(value.capacity.generated));
        check(value.institution_cost==opening_cost &&
              live.p2_modifier_ppm("A","country_bureaucracy_add")==0,
              "institutional output altered nominal cost or leaked into flat modifiers");
    };
    scenario("chiefs_distribute_aid",P1InstitutionKind::welfare,"CORE","CORE",true,false,120);
    scenario("chiefs_distribute_aid",P1InstitutionKind::welfare,"CORE","CORE",true,true,120);
    scenario("poor_laws",P1InstitutionKind::welfare,"CORE","CORE",true,false,100);
    scenario("",P1InstitutionKind::welfare,"CORE","CORE",true,false,100);
    scenario("chiefs_distribute_aid",P1InstitutionKind::welfare,"CORE","",true,false,100);
    scenario("chiefs_distribute_aid",P1InstitutionKind::welfare,"CORE","CORE",false,false,100);
    scenario("chiefs_distribute_aid",P1InstitutionKind::welfare,"CORE","DEPENDENCY",true,false,100);
    scenario("chiefs_distribute_aid",P1InstitutionKind::education,"CORE","CORE",true,false,100);
    scenario("chiefs_distribute_aid",P1InstitutionKind::welfare,"DEPENDENCY","DEPENDENCY",true,false,100);
    scenario("chiefs_distribute_aid",P1InstitutionKind::welfare,"FOREIGN","FOREIGN",true,false,100);
}

static void canonical_named_acceptance_changes_only_verified_pop_and_country() {
    const auto run=[](const std::string& named,const std::string& target,
                      const std::string& citizenship,const std::string& amendment,
                      int before,int after,int invalid_parent=0) {
        phasegh::World gh{24000};
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"SOURCE","ORIGIN");
        geo.seed_province(2,"TARGET",target);
        geo.seed_unit(1,"ORIGIN","SOURCE-UNIT");
        geo.seed_unit(2,target,"TARGET-UNIT");
        geo.seed_assignment(1,1);geo.seed_assignment(2,2);
        auto& c=geo.seed_core().seed_core();
        for(const auto& tag:{std::string("ORIGIN"),target}) {
            c.seed_actor(tag,0);
            c.seed_government(tag,"HEAD","PRIME",5,3);
            c.seed_p1_constitution(tag,{P1GovernancePrinciple::monarchy,
                P1PowerDistribution::autocracy,P1LegislatureKind::none,
                "fixture:constitution"});
            law(c,tag,"citizenship",citizenship);
            law(c,tag,"migration",tag==target?"migration_controls":"no_migration_controls");
        }
        const std::string heritage=named=="basque"?"PRIMARY_H":"OTHER_H";
        c.p2_seed_culture({"primary",{"PRIMARY_H"},{"PRIMARY_HG"},
                            {"PRIMARY_L"},{"PRIMARY_LG"},{},{}});
        c.p2_seed_culture({named,{heritage},{"SHARED_HG"},{"OTHER_L"},{"SHARED_LG"},{},{}});
        const auto alias=named=="basque"?"Basque":"Polish";
        c.p2_seed_culture({alias,{heritage},{"SHARED_HG"},{"OTHER_L"},{"SHARED_LG"},{},{}});
        c.p2_seed_national_identity({"ORIGIN",{"primary"},{}});
        c.p2_seed_national_identity({target,{"primary"},{}});
        auto& pop=c.seed_population();
        const auto h=pop.household_profile({"SOURCE","mixed","shared",25'000});
        const auto e=pop.economic_profile({"SOURCE","worker","wage","employee",
                                            pop.default_education()});
        const auto seed=[&](const std::string& culture) {
            const auto d=pop.demographic_profile({"SOURCE",23640,phaseb::Sex::male,
                culture,"faith","citizen","free",pop.default_health(),
                phaseb::Service::civilian});
            return pop.seed_population(d,e,h,100'000);
        };
        const Id named_slice=seed(named), alias_slice=seed(alias), unknown=seed("unknown");
        phaseb::DemographyRules rules;
        rules.fertility_ppm=rules.infant_death_ppm=rules.child_death_ppm=0;
        rules.adult_death_ppm=rules.senior_death_ppm=rules.very_old_death_ppm=0;
        rules.illness_incidence_ppm=rules.accident_ppm=0;
        rules.primary_access_ppm=rules.secondary_access_ppm=rules.tertiary_access_ppm=0;
        rules.adult_literacy_access_ppm=rules.training_ppm=0;
        rules.migration_ppm=310'000;
        rules.destination_by_region["SOURCE"]="TARGET";
        c.enable_demography(rules);
        std::string parent=target+":citizenship";
        if(invalid_parent==1) {
            P2BaseLaw local;
            local.id="LOCAL";local.country=target;local.domain="regional_rules";
            local.scope="TARGET";local.tier=P2NormTier::local_ordinance;
            local.base_choice="local_rule";local.legal_basis="fixture:local";
            c.p2_seed_base(local);
            parent=local.id;
        } else if(invalid_parent==2)parent=target+":migration";
        const P2Amendment addition{amendment,parent,"fixture:amendment","",
            {{"verified-amendment-clause",target,
              P2ClauseKind::political_association_allowed,1}}};
        if(invalid_parent)c.p2_seed_amendment(addition);
        auto world=daily::World::adopt(std::move(gh),17);
        const auto check_score=[&](int expected,bool allowed) {
            const auto& live=view(world);
            const auto actual=live.p2_acceptance(target,named_slice);
            const auto other=live.p2_acceptance(target,alias_slice);
            const auto origin=live.p2_acceptance("ORIGIN",named_slice);
            const auto level=expected>=80?P2AcceptanceLevel::full:
                expected>=60?P2AcceptanceLevel::second_class:
                expected>=40?P2AcceptanceLevel::open_prejudice:
                expected>=20?P2AcceptanceLevel::cultural_erasure:
                              P2AcceptanceLevel::violent_hostility;
            const auto score=[](const auto& candidate) {
                return candidate?candidate->score:-1;
            };
            if(!(actual&&actual->score==expected&&actual->level==level&&
                 actual->region=="SOURCE"&&other&&other->score==before&&
                 origin&&origin->score==before&&!live.p2_acceptance(target,unknown)))
                throw std::runtime_error("named culture acceptance scope: "+named+
                    ", expected="+std::to_string(expected)+", target="+
                    std::to_string(score(actual))+", decoy="+std::to_string(score(other))+
                    ", origin="+std::to_string(score(origin))+
                    ", date="+std::to_string(world.day_index()));
            check(live.p2_migration_permitted(named_slice,"TARGET")==allowed&&
                  !live.p2_migration_permitted(alias_slice,"TARGET")&&
                  !live.p2_migration_permitted(unknown,"TARGET"),
                  "named culture acceptance did not feed the actual destination gate");
        };
        const auto arrived=[&] {
            const auto& people=view(world).people();
            phaseb::People count=0;
            for(const auto& [id,row]:people.slices()) {
                (void)id;
                const auto& demo=people.demographic(row.key.demographic);
                if(demo.region!="TARGET")continue;
                check(demo.culture==named,"an ineligible culture crossed the controlled border");
                count+=row.count;
            }
            return count;
        };
        check_score(before,false);
        if(invalid_parent) {
            world.advance_day();
            check(arrived()==0,"incompatible or local parent allowed real migration");
            return;
        }
        const auto enact=[&](bool repeal) {
            const auto start=world.day_index();
            phasegh::World::Plan plan;
            plan[9].push_back([&,start,repeal](phasegh::Stage& stage) {
                auto& live=stage.phase_c();
                P2Bill b;
                b.id=repeal?"REPEAL":"ADOPT";b.country=target;b.proposer="PRIME";
                b.legal_basis="fixture:legislation";b.effective_day=start+2;
                b.action=repeal?P2BillAction::repeal_amendment:P2BillAction::attach_amendment;
                if(repeal)b.target_id=amendment;
                else b.proposed_amendment=addition;
                const auto id=b.id;
                live.p2_submit_bill(std::move(b),start);
                live.p2_approve_bill(id,{});
                live.p2_promulgate_bill(id,"HEAD",start);
            });
            world.advance_day(plan);
            world.advance_day();
        };
        enact(false);
        check_score(before,false);
        check(arrived()==0,"pending law changed migration before effective day");
        world.advance_day();
        check_score(after,true);
        check(arrived()==1000,"the named eligible POP did not really cross on effective day");
        const auto bytes=daily::SnapshotCodec::encode(world);
        world=daily::SnapshotCodec::decode(bytes);
        check_score(after,true);
        enact(true);
        const auto before_repeal=arrived();
        world.advance_day();
        check_score(before,false);
        check(arrived()==before_repeal&&view(world).people().population()==300'000,
              "repealed enactment still moved people or violated POP balance");
    };
    run("basque","ESP","subjecthood","amendment_reinstated_fueros",20,40);
    run("polish","HAI","cultural_exclusion","amendment_haiti_polish_naturalization",20,80);
    run("basque","ESP","subjecthood","amendment_reinstated_fueros",20,40,1);
    run("polish","HAI","cultural_exclusion","amendment_haiti_polish_naturalization",20,80,2);
}

static void no_colonial_affairs_adds_only_verified_homeland_acceptance() {
    phasegh::World gh{24000};
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"SOURCE","ORIGIN");
    geo.seed_province(2,"TARGET","DEST");
    geo.seed_province(3,"OTHER","ORIGIN");
    geo.seed_unit(1,"ORIGIN","HOME");
    geo.seed_unit(2,"DEST","DEST-UNIT");
    geo.seed_assignment(1,1);geo.seed_assignment(2,2);geo.seed_assignment(3,1);
    auto& c=geo.seed_core().seed_core();
    for(const auto* tag:{"ORIGIN","DEST"}) {
        c.seed_actor(tag,0);
        c.seed_government(tag,"HEAD","PRIME",5,3);
        c.seed_p1_constitution(tag,{P1GovernancePrinciple::monarchy,
            P1PowerDistribution::autocracy,P1LegislatureKind::none,
            "fixture:constitution"});
        law(c,tag,"citizenship","subjecthood");
        law(c,tag,"colonization","colonial_exploitation");
        law(c,tag,"migration",std::string(tag)=="DEST"?
                            "migration_controls":"no_migration_controls");
    }
    c.p2_seed_culture({"primary",{"primary-H"},{"primary-HG"},
                        {"primary-L"},{"primary-LG"},{},{}});
    c.p2_seed_culture({"homeland",{"H"},{"HG"},{"L"},{"LG"},{},{"SOURCE"}});
    c.p2_seed_culture({"elsewhere",{"H"},{"HG"},{"L"},{"LG"},{},{"OTHER"}});
    c.p2_seed_culture({"unverified-home",{"H"},{"HG"},{"L"},{"LG"},{},{}});
    c.p2_seed_national_identity({"ORIGIN",{"primary"},{}});
    c.p2_seed_national_identity({"DEST",{"primary"},{}});
    auto& pop=c.seed_population();
    const auto h=pop.household_profile({"SOURCE","mixed","shared",100'000});
    const auto e=pop.economic_profile({"SOURCE","worker","wage","employee",
                                       pop.default_education()});
    const auto seed=[&](const std::string& culture) {
        const auto d=pop.demographic_profile({"SOURCE",23640,phaseb::Sex::male,
            culture,"faith","citizen","free",pop.default_health(),
            phaseb::Service::civilian});
        return pop.seed_population(d,e,h,100'000);
    };
    const auto home=seed("homeland"), other=seed("elsewhere");
    const auto no_home=seed("unverified-home"), unknown=seed("missing-traits");
    phaseb::DemographyRules rules;
    rules.fertility_ppm=rules.infant_death_ppm=rules.child_death_ppm=0;
    rules.adult_death_ppm=rules.senior_death_ppm=rules.very_old_death_ppm=0;
    rules.illness_incidence_ppm=rules.accident_ppm=0;
    rules.primary_access_ppm=rules.secondary_access_ppm=rules.tertiary_access_ppm=0;
    rules.adult_literacy_access_ppm=rules.training_ppm=0;
    rules.migration_ppm=310'000;
    rules.destination_by_region["SOURCE"]="TARGET";
    c.enable_demography(rules);
    auto world=daily::World::adopt(std::move(gh),21);
    const auto snapshot_counts=[&](int expected,bool allowed) {
        const auto& live=view(world);
        const auto score=[&](const std::string& tag,Id id) {
            const auto evaluated=live.p2_acceptance(tag,id);
            return evaluated?evaluated->score:-1;
        };
        check(score("DEST",home)==expected&&score("ORIGIN",home)==30&&
              score("DEST",other)==0&&score("DEST",no_home)==0&&
              score("DEST",unknown)==-1,
              "colonization law did not add only verified homeland points in its own country");
        check(live.p2_migration_permitted(home,"TARGET")==allowed&&
              !live.p2_migration_permitted(other,"TARGET")&&
              !live.p2_migration_permitted(no_home,"TARGET")&&
              !live.p2_migration_permitted(unknown,"TARGET"),
              "colonization acceptance did not preserve actual migration control scope");
    };
    const auto arrived=[&] {
        phaseb::People count=0;
        const auto& people=view(world).people();
        for(const auto& [id,row]:people.slices()) {
            (void)id;
            const auto& dem=people.demographic(row.key.demographic);
            if(dem.region!="TARGET")continue;
            check(dem.culture=="homeland","an unverified homeland passed migration control");
            count+=row.count;
        }
        return count;
    };
    const auto replace=[&](const std::string& choice,const std::string& id) {
        const auto start=world.day_index();
        phasegh::World::Plan act;
        act[9].push_back([&](phasegh::Stage& stage) {
            auto& live=stage.phase_c();
            P2Bill bill;
            bill.id=id;bill.country="DEST";bill.proposer="PRIME";
            bill.legal_basis="fixture:enactment";bill.effective_day=start+2;
            bill.action=P2BillAction::enact_or_replace_base;
            bill.proposed_base.id=id+":LAW";bill.proposed_base.country="DEST";
            bill.proposed_base.domain="lawgroup_colonization";
            bill.proposed_base.scope="national";bill.proposed_base.base_choice=choice;
            bill.proposed_base.legal_basis="fixture:colonization";
            live.p2_submit_bill(std::move(bill),start);
            live.p2_approve_bill(id,{});
            live.p2_promulgate_bill(id,"HEAD",start);
        });
        world.advance_day(act);
        world.advance_day();
    };
    snapshot_counts(30,false);
    replace("law_no_colonial_affairs","ADOPT");
    snapshot_counts(30,false);
    check(arrived()==0,"pending colonization law affected movement early");
    world.advance_day();
    snapshot_counts(40,true);
    check(arrived()==1000,"verified homeland POP did not actually move under the enacted law");
    const auto saved=daily::SnapshotCodec::encode(world);
    world=daily::SnapshotCodec::decode(saved);
    snapshot_counts(40,true);
    replace("law_colonial_exploitation","REPLACE");
    const auto previous=arrived();
    world.advance_day();
    snapshot_counts(30,false);
    check(arrived()==previous&&view(world).people().population()==400'000,
          "replaced colonization law retained a stale effect on real migration");
}

static void land_reform_restricts_only_peasant_internal_migration() {
    const auto run=[](const std::string& choice,bool domestic,bool restricted,
                      const std::string& borders="no_migration_controls") {
        const std::string target=domestic?"A":"B";
        phasegh::World gh{24000};
        auto& geo=gh.seed_geography();
        geo.set_dataset("peasant-internal-migration-fixture");
        geo.seed_province(1,"SOURCE","A");
        geo.seed_province(2,"TARGET",target);
        geo.seed_unit(1,"A","SOURCE-UNIT");
        geo.seed_unit(2,target,"TARGET-UNIT");
        geo.seed_assignment(1,1);geo.seed_assignment(2,2);
        auto& c=geo.seed_core().seed_core();
        c.seed_actor("A",50);c.seed_actor("HOUSE",0);
        c.seed_government("A","HEAD","PRIME",5,3);
        const auto constitution=[&](const std::string& tag) {
            c.seed_p1_constitution(tag,{P1GovernancePrinciple::monarchy,
                P1PowerDistribution::autocracy,P1LegislatureKind::none,
                "fixture:constitution"});
        };
        constitution("A");
        if(!domestic) {
            c.seed_actor("B",0);
            c.seed_government("B","HEAD","PRIME",5,3);
            constitution("B");
            law(c,"B","migration",borders);
            law(c,"B","land_reform","tenant_farmers");
        } else law(c,"A","migration",borders);
        if(!choice.empty())law(c,"A","land_reform",choice);
        auto& pop=c.seed_population();
        const auto home=pop.household_profile({"SOURCE","mixed","shared",50});
        const auto away=pop.household_profile({"TARGET","mixed","shared",0});
        const auto dem=pop.demographic_profile({"SOURCE",23640,phaseb::Sex::male,
            "culture","faith","citizen","free",pop.default_health(),
            phaseb::Service::civilian});
        const auto seed=[&](const std::string& profession) {
            const auto economic=pop.economic_profile({"SOURCE",profession,"wage",
                "employee",pop.default_education()});
            return pop.seed_population(dem,economic,home,100'000);
        };
        const auto peasant=seed("peasant"),worker=seed("worker");
        phaseb::DemographyRules rules;
        rules.fertility_ppm=rules.infant_death_ppm=rules.child_death_ppm=0;
        rules.adult_death_ppm=rules.senior_death_ppm=rules.very_old_death_ppm=0;
        rules.illness_incidence_ppm=rules.accident_ppm=0;
        rules.primary_access_ppm=rules.secondary_access_ppm=rules.tertiary_access_ppm=0;
        rules.adult_literacy_access_ppm=rules.training_ppm=0;
        rules.migration_ppm=310'000;
        rules.destination_by_region["SOURCE"]="TARGET";
        c.enable_demography(rules);
        auto world=daily::World::adopt(std::move(gh),19);
        const auto start=daily::SnapshotCodec::encode(world);
        const bool closed=!domestic&&borders=="closed_borders";
        check(view(world).p2_migration_permitted(peasant,"SOURCE"),
              "same-region movement is not internal migration");
        phasegh::World::Plan explicit_move;
        explicit_move[10].push_back([=](phasegh::Stage& stage) {
            stage.phase_c().migrate_with_explicit_property_allocation(
                peasant,7,"TARGET",away,"A","HOUSE",11,"",0);
        });
        if(restricted||closed) {
            check(rejects([&]{world.advance_day(explicit_move);})&&
                  daily::SnapshotCodec::encode(world)==start,
                  choice=="serfdom"?
                    "Serfdom did not atomically block free peasants moving domestically":
                    "Tenant Farmers or ordinary closed border did not atomically block movement");
        } else {
            auto explicit_world=daily::SnapshotCodec::decode(start);
            explicit_world.advance_day(explicit_move);
            check(view(explicit_world).cash("A")==39&&
                  view(explicit_world).cash("HOUSE")==11,
                  "allowed explicit peasant migration failed to move actual property");
        }
        check(view(world).p2_migration_permitted(worker,"TARGET")==!closed,
              "land reform restricted a non-peasant or bypassed an ordinary closed border");
        world.advance_day();
        People peasants=0,workers=0;
        const auto& people=view(world).people();
        for(const auto& [id,row]:people.slices()) {
            if(people.demographic(row.key.demographic).region!="TARGET")continue;
            if(people.economic(row.key.economic).profession=="peasant")peasants+=row.count;
            else workers+=row.count;
        }
        check(peasants==((restricted||closed)?0:1000)&&
              workers==(closed?0:1000)&&people.population()==200'000,
              "daily peasant/internal law did not change actual saved POP transfer only");
    };
    run("serfdom",true,true);
    run("tenant_farmers",true,true);
    run("peasant_proprietorship",true,false,"closed_borders");
    run("",true,false);
    run("serfdom",false,false);
    run("tenant_farmers",false,false);
    run("tenant_farmers",false,false,"closed_borders");
}

int main(){
    try{
        chiefs_aid_uses_only_effective_welfare_bureaucracy();
        source_ideologies_laws_traits();
        no_colonial_affairs_adds_only_verified_homeland_acceptance();
        canonical_named_acceptance_changes_only_verified_pop_and_country();
        acceptance_and_five_tier_rights();
        physical_migration_and_law_block();
        land_reform_restricts_only_peasant_internal_migration();
        abolition_preserves_real_people_and_membership();
        economic_law_restricts_real_company_ownership();
        military_peasant_levy_restricts_real_conscripts();
        effective_workhouse_amendment_reduces_only_welfare_cost(
            "amendment_religious_workhouses");
        effective_workhouse_amendment_reduces_only_welfare_cost(
            "amendment_workhouse_regulations");
        effective_health_institution_and_flat_law_change_daily_mortality();
        childrens_rights_changes_only_exact_profession_mortality();
        enacted_education_changes_real_daily_schooling();
        std::cout<<"VIC2.5 POLICY IDENTITY AND REFERENCE DATA PASS\n";
    }catch(const std::exception& e){
        std::cerr<<"POLITICS IDENTITY FAILURE: "<<e.what()<<"\n";
        return 1;
    }
}
