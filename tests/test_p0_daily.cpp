#include "vic25/p0_daily.hpp"
#include "vic25/daily_snapshot.hpp"
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

using namespace vic25;
static void require_test(bool v,const char* msg){if(!v)throw std::runtime_error(msg);}
static phaseb::Id people(phasegh::World& gh,const std::string& region,
                  phaseb::People n,const std::string& profession,
                  phaseb::Service service=phaseb::Service::civilian) {
    auto& p=gh.seed_geography().seed_core().seed_core().seed_population();
    const auto h=p.household_profile({region,"mixed","shared",n});
    const auto econ=p.economic_profile({region,profession,"wage","employee",
                                       p.default_education()});
    const auto demo=p.demographic_profile({region,gh.month()-300,
                   phaseb::Sex::female,"culture","faith","citizen",
                   "free",p.default_health(),service});
    return p.seed_population(demo,econ,h,n,static_cast<phaseb::Id>(p.slices().size()+1));
}
static const phasec::IntegratedWorld& c(const p0::DailyGame& game) {
    return game.world().engine().base().core().core();
}
static phasegh::World fixture() {
    phasegh::World gh(24000);
    auto& e=gh.seed_geography();
    e.seed_province(1,"R","A");
    e.seed_unit(11,"A","ROOT");
    e.seed_assignment(1,11);
    auto& d=e.seed_core();
    auto& core=d.seed_core();
    core.seed_actor("A",1000);
    core.seed_actor("FIRM",2000);
    core.seed_actor("HH",1000);
    core.seed_actor("POOR",0);
    core.seed_actor("WORKERS",0);
    core.seed_actor("BUILDER",0);
    core.seed_actor("TREASURY",0);
    core.seed_market("R","food",100,1,200,0);
    core.seed_market("R","ore",10,1,30,0);
    core.seed_market("R","steel",10,1,30,0);
    core.seed_production_facility("FARM","FIRM","R",62);
    core.seed_stock("FIRM","R","ore",100);
    core.seed_stock("BUILDER","R","steel",2);
    core.seed_government("A","headA","PM_A",5,3);
    core.seed_interest_group("FOOD_MOVEMENT");
    auto workforce=people(gh,"R",4,"worker");
    auto builders=people(gh,"R",2,"worker");
    auto civilians=people(gh,"R",10,"civilian");
    (void)workforce;(void)builders;(void)civilians;
    phased::IndustryRecipe r;
    r.id="FOOD_RECIPE";r.site="FARM";
    r.inputs={{"ore",1}};r.outputs={{"food",2}};
    r.workers_per_batch=1;r.capacity_per_batch=1;
    d.seed_recipe(r);
    phased::ConstructionProject proj;
    proj.id="NEW_FARM";proj.client="FIRM";proj.contractor="BUILDER";
    proj.region="R";proj.material_good="steel";proj.target_site="FARM2";
    proj.work_required=2;proj.material_per_work=1;proj.workers_per_work=1;
    proj.cash_price_per_work=10;proj.target_capacity=5;
    d.seed_construction(proj);
    gh.validate();
    return gh;
}
static void daily_production_trade_ai_investment_replay() {
    auto w=fixture();
    const auto& pop=w.base().core().core().people().slices();
    auto worker=pop.begin()->first;
    auto builder=std::next(pop.begin())->first;
    auto buyer=std::next(pop.begin(),2)->first;
    auto& core=w.seed_geography().seed_core().seed_core();
    // Employment is a real Phase B contract; the P0 engine does not grant free labor.
    auto& seeded=core.seed_population();
    seeded.hire(worker,4,"FIRM","worker",10,phaseb::JobRequirements{});
    seeded.hire(builder,2,"BUILDER","worker",10,phaseb::JobRequirements{});
    p0::Policies options;
    options.manufacturers.push_back({"FOOD_RECIPE",15,"WORKERS","TREASURY",1});
    options.buyers.push_back({buyer,"HH","FIRM","R","food",1});
    options.investments.push_back({"NEW_FARM","food",100,10,1});
    options.countries.push_back({"A","PM_A","R","food","FIRM",120,0,100000});
    p0::DailyGame game(daily::World::adopt(std::move(w),7),options);
    const auto before=c(game).cash("HH");
    game.advance_day();
    require_test(game.world().date()==daily::Date{2000,1,2},"day did not advance");
    require_test(!game.world().engine().base().core().state().actual_production_runs.empty(),
                 "real daily industry did not run");
    require_test(c(game).stock("FIRM","R","food")==0 &&
                 c(game).stock("HH","R","food")==0,
                 "food must not be duplicated after purchase and real consumption");
    require_test(c(game).cash("HH")<before,"buyer did not pay real money");
    require_test(c(game).cash("WORKERS")>0,"worker wages were not settled");
    require_test(game.world().engine().base().core().construction("NEW_FARM").accepted_work==1,
        "daily private construction missing");
    require_test(game.world().engine().diplomacy().tariffs.at({"A","food"}).ppm==100000,
        "national AI failed to adopt lawful tariff");
    // Existing snapshots are saved by the native day codec; policy config is
    // campaign input and must be reused on load.
    auto saved=daily::SnapshotCodec::encode(game.world());
    auto restored=daily::SnapshotCodec::decode(saved);
    p0::DailyGame replay(std::move(restored),options);
    for(int i=0;i<4;++i){game.advance_day();replay.advance_day();}
    require_test(daily::SnapshotCodec::encode(game.world())==
                 daily::SnapshotCodec::encode(replay.world()),
                 "same snapshot and policies must replay exactly");
    require_test(game.world().engine().base().core().construction("NEW_FARM").commissioned,
                 "daily private investment must commission completed facilities");
}
static void daily_market_shortage_and_political_response() {
    auto w=fixture();
    const auto& slices=w.base().core().core().people().slices();
    auto buyer=std::next(slices.begin(),2)->first;
    p0::Policies opt;
    opt.buyers.push_back({buyer,"HH","FIRM","R","food",1});
    opt.political_reactions.push_back({buyer,"R","FOOD_MOVEMENT",2,1});
    p0::DailyGame g(daily::World::adopt(std::move(w),4),opt);
    g.advance_day();
    require_test(c(g).food_grievance("R")>0,"daily shortage did not enter political evidence");
    g.advance_day();
    require_test(c(g).group_members(buyer,"FOOD_MOVEMENT")>0,
        "POP did not join a grievance movement after observed scarcity");
    require_test(c(g).protest_participants(buyer)>0,
        "real POP did not participate in bounded protest");
}
static void daily_battle_consumes_actual_ammo_and_pop() {
    phasegh::World gh(24000);
    auto& e=gh.seed_geography();
    e.seed_province(1,"R","A");
    e.seed_unit(11,"A","ROOT");e.seed_assignment(1,11);
    auto& core=e.seed_core().seed_core();
    for(auto who:{"A","ATT","DEF"})core.seed_actor(who,100);
    core.seed_unit("ATT");core.seed_unit("DEF");
    core.seed_stock("ATT","R","ammunition",20);
    core.seed_stock("DEF","R","ammunition",20);
    auto at=people(gh,"R",20,"soldier",phaseb::Service::active);
    auto de=people(gh,"R",20,"soldier",phaseb::Service::active);
    core.seed_active("ATT",at);
    core.seed_active("DEF",de);
    const auto population=core.people().population();
    p0::Policies rules;rules.engagements.push_back({"ATT","DEF","R",at,de,1500000});
    p0::DailyGame g(daily::World::adopt(std::move(gh),2),rules);
    g.advance_day();
    require_test(c(g).stock("ATT","R","ammunition")<20 &&
                 c(g).stock("DEF","R","ammunition")<20,
                 "battle did not use real ammunition");
    require_test(c(g).people().population()<population,
                 "daily battle did not inflict real casualties");
    const auto before=c(g).people().population();
    g.advance_day(); // no ammo left: no phantom repeated casualty
    require_test(c(g).people().population()==before,"ammunition-free casualties");
}

