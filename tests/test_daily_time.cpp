#include "vic25/daily_snapshot.hpp"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <string>
#include <utility>
using namespace vic25::daily;
// Breaking API change: exactly ONE stage-valid plan on the current date.
// Prevent ever restoring a (month_close_plan,native_plan) overload.
using DayPlan=vic25::phasegh::World::Plan;
static_assert(std::is_invocable_v<decltype(&World::advance_day),
                                  World&,const DayPlan&>);
static_assert(!std::is_invocable_v<decltype(&World::advance_day),
                                   World&,const DayPlan&,const DayPlan&>,
              "MonthClose callback adapter must not be reintroduced");

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
    const auto cash_before=ledger(d).cash("BUYER");
    const auto stock_before=ledger(d).stock("BUYER","R1","steel");
    const auto journal_before=d.journal().size();
    const auto feb1=d.advance_day();
    // Native month-open runs on Feb 1. It is one legitimate day commit,
    // not a second Jan MonthClose or repeated January economic transaction.
    check(!feb1.monthly_close && d.date()==Date{2000,2,2} &&
          d.engine().month()==24001 &&
          d.engine().commit_id()==prior+1 &&
          ledger(d).cash("BUYER")==cash_before &&
          ledger(d).stock("BUYER","R1","steel")==stock_before &&
          d.journal().size()==journal_before,
          "Feb 1 must open February without replaying January transactions");
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
    const auto previous_commit=good.engine().commit_id();
    const auto success=good.advance_day();
    check(success.monthly_close&&
          good.engine().commit_id()==previous_commit+1 &&
          ledger(good).cash("BUYER")==98 &&
          good.date()==Date{2000,2,1},"retry after rollback must be safe");
    // The old month-close callback route was deleted. A Plan is always
    // today's native stage work, including the first day of any month.
    const auto commit_before_feb=good.engine().commit_id();
    vic25::phasegh::World::Plan feb1;
    feb1[5].push_back([](vic25::phasegh::Stage& gh){
        gh.phase_c().pay("BUYER","SELLER",1);
    });
    const auto report=good.advance_day(feb1);
    check(!report.monthly_close && good.date()==Date{2000,2,2} &&
          good.engine().commit_id()==commit_before_feb+1 &&
          ledger(good).cash("BUYER")==97,
          "native callback must execute Feb 1, never defer to MonthClose");
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
    const auto legacy_c_path=std::filesystem::temp_directory_path()/
        "vic25-daily-legacy-phase-c-checkpoint.bin";
    std::filesystem::remove(legacy_c_path);
    vic25::phasec::SnapshotCodec::save(c,legacy_c_path);
    std::ifstream legacy_input(legacy_c_path,std::ios::binary);
    const std::string old_saved_bytes{std::istreambuf_iterator<char>{legacy_input},
                                      std::istreambuf_iterator<char>{}};
    legacy_input.close();
    auto migrated_c_file=Migrator::phase_c_file(legacy_c_path,54321);
    check(migrated_c_file.migration_checksum()==
              vic25::phasec::persistence_detail::checksum(old_saved_bytes) &&
          vic25::phasec::SnapshotCodec::encode(
              migrated_c_file.engine().base().core().core())==c_payload,
          "C disk checkpoint migration must preserve payload and source checksum");
    std::filesystem::remove(legacy_c_path);
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
static void quiet_days_process_only_due_regional_decree_changes() {
    using vic25::phasec::P1GovernancePrinciple;
    using vic25::phasec::P1PowerDistribution;
    using vic25::phasec::P1LegislatureKind;
    using vic25::phasec::P1MandateKind;
    auto baseline=base_world();
    auto& c=baseline.seed_geography().seed_core().seed_core();
    c.seed_government("COUNTRY-A","HEAD","PM",5,3);
    c.seed_p1_constitution("COUNTRY-A",{P1GovernancePrinciple::monarchy,
        P1PowerDistribution::autocracy,P1LegislatureKind::none,"fixture:constitution"});
    auto world=World::adopt(std::move(baseline),23);
    world.advance_day(); // first-of-month native work intentionally runs
    const auto skip_quiet=[&](World& w) {
        const auto engine_commit=w.engine().commit_id();
        const auto calendar_commit=w.day_commit_id();
        w.advance_day();
        check(w.engine().commit_id()==engine_commit&&w.day_commit_id()==calendar_commit+1,
              "an irrelevant quiet day lost the fast path");
    };
    skip_quiet(world);
    const auto adopted=world.day_index();
    DayPlan schedule;
    schedule[9].push_back([adopted](vic25::phasegh::Stage& stage) {
        stage.phase_c().p1_schedule_mandate({"ROAD","COUNTRY-A","R1","fixture:roads",
            P1MandateKind::decree_road_maintenance},"PM",adopted);
    });
    world.advance_day(schedule);
    const std::string modifier="state_construction_mult";
    check(ledger(world).p1_active_mandates().empty()&&
          ledger(world).p1_local_decree_ppm("COUNTRY-A","R1",modifier)==0,
          "regional decree applied before its effective day");

    auto restored=SnapshotCodec::decode(SnapshotCodec::encode(world));
    const auto process_due=[&](World& a,World& b) {
        const auto old_commit=a.engine().commit_id();
        a.advance_day();b.advance_day();
        check(a.engine().commit_id()==old_commit+1,
              "idle fast path skipped a due regional decree change");
        check(SnapshotCodec::encode(a)==SnapshotCodec::encode(b),
              "pending regional decree did not replay from its saved date");
    };
    process_due(world,restored);
    const auto& road=ledger(world).p1_active_mandates().at("ROAD");
    check(road.effective_day==adopted+1&&
          ledger(world).p1_local_decree_ppm("COUNTRY-A","R1",modifier)==100'000&&
          ledger(world).p1_authority_capacity("COUNTRY-A").committed==100,
          "quiet effective day did not activate the real authority-financed decree");
    skip_quiet(world); // an active decree without a due change is not daily work

    const auto revoked=world.day_index();
    DayPlan withdrawal;
    withdrawal[9].push_back([revoked](vic25::phasegh::Stage& stage) {
        stage.phase_c().p1_schedule_mandate_termination("COUNTRY-A","ROAD","PM",revoked);
    });
    world.advance_day(withdrawal);
    check(ledger(world).p1_active_mandates().contains("ROAD"),
          "decree withdrawal took effect before its lawful date");
    restored=SnapshotCodec::decode(SnapshotCodec::encode(world));
    process_due(world,restored);
    check(ledger(world).p1_active_mandates().empty()&&
          ledger(world).p1_local_decree_ppm("COUNTRY-A","R1",modifier)==0&&
          ledger(world).p1_authority_capacity("COUNTRY-A").committed==0,
          "quiet dated withdrawal did not release the actual policy and authority");
    skip_quiet(world);
}

