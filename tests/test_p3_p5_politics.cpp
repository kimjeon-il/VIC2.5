#include "vic25/daily_snapshot.hpp"
#include "vic25/p0_daily.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace vic25;
using namespace vic25::phasec;

static void need(bool value,const char* reason) {
    if(!value)throw std::runtime_error(reason);
}
static daily::DayIndex day(daily::Date date) {
    return daily::to_day_index(date);
}
static const IntegratedWorld& c(const daily::World& world) {
    return world.engine().base().core().core();
}
template<class F>static bool rejects(F&& command) {
    try{command();}catch(const std::exception&){return true;}
    return false;
}

struct PoliticalFixture {
    phasegh::World gh{24000};
    phaseb::Id workers{},loyal{},rebel{};
    PoliticalFixture(bool armed=false,
                     P3MovementGoal goal=P3MovementGoal::legal_reform,
                     bool no_parliament=false) {
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"R","A");
        geo.seed_unit(100,"A","District");
        geo.seed_assignment(1,100);
        auto& core=geo.seed_core().seed_core();
        core.seed_actor("A",1000);
        core.seed_actor("EMP",100);
        core.seed_government("A","OLD_HEAD","PM",5,3);
        core.p5_seed_capital("A","R");
        core.seed_p1_constitution("A",{
            no_parliament?P1GovernancePrinciple::monarchy:
                          P1GovernancePrinciple::presidential_republic,
            no_parliament?P1PowerDistribution::autocracy:
                          P1PowerDistribution::universal_vote,
            no_parliament?P1LegislatureKind::none:
                          P1LegislatureKind::unicameral,
            "test:constitutional-rule"});
        core.seed_interest_group("WORKERS");
        core.seed_p1_coalition("A",{{"WORKERS"},{},false});
        core.p4_register_party({"REFORM","A","HEAD_REFORM",100'000,true,true});
        core.p3_seed_group({"A","WORKERS","","political_reform","reform",
                            P3GroupType::trade_unions,800'000,0});
        core.p3_seed_movement({"MOV","A","political_reform","reform","",
            goal,{"WORKERS"},{"REFORM"},1'000'000,1'000'000,true});
        auto& pop=core.seed_population();
        const auto hh=pop.household_profile({"R","mixed","shared",50});
        const auto civilian=pop.economic_profile({
            "R","worker","wage","employee",pop.default_education()});
        const auto inactive=pop.economic_profile({
            "R","not_assigned","inactive","ineligible",pop.default_education()});
        const auto dem=pop.demographic_profile({
            "R",24000-360,phaseb::Sex::female,"culture","faith",
            "citizen","free",pop.default_health(),phaseb::Service::civilian});
        workers=pop.seed_population(dem,civilian,hh,50,1);
        pop.hire(workers,10,"EMP","worker",10,{});
        core.p3_set_stance(workers,"political_reform",
                            {900'000,900'000});
        if(armed) {
            core.seed_unit("REB");
            core.seed_unit("LOYAL");
            const auto soldiers=pop.demographic_profile({
                "R",24000-360,phaseb::Sex::male,"culture","faith",
                "citizen","free",pop.default_health(),phaseb::Service::active});
            rebel=pop.seed_population(soldiers,inactive,hh,5,2);
            loyal=pop.seed_population(soldiers,inactive,hh,5,3);
            core.seed_active("REB",rebel);
            core.seed_active("LOYAL",loyal);
            core.seed_army_deployment("REB","A","R");
            core.seed_army_deployment("LOYAL","A","R");
            core.seed_stock("REB","R","ammunition",20);
            core.seed_stock("LOYAL","R","ammunition",20);
            core.seed_stock("REB","R","rifle",20);
            core.seed_stock("LOYAL","R","rifle",20);
        }
        gh.validate();
    }
};

static void real_pop_attitudes_and_national_clout() {
    PoliticalFixture fixture;
    fixture.gh.seed_geography().seed_core().seed_core().p3_seed_movement({
        "DORMANT","A","political_reform","reform","",
        P3MovementGoal::legal_reform,{"WORKERS"},{"REFORM"},
        250'000,50'000,false});
    auto world=daily::World::adopt(std::move(fixture.gh),301);
    auto base_pop=c(world).people().population();
    auto cash=c(world).cash("A");
    phasegh::World::Plan form;
    form[9].push_back([slice=fixture.workers](phasegh::Stage& gh) {
        gh.phase_c().join_interest_group(slice,"WORKERS",30);
    });
    world.advance_day(form);
    const auto& observed=c(world);
    need(observed.p3_attitudes().at(fixture.workers).radical_ppm>0&&
         observed.p3_group_view("A","WORKERS").members==30&&
         observed.p3_group_view("A","WORKERS").clout_ppm>0&&
         observed.p3_movement_view("MOV").supporters>=30&&
         observed.p3_movement_view("MOV").supporters<=base_pop&&
         observed.p3_movements().at("DORMANT").active&&
         observed.p1_legitimacy("A").observed_groups,
         "P3 actual POP unemployment, clout or movement support missing");
    need(observed.people().population()==base_pop&&
         observed.cash("A")==cash,
         "P3 may not mint money or count organizational membership as people");
    auto saved=daily::SnapshotCodec::encode(world);
    auto reloaded=daily::SnapshotCodec::decode(saved);
    need(daily::SnapshotCodec::encode(reloaded)==saved,
         "P3 attitudes and durable movement are not checkpoint deterministic");
}

static void actual_next_day_strike_and_no_double_participants() {
    PoliticalFixture fixture;
    auto world=daily::World::adopt(std::move(fixture.gh),302);
    phasegh::World::Plan start;
    start[9].push_back([slice=fixture.workers](phasegh::Stage& gh) {
        gh.phase_c().join_interest_group(slice,"WORKERS",30);
        P5CollectiveAction action;
        action.id="STRIKE-1";action.movement="MOV";
        action.group="WORKERS";action.region="R";
        action.kind=P5ActionKind::strike;
        action.slice=slice;action.participants=4;action.strikers=2;
        action.day=day({2000,1,1});
        gh.phase_c().p5_perform_action(action);
    });
    world.advance_day(start);
    need(c(world).phase_d_workforce("EMP")==10&&
         c(world).p5_actions().size()==1,
         "P5 strike improperly changed already-completed production day");
    const auto saved=daily::SnapshotCodec::encode(world);
    auto replay=daily::SnapshotCodec::decode(saved);
    world.advance_day();replay.advance_day();
    need(c(world).phase_d_workforce("EMP")==8&&
         daily::SnapshotCodec::encode(world)==
             daily::SnapshotCodec::encode(replay),
         "P5 daily strike must reduce actual employment availability exactly once");
    world.advance_day();
    need(c(world).phase_d_workforce("EMP")==10,
         "P5 one-day strike must expire without permanently deleting jobs");

    auto illegal=world;
    const auto original=daily::SnapshotCodec::encode(illegal);
    phasegh::World::Plan conflict;
    conflict[9].push_back([slice=fixture.workers](phasegh::Stage& gh) {
        P5CollectiveAction action;
        action.id="TOO-MANY";action.movement="MOV";
        action.group="WORKERS";action.kind=P5ActionKind::demonstration;
        action.slice=slice;action.participants=50;
        action.day=day({2000,1,4});
        gh.phase_c().p5_perform_action(action);
    });
    need(rejects([&]{illegal.advance_day(conflict);})&&
         daily::SnapshotCodec::encode(illegal)==original,
         "P5 participant oversubscription must roll back the entire day");
}

static void armed_conflict_requires_real_deployments_and_verified_control() {
    PoliticalFixture fixture(true,P3MovementGoal::replace_government);
    auto world=daily::World::adopt(std::move(fixture.gh),303);
    const auto start_population=c(world).people().population();
    const auto start_cash=c(world).cash("A");
    phasegh::World::Plan mobilization;
    mobilization[7].push_back([](phasegh::Stage& gh) {
        gh.phase_c().assign_equipment("REB","R","rifle",5);
        gh.phase_c().assign_equipment("LOYAL","R","rifle",5);
    });
    mobilization[9].push_back([slice=fixture.workers](phasegh::Stage& gh) {
        gh.phase_c().join_interest_group(slice,"WORKERS",30);
    });
    world.advance_day(mobilization);
    while(world.date()<daily::Date{2000,1,8})world.advance_day();
    const auto& warning=c(world).p5_crises().at("P5:MOV");
    need(warning.stage==P5CrisisStage::warning&&
         warning.pledged_units.empty()&&
         c(world).army_deployments().size()==2,
         "P5 risk must generate political warnings without phantom rebel armies");

    auto no_army=world;
    const auto encoded=daily::SnapshotCodec::encode(no_army);
    phasegh::World::Plan unlawful;
    unlawful[9].push_back([](phasegh::Stage& gh) {
        gh.phase_c().p5_begin_armed_conflict(
            "P5:MOV",day({2000,1,8}));
    });
    need(rejects([&]{no_army.advance_day(unlawful);})&&
         daily::SnapshotCodec::encode(no_army)==encoded,
         "P5 armed uprising without real pledged military must fail");

    phasegh::World::Plan pledge;
    pledge[9].push_back([](phasegh::Stage& gh) {
        gh.phase_c().p5_pledge_army_unit("P5:MOV","REB");
    });
    world.advance_day(pledge); // Jan 8
    phasegh::World::Plan clash;
    clash[9].push_back([](phasegh::Stage& gh) {
        gh.phase_c().p5_begin_armed_conflict(
            "P5:MOV",day({2000,1,9}));
    });
    clash[10].push_back([r=fixture.rebel,l=fixture.loyal]
                        (phasegh::Stage& gh) {
        gh.phase_c().p5_record_real_engagement(
            "P5:MOV","LOYAL",l,"REB",r,"R",
            CombatLoss{0,0,0,0,0,1},CombatLoss{0,0,0,0,0,1},
            day({2000,1,9}));
    });
    world.advance_day(clash); // Jan 9
    need(c(world).p5_crises().at("P5:MOV").verified_engagements==1&&
         c(world).stock("REB","R","ammunition")==19&&
         c(world).stock("LOYAL","R","ammunition")==19,
         "P5 civil battle must use actual C military units and consumed ammunition");
    phasegh::World::Plan control;
    control[10].push_back([](phasegh::Stage& gh) {
        gh.phase_c().occupy("R","P5:P5:MOV");
        gh.phase_c().p5_confirm_actual_control(
            "P5:MOV","R",day({2000,1,10}));
    });
    world.advance_day(control); // Jan10
    phasegh::World::Plan settlement;
    settlement[9].push_back([](phasegh::Stage& gh) {
        P5Settlement peace;
        peace.id="SETTLEMENT-1";peace.crisis_id="P5:MOV";
        peace.legal_basis="real-controlled-capital";
        peace.successor_head="NEW_HEAD";
        peace.kind=P5SettlementKind::cabinet_overthrow;
        peace.day=day({2000,1,11});
        gh.phase_c().p5_settle(peace);
    });
    world.advance_day(settlement);
    need(c(world).government("A").head_of_state=="OLD_HEAD",
         "P5 government turnover must not predate the committed settlement");
    world.advance_day();
    need(c(world).government("A").head_of_state=="NEW_HEAD"&&
         c(world).military_control("R")=="A"&&
         c(world).p5_crises().at("P5:MOV").stage==P5CrisisStage::settled&&
         c(world).p5_settlements().size()==1&&
         c(world).people().population()==start_population&&
         c(world).cash("A")==start_cash,
         "P5 evidenced regime transition must preserve actual treasury and POP");
}

static void legal_concession_resolves_without_armed_conflict() {
    PoliticalFixture fixture(false,P3MovementGoal::replace_government,true);
    auto world=daily::World::adopt(std::move(fixture.gh),304);
    phasegh::World::Plan join;
    join[9].push_back([slice=fixture.workers](phasegh::Stage& gh){
        gh.phase_c().join_interest_group(slice,"WORKERS",30);
    });
    world.advance_day(join);
    while(world.date()<daily::Date{2000,1,8})world.advance_day();
    need(c(world).p5_crises().at("P5:MOV").stage==
             P5CrisisStage::warning,
         "P5 documented political crisis should be observable before legal reform");
    phasegh::World::Plan bill;
    bill[9].push_back([](phasegh::Stage& gh) {
        P2Bill law;
        law.id="REFORM-BILL";law.country="A";law.proposer="PM";
        law.legal_basis="verified:constitutional-compromise";
        law.action=P2BillAction::enact_or_replace_base;
        law.effective_day=day({2000,1,9});
        law.proposed_base.id="REFORM-ACT";
        law.proposed_base.country="A";
        law.proposed_base.domain="political_reform";
        law.proposed_base.scope="national";
        law.proposed_base.base_choice="reform";
        law.proposed_base.legal_basis="verified:reform-act";
        auto& c=gh.phase_c();
        c.p2_submit_bill(law,day({2000,1,8}));
        c.p2_approve_bill("REFORM-BILL",{});
        c.p2_promulgate_bill("REFORM-BILL","OLD_HEAD",
                            day({2000,1,8}));
    });
    world.advance_day(bill); // Jan8
    phasegh::World::Plan settle;
    settle[9].push_back([](phasegh::Stage& gh) {
        P5Settlement outcome;
        outcome.id="LEGAL-PEACE";outcome.crisis_id="P5:MOV";
        outcome.legal_basis="REFORM-ACT";
        outcome.kind=P5SettlementKind::legal_concession;
        outcome.day=day({2000,1,9});
        gh.phase_c().p5_settle(outcome);
    });
    world.advance_day(settle); // Jan9 law takes effect before settlement
    need(c(world).p5_settlements().size()==1&&
         c(world).p5_crises().at("P5:MOV").stage==
             P5CrisisStage::settled&&
         c(world).army_deployments().empty()&&
         c(world).people().population()==50,
         "P5 legal reform should settle crisis without creating arms or people");
}

int main() {
    try {
        real_pop_attitudes_and_national_clout();
        actual_next_day_strike_and_no_double_participants();
        armed_conflict_requires_real_deployments_and_verified_control();
        legal_concession_resolves_without_armed_conflict();
        std::cout<<"P3/P5 politics: four real-ledger integration scenarios PASS\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"P3/P5 POLITICS FAILED: "<<error.what()<<"\n";
        return 1;
    }
}