static void daily_insolvent_hunger_is_not_ignored() {
    auto w=fixture();
    const auto& slices=w.base().core().core().people().slices();
    const auto buyer=std::next(slices.begin(),2)->first;
    p0::Policies opt;
    opt.buyers.push_back({buyer,"POOR","FIRM","R","food",1});
    opt.political_reactions.push_back({buyer,"R","FOOD_MOVEMENT",1,1});
    p0::DailyGame g(daily::World::adopt(std::move(w),12),opt);
    const auto price=c(g).market("R","food").price;
    g.advance_day();
    require_test(c(g).cash("POOR")==0 && c(g).food_grievance("R")>0,
        "insolvent POP must still register essential unmet food demand");
    require_test(c(g).market("R","food").price==price,
        "an insolvent buyer must not cause false solvent-demand price inflation");
    require_test(c(g).group_members(buyer,"FOOD_MOVEMENT")>0,
        "observable same-day hunger should support political response");
}
static void daily_price_not_replayed_at_month_close() {
    auto w=fixture();
    const auto& slices=w.base().core().core().people().slices();
    const auto buyer=std::next(slices.begin(),2)->first;
    p0::Policies options;
    options.buyers.push_back({buyer,"HH","FIRM","R","food",1});
    p0::DailyGame g(daily::World::adopt(std::move(w),17),options);
    for(int i=0;i<30;++i)g.advance_day(); // Jan 1 through Jan 30
    require_test(g.world().date()==daily::Date{2000,1,31},
        "invalid daily advance before month boundary");
    const auto price30=c(g).market("R","food").price;
    const auto report=g.advance_day(); // Jan 31, then MonthClose
    require_test(report.monthly_close &&
                 g.world().date()==daily::Date{2000,2,1},
                 "Gregorian month boundary or MonthClose failed");
    require_test(c(g).market("R","food").price==price30+1,
                 "MonthClose applied a duplicate P0 price movement");
    g.advance_day();
    require_test(g.world().date()==daily::Date{2000,2,2},
                 "native day did not resume after monthly accounting close");
}


