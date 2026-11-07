#include "vic25/daily_snapshot.hpp"
#include "vic25/p0_daily.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace vic25;
using namespace vic25::phasec;

static void need(bool good,const char* error) {
    if(!good)throw std::runtime_error(error);
}
static daily::DayIndex d(daily::Date date) {
    return daily::to_day_index(date);
}
static const IntegratedWorld& c(const daily::World& world) {
    return world.engine().base().core().core();
}
template<class Callback>static bool rejected(Callback&& callback) {
    try{callback();}catch(const std::exception&){return true;}
    return false;
}

struct Scenario {
    phasegh::World gh{24000};
    Id civilians{},irregulars{},loyal{};
    explicit Scenario(bool national_crisis=false) {
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"R","A");
        geo.seed_unit(100,"A","District");
        geo.seed_assignment(1,100);
        auto& core=geo.seed_core().seed_core();
        core.seed_actor("A",500);
        core.seed_actor("SELLER",100);
        core.seed_actor("BUYER",1000);
        core.seed_government("A","HEAD","PRIME",5,3);
        core.seed_p1_constitution("A",{
            P1GovernancePrinciple::presidential_republic,
            P1PowerDistribution::universal_vote,
            P1LegislatureKind::unicameral,"constitution:A"});
        core.seed_interest_group("RURAL");
        core.p4_register_party({"REFORM","A","HEAD_CANDIDATE",
                                100'000,true,true});
        core.p3_seed_group({
            "A","RURAL","","regional_reform","adopted",
            P3GroupType::rural_folk,500'000,0});
        core.p3_seed_movement({
            "MOV","A","regional_reform","adopted","R",
            national_crisis?P3MovementGoal::replace_government:
                            P3MovementGoal::legal_reform,
            {"RURAL"},{"REFORM"},1'000'000,1'000'000,true});
        core.seed_market("R","grain",10,5,20);
        core.seed_stock("SELLER","R","grain",100);
        auto& pop=core.seed_population();
        const auto hh=pop.household_profile({"R","shared","shared",100});
        const auto civilian_econ=pop.economic_profile({
            "R","worker","wage","employee",pop.default_education()});
        const auto soldier_econ=pop.economic_profile({
            "R","not_assigned","inactive","ineligible",pop.default_education()});
        const auto civilians_dem=pop.demographic_profile({
            "R",24000-360,phaseb::Sex::female,
            "culture","faith","citizen","free",
            pop.default_health(),phaseb::Service::civilian});
        const auto soldiers_dem=pop.demographic_profile({
            "R",24000-360,phaseb::Sex::male,
            "culture","faith","citizen","free",
            pop.default_health(),phaseb::Service::active});
        civilians=pop.seed_population(civilians_dem,civilian_econ,hh,100,1);
        irregulars=pop.seed_population(soldiers_dem,soldier_econ,hh,5,2);
        loyal=pop.seed_population(soldiers_dem,soldier_econ,hh,5,3);
        core.p3_set_stance(civilians,"regional_reform",{900'000,900'000});
        core.seed_unit("IRREGULAR");
        core.seed_unit("REGULAR");
        core.seed_active("IRREGULAR",irregulars);
        core.seed_active("REGULAR",loyal);
        core.seed_army_deployment("IRREGULAR","A","R");
        core.seed_army_deployment("REGULAR","A","R");
        core.seed_stock("IRREGULAR","R","rifle",20);
        core.seed_stock("IRREGULAR","R","ammunition",20);
        core.seed_stock("REGULAR","R","rifle",20);
        core.seed_stock("REGULAR","R","ammunition",20);
        gh.validate();
    }
};
static P5GuerrillaCampaign regional_campaign() {
    P5GuerrillaCampaign journal;
    journal.id="G";
    journal.country="A";
    journal.movement="MOV";
    journal.regions={"R"};
    journal.units={"IRREGULAR"};
    journal.started_day=d({2000,1,1});
    return journal;
}
static void begin(phasegh::Stage& gh,Id civilians) {
    auto& core=gh.phase_c();
    core.join_interest_group(civilians,"RURAL",40);
    core.p5_begin_guerrilla(regional_campaign());
}
static void physical_unit_and_regional_support_are_mandatory() {
    Scenario fixture;
    auto world=daily::World::adopt(std::move(fixture.gh),701);
    const auto original=daily::SnapshotCodec::encode(world);
    phasegh::World::Plan impossible;
    impossible[9].push_back([id=fixture.civilians](phasegh::Stage& gh){
        gh.phase_c().join_interest_group(id,"RURAL",40);
        auto campaign=regional_campaign();
        campaign.units={"PHANTOM_ARMY"};
        gh.phase_c().p5_begin_guerrilla(campaign);
    });
    need(rejected([&]{world.advance_day(impossible);})&&
         daily::SnapshotCodec::encode(world)==original,
         "P5-G must reject and roll back fictitious irregular units");
    phasegh::World::Plan actual;
    actual[9].push_back([id=fixture.civilians](phasegh::Stage& gh){
        begin(gh,id);
    });
    world.advance_day(actual);
    const auto region=c(world).p5_guerrilla_region_view("G","R");
    need(region.sovereign=="A"&&region.military_controller=="A"&&
         region.irregular_strength==5&&
         region.available_ammunition==20&&
         region.supporters>=40&&region.supporters<=region.population&&
         c(world).p5_guerrilla_campaigns().at("G").stage==
             P5GuerrillaStage::active,
         "P5-G real local POP support, physical unit or sovereignty missing");
    need(c(world).people().population()==110,
         "P5-G cannot mint soldiers or political supporters as new population");
}

static void expenditure_real_market_access_and_checkpoint_replay() {
    Scenario fixture;
    auto world=daily::World::adopt(std::move(fixture.gh),702);
    phasegh::World::Plan open;
    open[9].push_back([id=fixture.civilians](phasegh::Stage& gh){
        begin(gh,id);
    });
    open[10].push_back([](phasegh::Stage& gh){
        gh.phase_c().p5_guerrilla_disrupt(
            "G","G-incident-1","R","IRREGULAR",2,d({2000,1,1}));
    });
    world.advance_day(open);
    need(c(world).stock("IRREGULAR","R","ammunition")==18&&
         c(world).p5_guerrilla_disruption_ppm("R")==0&&
         c(world).sovereignty("R")=="A"&&
         c(world).military_control("R")=="A",
         "P5-G physical expenditure must precede next-day economic effect");
    const auto save=daily::SnapshotCodec::encode(world);
    auto replay=daily::SnapshotCodec::decode(save);
    need(daily::SnapshotCodec::encode(replay)==save,
         "P5-G incident and pending access day must survive save/load");
    const auto trade=[](phasegh::Stage& gh) {
        const auto sold=gh.phase_c().clear_consumer_market(
            "SELLER","BUYER","R","grain",20);
        need(sold==18,"P5-G temporary market access should limit fulfilled goods");
    };
    phasegh::World::Plan sales;
    sales[7].push_back(trade);
    world.advance_day(sales);replay.advance_day(sales); // 2000-01-02
    need(c(world).p5_guerrilla_disruption_ppm("R")==100'000&&
         c(world).stock("BUYER","R","grain")==18&&
         c(world).stock("SELLER","R","grain")==82&&
         daily::SnapshotCodec::encode(world)==
             daily::SnapshotCodec::encode(replay),
         "P5-G must apply the same legal fixed-point market access on replay");
    world.advance_day();world.advance_day(); // Jan3 + Jan4
    world.advance_day(); // Jan5, disruption expires
    need(c(world).p5_guerrilla_disruption_ppm("R")==0&&
         c(world).stock("SELLER","R","grain")==82,
         "P5-G access penalty must expire without deleting unsold real goods");
}

static void local_skirmish_uses_real_existing_armed_POP() {
    Scenario fixture;
    auto world=daily::World::adopt(std::move(fixture.gh),703);
    const auto n=c(world).people().population();
    phasegh::World::Plan clash;
    clash[7].push_back([](phasegh::Stage& gh){
        gh.phase_c().assign_equipment("IRREGULAR","R","rifle",5);
        gh.phase_c().assign_equipment("REGULAR","R","rifle",5);
    });
    clash[9].push_back([id=fixture.civilians](phasegh::Stage& gh){
        begin(gh,id);
    });
    clash[10].push_back([rebel=fixture.irregulars,gov=fixture.loyal]
                        (phasegh::Stage& gh){
        gh.phase_c().p5_guerrilla_skirmish(
            "G","G-skirmish-1","IRREGULAR",rebel,"REGULAR",gov,"R",
            CombatLoss{0,0,0,0,0,1},CombatLoss{0,0,0,0,0,1},
            d({2000,1,1}));
    });
    world.advance_day(clash);
    need(c(world).p5_guerrilla_campaigns().at("G").verified_skirmishes==1&&
         c(world).stock("IRREGULAR","R","ammunition")==19&&
         c(world).stock("REGULAR","R","ammunition")==19&&
         c(world).people().population()==n,
         "P5-G nonfatal skirmish must consume real ammo without ghost casualties");
}

static void conventional_escalation_and_regional_return_share_units() {
    Scenario fixture(true);
    auto world=daily::World::adopt(std::move(fixture.gh),704);
    phasegh::World::Plan start;
    start[9].push_back([id=fixture.civilians](phasegh::Stage& gh){
        begin(gh,id);
    });
    world.advance_day(start);
    while(world.date()<daily::Date{2000,1,8})world.advance_day();
    need(c(world).p5_crises().at("P5:MOV").stage==P5CrisisStage::warning,
         "P5 political warning should mature before conventional war");
    phasegh::World::Plan pledge;
    pledge[9].push_back([](phasegh::Stage& gh){
        gh.phase_c().p5_pledge_army_unit("P5:MOV","IRREGULAR");
    });
    world.advance_day(pledge); // Jan8
    phasegh::World::Plan front;
    front[9].push_back([](phasegh::Stage& gh){
        gh.phase_c().p5_begin_armed_conflict(
            "P5:MOV",d({2000,1,9}));
    });
    front[10].push_back([](phasegh::Stage& gh){
        gh.phase_c().occupy("R","P5:P5:MOV");
        gh.phase_c().p5_confirm_actual_control("P5:MOV","R",d({2000,1,9}));
    });
    world.advance_day(front); // Jan9
    const auto before_strength=c(world).active_strength("IRREGULAR");
    phasegh::World::Plan conventional;
    conventional[9].push_back([](phasegh::Stage& gh){
        gh.phase_c().p5_guerrilla_to_conventional(
            "G","P5:MOV",d({2000,1,10}));
    });
    world.advance_day(conventional); // Jan10
    need(c(world).p5_guerrilla_campaigns().at("G").stage==
             P5GuerrillaStage::conventional&&
         c(world).active_strength("IRREGULAR")==before_strength&&
         c(world).sovereignty("R")=="A",
         "P5-G conventional front must reuse actual C army and keep legal title");

    phasegh::World::Plan reclaim;
    reclaim[10].push_back([](phasegh::Stage& gh){
        gh.phase_c().occupy("R","A");
    });
    world.advance_day(reclaim); // Jan11
    phasegh::World::Plan remnant;
    remnant[9].push_back([](phasegh::Stage& gh){
        gh.phase_c().p5_guerrilla_resume("G",d({2000,1,12}));
    });
    world.advance_day(remnant);
    need(c(world).p5_guerrilla_campaigns().at("G").stage==
             P5GuerrillaStage::active&&
         c(world).active_strength("IRREGULAR")==before_strength,
         "P5-G failed front should return as the same surviving irregular unit");
    phasegh::World::Plan ceasefire;
    ceasefire[9].push_back([](phasegh::Stage& gh) {
        gh.phase_c().p5_guerrilla_end({
            "G","scenario:documented-ceasefire",
            P5GuerrillaResolution::negotiated_ceasefire,
            d({2000,1,13})
        });
    });
    world.advance_day(ceasefire);
    need(c(world).p5_guerrilla_campaigns().at("G").stage==
             P5GuerrillaStage::ceasefire&&
         c(world).active_strength("IRREGULAR")==before_strength&&
         c(world).p5_guerrilla_outcomes().size()==1,
         "P5-G ceasefire must not destroy living troops or physical equipment");
}

int main() {
    try {
        physical_unit_and_regional_support_are_mandatory();
        expenditure_real_market_access_and_checkpoint_replay();
        local_skirmish_uses_real_existing_armed_POP();
        conventional_escalation_and_regional_return_share_units();
        std::cout<<"P5-G regional guerrilla journal: four scenarios PASSED\n";
        return 0;
    } catch(const std::exception& ex) {
        std::cerr<<"P5-G REGRESSION FAILED: "<<ex.what()<<"\n";
        return 1;
    }
}
