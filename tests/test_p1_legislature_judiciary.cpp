#include "vic25/daily_snapshot.hpp"
#include "vic25/p0_daily.hpp"
#include <stdexcept>
#include <string>
#include <vector>
using namespace vic25;
using namespace vic25::phasec;

static void require_test(bool value,const char* reason) {
    if(!value)throw std::runtime_error(reason);
}
static const IntegratedWorld& core(const daily::World& world) {
    return world.engine().base().core().core();
}
struct Fixture {
    phasegh::World gh{24000};
    explicit Fixture(const std::string& law_choice) {
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"R","A");
        geo.seed_unit(100,"A","Administration");
        geo.seed_assignment(1,100);
        auto& c=geo.seed_core().seed_core();
        c.seed_actor("A",1000);
        c.seed_actor("EMP",1000);
        c.seed_actor("WORKER",0);
        c.seed_government("A","HEAD","PM",5,3);
        c.seed_p1_constitution("A",{
            P1GovernancePrinciple::parliamentary_republic,
            P1PowerDistribution::universal_vote,
            P1LegislatureKind::unicameral,"test:constitutional"});
        c.seed_p1_coalition("A",{{},{"GOV"},false});
        c.seed_country_treasury("A","A");
        c.p4_register_party({"GOV","A","",800'000,true,true});
        c.p4_register_party({"OPP","A","",200'000,true,true});
        auto& pop=c.seed_population();
        const auto hh=pop.household_profile({"R","mixed","shared",100});
        const auto e=pop.economic_profile(
            {"R","worker","wage","employee",pop.default_education()});
        const auto d=pop.demographic_profile({
            "R",24000-360,phaseb::Sex::male,"c","r","citizen","free",
            pop.default_health(),phaseb::Service::civilian});
        const auto worker_slice=pop.seed_population(d,e,hh,100,1);
        pop.hire(worker_slice,1,"EMP","worker",100,{});
        if(!law_choice.empty()) {
            P2BaseLaw law;
            law.id="JUDICIAL_ACT";law.country="A";
            law.domain="judicial_system";law.scope="national";
            law.base_choice=law_choice;
            law.legal_basis="statute:judiciary";
            law.tier=P2NormTier::statute;
            c.p2_seed_base(std::move(law));
        }
        gh.validate();
    }
};
static void no_confidence_is_an_actual_vote() {
    Fixture f{"limited_independence"};
    auto world=daily::World::adopt(std::move(f.gh),101);
    phasegh::World::Plan vote;
    vote[9].push_back([](phasegh::Stage& gh) {
        auto& c=gh.phase_c();
        c.hold_country_election("A",100,0,{{"GOV",35},{"OPP",65}});
        c.p1_vote_no_confidence("A",{"OPP"},
            daily::to_day_index({2000,1,1}));
    });
    world.advance_day(vote);
    require_test(core(world).p1_coalition("A").caretaker,
                 "actual opposition majority must dismiss cabinet confidence");
    require_test(core(world).government("A").prime_minister=="PM",
                 "no-confidence cannot invent a successor politician");
    require_test(core(world).cash("A")==1000,
                 "parliamentary motion cannot change real treasury cash");
    const auto snap=daily::SnapshotCodec::encode(world);
    auto loaded=daily::SnapshotCodec::decode(snap);
    require_test(core(loaded).p1_coalition("A").caretaker&&
                 daily::SnapshotCodec::encode(loaded)==snap,
                 "no-confidence state must survive exact snapshot roundtrip");
}
static void judicial_law_constrains_only_overdue_real_claims() {
    for(const auto& [choice,expected]:std::vector<
          std::pair<std::string,Money>>{
              {"",100},{"executive_subordinate",40},
              {"limited_independence",65},{"independent",85}}) {
        Fixture f{choice};
        auto world=daily::World::adopt(std::move(f.gh),102);
        const auto observed=core(world).p1_judiciary_view("A");
        require_test(observed.defined_by_law==!choice.empty()&&
                     observed.enforceable_contract_ppm==
                        (choice.empty()?1'000'000:
                         choice=="executive_subordinate"?400'000:
                         choice=="limited_independence"?650'000:850'000),
                     "judicial policy must project from enacted law");
        phasegh::World::Plan accrue;
        accrue[5].push_back([](phasegh::Stage& gh){
            gh.phase_c().phase_time_accrue_contract_wage(
                "DEBT","EMP","WORKER","A","A",100,
                daily::to_day_index({2000,1,1}));
        });
        world.advance_day(accrue);
        for(int n=0;n<31;++n)world.advance_day();
        phasegh::World::Plan settle;
        settle[5].push_back([today=world.day_index()](phasegh::Stage& gh) {
            gh.phase_c().phase_time_settle_wage_arrears(today);
        });
        world.advance_day(settle);
        const auto remaining=core(world).wage_arrears().find("DEBT");
        require_test((remaining==core(world).wage_arrears().end()?
                      Money{0}:remaining->second.outstanding)==100-expected,
                     "overdue wage recovery must use actual employer cash");
        require_test(core(world).cash("EMP")==1000-expected,
                     "judicial enforcement may not fabricate creditor funding");
    }
}
int main() {
    no_confidence_is_an_actual_vote();
    judicial_law_constrains_only_overdue_real_claims();
}
