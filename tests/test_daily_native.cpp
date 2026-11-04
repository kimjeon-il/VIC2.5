#include "vic25/daily_snapshot.hpp"
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
using namespace vic25::daily;
using vic25::phaseb::Sex;
using vic25::phaseb::Service;
static void need(bool truth,const char* why){if(!truth)throw std::runtime_error(why);}
template<class F>static bool reject(F&& work){
    try{work();}catch(const std::exception&){return true;}
    return false;
}
static const vic25::phasec::IntegratedWorld& ledger(const World& w){
    return w.engine().base().core().core();
}
static const vic25::phased::World& industrial(const World& w) {
    return w.engine().base().core();
}
struct Fixture {
    vic25::phasegh::World gh{24000};
    vic25::phaseb::Id mother{},death_slice{},builder_slice{},mill_slice{};
    Fixture(){
        auto& geography=gh.seed_geography();
        geography.set_dataset("native-daily-fixture/1");
        geography.seed_province(1,"R1","A");
        geography.seed_province(2,"R2","B");
        geography.seed_border(1,2);
        geography.seed_unit(101,"A","A1");
        geography.seed_unit(201,"B","B1");
        geography.seed_assignment(1,101);
        geography.seed_assignment(2,201);
        auto& d=geography.seed_core();
        auto& c=d.seed_core();
        c.seed_actor("A",100);
        c.seed_actor("B",100);
        c.seed_actor("CLIENT",100);
        c.seed_actor("BUILDER",0);
        c.seed_actor("MILL",100);
        c.seed_actor("SELLER",100);
        c.seed_actor("BUYER",100);
        c.seed_actor("CARRIER",100);
        c.seed_government("A","H_A","PM_A",5,3);
        c.seed_government("B","H_B","PM_B",5,3);
        c.seed_stock("BUILDER","R1","stone",4);
        c.seed_stock("MILL","R1","ore",4);
        c.seed_stock("MILL","R1","coal",4);
        c.seed_stock("SELLER","R1","iron",10);
        c.seed_production_facility("MILL_SITE","MILL","R1",5);
        auto& p=c.seed_population();
        const auto h=p.household_profile({"R1","shared","shared",100});
        const auto worker=p.economic_profile({"R1","worker","wage","employee",p.default_education()});
        const auto inactive=p.economic_profile({"R1","not_assigned","inactive","ineligible",p.default_education()});
        const auto make=[&](Sex sex,int birth,vic25::phaseb::Id econ,
                            vic25::phaseb::People people,vic25::phaseb::Id partition) {
            const auto d_id=p.demographic_profile({"R1",birth,sex,
                "culture","faith","citizen","free",p.default_health(),Service::civilian});
            return p.seed_population(d_id,econ,h,people,partition);
        };
        mother=make(Sex::female,24000-360,worker,30,1);
        death_slice=make(Sex::male,24000-360,inactive,10,2);
        builder_slice=make(Sex::male,24000-360,worker,5,3);
        mill_slice=make(Sex::male,24000-360,worker,5,4);
        p.hire(builder_slice,3,"BUILDER","worker",2,{});
        p.hire(mill_slice,4,"MILL","worker",2,{});
        vic25::phased::ConstructionProject project;
        project.id="PROJECT";project.client="CLIENT";project.contractor="BUILDER";
        project.region="R1";project.material_good="stone";project.target_site="NEW_SITE";
        project.work_required=2;project.material_per_work=1;
        project.workers_per_work=1;project.target_capacity=5;
        project.cash_price_per_work=10;
        d.seed_construction(std::move(project));
        vic25::phased::IndustryRecipe recipe;
        recipe.id="REC";recipe.site="MILL_SITE";
        recipe.inputs={{"coal",1}};recipe.outputs={{"steel",2}};
        recipe.workers_per_batch=1;
        d.seed_recipe(std::move(recipe));
        gh.seed_transport_edge({"EDGE","CARRIER",1,2,310,0,1,true});
        gh.validate();
    }
};
static Command cmd(std::string id,Kind kind) {
    Command x;x.id=std::move(id);x.kind=kind;return x;
}
static void cross_domain_daily_events_and_replay(){
    Fixture f;
    auto w=World::adopt(std::move(f.gh),1234);
    const auto base_count=ledger(w).people().population();
    auto offer=cmd("T-OPEN",Kind::treaty_offer);
    offer.from="A";offer.to="B";offer.region="DIP-1";offer.good="iron";
    offer.destination="PM_A";offer.auxiliary=3;
    w.enqueue({2000,1,1},offer);
    auto birth=cmd("POP-BIRTH",Kind::pop_birth);
    birth.from=std::to_string(f.mother);birth.amount=2;
    w.enqueue({2000,1,1},birth);
    w.advance_day();
    need(ledger(w).people().population()==base_count+2,
         "daily POP birth did not affect authoritative Phase B");
    need(!w.engine().diplomacy().daily_treaties.at("DIP-1").active,
         "proposal should not activate unratified treaty");
    const auto save=SnapshotCodec::encode(w);
    w=SnapshotCodec::decode(save);
    need(SnapshotCodec::encode(w)==save,"native daily snapshot restore after day event");

    auto ratify=cmd("T-RATIFY",Kind::treaty_ratify);
    ratify.from="B";ratify.region="DIP-1";ratify.destination="PM_B";
    w.enqueue({2000,1,2},ratify);
    auto death=cmd("POP-DEATH",Kind::pop_death);
    death.from=std::to_string(f.death_slice);death.amount=1;
    w.enqueue({2000,1,2},death);
    auto produce=cmd("PROD-C",Kind::production);
    produce.from="MILL";produce.region="R1";produce.good="ore";
    produce.to="bar";produce.amount=2;produce.unit_price=2;
    w.enqueue({2000,1,2},produce);
    w.advance_day();
    need(ledger(w).people().population()==base_count+1 &&
         ledger(w).stock("MILL","R1","ore")==2 &&
         ledger(w).stock("MILL","R1","bar")==2,
         "C actual daily POP death or real manufacturing ledger not applied");

    auto make=cmd("PROD-D",Kind::manufacture);
    make.from="REC";make.amount=1;
    w.enqueue({2000,1,3},make);
    auto build=cmd("CONSTRUCTION-1",Kind::construction);
    build.from="PROJECT";build.amount=1;
    w.enqueue({2000,1,3},build);
    w.advance_day();
    need(industrial(w).construction("PROJECT").accepted_work==1 &&
         industrial(w).construction("PROJECT").paid_work==1 &&
         ledger(w).stock("BUILDER","R1","stone")==3 &&
         ledger(w).cash("CLIENT")==90 &&
         ledger(w).stock("MILL","R1","steel")==2,
         "D real daily construction/WIP/industry inputs not booked");
    auto build2=cmd("CONSTRUCTION-2",Kind::construction);
    build2.from="PROJECT";build2.amount=1;
    w.enqueue({2000,1,4},build2);
    w.advance_day();
    need(w.engine().diplomacy().daily_treaties.at("DIP-1").active,
         "H daily treaty did not activate on effective date");
    need(industrial(w).construction("PROJECT").ready &&
         !industrial(w).construction("PROJECT").commissioned,
         "D construction should await next day commission");
    w.advance_day();
    need(industrial(w).construction("PROJECT").commissioned &&
         ledger(w).facility("NEW_SITE").capacity==5 &&
         ledger(w).phase_d_wip("CLIENT")==0 &&
         ledger(w).cash("CLIENT")==80,
         "D commissioned real facility on day after completion");

    auto cargo=cmd("CARGO-1",Kind::cargo_dispatch);
    cargo.from="SELLER";cargo.to="BUYER";cargo.region="R1";
    cargo.destination="R2";cargo.good="iron";cargo.route_id="EDGE";
    cargo.amount=4;cargo.unit_price=4;cargo.auxiliary=1;
    w.enqueue({2000,1,29},cargo);
    while(w.date()<Date{2000,1,29})w.advance_day();
    w.advance_day();
    need(ledger(w).stock("SELLER","R1","iron")==6 &&
         ledger(w).stock("TRANSIT:DAY:CARGO-1","in_transit","iron")==4,
         "G/F physical shipment dispatch must move actual inventory");
    const auto midflight=SnapshotCodec::encode(w);
    auto reloaded=SnapshotCodec::decode(midflight);
    need(SnapshotCodec::encode(reloaded)==midflight &&
         reloaded.cargo().at("CARGO-1").arrival==to_day_index({2000,2,2}),
         "daily transit ETA persisted on reload");
    while(w.date()<Date{2000,2,2}){w.advance_day();reloaded.advance_day();}
    need(SnapshotCodec::encode(w)==SnapshotCodec::encode(reloaded),
         "transit + treaty + industry deterministic across monthly close");
    w.advance_day();
    reloaded.advance_day();
    need(SnapshotCodec::encode(w)==SnapshotCodec::encode(reloaded) &&
         w.cargo().at("CARGO-1").arrived &&
         ledger(w).stock("TRANSIT:DAY:CARGO-1","in_transit","iron")==0 &&
         ledger(w).stock("BUYER","R2","iron")==3,
         "exact day arrival and explicit cargo loss mismatch");
}
static void daily_rollback_due_to_resource_constraints() {
    Fixture f;auto w=World::adopt(std::move(f.gh));
    auto fail=cmd("BAD-CONSTRUCTION",Kind::construction);
    fail.from="PROJECT";fail.amount=100;
    w.enqueue({2000,1,1},fail);
    const auto before=SnapshotCodec::encode(w);
    need(reject([&]{w.advance_day();}) &&
         before==SnapshotCodec::encode(w),
         "D failed daily work must rollback all C/D/E/GH and queue metadata");

    Fixture g;auto t=World::adopt(std::move(g.gh));
    auto shipment=cmd("OVER-CAPACITY",Kind::cargo_dispatch);
    shipment.from="SELLER";shipment.to="BUYER";shipment.region="R1";
    shipment.destination="R2";shipment.good="iron";shipment.route_id="EDGE";
    shipment.amount=11;shipment.unit_price=3;shipment.auxiliary=0;
    t.enqueue({2000,1,1},shipment);
    const auto before_fail=SnapshotCodec::encode(t);
    need(reject([&]{t.advance_day();}) &&
         before_fail==SnapshotCodec::encode(t),
         "G capacity/stock failure must rollback day");
}
static void migrates_old_daily_v1_without_inventing_actions() {
    Fixture f;
    const auto gh=vic25::phasegh::SnapshotCodec::encode(f.gh);
    std::ostringstream out(std::ios::binary);
    vic25::phasec::persistence_detail::BinaryWriter w{out};
    w(std::string("VIC25_DAILY_SNAPSHOT_1"));
    w(std::string("vic25-daily-save/1.0"));
    w(std::string(time_contract));
    w(std::string(calendar_contract));
    w(static_cast<std::uint64_t>(gh.size()));
    out.write(gh.data(),static_cast<std::streamsize>(gh.size()));
    w(to_day_index({2000,1,1}));
    w(std::uint64_t{0});w(std::uint64_t{17});
    w(std::string("phase-gh/codec"));
    w(std::uint64_t{19});
    w(std::set<std::string>{});w(std::uint64_t{0});w(std::uint64_t{0});
    const auto original=out.str();
    std::ostringstream tail(std::ios::binary);
    vic25::phasec::persistence_detail::BinaryWriter check{tail};
    check(vic25::phasec::persistence_detail::checksum(original));
    const auto old=original+tail.str();
    auto migrated=SnapshotCodec::decode(old);
    need(migrated.date()==Date{2000,1,1} &&
         SnapshotCodec::encode(migrated)!=old &&
         vic25::phasegh::SnapshotCodec::encode(migrated.engine())==gh,
         "v1 daily metadata migration changed authoritative world");
}
int main(){
    try{
        cross_domain_daily_events_and_replay();
        daily_rollback_due_to_resource_constraints();
        migrates_old_daily_v1_without_inventing_actions();
        std::cout<<"NATIVE_DAILY_BCDGH: 3 domains groups PASS\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"NATIVE_DAILY_BCDGH FAILED: "<<e.what()<<"\n";
        return 1;
    }
}
