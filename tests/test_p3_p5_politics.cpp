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

static void shinsengumi_changes_movement_risk_without_altering_pop() {
    const auto run=[](const std::string& choice,int expected_reduction) {
        PoliticalFixture fixture;
        auto& geo=fixture.gh.seed_geography();
        geo.seed_province(2,"FOREIGN","B");geo.seed_unit(101,"B","Foreign district");
        geo.seed_assignment(2,101);
        auto& core=geo.seed_core().seed_core();
        core.seed_actor("B",1000);
        core.seed_government("B","B_HEAD","B_PM",5,3);
        core.seed_p1_constitution("B",{P1GovernancePrinciple::monarchy,
            P1PowerDistribution::autocracy,P1LegislatureKind::none,"test:constitution"});
        core.seed_interest_group("FOREIGNERS");
        core.p3_seed_group({"B","FOREIGNERS","","political_reform","reform",
            P3GroupType::trade_unions,800'000,0});
        core.p3_seed_movement({"FOREIGN_MOV","B","political_reform","reform","",
            P3MovementGoal::legal_reform,{"FOREIGNERS"},{},1'000'000,1'000'000,true});
        core.p3_seed_movement({"LOW","A","political_reform","reform","",
            P3MovementGoal::legal_reform,{"WORKERS"},{},250'000,30'000,true});
        auto& pop=core.seed_population();
        const auto h=pop.household_profile({"FOREIGN","mixed","shared",50});
        const auto e=pop.economic_profile({"FOREIGN","worker","wage","employee",
                                           pop.default_education()});
        const auto d=pop.demographic_profile({"FOREIGN",23640,phaseb::Sex::male,
            "culture","faith","citizen","free",pop.default_health(),phaseb::Service::civilian});
        const auto foreign=pop.seed_population(d,e,h,50);
        core.p3_set_stance(foreign,"political_reform",{900'000,900'000});
        if(!choice.empty())
            core.p2_seed_base({"SECURITY","A","internal_security","national",
                               "law_"+choice,"test:effective-security-law"});
        auto world=daily::World::adopt(std::move(fixture.gh),309);
        need(c(world).p3_movement_view("LOW").radical_ppm==
                 std::max(0,30'000-expected_reduction),
             "Shinsengumi low movement radicalism did not clamp correctly");
        need(rejects([&]{(void)c(world).p3_movement_view("NOT-REGISTERED");}),
             "unknown movement acquired a policy score");
        phasegh::World::Plan join;
        join[9].push_back([worker=fixture.workers,foreign](phasegh::Stage& gh) {
            gh.phase_c().join_interest_group(worker,"WORKERS",30);
            gh.phase_c().join_interest_group(foreign,"FOREIGNERS",30);
        });
        world.advance_day(join);
        const auto& live=c(world);
        const auto& raw=live.p3_movements().at("MOV");
        const auto home=live.p3_movement_view("MOV");
        const auto away=live.p3_movement_view("FOREIGN_MOV");
        const auto risk=live.p5_risk("MOV");
        const auto attitude=live.p3_attitudes().at(fixture.workers).radical_ppm;
        need(raw.radical_ppm==1'000'000 && home.supporters>=30 && away.supporters>=30 &&
             home.radical_ppm==1'000'000-expected_reduction &&
             away.radical_ppm==1'000'000 && risk.radical_ppm==home.radical_ppm,
             "effective security-law radicalism failed to reach only its real movement risk");
        auto saved=daily::SnapshotCodec::encode(world);
        auto loaded=daily::SnapshotCodec::decode(saved);
        need(c(loaded).p3_movement_view("MOV").radical_ppm==home.radical_ppm &&
             daily::SnapshotCodec::encode(loaded)==saved,
             "movement law changed persisted radicalism instead of deriving the view");
        need(live.people().population()==100 && live.cash("A")==1000 && live.cash("B")==1000,
             "movement radicalism changed underlying people or cash");
        return std::pair{attitude,risk.pressure_ppm};
    };
    const auto [no_law_attitude,no_law_pressure]=run("",0);
    const auto [shinsengumi_attitude,shinsengumi_pressure]=run("shinsengumi",50'000);
    const auto [institution_attitude,institution_pressure]=run("secret_police",0);
    need(no_law_attitude==shinsengumi_attitude && no_law_attitude==institution_attitude &&
         shinsengumi_pressure<no_law_pressure && institution_pressure==no_law_pressure,
         "movement law should change real pressure without modifying POP radicalism");
}

static void exact_professions_weight_actual_group_members() {
    const auto run=[](const std::string& domain,const std::string& choice,
                      const std::string& profession,People weighted,bool amendment=false) {
        phasegh::World gh(24000);
        auto& geo=gh.seed_geography();
        for(const auto* nation:{"A","B"}) {
            const auto id=std::string_view(nation)=="A"?1:2;
            geo.seed_province(id,nation,nation);geo.seed_unit(id,nation,"District");
            geo.seed_assignment(id,id);
        }
        auto& core=geo.seed_core().seed_core();
        core.seed_actor("EMP",1000);
        core.seed_interest_group("Role");core.seed_interest_group("Peer");
        for(const auto* nation:{"A","B"}) {
            core.seed_actor(nation,55);
            core.seed_government(nation,"HEAD","PM",5,3);
            core.seed_p1_constitution(nation,{P1GovernancePrinciple::monarchy,
                P1PowerDistribution::autocracy,P1LegislatureKind::none,"fixture:constitution"});
            core.p3_seed_group({nation,"Role","","","",P3GroupType::custom,0,0});
            core.p3_seed_group({nation,"Peer","","","",P3GroupType::custom,0,0});
        }
        if(!choice.empty())core.p2_seed_base({"law-A","A",domain,"national",
                                             choice,"fixture:law"});
        if(amendment)core.p2_seed_amendment({"amendment_bourgeois_specialists","law-A",
            "fixture:law","",{{"approved-provision","A",
                P2ClauseKind::political_association_allowed,1}}});
        auto& pop=core.seed_population();
        std::vector<std::pair<phaseb::Id,std::string>> joining;
        for(const auto* nation:{"A","B"}) {
            const auto h=pop.household_profile({nation,"mixed","shared",100});
            const auto d=pop.demographic_profile({nation,23640,phaseb::Sex::male,
                "culture","faith","citizen","free",pop.default_health(),phaseb::Service::civilian});
            for(const auto* name:{"Role","Peer"}) {
                const auto job=std::string_view(name)=="Role"?profession:"worker";
                const auto e=pop.economic_profile({nation,job,"wage","employee",pop.default_education()});
                const auto id=pop.seed_population(d,e,h,100);
                pop.hire(id,1,"EMP",job,1,{});
                joining.emplace_back(id,name);
            }
        }
        auto world=daily::World::adopt(std::move(gh),307);
        phasegh::World::Plan plan;
        plan[9].push_back([joining](phasegh::Stage& stage) {
            for(const auto& [id,name]:joining)stage.phase_c().join_interest_group(id,name,100);
        });
        world.advance_day(plan);
        const auto& live=c(world);
        const auto actual=live.p3_group_view("A","Role");
        const auto peer=live.p3_group_view("A","Peer");
        const auto score=[&](People n) { return (n*1'000'000/(weighted+100))*9/10; };
        need(actual.members==100 && peer.members==100 &&
             actual.clout_ppm==score(weighted) && peer.clout_ppm==score(100),
             "canonical occupation law did not weight only its actual local group members");
        need(live.p3_group_view("B","Role").clout_ppm==450'000 &&
             live.p3_group_view("B","Peer").clout_ppm==450'000 &&
             live.people().population()==400 && live.cash("EMP")==1000 &&
             live.cash("A")==55 && live.cash("B")==55,
             "occupation law changed foreign clout or manufactured POP/cash");
        for(const auto& [id,group]:joining) {
            (void)group;
            need(live.people().employed(id)==1,"occupation law recreated employment");
        }
    };
    run("land_reform","law_latifundias","peasants",80); // Preexisting plural.
    run("land_reform","law_latifundias","peasant",80);
    run("land_reform","law_latifundias","Peasant",100); // Not a case-folded alias.
    run("bureaucracy","law_appointed_bureaucrats","bureaucrat",150);
    run("bureaucracy","law_communal_administration","bureaucrat",75);
    run("bureaucracy","law_communal_administration","bureaucrat",90,true);
    run("distribution_of_power","law_technocracy","academic",133);
    run("labour_associations","law_guild_system","shopkeeper",125);
    run("land_reform","law_latifundias","aristocrat",140);
    run("distribution_of_power","law_oligarchy","capitalist",125);
    run("distribution_of_power","law_elder_council","clergyman",150);
    run("distribution_of_power","law_organic_regulation","clerk",125);
    run("distribution_of_power","law_technocracy","engineer",133);
    run("land_reform","law_homesteading","farmer",125);
    run("internal_security","law_extraordinary_commission","soldier",115);
    run("","","peasant",100);
}

static void funded_institutions_weight_only_their_typed_group_members() {
    enum class Site { valid, absent, unfunded, unstaffed, shared, foreign, police };
    const auto run=[](const std::string& domain,const std::string& choice,
                      P1InstitutionKind kind,P3GroupType type,Site condition,
                      People expected,bool with_trait=false) {
        phasegh::World gh(24000);
        auto& geo=gh.seed_geography();
        geo.seed_unit(1,"A","Local");geo.seed_unit(2,"B","Foreign");
        geo.seed_province(1,"HOME","A");geo.seed_assignment(1,1);
        geo.seed_province(2,"OTHER","A");geo.seed_assignment(2,1);
        geo.seed_province(3,"FOREIGN","B");geo.seed_assignment(3,2);
        auto& core=geo.seed_core().seed_core();
        core.seed_interest_group("Target");core.seed_interest_group("Peer");
        for(const auto* nation:{"A","B"}) {
            core.seed_actor(nation,condition==Site::unfunded&&
                std::string_view(nation)=="A"?0:50);
            core.seed_government(nation,"HEAD","PM",5,3);
            core.seed_p1_constitution(nation,{P1GovernancePrinciple::monarchy,
                P1PowerDistribution::autocracy,P1LegislatureKind::none,"fixture:constitution"});
            core.p3_seed_group({nation,"Target","","","",type,0,0});
            core.p3_seed_group({nation,"Peer","","","",P3GroupType::custom,0,0});
        }
        core.p2_seed_base({"department","A",domain,"national",choice,"fixture:law"});
        if(with_trait)core.p3_set_ideologies("A","Target",{"materialist"});
        core.seed_actor("SERVICE",condition==Site::unfunded?0:100);
        core.seed_actor("EMP",1000);
        auto& pop=core.seed_population();
        std::vector<std::pair<phaseb::Id,std::string>> joining;
        for(const auto* region:{"HOME","OTHER","FOREIGN"}) {
            core.p1_set_incorporation(region,true);
            const auto h=pop.household_profile({region,"mixed","shared",100});
            const auto d=pop.demographic_profile({region,23640,phaseb::Sex::male,
                "culture","faith","citizen","free",pop.default_health(),phaseb::Service::civilian});
            const auto st=pop.economic_profile({region,"staff","wage","employee",pop.default_education()});
            const bool staff=condition==Site::shared?std::string_view(region)=="OTHER":
                condition!=Site::unstaffed&&condition!=Site::absent;
            if(staff) {
                const auto id=pop.seed_population(d,st,h,100);
                pop.hire(id,10,"SERVICE","staff",1,{});
            }
            if(std::string_view(region)=="OTHER")continue;
            for(const auto* name:{"Target","Peer"}) {
                const auto e=pop.economic_profile({region,name,"wage","employee",pop.default_education()});
                const auto id=pop.seed_population(d,e,h,100);
                pop.hire(id,1,"EMP",name,1,{});
                joining.emplace_back(id,name);
            }
        }
        const auto service=[&](const std::string& id,const std::string& region,
                               const std::string& country,P1InstitutionKind actual,int level) {
            core.seed_public_institution(id,"SERVICE",region,"inputs",100,10);
            core.seed_p1_institution(country,id,actual,level);
        };
        if(condition!=Site::absent&&condition!=Site::foreign)
            service("MAIN","HOME","A",condition==Site::police?P1InstitutionKind::policing:kind,condition==Site::shared?3:1);
        if(condition==Site::valid||condition==Site::shared)
            service("HIGH","OTHER","A",kind,condition==Site::shared?1:3);
        if(condition==Site::foreign)
            service("FOREIGN_SERVICE","FOREIGN","B",kind,3);
        if(condition==Site::valid)
            core.p2_seed_amendment({"effective-level","department","fixture:level","",
                {{"proven-level","HIGH",P2ClauseKind::public_institution_level,2}}});
        auto world=daily::World::adopt(std::move(gh),308);
        const auto before_population=c(world).people().population();
        phasegh::World::Plan plan;
        plan[9].push_back([joining](phasegh::Stage& stage) {
            for(const auto& [id,name]:joining)stage.phase_c().join_interest_group(id,name,100);
        });
        world.advance_day(plan);
        const auto& live=c(world);
        const auto home=live.p3_group_view("A","Target");
        const auto peer=live.p3_group_view("A","Peer");
        const auto clout=[&](People weight){return (weight*1'000'000/(expected+100))*9/10;};
        if(home.members!=100||peer.members!=100||home.clout_ppm!=clout(expected)||
           peer.clout_ppm!=clout(100))
            throw std::runtime_error("effective institution clout differs: "+choice+
                ", site case "+std::to_string(static_cast<int>(condition))+
                ", got "+std::to_string(home.clout_ppm)+"/"+std::to_string(peer.clout_ppm)+
                ", expected "+std::to_string(clout(expected))+"/"+std::to_string(clout(100)));
        need(live.p3_group_view("B","Target").clout_ppm==450'000 &&
             live.people().population()==before_population && live.cash("EMP")==1000,
             "institutional clout mutated population/cash or leaked overseas");
        if(with_trait) {
            const auto traits=live.p3_active_traits("A","Target");
            need(std::any_of(traits.begin(),traits.end(),[](const auto& trait) {
                return trait.modifier=="state_education_access_add"&&trait.value_ppm==-100'000;
            }),"clout-free approval projection lost the existing Education trait");
            if(condition==Site::valid)
                need(live.p1_institution_capacity("HIGH").usable_slots==36,
                     "existing active Education trait was omitted from real capacity");
        }
        return std::tuple{home.members,home.approval,home.radical_ppm,home.engagement_ppm};
    };
    constexpr auto H=P1InstitutionKind::healthcare,W=P1InstitutionKind::welfare,
                   E=P1InstitutionKind::education;
    constexpr auto D=P3GroupType::devout,L=P3GroupType::landowners,
                   A=P3GroupType::armed_forces,B=P3GroupType::petite_bourgeoisie,
                   I=P3GroupType::intelligentsia;
    run("health_system","law_charitable_health_system",H,D,Site::valid,120);
    run("health_system","law_private_health_insurance",H,B,Site::valid,120);
    run("welfare","law_chiefs_distribute_aid",W,L,Site::valid,120);
    run("welfare","law_chiefs_distribute_aid",W,D,Site::valid,130);
    run("welfare","law_chiefs_distribute_aid",W,A,Site::valid,120);
    run("education_system","law_private_schools",E,I,Site::valid,120);
    const auto effective=run("education_system","law_religious_schools",E,D,Site::valid,120,true);
    const auto unfunded=run("education_system","law_religious_schools",E,D,Site::unfunded,100,true);
    need(effective==unfunded,"institution loss should not change approval, attitudes, engagement or members");
    for(const auto invalid:{Site::absent,Site::unfunded,Site::unstaffed,Site::foreign,Site::police})
        run("welfare","law_chiefs_distribute_aid",W,D,invalid,100);
    run("health_system","law_charitable_health_system",H,D,Site::shared,110);
}

int main() {
    try {
        real_pop_attitudes_and_national_clout();
        exact_professions_weight_actual_group_members();
        funded_institutions_weight_only_their_typed_group_members();
        shinsengumi_changes_movement_risk_without_altering_pop();
        actual_next_day_strike_and_no_double_participants();
        armed_conflict_requires_real_deployments_and_verified_control();
        legal_concession_resolves_without_armed_conflict();
        std::cout<<"P3/P5 politics: seven real-ledger integration scenarios PASS\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"P3/P5 POLITICS FAILED: "<<error.what()<<"\n";
        return 1;
    }
}
