#include "vic25/phase_e_snapshot.hpp"
#include "vic25/phase_e_raster.hpp"
#include "vic25/phase_e_view.hpp"
#include "vic25/phase_e_parallel.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
using namespace vic25::phasee;

static void check(bool p,const char* reason) {
    if(!p)throw std::runtime_error(reason);
}
template<class F>static bool fails(F&& f) {
    try {f();}catch(const std::exception&){return true;}
    return false;
}
static World fixture() {
    World w(1000);
    w.set_dataset("fictional-grid-v1");
    for(Id p=1;p<=12;++p)
        w.seed_province(p,"R"+std::to_string(p),p<=6?"A":"B",100);
    for(Id p=1;p<=12;++p) {
        const Id x=(p-1)%4,y=(p-1)/4;
        if(x<3)w.seed_border(p,p+1);
        if(y<2)w.seed_border(p,p+4);
    }
    w.seed_unit(101,"A","A1");
    w.seed_unit(102,"A","A2");
    w.seed_unit(201,"B","B1");
    w.seed_unit(202,"B","B2");
    for(Id p=1;p<=12;++p)
        w.seed_assignment(p,p<=3?101:p<=6?102:p<=9?201:202);
    w.validate();
    return w;
}
static void plan(World& w,std::function<void(SpatialStage&)> f,int at=9) {
    World::Plan commands;
    commands[static_cast<std::size_t>(at)].push_back(std::move(f));
    w.advance(commands);
}
static void apply(World& w) {w.advance();}
static vic25::phaseb::Id seed_pop(World& w,Id province,People n) {
    auto& pop=w.seed_core().seed_core().seed_population();
    const auto region="R"+std::to_string(province);
    const auto h=pop.household_profile({region,"mixed","shared",n});
    const auto e=pop.economic_profile({region,"worker","wage","employee",pop.default_education()});
    const auto d=pop.demographic_profile({region,w.month()-400,vic25::phaseb::Sex::female,
                                        "culture","religion","citizen","free",
                                        pop.default_health(),vic25::phaseb::Service::civilian});
    return pop.seed_population(d,e,h,n,static_cast<vic25::phaseb::Id>(pop.slices().size()+1));
}
static void test_01(){auto w=fixture();check(fails([&]{w.seed_province(1,"P","A");}),"duplicate province");}
static void test_02(){auto w=fixture();check(w.province(1).neighbors.contains(2)&&
                       w.province(2).neighbors.contains(1)&&
                       !w.province(1).neighbors.contains(6),"edge topology");}
static void test_03(){auto w=fixture();w.seed_coast(1,77);check(w.province(1).coastal_seas.contains(77)&&
                        !w.province(1).neighbors.contains(77),"coast != land");}
static void test_07(){auto a=fixture(),b=fixture();check(SnapshotCodec::encode(a)==SnapshotCodec::encode(b),
                          "deterministic dataset serialization");}
static void test_08(){auto w=fixture();check(w.province(1).id==1&&w.province(1).area_m2==100,
                          "static id and area");}
static void test_09(){auto w=fixture();seed_pop(w,1,10);check(w.population({1})==10,"POP region binding");}
static void test_11(){auto w=fixture();plan(w,[](SpatialStage& s){s.occupy(2,"B");},10);
    check(w.sovereign(2)=="A"&&w.military_control(2)=="B","occupation not sovereignty");}
static void test_12(){auto w=fixture();plan(w,[](SpatialStage& s){s.queue_claim("claim-1",1,"B",true);});
    apply(w);check(w.spatial().claims.at(1).contains("B")&&w.sovereign(1)=="A","claim");}
static void test_13(){auto w=fixture();const auto old=SnapshotCodec::encode(w);
    check(fails([&]{plan(w,[](SpatialStage& s){s.queue_claim("dup",1,"B",true);
                 s.queue_claim("dup",2,"B",true);});}),"duplicate cmd rejected");
    check(SnapshotCodec::encode(w)==old,"duplicate rollback");}
static void test_14(){auto w=fixture();for(Id p=1;p<=12;++p)seed_pop(w,p,100);
    plan(w,[](SpatialStage& s){s.queue_transfer("cession",{2,3},"B",201);});apply(w);
    check(w.sovereign_provinces("A").size()==4&&w.sovereign_provinces("B").size()==8&&
          w.population(w.sovereign_provinces("A"))==400&&
          w.population(w.sovereign_provinces("B"))==800,"cession accounting");}
