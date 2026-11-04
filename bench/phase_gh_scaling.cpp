#include "vic25/phase_gh.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(__linux__)
#include <sys/resource.h>
#endif
using namespace vic25::phasegh;
static int parse(const char* text,int min,int max) {
    const int v=std::stoi(text);
    if(v<min||v>max)throw std::invalid_argument("stress argument out of range");
    return v;
}
int main(int argc,char** argv) {
    try {
        const int provinces=argc>=2?parse(argv[1],2,30000):10000;
        const int routes=argc>=3?parse(argv[2],1,1000):100;
        const int months=argc>=4?parse(argv[3],1,12):3;
        if(provinces<routes*2)throw std::invalid_argument("needs two provinces per route");
        const int countries=std::min(200,provinces);
        World w(1801*12);
        auto& e=w.seed_geography();
        e.set_dataset("synthetic-30k-province-stress-not-historical-GIS");
        for(int country=0;country<countries;++country) {
            const auto name="C"+std::to_string(country);
            e.seed_unit(static_cast<Id>(country+100001),name,name);
        }
        for(int p=1;p<=provinces;++p) {
            const std::string region="R"+std::to_string(p);
            const int group=(p-1)%countries;
            const auto country="C"+std::to_string(group);
            e.seed_province(static_cast<Id>(p),region,country);
            e.seed_assignment(p,static_cast<Id>(group+100001));
            if(p>1)e.seed_border(p-1,p);
        }
        auto& c=e.seed_core().seed_core();
        for(int country=0;country<countries;++country) {
            const auto name="C"+std::to_string(country);
            c.seed_actor(name,100000);
            c.seed_government(name,"HEAD_"+name,"PM_"+name,5,3);
        }
        c.seed_actor("CARRIER",1000);
        for(int i=0;i<routes;++i) {
            const Id from=static_cast<Id>(2*i+1);
            const Id to=static_cast<Id>(2*i+2);
            const std::string seller="SELLER"+std::to_string(i);
            const std::string buyer="BUYER"+std::to_string(i);
            const std::string start="R"+std::to_string(from);
            const std::string end="R"+std::to_string(to);
            const auto importer="C"+std::to_string((static_cast<int>(to)-1)%countries);
            c.seed_actor(seller,0);
            c.seed_actor(buyer,10000);
            c.seed_stock(seller,start,"iron",months+3);
            c.seed_market(start,"iron",100,10,200);
            c.seed_market(end,"iron",170,10,250);
            const std::string link="LINK"+std::to_string(i);
            w.seed_transport_edge(TransportEdge{link,"CARRIER",
                                                 from,to,1,5,1,true});
            w.seed_route(Route{"ROUTE"+std::to_string(i),seller,buyer,
                                     "iron",from,to,1,1});
            w.seed_tariff(importer,"iron",100000);
        }
        w.validate();
        std::vector<double> durations;
        durations.reserve(months);
        for(int i=0;i<months;++i) {
            const auto before=std::chrono::steady_clock::now();
            w.advance();
            const auto after=std::chrono::steady_clock::now();
            durations.push_back(std::chrono::duration<double,std::milli>(
                                      after-before).count());
        }
        w.validate();
        std::sort(durations.begin(),durations.end());
        auto perc=[&](double p){return durations[static_cast<std::size_t>(
                 p*(durations.size()-1))];};
        std::uint64_t rss_kb=0;
#if defined(__linux__)
        rusage usage{};
        if(getrusage(RUSAGE_SELF,&usage)==0)
            rss_kb=static_cast<std::uint64_t>(usage.ru_maxrss);
#endif
        if(w.commerce().fills.size()!=static_cast<std::size_t>(routes*months))
            throw std::runtime_error("missing real executed routes");
        std::cout<<"GH_SCALE status=PASS synthetic=true provinces="<<provinces
                 <<" countries="<<countries<<" routes="<<routes
                 <<" months="<<months
                 <<" fills="<<w.commerce().fills.size()
                 <<" p50_ms="<<perc(.5)
                 <<" p95_ms="<<perc(.95)
                 <<" p99_ms="<<perc(.99)
                 <<" peak_rss_kb="<<rss_kb
                 <<" commit_id="<<w.commit_id()<<"\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"GH_SCALE status=FAIL reason="<<e.what()<<"\n";
        return 1;
    }
}
