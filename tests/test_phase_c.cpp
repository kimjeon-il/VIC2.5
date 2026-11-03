#ifdef NDEBUG
#undef NDEBUG
#endif
#include "vic25/phase_c_snapshot.hpp"
#include "phase_c_t46_crisis.hpp"
#include "phase_c_t47_scaling.hpp"
#include <filesystem>
#include <fstream>
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace vic25::phasec;
using vic25::phaseb::Service;
using vic25::phaseb::Sex;

static void verify(bool ok, const char* why) {
    if (!ok) throw std::runtime_error(why);
}
template<class Fn> static bool rejected(Fn&& fn) {
    try { fn(); } catch (const std::exception&) { return true; }
    return false;
}
struct F {
    IntegratedWorld w{24000};
    Id civ{}, reserve{}, active{}, child{};
    F() {
        w.seed_actor("F", 1000);
        w.seed_actor("H", 50);
        w.seed_actor("G", 0);
        w.seed_actor("S", 0);
        w.seed_actor("T", 0);
        w.seed_stock("S", "R", "ore", 200);
        w.seed_stock("U", "R", "ammunition", 100);
        w.seed_region("R", "A");
        w.seed_unit("U");
        auto& p = w.seed_population();
        auto h = p.household_profile({"R", "mixed", "shared", 10});
        auto e = p.economic_profile({"R", "worker", "wage", "employee", p.default_education()});
        auto nc = p.economic_profile({"R", "not_assigned", "inactive", "ineligible", p.default_education()});
        auto make = [&](Service service, std::int64_t birth, Id economic, People count) -> Id {
            const auto d = p.demographic_profile(
                {"R", birth, Sex::male, "culture", "religion", "citizen",
                 "free", p.default_health(), service});
            return p.seed_population(d, economic, h, count);
        };
        civ = make(Service::civilian, 23640, e, 200);
        reserve = make(Service::reserve, 23640, e, 100);
        active = make(Service::active, 23640, e, 100);
        child = make(Service::civilian, 23900, nc, 200);
        w.seed_active("U", active);
        w.validate();
    }
};
struct Suite {
    int pass = 0, blocked = 0, failed = 0;
    std::vector<std::string> status;
    void run(int number, const std::function<void()>& fn) {
        const auto id = "D4-T" + (number < 10 ? std::string("0") : std::string()) +
                        std::to_string(number);
        try {
            fn();
            status.push_back(id + " PASS");
            ++pass;
        } catch (const std::exception& ex) {
            status.push_back(id + " FAIL: " + ex.what());
            ++failed;
        }
    }
    void skip(int number, const std::string& reason) {
        const auto id = "D4-T" + (number < 10 ? std::string("0") : std::string()) +
                        std::to_string(number);
        status.push_back(id + " BLOCKED: " + reason);
        ++blocked;
    }
};

