#include "vic25/phase_e.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif
using namespace vic25::phasee;
static double peak_rss_mib(){
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS m{};
    if(GetProcessMemoryInfo(GetCurrentProcess(),&m,sizeof(m)))
        return double(m.PeakWorkingSetSize)/(1024*1024);
    return -1;
#else
    rusage r{};
    if(getrusage(RUSAGE_SELF,&r)!=0)return -1;
#if defined(__APPLE__)
    return double(r.ru_maxrss)/(1024*1024);
#else
    return double(r.ru_maxrss)/1024;
#endif
#endif
}
static double percentile(std::vector<double> sorted,double p){
    std::sort(sorted.begin(),sorted.end());
    if(sorted.empty())return 0;
    const double index=(sorted.size()-1)*p;
    const auto lower=static_cast<std::size_t>(index);
    const auto upper=std::min(lower+1,sorted.size()-1);
    return sorted[lower]+(sorted[upper]-sorted[lower])*(index-lower);
}
int main(int argc,char** argv) {
    try {
        if(argc!=4)throw std::invalid_argument("usage: phase_e_scaling provinces pop_cells months");
        const auto n=std::stoull(argv[1]);
        const auto k=std::stoull(argv[2]);
        const auto months=std::stoull(argv[3]);
        if(!n||!k||!months||n>100000||k>1000000||months>16)
            throw std::invalid_argument("unsupported bounded benchmark");
        World w(30000);
        w.set_dataset("synthetic-EV59-1");
        w.seed_unit(101,"A","A1");
        for(std::uint64_t i=1;i<=n;++i){
            w.seed_province(i,"R"+std::to_string(i),"A",1000);
            w.seed_assignment(i,101);
        }
        auto& p=w.seed_core().seed_core().seed_population();
        constexpr int regions=8;
        struct Source{vic25::phaseb::Id d{},e{},h{};};
        std::array<Source,regions> profiles{};
        for(int r=0;r<regions;++r){
            const std::string region="R"+std::to_string(r+1);
            profiles[r].h=p.household_profile({region,"mixed","shared",static_cast<People>(k)});
            profiles[r].e=p.economic_profile({region,"worker","wage","employee",p.default_education()});
            profiles[r].d=p.demographic_profile({region,w.month()-400,vic25::phaseb::Sex::female,
                "culture","faith","citizen","free",
                p.default_health(),vic25::phaseb::Service::civilian});
        }
        for(std::uint64_t i=0;i<k;++i){
            const auto& s=profiles[i%regions];
            p.seed_population(s.d,s.e,s.h,1,static_cast<vic25::phaseb::Id>(i+1));
        }
        w.validate();
        auto now=[]{return std::chrono::steady_clock::now();};
        std::vector<double> samples;
        std::array<double,12> per_stage_sum{};
        for(std::uint64_t i=0;i<months;++i){
            const auto start=now();
            std::array<std::chrono::steady_clock::time_point,12> boundaries{};
            World::Plan plan;
            for(int stage=0;stage<12;++stage){
                plan[stage].push_back([&,stage](SpatialStage&){
                    boundaries[stage]=now();
                });
            }
            w.advance(plan);
            const auto finish=now();
            const double ms=std::chrono::duration<double,std::milli>(finish-start).count();
            samples.push_back(ms);
            for(int stage=0;stage<11;++stage)
                per_stage_sum[stage]+=
                    std::chrono::duration<double,std::milli>(boundaries[stage+1]-boundaries[stage]).count();
            per_stage_sum[11]+=
                std::chrono::duration<double,std::milli>(finish-boundaries[11]).count();
            if(w.population(w.sovereign_provinces("A"))!=static_cast<People>(k))
                throw std::runtime_error("EV59 source POP headcount changed");
            if(w.spatial().provinces.size()!=n)
                throw std::runtime_error("EV59 province count changed");
        }
        std::cout<<std::fixed<<std::setprecision(3);
        std::cout<<"EV59_METRICS provinces="<<n<<" pop_cells="<<k
                 <<" months="<<months<<" p50_ms="<<percentile(samples,.50)
                 <<" p95_ms="<<percentile(samples,.95)
                 <<" p99_ms="<<percentile(samples,.99)
                 <<" peak_mib="<<peak_rss_mib()<<"\n";
        for(int i=0;i<12;++i)
            std::cout<<"EV59_STAGE stage="<<i
                     <<" approx_ms="<<per_stage_sum[i]/months<<"\n";
        std::cout<<"EV59_SYNTHETIC_CONSERVATION PASS\n";
    }catch(const std::exception& e){
        std::cerr<<"EV59 FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
