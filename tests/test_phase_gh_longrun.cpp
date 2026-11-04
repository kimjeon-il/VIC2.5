#include "vic25/phase_gh_snapshot.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace vic25::phasegh;
static void check(bool p,const char* why){if(!p)throw std::runtime_error(why);}
static World fixture() {
    World w(1801*12);
    auto& e=w.seed_geography();
    e.set_dataset("1801-synthetic-gh-100yr");
    for(Id id=1;id<=8;++id) {
        const std::string region="R"+std::to_string(id);
        const std::string country=id<=4?"A":"B";
        e.seed_province(id,region,country);
    }
    e.seed_unit(11,"A","A");e.seed_unit(22,"B","B");
    for(Id id=1;id<=8;++id) {
        e.seed_assignment(id,id<=4?11:22);
        if(id>1)e.seed_border(id-1,id);
    }
    auto& c=e.seed_core().seed_core();
    c.seed_actor("A",100000);
    c.seed_actor("B",100000);
    c.seed_actor("S",0);
    c.seed_actor("BUYER",500000);
    c.seed_actor("ORG:X",0);
    c.seed_stock("S","R4","iron",1300);
    c.seed_stock("S","R3","coal",1300);
    c.seed_market("R4","iron",100,10,300);
    c.seed_market("R5","iron",150,10,300);
    c.seed_market("R3","coal",50,5,150);
    c.seed_market("R6","coal",90,5,150);
    c.seed_government("A","headA","PM_A",5,3);
    c.seed_government("B","headB","PM_B",5,3);
    w.seed_tariff("B","iron",100000);
    w.seed_tariff("B","coal",100000);
    w.seed_route(Route{"iron","S","BUYER","iron",4,5,1,1});
    w.seed_route(Route{"coal","S","BUYER","coal",3,6,1,2});
    return w;
}
int main(){
    try {
        const auto start=std::chrono::steady_clock::now();
        auto w=fixture();
        World::Plan init;
        init[9].push_back([](Stage& s) {
            s.create_organization("X",{"A","B"},0,
                                   {{"A","PM_A"},{"B","PM_B"}});
            s.propose_tariff_treaty("iron-free","A","B","iron","PM_A");
            s.ratify_treaty("iron-free","B","PM_B");
            s.geography().queue_claim("CLAIM_B_R4",4,"B",true);
        });
        w.advance(init);
        World* retained=nullptr; // pointer not used; avoids copying giant worlds
        (void)retained;
        std::string mid_save;
        for(int i=1;i<1200;++i) {
            w.advance();
            if(i==599)mid_save=SnapshotCodec::encode(w);
            if((i%120)==0) {
                w.validate();
                check(w.diplomacy().organizations.at("X").active,
                      "longrun organization vanished");
                check(w.diplomacy().treaties.at("iron-free").active,
                      "longrun ratified trade term vanished");
            }
        }
        check(w.commit_id()==1200 && w.month()==1801*12+1200,
              "100 years of full staged months required");
        check(w.commerce().fills.size()==2400 &&
              w.base().core().core().stock("S","R4","iron")==100 &&
              w.base().core().core().stock("S","R3","coal")==100,
              "100-year contract route count/stock conservation");
        check(w.trade_influence("A","B").observed,
              "100-year source trade evidence preserved");
        check(w.base().core().core().stock("BUYER","R5","iron")==1199 &&
              w.base().core().core().stock("BUYER","R6","coal")==1198,
              "physical travel causes exact 1/2 month arrival delay");
        auto replay=SnapshotCodec::decode(mid_save);
        for(int i=600;i<1200;++i)replay.advance();
        check(SnapshotCodec::encode(w)==SnapshotCodec::encode(replay),
              "600-month checkpoint followed by 600-month replay must be bitwise identical");
        const auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now()-start).count();
        std::cout<<"G/H longrun 1200 months + 600-month replay PASS; ms="
                 <<ms<<" fills="<<w.commerce().fills.size()<<"\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"G/H 100-year test FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
