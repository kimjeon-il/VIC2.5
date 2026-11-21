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
        c.p3_seed_group({"A","Workers","","","",
                         P3GroupType::trade_unions,850'000,0});
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

static void negotiated_sponsor_row(const char* amendment,int strength,
        int government_approval,bool bureau_parent=false,bool stack=false) {
    const std::string domain=bureau_parent?"bureaucracy":"sponsor_fixture";
    const std::string opening_law=bureau_parent?"law_hereditary_bureaucrats":"opening";
    const std::string revised_law=bureau_parent?"law_appointed_bureaucrats":"revised";
    const std::string ending_law=bureau_parent?"law_elected_bureaucrats":"replaced";
    constexpr const char* extra="amendment_generic_sponsor_polstr_increase";
    const int weighted=50+50*(strength+(stack?20:0))/100;
    const int expected_sponsor=(weighted*1'000'000LL/(weighted+50))*9/10;
    const int expected_peer=(50*1'000'000LL/(weighted+50))*9/10;
    auto exercise=[&](bool direct) {
        phasegh::World gh(24000);
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"RA","A");geo.seed_province(2,"RB","B");
        geo.seed_unit(1,"A","A");geo.seed_unit(2,"B","B");
        geo.seed_assignment(1,1);geo.seed_assignment(2,2);
        auto& c=geo.seed_core().seed_core();
        for(const auto* country:{"A","B"}) {
            c.seed_actor(country,100);
            c.seed_government(country,"HEAD","PM",5,3);
            c.seed_p1_constitution(country,{P1GovernancePrinciple::monarchy,
                P1PowerDistribution::autocracy,P1LegislatureKind::none,"fixture:charter"});
        }
        c.seed_interest_group("Workers");
        c.seed_interest_group("Peer");
        for(const auto* country:{"A","B"})
            for(const auto* group:{"Workers","Peer"})
                c.p3_seed_group({country,group,"","","",P3GroupType::custom,0,0});
        c.seed_p1_coalition("A",{{"Peer"},{},false});
        c.seed_p1_coalition("B",{{"Workers"},{},false});
        auto& pop=c.seed_population();
        const auto seed=[&](const std::string& region) {
            const auto h=pop.household_profile({region,"mixed","shared",25});
            const auto e=pop.economic_profile({region,"worker","wage","employee",
                                              pop.default_education()});
            const auto d=pop.demographic_profile({region,23651,phaseb::Sex::male,
                "culture","faith","citizen","free",pop.default_health(),
                phaseb::Service::civilian});
            return pop.seed_population(d,e,h,100);
        };
        const auto a=seed("RA"),b=seed("RB");
        auto provision=P2Amendment{amendment,direct?"opening-law":"",
            "fixture:compromise","",{{"association","A",
                P2ClauseKind::political_association_allowed,1}}};
        c.p2_seed_base({"opening-law","A",domain,"national",
            opening_law,"fixture:law"});
        if(direct)c.p2_seed_amendment(provision);
        else {
            c.p2_register_group_demand({"actual-demand","A","Workers",
                domain,provision,200'000});
            if(stack)
                c.p2_register_group_demand({"secondary-demand","A","Workers",domain,
                    {extra,"","fixture:second-compromise","",
                     {{"strike","A",P2ClauseKind::peaceful_strike_allowed,1}}},
                    200'000});
        }
        auto world=daily::World::adopt(std::move(gh),43);
        const auto start=world.day_index();
        phasegh::World::Plan enrol;
        enrol[9].push_back([&](phasegh::Stage& stage) {
            auto& own=stage.phase_c();
            for(const auto cohort:{a,b}) {
                own.join_interest_group(cohort,"Workers",50);
                own.join_interest_group(cohort,"Peer",50);
            }
            if(!direct) {
                P2Bill bill;
                bill.id="NEGOTIATED";bill.country="A";bill.proposer="PM";
                bill.legal_basis="fixture:agreement";bill.effective_day=start+2;
                bill.proposed_base={"negotiated-law","A",domain,"national",
                    revised_law,"fixture:agreement"};
                own.p2_submit_bill(std::move(bill),start);
            }
        });
        world.advance_day(enrol);
        const auto clout=[&](const std::string& country,const std::string& group) {
            return ledger(world).p3_group_view(country,group).clout_ppm;
        };
        ok(clout("A","Workers")==450'000&&clout("A","Peer")==450'000&&
           clout("B","Workers")==450'000&&clout("B","Peer")==450'000,
           "sponsor benefit appeared without an effective negotiated amendment");
        if(direct) {
            ok(ledger(world).p2_amendment_active(amendment),
               "negative fixture must have an active amendment without sponsorship");
            auto loaded=daily::SnapshotCodec::decode(daily::SnapshotCodec::encode(world));
            ok(ledger(loaded).p3_group_view("A","Workers").clout_ppm==450'000,
               "snapshot without negotiations invented sponsor provenance");
            return;
        }
        const auto& agreements=ledger(world).p2_negotiated().at("NEGOTIATED");
        ok(agreements.size()==(stack?2u:1u)&&
           std::all_of(agreements.begin(),agreements.end(),
               [](const auto& row){return row.group=="Workers";}),
           "fixture lacked genuine separately negotiated sponsors");
        phasegh::World::Plan approve;
        approve[9].push_back([&](phasegh::Stage& stage) {
            auto& own=stage.phase_c();
            if(own.p2_bills().at("NEGOTIATED").status==P2BillStatus::submitted) {
                own.p2_approve_bill("NEGOTIATED",{});
                own.p2_promulgate_bill("NEGOTIATED","HEAD",start+1);
            } else {
                ok(own.p2_bills().at("NEGOTIATED").status==P2BillStatus::promulgated,
                   "negotiated bill was not lawfully approved");
            }
        });
        world.advance_day(approve);
        ok(clout("A","Workers")==450'000,"promulgation paid sponsor early");
        auto replay=daily::SnapshotCodec::decode(daily::SnapshotCodec::encode(world));
        world.advance_day();replay.advance_day();
        ok(ledger(world).p2_amendment_active(amendment)&&
           clout("A","Workers")==expected_sponsor&&clout("A","Peer")==expected_peer&&
           clout("B","Workers")==450'000&&clout("B","Peer")==450'000,
           "effective negotiated strength did not reach only the exact same-country sponsor");
        ok(daily::SnapshotCodec::encode(world)==daily::SnapshotCodec::encode(replay),
           "negotiated clout changed after loading");
        const auto approval=[&]() {
            return ledger(world).p3_group_view("A","Workers").approval;
        };
        ok(approval()==50&&ledger(world).p3_group_view("B","Workers").approval==50,
           "opposition or same-name foreign group acquired ruling approval");
        if(government_approval>0) {
            const auto cabinet=[&](const std::string& member) {
                const auto date=world.day_index();
                phasegh::World::Plan change;
                change[9].push_back([&](phasegh::Stage& stage) {
                    stage.phase_c().schedule_p1_cabinet_daily(
                        "A","HEAD","PM",{{member},{},false},date+1,"fixture:coalition");
                });
                world.advance_day(change);
                world.advance_day();
            };
            cabinet("Workers");
            ok(approval()==50+government_approval &&
               ledger(world).p3_group_view("A","Peer").approval==50 &&
               ledger(world).p3_group_view("B","Workers").approval==50,
               "the actual ruling sponsor did not receive exact approval points");
            auto governing=daily::SnapshotCodec::decode(daily::SnapshotCodec::encode(world));
            ok(ledger(governing).p3_group_view("A","Workers").approval==50+government_approval,
               "government sponsor approval did not survive native reload");
            cabinet("Peer");
            ok(approval()==50&&clout("A","Workers")==expected_sponsor,
               "loss of government membership must remove only approval");
        }
        auto active=daily::SnapshotCodec::decode(daily::SnapshotCodec::encode(world));
        ok(ledger(active).p3_group_view("A","Workers").clout_ppm==expected_sponsor,
           "active snapshot did not rebuild the sponsor bonus");
        const auto today=world.day_index();
        phasegh::World::Plan replace_parent;
        replace_parent[9].push_back([&](phasegh::Stage& stage) {
            auto& own=stage.phase_c();
            P2Bill bill;
            bill.id="REPLACE-PARENT";bill.country="A";bill.proposer="PM";
            bill.legal_basis="fixture:replace";bill.effective_day=today+1;
            bill.proposed_base={"without-sponsor","A",domain,"national",
                                ending_law,"fixture:replace"};
            own.p2_submit_bill(std::move(bill),today);
            own.p2_approve_bill("REPLACE-PARENT",{});
            own.p2_promulgate_bill("REPLACE-PARENT","HEAD",today);
        });
        active.advance_day(replace_parent);
        active.advance_day();
        ok(!ledger(active).p2_amendment_active(amendment)&&
           ledger(active).p3_group_view("A","Workers").clout_ppm==450'000,
           "replacement of the actual parent retained the old sponsor bonus");
        phasegh::World::Plan repeal;
        repeal[9].push_back([&](phasegh::Stage& stage) {
            auto& own=stage.phase_c();
            P2Bill bill;
            bill.id="REPEAL";bill.country="A";bill.proposer="PM";
            bill.legal_basis="fixture:repeal";bill.effective_day=today+1;
            bill.action=P2BillAction::repeal_amendment;bill.target_id=amendment;
            own.p2_submit_bill(std::move(bill),today);
            own.p2_approve_bill("REPEAL",{});
            own.p2_promulgate_bill("REPEAL","HEAD",today);
        });
        world.advance_day(repeal);
        ok(clout("A","Workers")==expected_sponsor,"repeal stopped clout before effective date");
        // Repeal becomes effective at Stage0 in the staged copy; a later
        // failure must roll back the recorded law and its derived clout.
        const auto opening=daily::SnapshotCodec::encode(world);
        phasegh::World::Plan fail;
        fail[9].push_back([](phasegh::Stage&) {
            throw std::invalid_argument("fixture: abort after Stage0");
        });
        bool aborted=false;
        try { world.advance_day(fail); }
        catch(const std::invalid_argument& e) {
            aborted=std::string(e.what())=="fixture: abort after Stage0";
        }
        ok(aborted&&daily::SnapshotCodec::encode(world)==opening&&
           clout("A","Workers")==expected_sponsor,
           "failed day advanced the live sponsor record");
        world.advance_day();
        ok(!ledger(world).p2_amendment_active(amendment)&&
           ledger(world).p2_negotiated().contains("NEGOTIATED")&&
           (!stack||ledger(world).p2_amendment_active(extra))&&
           clout("A","Workers")== (stack?490'908:450'000)&&
           clout("A","Peer")== (stack?409'090:450'000),
           "archived negotiation paid a repealed sponsor or removed another active source");
    };
    exercise(true);
    exercise(false);
}

static void negotiated_sponsor_uses_actual_same_country_group() {
    negotiated_sponsor_row("amendment_generic_sponsor_polstr_increase",20,0);
}
static void remaining_negotiated_sponsor_rows_use_actual_coalition() {
    negotiated_sponsor_row("amendment_preserved_bureaucratic_caste",15,0,true);
    negotiated_sponsor_row("amendment_preserved_bureaucratic_caste",0,0);
    negotiated_sponsor_row("amendment_generic_sponsor_polstr_approval_increase",20,2);
    negotiated_sponsor_row("amendment_generic_sponsor_polstr_increase_negotiation_1",20,1);
    negotiated_sponsor_row("amendment_generic_sponsor_polstr_increase_negotiation_2",30,2);
    negotiated_sponsor_row("amendment_generic_sponsor_polstr_increase_negotiation_3",40,3);
    negotiated_sponsor_row("amendment_generic_sponsor_polstr_increase_negotiation_1",
        20,1,false,true);
}

int main(){
    try{
        actual_upper_house_personal_terms_and_replay();
        negotiation_and_two_chamber_approval();
        real_economic_and_military_law_rejections();
        negotiated_sponsor_uses_actual_same_country_group();
        remaining_negotiated_sponsor_rows_use_actual_coalition();
        std::cout<<"P2 advanced politics: five focused scenarios PASS\n";
        return 0;
    }catch(const std::exception& ex){
        std::cerr<<"P2 ADVANCED POLITICS FAILED: "<<ex.what()<<"\n";
        return 1;
    }
}