static void low_unit_price_still_changes_across_months() {
    phasegh::World gh(24000);
    auto& e=gh.seed_geography();
    e.seed_province(1,"R","A");
    e.seed_unit(11,"A","ROOT");
    e.seed_assignment(1,11);
    auto& c=e.seed_core().seed_core();
    c.seed_actor("A",100);
    c.seed_actor("SELLER",0);
    c.seed_actor("HH",1000);
    c.seed_market("R","food",1,1,10);
    c.seed_government("A","head","PM",5,3);
    auto& p=c.seed_population();
    const auto h=p.household_profile({"R","mixed","shared",5});
    const auto econ=p.economic_profile({"R","worker","wage","employee",p.default_education()});
    const auto demo=p.demographic_profile({"R",24000-300,phaseb::Sex::female,
                    "culture","faith","citizen","free",
                    p.default_health(),phaseb::Service::civilian});
    const auto slice=p.seed_population(demo,econ,h,5,1);
    p0::Policies policy;
    policy.buyers.push_back({slice,"HH","SELLER","R","food",1});
    p0::DailyGame game(daily::World::adopt(std::move(gh),3),policy);
    for(int i=0;i<102;++i)game.advance_day();
    require_test(game.world().engine().base().core().core().market("R","food").price>=2,
         "low nominal prices must accrue pressure across month boundaries");
}


static void duplicate_pop_demand_is_rejected() {
    auto w=fixture();
    const auto& slices=w.base().core().core().people().slices();
    const auto buyer=std::next(slices.begin(),2)->first;
    p0::Policies config;
    config.buyers.push_back({buyer,"HH","FIRM","R","food",1});
    config.buyers.push_back({buyer,"HH","FIRM","R","food",1});
    bool rejected=false;
    try{p0::DailyGame game(daily::World::adopt(std::move(w),8),config);}
    catch(const std::invalid_argument&){rejected=true;}
    require_test(rejected,"same POP and commodity must not be counted twice");
}
static void additional_active_cohorts_do_not_block_combat() {
    phasegh::World gh(24000);
    auto& e=gh.seed_geography();
    e.seed_province(1,"R","A");e.seed_unit(11,"A","ROOT");e.seed_assignment(1,11);
    auto& c=e.seed_core().seed_core();
    for(auto who:{"A","ATT","DEF"})c.seed_actor(who,100);
    c.seed_unit("ATT");c.seed_unit("DEF");
    c.seed_stock("ATT","R","ammunition",12);
    c.seed_stock("DEF","R","ammunition",12);
    const auto leadA=people(gh,"R",20,"soldier",phaseb::Service::active);
    const auto reserveA=people(gh,"R",20,"soldier",phaseb::Service::active);
    const auto leadB=people(gh,"R",20,"soldier",phaseb::Service::active);
    const auto reserveB=people(gh,"R",20,"soldier",phaseb::Service::active);
    c.seed_active("ATT",leadA);c.seed_active("ATT",reserveA);
    c.seed_active("DEF",leadB);c.seed_active("DEF",reserveB);
    p0::Policies opts;
    opts.engagements.push_back({"ATT","DEF","R",leadA,leadB,1'000'000});
    // Capture baselines before moving the source world into the day engine.
    // A reference into the moved-from GH world is not a valid oracle.
    const auto population_before=c.people().population();
    const auto events_before=c.people().events().size();
    p0::DailyGame g(daily::World::adopt(std::move(gh),17),opts);
    g.advance_day();
    // Twelve rounds at this calibration wound people but need not kill any.
    require_test(::c(g).people().population()==population_before &&
                 ::c(g).people().events().size()>events_before,
       "extra unit cohorts must not disable actual injury transitions");
    require_test(::c(g).stock("ATT","R","ammunition")<12 &&
                 ::c(g).stock("DEF","R","ammunition")<12,
       "multi-cohort engagement must consume actual ammunition");
}


static void multiple_households_do_not_inflate_one_sellers_stock() {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"R","A");
    geo.seed_unit(11,"A","ROOT");geo.seed_assignment(1,11);
    auto& c=geo.seed_core().seed_core();
    c.seed_actor("A",100);
    c.seed_actor("SELLER",0);
    c.seed_actor("HH1",1000);
    c.seed_actor("HH2",1000);
    c.seed_stock("SELLER","R","food",10);
    c.seed_market("R","food",100,1,300,7);
    const auto h1=people(gh,"R",2,"civilian");
    const auto h2=people(gh,"R",2,"civilian");
    p0::Policies opts;
    opts.buyers.push_back({h1,"HH1","SELLER","R","food",1});
    opts.buyers.push_back({h2,"HH2","SELLER","R","food",1});
    p0::DailyGame game(daily::World::adopt(std::move(gh),5),opts);
    game.advance_day();
    require_test(::c(game).stock("SELLER","R","food")==6,
        "two households must buy four real goods");
    require_test(::c(game).market("R","food").price==100,
        "multiple purchasers must not double count one sellers offered stock");
}
static void a_strike_is_not_a_permanent_hiring_vacancy() {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"R","A");
    geo.seed_unit(11,"A","ROOT");geo.seed_assignment(1,11);
    auto& c=geo.seed_core().seed_core();
    c.seed_actor("A",100);
    c.seed_actor("FIRM",100);
    c.seed_interest_group("MOVEMENT");
    const auto workforce=people(gh,"R",4,"worker");
    c.seed_population().hire(workforce,2,"FIRM","worker",10,
                             phaseb::JobRequirements{});
    phasegh::World::Plan plan;
    plan[9].push_back([workforce](phasegh::Stage& step) {
        auto& c=step.phase_c();
        c.join_interest_group(workforce,"MOVEMENT",2);
        c.protest(workforce,"MOVEMENT",2,2);
    });
    gh.advance(plan);
    require_test(gh.month()==24001,"strike fixture must reach following month");
    p0::Policies opts;
    opts.staffing.push_back({"FIRM","R",2,10});
    p0::DailyGame game(daily::World::adopt(std::move(gh),6),opts);
    game.advance_day();
    require_test(::c(game).people().employed(workforce)==2,
       "temporary strike must not create new permanent employment contracts");
    require_test(::c(game).currently_striking(workforce)==2,
       "actual strike should remain active for the configured period");
}


