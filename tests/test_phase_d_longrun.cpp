#include "vic25/phase_d_snapshot.hpp"
#include <iostream>
#include <stdexcept>

using namespace vic25::phased;
using vic25::phaseb::Sex;
using vic25::phaseb::Service;

static void need(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
static Id cohort(World& w,People n){
    auto& p=w.seed_core().seed_population();
    auto h=p.household_profile({"R","shared","shared",n});
    auto e=p.economic_profile({"R","worker","wage","employee",p.default_education()});
    auto d=p.demographic_profile({"R",w.month()-300,Sex::male,"culture","faith",
                                   "citizen","free",p.default_health(),Service::civilian});
    return p.seed_population(d,e,h,n,static_cast<Id>(p.slices().size()+1));
}
static World initial() {
    World w{1800};
    auto& c=w.seed_core();
    c.seed_actor("FACTORY",500);
    c.seed_actor("CLIENT",100);
    c.seed_actor("BUILDER",0);
    c.seed_actor("GOV",50);
    c.seed_actor("LAB",0);
    c.seed_region("R","A");
    c.seed_stock("FACTORY","R","ore",600);
    c.seed_stock("FACTORY","R","building_materials",1);
    c.seed_stock("BUILDER","R","steel",2);
    c.seed_stock("LAB","R","reagent",2);
    c.seed_production_facility("FACTORY_SITE","FACTORY","R",2);
    c.seed_production_facility("LAB_SITE","LAB","R",2);
    auto builder=cohort(w,2),workers=cohort(w,2),researchers=cohort(w,2);

    ConstructionProject p;
    p.id="P";p.client="CLIENT";p.contractor="BUILDER";p.region="R";
    p.material_good="steel";p.target_site="NEW_FACTORY";
    p.work_required=2;p.material_per_work=1;p.workers_per_work=1;
    p.cash_price_per_work=20;p.target_capacity=3;
    w.seed_construction(p);

    IndustryRecipe recipe;
    recipe.id="ORE_TO_BAR";recipe.site="FACTORY_SITE";
    recipe.inputs={{"ore",1}};recipe.outputs={{"bar",1}};
    recipe.workers_per_batch=1;
    w.seed_recipe(recipe);

    ResearchProject r;
    r.id="R";r.holder="LAB";r.sponsor="GOV";r.lab_site="LAB_SITE";
    r.region="R";r.material_good="reagent";r.tech_id="KNOWLEDGE";
    r.material_per_work=1;r.required_evidence=2;r.workers_per_work=1;
    r.cash_cost_per_work=10;
    w.seed_research(r);

    // Persist cohort IDs as explicit C++ command context, not a second headcount.
    World::Plan p0;
    p0[1].push_back([](Stage& s){
        s.plan_work("P",1);
        s.plan_research("R",1);
    });
    p0[2].push_back([builder,workers,researchers](Stage& s){
        s.phase_c().hire(builder,2,"BUILDER",2);
        s.phase_c().hire(workers,2,"FACTORY",2);
        s.phase_c().hire(researchers,2,"LAB",2);
    });
    p0[4].push_back([](Stage& s){
        s.manufacture("ORE_TO_BAR",1);
        s.perform_work("P",1);
        s.perform_research("R",1);
    });
    p0[5].push_back([](Stage& s){
        s.pay_work("P",1);
        s.pay_research("R",1);
    });
    p0[8].push_back([](Stage& s){s.capitalize_work("P",1);});
    w.advance(p0);
    return w;
}
static World run(){
    auto w=initial();
    for(int i=1;i<1200;++i) {
        World::Plan p;
        if(i==1){
            p[1].push_back([](Stage& s){
                s.plan_work("P",1);
                s.plan_research("R",1);
            });
            p[4].push_back([](Stage& s){
                s.perform_work("P",1);
                s.perform_research("R",1);
            });
            p[5].push_back([](Stage& s){
                s.pay_work("P",1);
                s.pay_research("R",1);
            });
            p[8].push_back([](Stage& s){s.capitalize_work("P",1);});
            p[9].push_back([](Stage& s){s.review_research("R");});
        }
        if(i<600) {
            p[4].push_back([](Stage& s){
                const auto& f=s.phase_c().facility("FACTORY_SITE");
                if(f.capacity>f.damaged)
                    s.manufacture("ORE_TO_BAR",1);
            });
        }
        if(i==250||i==400)
            p[10].push_back([](Stage& s){s.phase_c().war_damage_facility("FACTORY_SITE",1);});
        if(i==500)
            p[4].push_back([](Stage& s){
                s.phase_c().rebuild_facility("FACTORY_SITE","building_materials",1,1);
            });
        w.advance(p);
        if(i==1)
            need(w.construction("P").ready &&
                 w.knowledge("LAB","KNOWLEDGE")==KnowledgeLevel::unknown,
                 "month2 review/ready timing");
        if(i==2)
            need(w.construction("P").commissioned &&
                 w.knowledge("LAB","KNOWLEDGE")==KnowledgeLevel::usable,
                 "month3 actual commissioning and research effectiveness");
        if(i==401)
            need(w.core().facility("FACTORY_SITE").damaged==2,
                 "real damage not applied at end of production month");
        if(i==501)
            need(w.core().facility("FACTORY_SITE").damaged==0||
                 w.core().facility("FACTORY_SITE").damaged==1,
                 "rebuild scheduling lost physical damage state");
        if(i==600||i==1000) {
            const auto saved=SnapshotCodec::encode(w);
            auto loaded=SnapshotCodec::decode(saved);
            need(SnapshotCodec::encode(loaded)==saved,
                 "long-run checkpoint lost domain/Phase C canonical state");
        }
    }
    need(w.commit_id()==1200 && w.core().stock("FACTORY","R","bar")==500 &&
         w.core().stock("FACTORY","R","ore")==100 &&
         w.construction("P").commissioned &&
         w.core().financial().balance("CLIENT:fixed_asset")==40 &&
         w.knowledge("LAB","KNOWLEDGE")==KnowledgeLevel::usable &&
         w.core().people().population()==6,
         "bounded Phase D long-run resource, POP or fixed-capital ledger drift");
    return w;
}
int main(){
    try{
        const auto a=run(),b=run();
        need(SnapshotCodec::encode(a)==SnapshotCodec::encode(b),
             "independent 1,200-month Phase D replay differs");
        std::cout<<"Phase D bounded low-density 1,200-month run x2 PASS "
                    "(~500 active industry months; not 100k/1m POP D-V L5).\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"Phase D bounded 1200-month FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
