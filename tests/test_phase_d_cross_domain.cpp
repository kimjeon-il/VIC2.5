#include "vic25/phase_d_snapshot.hpp"
#include <iostream>
#include <stdexcept>
using namespace vic25::phased;
using vic25::phaseb::Sex;using vic25::phaseb::Service;
static void need(bool ok,const char* s){if(!ok)throw std::runtime_error(s);}
template<class F>static bool rejected(F&& f){try{f();}catch(const std::exception&){return true;}return false;}
static Id worker(World& w,People n,Id part){
    auto& p=w.seed_core().seed_population();
    auto h=p.household_profile({"R","mixed","shared",n});
    auto e=p.economic_profile({"R","worker","wage","employee",p.default_education()});
    auto d=p.demographic_profile({"R",w.month()-360,Sex::female,
                                  "culture","faith","citizen","free",p.default_health(),Service::civilian});
    return p.seed_population(d,e,h,n,part);
}
static World seed(){
    World w{6100};auto& c=w.seed_core();
    c.seed_actor("CLIENT",100);c.seed_actor("BUILDER",0);
    c.seed_actor("GOV",100);c.seed_actor("LAB",0);
    c.seed_region("R","STATE");c.seed_stock("BUILDER","R","stone",1);
    c.seed_stock("LAB","R","reagent",1);
    c.seed_production_facility("LABSITE","LAB","R",1);
    auto builder=worker(w,1,10),researcher=worker(w,1,11);
    ConstructionProject p;
    p.id="PROJECT";p.client="CLIENT";p.contractor="BUILDER";
    p.region="R";p.target_site="NEW_SITE";p.material_good="stone";
    p.work_required=1;p.material_per_work=1;p.workers_per_work=1;
    p.target_capacity=2;p.cash_price_per_work=20;
    w.seed_construction(p);
    ResearchProject r;
    r.id="RESEARCH";r.holder="LAB";r.sponsor="GOV";
    r.lab_site="LABSITE";r.region="R";r.material_good="reagent";
    r.tech_id="KNOWLEDGE";r.required_evidence=1;r.material_per_work=1;
    r.workers_per_work=1;r.cash_cost_per_work=10;
    w.seed_research(r);
    World::Plan month;
    month[1].push_back([](Stage& s){
        s.command_once("PLAN_BUILD",[](Stage& live){live.plan_work("PROJECT",1);});
        s.command_once("PLAN_RESEARCH",[](Stage& live){live.plan_research("RESEARCH",1);});
    });
    month[2].push_back([builder,researcher](Stage& s){
        s.phase_c().hire(builder,1,"BUILDER",1);
        s.phase_c().hire(researcher,1,"LAB",1);
    });
    month[4].push_back([](Stage& s){
        s.command_once("EXEC_BUILD",[](Stage& live){live.perform_work("PROJECT",1);});
        s.command_once("EXEC_RESEARCH",[](Stage& live){live.perform_research("RESEARCH",1);});
    });
    month[5].push_back([](Stage& s){
        s.pay_work("PROJECT",1);s.pay_research("RESEARCH",1);
    });
    month[8].push_back([](Stage& s){s.capitalize_work("PROJECT",1);});
    month[9].push_back([](Stage& s){s.review_research("RESEARCH");});
    w.advance(month);
    return w;
}
int main(){
    try{
        World w=seed();const auto frozen=SnapshotCodec::encode(w);
        need(w.construction("PROJECT").ready && w.core().phase_d_wip("CLIENT")==20 &&
             w.knowledge("LAB","KNOWLEDGE")==KnowledgeLevel::unknown,
             "initial construction/research not reconciled");
        World resumed=SnapshotCodec::decode(frozen);
        w.advance();resumed.advance();
        need(SnapshotCodec::encode(w)==SnapshotCodec::encode(resumed) &&
             w.core().phase_d_wip("CLIENT")==0 &&
             w.core().facility("NEW_SITE").capacity==2 &&
             w.knowledge("LAB","KNOWLEDGE")==KnowledgeLevel::usable,
             "resume diverged from actual C/D causal monthly journal");
        const auto before=SnapshotCodec::encode(w);
        World::Plan duplicate;
        duplicate[9].push_back([](Stage& s){
            s.command_once("PLAN_BUILD",[](Stage& live){
                live.phase_c().pay("CLIENT","BUILDER",1);
            });
        });
        need(rejected([&]{w.advance(duplicate);}) &&
             SnapshotCodec::encode(w)==before,
             "Phase D external idempotency failed across save/commit");
        std::cout<<"Cross-domain verified construction+research save/replay and command ID PASS\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"Phase D cross-domain FAIL: "<<e.what()<<"\n";return 1;
    }
}