static void test_15(){auto w=fixture();
    plan(w,[](SpatialStage& s){s.queue_transfer("bad",{2,999},"B",201);});
    const auto prior=SnapshotCodec::encode(w);
    check(fails([&]{apply(w);}),"atomic invalid province");
    check(prior==SnapshotCodec::encode(w),"rollback Phase C/D/E");}
static void test_16(){auto w=fixture();plan(w,[](SpatialStage& s){
    s.queue_transfer("swap1",{1},"B",201);s.queue_transfer("swap2",{7},"A",101);
});apply(w);check(w.sovereign(1)=="B"&&w.sovereign(7)=="A"&&
w.sovereign_provinces("A").size()==6,"territory exchange");}

static void test_17(){
    auto w=fixture();
    for(Id p=1;p<=12;++p)seed_pop(w,p,100);
    w.seed_core().seed_core().seed_actor("CREDITOR",425);
    const auto funds=w.core().core().cash("CREDITOR");
    const auto before=w.population(w.sovereign_provinces("A"))+
                      w.population(w.sovereign_provinces("B"));
    plan(w,[](SpatialStage& s) {
        s.queue_independence("independence-C",{4,5},"C",301,
                             "C-PROVINCE","HEAD_C","PRIME_C",7,4);
    });
    check(w.sovereign_provinces("A").size()==6,"independence not applied early");
    apply(w);
    check(w.sovereign_provinces("A").size()==4&&
          w.sovereign_provinces("B").size()==6&&
          w.sovereign_provinces("C").size()==2 &&
          w.provinces_in_unit(301)==std::set<Id>({4,5}) &&
          w.core().core().government("C").prime_minister=="PRIME_C" &&
          w.core().core().cash("CREDITOR")==funds &&
          w.population(w.sovereign_provinces("A"))+
          w.population(w.sovereign_provinces("B"))+
          w.population(w.sovereign_provinces("C"))==before,
          "independence must preserve POP and financial titles");
    const auto saved=SnapshotCodec::encode(w);
    auto roundtrip=SnapshotCodec::decode(saved);
    check(SnapshotCodec::encode(roundtrip)==saved,"independence save replay");
    const auto original=SnapshotCodec::encode(w);
    plan(w,[](SpatialStage& s){
        s.queue_independence("invalid-independence",{7,999},"D",302,
                             "D-PROVINCE","HEAD_D","PRIME_D",1,1);
    });
    const auto beforeReject=SnapshotCodec::encode(w);
    check(fails([&]{w.advance();})&&
          beforeReject==SnapshotCodec::encode(w),"invalid independence rollback");
    (void)original;
}

static void test_18(){auto w=fixture();plan(w,[](SpatialStage& s){s.occupy(2,"B");},10);
    check(!w.spatial().fiscal.contains(2),"occupation cannot auto-create tax authority");}
static void test_19(){auto w=fixture();check(fails([&]{plan(w,[](SpatialStage& s){
    s.queue_reassign("r1",1,102);s.queue_reassign("r2",1,102);
});}),"same province twice should conflict");}
static void test_20(){auto w=fixture();const auto before=w.state_borders();
    plan(w,[](SpatialStage& s){s.queue_transfer("border",{1},"B",201);});apply(w);
    const auto after=w.state_borders();
    check(before!=after&&w.province(1).neighbors.contains(2),"border derived");}
static void test_21(){auto w=fixture();plan(w,[](SpatialStage& s){s.queue_reassign("move",3,102);});
    check(w.leaf(3)==101,"no current-month admin effect");
    apply(w);check(w.leaf(3)==102&&w.provinces_in_unit(102).size()==4,"reassign");}
static void test_22(){auto w=fixture();plan(w,[](SpatialStage& s){
    s.queue_create_admin("create",103,"A3","A");
    s.queue_reassign("assign",3,103);
});apply(w);check(w.provinces_in_unit(103)==std::set<Id>{3},"split on leaf");}
static void test_23(){auto w=fixture();plan(w,[](SpatialStage& s){s.queue_merge("merge",101,102);});
    apply(w);check(w.provinces_in_unit(101).size()==6&&!w.spatial().units.at(102).active,
                   "merge administration");}