static void salary(F& f) {
    IntegratedWorld::Plan p;
    p[2].push_back([&](auto& w){w.hire(f.civ, 20, "F", 10);});
    p[5].push_back([](auto& w){w.wages("F", "H", "G", 200, 20);});
    f.w.advance(p);
}
static void election(F& f) {
    IntegratedWorld::Plan p;
    p[9].push_back([](auto& w){
        w.hold_election(250, 20, 3, {{"A", 130}, {"B", 70}, {"C", 30}});
    });
    f.w.advance(p);
}
static void combat(F& f) {
    IntegratedWorld::Plan p;
    p[10].push_back([&](auto& w){w.battle("U", f.active, "R", CombatLoss{50, 20, 10, 0, 0, 5});});
    f.w.advance(p);
}
static void integration_election_mobilization() {
    IntegratedWorld w{240};
    w.seed_actor("F",500);
    w.seed_actor("H",0);
    w.seed_actor("G",0);
    w.seed_region("R","A");
    w.seed_unit("U");
    w.seed_stock("U","R","ammunition",20);
    auto& p=w.seed_population();
    const auto h=p.household_profile({"R","households","shared",500});
    const auto e=p.economic_profile({"R","worker","wage","employee",p.default_education()});
    const auto n=p.economic_profile({"R","not_assigned","inactive","ineligible",p.default_education()});
    auto add=[&](Service service,int age,People count,Id econ){
        auto d=p.demographic_profile({"R",240-age*12,Sex::male,"c","r","citizen","free",p.default_health(),service});
        return p.seed_population(d,econ,h,count);
    };
    const Id adult=add(Service::civilian,25,500,e);
    add(Service::civilian,19,50,e);
    add(Service::civilian,10,250,n);
    const Id reserve=add(Service::reserve,25,100,e);
    const Id active=add(Service::active,25,100,e);
    w.seed_active("U",active);
    verify(w.people().population()==1000 && w.eligible_voters()==600,"bad initial voter count");
    IntegratedWorld::Plan plan;
    plan[2].push_back([adult,reserve](auto& st){
        st.hire(adult,260,"F",1);
        st.hire(reserve,40,"F",1);
    });
    plan[5].push_back([](auto& st){st.wages("F","H","G",300,30);});
    plan[9].push_back([](auto& st){
        st.hold_election(550,20,3,{{"A",220},{"B",130},{"C",50}});
        st.enact_voting_age(18);
    });
    plan[10].push_back([active,reserve](auto& st){
        st.battle("U",active,"R",CombatLoss{10,20,0,0,0,10});
        st.mobilize(reserve,40,"U");
    });
    w.advance(plan);
    verify(w.people().population()==990 && w.active_strength("U")==130,"bad casualty/mobilization");
    verify(w.cash("F")==200 && w.cash("H")==270 && w.cash("G")==30,"bad payroll/tax");
    verify(w.people().labor().employed==260,"employment not released by mobilization");
    verify(w.election().seats.at("A")==2 && w.election().seats.at("B")==1 &&
           w.election().seats.at("C")==0,"bad seats");
    verify(w.voting_age()==21,"law applied before next month");
    w.advance();
    verify(w.voting_age()==18 && w.eligible_voters()==610,"law not effective next month");
}
static void integration_logistics_occupation() {
    IntegratedWorld w{300};
    w.seed_actor("G",200);
    w.seed_actor("F",0);
    w.seed_actor("T",0);
    w.seed_region("A","A");
    w.seed_region("X","B");
    w.seed_unit("A-UNIT");
    w.seed_unit("B-UNIT");
    w.seed_stock("F","A","medicine",60);
    w.seed_stock("B-UNIT","X","ammunition",5);
    auto& p=w.seed_population();
    auto ha=p.household_profile({"A","families","shared",50});
    auto hb=p.household_profile({"X","families","shared",200});
    auto ea=p.economic_profile({"A","soldier","wage","employee",p.default_education()});
    auto eb=p.economic_profile({"X","soldier","wage","employee",p.default_education()});
    auto ec=p.economic_profile({"X","farmer","subsistence","self_employed",p.default_education()});
    auto da=p.demographic_profile({"A",0,Sex::male,"a","r","citizen","free",p.default_health(),Service::active});
    auto db=p.demographic_profile({"X",0,Sex::male,"b","r","citizen","free",p.default_health(),Service::active});
    auto dc=p.demographic_profile({"X",0,Sex::female,"b","r","citizen","free",p.default_health(),Service::civilian});
    auto a=p.seed_population(da,ea,ha,100);
    auto b=p.seed_population(db,eb,hb,20);
    p.seed_population(dc,ec,hb,480);
    w.seed_active("A-UNIT",a);
    w.seed_active("B-UNIT",b);
    IntegratedWorld::Plan first;
    first[7].push_back([](auto& st){
        st.pay("G","F",120);
        st.pay("G","T",10);
        st.ship("med-1","F","A-UNIT","A","X","medicine",60,2,5);
    });
    w.advance(first);
    verify(w.cash("G")==70 && w.cash("F")==120 && w.cash("T")==10,"bad procurement");
    verify(w.stock("A-UNIT","X","medicine")==0 && w.stock("TRANSIT:med-1","in_transit","medicine")==60,
           "bad cargo dispatch");
    w.advance();
    verify(w.stock("A-UNIT","X","medicine")==0,"used cargo before arrival");
    IntegratedWorld::Plan final;
    final[10].push_back([a,b](auto& st){
        st.battle("A-UNIT",a,"X",CombatLoss{3,7,0,0,0,0});
        st.battle("B-UNIT",b,"X",CombatLoss{0,0,0,0,0,5});
        st.consume("A-UNIT","X","medicine",7);
        st.occupy("X","A");
    });
    w.advance(final);
    verify(w.people().population()==597 && w.active_strength("A-UNIT")==97, "bad combat");
    verify(w.stock("A-UNIT","X","medicine")==48,"bad treatment stock");
    verify(w.military_control("X")=="A" && w.sovereignty("X")=="B","occupation overwrote sovereign");
}
int main() {
    Suite test;
    test.run(1, []{ F f;verify(f.w.people().population()==600,"duplicated population"); });
    test.run(2, []{
        F f; IntegratedWorld::Plan p;
        p[2].push_back([&](auto& w){w.hire(f.civ,200,"F",1);});
        p[10].push_back([&](auto& w){w.mobilize(f.civ,201,"U");});
        verify(rejected([&]{f.w.advance(p);}),"overallocated civilian");
        verify(f.w.month()==24000 && f.w.people().employed(f.civ)==0,"partial rollback");
    });
    test.run(3, []{
        F f;
        f.w.seed_government("A","King","Prime",3,2);
        const Money cash_before=f.w.cash("F");
        IntegratedWorld::Plan p;
        p[9].push_back([](auto& w){w.change_head_of_state("A","King","Queen");});
        f.w.advance(p);
        verify(f.w.government("A").head_of_state=="King", "succession retroactive");
        f.w.advance();
        verify(f.w.government("A").head_of_state=="Queen" &&
               f.w.government("A").prime_minister=="Prime" &&
               f.w.cash("F")==cash_before &&
               f.w.people().population()==600,
               "government succession reset state or obligations");
    });
    test.run(4, []{
        F f;IntegratedWorld::Plan p;
        p[10].push_back([](auto& w){w.occupy("R","B");});
        f.w.advance(p);
        verify(f.w.military_control("R")=="B"&&f.w.sovereignty("R")=="A","sovereignty mutated");
    });
    test.run(5, []{
        F f;
        f.w.seed_actor("U",0);
        f.w.seed_region("Unused","A");
        f.w.seed_region("Replacement","A");
        const auto military_id=f.w.typed_id(EntityKind::unit,"U");
        const auto financial_id=f.w.typed_id(EntityKind::actor,"U");
        const auto old=f.w.typed_id(EntityKind::region,"Unused");
        const auto successor=f.w.typed_id(EntityKind::region,"Replacement");
        verify(military_id!=financial_id,"typed ID namespaces collapsed");
        verify(rejected([&]{f.w.require_typed(military_id,EntityKind::region);}),
               "wrong entity type accepted");
        verify(rejected([&]{f.w.seed_region("Unused","A");}),"duplicate region ID issued");
        IntegratedWorld::Plan retire;
        retire[9].push_back([](auto& w){w.retire_unused_region("Unused","Replacement");});
        f.w.advance(retire);
        verify(f.w.successor_of(old)==successor,"lineage successor not recorded");
        verify(rejected([&]{f.w.require_typed(old,EntityKind::region);}),
               "retired typed ID was reused");
        IntegratedWorld::Plan abuse;
        abuse[10].push_back([](auto& w){w.occupy("Unused","B");});
        verify(rejected([&]{f.w.advance(abuse);}),"tombstoned location accepted orders");
    });
    test.run(6, []{
        F f;
        f.w.seed_actor("H2",0);
        f.w.seed_shares("land","H",5);
        auto& p=f.w.seed_population();
        const auto economic=p.economic_profile({"R","farmer","subsistence","self_employed",p.default_education()});
        const auto old=p.slice(f.civ);
        const auto origin=p.seed_population(old.key.demographic,economic,old.key.household,10,8801);
        const auto hh2=p.household_profile({"R2","migrants","shared",2});
        const Money before=f.w.cash("H")+f.w.cash("H2");
        IntegratedWorld::Plan plan;
        plan[10].push_back([&](auto& w){
            const auto arrived=w.migrate_with_explicit_property_allocation(
                origin,2,"R2",hh2,"H","H2",20,"land",2);
            verify(w.people().demographic(w.people().slice(arrived).key.demographic).region=="R2",
                   "migrant destination not allocated");
        });
        f.w.advance(plan);
        verify(f.w.people().population()==610 &&
               f.w.cash("H")+f.w.cash("H2")==before &&
               f.w.cash("H")==30 && f.w.cash("H2")==20 &&
               f.w.shares("land","H")==3 && f.w.shares("land","H2")==2 &&
               f.w.financial().balance("H2:security:land")==2 &&
               f.w.people().slice(origin).count==8,
               "partial migration duplicated residents or lost explicitly allocated property");
    });
    test.run(7, []{
        F f;
        f.w.seed_forex_balance("F","USD",100);
        f.w.seed_forex_balance("F","EUR",0);
        f.w.seed_forex_balance("H","EUR",90);
        f.w.seed_forex_balance("H","USD",0);
        IntegratedWorld::Plan exchange;
        exchange[7].push_back([](auto& w){w.exchange_fx("F","H","USD",100,"EUR",90);});
        f.w.advance(exchange);
        verify(f.w.fx_balance("F","USD")==0&&f.w.fx_balance("F","EUR")==90&&
               f.w.fx_balance("H","USD")==100&&f.w.fx_balance("H","EUR")==0&&
               f.w.cash("F")==1000&&f.w.cash("H")==50,
               "currency exchange minted or merged unrelated currency balances");
        IntegratedWorld::Plan nonexistent;
        nonexistent[7].push_back([](auto& w){w.exchange_fx("F","H","USD",50,"EUR",50);});
        verify(rejected([&]{f.w.advance(nonexistent);}),
               "unfunded foreign exchange was created");
    });
    test.run(8, []{
        F f;IntegratedWorld::Plan p;
        p[5].push_back([](auto& w){
            w.command_once("pay-1", [](auto& x){x.pay("F","H",10);});
            w.command_once("pay-1", [](auto& x){x.pay("F","H",10);});
        });
        verify(rejected([&]{f.w.advance(p);}),"duplicate command applied");
        verify(f.w.cash("F")==1000&&f.w.cash("H")==50&&f.w.commit_id()==0,
               "duplicate command did not roll back");
    });
    test.run(9, []{
        F f;IntegratedWorld::Plan p;
        p[2].push_back([&](auto& w){w.hire(f.civ,2,"F",10);});
        p[4].push_back([](auto& w){w.produce("F","R","ore","tool",5,2);});
        verify(rejected([&]{f.w.advance(p);}),"missing input should fail");
        verify(f.w.month()==24000 && f.w.stock("F","R","tool")==0,"rollback failed");
    });
    test.run(10, []{
        F f;IntegratedWorld::Plan p;
        p[3].push_back([](auto& w){w.trade("S","F","R","ore",200,10);});
        verify(rejected([&]{f.w.advance(p);}),"overspend allowed");
        verify(f.w.cash("F")==1000 && f.w.stock("S","R","ore")==200,"money/stock rollback failed");
    });
    test.run(11, []{
        F f;
        auto& p=f.w.seed_population();
        auto e=p.economic_profile({"R","farmer","subsistence","self_employed",p.default_education()});
        const auto old=p.slice(f.civ);
        const auto household=old.key.household;
        const auto own=p.seed_population(old.key.demographic,e,household,3,999);
        const Money initial_cash=f.w.cash("H");
        IntegratedWorld::Plan plan;
        plan[4].push_back([own](auto& w){w.household_produce(own,"food",6,2);});
        plan[7].push_back([own](auto& w){w.household_consume(own,"food",4);});
        f.w.advance(plan);
        verify(f.w.people().population()==603&&
               f.w.actual_household_consumption(household,"food")==4&&
               f.w.stock("HH:"+std::to_string(household),"R","food")==2&&
               f.w.cash("H")==initial_cash,
               "in-kind produce/consume invented cash or stock");
    });
    test.run(12, []{
        F f;
        f.w.seed_stock("S","R","food",5);
        f.w.seed_market("R","food",4,2,5,1);
        IntegratedWorld::Plan plan;
        plan[7].push_back([](auto& w){
            verify(w.clear_consumer_market("S","H","R","food",10)==5,
                   "market sold more than stock");
        });
        f.w.advance(plan);
        const auto& m=f.w.market("R","food");
        verify(m.delivered==5 && m.solvent_demand==10 && m.unmet==5 &&
               m.price==5 && f.w.stock("S","R","food")==0 &&
               f.w.stock("H","R","food")==5 && f.w.cash("H")==30,
               "market shortage and next-month quote mismatch");
        f.w.advance();
        verify(f.w.market("R","food").price==5,"price altered without new price pressure");
    });
    test.run(13, []{
        F f;IntegratedWorld::Plan p;
        p[1].push_back([](auto& w){w.lend("F",100);});
        p[8].push_back([](auto& w){w.repay("F",30);});
        f.w.advance(p);
        verify(f.w.cash("F")==1070 && f.w.financial().balance("F:debt")==-70,
               "loan principal not conserved");
    });
    test.run(14, []{
        F f;f.w.seed_shares("firm-equity", "F", 100);
        IntegratedWorld::Plan p;
        p[7].push_back([](auto& w){w.sell_shares("firm-equity","F","H",10);});
        p[9].push_back([](auto& w){w.declare_dividend("firm-equity","F","H",3);});
        f.w.advance(p);
        verify(f.w.shares("firm-equity","F")==90 &&
               f.w.shares("firm-equity","H")==10 &&
               f.w.cash("H")==40, "share title/cash mismatch");
        IntegratedWorld::Plan next;
        next[5].push_back([](auto& w){w.pay_dividend("firm-equity","H");});
        f.w.advance(next);
        verify(f.w.cash("H")==43 && f.w.shares("firm-equity","H")==10,
               "dividend payment not matched to shares");
    });
    test.run(15, []{F f;salary(f);verify(f.w.cash("G")==20&&f.w.cash("H")==230,"wage withholding");});
    test.run(16, []{
        F f;IntegratedWorld::Plan p;
        p[1].push_back([](auto& w){w.issue_bond("issue-001","G","H",40);});
        f.w.advance(p);
        verify(f.w.cash("G")==40 && f.w.cash("H")==10 &&
               f.w.shares("bond:issue-001","H")==40 &&
               f.w.financial().balance("G:debt")==-40 &&
               f.w.financial().balance("H:security:bond:issue-001")==40,
               "bond issuance without actual paid proceeds");
    });
    test.run(17, []{
        F f;
        f.w.seed_government("A","Head","Premier",3,2);
        IntegratedWorld::Plan illegal;
        illegal[9].push_back([](auto& w){
            w.hold_election(250,0,3,{{"A",160},{"B",90}});
            w.propose_tax_law("A","Intruder",{"A"},300'000);
        });
        verify(rejected([&]{f.w.advance(illegal);}),"unauthorized law passed");
        verify(f.w.month()==24000 && f.w.income_tax_ppm()==100'000,
               "illegal bill changed month or tax");
        IntegratedWorld::Plan valid;
        valid[9].push_back([](auto& w){
            w.hold_election(250,0,3,{{"A",160},{"B",90}});
            w.propose_tax_law("A","Premier",{"A"},300'000);
        });
        f.w.advance(valid);
        verify(f.w.income_tax_ppm()==100'000,"new statute applied same month");
        f.w.advance();
        verify(f.w.income_tax_ppm()==300'000,"valid legislature failed");
    });
    test.run(18, []{
        F f;IntegratedWorld::Plan p;
        p[9].push_back([](auto& w){w.enact_voting_age(18);});
        f.w.advance(p);
        verify(f.w.voting_age()==21,"law effective same month");
        f.w.advance();verify(f.w.voting_age()==18,"law did not become effective");
    });
    test.run(19, []{
        F f;election(f);
        const auto& e=f.w.election();
        verify(e.eligible==300&&e.cast==250&&e.valid==230&&e.invalid==20,"ballot accounting");
    });
    test.run(20, []{
        F f;IntegratedWorld::Plan p;
        // Eligible adult civilians + reservists are 300; use 220/130/50
        // as synthetic ballot weights only after seeding 100 more adult voters.
        auto& p0=f.w.seed_population();
        const auto old=p0.slice(f.civ);
        p0.seed_population(old.key.demographic,old.key.economic,old.key.household,100,old.key.partition);
        p[9].push_back([](auto& w){w.hold_election(400,0,3,{{"A",220},{"B",130},{"C",50}});});
        f.w.advance(p);
        verify(f.w.election().seats.at("A")==2&&f.w.election().seats.at("B")==1,"D'Hondt");
    });
    test.run(21, []{
        F f;f.w.seed_person_anchor("voter-1",f.civ);
        IntegratedWorld::Plan duplicate;
        duplicate[9].push_back([](auto& w){
            w.cast_named_ballot("election-1","north","voter-1","A");
            w.cast_named_ballot("election-1","south","voter-1","B");
        });
        verify(rejected([&]{f.w.advance(duplicate);}),"same voter voted in two districts");
        verify(f.w.commit_id()==0&&f.w.named_votes("election-1")==0,"failed votes persisted");
        IntegratedWorld::Plan legitimate;
        legitimate[9].push_back([](auto& w){w.cast_named_ballot("election-1","north","voter-1","A");});
        f.w.advance(legitimate);
        verify(f.w.named_votes("election-1")==1,"valid anchor vote missing");
        verify(rejected([&]{f.w.advance(legitimate);}),"replayed same election vote");
    });
    test.run(22, []{
        F f;
        f.w.seed_government("A","Head","Outgoing",3,2);
        IntegratedWorld::Plan bad;
        bad[9].push_back([](auto& w){
            w.hold_election(250,0,3,{{"A",160},{"B",90}});
            w.appoint_cabinet("A","Head","NewPremier",{"B"});
        });
        verify(rejected([&]{f.w.advance(bad);}),"minority appointed without confidence");
        verify(f.w.government("A").prime_minister=="Outgoing","failed appointment mutated state");
        IntegratedWorld::Plan legal;
        legal[9].push_back([](auto& w){
            w.hold_election(250,0,3,{{"A",160},{"B",90}});
            w.appoint_cabinet("A","Head","NewPremier",{"A"});
        });
        f.w.advance(legal);
        verify(f.w.government("A").prime_minister=="Outgoing","new cabinet active too early");
        f.w.advance();
        verify(f.w.government("A").prime_minister=="NewPremier","valid coalition appointment failed");
    });
    test.run(23, []{
        F f;f.w.seed_interest_group("Workers");f.w.seed_interest_group("Agrarians");
        IntegratedWorld::Plan p;
        p[9].push_back([&](auto& w){
            w.join_interest_group(f.civ,"Workers",150);
            w.join_interest_group(f.civ,"Agrarians",100);
            w.protest(f.civ,"Workers",50);
            w.protest(f.civ,"Agrarians",20);
        });
        f.w.advance(p);
        verify(f.w.group_members(f.civ,"Workers")==150&&
               f.w.group_members(f.civ,"Agrarians")==100&&
               f.w.protest_participants(f.civ)==70&&
               f.w.people().population()==600,
               "group memberships and actual participants were conflated");
    });
    test.run(24, []{
        F f;
        f.w.seed_public_institution("hospital","G","R","medicine",3);
        f.w.seed_stock("G","R","medicine",2);
        IntegratedWorld::Plan legislature;
        legislature[9].push_back([](auto& w){w.enact_public_healthcare(true);});
        f.w.advance(legislature);
        verify(!f.w.has_public_healthcare_right(),"new healthcare law applied too early");
        IntegratedWorld::Plan impossible;
        impossible[2].push_back([&](auto& w){w.hire(f.civ,1,"G",10);});
        impossible[3].push_back([](auto& w){w.pay("F","G",20);});
        impossible[5].push_back([](auto& w){w.wages("G","H","S",10,0);});
        impossible[7].push_back([&](auto& w){w.provide_public_healthcare("hospital",f.civ,3);});
        verify(rejected([&]{f.w.advance(impossible);}),"medical care without medicine allowed");
        verify(f.w.stock("G","R","medicine")==2 && f.w.people().employed(f.civ)==0,
               "failed healthcare month leaked");
        IntegratedWorld::Plan actual;
        actual[2].push_back([&](auto& w){w.hire(f.civ,1,"G",10);});
        actual[3].push_back([](auto& w){w.pay("F","G",20);});
        actual[5].push_back([](auto& w){w.wages("G","H","S",10,0);});
        actual[7].push_back([&](auto& w){w.provide_public_healthcare("hospital",f.civ,2);});
        f.w.advance(actual);
        verify(f.w.has_public_healthcare_right() &&
               f.w.public_services_rendered("hospital")==2 &&
               f.w.stock("G","R","medicine")==0 &&
               f.w.people().population()==600,"public care without real resource/worker");
    });
    test.run(25, []{F f;verify(f.w.people().population()==600 && f.w.active_strength("U")==100,"service count");});
    test.run(26, []{
        F f;IntegratedWorld::Plan p;
        p[10].push_back([&](auto& w){w.mobilize(f.reserve,40,"U");w.mobilize(f.reserve,60,"U");});
        f.w.advance(p);
        verify(f.w.active_strength("U")==200&&f.w.people().population()==600,"mobilization");
    });
    test.run(27, []{
        F f;IntegratedWorld::Plan p;
        p[2].push_back([&](auto& w){w.hire(f.reserve,40,"F",1);});
        p[5].push_back([](auto& w){w.wages("F","H","G",40,0);});
        p[10].push_back([&](auto& w){w.mobilize(f.reserve,40,"U");});
        f.w.advance(p);
        verify(f.w.cash("H")==90&&f.w.people().labor().employed==0&&
               f.w.active_strength("U")==140,"employment before mobilization");
    });
    test.run(28, []{
        F f;
        f.w.seed_unit("V");
        f.w.seed_formation("DIV");
        f.w.attach_to_formation("DIV","U");
        f.w.attach_to_formation("DIV","V");
        const auto& row=f.w.seed_population().slice(f.active);
        auto other=f.w.seed_population().seed_population(
            row.key.demographic,row.key.economic,row.key.household,60,7878);
        f.w.seed_active("V",other);
        f.w.seed_stock("U","R","rifle",70);
        verify(f.w.formation_strength("DIV")==160,"double counted formation");
        IntegratedWorld::Plan duplicate;
        duplicate[7].push_back([](auto& w){
            w.assign_equipment("U","R","rifle",70);
            w.assign_equipment("V","R","rifle",70);
        });
        verify(rejected([&]{f.w.advance(duplicate);}),"equipment duplicate accepted");
        verify(f.w.commit_id()==0,"failed allocation leaked");
        IntegratedWorld::Plan real;
        real[7].push_back([](auto& w){w.assign_equipment("U","R","rifle",70);});
        f.w.advance(real);
        verify(f.w.assigned_equipment("U","R","rifle")==70&&
               f.w.assigned_equipment("V","R","rifle")==0&&
               f.w.formation_strength("DIV")==160,
               "real formation/equipment invalid");
    });
    test.run(29, []{
        F f;IntegratedWorld::Plan p;
        p[7].push_back([](auto& w){w.ship("a","S","H","R","B","ore",60,2,5);});
        f.w.advance(p);verify(f.w.stock("H","B","ore")==0,"instant arrival");
        f.w.advance();verify(f.w.stock("H","B","ore")==0,"early arrival");
        f.w.advance();verify(f.w.stock("H","B","ore")==55,"wrong arrival/loss");
    });
    test.run(30, []{
        F f;IntegratedWorld::Plan p;
        p[10].push_back([&](auto& w){
            w.battle("U",f.active,"R",CombatLoss{0,0,0,0,0,70});
            w.battle("U",f.active,"R",CombatLoss{0,0,0,0,0,40});
        });
        verify(rejected([&]{f.w.advance(p);}),"double ammo allowed");
        verify(f.w.stock("U","R","ammunition")==100,"roll back ammo");
    });
    test.run(31, []{
        F f;combat(f);verify(f.w.people().population()==550 && f.w.active_strength("U")==50,
                             "battle population/assignment");
        verify(f.w.stock("U","R","ammunition")==95,"ammunition");
    });
    test.run(32, []{
        F f;IntegratedWorld::Plan p;
        p[10].push_back([](auto& w){w.occupy("R","B");});
        f.w.advance(p);verify(f.w.sovereignty("R")=="A"&&f.w.military_control("R")=="B",
                              "control vs sovereignty");
    });
    test.run(33, integration_election_mobilization);
    test.run(34, integration_logistics_occupation);
    test.run(35, []{
        F f;
        f.w.seed_interest_group("Food Movement");
        f.w.seed_stock("S","R","food",5);
        f.w.seed_market("R","food",4,2,10,1);
        IntegratedWorld::Plan p;
        p[7].push_back([](auto& w){
            verify(w.clear_consumer_market("S","H","R","food",10)==5,
                   "harvest/stock shortage did not cap supply");
        });
        p[9].push_back([&](auto& w){
            verify(w.food_grievance("R")==5,"material shortage not exposed to political model");
            w.organize_from_food_grievance(f.civ,"Food Movement",5);
            w.protest(f.civ,"Food Movement",5);
        });
        f.w.advance(p);
        verify(f.w.group_members(f.civ,"Food Movement")==5 &&
               f.w.protest_participants(f.civ)==5 &&
               f.w.market("R","food").price>4 &&
               f.w.people().population()==600,
               "shortage/price/political organization not causally linked");
    });
    test.run(36, []{
        F f;f.w.seed_stock("F","R","ore",30);
        IntegratedWorld::Plan disabled;
        disabled[9].push_back([](auto& w){w.enact_conscription(false);});
        f.w.advance(disabled);
        IntegratedWorld::Plan peacetime;
        peacetime[2].push_back([&](auto& w){w.hire(f.reserve,10,"F",1);});
        peacetime[4].push_back([](auto& w){w.produce("F","R","ore","tool",10,10);});
        peacetime[9].push_back([](auto& w){w.enact_conscription(true);});
        f.w.advance(peacetime);
        verify(!f.w.conscription_enabled()&&f.w.people().employed(f.reserve)==10,
               "early draft law wrongly changed labor");
        IntegratedWorld::Plan wartime;
        wartime[4].push_back([](auto& w){w.produce("F","R","ore","tool",10,10);});
        wartime[10].push_back([&](auto& w){w.mobilize(f.reserve,8,"U");});
        f.w.advance(wartime);
        verify(f.w.conscription_enabled()&&f.w.people().employed(f.reserve)==2 &&
               f.w.active_strength("U")==108 && f.w.stock("F","R","tool")==20,
               "late mobilization did not contract next-month workforce");
        IntegratedWorld::Plan next;
        next[4].push_back([](auto& w){w.produce("F","R","ore","tool",10,10);});
        verify(rejected([&]{f.w.advance(next);}),"industry used mobilized workers");
        verify(f.w.stock("F","R","ore")==10&&f.w.commit_id()==3,
               "workforce shortage did not roll back unfinished production");
    });
    test.run(37, []{
        F f;f.w.seed_interest_group("Debt Reform");
        IntegratedWorld::Plan crisis;
        crisis[1].push_back([](auto& w){w.lend("F",100);});
        crisis[8].push_back([](auto& w){w.forgive_defaulted_loan("F",60);});
        crisis[9].push_back([&](auto& w){
            verify(w.bank_insolvent() && w.bank_losses()==60,
                   "bank insolvency not recognized");
            w.join_interest_group(f.civ,"Debt Reform",30);
            w.protest(f.civ,"Debt Reform",10);
        });
        f.w.advance(crisis);
        verify(f.w.financial().balance("F:debt")==-40 &&
               f.w.financial().balance("BANK:loan")==40 &&
               f.w.financial().balance("BANK:loan_loss")==60 &&
               f.w.group_members(f.civ,"Debt Reform")==30 &&
               f.w.protest_participants(f.civ)==10,
               "credit loss or financial-stress organization mismatch");
        IntegratedWorld::Plan next;
        next[1].push_back([](auto& w){w.lend("F",1);});
        verify(rejected([&]{f.w.advance(next);}),"insolvent bank extended unsupported credit");
    });
    test.run(38, []{
        F f;IntegratedWorld::Plan legislation;
        legislation[9].push_back([](auto& w){
            w.enact_income_tax(200'000);
            w.enact_conscription(false);
        });
        f.w.advance(legislation);
        verify(f.w.income_tax_ppm()==100'000&&f.w.conscription_enabled(),
               "law effective before next month");
        IntegratedWorld::Plan p;
        p[5].push_back([](auto& w){w.wages_under_law("F","H","G",100);});
        p[7].push_back([](auto& w){w.pay("G","S",10);});
        f.w.advance(p);
        verify(f.w.income_tax_ppm()==200'000&&!f.w.conscription_enabled()&&
               f.w.cash("G")==10&&f.w.cash("H")==130&&f.w.cash("S")==10,
               "tax law, payroll and procurement");
        IntegratedWorld::Plan forbidden;
        forbidden[10].push_back([&](auto& w){w.mobilize(f.reserve,1,"U");});
        verify(rejected([&]{f.w.advance(forbidden);}),"conscription without law accepted");
    });
    test.run(39, []{
        F f;f.w.seed_interest_group("Union");
        f.w.seed_stock("F","R","ore",10);
        IntegratedWorld::Plan first;
        first[2].push_back([&](auto& w){w.hire(f.civ,10,"F",1);});
        first[9].push_back([&](auto& w){
            w.join_interest_group(f.civ,"Union",10);
            w.protest(f.civ,"Union",7,5);
        });
        f.w.advance(first);
        IntegratedWorld::Plan overwork;
        overwork[4].push_back([](auto& w){w.produce("F","R","ore","tool",1,6);});
        verify(rejected([&]{f.w.advance(overwork);}),"strikers used as productive labor");
        verify(f.w.stock("F","R","ore")==10&&f.w.commit_id()==1,"failed strike month leaked");
        IntegratedWorld::Plan lawful;
        lawful[4].push_back([](auto& w){w.produce("F","R","ore","tool",1,5);});
        f.w.advance(lawful);
        verify(f.w.stock("F","R","tool")==1&&
               f.w.people().employed(f.civ)==10,
               "strike should reduce hours but not automatically terminate jobs");
    });
    test.run(40, []{
        F f;
        f.w.seed_stock("F","R","building_materials",4);
        f.w.seed_production_facility("factory","F","R",10);
        IntegratedWorld::Plan treaty;
        treaty[2].push_back([&](auto& w){w.hire(f.civ,4,"F",1);});
        treaty[9].push_back([](auto& w){w.sign_reparation_treaty("peace-1","F","H",100);});
        treaty[10].push_back([](auto& w){w.war_damage_facility("factory",4);});
        f.w.advance(treaty);
        verify(f.w.reparations_outstanding("peace-1")==100&&
               f.w.facility("factory").damaged==4&&
               f.w.cash("H")==50,"peace generated cash or instant restoration");
        IntegratedWorld::Plan recovery;
        recovery[4].push_back([](auto& w){
            w.rebuild_facility("factory","building_materials",4,4);
        });
        recovery[5].push_back([](auto& w){w.pay_reparation("peace-1",20);});
        recovery[10].push_back([&](auto& w){w.demobilize("U",f.active,10);});
        f.w.advance(recovery);
        verify(f.w.cash("H")==70&&f.w.reparations_outstanding("peace-1")==80 &&
               f.w.financial().balance("F:reparations_payable")==-80 &&
               f.w.financial().balance("H:reparations_receivable")==80 &&
               f.w.facility("factory").damaged==4 &&
               f.w.active_strength("U")==90 && f.w.people().population()==600 &&
               f.w.stock("F","R","building_materials")==0,
               "unpaid treaty or demobilization/rebuilding accounting incorrect");
        f.w.advance();
        verify(f.w.facility("factory").damaged==0,
               "material-backed reconstruction did not become operational next month");
    });
    test.run(41, []{
        F f;IntegratedWorld::Plan p;
        p[5].push_back([](auto& w){w.pay("F","H",100);});
        p[11].push_back([](auto&){throw std::runtime_error("injected failure");});
        verify(rejected([&]{f.w.advance(p);}),"missing stage11 rollback");
        verify(f.w.cash("F")==1000 && f.w.month()==24000&&f.w.commit_id()==0,"world partially committed");
    });
    test.run(42, []{
        F f;IntegratedWorld::Plan p;
        p[2].push_back([](auto& w){w.pay("F","H",10);});
        verify(rejected([&]{f.w.advance(p);}),"wrong stage accepted");
    });
    test.run(43, []{
        F f;IntegratedWorld::Plan first;
        first[5].push_back([](auto& w){
            w.command_once("receipt-007",[](auto& x){x.pay("F","H",10);});
        });
        f.w.advance(first);
        verify(f.w.cash("H")==60,"first command not executed");
        IntegratedWorld::Plan again;
        again[5].push_back([](auto& w){
            w.command_once("receipt-007",[](auto& x){x.pay("F","H",10);});
        });
        verify(rejected([&]{f.w.advance(again);}),"command replay accepted");
        verify(f.w.cash("H")==60&&f.w.commit_id()==1,
               "cross-month replay or partial commit");
    });
    test.run(44, []{
        F a,b;
        IntegratedWorld::Plan p;
        p[2].push_back([&](auto& w){w.hire(a.civ,20,"F",10);});
        p[5].push_back([](auto& w){w.wages("F","H","G",100,10);});
        a.w.advance(p);b.w.advance(p);
        verify(a.w.people().fingerprint()==b.w.people().fingerprint() &&
               a.w.cash("F")==b.w.cash("F") &&
               a.w.cash("G")==b.w.cash("G"),"repeat execution drift");
    });
    test.run(45, []{
        F f;
        f.w.seed_government("A","Head","Premier",3,2);
        f.w.seed_shares("capital","F",10);
        f.w.seed_forex_balance("F","USD",20);
        f.w.seed_interest_group("Guild");
        f.w.seed_market("R","ammunition",1,1,10);
        IntegratedWorld::Plan p;
        p[5].push_back([](auto& w){
            w.command_once("persisted-command",[](auto& st){st.pay("F","H",10);});
        });
        p[9].push_back([&](auto& w){
            w.hold_election(250,0,3,{{"A",150},{"B",100}});
            w.join_interest_group(f.civ,"Guild",10);
            w.appoint_cabinet("A","Head","NextPM",{"A"});
        });
        p[10].push_back([&](auto& w){
            w.battle("U",f.active,"R",CombatLoss{1,0,0,0,0,1});
        });
        f.w.advance(p);
        namespace fs=std::filesystem;
        const auto path=fs::temp_directory_path()/"vic25-d4-snapshot-test.dat";
        const auto corrupted=fs::temp_directory_path()/"vic25-d4-corrupt-test.dat";
        fs::remove(path);fs::remove(corrupted);
        SnapshotCodec::save(f.w,path);
        auto restored=SnapshotCodec::load(path);
        verify(restored.people().fingerprint()==f.w.people().fingerprint() &&
               restored.cash("F")==f.w.cash("F") &&
               restored.cash("H")==f.w.cash("H") &&
               restored.stock("U","R","ammunition")==f.w.stock("U","R","ammunition") &&
               restored.active_strength("U")==f.w.active_strength("U") &&
               restored.election().valid==f.w.election().valid &&
               restored.shares("capital","F")==10 &&
               restored.fx_balance("F","USD")==20 &&
               restored.group_members(f.civ,"Guild")==10 &&
               restored.journal()==f.w.journal() &&
               restored.commit_id()==f.w.commit_id(),
               "serialized world lost cross-domain state");
        const auto older=f.w;
        f.w.advance();restored.advance();
        verify(restored.government("A").prime_minister=="NextPM" &&
               restored.people().fingerprint()==f.w.people().fingerprint() &&
               restored.journal()==f.w.journal() &&
               restored.cash("F")==f.w.cash("F"),
               "saved world did not resume deterministic legal events");
        IntegratedWorld::Plan replay;
        replay[5].push_back([](auto& w){
            w.command_once("persisted-command",[](auto& st){st.pay("F","H",10);});
        });
        verify(rejected([&]{restored.advance(replay);}),
               "snapshot lost idempotent command cursor");
        {
            std::ifstream in(path,std::ios::binary);
            std::string bytes((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
            bytes.at(bytes.size()/2)^=1;
            std::ofstream out(corrupted,std::ios::binary);
            out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));
        }
        verify(rejected([&]{(void)SnapshotCodec::load(corrupted);}),
               "corrupted checkpoint was loaded");
        verify(rejected([&]{SnapshotCodec::save(older,path);}),
               "committed snapshot overwritten without versioned replacement");
        fs::remove(path);fs::remove(corrupted);
    });
    test.run(46, []{
        CrisisScenario a,b;
        perform(a);
        perform(b);
        verify(a.w.people().fingerprint()==b.w.people().fingerprint() &&
               a.w.journal()==b.w.journal() &&
               a.w.commit_id()==1200 && b.w.commit_id()==1200 &&
               a.w.cash("F")==b.w.cash("F") &&
               a.w.active_strength("U")==b.w.active_strength("U"),
               "D4-T46 1200-month integrated crisis not deterministic");
    });
    test.run(47, []{
        const auto small=vic25::phasec::acceptance::integrated_scale_benchmark(100000,8);
        const auto large=vic25::phasec::acceptance::integrated_scale_benchmark(1000000,8);
        verify(small.cells==100000 && large.cells==1000000 &&
               small.months==8 && large.months==8 &&
               small.events>=8000 && large.events>=8000 &&
               small.median_ms>0 && large.p95_ms>0,
               "D4-T47 representative multi-market/firm/army performance benchmark invalid");
    });
    test.run(48, [&]{
        verify(test.pass==47 && test.blocked==0 && test.failed==0,
               "D4-T48 cannot pass unless all preceding 47 D4 cases passed");
        const auto expected=std::array<std::string,9>{{
            "pop-design/1.1", "phase-c-common/1.0",
            "phase-c-economy/1.0", "phase-c-politics/1.0",
            "phase-c-military/1.0", "phase-c-integration/1.0",
            "checked-int64/1", "integer-lot/1",
            "vertical-slice-ruleset/1"
        }};
        const auto tokens=SnapshotCodec::contract_header();
        for(std::size_t i=0;i<expected.size();++i)
            verify(expected[i]==tokens[i],"runtime checkpoint contract version mismatch");
#ifndef VIC25_SOURCE_ROOT
#error "D4-T48 requires the configured repository source root"
#endif
        const auto root=std::filesystem::path{VIC25_SOURCE_ROOT};
        const auto pinned=root/"docs/phase-c/integration/contract_baseline.json";
        const auto politics=root/"docs/phase-c/politics/C-P1_STATE_GOVERNMENT_INSTITUTIONS.md";
        const auto political_audit=root/"docs/phase-c/politics/POLITICAL_ACCEPTANCE_TESTS.md";
        verify(std::filesystem::is_regular_file(pinned)&&
               std::filesystem::is_regular_file(politics)&&
               std::filesystem::is_regular_file(political_audit),
               "registered P1–P5 contract or baseline is missing");
        auto read=[](const auto& p)->std::string{
            std::ifstream in(p,std::ios::binary);
            verify(static_cast<bool>(in),"contract file cannot be read");
            return std::string{std::istreambuf_iterator<char>(in),
                               std::istreambuf_iterator<char>()};
        };
        verify(read(pinned).find("d4-contract-baseline/1.0")!=std::string::npos &&
               read(politics).find("phase-c-politics/1.0")!=std::string::npos &&
               read(political_audit).find("CP-T25")!=std::string::npos,
               "contract text/registry verification failed");
        F f;
        verify(f.w.people().population()==600 &&
               f.w.financial().balanced() &&
               f.w.commit_id()==0,
               "Phase B/S0/Phase C shared population-finance baseline invalid");
        // Keep the checksum mathematically valid while altering a contract
        // token; the loader must reject this *because of version mismatch*.
        const auto save_path=std::filesystem::temp_directory_path()/"vic25-d4-t48-v2.dat";
        const auto wrong_path=std::filesystem::temp_directory_path()/"vic25-d4-t48-wrong-version.dat";
        std::filesystem::remove(save_path);
        std::filesystem::remove(wrong_path);
        SnapshotCodec::save(f.w,save_path);
        auto save_bytes=read(save_path);
        const std::string needle="phase-c-politics/1.0";
        const auto where=save_bytes.find(needle);
        verify(where!=std::string::npos && save_bytes.size()>8,
               "required political contract token not saved");
        save_bytes[where+needle.size()-1]='1';
        const auto hash=persistence_detail::checksum(
            save_bytes.substr(0,save_bytes.size()-8));
        for(std::size_t i=0;i<8;++i)
            save_bytes[save_bytes.size()-8+i]=
                static_cast<char>((hash>>(i*8))&0xffU);
        {
            std::ofstream out(wrong_path,std::ios::binary|std::ios::trunc);
            verify(static_cast<bool>(out),"cannot create version mismatch fixture");
            out.write(save_bytes.data(),static_cast<std::streamsize>(save_bytes.size()));
            verify(static_cast<bool>(out),"could not write version mismatch fixture");
        }
        verify(rejected([&]{(void)SnapshotCodec::load(wrong_path);}),
               "checkpoint accepted altered contract token with valid checksum");
        std::filesystem::remove(save_path);
        std::filesystem::remove(wrong_path);
        // The *independent* Python CTest verifies exact Git blob IDs, ADR/INV
        // numbering and fixture/version contents. This bounded acceptance
        // test does NOT certify the complete CE/CP/CM feature backlog.
    });

    for (const auto& s : test.status) std::cout << s << '\n';
    std::cout << "D4 integration runnable subset: PASS=" << test.pass
              << " BLOCKED=" << test.blocked << " FAIL=" << test.failed
              << " TOTAL=" << test.status.size() << "/48\n";
    verify(test.status.size()==48,"missing test IDs");
    return test.failed ? 1 : 0;
}
