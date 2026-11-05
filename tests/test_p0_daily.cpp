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
    require_test(c(game).market("R","food").price>=2,
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
    p0::DailyGame g(daily::World::adopt(std::move(gh),17),opts);
    const auto population_before=c.people().population();
    const auto events_before=c.people().events().size();
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
        std::cout<<"P0 daily grand strategy: 10 bounded integration scenarios PASS\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"P0 DAILY TEST FAILED: "<<e.what()<<"\n";
        return 1;
    }
}
