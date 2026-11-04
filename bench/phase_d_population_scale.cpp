#include "vic25/phase_d.hpp"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef __linux__
#include <fstream>
#endif
using namespace vic25::phased;
using vic25::phaseb::Sex;using vic25::phaseb::Service;
static void need(bool cond,const char* msg){if(!cond)throw std::runtime_error(msg);}
static World build(std::int64_t cells,std::vector<Id>& employed){
    World w{7000};auto& c=w.seed_core();
    c.seed_actor("FACTORY",1000);c.seed_actor("BUILDER",500);
    c.seed_actor("LAB",0);c.seed_actor("GOV",200);
    c.seed_region("R","COUNTRY");
    c.seed_production_facility("FACTORY_SITE","FACTORY","R",100);
    c.seed_production_facility("LAB_SITE","LAB","R",20);
    c.seed_stock("FACTORY","R","ore",100);
    c.seed_stock("BUILDER","R","stone",20);
    c.seed_stock("LAB","R","reagent",20);
    auto& pop=c.seed_population();
    const auto h=pop.household_profile({"R","shared","shared",cells});
    const auto e=pop.economic_profile({"R","worker","wage","employee",pop.default_education()});
    const auto d=pop.demographic_profile({"R",w.month()-360,Sex::female,
         "culture","faith","citizen","free",pop.default_health(),Service::civilian});
    employed.reserve(60);
    for(std::int64_t i=0;i<cells;++i){
        const Id id=pop.seed_population(d,e,h,1,static_cast<Id>(i+1));
        if(i<60)employed.push_back(id);
    }
    IndustryRecipe recipe;
    recipe.id="ORE_RECIPE";recipe.site="FACTORY_SITE";
    recipe.inputs={{"ore",1}};recipe.outputs={{"bar",1}};
    recipe.workers_per_batch=1;recipe.capacity_per_batch=1;
    w.seed_recipe(recipe);
    ConstructionProject p;
    p.id="BRIDGE";p.client="BUILDER";p.contractor="BUILDER";
    // The pilot does not allow self-paid construction due to C-E ledger
    // counterparty restriction. Use a separate funded client for real payment.
    c.seed_actor("CLIENT",300);
    p.client="CLIENT";p.region="R";p.target_site="BRIDGE_SITE";
    p.material_good="stone";p.work_required=10;p.material_per_work=1;
    p.workers_per_work=1;p.cash_price_per_work=5;p.target_capacity=2;
    w.seed_construction(p);
    ResearchProject r;
    r.id="RESEARCH";r.holder="LAB";r.sponsor="GOV";
    r.lab_site="LAB_SITE";r.region="R";r.material_good="reagent";
    r.tech_id="SCIENCE";r.required_evidence=10;r.material_per_work=1;
    r.workers_per_work=1;r.cash_cost_per_work=5;
    w.seed_research(r);
    return w;
}
#ifdef __linux__
static std::string peak_rss(){
    std::ifstream f("/proc/self/status");std::string line;
    while(std::getline(f,line))
        if(line.rfind("VmHWM:",0)==0)return line;
    return "VmHWM: unavailable";
}
#else
static std::string peak_rss(){return "VmHWM: platform measurement unavailable";}
#endif
int main(int argc,char** argv){
    try{
        const std::int64_t cells=argc>1?std::stoll(argv[1]):10000;
        const int months=argc>2?std::stoi(argv[2]):2;
        need(cells>=60&&cells<=1000000&&months>=2&&months<=12,
             "invalid benchmark sparse POP cell count/months");
        const auto initial=std::chrono::steady_clock::now();
        std::vector<Id> staff;World w=build(cells,staff);
        const auto seeded=std::chrono::steady_clock::now();
        std::vector<double> times;
        for(int month=0;month<months;++month){
            World::Plan p;
            if(month<2){
                p[1].push_back([](Stage& s){
                    s.plan_work("BRIDGE",5);s.plan_research("RESEARCH",5);
                });
            }
            if(month==0){
                p[2].push_back([staff](Stage& s){
                    for(int i=0;i<20;++i)s.phase_c().hire(staff[i],1,"FACTORY",1);
                    for(int i=20;i<40;++i)s.phase_c().hire(staff[i],1,"BUILDER",1);
                    for(int i=40;i<60;++i)s.phase_c().hire(staff[i],1,"LAB",1);
                });
            }
            p[4].push_back([month](Stage& s){
                if(month<2){
                    s.perform_work("BRIDGE",5);s.perform_research("RESEARCH",5);
                }
                s.manufacture("ORE_RECIPE",5);
            });
            if(month<2){
                p[5].push_back([](Stage& s){
                    s.pay_work("BRIDGE",5);s.pay_research("RESEARCH",5);
                });
                p[8].push_back([](Stage& s){s.capitalize_work("BRIDGE",5);});
                if(month==1)
                    p[9].push_back([](Stage& s){s.review_research("RESEARCH");});
            }
            const auto began=std::chrono::steady_clock::now();
            w.advance(p);w.validate();
            const auto finished=std::chrono::steady_clock::now();
            times.push_back(std::chrono::duration<double,std::milli>(
                finished-began).count());
        }
        need(w.core().people().population()==cells&&
             w.core().stock("FACTORY","R","bar")==5*months&&
             w.construction("BRIDGE").commissioned &&
             w.core().facility("BRIDGE_SITE").capacity==2 &&
             w.research("RESEARCH").verified_work==10,
             "large POP benchmark world changed economic or population invariants");
        std::sort(times.begin(),times.end());
        const auto pct=[&](double p){
            const auto rank=static_cast<std::size_t>(std::ceil(p*times.size()));
            return times.at(std::max<std::size_t>(1,rank)-1);
        };
        std::cout<<"Phase D real sparse POP count="<<cells
                 <<" months="<<months
                 <<" seed_ms="<<std::chrono::duration<double,std::milli>(seeded-initial).count()
                 <<" avg_month_ms="<<std::accumulate(times.begin(),times.end(),0.0)/times.size()
                 <<" p50_month_ms="<<pct(0.50)
                 <<" p95_month_ms="<<pct(0.95)
                 <<" p99_month_ms="<<pct(0.99)
                 <<" max_month_ms="<<times.back()
                 <<" "<<peak_rss()<<"\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"Phase D scaling FAIL: "<<e.what()<<"\n";return 1;
    }
}