static void test_24(){auto w=fixture();w.seed_unit(100,"A","top"); // setup extra root
    plan(w,[](SpatialStage& s){s.queue_reparent("reparent",101,100);});apply(w);
    check(w.spatial().units.at(101).parent==100&&w.provinces_in_unit(100).size()==3,
          "reparent derived subtree");}
static void test_25(){auto w=fixture();w.seed_unit(100,"A","top");w.seed_unit(99,"A","root",100);
    const auto before=SnapshotCodec::encode(w);
    plan(w,[](SpatialStage& s){s.queue_reparent("cycle",100,99);});
    const auto prior=SnapshotCodec::encode(w);
    check(fails([&]{apply(w);})&&SnapshotCodec::encode(w)==prior,"cycle atomic rollback");(void)before;}
static void test_26(){auto w=fixture();check(fails([&]{w.seed_assignment(1,102);}),"double leaf");}
static void test_27(){auto w=fixture();check(fails([&]{plan(w,[](SpatialStage& s){s.queue_reassign("cross",1,201);});
                          apply(w);}),"cross-sovereignty admin");}
static void test_28(){auto w=fixture();plan(w,[](SpatialStage& s){s.queue_rename("rename",101,"NewName");});
    check(w.spatial().units.at(101).name=="A1","m authority still old");
    apply(w);check(w.spatial().units.at(101).name=="NewName","m+1 active");}
static void test_29(){auto w=fixture();plan(w,[](SpatialStage& s){s.queue_rename("ren",101,"North");});
    apply(w);check(w.spatial().units.at(101).id==101,"rename retains ID");}
static void test_31(){auto w=fixture();TerritorialScope sc;sc.id=1;sc.kind="lower-house";sc.provinces={1,4,7,10};
    w.seed_scope(sc);check(w.scope_members(1).contains(7)&&w.leaf(7)==201,
                         "district independent of State");}

static void test_32(){
    auto w=fixture();
    const auto slice=seed_pop(w,2,100);
    w.seed_core().seed_core().seed_person_anchor("P2-ANCHOR",slice);
    w.seed_scope({301,"lower-house",ScopeMode::explicit_provinces,0,{2}});
    w.seed_scope({302,"lower-house",ScopeMode::explicit_provinces,0,{2}});
    plan(w,[slice](SpatialStage& s){
        s.open_election("HOUSE", {301,302});
        s.allocate_electors("HOUSE",301,slice,60);
        s.allocate_electors("HOUSE",302,slice,40);
        s.register_elector("HOUSE",301,"P2-ANCHOR");
        s.cast_elector_ballot("HOUSE",301,"P2-ANCHOR","PARTY-A");
    });
    check(w.core().core().named_votes("HOUSE")==1,"real Phase C ballot registered once");
    const auto state=SnapshotCodec::encode(w);
    check(fails([&]{plan(w,[](SpatialStage& s){
        s.register_elector("HOUSE",302,"P2-ANCHOR");
    });})&&SnapshotCodec::encode(w)==state,"duplicated named voter rolled back");
    check(fails([&]{plan(w,[slice](SpatialStage& s){
        s.allocate_electors("HOUSE",302,slice,1);
    });}),"same slice district allocation cannot silently expand");
}
static void test_37(){
    auto w=fixture();
    const auto first=seed_pop(w,1,100),second=seed_pop(w,2,300);
    w.seed_core().seed_core().seed_actor("WORKSHOP1",0);
    w.seed_core().seed_core().seed_actor("WORKSHOP2",0);
    World::Plan plan_;
    plan_[2].push_back([first,second](SpatialStage& s){
        s.phase_c().hire(first,100,"WORKSHOP1",10);
        s.phase_c().hire(second,300,"WORKSHOP2",20);
    });
    w.advance(plan_);
    const auto [money,n]=w.contractual_wage_mean({1,2});
    check(money==7000&&n==400&&money/n==17&&money%n==200,
          "worker-weighted contracted wage 17.5, not naive mean15");
}
static void test_39(){
    auto w=fixture();
    const auto slice=seed_pop(w,3,100);
    w.seed_core().seed_core().seed_person_anchor("VOTER_P3",slice);
    w.seed_scope({450,"lower-house",ScopeMode::derived_administrative,101,{}});
    plan(w,[slice](SpatialStage& s){
        s.open_election("ELECTION-01",{450});
        s.allocate_electors("ELECTION-01",450,slice,1);
        s.register_elector("ELECTION-01",450,"VOTER_P3");
        s.queue_reassign("future-admin",3,102);
    });
    const auto frozen=w.election("ELECTION-01").provinces_by_district.at(450);
    w.advance();
    check(!w.scope_members(450).contains(3)&&frozen.contains(3),
          "frozen election boundary must outlive admin change");
    plan(w,[](SpatialStage& s){
        s.cast_elector_ballot("ELECTION-01",450,"VOTER_P3","PARTY-A");
    });
    check(w.core().core().named_votes("ELECTION-01")==1,
          "valid historic ballot uses frozen scope, not latest administrative map");
}
static void test_40(){
    auto w=fixture();
    const auto slice=seed_pop(w,2,100);
    w.seed_scope({501,"lower-house",ScopeMode::explicit_provinces,0,{2}});
    w.seed_scope({502,"lower-house",ScopeMode::explicit_provinces,0,{2}});
    plan(w,[slice](SpatialStage& s) {
        s.open_election("SPLIT-P2",{501,502});
        s.allocate_electors("SPLIT-P2",501,slice,60);
        s.allocate_electors("SPLIT-P2",502,slice,40);
    });
    check(w.election("SPLIT-P2").split_without_geometry.contains(2)&&
          w.election("SPLIT-P2").allocated.at({501,slice})+
          w.election("SPLIT-P2").allocated.at({502,slice})==100 &&
          w.province(2).id==2,
          "population electoral partition must never invent Province geometry");
    const auto before=SnapshotCodec::encode(w);
    check(fails([&]{plan(w,[slice](SpatialStage& s) {
        s.allocate_electors("SPLIT-P2",501,slice,61);
    });})&&before==SnapshotCodec::encode(w),
        "no overloaded or rewritten electorate");
}

