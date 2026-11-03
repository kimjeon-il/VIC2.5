#ifdef NDEBUG
#undef NDEBUG
#endif
#include "vic25/phase_c.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace vic25::phasec;
using vic25::phaseb::Service;
using vic25::phaseb::Sex;
using vic25::phaseb::Id;
using vic25::phaseb::People;

static void require(bool x, const char* why) { if (!x) throw std::runtime_error(why); }
static IntegratedWorld build_world() {
    IntegratedWorld w{5000};
    w.seed_actor("F",20000);
    w.seed_actor("H",1000);
    w.seed_actor("G",0);
    w.seed_stock("F","R","ore",1200);
    w.seed_region("R","A");
    w.seed_unit("U");
    auto& p=w.seed_population();
    auto h=p.household_profile({"R","mixed","shared",300});
    auto ew=p.economic_profile({"R","worker","wage","employee",p.default_education()});
    auto ec=p.economic_profile({"R","not_assigned","inactive","ineligible",p.default_education()});
    auto add=[&](int age, Sex sex, Id econ, People n){
        auto d=p.demographic_profile({"R",5000-age*12,sex,"C","T","citizen",
                                      "free",p.default_health(),Service::civilian});
        p.seed_population(d,econ,h,n);
    };
    add(25,Sex::female,ew,300);
    add(25,Sex::male,ew,300);
    add(8,Sex::female,ec,100);
    add(8,Sex::male,ec,100);
    add(70,Sex::female,ec,100);
    add(70,Sex::male,ec,100);
    vic25::phaseb::DemographyRules r;
    r.fertility_ppm=7000;
    r.illness_incidence_ppm=80;
    r.accident_ppm=50;
    r.migration_ppm=0;
    w.enable_demography(r);
    w.validate();
    return w;
}
static std::int64_t run(IntegratedWorld& w) {
    std::int64_t completed = 0;
    for (int month = 0; month < 1200; ++month) {
        IntegratedWorld::Plan p;
        bool manufactured = false;
        p[1].push_back([](auto& x){x.lend("F",10);});
        p[2].push_back([](auto& x){
            for (const auto& [sid,contract] : x.people().jobs())
                if (contract.employer=="F" && contract.count>0) return;
            for (const auto& [sid,s] : x.people().slices()) {
                if (s.count>x.people().employed(sid) &&
                    x.people().eligible_for(sid,vic25::phaseb::JobRequirements{})) {
                    x.hire(sid,1,"F",5);
                    return;
                }
            }
        });
        p[4].push_back([&](auto& x){
            People employed = 0;
            for (const auto& [sid,job] : x.people().jobs()) {
                (void)sid;
                if (job.employer=="F") employed+=job.count;
            }
            if (employed>0 && x.stock("F","R","ore")>0) {
                x.produce("F","R","ore","tool",1,1);
                manufactured = true;
            }
        });
        p[5].push_back([&](auto& x){if(manufactured)x.wages("F","H","G",5,1);});
        p[7].push_back([&](auto& x){
            if(manufactured) {
                x.trade("F","H","R","tool",1,4);
                x.consume("H","R","tool",1);
            }
        });
        p[8].push_back([](auto& x){x.repay("F",10);});
        if(month%48==0)p[9].push_back([](auto& x){
            const People eligible=x.eligible_voters();
            if (eligible>0)
                x.hold_election(std::min<People>(20,eligible),0,3,
                                {{"A",std::min<People>(20,eligible)}});
        });
        const auto report=w.advance(p);
        completed+=manufactured?1:0;
        require(report.finished_month==5001+month,"monthly time");
        require(w.cash("F")==20000-completed,"firm cash");
        require(w.cash("H")==1000,"household cash");
        require(w.cash("G")==completed,"government taxes");
        require(w.cash("F")+w.cash("H")+w.cash("G")==21000,"deposit conservation");
        require(w.stock("F","R","ore")==1200-completed,"ore conservation");
        require(w.stock("F","R","tool")==0&&w.stock("H","R","tool")==0,
                "phantom inventory");
        require(w.financial().balance("F:debt")==0,"loan principal outstanding");
        w.validate();
    }
    return completed;
}
int main(){
    const auto start=std::chrono::steady_clock::now();
    auto a=build_world(),b=a;
    const auto produced_a=run(a),produced_b=run(b);
    require(a.commit_id()==1200 && b.commit_id()==1200,"commit count");
    require(produced_a==produced_b&&produced_a>0,"production not deterministic");
    require(a.people().fingerprint()==b.people().fingerprint(),"demography replay mismatch");
    require(a.cash("F")==b.cash("F")&&a.cash("G")==b.cash("G"),
            "finance replay mismatch");
    require(a.journal()==b.journal(),"ordered journal differs");
    const auto seconds=std::chrono::duration<double>(
        std::chrono::steady_clock::now()-start).count();
    std::cout<<"Phase C vertical slice 1,200 months ×2 completed\n"
             <<"initial_people=1000 end_people="<<a.people().population()
             <<" products="<<produced_a
             <<" cash_F="<<a.cash("F")<<" cash_H="<<a.cash("H")
             <<" cash_G="<<a.cash("G")
             <<" events="<<a.journal().size()
             <<" elapsed_seconds="<<seconds<<"\n"
             <<"Coverage: real Phase B demographic dynamics + actual finance/inventory/"
               "production/wages/elections. Missing: endogenous market pricing, political"
               " movements, full war operations, full-world persistence.\n";
}