static void quiet_calendar_preserves_authoritative_infamy_decay() {
    namespace gh=vic25::phasegh;
    namespace b=vic25::phaseb;
    auto opening=base_world();
    auto& geo=opening.seed_geography();
    geo.seed_province(2,"R2","COUNTRY-B");geo.seed_unit(2,"COUNTRY-B","STATE-B");
    geo.seed_assignment(2,2);geo.seed_border(1,2);
    auto& c=geo.seed_core().seed_core();
    c.seed_government("COUNTRY-A","HEAD_A","PM_A",5,3);
    c.seed_government("COUNTRY-B","HEAD_B","PM_B",5,3);
    for(const std::string country:{"COUNTRY-A","COUNTRY-B"}) {
        const auto region=country=="COUNTRY-A"?"R1":"R2";
        const auto unit="SCENARIO-UNIT-"+country;
        c.seed_unit(unit);c.seed_army_deployment(unit,country,region);
        c.seed_stock(unit,region,"ammunition",2);
        auto& pop=c.seed_population();
        const auto h=pop.household_profile({region,"shared","shared",2});
        const auto e=pop.economic_profile({region,"soldier","wage","employee",
                                          pop.default_education()});
        const auto d=pop.demographic_profile({region,23640,b::Sex::male,
            "culture","faith","citizen","free",pop.default_health(),b::Service::active});
        c.seed_active(unit,pop.seed_population(d,e,h,2));
    }
    auto world=World::adopt(std::move(opening),21);
    DayPlan claim;
    claim[9].push_back([](gh::Stage& stage) {
        stage.geography().queue_claim("NATIVE-CALENDAR-CLAIM",2,"COUNTRY-A",true);
    });
    world.advance_day(claim);
    while(world.date()<Date{2000,2,1})world.advance_day();
    DayPlan crisis;
    crisis[9].push_back([](gh::Stage& stage) {
        stage.open_crisis("NATIVE-CALENDAR-CRISIS",2,"COUNTRY-A","COUNTRY-B","PM_A");
    });
    world.advance_day(crisis);
    while(world.date()<Date{2000,3,1})world.advance_day();
    DayPlan escalate;
    escalate[9].push_back([](gh::Stage& stage) {
        check(stage.begin_war_from_crisis("NATIVE-CALENDAR-CRISIS","COUNTRY-A","PM_A"),
              "fixture did not record an actual game crisis escalation");
    });
    const auto incident_day=world.day_index();
    world.advance_day(escalate);
    check(world.engine().infamy_view("COUNTRY-A").value_ppm==1'000'000&&
          world.engine().diplomacy().infamy_events.size()==1,
          "crisis did not add its accounted native H incident");
    DayPlan tick;
    tick[0].push_back([](gh::Stage&) {});
    auto forced=SnapshotCodec::decode(SnapshotCodec::encode(world));
    world.advance_day();forced.advance_day(tick);
    world=SnapshotCodec::decode(SnapshotCodec::encode(world));
    world.advance_day();forced.advance_day(tick);
    world.advance_day(tick);forced.advance_day(tick);
    const auto actual=world.engine().infamy_view("COUNTRY-A");
    check(actual.last_day==incident_day+3&&actual.value_ppm<1'000'000&&
          actual.value_ppm>0&&SnapshotCodec::encode(world)==SnapshotCodec::encode(forced),
          "unforced active H calendar lost decay or differed from explicit save/replay");
}