static void test_33(){auto w=fixture();TerritorialScope a{1,"lower-house",ScopeMode::explicit_provinces,0,{1,2}};
    TerritorialScope b{2,"municipal",ScopeMode::explicit_provinces,0,{1,2}};
    w.seed_scope(a);w.seed_scope(b);
    check(w.scope_members(1)==w.scope_members(2),"different contests may overlap");}
static void test_34(){auto w=fixture();TerritorialScope t{1,"local-government",
    ScopeMode::derived_administrative,101,{}};w.seed_scope(t);
    plan(w,[](SpatialStage& s){s.queue_reassign("move",3,102);});apply(w);
    check(w.scope_members(1)==std::set<Id>({1,2}),"derived follows admin change");}
static void test_35(){auto w=fixture();w.seed_scope({1,"military",ScopeMode::explicit_provinces,0,{2}});
    w.seed_scope({2,"fiscal",ScopeMode::explicit_provinces,0,{2}});
    check(w.scope_members(1).contains(2)&&w.scope_members(2).contains(2),"legal overlap");}
static void test_36(){auto w=fixture();w.seed_scope({1,"local-government",
    ScopeMode::derived_administrative,101,{}});
    check(w.core().core().financial().balance("BANK:deposit")==0,
          "scope must not create money");}
static void test_38(){auto w=fixture();w.seed_scope({1,"special",ScopeMode::explicit_provinces,0,{1,2}});
    w.seed_scope({2,"special",ScopeMode::explicit_provinces,0,{2,3}});
    auto union_set=w.scope_members(1);auto second=w.scope_members(2);
    union_set.insert(second.begin(),second.end());check(union_set.size()==3,"dedup union");}
static void test_41(){auto pos=sphere_pick({2,0,0,-1,0,0});
    check(std::abs(pos.lon)<1e-9&&std::abs(pos.lat)<1e-9,"ray sphere intersection");}
static void test_42(){RasterIdTile t{2,2,{101,102,103,104}};
    check(t.sample(0,0)==101&&t.sample(1,0)==102&&
          t.sample(0,1)==103&&t.sample(1,1)==104,"integer id sampling");}

