#include "vic25/phase_d.hpp"
#include <iostream>
#include <stdexcept>

using namespace vic25::phased;
using vic25::phaseb::Sex;
using vic25::phaseb::Service;
static void need(bool x,const char* msg){if(!x)throw std::runtime_error(msg);}
template<class Fn>static bool fails(Fn&& f){try{f();}catch(const std::exception&){return true;}return false;}
static Id people(World& w,People n){
    auto& pop=w.seed_core().seed_population();
    const auto household=pop.household_profile({"R","shared","shared",n});
    const auto econ=pop.economic_profile({"R","worker","wage","employee",pop.default_education()});
    const auto demo=pop.demographic_profile({"R",w.month()-300,Sex::female,"culture","faith",
                                             "citizen","free",pop.default_health(),Service::civilian});
    return pop.seed_population(demo,econ,household,n);
}
static World base(){
    World w{2600};
    auto& c=w.seed_core();
    c.seed_actor("OWNER",200);
    c.seed_actor("BUILDER",0);
    c.seed_actor("OTHER",0);
    c.seed_region("R","STATE");
    c.seed_stock("BUILDER","R","steel",2);
    c.seed_stock("BUILDER","R","ore",3);
    c.seed_production_facility("B_FACILITY","BUILDER","R",2);
    ConstructionProject p;
    p.id="CONSTRUCT";p.client="OWNER";p.contractor="BUILDER";
    p.target_site="NEW_FACTORY";p.region="R";p.material_good="steel";
    p.work_required=2;p.material_per_work=1;p.workers_per_work=1;
    p.cash_price_per_work=30;p.target_capacity=1;
    w.seed_construction(p);
    IndustryRecipe r;
    r.id="INDUSTRY";r.site="B_FACILITY";
    r.inputs={{"ore",1}};r.outputs={{"bars",1}};
    r.workers_per_batch=1;w.seed_recipe(r);
    return w;
}
static void test_legacy_cash_does_not_steal_reserved_phase_d_budget(){
    auto w=base();
    World::Plan p;
    p[1].push_back([](Stage& s){
        s.plan_work("CONSTRUCT",2);
        need(s.phase_c().phase_d_shared_cash_held("OWNER")==60,
             "expected exactly one shared Phase C reservation");
    });
    p[5].push_back([](Stage& s){s.phase_c().pay("OWNER","OTHER",150);});
    need(fails([&]{w.advance(p);}) && w.month()==2600 &&
         w.core().cash("OWNER")==200 &&
         w.core().cash("OTHER")==0 && w.commit_id()==0,
         "legacy C payment stole Phase D reserved money or failed to rollback");
}
static void test_shared_budget_legal_spending_then_phase_d_payment(){
    auto w=base();const auto builder=people(w,2);
    World::Plan p;
    p[1].push_back([](Stage& s){s.plan_work("CONSTRUCT",2);});
    p[2].push_back([builder](Stage& s){s.phase_c().hire(builder,2,"BUILDER",1);});
    p[4].push_back([](Stage& s){s.perform_work("CONSTRUCT",2);});
    p[5].push_back([](Stage& s){
        s.phase_c().pay("OWNER","OTHER",140); // remaining unreserved 140
        s.pay_work("CONSTRUCT",2);          // consume reserved 60
    });
    p[8].push_back([](Stage& s){s.capitalize_work("CONSTRUCT",2);});
    w.advance(p);
    need(w.core().cash("OWNER")==0 &&
         w.core().cash("OTHER")==140 && w.core().cash("BUILDER")==60 &&
         w.core().phase_d_wip("OWNER")==60 &&
         w.core().phase_d_shared_cash_held("OWNER")==0,
         "shared Phase C/D cash reservation did not reconcile");
}
static void test_legacy_production_competes_for_d_labor(){
    auto w=base();const auto staff=people(w,2);
    World::Plan p;
    p[2].push_back([staff](Stage& s){s.phase_c().hire(staff,2,"BUILDER",1);});
    p[4].push_back([](Stage& s){
        s.phase_c().produce("BUILDER","R","ore","bars",1,2);
        s.manufacture("INDUSTRY",1);
    });
    need(fails([&]{w.advance(p);}) && w.month()==2600 &&
         w.core().stock("BUILDER","R","ore")==3 &&
         w.core().stock("BUILDER","R","bars")==0,
         "legacy C and D duplicated the same POP labor service");
}
static void test_shared_site_capacity(){
    auto w=base();const auto staff=people(w,2);
    World::Plan p;
    p[2].push_back([staff](Stage& s){s.phase_c().hire(staff,2,"BUILDER",1);});
    p[4].push_back([](Stage& s){
        s.manufacture("INDUSTRY",2);
        s.manufacture("INDUSTRY",1);
    });
    need(fails([&]{w.advance(p);})&&w.core().stock("BUILDER","R","ore")==3,
         "same site capacity double-allocated");
}
static void test_goods_reservation_blocks_legacy_and_releases_when_used(){
    auto w=base();const auto staff=people(w,2);
    World::Plan fail;
    fail[2].push_back([staff](Stage& s){s.phase_c().hire(staff,2,"BUILDER",1);});
    fail[3].push_back([](Stage& s){
        s.phase_c().phase_d_reserve_material("BUILDER","R","ore","R&D",2);
    });
    fail[4].push_back([](Stage& s){
        s.phase_c().produce("BUILDER","R","ore","bars",2,1);
    });
    need(fails([&]{w.advance(fail);}) && w.core().stock("BUILDER","R","ore")==3,
         "legacy production consumed goods reserved by another process");
    World::Plan pass;
    pass[2].push_back([staff](Stage& s){s.phase_c().hire(staff,2,"BUILDER",1);});
    pass[3].push_back([](Stage& s){
        s.phase_c().phase_d_reserve_material("BUILDER","R","ore","R&D",2);
    });
    pass[4].push_back([](Stage& s){
        s.phase_c().phase_d_use_reserved_material("BUILDER","R","ore","R&D",2);
    });
    w.advance(pass);
    need(w.core().stock("BUILDER","R","ore")==1,
         "real reserved material was not consumed exactly once");
}
int main(){
    try{
        test_legacy_cash_does_not_steal_reserved_phase_d_budget();
        test_shared_budget_legal_spending_then_phase_d_payment();
        test_legacy_production_competes_for_d_labor();
        test_shared_site_capacity();
        test_goods_reservation_blocks_legacy_and_releases_when_used();
        std::cout<<"Shared Phase C/D reservation integration 5 cases PASS\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"Shared reservation FAIL: "<<e.what()<<"\n";return 1;
    }
}
