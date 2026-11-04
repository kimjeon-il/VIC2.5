#include "vic25/phase_c.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

using namespace vic25::phasec;
using vic25::phaseb::Service;
using vic25::phaseb::Sex;

int main(int argc,char** argv) {
    const std::int64_t count = argc>1 ? std::atoll(argv[1]):100'000;
    if(count<1 || count>1'000'000) return 2;
    const auto start=std::chrono::steady_clock::now();
    IntegratedWorld w{24000};
    w.seed_actor("F",2000);w.seed_actor("H",2000);w.seed_actor("G",2000);
    w.seed_region("R1","A");w.seed_unit("U");
    w.seed_government("A","Head","Prime",3,2);
    w.seed_stock("F","R1","tool",1000);
    w.seed_market("R1","tool",1,1,10,100);
    auto& pop=w.seed_population();
    auto hh=pop.household_profile({"R1","large-region","shared",20000});
    auto e=pop.economic_profile({"R1","worker","wage","employee",pop.default_education()});
    auto d=pop.demographic_profile({"R1",23700,Sex::female,"C","R","citizen","free",
                                     pop.default_health(),Service::civilian});
    for (std::int64_t i=0;i<count;i++) pop.seed_population(d,e,hh,1,static_cast<Id>(i)+1);
    if(w.people().population()!=count)throw std::runtime_error("incorrect initial population");
    const auto seeded=std::chrono::steady_clock::now();
    IntegratedWorld::Plan plan;
    plan[7].push_back([](auto& x) {
        for(int i=0;i<1000;i++)x.trade("F","H","R1","tool",1,1);
    });
    plan[9].push_back([](auto& x){
        x.hold_election(1000,0,3,{{"A",600},{"B",400}});
    });
    auto result=w.advance(plan);
    const auto finished=std::chrono::steady_clock::now();
    w.validate();
    if(result.population!=count || w.cash("F")!=3000 || w.cash("H")!=1000 ||
       w.stock("F","R1","tool")!=0 || w.stock("H","R1","tool")!=1000 ||
       w.election().total_seats!=3)
        throw std::runtime_error("integrated million-slice accounting validation failed");
    std::cout<<"Phase C scaling verified (single representative month)\n"
             <<"active_slices="<<w.people().slices().size()
             <<" persons="<<w.people().population()
             <<" actual_trades=1000"
             <<" seed_seconds="<<std::chrono::duration<double>(seeded-start).count()
             <<" stage_month_seconds="<<std::chrono::duration<double>(finished-seeded).count()
             <<" total_seconds="<<std::chrono::duration<double>(finished-start).count()
             <<" commit_id="<<w.commit_id()<<"\n";
}
