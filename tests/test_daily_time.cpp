#include "vic25/daily_snapshot.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
using namespace vic25::daily;

static void check(bool yes,const char* message) {
    if(!yes)throw std::runtime_error(message);
}
template<class Fn>static bool rejects(Fn&& f) {
    try{f();}catch(const std::exception&){return true;}
    return false;
}
static vic25::phasegh::World base_world(std::int64_t month=24000) {
    vic25::phasegh::World world(month);
    auto& geography=world.seed_geography();
    geography.set_dataset("daily-synthetic-1");
    geography.seed_province(1,"R1","COUNTRY-A");
    geography.seed_unit(1,"COUNTRY-A","STATE-A");
    geography.seed_assignment(1,1);
    auto& c=geography.seed_core().seed_core();
    c.seed_actor("SELLER",200);
    c.seed_actor("BUYER",100);
    c.seed_stock("SELLER","R1","steel",10);
    world.validate();
    return world;
}
static const vic25::phasec::IntegratedWorld& ledger(const World& world) {
    return world.engine().base().core().core();
}
static void calendar_exact() {
    check(leap(2000)&&!leap(1900)&&!leap(2100)&&leap(2400),
          "Gregorian leap centuries");
    check(days_in_month(2000,2)==29&&days_in_month(1900,2)==28,
          "February lengths");
    for(Date day:std::array<Date,7>{{
        {1801,1,1},{1836,12,31},{1900,2,28},{2000,2,29},
        {2026,11,5},{2028,2,29},{9999,12,31}
    }})
        check(from_day_index(to_day_index(day))==day,
              "calendar day roundtrip");
    check(following_day({1900,2,28})==Date{1900,3,1} &&
          following_day({2000,2,28})==Date{2000,2,29} &&
          following_day({2000,2,29})==Date{2000,3,1} &&
          following_day({2026,12,31})==Date{2027,1,1},
          "Gregorian rollover");
    check(rejects([]{(void)to_day_index({1900,2,29});}) &&
          rejects([]{(void)to_day_index({0,1,1});}),
          "invalid calendar accepted");
    check(from_legacy_month(24000)==Date{2000,1,1} &&
          legacy_month({2000,1,1})==24000 &&
          from_legacy_month(22032)==Date{1836,1,1},
          "legacy month ordinal / exact month opening");
}
static void daily_real_ledgers_and_month_close() {
    auto d=World::adopt(base_world());
    check(d.date()==Date{2000,1,1} && d.engine().month()==24000,
          "daily import opening date");
    d.enqueue({2000,1,1},{"trade-1",Kind::trade,
                         "SELLER","BUYER","R1","steel",3,5});
    d.enqueue({2000,1,2},{"payment-2",Kind::payment,
                         "BUYER","SELLER","","",5});
    d.enqueue({2000,1,3},{"occupation-3",Kind::military_control,
                         "","COUNTRY-B","R1",""});
    const auto trade=d.advance_day();
    check(trade.actions==1&&!trade.monthly_close&&
          d.engine().month()==24000 &&
          ledger(d).cash("SELLER")==215 &&
          ledger(d).cash("BUYER")==85 &&
          ledger(d).stock("BUYER","R1","steel")==3 &&
          ledger(d).stock("SELLER","R1","steel")==7,
          "daily actual cash/stock must settle once on their due date");
    const auto paid=d.advance_day();
    check(paid.actions==1&&ledger(d).cash("BUYER")==80&&
          ledger(d).cash("SELLER")==220 &&
          d.engine().month()==24000,"real daily payment");
    d.advance_day();
    check(ledger(d).military_control("R1")=="COUNTRY-B" &&
          ledger(d).sovereignty("R1")=="COUNTRY-A",
          "daily military control != legal sovereignty");
    for(int n=4;n<=30;n++) {
        check(d.date().day==n,"unexpected day progression");
        auto report=d.advance_day();
        check(!report.monthly_close && d.engine().month()==24000,
              "legacy month engine ran before the real month end");
    }
    check(d.date()==Date{2000,1,31},"last day before month close");
    auto closed=d.advance_day();
    check(closed.monthly_close&&d.date()==Date{2000,2,1}&&
          d.engine().month()==24001 &&
          ledger(d).stock("BUYER","R1","steel")==3 &&
          ledger(d).cash("BUYER")==80 &&
          d.journal().size()==3 &&
          d.day_commit_id()==31,"month-close must not duplicate daily effects");
    const auto prior=d.engine().commit_id();
    d.advance_day();
    check(d.engine().month()==24001&&d.engine().commit_id()==prior,
          "Feb 1 must not trigger a second Jan settlement");
}
static void daily_rejection_and_atomic_month_rollback() {
    auto d=World::adopt(base_world());
    d.enqueue({2000,1,1},{"bad",Kind::payment,"BUYER","SELLER","","",999999});
    const auto original=SnapshotCodec::encode(d);
    check(rejects([&]{d.advance_day();}),"unfunded daily command must fail");
    check(SnapshotCodec::encode(d)==original&&
          d.date()==Date{2000,1,1}&&d.pending_count()==1,
          "failed day must keep authoritative state, queue, date, IDs");
    check(rejects([&]{
        d.enqueue({2000,1,2},{"bad",Kind::payment,"BUYER","SELLER","","",1});
    }),"duplicate daily ID");
    auto good=World::adopt(base_world());
    for(int day=1;day<31;++day)good.advance_day();
    good.enqueue({2000,1,31},{"last-day",Kind::payment,
                              "BUYER","SELLER","","",2});
    const auto before=SnapshotCodec::encode(good);
    vic25::phasegh::World::Plan plan;
    plan[7].push_back([](vic25::phasegh::Stage& stage){
        stage.phase_c().pay("BUYER","SELLER",10000000);
    });
    check(rejects([&]{good.advance_day(plan);}) &&
          SnapshotCodec::encode(good)==before,
          "failed month-close must also rollback all that day's money");
    const auto success=good.advance_day();
    check(success.monthly_close&&ledger(good).cash("BUYER")==98 &&
          good.date()==Date{2000,2,1},"retry after rollback must be safe");
    check(rejects([&]{
        vic25::phasegh::World::Plan wrong;
        wrong[5].push_back([](auto&){});
        good.advance_day(wrong);
    }),"invalid monthly callback on nonmonth day must not be silently discarded");
}
static void exact_legacy_migration() {
    const auto legacy=base_world();
    const auto gh_bytes=vic25::phasegh::SnapshotCodec::encode(legacy);
    auto gh=Migrator::phase_gh_bytes(gh_bytes,1888);
    check(gh.migration_origin()=="phase-gh/codec" &&
          gh.migration_checksum()==vic25::phasec::persistence_detail::checksum(gh_bytes) &&
          vic25::phasegh::SnapshotCodec::encode(gh.engine())==gh_bytes,
          "G/H must migrate byte-identically without fabricated day history");
    const auto e_bytes=vic25::phasee::SnapshotCodec::encode(legacy.base());
    auto e=Migrator::phase_e_bytes(e_bytes);
    check(vic25::phasee::SnapshotCodec::encode(e.engine().base())==e_bytes,
          "E original world state not preserved");
    const auto d_bytes=vic25::phased::SnapshotCodec::encode(legacy.base().core());
    auto d=Migrator::phase_d_bytes(d_bytes);
    check(vic25::phased::SnapshotCodec::encode(d.engine().base().core())==d_bytes,
          "D original ledger state not preserved");
    const auto& c=legacy.base().core().core();
    const auto c_payload=vic25::phasec::SnapshotCodec::encode(c);
    auto migrated_c=Migrator::phase_c_payload(c_payload);
    check(vic25::phasec::SnapshotCodec::encode(
              migrated_c.engine().base().core().core())==c_payload,
          "C true financial/population state not preserved");
    vic25::phaseb::World population(24000);
    auto from_b=Migrator::phase_b_in_memory(std::move(population));
    check(from_b.date()==Date{2000,1,1} &&
          from_b.engine().base().core().core().people().population()==0,
          "B in-memory migration must not invent population");
    std::string damaged=gh_bytes;
    damaged[damaged.size()/2]^=0x14;
    check(rejects([&]{(void)Migrator::phase_gh_bytes(damaged);}),
          "corrupt monthly codec must never be silently migrated");
    check(rejects([&]{(void)SnapshotCodec::decode(gh_bytes);}),
          "unversioned migration through daily loader must be rejected");
}
static void save_load_deterministic_replay() {
    auto a=World::adopt(base_world(),98765),b=World::adopt(base_world(),98765);
    for(auto* w:{&a,&b}) {
        w->enqueue({2000,2,5},{"daily-pay",Kind::payment,
                               "BUYER","SELLER","","",9});
        w->enqueue({2000,3,1},{"day-60",Kind::military_control,
                               "","COUNTRY-B","R1",""});
    }
    for(int i=0;i<40;i++) {
        a.advance_day();b.advance_day();
    }
    check(SnapshotCodec::encode(a)==SnapshotCodec::encode(b),
          "deterministic day replay after 40 days");
    const auto snapshot=SnapshotCodec::encode(a);
    auto restored=SnapshotCodec::decode(snapshot);
    check(SnapshotCodec::encode(restored)==snapshot,
          "day snapshot full metadata/core roundtrip");
    for(int i=0;i<25;i++) {
        a.advance_day();restored.advance_day();
        check(SnapshotCodec::encode(a)==SnapshotCodec::encode(restored),
              "resume/replay after C/D/E/G/H checkpoint diverged");
    }
    std::string corrupted=snapshot;
    corrupted[corrupted.size()/2]^=0x10;
    check(rejects([&]{(void)SnapshotCodec::decode(corrupted);}),
          "daily snapshot must validate outer checksum");
    const auto path=std::filesystem::temp_directory_path()/
        "vic25-daily-time-v2-test.snapshot";
    std::filesystem::remove(path);
    SnapshotCodec::save(a,path);
    check(rejects([&]{SnapshotCodec::save(a,path);}),
          "must never overwrite old checkpoint");
    auto disk=SnapshotCodec::load(path);
    check(SnapshotCodec::encode(disk)==SnapshotCodec::encode(a),
          "disk save/load corrupt");
    std::filesystem::remove(path);
}
static void no_op_365_days_does_not_recalculate_monthly() {
    auto d=World::adopt(base_world(24000));
    const auto before_cash=ledger(d).cash("BUYER");
    const auto before_stock=ledger(d).stock("SELLER","R1","steel");
    std::uint64_t closes=0;
    for(int i=0;i<365;i++) {
        auto result=d.advance_day();
        if(result.monthly_close)++closes;
    }
    check(closes==11&&d.date()==Date{2000,12,31}&&
          ledger(d).cash("BUYER")==before_cash &&
          ledger(d).stock("SELLER","R1","steel")==before_stock,
          "no daily replication of monthly economic postings");
    check(d.day_commit_id()==365,"world-visible tick is exactly one day");
}
int main() {
    try {
        calendar_exact();
        daily_real_ledgers_and_month_close();
        daily_rejection_and_atomic_month_rollback();
        exact_legacy_migration();
        save_load_deterministic_replay();
        no_op_365_days_does_not_recalculate_monthly();
        std::cout<<"DAILY_TIME_V2_REAL_ENGINE: 6 groups PASS\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"DAILY_TIME_V2 FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
