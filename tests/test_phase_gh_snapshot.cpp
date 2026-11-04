#include "vic25/phase_gh_snapshot.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace vic25::phasegh;
static void check(bool x,const char* why){if(!x)throw std::runtime_error(why);}
template<class F>static bool fails(F&& fn){
    try{fn();}catch(const std::exception&){return true;}return false;
}
static World sample_world() {
    World w(100);
    auto& e=w.seed_geography();
    e.set_dataset("gh-save-geography-1");
    e.seed_province(1,"RA","A");
    e.seed_province(2,"RB","B");
    e.seed_border(1,2);
    e.seed_unit(10,"A","A");
    e.seed_unit(20,"B","B");
    e.seed_assignment(1,10);
    e.seed_assignment(2,20);
    auto& c=e.seed_core().seed_core();
    c.seed_actor("A",500);
    c.seed_actor("B",500);
    c.seed_actor("SELLER",100);
    c.seed_actor("BUYER",10000);
    c.seed_actor("ORG:SAVE",0);
    c.seed_stock("SELLER","RA","iron",12);
    c.seed_market("RA","iron",100,1,200);
    c.seed_market("RB","iron",170,1,300);
    c.seed_forex_balance("BUYER","USD",1000);
    c.seed_government("A","A-head","PM_A",5,3);
    c.seed_government("B","B-head","PM_B",5,3);
    w.seed_tariff("B","iron",100000);
    w.seed_route(Route{"w-route","SELLER","BUYER","iron",1,2,2,1,0});
    w.seed_transport_edge(TransportEdge{"land","A",1,2,2,1,1,true});
    return w;
}
static void roundtrip_and_deterministic_replay() {
    auto w=sample_world();
    World::Plan p;
    p[9].push_back([](Stage& s){
        s.create_organization("SAVE",{"A","B"},100,
                               {{"A","PM_A"},{"B","PM_B"}});
        s.propose_tariff_treaty("T","A","B","iron","PM_A");
        s.ratify_treaty("T","B","PM_B");
        s.geography().queue_claim("claimA",2,"A",true);
    });
    w.advance(p);
    World::Plan m101;
    m101[5].push_back([](Stage& s){s.pay_dues("SAVE","A",40);});
    m101[9].push_back([](Stage& s){
        s.open_crisis("C",2,"A","B","PM_A");
    });
    w.advance(m101);
    const auto data=SnapshotCodec::encode(w);
    auto restored=SnapshotCodec::decode(data);
    check(SnapshotCodec::encode(restored)==data,
          "G/H entire C/D/E and G/H state exact binary roundtrip");
    check(restored.commit_id()==w.commit_id()&&
          restored.diplomacy().organizations.at("SAVE").paid.at({101,"A"})==40 &&
          restored.diplomacy().treaties.at("T").active &&
          restored.commerce().fills.size()==2,
          "reconstruction preserves treaty, dues, actual trade and time");
    for(int i=0;i<15;++i) {
        w.advance();
        restored.advance();
        check(SnapshotCodec::encode(w)==SnapshotCodec::encode(restored),
              "save/replay world bytes diverged");
    }
    check(restored.base().core().core().stock("BUYER","RB","iron")==12,
          "all cargo arrives after saved world reload");
    check(restored.commerce().fills.size()==6,
          "new records stop after stock exhaustion, no extra sale");
}
static void corruption_contract_and_file_atomicity() {
    auto w=sample_world();
    w.advance();
    const auto data=SnapshotCodec::encode(w);
    std::string corrupt=data;
    corrupt[corrupt.size()/2]^=0x04;
    check(fails([&]{(void)SnapshotCodec::decode(corrupt);}),
          "checksum or nested checksum must reject corruption");
    auto ebytes=vic25::phasee::SnapshotCodec::encode(w.base());
    check(fails([&]{(void)SnapshotCodec::decode(ebytes);}),
          "E-only snapshot must not falsely deserialize as full G/H save");
    const auto path=std::filesystem::temp_directory_path()/
                            "vic25-phase-gh-snapshot-test.bin";
    std::filesystem::remove(path);
    SnapshotCodec::save(w,path);
    check(fails([&]{SnapshotCodec::save(w,path);}),
          "checkpoint should never silently overwrite committed save");
    auto loaded=SnapshotCodec::load(path);
    check(SnapshotCodec::encode(loaded)==data,
          "disk checkpoint roundtrip matches original");
    std::filesystem::remove(path);
}
int main() {
    try{
        roundtrip_and_deterministic_replay();
        corruption_contract_and_file_atomicity();
        std::cout<<"Phase G/H composite save and recovery: 2 test groups PASS\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"Phase G/H snapshot FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