static void test_43(){
    auto w=fixture();
    vic25::phasee::view::Locator locator;
    locator.add(9,12.75,42.1,w);
    const std::set<Id> visible={1,2,3,4,5,6,7,8,10,11,12};
    const auto fallback=locator.invisible_in(visible);
    check(fallback.size()==1&&fallback[0].province==9&&
          locator.pick_near(12.75,42.1,1.0,visible)==std::optional<Id>{9}&&
          w.province(9).id==9,
          "small island retains authoritative ID and a georeferenced LOD marker");
}
static void test_45(){
    auto w=fixture();
    vic25::phasee::view::Display renderer(w);
    const auto snapshot=SnapshotCodec::encode(w);
    check(renderer.owner(1)=="A","sovereignty display");
    renderer.set_layer(vic25::phasee::view::Layer::administration);
    check(renderer.owner(1)=="ADMIN:101","administrative display");
    renderer.set_layer(vic25::phasee::view::Layer::military);
    check(renderer.owner(1)=="A","military mode");
    renderer.set_layer(vic25::phasee::view::Layer::sovereign);
    check(renderer.owner(1)=="A"&&SnapshotCodec::encode(w)==snapshot,
          "palette modes must never mutate Phase C sovereign state");
}

static void test_44(){auto w=fixture();RasterIdTile t{2,2,{1,2,3,4}};
    const auto id_before=t.ids;plan(w,[](SpatialStage& s){s.queue_transfer("change",{1},"B",201);});apply(w);
    check(t.ids==id_before&&w.sovereign(1)=="B","static raster unchanged");}
static void test_46(){auto w=fixture();const auto legal=w.state_borders();
    plan(w,[](SpatialStage& s){s.occupy(1,"B");},10);
    check(legal==w.state_borders()&&w.military_control(1)=="B","occupation border distinct");}
static void test_47(){RasterIdTile t{2,2,{1,2,3,4}};
    check(t.sample_lonlat(-180,90)==1&&t.sample_lonlat(180,90)==2&&
          t.sample_lonlat(-180,-90)==3,"seam/endpoints");}
static void test_51(){auto w=fixture();for(Id i=1;i<=12;i++)seed_pop(w,i,100);
    const People before=w.population(w.sovereign_provinces("A"));
    plan(w,[](SpatialStage& s){s.queue_reassign("admin",3,102);});apply(w);
    check(w.population(w.sovereign_provinces("A"))==before&&
          w.population(w.provinces_in_unit(102))==400,"POP untouched by admin");}
static void test_52(){auto w=fixture();w.seed_core().seed_core().seed_actor("BUYER",125);
    const auto funds=w.core().core().cash("BUYER");
    plan(w,[](SpatialStage& s){s.queue_transfer("ceded",{1},"B",201);});apply(w);
    check(w.core().core().cash("BUYER")==funds,"money not autotransferred");}

static void test_53() {
    auto w=fixture();
    auto& core=w.seed_core().seed_core();
    core.seed_actor("EXPORTER",0);
    core.seed_actor("IMPORTER",0);
    core.seed_stock("EXPORTER","R1","steel",10);
    core.seed_market("R1","steel",5,1,20);
    w.seed_geographic_link("ROAD-R1-R7",1,7,"road");
    World::Plan first;
    first[7].push_back([](SpatialStage& s) {
        s.phase_c().ship("SHIPMENT-1","EXPORTER","IMPORTER",
                         "R1","R7","steel",5,3);
    });
    first[9].push_back([](SpatialStage& s) {
        s.queue_transfer("LAND-CESSION",{1},"B",201);
    });
    w.advance(first);
    const auto originalMarket=w.core().core().market("R1","steel").price;
    check(w.core().core().stock("TRANSIT:SHIPMENT-1","in_transit","steel")==5 &&
          !w.core().core().phase_e_shipment("SHIPMENT-1").delivered &&
          w.sovereign(1)=="A","shipment in transit before treaty");
    w.advance();
    check(w.sovereign(1)=="B"&&
          w.core().core().market("R1","steel").price==originalMarket &&
          w.geographic_link("ROAD-R1-R7").from==1 &&
          w.geographic_link("ROAD-R1-R7").to==7 &&
          w.core().core().stock("TRANSIT:SHIPMENT-1","in_transit","steel")==5,
          "sovereignty may not mint, seize or reroute cargo automatically");
    w.advance();
    w.advance();
    check(w.core().core().phase_e_shipment("SHIPMENT-1").delivered &&
          w.core().core().stock("IMPORTER","R7","steel")==5,
          "shipment still settles through Phase C inventory");
}

