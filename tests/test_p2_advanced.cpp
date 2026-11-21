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

static void direct_named_group_approval_tracks_law_not_the_coalition() {
    const std::vector<std::pair<std::string,P3GroupType>> groups={
        {"Land",P3GroupType::landowners},{"Rural",P3GroupType::rural_folk},
        {"Army",P3GroupType::armed_forces},{"Petty",P3GroupType::petite_bourgeoisie},
        {"Devout",P3GroupType::devout},{"Control",P3GroupType::custom},
        {"Second Land",P3GroupType::landowners}};
    const auto run=[&](const std::string& id,const std::string& domain,
                       const std::string& law_choice,const std::string& country,
                       const std::vector<int>& effects) {
        const std::string foreign=country=="JAP"?"OTHER":"JAP";
        phasegh::World gh(24000);
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"RA",country);geo.seed_province(2,"RB",foreign);
        geo.seed_unit(1,country,"AA");geo.seed_unit(2,foreign,"BB");
        geo.seed_assignment(1,1);geo.seed_assignment(2,2);
        auto& c=geo.seed_core().seed_core();
        for(const auto& [name,type]:groups){(void)type;c.seed_interest_group(name);}
        auto& pop=c.seed_population();
        const auto seed=[&](const std::string& owner,const std::string& region) {
            c.seed_actor(owner,100);
            c.seed_government(owner,"HEAD","PM",5,3);
            c.seed_p1_constitution(owner,{P1GovernancePrinciple::monarchy,
                P1PowerDistribution::autocracy,P1LegislatureKind::none,"fixture:charter"});
            for(const auto& [name,type]:groups)
                c.p3_seed_group({owner,name,"","","",type,0,0});
            c.seed_p1_coalition(owner,{{"Control"},{},false});
            c.p2_seed_base({owner+":BASE",owner,domain,"",law_choice,"fixture:law"});
            const auto h=pop.household_profile({region,"mixed","shared",25});
            const auto e=pop.economic_profile({region,"worker","wage","employee",
                                              pop.default_education()});
            const auto d=pop.demographic_profile({region,23651,phaseb::Sex::male,
                "culture","faith","citizen","free",pop.default_health(),
                phaseb::Service::civilian});
            return pop.seed_population(d,e,h,100);
        };
        const auto own_slice=seed(country,"RA"),foreign_slice=seed(foreign,"RB");
        auto world=daily::World::adopt(std::move(gh),29);
        const auto start=world.day_index();
        phasegh::World::Plan enroll;
        enroll[9].push_back([&](phasegh::Stage& stage) {
            auto& live=stage.phase_c();
            for(const auto cohort:{own_slice,foreign_slice})for(const auto& [name,type]:groups) {
                (void)type;live.join_interest_group(cohort,name,10);
            }
        });
        world.advance_day(enroll);
        const auto compare=[&](const daily::World& one,const daily::World& two,
                               const std::vector<int>& difference) {
            for(std::size_t i=0;i<groups.size();++i) {
                auto approval=[&](const daily::World& w,const std::string& side) {
                    return ledger(w).p3_group_view(side,groups[i].first).approval;
                };
                ok(approval(one,country)==approval(two,country)+difference[i]&&
                   approval(one,foreign)==approval(two,foreign),
                   "direct law approval did not reach only the exact same-country named category");
            }
        };
        auto baseline=world;
        phasegh::World::Plan attach;
        attach[9].push_back([&](phasegh::Stage& stage) {
            auto& live=stage.phase_c();
            P2Bill bill;
            bill.id="ATTACH";bill.country=country;bill.proposer="PM";
            bill.legal_basis="fixture:approval";bill.effective_day=start+2;
            bill.action=P2BillAction::attach_amendment;
            bill.proposed_amendment={id,country+":BASE","fixture:approval","",
                {{"association",country,P2ClauseKind::political_association_allowed,1}}};
            live.p2_submit_bill(std::move(bill),start+1);
            live.p2_approve_bill("ATTACH",{});
            live.p2_promulgate_bill("ATTACH","HEAD",start+1);
        });
        world.advance_day(attach);baseline.advance_day();
        const std::vector<int> zero(groups.size());
        compare(world,baseline,zero);
        auto replay=daily::SnapshotCodec::decode(daily::SnapshotCodec::encode(world));
        world.advance_day();baseline.advance_day();replay.advance_day();
        ok(ledger(world).p2_amendment_active(id)&&
           daily::SnapshotCodec::encode(world)==daily::SnapshotCodec::encode(replay),
           "direct approval proof lacks effective legal amendment or deterministic replay");
        compare(world,baseline,effects);
        const auto repeal_day=world.day_index();
        phasegh::World::Plan repeal;
        repeal[9].push_back([&](phasegh::Stage& stage) {
            auto& live=stage.phase_c();
            P2Bill bill;
            bill.id="REPEAL";bill.country=country;bill.proposer="PM";
            bill.legal_basis="fixture:repeal";bill.effective_day=repeal_day+1;
            bill.action=P2BillAction::repeal_amendment;bill.target_id=id;
            live.p2_submit_bill(std::move(bill),repeal_day);
            live.p2_approve_bill("REPEAL",{});
            live.p2_promulgate_bill("REPEAL","HEAD",repeal_day);
        });
        world.advance_day(repeal);baseline.advance_day();
        compare(world,baseline,effects);
        world.advance_day();baseline.advance_day();
        ok(!ledger(world).p2_amendment_active(id),"repeal did not actually execute");
        compare(world,baseline,zero);
    };
    run("amendment_reinstated_fueros","citizenship","law_subjecthood","SPA",
        {1,1,0,0,0,0,1});
    run("amendment_frugal_ordinance","edo_social_system","law_strict_edo_system","JAP",
        {2,0,2,-2,0,0,2});
    run("amendment_sovereign_of_the_nation","governance_principles","law_monarchy","A",
        {0,0,0,0,-1,0,0});
    run("amendment_american_fugitive_slaves_act","slavery","law_legacy_slavery","USA",
        {1,0,0,0,0,0,1});
    run("amendment_frugal_ordinance","edo_social_system","law_strict_edo_system","OTHER",
        {0,0,0,0,0,0,0});
    run("amendment_frugal_ordinance","edo_social_system","law_lax_edo_system","JAP",
        {0,0,0,0,0,0,0});
}

