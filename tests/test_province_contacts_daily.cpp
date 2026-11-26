#include "vic25/province_contacts.hpp"
#include "vic25/p0_daily.hpp"
#include "vic25/daily_snapshot.hpp"
#include <stdexcept>
#include <string>

using namespace vic25;
using namespace phasee::geography_import;

static void check(bool ok,const char* why) {
    if(!ok)throw std::runtime_error(why);
}

static phasegh::World fixture(bool service) {
    phasegh::World world(24000);
    auto& geo=world.seed_geography();
    geo.set_dataset("synthetic-contact-proof/1");
    geo.seed_province(1,"SHORE_A","A");
    geo.seed_province(2,"SHORE_B","B");
    geo.seed_province(3,"ISLAND","B");
    geo.seed_unit(10,"A","A district");
    geo.seed_unit(20,"B","B district");
    geo.seed_assignment(1,10);
    geo.seed_assignment(2,20);
    geo.seed_assignment(3,20);
    const std::vector<Contact> shared_sides{
        {{Kind::land,1},{Kind::land,2},9},
        {{Kind::land,1},{Kind::ocean,101},5},
        {{Kind::land,3},{Kind::ocean,101},6},
        {{Kind::land,2},{Kind::lake,102},5},
        {{Kind::land,3},{Kind::lake,102},8},
        {{Kind::ocean,101},{Kind::ocean,103},8}};
    auto broken=shared_sides;
    broken.back().shared_sides=0;
    bool rejected=false;
    try{seed_contacts(geo,"synthetic-contact-proof/1",broken);}
    catch(const std::invalid_argument&){rejected=true;}
    check(rejected&&geo.spatial().provinces.at(1).neighbors.empty(),
          "malformed late row partially applied the import");
    seed_contacts(geo,"synthetic-contact-proof/1",shared_sides);
    seed_contacts(geo,"synthetic-contact-proof/1",shared_sides);
    const auto& locs=geo.spatial().provinces;
    check(locs.at(1).neighbors==std::set<phasee::Id>{2}&&
          locs.at(2).neighbors==std::set<phasee::Id>{1}&&
          locs.at(3).neighbors.empty(),"non-land contact became a land border");
    check(locs.at(1).coastal_seas==std::set<phasee::Id>{101}&&
          locs.at(3).coastal_seas==std::set<phasee::Id>{101}&&
          locs.at(2).coastal_seas.empty(),"lake became a coast or ocean was lost");
    check(geo.spatial().geographic_links.empty()&&world.commerce().transport_edges.empty(),
          "contact import created a route or service");
    auto& c=geo.seed_core().seed_core();
    for(const auto* id:{"A","B","SELLER","TRADER","CARRIER"})
        c.seed_actor(id,std::string(id)=="TRADER"?1000:0);
    c.seed_government("A","HEAD_A","PM_A",5,3);
    c.seed_government("B","HEAD_B","PM_B",5,3);
    c.seed_market("SHORE_A","ore",10,10,10,0);
    c.seed_market("SHORE_B","ore",30,30,30,8);
    c.seed_market("ISLAND","ore",30,30,30,8);
    c.seed_stock("SELLER","SHORE_A","ore",20);
    if(service) {
        world.seed_verified_geographic_link("one-direction-road",1,2,"road");
        phasegh::TransportEdge edge;
        edge.id="documented-road";edge.carrier="CARRIER";
        edge.from=1;edge.to=2;edge.mode="road";
        edge.monthly_capacity=62;edge.freight_per_unit=2;edge.transit_days=2;
        edge.evidence_id="synthetic-road-record";
        edge.valid_from_month=23999;edge.valid_through_month=24012;
        world.seed_transport_edge(edge);
        check(!phasegh::quote_daily_freight_path(world.commerce(),2,1,24000),
              "directed road created reverse freight service");
    }
    world.validate();
    return world;
}

static void run(bool service) {
    p0::Policies p;
    p.monthly_clothing_units_per_person=0;
    p.merchant_offices.push_back({"TRADER","SHORE_B",8,0,50000});
    p.merchant_offices.push_back({"TRADER","ISLAND",8,0,50000});
    p0::DailyGame game(daily::World::adopt(fixture(service),6),p);
    game.advance_day();
    const auto& trade=game.world().engine().commerce();
    const auto stock=[&](const char* actor,const char* region){
        return game.world().engine().base().core().core().stock(actor,region,"ore");
    };
    check(stock("TRADER","SHORE_B")==0&&stock("TRADER","ISLAND")==0,
          "freight stock appeared before arrival");
    if(!service) {
        check(trade.fills.empty()&&trade.daily_shipments.empty()&&
              stock("SELLER","SHORE_A")==20,
              "borders or shared coast dispatched cargo without service");
        return;
    }
    check(trade.fills.size()==1&&trade.daily_shipments.size()==1&&
          trade.fills.front().physical_path==std::vector<std::string>{"documented-road"},
          "documented directed road failed to dispatch via native daily engine");
    const auto saved=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame replay(daily::SnapshotCodec::decode(saved),p);
    game.advance_day();replay.advance_day();
    check(stock("TRADER","SHORE_B")==0,"cargo arrived a day early");
    game.advance_day();replay.advance_day();
    check(stock("TRADER","SHORE_B")==2&&stock("TRADER","ISLAND")==0&&
          daily::SnapshotCodec::encode(game.world())==daily::SnapshotCodec::encode(replay.world()),
          "delivery or deterministic replay failed");
}

int main(){run(false);run(true);}