static void native_autonomous_extraction_physical_flow() {
    auto w=fixture();
    auto& d=w.seed_geography().seed_core();
    auto& core=d.seed_core();
    core.seed_production_facility("MINE","FIRM","R",31);
    core.seed_market("R","coal",20,1,40,6);
    phased::ResourceSite site;
    site.id="COAL-MINE";
    site.site="MINE";site.owner="FIRM";site.region="R";
    site.good="coal";site.monthly_capacity=31;
    site.workers_per_unit=1;site.site_service_per_unit=1;
    d.seed_resource_site(site);
    const auto worker=core.seed_population().slices().begin()->first;
    core.seed_population().hire(worker,4,"FIRM","worker",10,phaseb::JobRequirements{});
    p0::Policies rules;
    rules.extractors.push_back({"COAL-MINE",5,1,"WORKERS","TREASURY"});
    p0::DailyGame game(daily::World::adopt(std::move(w),19),rules);
    game.advance_day();
    require_test(::c(game).stock("FIRM","R","coal")==1 &&
                 ::c(game).cash("WORKERS")==1,
                 "resource extractor did not produce one day of physical output and wages");
    const auto initial=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame replay(daily::SnapshotCodec::decode(initial),rules);
    for(int i=0;i<30;i++) {
        game.advance_day();
        replay.advance_day();
    }
    require_test(daily::SnapshotCodec::encode(game.world())==
                 daily::SnapshotCodec::encode(replay.world()),
                 "resource site native day replay did not preserve actual flow");
    const auto& r=game.world().engine().base().core().resource_site("COAL-MINE");
    require_test(r.cumulative_output==5 &&
                 ::c(game).stock("FIRM","R","coal")==5 &&
                 ::c(game).cash("WORKERS")==5,
                 "resource extractor must stop at actual inventory target");
    game.advance_day(); // February 1, no hidden underground stock regenerated.
    require_test(game.world().engine().base().core().resource_site("COAL-MINE")
                          .cumulative_output==5,
                 "date rollover must not invent or duplicate in-situ output");
}


static void native_autonomous_funded_research_and_day_lab_limit() {
    auto gh=fixture();
    auto& d=gh.seed_geography().seed_core();
    auto& c=d.seed_core();
    c.seed_production_facility("RESEARCH-LAB","FIRM","R",31);
    phased::TechnologyDefinition tech;
    tech.id="TECH-DAILY";
    d.seed_technology(tech);
    phased::ResearchProject project;
    project.id="PROJECT-RESEARCH";
    project.holder="FIRM";
    project.sponsor="A";
    project.lab_site="RESEARCH-LAB";
    project.region="R";
    project.material_good="ore";
    project.tech_id="TECH-DAILY";
    project.material_per_work=1;
    project.required_evidence=2;
    project.workers_per_work=1;
    project.cash_cost_per_work=5;
    d.seed_research(project);
    const auto worker=c.seed_population().slices().begin()->first;
    c.seed_population().hire(worker,4,"FIRM","worker",10,
                             phaseb::JobRequirements{});
    p0::Policies rules;
    rules.researches.push_back({"PROJECT-RESEARCH",1});
    p0::DailyGame game(daily::World::adopt(std::move(gh),23),rules);
    const auto sponsor_cash=::c(game).cash("A");
    const auto materials=::c(game).stock("FIRM","R","ore");
    game.advance_day();
    const auto& progress=game.world().engine().base().core().state().researches.at(
                          "PROJECT-RESEARCH");
    require_test(progress.verified_work==1&&progress.paid_work==1&&
                 !progress.reviewed&&::c(game).cash("A")==sponsor_cash-5&&
                 ::c(game).stock("FIRM","R","ore")==materials-1,
                 "research Day1 did not use actual paid labor, materials and lab");
    auto bytes=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame replay(daily::SnapshotCodec::decode(bytes),rules);
    game.advance_day();replay.advance_day();
    require_test(daily::SnapshotCodec::encode(game.world())==
                 daily::SnapshotCodec::encode(replay.world()),
                 "research stage replay must be deterministic");
    require_test(game.world().engine().base().core().state().researches.at(
                     "PROJECT-RESEARCH").reviewed &&
                 ::c(game).cash("A")==sponsor_cash-10 &&
                 ::c(game).stock("FIRM","R","ore")==materials-2 &&
                 game.world().engine().base().core().knowledge(
                     "FIRM","TECH-DAILY")!=phased::KnowledgeLevel::usable,
                 "real paid research must be reviewed on Day2, not instantly usable");
    for(int day=3;day<=31;++day)game.advance_day();
    game.advance_day(); // February 1: next-month researched technology is usable.
    require_test(game.world().engine().base().core().knowledge(
                     "FIRM","TECH-DAILY")==phased::KnowledgeLevel::usable &&
                 ::c(game).cash("A")==sponsor_cash-10 &&
                 ::c(game).stock("FIRM","R","ore")==materials-2,
                 "next-month knowledge must not duplicate financing or material");
}


