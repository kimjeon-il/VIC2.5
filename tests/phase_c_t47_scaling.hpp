#pragma once
#include "vic25/phase_c.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(__linux__)
#include <sys/resource.h>
#endif

namespace vic25::phasec::acceptance {
struct ScaleResult {
    std::int64_t cells{}, months{};
    double median_ms{}, p95_ms{}, p99_ms{}, avg_cpu_ms{}, total_s{};
    std::uint64_t peak_rss_kib{};
    std::size_t events{};
};
inline double percentile(const std::vector<double>& sorted, double p){
    if(sorted.empty())throw std::runtime_error("no performance samples");
    const auto idx=static_cast<std::size_t>(std::ceil(p*sorted.size()));
    return sorted.at(std::min(sorted.size()-1,idx==0?0:idx-1));
}
inline ScaleResult integrated_scale_benchmark(std::int64_t count,std::int64_t months=8){
    if(count<4||count>1'000'000||months<3||months>48)
        throw std::invalid_argument("invalid scaling dimensions");
    const auto started=std::chrono::steady_clock::now();
    IntegratedWorld w{24000};
    for(int i=0;i<4;i++){
        const auto suffix=std::to_string(i);
        const std::string region="R"+suffix;
        w.seed_actor("F"+suffix,10000);
        w.seed_actor("H"+suffix,10000);
        w.seed_region(region,"A");
        w.seed_stock("F"+suffix,region,"tool",250*months);
        w.seed_stock("U"+suffix,region,"ammunition",20);
        w.seed_market(region,"tool",1,1,2,100);
        w.seed_unit("U"+suffix);
    }
    w.seed_actor("TREASURY",1000);
    w.seed_government("A","Head","Prime",3,2);
    auto& pop=w.seed_population();
    const auto h0=pop.household_profile({"R0","working-families","shared",count/4});
    const auto e0=pop.economic_profile({"R0","worker","wage","employee",pop.default_education()});
    const auto d0=pop.demographic_profile({"R0",23700,vic25::phaseb::Sex::female,
        "C","R","citizen","free",pop.default_health(),vic25::phaseb::Service::civilian});
    for(std::int64_t i=0;i<count-4;i++)
        (void)pop.seed_population(d0,e0,h0,1,static_cast<vic25::phaseb::Id>(i)+1);
    std::vector<vic25::phaseb::Id> soldiers;
    for(int i=0;i<4;i++){
        const auto region="R"+std::to_string(i);
        const auto h=pop.household_profile({region,"military","shared",1});
        const auto e=pop.economic_profile({region,"soldier","wage","employee",pop.default_education()});
        const auto d=pop.demographic_profile({region,23700,vic25::phaseb::Sex::male,
            "C","R","citizen","free",pop.default_health(),vic25::phaseb::Service::active});
        auto id=pop.seed_population(d,e,h,1,static_cast<vic25::phaseb::Id>(count+i+5));
        soldiers.push_back(id);
        w.seed_active("U"+std::to_string(i),id);
    }
    if(w.people().population()!=count)throw std::runtime_error("scale fixture population mismatch");
    std::vector<double> monthly_ms, cpu_ms;
    for(int m=0;m<months;m++){
        IntegratedWorld::Plan plan;
        plan[7].push_back([](auto& x){
            for(int i=0;i<4;i++){
                const std::string suffix=std::to_string(i);
                for(int j=0;j<250;j++)
                    x.trade("F"+suffix,"H"+suffix,"R"+suffix,"tool",1,1);
            }
        });
        if(m%4==0)plan[9].push_back([](auto& x){
            x.hold_election(1000,0,3,{{"A",600},{"B",400}});
        });
        if(m==0)plan[10].push_back([soldiers](auto& x){
            for(int i=0;i<4;i++)
                x.battle("U"+std::to_string(i),soldiers.at(static_cast<std::size_t>(i)),
                         "R"+std::to_string(i),CombatLoss{0,0,0,0,0,1});
        });
        const auto before=std::chrono::steady_clock::now();
        const auto cpu_before=std::clock();
        w.advance(plan);
        const auto cpu_after=std::clock();
        const auto after=std::chrono::steady_clock::now();
        monthly_ms.push_back(std::chrono::duration<double,std::milli>(after-before).count());
        cpu_ms.push_back(1000.0*static_cast<double>(cpu_after-cpu_before)/CLOCKS_PER_SEC);
        if(w.people().population()!=count || w.commit_id()!=static_cast<std::uint64_t>(m+1))
            throw std::runtime_error("large-scale population/commit inconsistency");
        w.validate();
    }
    for(int i=0;i<4;i++){
        const auto suffix=std::to_string(i), region="R"+suffix;
        if(w.cash("F"+suffix)!=10000+250*months||
           w.cash("H"+suffix)!=10000-250*months||
           w.stock("F"+suffix,region,"tool")!=0||
           w.stock("H"+suffix,region,"tool")!=250*months||
           w.stock("U"+suffix,region,"ammunition")!=19||
           w.active_strength("U"+suffix)!=1)
            throw std::runtime_error("large-scale cross-ledger property or unit failure");
    }
    std::sort(monthly_ms.begin(),monthly_ms.end());
    std::uint64_t peak=0;
#if defined(__linux__)
    struct rusage ru{};
    if(getrusage(RUSAGE_SELF,&ru)==0)
        peak=static_cast<std::uint64_t>(ru.ru_maxrss);
#endif
    const auto ended=std::chrono::steady_clock::now();
    ScaleResult result;
    result.cells=count;result.months=months;
    result.median_ms=percentile(monthly_ms,0.50);
    result.p95_ms=percentile(monthly_ms,0.95);
    result.p99_ms=percentile(monthly_ms,0.99);
    result.avg_cpu_ms=std::accumulate(cpu_ms.begin(),cpu_ms.end(),0.0)/cpu_ms.size();
    result.total_s=std::chrono::duration<double>(ended-started).count();
    result.peak_rss_kib=peak;
    result.events=w.journal().size();
    std::cout<<"D4-T47 representative synthetic workload cells="<<result.cells
             <<" months="<<result.months<<" p50_ms="<<result.median_ms
             <<" p95_ms="<<result.p95_ms<<" p99_ms="<<result.p99_ms
             <<" mean_cpu_ms="<<result.avg_cpu_ms
             <<" rss_peak_kib="<<result.peak_rss_kib
             <<" events="<<result.events
             <<" total_s="<<result.total_s<<"\n";
    return result;
}
} // namespace vic25::phasec::acceptance
