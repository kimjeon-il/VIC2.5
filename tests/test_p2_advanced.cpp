#include "vic25/daily_snapshot.hpp"
#include "vic25/p0_daily.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace vic25;
using namespace vic25::phasec;

static void ok(bool value,const char* explanation) {
    if(!value)throw std::runtime_error(explanation);
}
static daily::DayIndex day(daily::Date date) {
    return daily::to_day_index(date);
}
static const IntegratedWorld& ledger(const daily::World& world) {
    return world.engine().base().core().core();
}
static P2Clause rule(const std::string& id,const std::string& target,
                     P2ClauseKind kind,std::int64_t value) {
    return {id,target,kind,value};
}
struct Fixture {
    phasegh::World gh{24000};
    phaseb::Id citizens{};
    Fixture() {
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"REGION","A");
        geo.seed_unit(100,"A","ADMIN");
        geo.seed_assignment(1,100);
        auto& c=geo.seed_core().seed_core();
        c.seed_actor("A",1000);
        c.seed_actor("HOSP",1000);
        c.seed_government("A","HEAD","PM",5,3);
        c.seed_unit("GUARD");
        c.seed_p1_constitution("A",{
            P1GovernancePrinciple::parliamentary_republic,
            P1PowerDistribution::universal_vote,
            P1LegislatureKind::bicameral,"constitution:bicameral"});
        c.seed_interest_group("Workers");
        c.p4_register_party({"MAJ","A","HEAD_MAJ",900'000,true,true});
        c.p4_register_party({"OPP","A","HEAD_OPP",100'000,true,true});
        const auto initial=day({2000,1,2});
        c.p4_seed_contest({"LOWER","A","national","lower-act",
            P4Office::legislature,P4BallotMethod::list_dhondt,
            P4TermMode::restart_after_poll,48,1'000'000,5,
            initial,initial,-1,true,true,false});
        c.p4_seed_contest({"UPPER","A","national","senate-act",
            P4Office::upper_chamber,P4BallotMethod::list_dhondt,
            P4TermMode::restart_after_poll,1,1'000'000,3,
            initial,initial,-1,false,true,false});
        c.p4_seed_contest({"MAYOR","A","REGION","municipal-act",
            P4Office::local_head,P4BallotMethod::plurality,
            P4TermMode::restart_after_poll,1,1'000'000,1,
            initial,initial,-1,false,true,false});
        c.p2_register_upper_chamber("A","UPPER");

        auto& pop=c.seed_population();
        const auto hh=pop.household_profile({"REGION","shared","shared",100});
        const auto economic=pop.economic_profile({
            "REGION","worker","wage","employee",pop.default_education()});
        const auto demographic=pop.demographic_profile({
            "REGION",24000-360,phaseb::Sex::female,
            "culture","religion","citizen","free",
            pop.default_health(),phaseb::Service::civilian});
        citizens=pop.seed_population(demographic,economic,hh,100,1);
        pop.hire(citizens,2,"HOSP","worker",10,{});
        for(const auto& name:std::vector<std::string>{
            "MP1","MP2","MP3","MP4","MP5",
            "SEN1","SEN2","SEN3","MAYOR1"})
            c.seed_person_anchor(name,citizens);
        c.p4_nominate("LOWER","MAJ",{
            "MP1","MP2","MP3","MP4","MP5"});
        c.p4_nominate("UPPER","MAJ",{
            "SEN1","SEN2","SEN3"});
        c.p4_nominate("MAYOR","MAJ",{"MAYOR1"});

        c.seed_public_institution("CLINIC","HOSP","REGION",
                                  "medicine",10,2);
        c.seed_p1_institution("A","CLINIC",P1InstitutionKind::healthcare,5);

        P2BaseLaw term;
        term.id="ELECTED-TERM-ACT";term.country="A";
        term.domain="term_law";term.scope="national";
        term.base_choice="fixed-elected-terms";
        term.legal_basis="statute:terms";
        term.clauses={
            rule("s-term","UPPER",P2ClauseKind::term_months,1),
            rule("s-consecutive","UPPER",
                 P2ClauseKind::max_consecutive_terms,1),
            rule("m-term","MAYOR",P2ClauseKind::term_months,1),
            rule("m-consecutive","MAYOR",
                 P2ClauseKind::max_consecutive_terms,1)
        };
        c.p2_seed_base(std::move(term));
        P2BaseLaw economy;
        economy.id="ECONOMY-ACT";economy.country="A";
        economy.domain="wages_trade";economy.scope="national";
        economy.base_choice="regulated-market";
        economy.legal_basis="statute:economy";
        economy.clauses={
            rule("minimum-wage","A",
                 P2ClauseKind::minimum_monthly_contract_wage,20),
            rule("tariff-cap","A",
                 P2ClauseKind::maximum_import_tariff_ppm,100'000),
            rule("no-conscription","A",
                 P2ClauseKind::conscription_authorized,0)
        };
        c.p2_seed_base(std::move(economy));
        P2GroupDemand demand;
        demand.id="WORKER-HEALTH-DEMAND";
        demand.country="A";
        demand.interest_group="Workers";
        demand.law_domain="healthcare";
        demand.min_population_support_ppm=200'000;
        demand.amendment={
            "WORKER-HEALTH-AMENDMENT","","ig:compromise-health","care-right",
            {rule("health-right","A",
                  P2ClauseKind::public_healthcare_right,1),
             rule("clinic-level","CLINIC",
                  P2ClauseKind::public_institution_level,2)}
        };
        c.p2_register_group_demand(std::move(demand));
        gh.validate();
    }
};

static P2Bill propose_health() {
    P2Bill bill;
    bill.id="HEALTH-BILL";bill.country="A";bill.proposer="PM";
    bill.legal_basis="statute:health";
    bill.action=P2BillAction::enact_or_replace_base;
    bill.effective_day=day({2000,1,5});
    bill.proposed_base.id="HEALTH-ACT";
    bill.proposed_base.country="A";
    bill.proposed_base.domain="healthcare";
    bill.proposed_base.scope="national";
    bill.proposed_base.base_choice="restricted-care";
    bill.proposed_base.legal_basis="statute:health";
    bill.proposed_base.clauses={
        rule("basic-care","A",P2ClauseKind::public_healthcare_right,0)};
    return bill;
}

static void actual_upper_house_personal_terms_and_replay() {
    Fixture fixture;
    auto world=daily::World::adopt(std::move(fixture.gh),100);
    world.advance_day(); // 2000-01-01
    world.advance_day(); // 2000-01-02: three real contests
    const auto& c=ledger(world);
    ok(c.p4_history().size()==3&&
       c.country_election("A").total_seats==5&&
       c.p4_nonparliament_results().at("UPPER").seats.at("MAJ")==3&&
       c.p4_nonparliament_results().at("MAYOR").office_holder=="MAYOR1",
       "bicameral and local elections must retain separate real seats");
    const auto& senators=c.p4_person_terms();
    ok(senators.at({"UPPER","SEN1"}).total_terms==1&&
       senators.at({"LOWER","MP1"}).total_terms==1&&
       senators.at({"MAYOR","MAYOR1"}).total_terms==1&&
       !c.p2_candidate_eligible("UPPER","SEN1")&&
       !c.p2_candidate_eligible("MAYOR","MAYOR1"),
       "lawful individual Senate, legislature and mayor term limits not tracked");
    const auto encoded=daily::SnapshotCodec::encode(world);
    const auto restored=daily::SnapshotCodec::decode(encoded);
    ok(daily::SnapshotCodec::encode(restored)==encoded&&
       ledger(restored).p4_person_terms().size()==9,
       "POP-anchored elected offices did not survive checkpoint replay");
}

static void negotiation_and_two_chamber_approval() {
    Fixture fixture;
    auto world=daily::World::adopt(std::move(fixture.gh),101);
    world.advance_day();
    world.advance_day();
    phasegh::World::Plan bill;
    bill[9].push_back([slice=fixture.citizens](phasegh::Stage& gh) {
        auto& c=gh.phase_c();
        c.join_interest_group(slice,"Workers",30);
        c.p2_submit_bill(propose_health(),day({2000,1,3}));
    });
    world.advance_day(bill); // Day3: named group negotiates automatically
    const auto& after_offer=ledger(world);
    ok(after_offer.p2_negotiated().at("HEALTH-BILL").size()==1&&
       !after_offer.p2_amendment_active("WORKER-HEALTH-AMENDMENT"),
       "P3 group negotiation must be backed by real POP and future enactment");

    // A lower-house vote alone is not a valid bicameral Act.
    const auto opening=daily::SnapshotCodec::encode(world);
    phasegh::World::Plan lower_only;
    lower_only[9].push_back([](phasegh::Stage& gh) {
        auto& c=gh.phase_c();
        c.p2_approve_bill("HEALTH-BILL",{"MAJ"});
        c.p2_promulgate_bill("HEALTH-BILL","HEAD",day({2000,1,4}));
    });
    bool denied=false;
    try{world.advance_day(lower_only);}catch(const std::exception&){denied=true;}
    ok(denied&&daily::SnapshotCodec::encode(world)==opening,
       "P2 statute cannot be promulgated after lower chamber alone");

    phasegh::World::Plan both;
    both[9].push_back([](phasegh::Stage& gh) {
        auto& c=gh.phase_c();
        c.p2_approve_bill("HEALTH-BILL",{"MAJ"});
        c.p2_approve_upper_bill("HEALTH-BILL",{"MAJ"});
        c.p2_promulgate_bill("HEALTH-BILL","HEAD",day({2000,1,4}));
    });
    world.advance_day(both); // Day4: approved, promulgated; not effective
    ok(!ledger(world).p2_amendment_active("WORKER-HEALTH-AMENDMENT"),
       "P2 amendment cannot produce legal effects before promulgated effective date");
    const auto saved=daily::SnapshotCodec::encode(world);
    auto replay=daily::SnapshotCodec::decode(saved);
    world.advance_day();replay.advance_day(); // Day5: act and negotiated clause
    ok(ledger(world).p2_amendment_active("WORKER-HEALTH-AMENDMENT")&&
       ledger(world).p2_rule("A","A",
            P2ClauseKind::public_healthcare_right)==1&&
       ledger(world).p1_institution_capacity("CLINIC").level==2&&
       daily::SnapshotCodec::encode(world)==
           daily::SnapshotCodec::encode(replay),
       "bicameral negotiated law did not take effect atomically on exact day");
}

static void real_economic_and_military_law_rejections() {
    {
        Fixture fixture;
        auto world=daily::World::adopt(std::move(fixture.gh),102);
        const auto original=daily::SnapshotCodec::encode(world);
        phasegh::World::Plan work;
        work[2].push_back([slice=fixture.citizens](phasegh::Stage& gh) {
            gh.phase_c().hire(slice,1,"HOSP",10);
        });
        bool denied=false;
        try{world.advance_day(work);}catch(const std::exception&){denied=true;}
        ok(denied&&daily::SnapshotCodec::encode(world)==original,
           "P2 minimum wage violation must reject real Phase B hiring");
    }
    {
        Fixture fixture;
        auto world=daily::World::adopt(std::move(fixture.gh),103);
        const auto original=daily::SnapshotCodec::encode(world);
        phasegh::World::Plan mobilization;
        mobilization[10].push_back([slice=fixture.citizens](phasegh::Stage& gh){
            gh.phase_c().mobilize(slice,1,"GUARD");
        });
        bool denied=false;
        try{world.advance_day(mobilization);}
        catch(const std::exception&){denied=true;}
        ok(denied&&daily::SnapshotCodec::encode(world)==original,
           "P2 no-conscription law must constrain real POP military assignment");
    }
    {
        Fixture fixture;
        auto world=daily::World::adopt(std::move(fixture.gh),104);
        const auto original=daily::SnapshotCodec::encode(world);
        phasegh::World::Plan tariff;
        tariff[0].push_back([](phasegh::Stage& gh) {
            gh.adopt_tariff_daily("EXCESS","A","iron",200'000,"PM");
        });
        bool denied=false;
        try{world.advance_day(tariff);}
        catch(const std::exception&){denied=true;}
        ok(denied&&daily::SnapshotCodec::encode(world)==original,
           "P2 import tariff ceiling must bind real H customs action");
    }
}

int main(){
    try{
        actual_upper_house_personal_terms_and_replay();
        negotiation_and_two_chamber_approval();
        real_economic_and_military_law_rejections();
        std::cout<<"P2 advanced politics: three focused scenarios PASS\n";
        return 0;
    }catch(const std::exception& ex){
        std::cerr<<"P2 ADVANCED POLITICS FAILED: "<<ex.what()<<"\n";
        return 1;
    }
}
