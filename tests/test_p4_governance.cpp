#include "vic25/p0_daily.hpp"
#include "vic25/daily_snapshot.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace vic25;
static void check(bool value,const char* message){
    if(!value)throw std::runtime_error(message);
}
static const phasec::IntegratedWorld& c(const p0::DailyGame& g){
    return g.world().engine().base().core().core();
}
static daily::DayIndex day(daily::Date d){return daily::to_day_index(d);}

struct Scenario {
    phasegh::World gh{24000};
    phaseb::Id urban{}, rural{};
    explicit Scenario(bool fixed_cycle=false) {
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"RA1","A");
        geo.seed_province(2,"RA2","A");
        geo.seed_province(3,"RB1","B");
        geo.seed_border(1,2);
        geo.seed_border(2,3);
        geo.seed_unit(11,"A","CAPITAL");
        geo.seed_unit(22,"B","CAPITAL");
        geo.seed_assignment(1,11);
        geo.seed_assignment(2,11);
        geo.seed_assignment(3,22);
        auto& core=geo.seed_core().seed_core();
        core.seed_actor("A",100);
        core.seed_actor("B",100);
        core.seed_government("A","OLD_PRESIDENT","PM_A",5,3);
        core.seed_government("B","MONARCH_B","PM_B",5,3);
        core.seed_interest_group("Workers");
        core.seed_interest_group("Business");
        core.seed_p1_constitution("A",{
            phasec::P1GovernancePrinciple::presidential_republic,
            phasec::P1PowerDistribution::universal_vote,
            phasec::P1LegislatureKind::unicameral,"act:A-constitution"});
        core.seed_p1_constitution("B",{
            phasec::P1GovernancePrinciple::monarchy,
            phasec::P1PowerDistribution::autocracy,
            phasec::P1LegislatureKind::none,"act:B-constitution"});
        for(const auto& party:std::vector<phasec::P4Party>{
                {"A-Party","A","PRESIDENT_A",100'000,true,true},
                {"B-Party","A","PRESIDENT_B",100'000,true,true},
                {"C-Party","A","PRESIDENT_C",100'000,true,true}})
            core.p4_register_party(party);
        core.p4_set_group_affinity("A-Party","Workers",1'000'000);
        core.p4_set_group_affinity("B-Party","Workers",500'000);
        core.p4_set_group_affinity("C-Party","Business",1'000'000);
        const auto election=day({2000,1,2});
        core.p4_seed_contest({"A-lower","A","national","law:house",
            phasec::P4Office::legislature,
            phasec::P4BallotMethod::list_dhondt,
            fixed_cycle?phasec::P4TermMode::fixed_cycle:
                phasec::P4TermMode::restart_after_poll,
            48,1'000'000,5,election,election,-1,true,true,false});
        core.p4_seed_contest({"A-local","A","RA1","law:local",
            phasec::P4Office::local,
            phasec::P4BallotMethod::list_dhondt,
            phasec::P4TermMode::fixed_cycle,
            24,1'000'000,3,election,election,-1,false,true,false});
        core.p4_seed_contest({"A-president","A","national","law:head",
            phasec::P4Office::head_of_state,
            phasec::P4BallotMethod::plurality,
            phasec::P4TermMode::restart_after_poll,
            60,1'000'000,1,election,election,-1,false,true,false});
        auto& pop=core.seed_population();
        const auto make=[&](const char* region,phaseb::People n,
                             phaseb::Id partition) {
            const auto hh=pop.household_profile({region,"shared","shared",n});
            const auto econ=pop.economic_profile({
                region,"worker","wage","employee",pop.default_education()});
            const auto dem=pop.demographic_profile({
                region,24000-360,phaseb::Sex::female,
                "culture","faith","citizen","free",pop.default_health(),
                phaseb::Service::civilian});
            return pop.seed_population(dem,econ,hh,n,partition);
        };
        urban=make("RA1",40,1);
        rural=make("RA2",20,2);
        (void)make("RB1",10,3);
        gh.validate();
    }
};

static void distinct_elections_party_affinity_and_checkpoint(){
    Scenario fixture;
    p0::DailyGame g(daily::World::adopt(std::move(fixture.gh),41));
    phasegh::World::Plan membership;
    membership[9].push_back([id=fixture.urban](phasegh::Stage& stage){
        stage.phase_c().join_interest_group(id,"Workers",30);
    });
    g.advance_day(membership);  // Jan 1: real workers join P3 group
    check(c(g).p4_due_contests(day({2000,1,2})).size()==3,
          "P4 must allow three independent contests on one date");
    auto live=c(g).p4_autonomous_ballot("A-lower");
    check(live.eligible==60&&live.registered==60&&live.cast==60&&
          live.votes.at("A-Party")>live.votes.at("C-Party"),
          "one interest group may support more than one party without extra voters");
    g.advance_day();  // Jan 2: actual election
    const auto& state=c(g);
    check(state.p4_history().size()==3&&state.country_election("A").eligible==60,
          "P4 national election did not reuse existing C legislature");
    check(state.p4_nonparliament_results().at("A-local").eligible==40&&
          state.p4_nonparliament_results().at("A-president").eligible==60,
          "P4 local and head elections used wrong independent electorate");
    check(state.government("A").head_of_state=="OLD_PRESIDENT",
          "head-of-state election must not be effective before next day");
    const auto competition=state.p4_competition("A");
    check(competition.observed_elections==1&&
          competition.effective_parties_x100>=100,
          "party competition must be derived from seats, not preselected");
    const auto saved=daily::SnapshotCodec::encode(g.world());
    auto restored=daily::SnapshotCodec::decode(saved);
    check(daily::SnapshotCodec::encode(restored)==saved &&
          restored.engine().base().core().core().p4_history().size()==3,
          "P4 election history or due-index did not survive checkpoint");
    p0::DailyGame replay(std::move(restored));
    g.advance_day();replay.advance_day(); // Jan 3: elected head takes office
    check(c(g).government("A").head_of_state=="PRESIDENT_A"&&
          c(g).government("B").head_of_state=="MONARCH_B"&&
          daily::SnapshotCodec::encode(g.world())==
              daily::SnapshotCodec::encode(replay.world()),
          "P4 elected head changed unrelated sovereign or lost deterministic replay");
}

static void dissolve_then_early_poll_restarts_only_one_calendar() {
    Scenario fixture;
    p0::Policies policy;
    policy.dissolutions.push_back({
        "A-lower","PRESIDENT_A","law:early-election",
        {2000,1,3},{2000,2,1}});
    p0::DailyGame g(daily::World::adopt(std::move(fixture.gh),42),policy);
    g.advance_day();
    g.advance_day(); // January 2 election
    g.advance_day(); // January 3 dissolve, new president already in office
    check(c(g).p4_parliament_dissolved("A")&&
          c(g).p4_contest("A-lower").next_poll_day==day({2000,2,1})&&
          c(g).p4_contest("A-local").next_poll_day==day({2002,1,2})&&
          c(g).p4_contest("A-president").next_poll_day==day({2005,1,2}),
          "dissolution must reschedule lower chamber only");
    const auto bytes=daily::SnapshotCodec::encode(g.world());
    p0::DailyGame replay(daily::SnapshotCodec::decode(bytes),policy);
    while(g.world().date()<daily::Date{2000,2,2}) {
        g.advance_day();replay.advance_day();
    }
    check(!c(g).p4_parliament_dissolved("A")&&
          c(g).p4_contest("A-lower").next_poll_day==day({2004,2,1})&&
          c(g).p4_history().size()==4&&
          daily::SnapshotCodec::encode(g.world())==
              daily::SnapshotCodec::encode(replay.world()),
          "early election must restart its own term and checkpoint correctly");
}

static void constitutional_reform_changes_date_on_effective_day_only() {
    Scenario fixture;
    p0::Policies policies;
    phasec::P4TermAmendment amendment;
    amendment.contest_id="A-lower";
    amendment.law_id="amendment:shorter-term";
    amendment.effective_day=day({2000,1,4});
    amendment.next_poll_day=day({2002,1,2});
    amendment.term_months=24;
    amendment.applies_to_incumbent=true;
    amendment.term_mode=phasec::P4TermMode::fixed_cycle;
    policies.election_term_reforms.push_back({
        amendment,"PM_A",{"A-Party"},{2000,1,3}});
    p0::DailyGame g(daily::World::adopt(std::move(fixture.gh),43),policies);
    g.advance_day();
    g.advance_day();
    // The vote distribution may or may not give the proposed party a majority.
    // A lawful parliamentary amendment needs the actually elected coalition.
    auto law=policies.election_term_reforms.front();
    law.supporting_parties.clear();
    for(const auto& [party,n]:c(g).country_election("A").seats)
        if(n>0)law.supporting_parties.push_back(party);
    p0::Policies lawful=policies;
    lawful.election_term_reforms.clear();
    lawful.election_term_reforms.push_back(law);
    p0::DailyGame scheduled(g.world(),lawful);
    scheduled.advance_day();  // Jan 3: parliamentary term law approved
    check(c(scheduled).p4_contest("A-lower").next_poll_day==
              day({2004,1,2})&&
          c(scheduled).p4_has_due_amendment(day({2000,1,4})),
          "constitutional law applied before its published effective day");
    scheduled.advance_day(); // Jan 4: exact-day effective law
    check(c(scheduled).p4_contest("A-lower").next_poll_day==
              day({2002,1,2})&&
          c(scheduled).p4_contest("A-lower").term_months==24&&
          c(scheduled).p4_contest("A-local").next_poll_day==
              day({2002,1,2})&&
          c(scheduled).p4_contest("A-president").next_poll_day==
              day({2005,1,2}),
          "constitutional amendment incorrectly changed unrelated election cycle");
}

static void fixed_cycle_early_poll_preserves_original_regular_date(){
    Scenario fixture(true);
    p0::Policies policy;
    policy.dissolutions.push_back({
        "A-lower","PRESIDENT_A","law:fixed-cycle-early",
        {2000,1,3},{2000,2,1}});
    p0::DailyGame g(daily::World::adopt(std::move(fixture.gh),44),policy);
    g.advance_day();
    g.advance_day();
    g.advance_day();
    while(g.world().date()<daily::Date{2000,2,2})g.advance_day();
    check(c(g).p4_contest("A-lower").next_poll_day==day({2004,1,2})&&
          c(g).p4_contest("A-local").next_poll_day==day({2002,1,2}),
          "fixed-cycle early election must preserve old regular polling date");
}

static void native_base_world_counts_due_ballot_without_p0(){
    Scenario fixture;
    auto world=daily::World::adopt(std::move(fixture.gh),45);
    world.advance_day();
    world.advance_day(); // P4 due on January 2, no P0 controller present
    const auto& c=world.engine().base().core().core();
    check(c.p4_history().size()==3&&
          c.country_election("A").registered==60&&
          c.p4_due_contests(day({2000,1,2})).empty(),
          "base daily world must not silently skip or double-count P4 election");
    world.advance_day();
    check(world.engine().base().core().core().government("A").head_of_state==
              "PRESIDENT_A",
          "P4 executive transition must work in native world without P0");
}

int main(){
    try{
        distinct_elections_party_affinity_and_checkpoint();
        dissolve_then_early_poll_restarts_only_one_calendar();
        constitutional_reform_changes_date_on_effective_day_only();
        fixed_cycle_early_poll_preserves_original_regular_date();
        native_base_world_counts_due_ballot_without_p0();
        std::cout<<"P4 election governance: five integration scenarios PASS\n";
        return 0;
    }catch(const std::exception& ex){
        std::cerr<<"P4 GOVERNANCE FAILED: "<<ex.what()<<"\n";
        return 1;
    }
}