static void quiet_calendar_skips_empty_infamy_without_losing_later_incidents() {
    namespace gh=vic25::phasegh;
    auto opening=base_world();
    opening.seed_geography().seed_core().seed_core().seed_government(
        "COUNTRY-A","HEAD_A","PM_A",5,3);
    auto world=World::adopt(std::move(opening),22);
    DayPlan incident,tick,next_incident;
    incident[9].push_back([](gh::Stage& stage) {
        stage.record_infamy("small","COUNTRY-A","fixture:recorded diplomatic incident",1);
    });
    tick[0].push_back([](gh::Stage&) {});
    next_incident[9].push_back([](gh::Stage& stage) {
        stage.record_infamy("new","COUNTRY-A","fixture:second recorded diplomatic incident",1'000'000);
    });
    world.advance_day(incident);
    world.advance_day(tick);
    check(world.engine().infamy_view("COUNTRY-A").value_ppm==0&&
          world.engine().diplomacy().infamy.at("COUNTRY-A").carry==0,
          "fixture did not exhaust both balance and fractional carry");
    auto forced=SnapshotCodec::decode(SnapshotCodec::encode(world));
    const auto before=world.engine().commit_id();
    world.advance_day();world.advance_day();
    check(world.engine().commit_id()==before,
          "dormant zero Infamy must not require native calendar commits");
    forced.advance_day(tick);forced.advance_day(tick);
    world=SnapshotCodec::decode(SnapshotCodec::encode(world));
    world.advance_day(next_incident);forced.advance_day(next_incident);
    world.advance_day();forced.advance_day(tick);
    const auto& actual=world.engine().diplomacy().infamy.at("COUNTRY-A");
    const auto& expected=forced.engine().diplomacy().infamy.at("COUNTRY-A");
    check(actual.value_ppm==expected.value_ppm&&actual.carry==expected.carry&&
          actual.last_day==expected.last_day&&actual.value_ppm<1'000'000&&
          world.engine().diplomacy().infamy_events.size()==2,
          "idle zero carry changed later native incident or lost its decay");
}

