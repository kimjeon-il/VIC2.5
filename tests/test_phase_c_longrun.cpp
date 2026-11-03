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

static void require(bool x, const char* why) { if(!x) throw std::runtime_error(why); }

static IntegratedWorld build_world() {
    IntegratedWorld w{5000};
    w.seed_actor("F", 20000);
    w.seed_actor("H", 1000);
    w.seed_actor("G", 0);
    w.seed_stock("F", "R", "ore", 1200);
    w.seed_region("R", "A");
    w.seed_unit("A-unit");
    auto& p = w.seed_population();
    auto h = p.household_profile({"R", "workers", "shared", 20});
    auto e = p.economic_profile({"R", "worker", "wage", "employee", p.default_education()});
    auto d = p.demographic_profile({"R", 4700, Sex::male, "C", "T", "citizen",
                                    "free", p.default_health(), Service::civilian});
    p.seed_population(d, e, h, 20);
    w.validate();
    return w;
}
static void run(IntegratedWorld& w) {
    const auto slice = w.people().slices().begin()->first;
    for (int month = 0; month < 1200; ++month) {
        IntegratedWorld::Plan p;
        if (month == 0) p[2].push_back([slice](auto& x) { x.hire(slice, 1, "F", 5); });
        p[1].push_back([](auto& x){ x.lend("F", 10); });
        p[4].push_back([](auto& x){ x.produce("F", "R", "ore", "tool", 1, 1); });
        p[5].push_back([](auto& x){ x.wages("F", "H", "G", 5, 1); });
        p[7].push_back([](auto& x){
            x.trade("F", "H", "R", "tool", 1, 4);
            x.consume("H", "R", "tool", 1);
        });
        p[8].push_back([](auto& x){ x.repay("F", 10); });
        if (month % 48 == 0) p[9].push_back([](auto& x){
            x.hold_election(20, 0, 3, {{"A", 12}, {"B", 8}});
        });
        const auto report = w.advance(p);
        require(report.finished_month == 5001+month, "month index incorrect");
        require(w.people().population() == 20, "unexpected population");
        require(w.cash("F") == 20000-(month+1), "firm cash circulation incorrect");
        require(w.cash("H") == 1000, "worker consumption/transfer incorrect");
        require(w.cash("G") == month+1, "withholding accounting incorrect");
        require(w.cash("F")+w.cash("H")+w.cash("G")==21000, "money disappeared");
        require(w.stock("F", "R", "ore")==1199-month, "production input mismatch");
        require(w.stock("F", "R", "tool")==0, "phantom output");
        require(w.stock("H", "R", "tool")==0, "consumer double purchase");
        require(w.financial().balance("F:debt")==0, "loan repayment principal mismatch");
        w.validate();
    }
}
int main() {
    const auto started=std::chrono::steady_clock::now();
    auto a=build_world(), b=a;
    run(a);
    run(b);
    require(a.commit_id()==1200 && b.commit_id()==1200,"commit count");
    require(a.people().fingerprint()==b.people().fingerprint(),"population deterministic replay");
    require(a.cash("F")==b.cash("F") && a.cash("G")==b.cash("G"),"finance deterministic replay");
    require(a.journal()==b.journal(),"cross-domain event order differs");
    const auto seconds=std::chrono::duration<double>(
        std::chrono::steady_clock::now()-started).count();
    std::cout<<"Phase C integrated vertical-slice 1200-month deterministic regression PASS\n"
             <<"starting_people=20 end_people="<<a.people().population()
             <<" months="<<a.commit_id()
             <<" firm_cash="<<a.cash("F")
             <<" worker_cash="<<a.cash("H")
             <<" tax_cash="<<a.cash("G")
             <<" final_ore="<<a.stock("F","R","ore")
             <<" event_count="<<a.journal().size()
             <<" two_runs_seconds="<<seconds<<"\n"
             <<"Limit: no endogenous political unrest, combat, demography or macro-price feedback.\n";
    return 0;
}
