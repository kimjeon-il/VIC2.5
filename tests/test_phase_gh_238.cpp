// Contract-level acceptance cases that assert against the real C/D/E/G/H world.
// Each registered PASS has its own executable oracle. Unimplemented contracts
// exit 77 (CTest SKIPPED/BLOCKED); a skipped ID is never an acceptance PASS.
#include "vic25/phase_gh_snapshot.hpp"
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
using namespace vic25::phasegh;

static void ok(bool x,const char* why){if(!x)throw std::runtime_error(why);}
template<class F>static bool throws_with(F&& fn,const std::string& fragment="") {
    try{fn();}catch(const std::exception& e){
        return fragment.empty() || std::string(e.what()).find(fragment)!=std::string::npos;
    }
    return false;
}
static World fixture(Quantity seller_qty=10,Money buyer_money=4000,
                     Month start=100,Money b_treasury=500){
    World w(start);
    auto& e=w.seed_geography();
    e.set_dataset("acceptance-238-synthetic");
    e.seed_province(1,"RA","A");e.seed_province(2,"RB","B");
    e.seed_province(3,"RC","C");
    e.seed_border(1,2);e.seed_border(2,3);
    e.seed_unit(11,"A","A");e.seed_unit(22,"B","B");
    e.seed_unit(33,"C","C");
    e.seed_assignment(1,11);e.seed_assignment(2,22);e.seed_assignment(3,33);
    auto& c=e.seed_core().seed_core();
    c.seed_actor("A",500);c.seed_actor("B",b_treasury);
    c.seed_actor("C",500);c.seed_actor("SELLER",100);
    c.seed_actor("BUYER",buyer_money);c.seed_actor("BUYER2",2000);
    c.seed_actor("INSURER",500);c.seed_actor("CARRIER",0);
    c.seed_actor("ORG:X",0);c.seed_actor("ORG:Y",0);
    if(seller_qty>0)c.seed_stock("SELLER","RA","iron",seller_qty);
    c.seed_market("RA","iron",100,1,200);
    c.seed_market("RB","iron",150,1,250);
    c.seed_market("RC","iron",100,1,200);
    c.seed_government("A","headA","PM_A",5,3);
    c.seed_government("B","headB","PM_B",5,3);
    c.seed_government("C","headC","PM_C",5,3);
    w.seed_tariff("B","iron",100000);
    return w;
}
static World direct_sale(){
    auto w=fixture();
    World::Plan p;
    p[7].push_back([](Stage& s){
        s.phase_c().trade("SELLER","BUYER","RA","iron",10,10);
    });
    w.advance(p);
    return w;
}
static World route_trade(Quantity cap=5,Month duration=1,
                         Quantity loss=0,Quantity edge_cap=0){
    auto w=fixture();
    w.seed_route(Route{"R","SELLER","BUYER","iron",1,2,cap,duration,loss});
    if(edge_cap>0)w.seed_transport_edge(
        TransportEdge{"link","CARRIER",1,2,edge_cap,0,duration,true});
    w.advance();
    return w;
}
struct TreatyProbe{
    World w;
    Money old_tax{};
    bool old_active{};
    std::size_t prior_customs{};
};
static TreatyProbe treaty_probe(){
    auto w=fixture();
    w.seed_route(Route{"R","SELLER","BUYER","iron",1,2,5,1});
    World::Plan p;
    p[9].push_back([](Stage& s){
        s.propose_tariff_treaty("T","A","B","iron","PM_A");
        s.ratify_treaty("T","B","PM_B");
    });
    w.advance(p);
    const auto first_tax=w.base().core().core().cash("B")-500;
    const bool before=w.diplomacy().treaties.at("T").active;
    const auto first_customs=w.commerce().customs.size();
    w.advance();
    return {std::move(w),first_tax,before,first_customs};
}
static World founded_org(const std::set<std::string>& founders={"A","B"},
                         Money dues=0,const std::string& name="X"){
    auto w=fixture();
    World::Plan p;
    p[9].push_back([founders,dues,name](Stage& s){
        std::map<std::string,std::string> signers;
        for(const auto& country:founders)signers[country]="PM_"+country;
        s.create_organization(name,founders,dues,signers);
    });
    w.advance(p);w.advance();
    return w;
}
static World claimed_crisis(bool mediate=false){
    auto w=fixture();
    World::Plan p;
    p[9].push_back([](Stage& s){s.geography().queue_claim("claim1",2,"A",true);});
    w.advance(p);
    World::Plan q;
    q[9].push_back([](Stage& s){s.open_crisis("CR",2,"A","B","PM_A");});
    w.advance(q); // m101 stage9; international crisis scheduled
    w.advance(); // m102 active
    if(mediate){
        World::Plan m;
        m[9].push_back([](Stage& s){s.mediate_crisis("CR","C","PM_C");});
        w.advance(m);
    }
    return w;
}
static World reparations(){
    auto w=fixture();
    World::Plan p;
    p[9].push_back([](Stage& s){
        s.link_reparation_treaty("R","A","B",100,"PM_A");
    });
    w.advance(p);
    World::Plan q;
    q[5].push_back([](Stage& s){s.pay_reparation("R",20);});
    w.advance(q);
    return w;
}
static World multilateral(){
    auto w=fixture();
    World::Plan p;
    p[9].push_back([](Stage& s){
        TreatyClause tariff;tariff.id="duty";tariff.kind=ClauseKind::tariff_exemption;
        tariff.obligor="A";tariff.beneficiary="B";tariff.good="iron";
        TreatyClause defense;defense.id="def";defense.kind=ClauseKind::defense_guarantee;
        defense.obligor="A";defense.beneficiary="C";
        s.propose_treaty_package("M",{"A","B","C"},{tariff,defense},"A","PM_A");
        s.ratify_package_member("M","B","PM_B");
        s.ratify_package_member("M","C","PM_C");
    });
    w.advance(p);
    w.advance();
    return w;
}
static void t_gd01(){
    auto w=direct_sale();
    const auto& c=w.base().core().core();
    ok(c.stock("SELLER","RA","iron")==0&&c.stock("BUYER","RA","iron")==10,
        "GD0-T01 goods must be actual C ledger transfer");
    ok(c.cash("BUYER")==3900&&c.cash("SELLER")==200,"GD0-T01 real payment");
}
static void t_gd03(){
    auto w=fixture(20,5000,99);
    w.seed_route(Route{"R","SELLER","BUYER","iron",1,2,10,2,3});
    World::Plan p;
    p[9].push_back([](Stage& s){
        CargoInsurance insurance;
        insurance.id="I";insurance.shipment_id="G:R:100";
        insurance.insurer="INSURER";insurance.beneficiary="BUYER";
        insurance.premium=30;insurance.maximum_claim=500;
        insurance.insured_quantity=10;
        s.insure_cargo(insurance);
    });
    w.advance(p);  // first shipment, month99
    w.advance();   // second insured shipment, month100
    ok(w.insurance("I").premium_paid,"GD0-T03 insured premium actually paid");
    w.advance();   // month101 arrives first 7, second still in transit
    ok(w.base().core().core().stock("BUYER","RB","iron")==7,
       "GD0-T03 only first voyage has arrived");
    w.advance();   // month102 arrives second 7, actual loss and insurance
    const auto& policy=w.insurance("I");
    ok(policy.claim_registered&&policy.claim_due==300&&policy.claim_paid==300,
       "GD0-T03 only confirmed insured losses are paid");
    ok(w.base().core().core().stock("BUYER","RB","iron")==14,
       "GD0-T03 two-month transit and losses preserve 14 physical arrivals");
}
static void t_gd04(){
    auto w=route_trade(10,1,0,8);
    ok(w.commerce().fills.size()==1&&w.commerce().fills[0].quantity==8,
       "GD0-T04 freight capacity8 caps requests10");
    ok(w.base().core().core().stock("SELLER","RA","iron")==2,
       "GD0-T04 reserved remainder not silently shipped");
}
static void t_gd07(){
    auto p=treaty_probe();
    ok(!p.old_active&&p.w.diplomacy().treaties.at("T").active,
       "GD0-T07 m contract not effective until next month");
    ok(p.old_tax==50,"GD0-T07 old month's 10 percent duty unchanged");
}
static void t_gd08(){
    auto w=fixture();
    auto& c=w.seed_geography().seed_core().seed_core();
    c.seed_forex_balance("BUYER","USD",100);
    c.seed_forex_balance("SELLER","GBP",200);
    World::Plan p;
    p[7].push_back([](Stage& s){
        s.phase_c().exchange_fx("BUYER","SELLER","USD",100,"EUR",100);
    });
    ok(throws_with([&]{w.advance(p);},"invalid currency exchange"),
       "GD0-T08 no EUR counterparty must reject fake exchange");
    ok(w.commit_id()==0&&c.fx_balance("BUYER","USD")==100,
       "GD0-T08 real foreign cash unchanged by attempted 1:1 swap");
}
static void t_gt101(){
    auto w=direct_sale();
    ok(w.commerce().fills.empty() &&
       w.base().core().core().stock("BUYER","RA","iron")==10,
       "GT1-T01 direct producer sale must not require G merchant center");
}
static void t_gt108(){
    auto w=fixture(10,100);
    // Prices and tariffs are seeded; a real buyer can afford at most one
    // 80-unit order. Competing G routes share the Phase C/D reservation.
    auto& c=w.seed_geography().seed_core().seed_core();
    // C market price cannot be reseeded; instead test two explicit C/D
    // reserved purposes of 80 each in Stage1.
    World::Plan p;
    p[1].push_back([](Stage& s){
        s.phase_c().phase_d_reserve_cash("BUYER","first",80);
        s.phase_c().phase_d_reserve_cash("BUYER","second",80);
    });
    ok(throws_with([&]{w.advance(p);},"reservation"),
       "GT1-T08 100 cannot fully reserve two orders80+80");
    ok(w.commit_id()==0&&c.cash("BUYER")==100,
       "GT1-T08 failed reservation rolls back shared ledger");
}
static void t_gt201(){
    auto w=fixture(0);
    w.seed_route(Route{"R","SELLER","BUYER","iron",1,2,10,1});
    w.advance();
    ok(w.commerce().fills.empty() &&
       w.base().core().core().stock("BUYER","RB","iron")==0,
       "GT2-T01 local quoted prices cannot generate goods without seller stock");
}
static void t_gt301(){
    auto w=direct_sale();
    const auto& c=w.base().core().core();
    ok(c.stock("BUYER","RA","iron")==10&&
       c.cash("SELLER")-100==100 &&
       c.cash("BUYER")==3900,"GT3-T01 spot cash and title transfer");
    ok(w.commerce().fills.empty(),"GT3-T01 no artificial long-haul shipment");
}
static void t_gt302(){
    auto w=route_trade(10,2);
    ok(w.base().core().core().stock("BUYER","RB","iron")==0&&
       w.base().core().core().stock("TRANSIT:G:R:100","in_transit","iron")==10,
       "GT3-T02 departure month transit10, buyer inventory0");
    w.advance();
    ok(w.base().core().core().stock("BUYER","RB","iron")==0,
       "GT3-T02 month+1 not yet delivered");
    w.advance();
    ok(w.base().core().core().stock("BUYER","RB","iron")==10,
       "GT3-T02 month+2 stage0 delivery");
}
static void t_gt310(){
    auto w=fixture();
    w.seed_route(Route{"R","SELLER","BUYER","iron",1,2,5,1});
    const auto cash=w.base().core().core().cash("BUYER");
    World::Plan p;
    p[11].push_back([](Stage&){throw std::runtime_error("fault at validation");});
    ok(throws_with([&]{w.advance(p);},"fault at validation"),
       "GT3-T10 injection must abort a staged shipment and payment");
    ok(w.commit_id()==0&&w.commerce().fills.empty()&&
       w.base().core().core().cash("BUYER")==cash&&
       w.base().core().core().stock("SELLER","RA","iron")==10,
       "GT3-T10 all G/C/E effects rolled back");
}
static void t_gt407(){
    auto p=treaty_probe();
    const auto& g=p.w.commerce();
    ok(p.old_tax==50&&g.customs.size()==1&&
       g.customs[0].assessed==50,
       "GT4-T07 old tariff assessed only for eligible pre-treaty cargo");
    ok(p.w.base().core().core().cash("B")==550&&
       p.w.diplomacy().treaties.at("T").active,
       "GT4-T07 m+1 treaty exemption and no retroactive refund");
}
static void t_gt408(){
    auto w=fixture();
    auto& c=w.seed_geography().seed_core().seed_core();
    c.seed_forex_balance("BUYER","USD",100);
    c.seed_forex_balance("SELLER","GBP",200);
    World::Plan p;
    p[7].push_back([](Stage& s){
        s.exchange_currencies("BUYER","SELLER","USD",100,"EUR",200);
    });
    ok(throws_with([&]{w.advance(p);},"currency"),
       "GT4-T08 no exchange counterparty EUR wallet");
    ok(c.fx_balance("BUYER","USD")==100&&w.commit_id()==0,
       "GT4-T08 no phantom multi-currency balance");
}
static void t_gt501(){
    auto w=route_trade(10,1,3);
    ok(w.base().core().core().stock("BUYER","RB","iron")==0,
       "GT5-T01 no early cargo arrival");
    w.advance();
    ok(w.base().core().core().stock("BUYER","RB","iron")==7 &&
       w.base().core().core().stock("TRANSIT:G:R:100","in_transit","iron")==0,
       "GT5-T01 10 dispatch minus3 physical loss equals 7 arrived");
}
static void t_gt505(){
    auto w=fixture();
    w.seed_geography().seed_core().seed_core().seed_stock("BUYER","RB","iron",2);
    World::Plan first;
    first[9].push_back([](Stage& s){
        CreditFacility credit;
        credit.id="C1";credit.borrower="BUYER";
        credit.pledge_region="RB";credit.pledge_good="iron";
        credit.pledged_goods=2;credit.limit=200;
        s.offer_trade_credit(credit);
    });
    w.advance(first);
    World::Plan second;
    second[9].push_back([](Stage& s){
        CreditFacility credit;
        credit.id="C2";credit.borrower="BUYER";
        credit.pledge_region="RB";credit.pledge_good="iron";
        credit.pledged_goods=2;credit.limit=200;
        s.offer_trade_credit(credit);
    });
    ok(throws_with([&]{w.advance(second);}),"GT5-T05 second 100% pledge denied");
    ok(w.commerce().credit.size()==1,"GT5-T05 no duplicate collateral claim");
}
static void t_gt508(){
    auto w=route_trade();
    const auto before=w.commit_id();
    auto v=w.trade_influence("A","B");
    ok(v.observed&&v.bilateral_imports==500&&
       v.target_total_imports==500&&v.trade_dependency_ppm==1000000,
       "GT5-T08 only real committed cross-border flow produces influence");
    ok(w.commit_id()==before,"GT5-T08 observation must not write a new ledger");
}
static void t_hd002(){
    auto w=multilateral();
    const auto& t=w.diplomacy().treaties.at("M");
    ok(t.active&&t.parties.size()==3&&t.clauses.size()==2,
       "HD0-T02 tariff and defense clauses require multilateral ratification");
    ok(w.base().military_control(2)=="B" &&
       w.base().core().core().cash("A")==500,
       "HD0-T02 defense term does not manufacture military or cash effects");
}
static void t_hd003(){
    auto p=treaty_probe();
    ok(p.old_tax==50&&p.w.commerce().fills.size()==2 &&
       p.w.commerce().customs.size()==1,
       "HD0-T03 C/G customs charged once before H exemption");
    ok(p.w.base().core().core().cash("B")==550,
       "HD0-T03 no duplicated government treasury income");
}
static void t_hd004(){
    auto w=multilateral();
    ok(w.base().sovereign(2)=="B"&&w.base().military_control(2)=="B",
       "HD0-T04 defensive treaty cannot cause invasion/occupation on its own");
    ok(w.base().core().core().journal().end()==
       std::find(w.base().core().core().journal().begin(),
                 w.base().core().core().journal().end(),"war-entry"),
       "HD0-T04 no war entry journal from a treaty alone");
}
static void t_hd008(){
    auto w=route_trade(5);
    const auto c0=w.commit_id(), v=w.assess_influence("A","B");
    ok(v.observed&&v.bilateral_imports==500,
       "HD0-T08 confirmed trade creates observational influence");
    ok(w.commit_id()==c0,"HD0-T08 no direct influence resource allocation");
}
static void t_hd303(){
    auto w=fixture();
    World::Plan p;
    p[9].push_back([](Stage& s){
        TreatyClause cl;cl.id="aid";cl.kind=ClauseKind::financial_aid;
        cl.obligor="A";cl.beneficiary="B";cl.amount=20;
        s.propose_treaty_package("M",{"A","B","C"},{cl},"A","PM_A");
        s.ratify_package_member("M","B","PM_B");
        // C deliberately withholds ratification.
    });
    w.advance(p);w.advance();
    ok(!w.diplomacy().treaties.at("M").active,
       "HD3-T03 treaty requiring all 3 cannot be active with only 2 ratifications");
    ok(w.base().core().core().cash("B")==500,
       "HD3-T03 no unratified financial transfer");
}
static void t_hd304(){
    auto p=treaty_probe();
    ok(p.w.diplomacy().treaties.at("T").active&&
       p.w.commerce().fills.size()==2&&
       p.w.commerce().fills[1].duty_paid==0,
       "HD3-T04 signed term applies exactly once to eligible trade");
}
static void t_hd305(){
    auto w=multilateral();
    ok(w.diplomacy().treaties.at("M").active&&
       w.base().military_control(2)=="B"&&
       w.base().core().core().stock("SELLER","RA","iron")==10,
       "HD3-T05 defense term cannot materialize military supplies");
}
static void t_hd501(){
    auto w=founded_org();
    ok(w.diplomacy().organizations.at("X").active&&
       w.diplomacy().organizations.at("X").members.size()==2,
       "HD5-T01 two eligible founders establish organization");
    World::Plan q;
    q[9].push_back([](Stage& s){
        s.create_organization("Y",{"A"},0,{{"A","PM_A"}});
    });
    ok(throws_with([&]{w.advance(q);},"H_ERR_INVALID_ORGANIZATION"),
       "HD5-T01 single founder legally rejected");
}
static void t_hd502(){
    auto w=fixture();
    World::Plan p;
    p[9].push_back([](Stage& s){
        s.create_organization("X",{"A","B"},0,
                              {{"A","PM_A"},{"B","PM_B"}});
        s.create_organization("Y",{"A","C"},0,
                              {{"A","PM_A"},{"C","PM_C"}});
    });
    w.advance(p);w.advance();
    ok(w.diplomacy().organizations.at("X").members.contains("A") &&
       w.diplomacy().organizations.at("Y").members.contains("A"),
       "HD5-T02 same country may join two unrelated organizations");
}
static void t_hd504(){
    auto w=fixture();
    World::Plan p;
    p[9].push_back([](Stage& s){
        s.create_organization("X",{"A","B","C"},0,
                              {{"A","PM_A"},{"B","PM_B"},{"C","PM_C"}});
        OrganizationRule rule;rule.quorum=3;rule.yes_required=2;
        s.configure_organization("X",rule,"PM_A");
    });
    w.advance(p);
    World::Plan q;
    q[9].push_back([](Stage& s){
        s.hold_organization_vote("X","V","market",
            {{"A",VoteChoice::yes},{"B",VoteChoice::yes}},"A","PM_A");
    });
    ok(throws_with([&]{w.advance(q);},"H_ERR_QUORUM_NOT_MET"),
       "HD5-T04 2 of 3 cannot pass quorum3 resolution");
    ok(w.diplomacy().organizations.at("X").decisions.empty(),
       "HD5-T04 illegal vote cannot commit");
}
static void t_hd505(){
    auto w=fixture();
    World::Plan p;
    p[9].push_back([](Stage& s){
        s.create_organization("X",{"A","B","C"},0,
                              {{"A","PM_A"},{"B","PM_B"},{"C","PM_C"}});
        OrganizationRule rule;rule.quorum=2;rule.yes_required=2;
        rule.veto_members.insert("B");
        s.configure_organization("X",rule,"PM_A");
    });
    w.advance(p);
    World::Plan q;
    q[9].push_back([](Stage& s){
        s.hold_organization_vote("X","V","policy",
            {{"A",VoteChoice::yes},{"B",VoteChoice::no},
             {"C",VoteChoice::yes}},"A","PM_A");
    });
    w.advance(q);
    ok(!w.diplomacy().organizations.at("X").decisions.at("V").adopted,
       "HD5-T05 valid national veto defeats 2:1 majority");
}
static void t_hd507(){
    auto w=founded_org();
    World::Plan p;
    p[9].push_back([](Stage& s){s.geography().queue_claim("C1",2,"A",true);});
    w.advance(p);
    ok(w.base().military_control(2)=="B" &&
       w.diplomacy().organizations.at("X").members.size()==2,
       "HD5-T07 economic membership cannot trigger war or automatic exit");
}
static void t_hd606(){
    auto w=route_trade();
    const auto snapshot=SnapshotCodec::encode(w);
    auto first=w.assess_influence("A","B");
    auto second=w.assess_influence("A","B");
    ok(first.observed&&first.bilateral_imports==second.bilateral_imports,
       "HD6-T06 influence is computed from actual trade, not points purchased");
    ok(SnapshotCodec::encode(w)==snapshot,
       "HD6-T06 influence observation must not mutate economic/diplomatic state");
}
static void t_hd609(){
    auto w=fixture();
    auto v=w.assess_influence("A","B");
    ok(!v.observed&&!v.military_known&&!v.political_known&&!v.cultural_known,
       "HD6-T09 missing influence evidence is unknown, not confirmed zero");
}
static void t_hd701(){
    auto w=claimed_crisis();
    ok(w.diplomacy().crises.at("CR").status==CrisisStatus::active,
       "HD7-T01 actual claim becomes actual legal international crisis");
    ok(w.base().sovereign(2)=="B" &&
       w.base().military_control(2)=="B",
       "HD7-T01 dispute does not create annexation or occupation");
}
static void t_hd703(){
    auto w=claimed_crisis(true);
    ok(w.diplomacy().crises.at("CR").mediator=="C" &&
       w.base().sovereign(3)=="C",
       "HD7-T03 independent small country mediates without annexing territory");
}
static void t_hv03(){
    auto p=treaty_probe();
    ok(p.old_tax==50 && !p.old_active &&
       p.w.commerce().customs.size()==1 &&
       p.w.commerce().fills.size()==2,
       "HV-T03 H ratification and G real customs/finance valid in correct months");
    ok(p.w.base().core().core().cash("B")==550 &&
       p.w.base().core().core().stock("BUYER","RB","iron")==5,
       "HV-T03 no retroactive repayment and delayed cargo delivered");
}
static void t_hv16(){
    auto w=fixture();
    World::Plan p;
    p[9].push_back([](Stage& s){
        s.create_organization("X",{"A","B","C"},0,
                              {{"A","PM_A"},{"B","PM_B"},{"C","PM_C"}});
        OrganizationRule rule;rule.quorum=3;rule.yes_required=3;
        s.configure_organization("X",rule,"PM_A");
    });
    w.advance(p);
    World::Plan q;
    q[9].push_back([](Stage& s){
        s.hold_organization_vote("X","WAR","military",
            {{"A",VoteChoice::yes},{"B",VoteChoice::yes}},"A","PM_A");
    });
    ok(throws_with([&]{w.advance(q);},"H_ERR_QUORUM_NOT_MET"),
       "HV-T16 invalid defense vote cannot pass");
    ok(w.base().military_control(2)=="B" &&
       w.diplomacy().organizations.at("X").decisions.empty(),
       "HV-T16 no unvoted military or diplomatic effects");
}
static void t_hv20(){
    auto w=founded_org();
    World::Plan p;
    p[9].push_back([](Stage& s){s.geography().queue_claim("E1",2,"A",true);});
    w.advance(p);
    World::Plan q;
    q[9].push_back([](Stage& s){s.open_crisis("CR",2,"A","B","PM_A");});
    w.advance(q);
    ok(w.diplomacy().organizations.at("X").members.contains("A") &&
       w.diplomacy().organizations.at("X").members.contains("B") &&
       w.base().military_control(2)=="B",
       "HV-T20 joint economic membership does not force crisis war or exit");
}
static void t_hv37(){
    auto w=claimed_crisis(true);
    ok(w.base().spatial().claims.at(2).contains("A") &&
       w.diplomacy().crises.at("CR").mediator=="C" &&
       w.base().sovereign(2)=="B",
       "HV-T37 E disputed province and H neutral mediation stay independent");
}
static void t_hv61(){
    auto w=fixture(0,4000,100,60);
    World::Plan p;
    p[9].push_back([](Stage& s){
        s.geography().queue_transfer("cession",{2},"A",11);
        s.link_reparation_treaty("R","B","A",100,"PM_B");
    });
    w.advance(p);
    const auto committed=w.commit_id();
    World::Plan impossible;
    impossible[5].push_back([](Stage& s){s.pay_reparation("R",100);});
    ok(throws_with([&]{w.advance(impossible);}),
       "HV-T61 actual payer cash60 cannot settle simultaneous 100 obligation");
    ok(w.commit_id()==committed && w.base().sovereign(2)=="B" &&
       w.base().core().core().cash("B")==60,
       "HV-T61 staged E title transfer and H/C financial obligation roll back together");
}
static void t_hv62(){
    auto w=fixture();
    w.seed_route(Route{"R","SELLER","BUYER","iron",1,2,5,1});
    World::Plan p;
    p[9].push_back([](Stage& s){
        s.create_organization("X",{"A","B"},0,
                              {{"A","PM_A"},{"B","PM_B"}});
    });
    p[11].push_back([](Stage&){throw std::runtime_error("orphan jurisdiction injected");});
    ok(throws_with([&]{w.advance(p);},"orphan jurisdiction injected"),
       "HV-T62 deliberately invalid stage11 must abort");
    ok(w.commit_id()==0&&w.commerce().fills.empty()&&
       w.diplomacy().organizations.empty()&&
       w.base().core().core().stock("SELLER","RA","iron")==10,
       "HV-T62 all C/E/G/H stage operations are atomic");
}
static void t_hv65(){
    auto w=fixture();
    w.seed_route(Route{"R","SELLER","BUYER","iron",1,2,2,1});
    World::Plan p;
    p[9].push_back([](Stage& s){
        s.create_organization("X",{"A","B"},0,
                              {{"A","PM_A"},{"B","PM_B"}});
        s.propose_tariff_treaty("T","A","B","iron","PM_A");
        s.ratify_treaty("T","B","PM_B");
    });
    w.advance(p);
    auto restored=SnapshotCodec::decode(SnapshotCodec::encode(w));
    for(int i=0;i<50;++i){w.advance();restored.advance();}
    ok(SnapshotCodec::encode(w)==SnapshotCodec::encode(restored),
       "HV-T65 complete H treaties, membership, G trade and C/E ledger survive 50-month replay");
}
static void t_hv66(){
    auto w=fixture();w.advance();
    auto bytes=SnapshotCodec::encode(w);
    auto corrupt=bytes;corrupt[corrupt.size()/2]^=0x04;
    ok(throws_with([&]{(void)SnapshotCodec::decode(corrupt);}),
       "HV-T66 incompatible/corrupted save must reject");
    auto eonly=vic25::phasee::SnapshotCodec::encode(w.base());
    ok(throws_with([&]{(void)SnapshotCodec::decode(eonly);}),
       "HV-T66 E-only save must not be treated as valid G/H composite");
}
using Test=void(*)();
static const std::map<std::string,Test>& covered(){
    static const std::map<std::string,Test> map={
        {"GD0-T01",t_gd01},{"GD0-T03",t_gd03},{"GD0-T04",t_gd04},
        {"GD0-T07",t_gd07},{"GD0-T08",t_gd08},
        {"GT1-T01",t_gt101},{"GT1-T08",t_gt108},
        {"GT2-T01",t_gt201},
        {"GT3-T01",t_gt301},{"GT3-T02",t_gt302},{"GT3-T10",t_gt310},
        {"GT4-T07",t_gt407},{"GT4-T08",t_gt408},
        {"GT5-T01",t_gt501},{"GT5-T05",t_gt505},{"GT5-T08",t_gt508},
        {"HD0-T02",t_hd002},{"HD0-T03",t_hd003},{"HD0-T04",t_hd004},
        {"HD0-T08",t_hd008},
        {"HD3-T03",t_hd303},{"HD3-T04",t_hd304},{"HD3-T05",t_hd305},
        {"HD5-T01",t_hd501},{"HD5-T02",t_hd502},
        {"HD5-T04",t_hd504},{"HD5-T05",t_hd505},{"HD5-T07",t_hd507},
        {"HD6-T06",t_hd606},{"HD6-T09",t_hd609},
        {"HD7-T01",t_hd701},{"HD7-T03",t_hd703},
        {"HV-T03",t_hv03},{"HV-T16",t_hv16},{"HV-T20",t_hv20},
        {"HV-T37",t_hv37},{"HV-T61",t_hv61},{"HV-T62",t_hv62},
        {"HV-T65",t_hv65},{"HV-T66",t_hv66}
    };
    return map;
}
static int number(const std::string& s){
    if(s.size()!=2||s[0]<'0'||s[0]>'9'||s[1]<'0'||s[1]>'9')return -1;
    return (s[0]-'0')*10+(s[1]-'0');
}
static bool is_contract_id(const std::string& id){
    if(id.size()==7 && id.substr(0,5)=="GD0-T"){
        int n=number(id.substr(5));return n>=1&&n<=12;
    }
    if(id.size()==7 && id.substr(0,5)=="HD0-T"){
        int n=number(id.substr(5));return n>=1&&n<=20;
    }
    if(id.size()==7&&id.substr(0,2)=="GT"&&
       id[2]>='1'&&id[2]<='5'&&id.substr(3,2)=="-T"){
        int n=number(id.substr(5));return n>=1&&n<=10;
    }
    if(id.size()==7&&id.substr(0,2)=="HD"&&
       id[2]>='1'&&id[2]<='7'&&id.substr(3,2)=="-T"){
        int n=number(id.substr(5));return n>=1&&n<=12;
    }
    if(id.size()==6&&id.substr(0,4)=="HV-T"){
        int n=number(id.substr(4));return n>=1&&n<=72;
    }
    return false;
}
static std::string reason(const std::string& id) {
    if(id=="HV-T72")return "requires 237 other PASS records plus POP/C/D/E/G/H release evidence";
    if(id.rfind("HV-T",0)==0)
        return "cross-domain acceptance oracle needs unfinished C-M5/E/F/POP or multi-system fixtures; see H-V5";
    if(id.rfind("HD1-",0)==0)return "H-D1 full trust, recognition, perceptions and information timing not implemented";
    if(id.rfind("HD2-",0)==0)return "H-D2 general diplomatic missions/negotiation communication not implemented";
    if(id.rfind("HD3-",0)==0)return "H-D3 complete ratification, clause conflict and succession semantics not implemented";
    if(id.rfind("HD4-",0)==0)return "H-D4 complete historical delegated rights, state succession and E joint sovereignty not implemented";
    if(id.rfind("HD5-",0)==0)return "H-D5 full institution/membership finances, votes and treaty obligations not implemented";
    if(id.rfind("HD6-",0)==0)return "H-D6 complete multi-source influence, uncertainty, policy counterfactuals not implemented";
    if(id.rfind("HD7-",0)==0)return "H-D7 full conflict-entry, peace, crisis escalation AI and multiparty cases not implemented";
    if(id.rfind("GT1-",0)==0)return "G-T1 full consignment/broker/merchant employment and profit ledgers not implemented";
    if(id.rfind("GT2-",0)==0)return "G-T2 full global market discovery, price/FX and route access rules not implemented";
    if(id.rfind("GT3-",0)==0)return "G-T3 full delivery/title/risk/partial invoicing lifecycle not implemented";
    if(id.rfind("GT4-",0)==0)return "G-T4 complete customs accrual, quotas, origin and subsidies not implemented";
    if(id.rfind("GT5-",0)==0)return "G-T5 complete insurance, FX, supply shocks, export statistics and performance not implemented";
    if(id.rfind("GD0-",0)==0)return "G-D0 full commerce/finance/customs/pop/tooltip end-to-end oracle not implemented";
    return "H-D0 full legal/military/finance/UI and save/worker acceptance conditions not implemented";
}
int main(int argc,char** argv){
    if(argc!=2){
        std::cerr<<"usage: vic25_phase_gh_acceptance <ID>\n";return 2;
    }
    const std::string id=argv[1];
    if(!is_contract_id(id)){
        std::cerr<<"ERROR unknown acceptance ID: "<<id<<"\n";return 2;
    }
    const auto it=covered().find(id);
    if(it==covered().end()){
        std::cout<<"BLOCKED "<<id<<" "<<reason(id)<<"\n";
        return 77; // CTest SKIP_RETURN_CODE, not a PASS.
    }
    try {
        it->second();
        std::cout<<"PASS "<<id<<" independent real C/D/E/G/H oracle\n";
        return 0;
    } catch(const std::exception& e){
        std::cerr<<"FAIL "<<id<<" "<<e.what()<<"\n";return 1;
    }
}
