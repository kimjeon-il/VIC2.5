#include "vic25/p0_daily.hpp"
#include "vic25/daily_snapshot.hpp"
#include <iostream>
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
    require_test(c(game).stock("FIRM","R","food")>0,"real daily industry did not run");
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
int main(){
    try{
        daily_production_trade_ai_investment_replay();
        daily_market_shortage_and_political_response();
        daily_battle_consumes_actual_ammo_and_pop();
        std::cout<<"P0 daily grand strategy: 3 bounded integration scenarios PASS\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"P0 DAILY TEST FAILED: "<<e.what()<<"\n";
        return 1;
    }
}
