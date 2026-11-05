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
        c.seed_production_facility("MILL_SITE","MILL","R1",31);
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
    Fixture production;
    auto prod=World::adopt(std::move(production.gh));
    auto overproduction=cmd("OVER-DAY-CAPACITY",Kind::manufacture);
    overproduction.from="REC";overproduction.amount=2;
    prod.enqueue({2000,1,1},overproduction);
    const auto prod_before=SnapshotCodec::encode(prod);
    need(reject([&]{prod.advance_day();}) &&
         prod_before==SnapshotCodec::encode(prod),
         "D monthly installed capacity must not be granted anew each day");
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
static void daily_diplomacy_changes_real_trade_tariff() {
    const auto seed=[] {
        Fixture f;
        auto& c=f.gh.seed_geography().seed_core().seed_core();
        c.seed_market("R1","iron",10,1,15);
        c.seed_market("R2","iron",20,1,30);
        f.gh.seed_tariff("B","iron",100000);
        f.gh.seed_route({"TRADE-IRON","SELLER","BUYER","iron",1,2,3,1,0});
        return World::adopt(std::move(f.gh));
    };
    auto exempt=seed(),taxed=seed();
    auto proposal=cmd("T-DUTY",Kind::treaty_offer);
    proposal.from="A";proposal.to="B";proposal.region="TAX-TREATY";
    proposal.good="iron";proposal.destination="PM_A";proposal.auxiliary=3;
    exempt.enqueue({2000,1,1},proposal);
    auto approval=cmd("R-DUTY",Kind::treaty_ratify);
    approval.from="B";approval.region="TAX-TREATY";approval.destination="PM_B";
    exempt.enqueue({2000,1,2},approval);
    while(exempt.date()<Date{2000,2,1})exempt.advance_day();
    while(taxed.date()<Date{2000,2,1})taxed.advance_day();
    need(exempt.engine().diplomacy().daily_treaties.at("TAX-TREATY").active,
         "H signed treaty not day-effective");
    const auto& x=exempt.engine().commerce().fills;
    const auto& y=taxed.engine().commerce().fills;
    need(x.size()==1&&y.size()==1 &&
         x[0].duty_paid==0&&y[0].duty_paid>0,
         "H daily effective treaty did not alter actual G tariff settlement");
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

static void conversion_only_epidemic_and_consumption(){
    Fixture fixture;
    const auto patient=fixture.death_slice;
    auto world=World::adopt(std::move(fixture.gh),2026);
    const auto original_people=ledger(world).people().population();

    auto consumption=cmd("CONVERSION-CONSUME",Kind::consumption);
    consumption.from="SELLER";consumption.region="R1";
    consumption.good="iron";consumption.amount=3;
    world.enqueue({2000,1,1},consumption);

    auto epidemic=cmd("CONVERSION-EPIDEMIC",Kind::epidemic);
    epidemic.from=std::to_string(patient);
    epidemic.amount=2;epidemic.unit_price=1; // two illnesses, one death
    world.enqueue({2000,1,1},epidemic);

    const auto report=world.advance_day();
    need(report.actions==2 && ledger(world).stock("SELLER","R1","iron")==7 &&
         ledger(world).people().population()==original_people-1 &&
         world.journal().size()==2,
         "conversion-only consumption/epidemic did not use authoritative C/B");
    const auto saved=SnapshotCodec::encode(world);
    need(SnapshotCodec::encode(SnapshotCodec::decode(saved))==saved,
         "conversion-only domain commands corrupt daily v2.0 snapshots");

    auto overuse=cmd("INVALID-CONSUMPTION",Kind::consumption);
    overuse.from="SELLER";overuse.region="R1";
    overuse.good="iron";overuse.amount=1000;
    world.enqueue({2000,1,2},overuse);
    const auto before=SnapshotCodec::encode(world);
    need(reject([&]{world.advance_day();}) &&
         SnapshotCodec::encode(world)==before,
         "invalid direct daily consumption must rollback all domains/date");
}

static void conversion_only_daily_tariff_and_authority(){
    Fixture fixture;
    fixture.gh.seed_tariff("B","iron",100'000);
    auto world=World::adopt(std::move(fixture.gh));
    auto policy=cmd("TARIFF-DAY2",Kind::tariff);
    policy.from="B";policy.to="PM_B";policy.good="iron";
    policy.amount=250'000;
    world.enqueue({2000,1,2},policy);
    world.advance_day(); // 2000-01-01: unchanged
    need(world.engine().diplomacy().tariffs.at({"B","iron"}).ppm==100'000,
         "daily tariff applied before effective date");
    world.advance_day();
    need(world.engine().diplomacy().tariffs.at({"B","iron"}).ppm==250'000 &&
         world.engine().diplomacy().scheduled_tariffs.empty(),
         "authorized daily tariff did not activate or polluted monthly schedule");

    auto illegal=cmd("TARIFF-WRONG-LEADER",Kind::tariff);
    illegal.from="B";illegal.to="PM_A";illegal.good="iron";
    illegal.amount=300'000;
    world.enqueue({2000,1,3},illegal);
    const auto before=SnapshotCodec::encode(world);
    need(reject([&]{world.advance_day();}) &&
         before==SnapshotCodec::encode(world),
         "unauthorized tariff change must rollback full daily state");
}

// A hand-authored authentic conversion-branch daily v1.1 header and metadata.
// Preserve the version/old enum tags independently from current C++ Kind.
static std::string conversion_legacy_11_fixture(const vic25::phasegh::World& gh,
                                                  const Command& command,
                                                  std::uint8_t old_kind){
    const auto original=vic25::phasegh::SnapshotCodec::encode(gh);
    std::ostringstream stream(std::ios::binary|std::ios::out);
    vic25::phasec::persistence_detail::BinaryWriter w{stream};
    w(std::string("VIC25_DAILY_SNAPSHOT_1"));
    w(std::string("vic25-daily-save/1.1"));
    w(std::string(time_contract));
    w(std::string(calendar_contract));
    w(static_cast<std::uint64_t>(original.size()));
    stream.write(original.data(),static_cast<std::streamsize>(original.size()));
    w(to_day_index({2000,1,1}));
    w(std::uint64_t{0}); // day commits
    w(std::uint64_t{14}); // deterministic seed
    w(std::string("conversion/v1.1"));
    w(std::uint64_t{0}); // original migration checksum (fixture)
    w(std::set<std::string>{command.id});
    w(std::uint64_t{1}); // one scheduled date
    w(to_day_index({2000,1,1}));
    w(std::uint64_t{1}); // one command in the date
    w(command.id);w(old_kind);w(command.from);w(command.to);
    w(command.region);w(command.good);w(command.amount);
    w(command.unit_price);w(command.destination);
    w(std::uint64_t{0}); // no completed journal entries
    const auto payload=stream.str();
    std::ostringstream tail(std::ios::binary|std::ios::out);
    vic25::phasec::persistence_detail::BinaryWriter writer{tail};
    writer(vic25::phasec::persistence_detail::checksum(payload));
    return payload+tail.str();
}

static void conversion_legacy_11_safe_migration(){
    Fixture fixture;
    auto consumption=cmd("LEGACY11-CONSUME",Kind::consumption);
    consumption.from="SELLER";consumption.region="R1";
    consumption.good="iron";consumption.destination="R2";consumption.amount=2;
    const auto old=conversion_legacy_11_fixture(fixture.gh,consumption,6);
    auto restored=SnapshotCodec::decode(old);
    need(restored.pending_count()==1 &&
         SnapshotCodec::encode(restored)!=old,
         "conversion v1.1 daily save must upgrade without altering current ABI");
    const auto v2=SnapshotCodec::encode(restored);
    need(SnapshotCodec::encode(SnapshotCodec::decode(v2))==v2,
         "conversion v1.1->v2.0 save/reload did not preserve commands");
    restored.advance_day();
    need(ledger(restored).stock("SELLER","R1","iron")==8 &&
         restored.journal().back().kind==Kind::consumption,
         "legacy v1.1 numeric tag 6 must not become a current production command");

    auto epidemic=cmd("LEGACY11-EPIDEMIC",Kind::epidemic);
    epidemic.from=std::to_string(fixture.death_slice);
    epidemic.amount=1;epidemic.unit_price=1;
    auto illness=SnapshotCodec::decode(
        conversion_legacy_11_fixture(fixture.gh,epidemic,4));
    const auto count=ledger(illness).people().population();
    illness.advance_day();
    need(ledger(illness).people().population()==count-1 &&
         illness.journal().back().kind==Kind::epidemic,
         "legacy v1.1 epidemic tag 4 remap failed");

    auto policy=cmd("LEGACY11-TARIFF",Kind::tariff);
    policy.from="B";policy.to="PM_B";policy.good="iron";policy.amount=200'000;
    auto law=SnapshotCodec::decode(
        conversion_legacy_11_fixture(fixture.gh,policy,9));
    law.advance_day();
    need(law.engine().diplomacy().tariffs.at({"B","iron"}).ppm==200'000,
         "legacy v1.1 tariff kind must map to H-owned dated policy");

    auto unsupported=cmd("LEGACY11-ROUTELESS",Kind::cargo_dispatch);
    unsupported.from="SELLER";unsupported.to="BUYER";
    unsupported.region="R1";unsupported.destination="R2";
    unsupported.good="iron";unsupported.amount=2;
    need(reject([&] {
        (void)SnapshotCodec::decode(
            conversion_legacy_11_fixture(fixture.gh,unsupported,10));
    }),"legacy v1.1 route-free cargo must not bypass real G transport graph");
}


static void autonomous_pop_is_day_native_not_monthly_replayed() {
    Fixture f;
    vic25::phaseb::DemographyRules rules;
    rules.fertility_ppm=1'000'000; // high synthetic rate to expose time
    rules.infant_death_ppm=0;
    rules.child_death_ppm=0;
    rules.adult_death_ppm=0;
    rules.senior_death_ppm=0;
    rules.very_old_death_ppm=0;
    rules.illness_incidence_ppm=0;
    rules.accident_ppm=0;
    rules.migration_ppm=0;
    f.gh.seed_geography().seed_core().seed_core().enable_demography(rules);
    auto w=World::adopt(std::move(f.gh),891);
    const auto start=ledger(w).people().population();
    auto restored=SnapshotCodec::decode(SnapshotCodec::encode(w));
    for(int i=0;i<15;++i){w.advance_day();restored.advance_day();}
    need(ledger(w).people().population()>start &&
         w.engine().month()==24000,
         "autonomous births must change POP before MonthClose");
    need(SnapshotCodec::encode(w)==SnapshotCodec::encode(restored),
         "native daily POP exposure must replay deterministically");
    for(int i=15;i<30;++i){w.advance_day();restored.advance_day();}
    const auto january30=ledger(w).people().population();
    need(w.date()==Date{2000,1,31},"invalid Gregorian day progression");
    auto result=w.advance_day();
    restored.advance_day();
    const auto january_close=ledger(w).people().population();
    need(result.monthly_close&&result.next==Date{2000,2,1} &&
         january_close>=january30 &&
         january_close-january30<=2,
         "MonthClose must not replay an entire month of daily POP births");
    need(SnapshotCodec::encode(w)==SnapshotCodec::encode(restored),
         "native day POP and legacy close must remain deterministic");
}


static void autonomous_daily_trade_budget_and_month_close() {
    Fixture f;
    auto& c=f.gh.seed_geography().seed_core().seed_core();
    c.seed_market("R1","iron",10,1,25);
    c.seed_market("R2","iron",20,1,30);
    f.gh.seed_route({"AUTO-G","SELLER","BUYER","iron",1,2,5,1,0});
    auto world=World::adopt(std::move(f.gh),4433);
    const auto start_seller=ledger(world).stock("SELLER","R1","iron");
    const auto start_buyer_cash=ledger(world).cash("BUYER");
    for(int day=1;day<=6;++day){
        world.advance_day();
        need(world.engine().commerce().fills.empty(),
             "G monthly quota allocated before real daily cumulative entitlement");
    }
    world.advance_day(); // Jan 7: floor(5*7/31) = 1
    need(world.engine().commerce().fills.size()==1 &&
         world.engine().commerce().fills.front().quantity==1 &&
         ledger(world).stock("SELLER","R1","iron")==start_seller-1,
         "G autonomous real physical shipment must execute by January 7");
    const auto snapshot=SnapshotCodec::encode(world);
    auto replay=SnapshotCodec::decode(snapshot);
    need(SnapshotCodec::encode(replay)==snapshot,
         "G autonomous daily trade failed snapshot roundtrip");
    while(world.date()<Date{2000,2,1}){
        world.advance_day();
        replay.advance_day();
    }
    need(SnapshotCodec::encode(world)==SnapshotCodec::encode(replay),
         "G trade events must replay deterministically across month-close");
    vic25::Quantity total=0;
    for(const auto& fill:world.engine().commerce().fills){
        if(fill.month==24000&&fill.route_id=="AUTO-G")total+=fill.quantity;
    }
    need(total==5 && ledger(world).stock("SELLER","R1","iron")==start_seller-5 &&
         ledger(world).cash("BUYER")<start_buyer_cash,
         "G month-end must not repeat an already-autonomous daily contract");
    world.advance_day(); // Feb 1 resets G monthly edge allowances on first DAY.
    need(world.engine().commerce().fills.size()==5,
         "G must not grant next month's complete volume on February 1");
}

int main(){
    try{
        cross_domain_daily_events_and_replay();
        daily_rollback_due_to_resource_constraints();
        migrates_old_daily_v1_without_inventing_actions();
        daily_diplomacy_changes_real_trade_tariff();
        autonomous_pop_is_day_native_not_monthly_replayed();
        conversion_only_epidemic_and_consumption();
        conversion_only_daily_tariff_and_authority();
        conversion_legacy_11_safe_migration();
        autonomous_daily_trade_budget_and_month_close();
        std::cout<<"NATIVE_DAILY_BCDGH: 3 domains groups PASS\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"NATIVE_DAILY_BCDGH FAILED: "<<e.what()<<"\n";
        return 1;
    }
}