static phasegh::World real_h_crisis_and_organization_fixture() {
    phasegh::World w(23998); // November 1999, legacy seeding only
    auto& e=w.seed_geography();
    e.seed_province(1,"RA","A");e.seed_province(2,"RB","B");
    e.seed_province(3,"RC","C");e.seed_border(1,2);e.seed_border(2,3);
    e.seed_unit(11,"A","UA");e.seed_unit(22,"B","UB");
    e.seed_unit(33,"C","UC");
    e.seed_assignment(1,11);e.seed_assignment(2,22);e.seed_assignment(3,33);
    auto& c=e.seed_core().seed_core();
    for(const char* name:{"A","B","C"})c.seed_actor(name,100);
    c.seed_actor("ORG:LEAGUE",0);
    c.seed_government("A","HEAD_A","PM_A",5,3);
    c.seed_government("B","HEAD_B","PM_B",5,3);
    c.seed_government("C","HEAD_C","PM_C",5,3);
    phasegh::World::Plan claimed;
    claimed[9].push_back([](phasegh::Stage& h){
        h.geography().queue_claim("CLAIM-A-RB",2,"A",true);
    });
    w.advance(claimed); // December 1999, true claim in E
    phasegh::World::Plan organized;
    organized[9].push_back([](phasegh::Stage& h){
        h.open_crisis("CRISIS-RB",2,"A","B","PM_A");
        h.create_organization("LEAGUE",{"A","B"},10,
                              {{"A","PM_A"},{"B","PM_B"}});
    });
    w.advance(organized); // January 2000, legal pending activation
    require_test(w.month()==24000,"crisis fixture not aligned to 2000");
    return w;
}

static void native_autonomous_h_crisis_and_organization_funding() {
    auto gh=real_h_crisis_and_organization_fixture();
    auto day=daily::World::adopt(std::move(gh),77);
    phasegh::World::Plan mediated;
    mediated[9].push_back([](phasegh::Stage& s) {
        s.mediate_crisis("CRISIS-RB","C","PM_C");
        phasegh::CrisisProposal offer;
        offer.id="REAL-OFFER";offer.proposer="C";
        offer.payer="B";offer.recipient="A";offer.compensation=20;
        offer.freeze_claim=true;
        s.offer_crisis_compromise("CRISIS-RB",offer,"PM_C");
    });
    day.advance_day(mediated); // Jan 1: real H legal state and proposal
    require_test(day.engine().diplomacy().crises.at("CRISIS-RB").status==
                 phasegh::CrisisStatus::mediation &&
                 day.engine().diplomacy().organizations.at("LEAGUE").active,
                 "H real legal effectiveness or neutral mediation failed");

    p0::Policies mandates;
    mandates.crisis_mandates.push_back({"A","PM_A",0,true,false});
    mandates.crisis_mandates.push_back({"B","PM_B",30,true,false});
    mandates.organization_dues.push_back({"A","PM_A","LEAGUE"});
    mandates.organization_dues.push_back({"B","PM_B","LEAGUE"});
    p0::DailyGame game(std::move(day),mandates);
    game.advance_day(); // Jan 2: actual country policy decisions and payments
    const auto& org=game.world().engine().diplomacy().organizations.at("LEAGUE");
    const auto& crisis=game.world().engine().diplomacy().crises.at("CRISIS-RB");
    require_test(crisis.status==phasegh::CrisisStatus::settled &&
                 crisis.proposals.at("REAL-OFFER").approvers.size()==2 &&
                 c(game).cash("A")==90 && c(game).cash("B")==90 &&
                 c(game).cash("ORG:LEAGUE")==20 &&
                 org.paid.at({24000,"A"})==10 && org.paid.at({24000,"B"})==10,
                 "H autonomous diplomacy must use actual signed offers/dues/treasury");
    const auto snapshot=daily::SnapshotCodec::encode(game.world());
    auto replay=p0::DailyGame(daily::SnapshotCodec::decode(snapshot),mandates);
    game.advance_day();replay.advance_day();
    require_test(daily::SnapshotCodec::encode(game.world())==
                 daily::SnapshotCodec::encode(replay.world()) &&
                 c(game).cash("ORG:LEAGUE")==20,
                 "H autonomous obligations replayed or paid duplicate monthly dues");

    auto gh_unwilling=real_h_crisis_and_organization_fixture();
    auto pending=daily::World::adopt(std::move(gh_unwilling),77);
    pending.advance_day(mediated);
    p0::Policies no_coercion=mandates;
    no_coercion.crisis_mandates[1].maximum_cash_settlement=5;
    p0::DailyGame limited(std::move(pending),no_coercion);
    limited.advance_day();
    require_test(limited.world().engine().diplomacy().crises.at("CRISIS-RB").status==
                 phasegh::CrisisStatus::mediation &&
                 limited.world().engine().diplomacy().crises.at("CRISIS-RB").
                      proposals.at("REAL-OFFER").approvers.size()==1,
                 "unaffordable crisis concession must not be accepted automatically");

    auto gh_unauthorized=real_h_crisis_and_organization_fixture();
    auto unauthorized=daily::World::adopt(std::move(gh_unauthorized));
    unauthorized.advance_day(mediated);
    p0::Policies wrong=mandates;
    wrong.organization_dues[0].prime_minister="INVALID-PRIME";
    p0::DailyGame rejected(std::move(unauthorized),wrong);
    const auto before=daily::SnapshotCodec::encode(rejected.world());
    bool failed=false;
    try{rejected.advance_day();}catch(const std::exception&){failed=true;}
    require_test(failed && daily::SnapshotCodec::encode(rejected.world())==before,
                 "H unlawful government policy must rollback every daily domain");
}