static void canonical_enactment_success_changes_real_review_outcome() {
    const auto scenario=[](const std::string& country,const std::string& id,
                           const std::string& parent,const std::string& domain,
                           const std::string& choice,int row_penalty,
                           bool qualifies,bool pending=false) {
        const auto exercise=[&](bool attach,bool below=false) {
            phasegh::World gh(24000);
            auto& geo=gh.seed_geography();
            geo.seed_province(1,"R",country);geo.seed_unit(1,country,"R");
            geo.seed_assignment(1,1);
            auto& c=geo.seed_core().seed_core();
            c.seed_actor(country,100);
            c.seed_government(country,"HEAD","PM",5,3);
            c.seed_p1_constitution(country,{P1GovernancePrinciple::monarchy,
                P1PowerDistribution::autocracy,P1LegislatureKind::none,"fixture:authority"});
            c.seed_interest_group("Actual");
            c.p3_seed_group({country,"Actual","","","",P3GroupType::custom,0,0});
            c.seed_p1_coalition(country,{{"Actual"},{},false});
            c.p2_seed_base({"opening",country,"distribution_of_power","",parent,"fixture:law"});
            auto& pop=c.seed_population();
            const auto h=pop.household_profile({"R","mixed","shared",250});
            const auto e=pop.economic_profile({"R","worker","wage","employee",
                                              pop.default_education()});
            const auto d=pop.demographic_profile({"R",23651,phaseb::Sex::male,
                "culture","faith","citizen","free",pop.default_health(),
                phaseb::Service::civilian});
            const auto members=pop.seed_population(d,e,h,1000);
            c.p3_seed_movement({"Backing",country,domain,choice,"",
                P3MovementGoal::legal_reform,{"Actual"},{},300'000,100'000,true});
            auto world=daily::World::adopt(std::move(gh),19);
            const auto start=world.day_index();
            const auto due=start+(domain=="distribution_of_power"?150:
                                  domain=="governance_principles"?200:100);
            // Match the engine's existing deterministic roll, with no injected
            // seed, and choose an interval between the two real thresholds.
            std::string proposal;
            for(int candidate=0;candidate<10000;++candidate) {
                proposal="review-"+std::to_string(candidate);
                std::uint64_t roll=14695981039346656037ULL;
                for(const unsigned char ch:country+":"+proposal+":"+
                        std::to_string(due)+":0") {roll^=ch;roll*=1099511628211ULL;}
                const auto cutoff=static_cast<unsigned>(400'000-row_penalty);
                const auto bucket=roll%1'000'000;
                if((below&&bucket>=cutoff-1'000&&bucket<cutoff)||
                   (!below&&bucket>=cutoff&&bucket<cutoff+1'000))break;
                proposal.clear();
            }
            ok(!proposal.empty(),"no adversarial review roll found");
            phasegh::World::Plan submit;
            submit[9].push_back([&](phasegh::Stage& stage) {
                auto& live=stage.phase_c();
                live.join_interest_group(members,"Actual",400);
                P2Bill bill;
                bill.id=proposal;bill.country=country;bill.proposer="PM";
                bill.legal_basis="fixture:ordinary-review";
                bill.proposed_base={"revised",country,domain,"national",
                    choice,"fixture:ordinary-review"};
                live.p2_submit_bill(std::move(bill),start);
                if(!attach)return;
                P2Bill amendment;
                amendment.id="ATTACH";amendment.country=country;amendment.proposer="PM";
                amendment.legal_basis="fixture:lawlike-source";
                amendment.action=P2BillAction::attach_amendment;
                amendment.effective_day=pending?due+1:start+1;
                amendment.proposed_amendment={id,"opening","fixture:lawlike-source","",
                    {{"association",country,P2ClauseKind::political_association_allowed,1}}};
                live.p2_submit_bill(std::move(amendment),start);
                live.p2_approve_bill("ATTACH",{});
                live.p2_promulgate_bill("ATTACH","HEAD",start);
            });
            world.advance_day(submit);
            while(world.day_index()<due)world.advance_day();
            const auto& before=ledger(world);
            ok(before.p3_movement_view("Backing").support_ppm==400'000&&
               before.p1_legitimacy(country).score>=25&&
               before.p2_enactments().at(proposal).checks==0&&
               before.p2_amendment_active(id)==(attach&&!pending),
               "review controls are confounded by substantive support, timing or eligibility");
            auto replay=daily::SnapshotCodec::decode(daily::SnapshotCodec::encode(world));
            world.advance_day();replay.advance_day();
            const auto& reviewed=ledger(world).p2_enactments().at(proposal);
            ok(reviewed.checks==1 && !reviewed.ready &&
               ledger(world).p2_bills().at(proposal).status==P2BillStatus::submitted&&
               daily::SnapshotCodec::encode(world)==daily::SnapshotCodec::encode(replay),
               "review changed lawful vote/assent status or failed replay");
            return reviewed.phase;
        };
        ok(exercise(false)==2,"baseline roll did not pass the actual 40% threshold");
        ok(exercise(true)==(qualifies&&!pending?1:2),
           "canonical law did not apply exactly to this actual staged success threshold");
        if(qualifies&&!pending)
            ok(exercise(true,true)==2,
               "canonical law removed a success below the expected modified threshold");
    };
    const std::string domain="distribution_of_power";
    const std::string autocracy="law_autocracy", oligarchy="law_oligarchy";
    const std::string tradition="amendment_tradition_of_free_elections";
    const std::string metternich="amendment_geheime_staatskonferenz_metternich";
    scenario("AUS",metternich,autocracy,domain,oligarchy,50'000,true);
    scenario("AUS","amendment_geheime_staatskonferenz_kolowrat",
        autocracy,domain,oligarchy,150'000,true);
    scenario("AUS","amendment_geheime_staatskonferenz",
        autocracy,domain,oligarchy,100'000,true);
    for(const auto& choice:{autocracy,std::string("oligarchy"),
            std::string("law_technocracy"),std::string("law_anarchy")})
        scenario("A",tradition,oligarchy,domain,choice,100'000,true);
    scenario("OTHER",metternich,autocracy,domain,oligarchy,50'000,false);
    scenario("AUS",metternich,oligarchy,domain,autocracy,50'000,false);
    scenario("AUS",metternich,autocracy,domain,oligarchy,50'000,true,true);
    scenario("A",tradition,oligarchy,domain,"law_wealth_voting",100'000,false);
    scenario("A",tradition,oligarchy,"different_domain",autocracy,100'000,false);
    scenario("A",tradition,oligarchy,"lawgroup_distribution_of_power",
             "law_technocracy",100'000,true);
}

static void austrian_law_speed_changes_only_new_review_intervals() {
    const auto run=[](const std::string& country,const std::string& parent_choice,
                      const std::string& source,int speed_ppm,int first,
                      bool starts_after_scheduling=false) {
        phasegh::World gh(24000);
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"R",country);geo.seed_unit(1,country,"R");
        geo.seed_assignment(1,1);
        auto& c=geo.seed_core().seed_core();
        c.seed_actor(country,100);
        c.seed_government(country,"HEAD","PM",5,3);
        c.seed_p1_constitution(country,{P1GovernancePrinciple::monarchy,
            P1PowerDistribution::autocracy,P1LegislatureKind::none,"fixture:authority"});
        c.seed_interest_group("Actual");
        c.p3_seed_group({country,"Actual","","","",P3GroupType::custom,0,0});
        c.seed_p1_coalition(country,{{"Actual"},{},false});
        c.p2_seed_base({"opening",country,"distribution_of_power","",parent_choice,"fixture:law"});
        if(!source.empty()&&!starts_after_scheduling)c.p2_seed_amendment({source,"opening","fixture:active","",
            {{"association",country,P2ClauseKind::political_association_allowed,1}}});
        auto& pop=c.seed_population();
        const auto h=pop.household_profile({"R","mixed","shared",250});
        const auto e=pop.economic_profile({"R","worker","wage","employee",
                                          pop.default_education()});
        const auto d=pop.demographic_profile({"R",23651,phaseb::Sex::male,
            "culture","faith","citizen","free",pop.default_health(),
            phaseb::Service::civilian});
        const auto cohort=pop.seed_population(d,e,h,1000);
        c.p3_seed_movement({"Backing",country,"review_fixture","reformed","",
            P3MovementGoal::legal_reform,{"Actual"},{},300'000,100'000,true});
        auto world=daily::World::adopt(std::move(gh),31);
        const auto start=world.day_index();
        phasegh::World::Plan submit;
        submit[9].push_back([&](phasegh::Stage& stage) {
            auto& live=stage.phase_c();
            live.join_interest_group(cohort,"Actual",400);
            P2Bill bill;
            bill.id="STAGED";bill.country=country;bill.proposer="PM";
            bill.legal_basis="fixture:review";
            bill.proposed_base={"revised",country,"review_fixture","national",
                "reformed","fixture:review"};
            live.p2_submit_bill(std::move(bill),start);
            if(starts_after_scheduling) {
                P2Bill attach;
                attach.id="ATTACH-SPEED";attach.country=country;attach.proposer="PM";
                attach.legal_basis="fixture:active";attach.effective_day=start+1;
                attach.action=P2BillAction::attach_amendment;
                attach.proposed_amendment={source,"opening","fixture:active","",
                    {{"association",country,P2ClauseKind::political_association_allowed,1}}};
                live.p2_submit_bill(std::move(attach),start);
                live.p2_approve_bill("ATTACH-SPEED",{});
                live.p2_promulgate_bill("ATTACH-SPEED","HEAD",start);
            }
        });
        world.advance_day(submit);
        const auto progress=[&]() -> const P2Enactment& {
            return ledger(world).p2_enactments().at("STAGED");
        };
        ok(progress().next_review_day==start+first,
           "active speed did not lengthen the first real review interval");
        const auto until=[&](std::int64_t due) {
            while(world.day_index()<due)world.advance_day();
        };
        const auto base_period=[&]() {
            const auto& core=ledger(world);
            const auto legit=core.p1_legitimacy(country).score;
            const auto authority=core.p1_authority_budget(country);
            const auto unused=authority.supply>0?
                std::clamp(authority.available*1000/authority.supply,0,1000):0;
            ok(legit>=25&&core.p3_movement_view("Backing").support_ppm==400'000,
               "speed fixture lacked actual eligible regular review support");
            return 100*(legit>=90?750:legit<50?1500:1000)/1000*
                       (1000-unused/4)/1000;
        };
        const auto first_day=start+first;
        until(first_day);
        ok(progress().checks==0&&progress().next_review_day==first_day&&
           (!starts_after_scheduling||ledger(world).p2_amendment_active(source)),
           "first review ran early or new law rebased a saved checkpoint");
        const auto first_unmodified=base_period();
        auto replay=daily::SnapshotCodec::decode(daily::SnapshotCodec::encode(world));
        world.advance_day();replay.advance_day();
        const auto expected_period=std::clamp<std::int64_t>(
            (first_unmodified*1'000'000LL+speed_ppm-1)/speed_ppm,30,300);
        const auto second_day=first_day+expected_period;
        ok(progress().checks==1&&progress().last_review_day==first_day&&
           progress().next_review_day==second_day&&
           daily::SnapshotCodec::encode(world)==daily::SnapshotCodec::encode(replay),
           "speed did not preserve first/recurring date and saved replay equivalence");
        if(source.empty()||speed_ppm==1'000'000)return;
        const auto now=world.day_index();
        phasegh::World::Plan repeal;
        repeal[9].push_back([&](phasegh::Stage& stage) {
            auto& live=stage.phase_c();
            P2Bill bill;
            bill.id="REPEAL-SPEED";bill.country=country;bill.proposer="PM";
            bill.legal_basis="fixture:repeal";bill.effective_day=now+1;
            bill.action=P2BillAction::repeal_amendment;bill.target_id=source;
            live.p2_submit_bill(std::move(bill),now);
            live.p2_approve_bill("REPEAL-SPEED",{});
            live.p2_promulgate_bill("REPEAL-SPEED","HEAD",now);
        });
        world.advance_day(repeal);
        auto after_promulgation=daily::SnapshotCodec::decode(
            daily::SnapshotCodec::encode(world));
        world.advance_day();after_promulgation.advance_day();
        ok(!ledger(world).p2_amendment_active(source)&&
           progress().next_review_day==second_day&&progress().checks==1&&
           daily::SnapshotCodec::encode(world)==daily::SnapshotCodec::encode(after_promulgation),
           "actual repeal or reload illegally rescheduled a saved checkpoint");
        until(second_day);
        const auto later_unmodified=base_period();
        world.advance_day();
        ok(progress().checks==2&&progress().last_review_day==second_day&&
           progress().next_review_day==
              second_day+std::clamp<std::int64_t>(later_unmodified,30,300),
           "expired law did not leave only new subsequent intervals unmodified");
    };
    const std::string auto_law="law_autocracy";
    run("AUS",auto_law,"",1'000'000,100);
    run("AUS",auto_law,"amendment_geheime_staatskonferenz_metternich",900'000,112);
    run("AUS",auto_law,"amendment_geheime_staatskonferenz",800'000,125);
    run("AUS",auto_law,"amendment_geheime_staatskonferenz_kolowrat",750'000,134);
    run("OTHER",auto_law,"amendment_geheime_staatskonferenz_metternich",1'000'000,100);
    run("AUS","law_oligarchy","amendment_geheime_staatskonferenz_metternich",1'000'000,100);
    run("AUS",auto_law,"amendment_geheime_staatskonferenz_metternich",
        900'000,100,true);
}

static void political_concessions_commit_real_national_authority() {
    const auto run=[](const std::string& id,int expected,bool bad_parent=false) {
        phasegh::World gh(24000);
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"R","A");geo.seed_province(2,"S","B");
        geo.seed_unit(1,"A","AA");geo.seed_unit(2,"B","BB");
        geo.seed_assignment(1,1);geo.seed_assignment(2,2);
        auto& c=geo.seed_core().seed_core();
        for(const auto* country:{"A","B"}) {
            c.seed_actor(country,100);
            c.seed_government(country,"HEAD","PM",5,3);
            c.seed_p1_constitution(country,{P1GovernancePrinciple::monarchy,
                P1PowerDistribution::autocracy,P1LegislatureKind::none,"fixture:authority"});
            c.p2_seed_base({std::string(country)+":POWER",country,
                "distribution_of_power","","law_autocracy","fixture:power"});
        }
        if(bad_parent)c.p2_seed_base({"unrecognized","A","fictional_local_finance","national",
            "not-a-canonical-law","fixture:invalid-parent"});
        c.seed_interest_group("Members");
        c.p3_seed_group({"A","Members","","","",P3GroupType::custom,0,0});
        auto& pop=c.seed_population();
        const auto h=pop.household_profile({"R","mixed","shared",25});
        const auto e=pop.economic_profile({"R","worker","wage","employee",
                                          pop.default_education()});
        const auto d=pop.demographic_profile({"R",23651,phaseb::Sex::male,
            "culture","faith","citizen","free",pop.default_health(),
            phaseb::Service::civilian});
        const auto resident=pop.seed_population(d,e,h,100);
        c.p3_seed_movement({"ACTUAL","A","fixture-cause","reform","R",
            P3MovementGoal::legal_reform,{"Members"},{},300'000,100'000,true});
        auto world=daily::World::adopt(std::move(gh),31);
        const auto start=world.day_index();
        phasegh::World::Plan attach;
        attach[9].push_back([&](phasegh::Stage& stage) {
            auto& live=stage.phase_c();
            live.join_interest_group(resident,"Members",10);
            live.p1_activate_mandate({"REGIONAL","A","R",
                P1PoliticalMandateKind::regional_decree,P1DecreeKind::construction_drive,100},start);
            P2Bill bill;
            bill.id="ATTACH";bill.country="A";bill.proposer="PM";
            bill.legal_basis="fixture:cost";bill.effective_day=start+2;
            bill.action=P2BillAction::attach_amendment;
            bill.proposed_amendment={id,bad_parent?"unrecognized":"A:POWER",
                "fixture:cost","",{{"association","A",
                P2ClauseKind::political_association_allowed,1}}};
            live.p2_submit_bill(std::move(bill),start);
            live.p2_approve_bill("ATTACH",{});
            live.p2_promulgate_bill("ATTACH","HEAD",start);
        });
        world.advance_day(attach);
        const auto check_budget=[&](int cost) {
            const auto a=ledger(world).p1_authority_budget("A");
            const auto b=ledger(world).p1_authority_budget("B");
            const auto flow=ledger(world).p1_authority_capacity("A");
            ok(a.supply==300&&a.reserved==100+cost&&a.available==200-cost&&
               flow.generated==300&&flow.committed==100+cost&&flow.balance==200-cost&&
               b.supply==300&&b.reserved==0&&b.available==300,
               "authored concession did not reserve exact continuing authority once");
        };
        check_budget(0);
        world.advance_day();
        check_budget(0);
        auto replay=daily::SnapshotCodec::decode(daily::SnapshotCodec::encode(world));
        world.advance_day();replay.advance_day();
        ok(ledger(world).p2_amendment_active(id)&&ledger(world).p2_negotiated().empty()&&
           daily::SnapshotCodec::encode(world)==daily::SnapshotCodec::encode(replay),
           "cost fixture did not enact direct concession safely through native replay");
        check_budget(expected);

        const auto try_movement=[&]() {
            const auto day=world.day_index();
            phasegh::World::Plan action;
            action[9].push_back([day](phasegh::Stage& stage) {
                stage.phase_c().p1_activate_mandate({"MOVEMENT","A","ACTUAL",
                    P1PoliticalMandateKind::bolster_movement,
                    P1DecreeKind::construction_drive,200},day);
            });
            world.advance_day(action);
        };
        if(expected>0) {
            const auto before=daily::SnapshotCodec::encode(world);
            bool rejected=false;
            try { try_movement(); }
            catch(const std::invalid_argument& e) {
                rejected=std::string(e.what()).find("insufficient unreserved national authority")!=std::string::npos;
            }
            ok(rejected&&daily::SnapshotCodec::encode(world)==before,
               "real mandate escaped the concession's reserved capacity or modified the live state");
        }
        const auto repeal_day=world.day_index();
        phasegh::World::Plan repeal;
        repeal[9].push_back([&](phasegh::Stage& stage) {
            auto& live=stage.phase_c();
            P2Bill bill;
            bill.id="REPEAL";bill.country="A";bill.proposer="PM";
            bill.legal_basis="fixture:repeal";bill.effective_day=repeal_day+1;
            bill.action=P2BillAction::repeal_amendment;bill.target_id=id;
            live.p2_submit_bill(std::move(bill),repeal_day);
            live.p2_approve_bill("REPEAL",{});
            live.p2_promulgate_bill("REPEAL","HEAD",repeal_day);
        });
        world.advance_day(repeal);
        check_budget(expected);
        world.advance_day();
        ok(!ledger(world).p2_amendment_active(id),"concession repeal not legally effective");
        check_budget(0);
        try_movement();
        ok(ledger(world).p1_authority_budget("A").reserved==300,
           "lawful new mandate did not spend the released capacity");
    };
    run("amendment_generic_sponsor_polstr_increase",50);
    run("amendment_generic_sponsor_polstr_approval_increase",50);
    run("amendment_generic_sponsor_polstr_increase_negotiation_1",50);
    run("amendment_generic_sponsor_polstr_increase_negotiation_2",100);
    run("amendment_generic_sponsor_polstr_increase_negotiation_3",150);
    run("amendment_generic_sponsor_polstr_increase_negotiation_3",0,true);
}

int main(){
    try{
        actual_upper_house_personal_terms_and_replay();
        negotiation_and_two_chamber_approval();
        real_economic_and_military_law_rejections();
        negotiated_sponsor_uses_actual_same_country_group();
        remaining_negotiated_sponsor_rows_use_actual_coalition();
        direct_named_group_approval_tracks_law_not_the_coalition();
        canonical_enactment_success_changes_real_review_outcome();
        austrian_law_speed_changes_only_new_review_intervals();
        political_concessions_commit_real_national_authority();
        std::cout<<"P2 advanced politics: nine focused scenarios PASS\n";
        return 0;
    }catch(const std::exception& ex){
        std::cerr<<"P2 ADVANCED POLITICS FAILED: "<<ex.what()<<"\n";
        return 1;
    }
}
