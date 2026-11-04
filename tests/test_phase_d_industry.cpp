#include "vic25/phase_d_snapshot.hpp"
#include <iostream>
#include <stdexcept>
using namespace vic25::phased;
using vic25::phaseb::Sex; using vic25::phaseb::Service;
static void need(bool x,const char* m){if(!x)throw std::runtime_error(m);}
template<class F>static bool rejects(F&& f){try{f();}catch(const std::exception&){return true;}return false;}
static Id worker(World& w,People n,Id partition){
    auto& p=w.seed_core().seed_population();
    auto h=p.household_profile({"R","shared","shared",n});
    auto e=p.economic_profile({"R","worker","wage","employee",p.default_education()});
    auto d=p.demographic_profile({"R",w.month()-360,Sex::female,"culture","faith","citizen","free",p.default_health(),Service::civilian});
    return p.seed_population(d,e,h,n,partition);
}
static World world(){
    World w{4000};auto& c=w.seed_core();
    c.seed_actor("MINE",0);c.seed_actor("MILL",50);c.seed_actor("FOREST",0);
    c.seed_region("R","STATE");
    c.seed_production_facility("MINE_SITE","MINE","R",5);
    c.seed_production_facility("MILL_SITE","MILL","R",5);
    c.seed_production_facility("FOREST_SITE","FOREST","R",3);
    c.seed_stock("MILL","R","coal",2);
    w.seed_industry_class({"PRIMARY","","raw materials"});
    w.seed_industry_class({"EXTR","PRIMARY","mining"});
    w.seed_industry_class({"MANU","","manufacture"});
    ResourceSite ore;
    ore.id="ORE";ore.site="MINE_SITE";ore.owner="MINE";ore.region="R";
    ore.good="ore";ore.monthly_capacity=5;ore.decline_ppm_active_month=200'000;
    w.seed_resource_site(ore);
    ResourceSite forest;
    forest.id="FOREST";forest.site="FOREST_SITE";forest.owner="FOREST";
    forest.region="R";forest.good="timber";forest.monthly_capacity=3;
    forest.standing_biomass=2;forest.renewal_per_month=1;
    forest.carrying_capacity=4;forest.renewable=true;
    w.seed_resource_site(forest);
    IndustryRecipe r;
    r.id="SMELT";r.site="MILL_SITE";r.industry_class="MANU";
    r.inputs={{"ore",1},{"coal",1}};
    r.outputs={{"steel",2}};r.byproducts={{"slag",1}};
    r.nonmarket_waste_per_batch=1;r.workers_per_batch=1;
    w.seed_recipe(r);return w;
}
static void test_mining_trade_joint_outputs(){
    auto w=world();auto miner=worker(w,3,11),maker=worker(w,2,12);
    World::Plan p;
    p[2].push_back([miner,maker](Stage& s){
        s.phase_c().hire(miner,3,"MINE",1);
        s.phase_c().hire(maker,2,"MILL",1);
    });
    p[4].push_back([](Stage& s){s.extract_resource("ORE",3);});
    p[7].push_back([](Stage& s){s.phase_c().trade("MINE","MILL","R","ore",3,2);});
    w.advance(p);
    need(w.resource_site("ORE").cumulative_output==3 && w.core().stock("MILL","R","ore")==3 &&
         w.core().cash("MILL")==44 &&
         w.realized_batches("MANU",4000,4000)==0,"ore extraction, sales, industry timing");
    World::Plan next;
    next[4].push_back([](Stage& s){s.manufacture("SMELT",2);});
    w.advance(next);
    need(w.core().stock("MILL","R","ore")==1 &&
         w.core().stock("MILL","R","coal")==0 &&
         w.core().stock("MILL","R","steel")==4 &&
         w.core().stock("MILL","R","slag")==2 &&
         w.nonmarket_waste("SMELT")==2 &&
         w.realized_batches("MANU",4001,4001)==2,
         "joint outputs or industrial statistics");
    const auto saved=SnapshotCodec::encode(w);
    auto recovered=SnapshotCodec::decode(saved);
    need(SnapshotCodec::encode(recovered)==saved&&recovered.resource_site("ORE").cumulative_output==3,
         "resource and output records snapshot mismatch");
    World::Plan over;
    over[4].push_back([](Stage& s){s.extract_resource("ORE",5);});
    need(rejects([&]{w.advance(over);}) &&
         w.resource_site("ORE").cumulative_output==3 &&
         w.resource_site("ORE").monthly_capacity==4 &&
         w.core().stock("MINE","R","ore")==0,
         "flow-capacity violation leaked through rollback");
}
static void test_no_mineral_total_and_activity_decline(){
    auto w=world();auto miner=worker(w,5,27);
    World::Plan first;
    first[2].push_back([miner](Stage& s){s.phase_c().hire(miner,5,"MINE",1);});
    first[4].push_back([](Stage& s){s.extract_resource("ORE",5);});
    w.advance(first);
    World::Plan second;
    second[4].push_back([](Stage& s){s.extract_resource("ORE",4);});
    w.advance(second);
    need(w.resource_site("ORE").cumulative_output==9 &&
         w.resource_site("ORE").monthly_capacity==4 &&
         w.core().stock("MINE","R","ore")==9,
         "nonrenewable output must not be capped by any legacy total");
    // Fractional ppm remainder is accumulated, not rounded up prematurely.
    World::Plan third;
    third[4].push_back([](Stage& s){s.extract_resource("ORE",4);});
    w.advance(third);
    need(w.resource_site("ORE").monthly_capacity==4 &&
         w.resource_site("ORE").decline_remainder==800'000,
         "sub-unit decline must be kept as fractional remainder");
    w.advance();
    need(w.resource_site("ORE").monthly_capacity==3 &&
         w.resource_site("ORE").cumulative_output==13,
         "activity-based monthly flow decline missing");
    const auto bytes=SnapshotCodec::encode(w);
    auto restored=SnapshotCodec::decode(bytes);
    need(SnapshotCodec::encode(restored)==bytes &&
         restored.resource_site("ORE").cumulative_output==13,
         "flow-only source snapshot roundtrip failed");
}
static void test_renewable_carrying_capacity(){
    auto w=world();auto id=worker(w,3,13);
    World::Plan first;
    first[2].push_back([id](Stage& s){s.phase_c().hire(id,3,"FOREST",1);});
    first[4].push_back([](Stage& s){s.extract_resource("FOREST",2);});
    w.advance(first);
    need(w.resource_site("FOREST").standing_biomass==1 &&
         w.core().stock("FOREST","R","timber")==2,"renewal and harvest balance");
    w.advance();need(w.resource_site("FOREST").standing_biomass==2,"renewal month2");
    w.advance();w.advance();
    need(w.resource_site("FOREST").standing_biomass==4,"renewal exceeded carrying capacity");
}
static void test_same_month_factory_output_not_usable_as_past_input(){
    World w{4200};auto& c=w.seed_core();
    c.seed_actor("A",10);c.seed_region("R","S");
    c.seed_production_facility("SITE","A","R",2);
    c.seed_stock("A","R","ore",1);
    auto id=worker(w,2,14);
    IndustryRecipe a;a.id="A";a.site="SITE";
    a.inputs={{"ore",1}};a.outputs={{"bar",1}};a.workers_per_batch=1;
    IndustryRecipe b=a;b.id="B";b.inputs={{"bar",1}};b.outputs={{"tool",1}};
    w.seed_recipe(a);w.seed_recipe(b);
    World::Plan p;
    p[2].push_back([id](Stage& s){s.phase_c().hire(id,2,"A",1);});
    p[4].push_back([](Stage& s){s.manufacture("A",1);s.manufacture("B",1);});
    need(rejects([&]{w.advance(p);})&&w.month()==4200 &&
         w.core().stock("A","R","ore")==1&&w.core().stock("A","R","bar")==0,
         "same Stage4 created intermediate bypassed Stage3");
}
int main(){
    try{
        test_mining_trade_joint_outputs();
        test_no_mineral_total_and_activity_decline();
        test_renewable_carrying_capacity();
        test_same_month_factory_output_not_usable_as_past_input();
        std::cout<<"Phase D industry extraction, coproduct and timing cases PASS\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"Industry FAIL: "<<e.what()<<"\n";return 1;
    }
}