static void autonomous_f_two_hop_freight_and_save_replay() {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"A-HUB","A");
    geo.seed_province(2,"B-HUB","B");
    geo.seed_province(3,"C-HUB","C");
    geo.seed_border(1,2);geo.seed_border(2,3);
    geo.seed_unit(11,"A","UA");geo.seed_unit(22,"B","UB");
    geo.seed_unit(33,"C","UC");
    geo.seed_assignment(1,11);geo.seed_assignment(2,22);
    geo.seed_assignment(3,33);
    auto& initial_core=geo.seed_core().seed_core();
    initial_core.seed_actor("SELLER",100);initial_core.seed_actor("BUYER",100);
    initial_core.seed_actor("CARRIER",100);
    initial_core.seed_stock("SELLER","A-HUB","iron",3);
    gh.seed_transport_edge({"HOP-A-B","CARRIER",1,2,310,1,1,true});
    gh.seed_transport_edge({"HOP-B-C","CARRIER",2,3,310,1,1,true});
    phasef::FreightLane lane;
    lane.id="IRON";lane.seller="SELLER";lane.buyer="BUYER";
    lane.good="iron";lane.origin_region="A-HUB";
    lane.destination_region="C-HUB";
    lane.target_inventory=2;lane.maximum_daily_units=2;
    lane.edge_transit_days={{"HOP-A-B",1},{"HOP-B-C",1}};
    p0::Policies options;
    options.freight_lanes.push_back(lane);
    p0::DailyGame game(daily::World::adopt(std::move(gh),55),options);
    game.advance_day(); // Jan 1: autonomous A->B dispatch, actual freight
    require_test(game.world().cargo().size()==1,
                 "autonomous F cargo must be tracked under exact dispatch ID");
    const auto first=game.world().cargo().begin()->second;
    require_test(first.origin=="A-HUB"&&first.destination=="B-HUB" &&
                 c(game).stock("SELLER","A-HUB","iron")==1&&
                 c(game).cash("SELLER")==98 &&
                 c(game).cash("CARRIER")==102,
                 "first F hop must move real stock and pay actual freight");
    game.advance_day(); // Jan 2: arrival at intermediate hub
    require_test(c(game).stock("BUYER","B-HUB","iron")==2 &&
                 game.world().cargo().begin()->second.arrived,
                 "F did not deliver to actual intermediate owner");
    const auto checkpoint=daily::SnapshotCodec::encode(game.world());
    auto replay=p0::DailyGame(daily::SnapshotCodec::decode(checkpoint),options);
    game.advance_day();replay.advance_day(); // Jan 3: autonomous B->C
    require_test(game.world().cargo().size()==2 &&
                 c(game).stock("BUYER","B-HUB","iron")==0 &&
                 c(game).cash("BUYER")==98 &&
                 c(game).cash("CARRIER")==104,
                 "second real carrier must be charged exactly once");
    game.advance_day();replay.advance_day(); // Jan 4: final delivery
    require_test(daily::SnapshotCodec::encode(game.world())==
                 daily::SnapshotCodec::encode(replay.world())&&
                 c(game).stock("BUYER","C-HUB","iron")==2,
                 "multi-hop F forwarding/replay failed or stock duplicated");
    game.advance_day(); // Jan 5: target met, no extra movement
    require_test(game.world().cargo().size()==2 &&
                 c(game).stock("SELLER","A-HUB","iron")==1,
                 "fulfilled autonomous F lane should stop shipping");

    phasegh::World missing(24000);
    auto& eg=missing.seed_geography();
    eg.seed_province(1,"R1","A");eg.seed_province(2,"R2","B");
    eg.seed_border(1,2);
    eg.seed_unit(11,"A","UA");eg.seed_unit(22,"B","UB");
    eg.seed_assignment(1,11);eg.seed_assignment(2,22);
    auto& cc=eg.seed_core().seed_core();
    cc.seed_actor("S",100);cc.seed_actor("B",100);cc.seed_actor("CARRIER",100);
    cc.seed_stock("S","R1","iron",3);
    missing.seed_transport_edge({"NO-DURATION","CARRIER",1,2,310,1,1,true});
    phasef::FreightLane bad_lane;
    bad_lane.id="BAD";bad_lane.seller="S";bad_lane.buyer="B";
    bad_lane.good="iron";bad_lane.origin_region="R1";
    bad_lane.destination_region="R2";bad_lane.target_inventory=1;
    bad_lane.maximum_daily_units=1;bad_lane.edge_transit_days={{"UNRELATED",1}};
    p0::Policies policy;policy.freight_lanes.push_back(bad_lane);
    p0::DailyGame invalid(daily::World::adopt(std::move(missing)),policy);
    const auto before=daily::SnapshotCodec::encode(invalid.world());
    bool refused=false;
    try{invalid.advance_day();}catch(const std::exception&){refused=true;}
    require_test(refused && daily::SnapshotCodec::encode(invalid.world())==before,
                 "missing grounded transit days must not fabricate route or post money");
}


