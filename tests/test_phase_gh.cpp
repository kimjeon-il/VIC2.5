#include "vic25/phase_gh.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
using vic25::phasegh::World;
using vic25::phasegh::Stage;
using vic25::phasegh::Route;

static void check(bool ok, const std::string& reason) {
    if(!ok)throw std::runtime_error(reason);
}
template<class F> static bool rejected(F&& f) {
    try{f();}catch(const std::exception&){return true;}
    return false;
}
static World make_world(bool routes=true) {
    World w(100);
    auto& e=w.seed_geography();
    e.set_dataset("phase-gh-synthetic-three-country");
    e.seed_province(1,"RA","A");
    e.seed_province(2,"RB","B");
    e.seed_province(3,"RC","C");
    e.seed_border(1,2);
    e.seed_border(2,3);
    e.seed_unit(11,"A","A-ROOT");
    e.seed_unit(22,"B","B-ROOT");
    e.seed_unit(33,"C","C-ROOT");
    e.seed_assignment(1,11);
    e.seed_assignment(2,22);
    e.seed_assignment(3,33);
    auto& c=e.seed_core().seed_core();
    c.seed_actor("A",200);
    c.seed_actor("B",200);
    c.seed_actor("C",200);
    c.seed_actor("ORG:X",0);
    c.seed_actor("SELLER",0);
    c.seed_actor("BUYER",3000);
    c.seed_stock("SELLER","RA","iron",10);
    c.seed_market("RA","iron",100,1,200);
    c.seed_market("RB","iron",150,1,250);
    c.seed_government("A","headA","PM_A",5,3);
    c.seed_government("B","headB","PM_B",5,3);
    c.seed_government("C","headC","PM_C",5,3);
    w.seed_tariff("B","iron",100000);
    if(routes) w.seed_route(Route{"route1","SELLER","BUYER","iron",1,2,5,1});
    w.validate();
    return w;
}
static void tariffs_trade_and_diplomatic_influence() {
    auto w=make_world();
    World::Plan p;
    p[9].push_back([](Stage& h){
        h.propose_tariff_treaty("tariff1","A","B","iron","PM_A");
        h.ratify_treaty("tariff1","B","PM_B");
    });
    auto rep=w.advance(p);
    check(rep.starting_month==100 && rep.commit_id==1,"C/D/E month commit");
    const auto& c=w.base().core().core();
    check(c.cash("SELLER")==500,"real seller proceeds from C financial ledger");
    check(c.cash("BUYER")==2450,"trade cost + duty from real buyer wallet");
    check(c.cash("B")==250,"real import duty paid to actual country actor");
    check(c.stock("SELLER","RA","iron")==5,"real inventory sold");
    check(c.stock("BUYER","RB","iron")==0,"unarrived goods not immediately usable");
    check(c.stock("TRANSIT:G:route1:100","in_transit","iron")==5,
          "real goods tracked in C in-transit ledger");
    check(w.commerce().fills.size()==1&&w.commerce().fills[0].duty_paid==50,
          "G fill references C transfer and fiscal payment");
    check(!w.diplomacy().treaties.at("tariff1").active,
          "m100 signature not m100 tariff exemption");
    const auto influence=w.trade_influence("A","B");
    check(influence.observed && influence.bilateral_imports==500 &&
          influence.target_total_imports==500 &&
          influence.trade_dependency_ppm==1000000 &&
          !influence.military_known,"influence is read-only trade evidence");
    check(!w.trade_influence("B","A").observed,"A-to-B not B-to-A");
    w.advance();
    const auto& c2=w.base().core().core();
    check(w.diplomacy().treaties.at("tariff1").active,
          "ratified treaty effective at m101 stage0");
    check(c2.stock("BUYER","RB","iron")==5,
          "m100 shipment now available at m101");
    check(c2.stock("SELLER","RA","iron")==0 &&
          c2.stock("TRANSIT:G:route1:101","in_transit","iron")==5,
          "next real merchant trade uses remaining seller inventory");
    check(c2.cash("SELLER")==1000 && c2.cash("BUYER")==1950 &&
          c2.cash("B")==250,"m101 treaty exempts duty but not base price");
    check(w.commerce().fills.size()==2 &&
          w.commerce().fills[1].duty_paid==0,"no duplicate tariff collection");
    w.advance();
    check(w.base().core().core().stock("BUYER","RB","iron")==10,
          "second shipment arrives without goods creation");
    check(w.commerce().fills.size()==2,"no phantom stock when seller empty");
}
static void international_organization_and_crisis() {
    auto w=make_world(false);
    World::Plan first;
    first[9].push_back([](Stage& h){
        h.create_organization("X",{"A","B","C"},100,
                              {{"A","PM_A"},{"B","PM_B"},{"C","PM_C"}});
        h.geography().queue_claim("E:claim-A-on-P2",2,"A",true);
    });
    w.advance(first);
    check(!w.diplomacy().organizations.at("X").active,"org only active next month");
    check(!w.base().spatial().claims.contains(2),"claim only effective next month");
    World::Plan second;
    second[5].push_back([](Stage& h){
        h.pay_dues("X","A",40);
        h.pay_dues("X","B",40);
        h.pay_dues("X","C",40);
    });
    second[9].push_back([](Stage& h){
        h.open_crisis("crisis-P2",2,"A","B","PM_A");
    });
    w.advance(second);
    check(w.diplomacy().organizations.at("X").active,"charter effective");
    check(w.base().spatial().claims.at(2).contains("A"),"E real claim");
    check(w.base().core().core().cash("ORG:X")==120,"C actual organization dues");
    for(const auto& member:{"A","B","C"})
        check(w.diplomacy().organizations.at("X").paid.at({101,member})==40,
              "H contributions reflect C actual payouts");
    check(w.diplomacy().crises.at("crisis-P2").status ==
          vic25::phasegh::CrisisStatus::scheduled,"crisis starts next month");
    World::Plan third;
    third[9].push_back([](Stage& h){
        h.mediate_crisis("crisis-P2","C","PM_C");
        h.resolve_crisis_without_war("crisis-P2","PM_A","PM_B");
    });
    w.advance(third);
    check(w.diplomacy().crises.at("crisis-P2").status ==
          vic25::phasegh::CrisisStatus::settled,"multilateral peaceful settlement");
    check(w.base().sovereign(2)=="B" &&
          w.base().military_control(2)=="B",
          "crisis not a sovereignty transfer or military event");
    check(w.base().core().core().cash("ORG:X")==120,
          "no imaginary recurring organization funding");
}
static void cross_domain_atomic_rollback() {
    auto w=make_world();
    const auto before_stock=w.base().core().core().stock("SELLER","RA","iron");
    const auto before_cash=w.base().core().core().cash("BUYER");
    World::Plan p;
    p[9].push_back([](Stage& h){
        h.propose_tariff_treaty("T","A","B","iron","PM_A");
        h.ratify_treaty("T","B","PM_B");
    });
    p[11].push_back([](Stage&){
        throw std::runtime_error("injected post-trade rollback failure");
    });
    check(rejected([&]{w.advance(p);}),"injected fault must abort");
    check(w.commit_id()==0 && w.month()==100,"C/D/E time and commit rolled back");
    check(w.base().core().core().cash("BUYER")==before_cash &&
          w.base().core().core().stock("SELLER","RA","iron")==before_stock,
          "C money and goods rolled back");
    check(w.commerce().fills.empty() && w.diplomacy().treaties.empty(),
          "G/H sidecars rolled back with C/D/E");
    w.advance();
    check(w.commit_id()==1 && w.commerce().fills.size()==1,
          "world can advance deterministically after failure");
}
static void rejected_authority_and_zero_resources() {
    auto w=make_world(false);
    World::Plan p;
    p[9].push_back([](Stage& h){
        h.propose_tariff_treaty("bad","A","B","iron","not_prime");
    });
    check(rejected([&]{w.advance(p);}),"unauthorized government treaty rejected");
    check(w.commit_id()==0 && w.diplomacy().treaties.empty(),
          "no legal side effect from invalid treaty");
    World::Plan q;
    q[9].push_back([](Stage& h) {
        h.open_crisis("fake",2,"A","B","PM_A");
    });
    check(rejected([&]{w.advance(q);}),"dispute missing E Claim rejected");
    check(w.commit_id()==0 && w.diplomacy().crises.empty(),
          "no automatic crisis without real claim");
}
int main() {
    try {
        tariffs_trade_and_diplomatic_influence();
        international_organization_and_crisis();
        cross_domain_atomic_rollback();
        rejected_authority_and_zero_resources();
        std::cout << "Phase G/H bounded C/D/E integration: 4 scenario groups passed\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr << "Phase G/H bounded integration FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