static void test_54(){auto w=fixture();
    plan(w,[](SpatialStage& s){s.occupy(1,"B");},10);
    check(w.month()==1001&&w.sovereign(1)=="A"&&w.military_control(1)=="B",
          "military stage10 to next opening");}
static void test_55(){auto w=fixture();w.seed_core().seed_core().seed_actor("A_FUNDS",0);
    const auto before=SnapshotCodec::encode(w);
    World::Plan p;
    p[5].push_back([](SpatialStage& s){s.phase_c().pay("A_FUNDS","NONEXIST",100);});
    p[9].push_back([](SpatialStage& s){s.queue_rename("r",101,"Changed");});
    check(fails([&]{w.advance(p);})&&before==SnapshotCodec::encode(w),
          "cross-domain atomic rollback");}
static void test_56(){auto w=fixture();plan(w,[](SpatialStage& s){s.queue_reassign("mv",3,102);});
    const auto data=SnapshotCodec::encode(w);
    auto loaded=SnapshotCodec::decode(data);
    check(SnapshotCodec::encode(loaded)==data,"snapshot roundtrip");
    check(fails([&]{auto corrupt=data;corrupt[corrupt.size()/2]^=0x55;
                         (void)SnapshotCodec::decode(corrupt);}),"checksum mismatch");}

static void test_57(){
    auto one=fixture(),four=fixture();
    seed_pop(one,1,100);seed_pop(four,1,100);
    one.seed_core().seed_core().seed_actor("SHOP",150);
    four.seed_core().seed_core().seed_actor("SHOP",150);
    const std::vector<Operation> commands={
        {"RENAME-A2",OpKind::rename,{},102,0,"NEW-A2"},
        {"MOVE-P3",OpKind::reassign,{3},102},
        {"MOVE-P2",OpKind::reassign,{2},102}
    };
    const auto run1=vic25::phasee::parallel::commit(one,commands,1);
    const auto run4=vic25::phasee::parallel::commit(four,commands,4);
    check(run1.actual_worker_count==1&&run4.actual_worker_count==3,
          "must spawn actual independent immutable preflight workers");
    one.advance();four.advance();
    check(SnapshotCodec::encode(one)==SnapshotCodec::encode(four)&&
          one.spatial().journal.size()==four.spatial().journal.size()&&
          one.population({1})==four.population({1}) &&
          one.core().core().cash("SHOP")==four.core().core().cash("SHOP"),
          "world state and event order must be exactly deterministic");
    check(fails([&]{
        (void)vic25::phasee::parallel::prepare(one,{
            {"BAD",OpKind::reassign,{1},201}
        },4);
    }),"parallel validators must reject invalid sovereign assignment");
}

static void test_58(){auto a=fixture(),b=fixture();
    for(int m=0;m<1200;++m){a.advance();b.advance();}
    check(a.month()==2200&&SnapshotCodec::encode(a)==SnapshotCodec::encode(b),
          "1200 months deterministic");}


