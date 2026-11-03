#include "vic25/phase_b.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
using namespace vic25::phaseb;
int main(int argc,char** argv){
    const std::int64_t count=argc>1?std::atoll(argv[1]):100'000;
    if(count<1||count>1'000'000)return 1;
    World w{24000};
    auto h=w.household_profile({"R1","pooled","shared",1000});
    auto e=w.economic_profile({"R1","farmer","subsistence","self_employed",w.default_education()});
    auto d=w.demographic_profile({"R1",23700,Sex::female,"C","R","N","citizen",w.default_health(),Service::civilian});
    const auto start=std::chrono::steady_clock::now();
    for(std::int64_t i=0;i<count;i++)w.seed_population(d,e,h,1,static_cast<Id>(i)+1);
    const auto end=std::chrono::steady_clock::now();
    if(w.population()!=count||!w.validate())return 2;
    std::cout<<"cells="<<w.slices().size()<<" people="<<w.population()
             <<" build_s="<<std::chrono::duration<double>(end-start).count()<<'\n';
}
