#include "vic25/phase_gh.hpp"
#include <iostream>
#include <stdexcept>
using namespace vic25::phasegh;
static void verify(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class F>static bool rejected(F&& f) {
    try{f();}catch(const std::exception&){return true;}return false;
}
static World fixture(bool route=false) {
    World w(100);
    auto& e=w.seed_geography();
    e.set_dataset("GH-diplomacy-fixture");
    e.seed_province(1,"RA1","A");
    e.seed_province(4,"RA4","A");
    e.seed_province(2,"RB2","B");
    e.seed_province(3,"RC3","C");
    e.seed_border(1,2);e.seed_border(2,3);e.seed_border(1,4);
    e.seed_unit(11,"A","A");e.seed_unit(22,"B","B");
    e.seed_unit(33,"C","C");
    e.seed_assignment(1,11);e.seed_assignment(4,11);
    e.seed_assignment(2,22);e.seed_assignment(3,33);
    auto& c=e.seed_core().seed_core();
    c.seed_actor("A",500);c.seed_actor("B",500);c.seed_actor("C",500);
    c.seed_actor("SELLER",100);c.seed_actor("BUYER",5000);
    c.seed_actor("ORG:X",0);
    c.seed_stock("SELLER","RA4","iron",8);
    c.seed_market("RA4","iron",100,1,200);
    c.seed_market("RB2","iron",160,1,240);
    c.seed_government("A","A-head","PM_A",5,3);
    c.seed_government("B","B-head","PM_B",5,3);
    c.seed_government("C","C-head","PM_C",5,3);
    w.seed_tariff("B","iron",100000);
    if(route)w.seed_route(Route{"route","SELLER","BUYER","iron",4,2,5,1});
    return w;
}
static TreatyClause clause(const std::string& id,ClauseKind kind,
                           const std::string& obligor,const std::string& beneficiary,
                           const std::string& good="",Money amount=0) {
    TreatyClause t;t.id=id;t.kind=kind;t.obligor=obligor;
    t.beneficiary=beneficiary;t.good=good;t.amount=amount;
    return t;
}
static void multilateral_treaty_and_trade_restrictions() {
    auto w=fixture(true);
    World::Plan p;
    p[9].push_back([](Stage& s) {
        auto a=clause("zero_tariff",ClauseKind::tariff_exemption,"A","B","iron");
        auto b=clause("volume_limit",ClauseKind::trade_quota,"A","B","iron",3);
        auto c=clause("aid100",ClauseKind::financial_aid,"A","B","",100);
        auto d=clause("mutual_defense",ClauseKind::defense_guarantee,"B","C");
        auto e=clause("research",ClauseKind::research_cooperation,"A","C");
        s.propose_treaty_package("T3",{"A","B","C"},{a,b,c,d,e},"A","PM_A");
        s.ratify_package_member("T3","B","PM_B");
        s.ratify_package_member("T3","C","PM_C");
    });
    w.advance(p);
    verify(w.commerce().fills.size()==1 &&
           w.commerce().fills[0].quantity==5&&
           w.commerce().fills[0].duty_paid==50,
           "new treaty must not apply retroactively at m100");
    verify(!w.diplomacy().treaties.at("T3").active,
           "signatures without stage0 effect");
    World::Plan m101;
    m101[5].push_back([](Stage& s){
        s.disburse_treaty_aid("T3","aid100",100);
    });
    w.advance(m101);
    verify(w.diplomacy().treaties.at("T3").active,
           "all three real ratifications effective m101");
    verify(w.commerce().fills.size()==2 &&
           w.commerce().fills[1].quantity==3 &&
           w.commerce().fills[1].duty_paid==0,
           "actual quota limits second dispatch and tariff exemption applies");
    verify(w.base().core().core().cash("A")==400 &&
           w.base().core().core().cash("B")==650,
           "real C financial flow of aid, prior customs remains untouched");
    verify(w.diplomacy().treaties.at("T3").clauses.at(2).fulfilled==100,
           "aid legal obligation and actual settlement match");
    verify(w.base().core().core().military_control("RB2")=="B",
           "defensive clause alone creates no war or occupation");
    w.validate();
}
static void territory_and_reparations_are_E_and_C_events() {
    auto w=fixture();
    World::Plan m100;
    m100[9].push_back([](Stage& s) {
        auto c=clause("cede_P1",ClauseKind::territorial_cession,"A","B");
        c.province=1;c.receiving_admin=22;
        auto r=clause("repay",ClauseKind::reparations,"A","B","",50);
        s.propose_treaty_package("PEACE",{"A","B"},{c,r},"A","PM_A");
        s.ratify_package_member("PEACE","B","PM_B");
    });
    w.advance(m100);
    verify(w.base().sovereign(1)=="A","stage9 may not move current sovereignty");
    World::Plan m101;
    m101[5].push_back([](Stage& s) {
        s.pay_reparation("H:REPARATION:PEACE:repay",20);
    });
    w.advance(m101);
    verify(w.base().sovereign(1)=="B"&&w.base().leaf(1)==22,
           "E applies sovereignty/administrative transfer from ratified treaty");
    verify(w.base().military_control(1)=="A",
           "real E sovereignty transfer must not silently update military control");
    verify(w.base().core().core().reparations_outstanding(
             "H:REPARATION:PEACE:repay")==30,
           "C reparation payable actual 50 minus real 20");
    verify(w.base().core().core().cash("A")==480 &&
           w.base().core().core().cash("B")==520,
           "C financial payment uses bank deposits, not phantom H cash");
    w.validate();
}
static void organization_voting_veto_membership_change() {
    auto w=fixture();
    World::Plan m100;
    m100[9].push_back([](Stage& s){
        s.create_organization("X",{"A","B"},0,
                               {{"A","PM_A"},{"B","PM_B"}});
        OrganizationRule r;r.quorum=2;r.yes_required=2;r.veto_members={"B"};
        s.configure_organization("X",r,"PM_A");
    });
    w.advance(m100);
    World::Plan m101;
    m101[9].push_back([](Stage& s){
        s.hold_organization_vote("X","ACCEPT_C","admit:C",
           {{"A",VoteChoice::yes},{"B",VoteChoice::yes}},"A","PM_A");
        s.admit_organization_member("X","C","ACCEPT_C","PM_C");
    });
    w.advance(m101);
    verify(!w.diplomacy().organizations.at("X").members.contains("C"),
           "membership admission next-month effect");
    World::Plan m102;
    m102[9].push_back([](Stage& s){
        s.hold_organization_vote("X","DECLINED","military-action",
           {{"A",VoteChoice::yes},{"B",VoteChoice::no},
            {"C",VoteChoice::yes}},"A","PM_A");
        s.leave_organization("X","B","PM_B");
    });
    w.advance(m102);
    verify(w.diplomacy().organizations.at("X").members.contains("C") &&
           !w.diplomacy().organizations.at("X").decisions.at("DECLINED").adopted,
           "actual veto overrides 2:1 and admission effective");
    w.advance();
    const auto& org=w.diplomacy().organizations.at("X");
    verify(org.members.size()==2&&org.members.contains("A")&&
           org.members.contains("C")&&!org.members.contains("B"),
           "exit executed without recreating organization or country");
    verify(w.base().core().core().cash("ORG:X")==0,
           "organization vote creates no money");
}
static void four_dimensional_influence_from_real_events() {
    auto w=fixture();
    w.seed_geography().seed_core().seed_core().seed_stock("A","RA1","arms",5);
    World::Plan m100;
    m100[5].push_back([](Stage& s){
        s.fund_cultural_exchange("SCHOLAR_A_B","A","B",20,"PM_A");
    });
    m100[7].push_back([](Stage& s){
        s.dispatch_military_assistance("ARMS_3","A","B","arms",
                                        1,2,3,50,1,"PM_A");
    });
    w.advance(m100);
    auto initial=w.assess_influence("A","B");
    verify(initial.cultural_known&&initial.cultural_funding==20 &&
           !initial.military_known,"in-transit arms not delivered at m100");
    verify(w.base().core().core().stock("A","RA1","arms")==2,
           "actual military supply leaves real C inventory");
    w.advance();
    const auto v=w.assess_influence("A","B");
    verify(v.military_known&&v.military_goods_delivered==3 &&
           v.military_material_value==150&&v.cultural_funding==20,
           "confirmed delivery and funded academic exchange appear in separate dimensions");
    verify(w.base().core().core().stock("B","RB2","arms")==3,
           "military aid did not create any goods");
    verify(!w.assess_influence("B","A").military_known,
           "influence direction asymmetric");
}
static void crisis_partial_offers_and_compensation() {
    auto w=fixture();
    World::Plan m100;
    m100[9].push_back([](Stage& s){
        s.geography().queue_claim("ECLAIM",2,"A",true);
    });
    w.advance(m100);
    World::Plan m101;
    m101[9].push_back([](Stage& s){
        s.open_crisis("CR",2,"A","B","PM_A");
    });
    w.advance(m101);
    verify(w.base().sovereign(2)=="B" &&
           w.diplomacy().crises.at("CR").status==CrisisStatus::scheduled,
           "claim generated a crisis, not war");
    World::Plan m102;
    m102[9].push_back([](Stage& s){
        s.mediate_crisis("CR","C","PM_C");
        s.submit_crisis_position("CR","A","supports_claim",false,"PM_A");
        s.submit_crisis_position("CR","B","conditional_compromise",true,"PM_B");
        CrisisProposal p;
        p.id="O";p.proposer="C";p.compensation=100;
        p.payer="B";p.recipient="A";p.freeze_claim=true;
        s.offer_crisis_compromise("CR",p,"PM_C");
        s.respond_crisis_offer("CR","O","A",true,"PM_A");
        s.respond_crisis_offer("CR","O","B",true,"PM_B");
    });
    w.advance(m102);
    verify(w.diplomacy().crises.at("CR").status==CrisisStatus::settled &&
           w.base().sovereign(2)=="B","accepted mediation does not transfer sovereignty");
    World::Plan m103;
    m103[5].push_back([](Stage& s){
        s.pay_reparation("H:CRISIS-REPARATION:CR:O",20);
    });
    w.advance(m103);
    verify(w.base().spatial().claims.at(2).empty() &&
           w.base().core().core().reparations_outstanding(
             "H:CRISIS-REPARATION:CR:O")==80,
           "E claimant withdrawal and C installment settled once");
    verify(!w.advise_crisis("CR","C").can_mediate,
           "AI read-only assessment of closed crisis");
}
int main(){
    try{
        multilateral_treaty_and_trade_restrictions();
        territory_and_reparations_are_E_and_C_events();
        organization_voting_veto_membership_change();
        four_dimensional_influence_from_real_events();
        crisis_partial_offers_and_compensation();
        std::cout<<"Phase H extended diplomacy: 5 integration scenarios PASS\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"Phase H extended diplomacy FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