// Old 2.0 bare idle advances updated just these two outer fields. Keep
// nested H bytes exact (including fractional carry and incident journal).
static std::string infamy_snapshot_with_calendar_gap(
        const std::string& bytes,const char* version,int skipped_days) {
    auto payload=bytes.substr(0,bytes.size()-8);
    std::istringstream in(payload,std::ios::binary);
    detail::Reader r{in};
    std::string magic,old_version,time,calendar;
    r(magic);r(old_version);r(time);r(calendar);
    std::uint64_t n{};r(n);
    in.seekg(static_cast<std::streamoff>(n),std::ios::cur);
    const auto clock_offset=static_cast<std::size_t>(in.tellg());
    DayIndex day{};std::uint64_t commits{};r(day);r(commits);
    const auto clock_end=static_cast<std::size_t>(in.tellg());
    std::ostringstream prefix(std::ios::binary);
    detail::Writer header{prefix};
    header(magic);header(std::string(version));header(time);header(calendar);
    const auto prefix_bytes=prefix.str();
    payload.replace(0,prefix_bytes.size(),prefix_bytes);
    std::ostringstream clock(std::ios::binary);
    detail::Writer head{clock};
    head(day+skipped_days);
    head(static_cast<std::uint64_t>(static_cast<std::int64_t>(commits)+skipped_days));
    payload.replace(clock_offset,clock_end-clock_offset,clock.str());
    std::ostringstream tail(std::ios::binary);
    detail::Writer checksum{tail};
    checksum(vic25::phasec::persistence_detail::checksum(payload));
    return payload+tail.str();
}
static void old_infamy_snapshot_resumes_without_inventing_past_decay() {
    namespace gh=vic25::phasegh;
    auto opening=base_world();
    opening.seed_geography().seed_core().seed_core().seed_government(
        "COUNTRY-A","HEAD_A","PM_A",5,3);
    auto world=World::adopt(std::move(opening),23);
    DayPlan incident,tick;
    incident[9].push_back([](gh::Stage& stage) {
        stage.record_infamy("retained","COUNTRY-A","fixture:recorded diplomatic incident",1'000'000);
    });
    tick[0].push_back([](gh::Stage&) {});
    world.advance_day(incident);world.advance_day(tick);
    const auto prior=world.engine().diplomacy().infamy.at("COUNTRY-A");
    check(prior.value_ppm>0&&prior.carry>0,"fixture must preserve nonzero balance and carry");
    const auto clean=SnapshotCodec::encode(world);
    const auto old=infamy_snapshot_with_calendar_gap(clean,"vic25-daily-save/2.0",2);
    auto restored=SnapshotCodec::decode(old);
    const auto& resume=restored.engine().diplomacy().infamy.at("COUNTRY-A");
    check(restored.date()==Date{2000,1,5}&&resume.value_ppm==prior.value_ppm&&
          resume.carry==prior.carry&&resume.last_day==restored.day_index()-1&&
          restored.engine().diplomacy().infamy_events.size()==1,
          "legacy resume changed recorded Infamy, inferred history or left the clock stale");
    const auto migrated=SnapshotCodec::encode(restored);
    check(migrated.find("vic25-daily-save/2.1")!=std::string::npos&&
          SnapshotCodec::encode(SnapshotCodec::decode(migrated))==migrated&&
          rejects([&]{(void)SnapshotCodec::decode(infamy_snapshot_with_calendar_gap(
              migrated,"vic25-daily-save/2.1",2));})&&
          rejects([&]{(void)SnapshotCodec::decode(infamy_snapshot_with_calendar_gap(
              clean,"vic25-daily-save/2.0",-1));}),
          "active snapshot version must reject new gaps and impossible future timestamps");
    const auto day=restored.day_index();
    auto replay=SnapshotCodec::decode(migrated);
    restored.advance_day();replay.advance_day(tick);
    const auto& after=restored.engine().diplomacy().infamy.at("COUNTRY-A");
    check(after.value_ppm==prior.value_ppm-(5'000'000+prior.carry)/366&&
          after.carry==(5'000'000+prior.carry)%366&&after.last_day==day&&
          SnapshotCodec::encode(restored)==SnapshotCodec::encode(replay),
          "first prospective day did not preserve exact saved carry and replay");
}

int main() {
    try {
        calendar_exact();
        quiet_calendar_preserves_authoritative_infamy_decay();
        quiet_calendar_skips_empty_infamy_without_losing_later_incidents();
        old_infamy_snapshot_resumes_without_inventing_past_decay();
        quiet_days_process_only_due_regional_decree_changes();
        daily_real_ledgers_and_month_close();
        daily_rejection_and_atomic_month_rollback();
        exact_legacy_migration();
        save_load_deterministic_replay();
        no_op_365_days_does_not_recalculate_monthly();
        std::cout<<"DAILY_TIME_V2_REAL_ENGINE: 10 groups PASS\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"DAILY_TIME_V2 FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