static void autonomous_daily_facility_maintenance_with_real_inputs() {
    auto w=fixture();
    auto& seeded=w.seed_geography().seed_core().seed_core();
    seeded.seed_stock("FIRM","R","steel",3);
    auto& persons=seeded.seed_population();
    const auto labor_slice=persons.slices().begin()->first;
    persons.hire(labor_slice,4,"FIRM","technician",10,
                 phaseb::JobRequirements{});
    auto world=daily::World::adopt(std::move(w),99);
    phasegh::World::Plan damage;
    damage[10].push_back([](phasegh::Stage& stage) {
        stage.phase_c().war_damage_facility("FARM",3);
    });
    world.advance_day(damage); // first day causes real physical damage
    require_test(world.engine().base().core().core().facility("FARM").damaged==3,
                 "unfunded maintenance fixture failed to create damage");
    p0::Policies policy;
    policy.maintenance.push_back({"FARM","steel",2,1,1,1,
                                  "WORKERS","TREASURY"});
    p0::DailyGame game(std::move(world),policy);
    const auto before=daily::SnapshotCodec::encode(game.world());
    phasegh::World::Plan fail;
    fail[9].push_back([](phasegh::Stage&){
        throw std::runtime_error("injected later stage failure");
    });
    bool aborted=false;
    try{game.advance_day(fail);}catch(const std::runtime_error&){aborted=true;}
    require_test(aborted &&
                 daily::SnapshotCodec::encode(game.world())==before,
                 "late failure must roll back materials, wages, repaired capacity and day");

    game.advance_day(); // day 2: two real restoration units
    require_test(c(game).facility("FARM").damaged==1 &&
                 c(game).stock("FIRM","R","steel")==1 &&
                 c(game).cash("FIRM")==1998 &&
                 c(game).cash("WORKERS")==2,
                 "D day-native maintenance must restore capacity and use real C ledger");
    const auto checkpoint=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame replay(daily::SnapshotCodec::decode(checkpoint),policy);
    game.advance_day();replay.advance_day(); // day 3, remaining one unit
    require_test(c(game).facility("FARM").damaged==0 &&
                 c(game).stock("FIRM","R","steel")==0 &&
                 c(game).cash("FIRM")==1997 &&
                 c(game).cash("WORKERS")==3 &&
                 daily::SnapshotCodec::encode(game.world())==
                 daily::SnapshotCodec::encode(replay.world()),
                 "D maintenance replay/remaining capacity accounting diverged");
    game.advance_day();replay.advance_day();
    require_test(c(game).facility("FARM").damaged==0 &&
                 c(game).cash("FIRM")==1997 &&
                 daily::SnapshotCodec::encode(game.world())==
                 daily::SnapshotCodec::encode(replay.world()),
                 "D no-damage day must never book phantom labor, stock or wages");

    p0::Policies duplicate=policy;
    duplicate.maintenance.push_back(policy.maintenance.front());
    bool refused=false;
    try {p0::DailyGame illegal(daily::SnapshotCodec::decode(checkpoint),duplicate);}
    catch(const std::invalid_argument&){refused=true;}
    require_test(refused,"duplicate owner-site maintenance policy must be rejected");
}


static daily::World seed_lawful_autonomous_treaty_and_council() {
    auto gh=real_h_crisis_and_organization_fixture();
    auto world=daily::World::adopt(std::move(gh),912);
    phasegh::World::Plan setup;
    setup[9].push_back([](phasegh::Stage& stage){
        phasegh::TreatyClause clause;
        clause.id="FISCAL-AID";clause.kind=phasegh::ClauseKind::financial_aid;
        clause.obligor="B";clause.beneficiary="A";clause.amount=20;
        stage.propose_treaty_package("PACT-A-B",{"A","B"},
                                    {clause},"A","PM_A");
        phasegh::OrganizationRule rule;
        rule.quorum=2;rule.yes_required=1;
        rule.veto_members.insert("B");
        stage.configure_organization("LEAGUE",rule,"PM_A");
    });
    world.advance_day(setup); // January 1: lawful C/H proposal, no AI fiat
    require_test(world.engine().diplomacy().organizations.at("LEAGUE").active,
                 "real H organization must be active before votes");
    return world;
}

