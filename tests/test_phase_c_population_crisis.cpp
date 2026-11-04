#include "phase_c_t46_crisis.hpp"
int main(){
    auto start=std::chrono::steady_clock::now();
    CrisisScenario a,b;
    perform(a);perform(b);
    require(a.w.people().fingerprint()==b.w.people().fingerprint(),
            "population/demography differs on repeated independent 1200-month run");
    require(a.w.journal()==b.w.journal(),"cross-domain journal replay not deterministic");
    for(auto actor:{"F","H","G","S"})
        require(a.w.cash(actor)==b.w.cash(actor),"financial outcomes diverge");
    require(a.w.active_strength("U")==b.w.active_strength("U"),
            "military assignments diverge");
    const auto duration=std::chrono::duration<double>(
        std::chrono::steady_clock::now()-start).count();
    std::cout<<"D4-T46 full-scope representative synthetic 1200 month ×2 regression PASS"
             <<" total_people="<<a.w.people().population()
             <<" active_slices="<<a.w.people().slices().size()
             <<" active_units="<<a.w.active_strength("U")
             <<" committed_months="<<a.w.commit_id()
             <<" journal_events="<<a.w.journal().size()
             <<" seconds_both="<<duration<<"\n";
    return 0;
}
