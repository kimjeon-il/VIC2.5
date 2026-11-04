#include "vic25/phase_gh.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
using vic25::phasegh::World;
using vic25::phasegh::Stage;
using vic25::phasegh::Route;
using vic25::phasegh::TransportEdge;
using vic25::phasegh::TradeContract;
using vic25::phasegh::CreditFacility;
using vic25::phasegh::CargoInsurance;
static void verify(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class F>static bool rejects(F&& f) {
    try{f();}catch(const std::exception&){return true;}return false;
}
static World fixture(bool auto_route=false) {
    World w(100);
    auto& e=w.seed_geography();
    e.set_dataset("G-H-commerce-fixture");
    e.seed_province(1,"RA","A");
    e.seed_province(2,"RB","B");
    e.seed_border(1,2);
    e.seed_unit(10,"A","A");
    e.seed_unit(20,"B","B");
    e.seed_assignment(1,10);
    e.seed_assignment(2,20);
    auto& c=e.seed_core().seed_core();
    for(auto actor:{"A","B","SELLER","BUYER","INSURER","CARRIER"})
        c.seed_actor(actor,std::string(actor)=="BUYER"?3000:
                            std::string(actor)=="INSURER"?500:100);
    c.seed_stock("SELLER","RA","iron",10);
    c.seed_stock("BUYER","RB","iron",2);
    c.seed_market("RA","iron",100,1,200);
    c.seed_market("RB","iron",150,1,250);
    c.seed_government("A","headA","PM_A",5,3);
    c.seed_government("B","headB","PM_B",5,3);
    w.seed_tariff("B","iron",100000);
    if(auto_route)w.seed_route(Route{"route","SELLER","BUYER","iron",
                                     1,2,5,1,2});
    return w;
}
static void foreign_contract_split_logistics_and_tariff() {
    auto w=fixture();
    w.seed_geography().seed_core().seed_core().seed_forex_balance("BUYER","USD",1000);
    w.seed_transport_edge(TransportEdge{"rail","CARRIER",1,2,2,5,1,true});
    World::Plan first;
    first[9].push_back([](Stage& s){
        TradeContract c;
        c.id="contractX";c.seller="SELLER";c.buyer="BUYER";
        c.good="iron";c.currency="USD";c.origin=1;c.destination=2;
        c.contracted=4;c.monthly_limit=4;c.unit_price=100;
        c.expires=110;
        s.create_trade_contract(c);
    });
    w.advance(first);
    const auto cash0=w.base().core().core().cash("BUYER");
    verify(w.contract("contractX").fulfilled==0,
           "contract cannot retroactively use m100 stage7");
    w.advance();
    const auto& c=w.base().core().core();
    verify(w.contract("contractX").fulfilled==2,"edge capacity splits contract");
    verify(w.commerce().fills.size()==1&&w.commerce().fills[0].quantity==2,
           "one actual contractual dispatch");
    verify(w.commerce().fills[0].currency=="USD",
           "foreign currency label must be preserved");
    verify(c.fx_balance("BUYER","USD")==800 &&
           c.fx_balance("SELLER","USD")==200,
           "foreign funds settled in actual C per-currency ledger");
    verify(c.cash("BUYER")==cash0-30 &&
           c.cash("B")==120&&c.cash("CARRIER")==110,
           "duty and freight are real C cash transfers, no currency mixing");
    verify(c.stock("BUYER","RB","iron")==2 &&
           c.stock("TRANSIT:G:contract:contractX:101","in_transit","iron")==2,
           "foreign purchase remains in transit until actual arrival");
    verify(w.commerce().customs.size()==1 &&
           w.commerce().customs[0].assessed==20 &&
           w.commerce().customs[0].paid==20,
           "customs audit corresponds to government cash");
    w.advance();
    verify(w.contract("contractX").fulfilled==4 &&
           w.base().core().core().fx_balance("BUYER","USD")==600,
           "second month's shipment consumes actual remaining FX");
    verify(w.base().core().core().stock("BUYER","RB","iron")==4,
           "first cargo arrives only in following month");
    w.advance();
    verify(w.base().core().core().stock("BUYER","RB","iron")==6 &&
           w.commerce().fills.size()==2,
           "no phantom shipments once contract is fully fulfilled");
    w.validate();
}
static void secured_credit_uses_C_bank_loan_and_repayment() {
    auto w=fixture();
    World::Plan first;
    first[9].push_back([](Stage& s){
        CreditFacility credit;
        credit.id="creditX";credit.borrower="BUYER";
        credit.pledge_region="RB";credit.pledge_good="iron";
        credit.pledged_goods=2;credit.limit=200;
        s.offer_trade_credit(credit);
    });
    w.advance(first);
    const auto before=w.base().core().core().cash("BUYER");
    World::Plan second;
    second[1].push_back([](Stage& s){s.draw_trade_credit("creditX",100);});
    second[8].push_back([](Stage& s){s.repay_trade_credit("creditX",40);});
    w.advance(second);
    const auto& c=w.base().core().core();
    verify(w.trade_credit("creditX").drawn==60,"contractual drawn debt tracked");
    verify(c.cash("BUYER")==before+60,"actual C bank deposit and repayment");
    verify(c.financial().balance("BANK:loan")==60,
           "C bank balance must match outstanding loan");
    w.validate();
    // The same physical lot cannot collateralize two fully secured lines.
    World::Plan conflicting;
    conflicting[9].push_back([](Stage& s){
        CreditFacility credit;
        credit.id="creditDouble";credit.borrower="BUYER";
        credit.pledge_region="RB";credit.pledge_good="iron";
        credit.pledged_goods=2;credit.limit=200;
        s.offer_trade_credit(credit);
    });
    verify(rejects([&]{w.advance(conflicting);}),
           "same inventory cannot be doubly pledged");
}
static void insured_loss_compensated_from_real_insurer_cash() {
    auto w=fixture(true);
    World::Plan first;
    first[9].push_back([](Stage& s){
        CargoInsurance insurance;
        insurance.id="policy1";
        insurance.shipment_id="G:route:101";
        insurance.insurer="INSURER";
        insurance.beneficiary="BUYER";
        insurance.premium=30;
        insurance.maximum_claim=500;
        insurance.insured_quantity=5;
        s.insure_cargo(insurance);
    });
    w.advance(first); // m100 first cargo
    w.advance(); // m101 second insured cargo shipped and premium paid
    verify(w.insurance("policy1").premium_paid,"actual insurance premium paid");
    verify(w.base().core().core().cash("INSURER")==530,
           "insurer receives actual C cash");
    w.advance(); // m102 second cargo arrives: 5 dispatched, 2 lost, 3 delivered
    const auto& policy=w.insurance("policy1");
    verify(policy.claim_registered&&policy.claim_due==200 &&
           policy.claim_paid==200,
           "claim calculated from confirmed shipment loss and paid by insurer");
    verify(w.base().core().core().cash("INSURER")==330,
           "insurance claim debits actual insurer account");
    verify(w.base().core().core().stock("BUYER","RB","iron")==10,
           "initial two units + first five + second three; loss not materialized");
    verify(w.base().core().core().stock("SELLER","RA","iron")==0,
           "no phantom supply added");
}
static void network_capacity_shared_among_trade_routes() {
    auto w=fixture();
    auto& c=w.seed_geography().seed_core().seed_core();
    c.seed_actor("BUYER2",3000);
    w.seed_transport_edge(TransportEdge{"edge","CARRIER",1,2,2,1,1,true});
    w.seed_route(Route{"r1","SELLER","BUYER","iron",1,2,5,1});
    w.seed_route(Route{"r2","SELLER","BUYER2","iron",1,2,5,1});
    w.advance();
    verify(w.commerce().fills.size()==1 &&
           w.commerce().fills.front().quantity==2,
           "one shared physical edge capacity; second route blocked");
    verify(w.base().core().core().stock("SELLER","RA","iron")==8,
           "only actual quantity leaves seller inventory");
    verify(w.commerce().edge_usage.at({100,"edge"})==2,
           "network link capacity records actual one-month reservation");
}
int main() {
    try {
        foreign_contract_split_logistics_and_tariff();
        secured_credit_uses_C_bank_loan_and_repayment();
        insured_loss_compensated_from_real_insurer_cash();
        network_capacity_shared_among_trade_routes();
        std::cout<<"Phase G extended trade: 4 independent integration scenarios PASS\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"Phase G extended trade FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