static void autonomous_h_treaty_risk_and_institutional_vote() {
    p0::Policies policy;
    policy.treaty_ratifications.push_back({"B","PM_B",20,false,false,false});
    policy.council_decisions.push_back(
        {"LEAGUE","COUNCIL-APPROVE","research:cooperation",
         "A","PM_A",{{"A","PM_A",50},{"B","PM_B",50}}});
    policy.council_decisions.push_back(
        {"LEAGUE","COUNCIL-VETO","trade:embargo",
         "A","PM_A",{{"A","PM_A",50},{"B","PM_B",200}}});
    auto world=seed_lawful_autonomous_treaty_and_council();
    p0::DailyGame game(std::move(world),policy);
    const Money beforeA=c(game).cash("A"),beforeB=c(game).cash("B");
    game.advance_day(); // January 2: lawful country AI decisions
    const auto& h=game.world().engine().diplomacy();
    require_test(h.treaties.at("PACT-A-B").ratifications.contains("B") &&
                 !h.treaties.at("PACT-A-B").active &&
                 h.organizations.at("LEAGUE").decisions.at("COUNCIL-APPROVE").adopted &&
                 !h.organizations.at("LEAGUE").decisions.at("COUNCIL-VETO").adopted &&
                 c(game).cash("A")==beforeA && c(game).cash("B")==beforeB,
                 "H AI must respect real treaty ratification, organization quorum/veto and no free money");

    const auto checkpoint=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame replay(daily::SnapshotCodec::decode(checkpoint),policy);
    game.advance_day();replay.advance_day();
    require_test(daily::SnapshotCodec::encode(game.world())==
                 daily::SnapshotCodec::encode(replay.world()) &&
                 h.organizations.at("LEAGUE").decisions.size()==2,
                 "H dated AI treaties/council decisions must be idempotent on replay");
    while(game.world().date()!=daily::Date{2000,2,1})
        game.advance_day(); // no backdated H legal effects until due month
    require_test(!game.world().engine().diplomacy().
                   treaties.at("PACT-A-B").active,
                 "H treaty must not retroactively activate during January");
    game.advance_day();
    require_test(game.world().engine().diplomacy().
                    treaties.at("PACT-A-B").active,
                 "approved H treaty must activate at actual February 1 opening");

    p0::Policies unwilling=policy;
    unwilling.treaty_ratifications[0].maximum_new_fiscal_obligation=5;
    p0::DailyGame limited(seed_lawful_autonomous_treaty_and_council(),unwilling);
    limited.advance_day();
    require_test(!limited.world().engine().diplomacy().
                   treaties.at("PACT-A-B").ratifications.contains("B"),
                 "country AI must not approve liabilities above declared fiscal mandate");

    p0::Policies impersonator=policy;
    impersonator.council_decisions[0].member_rules[1].prime_minister="INVALID";
    p0::DailyGame illegal(seed_lawful_autonomous_treaty_and_council(),impersonator);
    const auto prior=daily::SnapshotCodec::encode(illegal.world());
    bool denied=false;
    try{illegal.advance_day();}catch(const std::exception&){denied=true;}
    require_test(denied && prior==daily::SnapshotCodec::encode(illegal.world()),
                 "H forged member ballot must roll back earlier same-day treaty and all ledgers");

    p0::Policies duplicate=policy;
    duplicate.council_decisions.push_back(duplicate.council_decisions.front());
    bool rejected=false;
    try{p0::DailyGame invalid(seed_lawful_autonomous_treaty_and_council(),
                              duplicate);}
    catch(const std::invalid_argument&){rejected=true;}
    require_test(rejected,"duplicate H institutional decision must fail policy validation");
}

int main(){
    try{
        daily_production_trade_ai_investment_replay();
        daily_market_shortage_and_political_response();
        daily_insolvent_hunger_is_not_ignored();
        daily_price_not_replayed_at_month_close();
        low_unit_price_still_changes_across_months();
        daily_battle_consumes_actual_ammo_and_pop();
        additional_active_cohorts_do_not_block_combat();
        duplicate_pop_demand_is_rejected();
        multiple_households_do_not_inflate_one_sellers_stock();
        a_strike_is_not_a_permanent_hiring_vacancy();
        native_autonomous_extraction_physical_flow();
        native_autonomous_funded_research_and_day_lab_limit();
        native_autonomous_h_crisis_and_organization_funding();
        autonomous_f_two_hop_freight_and_save_replay();
        autonomous_daily_facility_maintenance_with_real_inputs();
        autonomous_h_treaty_risk_and_institutional_vote();
        std::cout<<"P0 daily grand strategy: 12 bounded integration scenarios PASS\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"P0 DAILY TEST FAILED: "<<e.what()<<"\n";
        return 1;
    }
}
