#include "vic25/p0_daily.hpp"
#include "vic25/daily_snapshot.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace vic25;

static void need(bool ok,const char* message) {
    if(!ok)throw std::runtime_error(message);
}
static daily::DayIndex day(daily::Date d) {
    return daily::to_day_index(d);
}
static const phasec::IntegratedWorld& actual(const daily::World& world) {
    return world.engine().base().core().core();
}
static phasec::P2Clause clause(
        std::string id,std::string target,phasec::P2ClauseKind type,
        std::int64_t value) {
    return {std::move(id),std::move(target),type,value};
}
struct Scenario {
    phasegh::World gh{24000};
    Scenario() {
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"RA","A");
        geo.seed_unit(11,"A","Capital");
        geo.seed_assignment(1,11);
        auto& c=geo.seed_core().seed_core();
        c.seed_actor("A",1000);
        c.seed_government("A","HEAD_A","PM_A",5,3);
        c.seed_p1_constitution("A",{
            phasec::P1GovernancePrinciple::presidential_republic,
            phasec::P1PowerDistribution::universal_vote,
            phasec::P1LegislatureKind::unicameral,"constitution:A"});
        c.p4_register_party({"PARTY_A","A","CANDIDATE_A",900'000,true,true});
        c.p4_register_party({"PARTY_B","A","CANDIDATE_B",100'000,true,true});
        const auto jan2=day({2000,1,2});
        const auto jan2004=day({2004,1,1});
        c.p4_seed_contest({"A-HOUSE","A","national","old-house-election-law",
            phasec::P4Office::legislature,
            phasec::P4BallotMethod::list_dhondt,
            phasec::P4TermMode::restart_after_poll,
            48,1'000'000,5,jan2004,jan2004,-1,true,true,false});
        c.p4_seed_contest({"A-HEAD","A","national","old-head-election-law",
            phasec::P4Office::head_of_state,
            phasec::P4BallotMethod::plurality,
            phasec::P4TermMode::restart_after_poll,
            1,1'000'000,1,jan2,jan2,-1,false,true,false});
        auto& p=c.seed_population();
        const auto h=p.household_profile({"RA","shared","shared",100});
        const auto e=p.economic_profile({
            "RA","worker","wage","employee",p.default_education()});
        const auto d=p.demographic_profile({
            "RA",24000-360,phaseb::Sex::female,"c","r",
            "citizen","free",p.default_health(),phaseb::Service::civilian});
        p.seed_population(d,e,h,100,1);
        phasec::P2BaseLaw law;
        law.id="TERM-LAW-V1";law.country="A";
        law.domain="elected_office_terms";law.scope="national";
        law.base_choice="constitutional-term-act";
        law.legal_basis="legal:term-enactment";
        law.tier=phasec::P2NormTier::statute;
        law.clauses={
            clause("house-term","A-HOUSE",phasec::P2ClauseKind::term_months,48),
            clause("house-cycle","A-HOUSE",phasec::P2ClauseKind::term_cycle_mode,0),
            clause("house-dissolve","A-HOUSE",
                   phasec::P2ClauseKind::dissolution_authorized,1),
            clause("head-term","A-HEAD",phasec::P2ClauseKind::term_months,1),
            clause("head-cycle","A-HEAD",phasec::P2ClauseKind::term_cycle_mode,0),
            clause("head-dissolve","A-HEAD",
                   phasec::P2ClauseKind::dissolution_authorized,0),
            clause("head-consecutive","A-HEAD",
                   phasec::P2ClauseKind::max_consecutive_terms,1)
        };
        c.p2_seed_base(std::move(law));
        gh.validate();
    }
};

static phasec::P2Bill bill_for(
        const std::string& bill_id,phasec::P2BillAction kind,
        std::int64_t effective) {
    phasec::P2Bill bill;
    bill.id=bill_id;bill.country="A";bill.proposer="PM_A";
    bill.action=kind;bill.effective_day=effective;
    bill.legal_basis="act:"+bill_id;
    return bill;
}

static void exact_day_law_enactment_amendment_and_repeal() {
    Scenario state;
    auto world=daily::World::adopt(std::move(state.gh),401);
    const auto start_cash=actual(world).cash("A");
    const auto start_population=actual(world).people().population();
    phasegh::World::Plan day1;
    day1[9].push_back([today=day({2000,1,1})](phasegh::Stage& gh){
        auto& c=gh.phase_c();
        c.hold_country_election("A",100,0,{{"PARTY_A",100}});
        auto bill=bill_for("EXTEND-HOUSE",phasec::P2BillAction::attach_amendment,
                           day({2000,1,3}));
        bill.proposed_amendment={
            "AMEND-HOUSE","TERM-LAW-V1","agreement:house-terms","term-overrides",
            {clause("extra-house","A-HOUSE",phasec::P2ClauseKind::term_months,60)}
        };
        c.p2_submit_bill(bill,today);
        c.p2_approve_bill("EXTEND-HOUSE",{"PARTY_A"});
        c.p2_promulgate_bill("EXTEND-HOUSE","HEAD_A",today);
    });
    world.advance_day(day1);
    need(actual(world).p2_has_due(day({2000,1,3}))&&
         actual(world).p4_contest("A-HOUSE").term_months==48,
         "P2 promulgation changed law before its effective day");

    const auto first_save=daily::SnapshotCodec::encode(world);
    auto replay=daily::SnapshotCodec::decode(first_save);
    need(daily::SnapshotCodec::encode(replay)==first_save,
         "P2 pending amendment was not saved byte-for-byte");
    world.advance_day();replay.advance_day(); // Jan 2; head election
    need(actual(world).p4_contest("A-HOUSE").term_months==48,
         "P2 law became effective a day early");
    world.advance_day();replay.advance_day(); // Jan 3; amendment
    need(actual(world).p4_contest("A-HOUSE").term_months==60&&
         actual(world).p2_amendment_active("AMEND-HOUSE")&&
         actual(world).p4_contest("A-HOUSE").next_poll_day==
             day({2004,1,1})&&
         daily::SnapshotCodec::encode(world)==
             daily::SnapshotCodec::encode(replay),
         "P2 amendment failed to preserve incumbent term and deterministic replay");

    phasegh::World::Plan revoke;
    revoke[9].push_back([today=day({2000,1,4})](phasegh::Stage& gh){
        auto& c=gh.phase_c();
        auto bill=bill_for("REVOKE-HOUSE",
                          phasec::P2BillAction::repeal_amendment,
                          day({2000,1,5}));
        bill.target_id="AMEND-HOUSE";
        c.p2_submit_bill(bill,today);
        c.p2_approve_bill("REVOKE-HOUSE",{"PARTY_A"});
        c.p2_promulgate_bill("REVOKE-HOUSE","CANDIDATE_A",today);
    });
    world.advance_day(revoke); // Jan 4
    world.advance_day(); // Jan 5
    need(!actual(world).p2_amendment_active("AMEND-HOUSE")&&
         actual(world).p4_contest("A-HOUSE").term_months==48&&
         actual(world).p2_history().size()==2&&
         actual(world).cash("A")==start_cash&&
         actual(world).people().population()==start_population,
         "P2 amendment repeal must restore base law without creating resources");
}

static void elected_head_term_limit_and_scheduled_law() {
    Scenario state;
    auto world=daily::World::adopt(std::move(state.gh),402);
    world.advance_day(); // Jan 1
    world.advance_day(); // Jan 2: Candidate A elected
    need(actual(world).p4_nonparliament_results().at("A-HEAD").office_holder==
             "CANDIDATE_A"&&
         !actual(world).p2_candidate_eligible("A-HEAD","CANDIDATE_A"),
         "P2 first electoral term must exhaust a one-term consecutive limit");
    world.advance_day(); // Jan 3: A takes office
    need(actual(world).government("A").head_of_state=="CANDIDATE_A",
         "P1 must apply election result to the real government");
    while(world.date()<daily::Date{2000,2,4})world.advance_day();
    need(actual(world).p4_nonparliament_results().at("A-HEAD").office_holder==
             "CANDIDATE_B"&&
         actual(world).government("A").head_of_state=="CANDIDATE_B",
         "P4 must exclude P2 term-limited incumbent from subsequent election");
}

static void invalid_amendments_and_illegal_votes_rollback() {
    Scenario state;
    auto world=daily::World::adopt(std::move(state.gh),403);
    const auto original=daily::SnapshotCodec::encode(world);
    phasegh::World::Plan illegal;
    illegal[9].push_back([today=day({2000,1,1})](phasegh::Stage& gh) {
        auto bill=bill_for("ILLEGAL-BILL",phasec::P2BillAction::attach_amendment,
                           day({2000,1,2}));
        bill.proposed_amendment={
            "ILLEGAL-EXCEPTION","TERM-LAW-V1","exception:wrong-vote","vote",
            {clause("a","A-HEAD",phasec::P2ClauseKind::term_months,2)}
        };
        gh.phase_c().p2_submit_bill(bill,today);
        gh.phase_c().p2_approve_bill("ILLEGAL-BILL",{"NONEXISTENT"});
    });
    bool rejected=false;
    try{world.advance_day(illegal);}catch(const std::exception&){rejected=true;}
    need(rejected&&daily::SnapshotCodec::encode(world)==original,
         "P2 unlawful parliamentary approval must roll back the entire day");

    // 19th/20th century legal hierarchy also forbids a local ordinance from
    // imposing country-wide voting-age or taxation conditions.
    auto state2=Scenario{};
    auto& c=state2.gh.seed_geography().seed_core().seed_core();
    phasec::P2BaseLaw bad;
    bad.id="INVALID-LOCAL";bad.country="A";
    bad.domain="voting_age";bad.scope="RA";
    bad.base_choice="local-voting-rule";bad.legal_basis="bad:local";
    bad.tier=phasec::P2NormTier::local_ordinance;
    bad.clauses={clause("national-age","A",
                  phasec::P2ClauseKind::voting_age_years,16)};
    rejected=false;
    try{c.p2_seed_base(std::move(bad));}catch(const std::exception&){rejected=true;}
    need(rejected,"P2 local ordinance must not overrule nationwide suffrage");
}

int main() {
    try{
        exact_day_law_enactment_amendment_and_repeal();
        elected_head_term_limit_and_scheduled_law();
        invalid_amendments_and_illegal_votes_rollback();
        std::cout<<"P2 law, amendment and elected-term scenarios: 3 PASS\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"P2 GOVERNANCE FAILED: "<<e.what()<<"\n";
        return 1;
    }
}