static void test_04(){
    const auto seam=vic25::phasee::raster::direction_to_cube({-1,0,0});
    const auto north=vic25::phasee::raster::direction_to_cube({0,0,1});
    const auto south=vic25::phasee::raster::direction_to_cube({0,0,-1});
    check(seam.face==1&&north.face==4&&south.face==5,"cube faces at seam/poles");
    const auto position=vic25::phasee::raster::address({0,0,1},6);
    check(position.face==4&&position.x<64&&position.y<64,"polar quadtree key");
}
static void test_05(){
    const std::vector<bool> land={true,false,false,false,false,true};
    const auto result=vic25::phasee::raster::compile_masks(
        3,2,land,{{17,{0,5}}});
    check(result.component_count.at(17)==2&&result.base.sample(2,1)==17&&
          result.base.sample(1,0)==0,"multipart Province and NoData");
}
static void test_06(){
    const std::vector<bool> land={true,true};
    check(fails([&]{
        (void)vic25::phasee::raster::compile_masks(2,1,land,{{1,{0}},{2,{0,1}}});
    }),"overlapping source masks must fail");
    check(fails([&]{
        (void)vic25::phasee::raster::compile_masks(2,1,land,{{1,{0}}});
    }),"land gap must fail");
}
static void test_30(){
    World w(1000);
    w.seed_unit(1,"A","large-original");
    for(Id p=1;p<=3000;++p){
        w.seed_province(p,"Q"+std::to_string(p),"A",1000);
        w.seed_assignment(p,1);
    }
    World::Plan plan;
    plan[9].push_back([](SpatialStage& s){
        s.queue_create_admin("large-new",2,"large-target","A");
        for(Id p=1;p<=1000;++p)
            s.queue_reassign("large-reassign-"+std::to_string(p),p,2);
    });
    w.advance(plan);
    w.advance();
    check(w.provinces_in_unit(2).size()==1000&&
          w.provinces_in_unit(1).size()==2000 &&
          w.spatial().provinces.size()==3000,"3000-Province bulk administrative reorganization");
}
static void test_48(){
    vic25::phasee::raster::TileCache c;
    const vic25::phasee::raster::TileAddress id{1,2,0,1};
    check(!c.sample(id,0,0).has_value(),"unloaded tile cannot return stale ID");
    c.request(id);
    check(c.pending(id)&&!c.sample(id,0,0).has_value(),"requested-but-pending tile");
    c.arrive(id,{1,1,{101}});
    check(!c.pending(id)&&c.sample(id,0,0)==101,"tile arrival");

    vic25::phasee::raster::TileCache limited(2,16);
    const vic25::phasee::raster::TileAddress x{1,2,0,0},y{1,2,1,0},z{1,2,2,0};
    limited.request(x);limited.arrive(x,{1,1,{111}});
    limited.request(y);limited.arrive(y,{1,1,{222}});
    limited.mark_visible(x);
    limited.request(z);limited.arrive(z,{1,1,{333}});
    check(limited.loaded_count()==2 && limited.resident_bytes()==16 &&
          limited.eviction_count()==1 && !limited.sample(y,0,0).has_value() &&
          limited.sample(x,0,0)==111 && limited.sample(z,0,0)==333,
          "bounded tile LRU prevents runaway CPU/GPU cache growth");
}
static void test_50(){
    auto w=fixture();
    vic25::phasee::raster::DirtyProvinceSet dirt;
    dirt.mark(1,w.spatial());
    check(dirt.changed().size()==1&&dirt.edges().size()==2&&
          dirt.edges().contains({1,2})&&dirt.edges().contains({1,5}),
          "dirty only affected Province and its adjacent edges");
}

int main(){
    const std::map<int,std::function<void()>> implemented={
      {1,test_01},{2,test_02},{3,test_03},{4,test_04},{5,test_05},{6,test_06},{7,test_07},{8,test_08},
      {9,test_09},{11,test_11},{12,test_12},{13,test_13},{14,test_14},
      {15,test_15},{16,test_16},{17,test_17},{18,test_18},{19,test_19},{20,test_20},
      {21,test_21},{22,test_22},{23,test_23},{24,test_24},{25,test_25},
      {26,test_26},{27,test_27},{28,test_28},{29,test_29},{30,test_30},
      {31,test_31},{32,test_32},{33,test_33},{34,test_34},{35,test_35},{36,test_36},{37,test_37},
      {38,test_38},{39,test_39},{40,test_40},{41,test_41},{42,test_42},{43,test_43},{44,test_44},{45,test_45},
      {46,test_46},{47,test_47},{48,test_48},{50,test_50},{51,test_51},{52,test_52},{53,test_53},
      {54,test_54},{55,test_55},{56,test_56},{57,test_57},{58,test_58}
    };
    int pass=0,blocked=0;
    for(int i=1;i<=60;++i) {
        const auto it=implemented.find(i);
        if(it==implemented.end()) {
            ++blocked;
            std::cout<<"EV-T"<<std::setw(2)<<std::setfill('0')<<i<<" BLOCKED\n";
            continue;
        }
        try {
            it->second();
            ++pass;
            std::cout<<"EV-T"<<std::setw(2)<<std::setfill('0')<<i<<" PASS\n";
        } catch(const std::exception& e) {
            std::cerr<<"EV-T"<<std::setw(2)<<std::setfill('0')<<i
                     <<" FAIL: "<<e.what()<<"\n";
            return 1;
        }
    }
    std::cout<<"PHASE_E_RESULT PASS="<<pass<<" BLOCKED="<<blocked
             <<" TOTAL=60 COMPLETE="<<(blocked==0?"YES":"NO")<<"\n";
    return 0; // bounded-regression green, not an all-60 acceptance claim.
}
