#include "vic25/daily_time.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <sys/resource.h>

using namespace vic25::daily;
using Clock=std::chrono::steady_clock;
static double millis(Clock::time_point a,Clock::time_point b){
    return std::chrono::duration<double,std::milli>(b-a).count();
}
static double percentile(std::vector<double> v,double ratio){
    if(v.empty())return 0;
    std::sort(v.begin(),v.end());
    const auto i=static_cast<std::size_t>((v.size()-1)*ratio);
    return v.at(i);
}
static double rss_mib(){
    rusage r{};
    if(getrusage(RUSAGE_SELF,&r)!=0)return -1;
    return static_cast<double>(r.ru_maxrss)/1024.0;
}
int main(){
  try {
    constexpr std::uint64_t provinces=30000,pops=100000;
    vic25::phasegh::World gh{24000};
    auto& e=gh.seed_geography();
    e.set_dataset("synthetic-daily-30k/1");
    e.seed_unit(1,"A","A-root");
    for(std::uint64_t n=1;n<=provinces;++n) {
        e.seed_province(n,"P"+std::to_string(n),"A",1000000);
        e.seed_assignment(n,1);
    }
    auto& c=e.seed_core().seed_core();
    c.seed_actor("PAYER",100);
    c.seed_actor("RECEIVER",0);
    auto& p=c.seed_population();
    const auto house=p.household_profile({"P1","shared","shared",static_cast<People>(pops)});
    const auto econ=p.economic_profile({"P1","worker","wage","employee",p.default_education()});
    const auto demo=p.demographic_profile({"P1",23640,vic25::phaseb::Sex::female,
        "culture","faith","citizen","free",p.default_health(),
        vic25::phaseb::Service::civilian});
    for(std::uint64_t i=0;i<pops;++i)
        p.seed_population(demo,econ,house,1,
                          static_cast<vic25::phaseb::Id>(i+1));
    gh.validate();
    auto world=World::adopt(std::move(gh));
    std::vector<double> idle;
    const auto prior=world.engine().base().core().core().people().population();
    for(int d=1;d<=29;++d){
        const auto t0=Clock::now();
        auto report=world.advance_day();
        idle.push_back(millis(t0,Clock::now()));
        if(report.monthly_close)throw std::runtime_error("early month close");
    }
    Command payment;
    payment.id="DIRECT-CASH";
    payment.kind=Kind::payment;
    payment.from="PAYER";
    payment.to="RECEIVER";
    payment.amount=5;
    world.enqueue({2000,1,30},payment);
    const auto active_start=Clock::now();
    world.advance_day();
    const auto active_ms=millis(active_start,Clock::now());
    const auto closing_start=Clock::now();
    auto close=world.advance_day();
    const auto monthclose_ms=millis(closing_start,Clock::now());
    const auto& state=world.engine().base().core().core();
    if(!close.monthly_close || world.date()!=Date{2000,2,1} ||
       world.day_commit_id()!=31 || world.engine().month()!=24001 ||
       state.cash("PAYER")!=95 || state.cash("RECEIVER")!=5 ||
       state.people().population()!=prior)
        throw std::runtime_error("daily 30k simulated finance/POP conservation failed");
    std::cout<<std::fixed<<std::setprecision(3);
    std::cout<<"DAILY_30K_REAL_MEASURE"
             <<" provinces="<<provinces<<" pop_cells="<<pops
             <<" idle_p50_ms="<<percentile(idle,.50)
             <<" idle_p95_ms="<<percentile(idle,.95)
             <<" active_ms="<<active_ms
             <<" close_ms="<<monthclose_ms
             <<" peak_rss_mib="<<rss_mib()<<"\n";
    std::cout<<"DAILY_30K_CONSERVATION PASS\n";
    return 0;
  } catch(const std::exception& ex) {
    std::cerr<<"DAILY_30K_FAIL: "<<ex.what()<<"\n";
    return 1;
  }
}
