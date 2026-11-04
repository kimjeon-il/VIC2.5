#include "vic25/phase_d.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

using namespace vic25::phased;
using vic25::phaseb::Sex;
using vic25::phaseb::Service;

static void check(bool yes,const char* message) {
    if(!yes)throw std::runtime_error(message);
}
template<class Fn> static bool reject(Fn&& fn) {
    try {fn();}catch(const std::exception&){return true;}
    return false;
}
static Id people(World& w,const std::string& region,People n) {
    auto& p=w.seed_core().seed_population();
    const auto h=p.household_profile({region,"mixed","shared",n});
    const auto e=p.economic_profile({region,"worker","wage","employee",p.default_education()});
    const auto d=p.demographic_profile({region,700,Sex::female,"culture","faith",
                                        "citizen","free",p.default_health(),Service::civilian});
    return p.seed_population(d,e,h,n);
}
static void seed_actor_set(World& w) {
    auto& c=w.seed_core();
    c.seed_actor("CLIENT",200);
    c.seed_actor("BUILDER",0);
    c.seed_actor("GOV",100);
    c.seed_actor("LAB",0);
    c.seed_actor("FACTORY",20);
    c.seed_actor("MARKET",0);
    c.seed_region("R","A");
}
static ConstructionProject project(Quantity work=2) {
    ConstructionProject p;
    p.id="P"; p.client="CLIENT";p.contractor="BUILDER";p.region="R";
    p.material_good="steel";p.target_site="NEW_MILL";
    p.work_required=work;p.material_per_work=1;p.target_capacity=3;
    p.workers_per_work=1;p.cash_price_per_work=20;
    return p;
}
static IndustryRecipe recipe(const std::string& site="OLD_MILL",
                             const std::string& tech="") {
    IndustryRecipe r;
    r.id="RECIPE";r.site=site;r.tech_id=tech;
    r.inputs={{"ore",1}};r.outputs={{"bars",2}};
    r.workers_per_batch=1;r.capacity_per_batch=1;
    return r;
}
static ResearchProject research(const std::string& tech="T") {
    ResearchProject r;
    r.id="R";r.holder="LAB";r.sponsor="GOV";r.lab_site="LAB_SITE";
    r.region="R";r.material_good="reagent";r.tech_id=tech;
    r.material_per_work=1;r.required_evidence=2;r.workers_per_work=1;
    r.cash_cost_per_work=10;
    return r;
}
static void full_work_plan(World::Plan& p,Quantity units) {
    p[1].push_back([units](Stage& s){s.plan_work("P",units);});
    p[4].push_back([units](Stage& s){s.perform_work("P",units);});
    p[5].push_back([units](Stage& s){s.pay_work("P",units);});
    p[8].push_back([units](Stage& s){s.capitalize_work("P",units);});
}
static void test_construction_accepted_wip_next_month() {
    World w{1000};seed_actor_set(w);
    const auto b=people(w,"R",2);
    w.seed_core().seed_stock("BUILDER","R","steel",2);
    w.seed_construction(project());
    World::Plan first;
    first[2].push_back([b](Stage& s){s.phase_c().hire(b,2,"BUILDER",2);});
    full_work_plan(first,1);
    w.advance(first);
    check(w.month()==1001 && w.construction("P").accepted_work==1 &&
          !w.construction("P").ready &&
          w.core().cash("CLIENT")==180,"first month cash or work");
    check(w.core().phase_d_wip("CLIENT")==20,"first month WIP");
    World::Plan second;full_work_plan(second,1);w.advance(second);
    check(w.construction("P").ready && !w.construction("P").commissioned,
          "complete site operated in its construction month");
    check(w.core().phase_d_wip("CLIENT")==40 && w.core().cash("CLIENT")==160,
          "accepted work financial capitalization");
    w.advance();
    check(w.month()==1003 && w.construction("P").commissioned &&
          w.core().facility("NEW_MILL").capacity==3 &&
          w.core().phase_d_wip("CLIENT")==0 &&
          w.core().financial().balance("CLIENT:fixed_asset")==40,
          "stage0 next-month commissioning / fixed asset");
    check(w.core().stock("BUILDER","R","steel")==0,"construction double-used material");
    w.validate();
}
static void research_month(World::Plan& p,Quantity q,bool review) {
    p[1].push_back([q](Stage& s){s.plan_research("R",q);});
    p[4].push_back([q](Stage& s){s.perform_research("R",q);});
    p[5].push_back([q](Stage& s){s.pay_research("R",q);});
    if(review)p[9].push_back([](Stage& s){s.review_research("R");});
}
static void test_funded_research_and_knowledge_lag() {
    World w{2000};seed_actor_set(w);
    const auto researcher=people(w,"R",2);
    w.seed_core().seed_production_facility("LAB_SITE","LAB","R",2);
    w.seed_core().seed_stock("LAB","R","reagent",2);
    w.seed_research(research());
    World::Plan first;
    first[2].push_back([researcher](Stage& s) {
        s.phase_c().hire(researcher,2,"LAB",1);
    });
    research_month(first,1,false);
    w.advance(first);
    check(w.research("R").verified_work==1 &&
          w.knowledge("LAB","T")==KnowledgeLevel::unknown &&
          w.core().cash("GOV")==90,"first research month");
    World::Plan second;research_month(second,1,true);w.advance(second);
    check(w.knowledge("LAB","T")==KnowledgeLevel::unknown &&
          w.research("R").reviewed && w.core().cash("LAB")==20,
          "research review must not be current-month knowledge");
    w.advance();
    check(w.knowledge("LAB","T")==KnowledgeLevel::usable &&
          w.core().stock("LAB","R","reagent")==0 &&
          w.state().research_evidence.size()==2,
          "research evidence and effective knowledge");
    w.validate();
}
static void test_industry_real_inputs_and_capacity() {
    World w{3000};seed_actor_set(w);
    auto id=people(w,"R",2);
    w.seed_core().seed_production_facility("OLD_MILL","FACTORY","R",2);
    w.seed_core().seed_stock("FACTORY","R","ore",3);
    w.seed_recipe(recipe());
    World::Plan plan;
    plan[2].push_back([id](Stage& s){s.phase_c().hire(id,2,"FACTORY",1);});
    plan[4].push_back([](Stage& s){s.manufacture("RECIPE",2);});
    w.advance(plan);
    check(w.core().stock("FACTORY","R","ore")==1 &&
          w.core().stock("FACTORY","R","bars")==4,
          "phase D multi-output industry production");
    World::Plan bad;
    bad[4].push_back([](Stage& s){s.manufacture("RECIPE",3);});
    check(reject([&]{w.advance(bad);}) && w.month()==3001 &&
          w.core().stock("FACTORY","R","ore")==1,"capacity overrun rolled back");
}
static void test_adoption_not_national_bonus() {
    World w{4000};seed_actor_set(w);
    const auto id=people(w,"R",2);
    w.seed_core().seed_production_facility("OLD_MILL","FACTORY","R",2);
    w.seed_core().seed_stock("FACTORY","R","ore",3);
    w.seed_recipe(recipe("OLD_MILL","IRON_TECH"));
    World::Plan bad;
    bad[2].push_back([id](Stage& s){s.phase_c().hire(id,2,"FACTORY",1);});
    bad[4].push_back([](Stage& s){s.manufacture("RECIPE",1);});
    check(reject([&]{w.advance(bad);}) && w.month()==4000,
          "unknown technique erroneously used");
    w.seed_knowledge("FACTORY","IRON_TECH",KnowledgeLevel::usable);
    World::Plan first;
    first[2].push_back([id](Stage& s){s.phase_c().hire(id,2,"FACTORY",1);});
    first[9].push_back([](Stage& s){s.propose_adoption("OLD_MILL","IRON_TECH");});
    w.advance(first);
    check(w.state().active_site_tech.empty(),"adoption became active early");
    World::Plan second;
    second[4].push_back([](Stage& s){s.manufacture("RECIPE",1);});
    w.advance(second);
    check(w.core().stock("FACTORY","R","bars")==2 &&
          w.state().active_site_tech.at("OLD_MILL")=="IRON_TECH",
          "adoption failed after next-month stage0");
}
static void test_cross_domain_ore_to_construction() {
    World w{5000};seed_actor_set(w);
    auto maker=people(w,"R",1);
    auto builder=people(w,"R",1);
    w.seed_core().seed_production_facility("OLD_MILL","FACTORY","R",2);
    w.seed_core().seed_stock("FACTORY","R","ore",1);
    auto r=recipe(); r.outputs={{"steel",1}};
    w.seed_recipe(r);
    w.seed_construction(project(1));
    World::Plan p;
    p[2].push_back([maker,builder](Stage& s) {
        s.phase_c().hire(maker,1,"FACTORY",2);
        s.phase_c().hire(builder,1,"BUILDER",2);
    });
    p[4].push_back([](Stage& s){s.manufacture("RECIPE",1);});
    p[7].push_back([](Stage& s) {
        s.phase_c().trade("FACTORY","BUILDER","R","steel",1,5);
    });
    w.advance(p);
    check(w.core().stock("BUILDER","R","steel")==1 &&
          w.construction("P").accepted_work==0,
          "stage7 goods must not feed stage4 construction");
    World::Plan next;full_work_plan(next,1);w.advance(next);
    check(w.construction("P").ready &&
          w.core().phase_d_wip("CLIENT")==20 &&
          w.core().stock("BUILDER","R","steel")==0,
          "next-month construction with real delivered input");
    w.advance();
    check(w.core().facility("NEW_MILL").owner=="CLIENT",
          "new facility ownership/commission bridge failed");
}
static void test_research_vs_industry_worker_conflict_rollback() {
    World w{6000};seed_actor_set(w);
    const auto id=people(w,"R",2);
    w.seed_core().seed_production_facility("OLD_MILL","LAB","R",3);
    w.seed_core().seed_production_facility("LAB_SITE","LAB","R",3);
    w.seed_core().seed_stock("LAB","R","ore",2);
    w.seed_core().seed_stock("LAB","R","reagent",1);
    auto r=recipe("OLD_MILL");r.id="LAB_RECIPE";
    w.seed_recipe(r);w.seed_research(research());
    World::Plan p;
    p[1].push_back([](Stage& s){s.plan_research("R",1);});
    p[2].push_back([id](Stage& s){s.phase_c().hire(id,2,"LAB",1);});
    p[4].push_back([](Stage& s){
        s.manufacture("LAB_RECIPE",2);
        s.perform_research("R",1); // requires third person
    });
    check(reject([&]{w.advance(p);}) && w.month()==6000 &&
          w.core().stock("LAB","R","ore")==2 &&
          w.core().stock("LAB","R","reagent")==1 &&
          w.research("R").verified_work==0 && w.core().commit_id()==0,
          "cross-domain worker double-booking did not rollback");
}
static void test_funding_and_research_material_fail_closed() {
    World w{7000};seed_actor_set(w);
    const auto staff=people(w,"R",2);
    w.seed_core().seed_production_facility("LAB_SITE","LAB","R",2);
    w.seed_research(research());
    World::Plan p;
    p[1].push_back([](Stage& s){s.plan_research("R",2);});
    p[2].push_back([staff](Stage& s){s.phase_c().hire(staff,2,"LAB",1);});
    p[4].push_back([](Stage& s){s.perform_research("R",2);});
    check(reject([&]{w.advance(p);}) &&
          w.core().cash("GOV")==100 && w.month()==7000 &&
          w.state().research_evidence.empty(),
          "unfunded experiment material should rollback");
}
static void test_late_failure_rolls_back_all_domains() {
    World w{8000};seed_actor_set(w);
    const auto b=people(w,"R",1);
    w.seed_core().seed_stock("BUILDER","R","steel",1);
    w.seed_construction(project(1));
    World::Plan p;full_work_plan(p,1);
    p[2].push_back([b](Stage& s){s.phase_c().hire(b,1,"BUILDER",1);});
    p[11].push_back([](Stage&){throw std::runtime_error("deliberate ownership fault");});
    check(reject([&]{w.advance(p);}) &&
          w.month()==8000 && w.commit_id()==0 &&
          w.core().cash("CLIENT")==200 &&
          w.core().stock("BUILDER","R","steel")==1 &&
          w.core().phase_d_wip("CLIENT")==0 &&
          w.construction("P").accepted_work==0 &&
          w.state().journal.empty(),"late fault did not rollback all ledgers");
    World::Plan clean;full_work_plan(clean,1);
    clean[2].push_back([b](Stage& s){s.phase_c().hire(b,1,"BUILDER",1);});
    w.advance(clean);
    check(w.commit_id()==1 && w.construction("P").ready,
          "replay after failed month did not recover");
}
static void test_unsupported_retrofit_is_rejected() {
    World w{9000};seed_actor_set(w);
    const auto id=people(w,"R",1);
    w.seed_core().seed_production_facility("OLD_MILL","FACTORY","R",1);
    w.seed_core().seed_stock("FACTORY","R","ore",1);
    auto r=recipe("OLD_MILL","ADV");r.needs_retrofit=true;
    w.seed_recipe(r);
    w.seed_knowledge("FACTORY","ADV",KnowledgeLevel::usable);
    World::Plan p;
    p[2].push_back([id](Stage& s){s.phase_c().hire(id,1,"FACTORY",1);});
    p[9].push_back([](Stage& s){s.propose_adoption("OLD_MILL","ADV");});
    check(reject([&]{w.advance(p);})&&w.month()==9000 &&
          w.state().pending_adoptions.empty(),
          "unimplemented retrofit must fail closed");
}
static void test_unsupported_nonstorable_dispatch_rejected() {
    World w{10000};seed_actor_set(w);
    const auto id=people(w,"R",1);
    w.seed_core().seed_production_facility("OLD_MILL","FACTORY","R",1);
    w.seed_core().seed_stock("FACTORY","R","ore",1);
    auto r=recipe(); r.requires_same_stage_nonstorable_energy=true;
    w.seed_recipe(r);
    World::Plan p;
    p[2].push_back([id](Stage& s){s.phase_c().hire(id,1,"FACTORY",1);});
    p[4].push_back([](Stage& s){s.manufacture("RECIPE",1);});
    check(reject([&]{w.advance(p);}) && w.month()==10000 &&
          w.core().stock("FACTORY","R","ore")==1 &&
          w.core().stock("FACTORY","R","bars")==0,
          "unapproved I2-DISPATCH must fail without consuming material");
}
static void test_repeated_world_deterministic() {
    auto run=[] {
        World w{11000};seed_actor_set(w);
        const auto id=people(w,"R",2);
        w.seed_core().seed_production_facility("OLD_MILL","FACTORY","R",2);
        w.seed_core().seed_stock("FACTORY","R","ore",8);
        w.seed_recipe(recipe());
        for(int n=0;n<4;++n) {
            World::Plan p;
            if(n==0)p[2].push_back([id](Stage& s){s.phase_c().hire(id,2,"FACTORY",1);});
            p[4].push_back([](Stage& s){s.manufacture("RECIPE",2);});
            w.advance(p);
        }
        check(w.commit_id()==4 && w.core().stock("FACTORY","R","bars")==16,
              "four-month industrial batch output");
        return std::make_pair(w.core().journal(),w.state().journal);
    };
    check(run()==run(),"Phase D independent replay differs");
}
int main() {
    try {
        test_construction_accepted_wip_next_month();
        test_funded_research_and_knowledge_lag();
        test_industry_real_inputs_and_capacity();
        test_adoption_not_national_bonus();
        test_cross_domain_ore_to_construction();
        test_research_vs_industry_worker_conflict_rollback();
        test_funding_and_research_material_fail_closed();
        test_late_failure_rolls_back_all_domains();
        test_unsupported_retrofit_is_rejected();
        test_unsupported_nonstorable_dispatch_rejected();
        test_repeated_world_deterministic();
        std::cout<<"Phase D bounded integration: 11 scenario cases PASS. "
                    "This is not the D-V 72/279 acceptance suite.\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"Phase D bounded integration FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
