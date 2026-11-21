#include "vic25/phase_gh.hpp"
#include "vic25/daily_snapshot.hpp"
#include <iostream>
#include <stdexcept>
using namespace vic25::phasegh;
static void verify(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class F>static bool rejected(F&& f) {
    try{f();}catch(const std::exception&){return true;}return false;
}
static World fixture(bool route=false,std::int64_t month=100) {
    World w(month);
    auto& e=w.seed_geography();
    e.set_dataset("GH-diplomacy-fixture");
    e.seed_province(1,"RA1","A");
    e.seed_province(4,"RA4","A");
    e.seed_province(2,"RB2","B");
    e.seed_province(3,"RC3","C");
    e.seed_border(1,2);e.seed_border(2,3);e.seed_border(1,4);
    e.seed_unit(11,"A","A");e.seed_unit(22,"B","B");
    e.seed_unit(33,"C","C");
    e.seed_assignment(1,11);e.seed_assignment(4,11);
    e.seed_assignment(2,22);e.seed_assignment(3,33);
    auto& c=e.seed_core().seed_core();
    c.seed_actor("A",500);c.seed_actor("B",500);c.seed_actor("C",500);
    c.seed_actor("SELLER",100);c.seed_actor("BUYER",5000);
    c.seed_actor("ORG:X",0);
    c.seed_stock("SELLER","RA4","iron",8);
    c.seed_market("RA4","iron",100,1,200);
    c.seed_market("RB2","iron",160,1,240);
    c.seed_government("A","A-head","PM_A",5,3);
    c.seed_government("B","B-head","PM_B",5,3);
    c.seed_government("C","C-head","PM_C",5,3);
    w.seed_tariff("B","iron",100000);
    if(route)w.seed_route(Route{"route","SELLER","BUYER","iron",4,2,5,1});
    return w;
}
static TreatyClause clause(const std::string& id,ClauseKind kind,
                           const std::string& obligor,const std::string& beneficiary,
                           const std::string& good="",Money amount=0) {
    TreatyClause t;t.id=id;t.kind=kind;t.obligor=obligor;
    t.beneficiary=beneficiary;t.good=good;t.amount=amount;
    return t;
}
static void national_influence_laws_change_real_daily_infamy() {
    using namespace vic25::phasec;
    struct Case { const char* law; const char* group; int generated; int scope; };
    // scope: 0 = national, 1 = local lookalike, 2 = foreign, 3 = wrong national parent, 4 = custom national domain.
    const Case cases[]={
        {"slave_trade","slavery",100,1},
        {"","",100,0},
        {"sakoku","trade_policy",50,0},
        {"slave_trade","slavery",75,0},
        {"legacy_slavery","slavery",90,0},
        {"laissez_faire","economic_system",100,0},
        {"laissez_faire","economic_system",100,4},
        {"slave_trade","slavery",100,2},
        {"traditionalism","economic_system",100,3},
        {"commercialized_agriculture","land_reform",95,0}
    };
    for(const auto& row:cases) {
        auto w=fixture();
        auto& c=w.seed_geography().seed_core().seed_core();
        for (const auto* country:{"A","B","C"})
            c.seed_p1_constitution(country,{P1GovernancePrinciple::monarchy,
                P1PowerDistribution::autocracy,P1LegislatureKind::none,
                "scenario:influence-constitution"});
        P1StandingEvidence a;
        a.country="A";a.source="scenario:standing:A";a.observed_day=0;
        a.confirmed_rank=P1PowerRank::decentralized_power;a.recognized=false;
        c.p1_record_standing(a);
        a.country="B";a.source="scenario:standing:B";
        a.confirmed_rank=P1PowerRank::insignificant_power;a.recognized=true;
        c.p1_record_standing(a);
        if(*row.law) {
            const std::string country=row.scope==2?"C":"A";
            const bool local=row.scope==1;
            P2BaseLaw base{"influence-fixture",country,
                (local||row.scope==4)?"local_rules":row.group,local?"RA1":"national",
                row.law,"scenario:influence"};
            if(local) base.tier=P2NormTier::local_ordinance;
            c.p2_seed_base(base);
            if(std::string_view(row.group)=="economic_system"||
               std::string_view(row.group)=="land_reform")
                c.p2_seed_amendment({"amendment_extraterritoriality_rights",
                    base.id,"scenario:influence","",{{"binding",country,
                        P2ClauseKind::maximum_import_tariff_ppm,1'000'000}}});
        }
        World::Plan sign;
        sign[9].push_back([](Stage& s) {
            auto d=clause("guarantee",ClauseKind::defense_guarantee,"A","B");
            s.propose_treaty_package("influence-pact",{"A","B"},{d},"A","PM_A");
            s.ratify_package_member("influence-pact","B","PM_B");
        });
        w.advance(sign);
        w.advance({});
        World::Plan event;
        event[9].push_back([](Stage& s){
            s.record_infamy("incident","A","scenario:real-diplomatic-incident",1'000'000);
        });
        w.advance_daily(event,0,false,1,30);
        w.advance_daily({},1,false,2,30);
        w.advance_daily({},2,false,3,30);
        const auto actual=w.infamy_view("A");
        const int expected_bonus=(row.generated-50)*5000;
        const auto expected=1'000'000-2LL*(5'000'000+5LL*expected_bonus)/365;
        if(actual.value_ppm!=expected)
            throw std::runtime_error(std::string("wrong committed infamy ") +
                row.law + " scope "+std::to_string(row.scope)+" expected "+
                std::to_string(expected)+" actual "+std::to_string(actual.value_ppm));
        const auto cap=w.diplomatic_capacity("A");
        verify(cap.influence.complete&&cap.ongoing_pacts==50&&
            cap.influence.generated==row.generated&&
            w.diplomacy().treaties.at("influence-pact").active&&
            actual.last_day==2&&w.infamy_view("B").value_ppm==0,
            "actual ratified guarantee cost, treaty and dated Infamy stay coherent");
    }
}
static void multilateral_treaty_and_trade_restrictions() {
    auto w=fixture(true);
    World::Plan p;
    p[9].push_back([](Stage& s) {
        auto a=clause("zero_tariff",ClauseKind::tariff_exemption,"A","B","iron");
        auto b=clause("volume_limit",ClauseKind::trade_quota,"A","B","iron",3);
        auto c=clause("aid100",ClauseKind::financial_aid,"A","B","",100);
        auto d=clause("mutual_defense",ClauseKind::defense_guarantee,"B","C");
        auto e=clause("research",ClauseKind::research_cooperation,"A","C");
        s.propose_treaty_package("T3",{"A","B","C"},{a,b,c,d,e},"A","PM_A");
        s.ratify_package_member("T3","B","PM_B");
        s.ratify_package_member("T3","C","PM_C");
    });
    w.advance(p);
    verify(w.commerce().fills.size()==1 &&
           w.commerce().fills[0].quantity==5&&
           w.commerce().fills[0].duty_paid==50,
           "new treaty must not apply retroactively at m100");
    verify(!w.diplomacy().treaties.at("T3").active,
           "signatures without stage0 effect");
    World::Plan m101;
    m101[5].push_back([](Stage& s){
        s.disburse_treaty_aid("T3","aid100",100);
    });
    w.advance(m101);
    verify(w.diplomacy().treaties.at("T3").active,
           "all three real ratifications effective m101");
    verify(w.commerce().fills.size()==2 &&
           w.commerce().fills[1].quantity==3 &&
           w.commerce().fills[1].duty_paid==0,
           "actual quota limits second dispatch and tariff exemption applies");
    verify(w.base().core().core().cash("A")==400 &&
           w.base().core().core().cash("B")==650,
           "real C financial flow of aid, prior customs remains untouched");
    verify(w.diplomacy().treaties.at("T3").clauses.at(2).fulfilled==100,
           "aid legal obligation and actual settlement match");
    verify(w.base().core().core().military_control("RB2")=="B",
           "defensive clause alone creates no war or occupation");
    w.validate();
}
static void territory_and_reparations_are_E_and_C_events() {
    auto w=fixture();
    World::Plan m100;
    m100[9].push_back([](Stage& s) {
        auto c=clause("cede_P1",ClauseKind::territorial_cession,"A","B");
        c.province=1;c.receiving_admin=22;
        auto r=clause("repay",ClauseKind::reparations,"A","B","",50);
        s.propose_treaty_package("PEACE",{"A","B"},{c,r},"A","PM_A");
        s.ratify_package_member("PEACE","B","PM_B");
    });
    w.advance(m100);
    verify(w.base().sovereign(1)=="A","stage9 may not move current sovereignty");
    World::Plan m101;
    m101[5].push_back([](Stage& s) {
        s.pay_reparation("H:REPARATION:PEACE:repay",20);
    });
    w.advance(m101);
    verify(w.base().sovereign(1)=="B"&&w.base().leaf(1)==22,
           "E applies sovereignty/administrative transfer from ratified treaty");
    verify(w.base().military_control(1)=="A",
           "real E sovereignty transfer must not silently update military control");
    verify(w.base().core().core().reparations_outstanding(
             "H:REPARATION:PEACE:repay")==30,
           "C reparation payable actual 50 minus real 20");
    verify(w.base().core().core().cash("A")==480 &&
           w.base().core().core().cash("B")==520,
           "C financial payment uses bank deposits, not phantom H cash");
    w.validate();
}
static void organization_voting_veto_membership_change() {
    auto w=fixture();
    World::Plan m100;
    m100[9].push_back([](Stage& s){
        s.create_organization("X",{"A","B"},0,
                               {{"A","PM_A"},{"B","PM_B"}});
        OrganizationRule r;r.quorum=2;r.yes_required=2;r.veto_members={"B"};
        s.configure_organization("X",r,"PM_A");
    });
    w.advance(m100);
    World::Plan m101;
    m101[9].push_back([](Stage& s){
        s.hold_organization_vote("X","ACCEPT_C","admit:C",
           {{"A",VoteChoice::yes},{"B",VoteChoice::yes}},"A","PM_A");
        s.admit_organization_member("X","C","ACCEPT_C","PM_C");
    });
    w.advance(m101);
    verify(!w.diplomacy().organizations.at("X").members.contains("C"),
           "membership admission next-month effect");
    World::Plan m102;
    m102[9].push_back([](Stage& s){
        s.hold_organization_vote("X","DECLINED","military-action",
           {{"A",VoteChoice::yes},{"B",VoteChoice::no},
            {"C",VoteChoice::yes}},"A","PM_A");
        s.leave_organization("X","B","PM_B");
    });
    w.advance(m102);
    verify(w.diplomacy().organizations.at("X").members.contains("C") &&
           !w.diplomacy().organizations.at("X").decisions.at("DECLINED").adopted,
           "actual veto overrides 2:1 and admission effective");
    w.advance();
    const auto& org=w.diplomacy().organizations.at("X");
    verify(org.members.size()==2&&org.members.contains("A")&&
           org.members.contains("C")&&!org.members.contains("B"),
           "exit executed without recreating organization or country");
    verify(w.base().core().core().cash("ORG:X")==0,
           "organization vote creates no money");
}
static void four_dimensional_influence_from_real_events() {
    auto w=fixture();
    w.seed_geography().seed_core().seed_core().seed_stock("A","RA1","arms",5);
    World::Plan m100;
    m100[5].push_back([](Stage& s){
        s.fund_cultural_exchange("SCHOLAR_A_B","A","B",20,"PM_A");
    });
    m100[7].push_back([](Stage& s){
        s.dispatch_military_assistance("ARMS_3","A","B","arms",
                                        1,2,3,50,1,"PM_A");
    });
    w.advance(m100);
    auto initial=w.assess_influence("A","B");
    verify(initial.cultural_known&&initial.cultural_funding==20 &&
           !initial.military_known,"in-transit arms not delivered at m100");
    verify(w.base().core().core().stock("A","RA1","arms")==2,
           "actual military supply leaves real C inventory");
    w.advance();
    const auto v=w.assess_influence("A","B");
    verify(v.military_known&&v.military_goods_delivered==3 &&
           v.military_material_value==150&&v.cultural_funding==20,
           "confirmed delivery and funded academic exchange appear in separate dimensions");
    verify(w.base().core().core().stock("B","RB2","arms")==3,
           "military aid did not create any goods");
    verify(!w.assess_influence("B","A").military_known,
           "influence direction asymmetric");
}
static void crisis_partial_offers_and_compensation() {
    auto w=fixture();
    World::Plan m100;
    m100[9].push_back([](Stage& s){
        s.geography().queue_claim("ECLAIM",2,"A",true);
    });
    w.advance(m100);
    World::Plan m101;
    m101[9].push_back([](Stage& s){
        s.open_crisis("CR",2,"A","B","PM_A");
    });
    w.advance(m101);
    verify(w.base().sovereign(2)=="B" &&
           w.diplomacy().crises.at("CR").status==CrisisStatus::scheduled,
           "claim generated a crisis, not war");
    World::Plan m102;
    m102[9].push_back([](Stage& s){
        s.mediate_crisis("CR","C","PM_C");
        s.submit_crisis_position("CR","A","supports_claim",false,"PM_A");
        s.submit_crisis_position("CR","B","conditional_compromise",true,"PM_B");
        CrisisProposal p;
        p.id="O";p.proposer="C";p.compensation=100;
        p.payer="B";p.recipient="A";p.freeze_claim=true;
        s.offer_crisis_compromise("CR",p,"PM_C");
        s.respond_crisis_offer("CR","O","A",true,"PM_A");
        s.respond_crisis_offer("CR","O","B",true,"PM_B");
    });
    w.advance(m102);
    verify(w.diplomacy().crises.at("CR").status==CrisisStatus::settled &&
           w.base().sovereign(2)=="B","accepted mediation does not transfer sovereignty");
    World::Plan m103;
    m103[5].push_back([](Stage& s){
        s.pay_reparation("H:CRISIS-REPARATION:CR:O",20);
    });
    w.advance(m103);
    verify(w.base().spatial().claims.at(2).empty() &&
           w.base().core().core().reparations_outstanding(
             "H:CRISIS-REPARATION:CR:O")==80,
           "E claimant withdrawal and C installment settled once");
    verify(!w.advise_crisis("CR","C").can_mediate,
           "AI read-only assessment of closed crisis");
}
static void sakoku_blocks_only_new_claimant_initiation() {
    using namespace vic25;
    auto gh=fixture(false,24000);
    auto& c=gh.seed_geography().seed_core().seed_core();
    c.seed_p1_constitution("A",{phasec::P1GovernancePrinciple::monarchy,
        phasec::P1PowerDistribution::autocracy,phasec::P1LegislatureKind::none,
        "fixture:executive-authority"});
    c.p2_seed_base({"opening-trade-law","A","trade_policy","national",
                    "law_free_trade","fixture:opening-law"});
    auto world=daily::World::adopt(std::move(gh),19);
    World::Plan claims;
    claims[9].push_back([](Stage& s) {
        s.geography().queue_claim("A-CLAIM",2,"A",true);
        s.geography().queue_claim("B-CLAIM-ON-A",1,"B",true);
        s.geography().queue_claim("B-CLAIM-ON-C",3,"B",true);
    });
    world.advance_day(claims);
    while(world.date().month==1)world.advance_day();

    constexpr const char* key="country_disallow_aggressive_plays_bool";
    const auto effective=world.day_index()+2;
    World::Plan opening;
    opening[9].push_back([effective](Stage& s) {
        auto& live=s.phase_c();
        phasec::P2Bill bill;
        bill.id="SAKOKU-BILL";bill.country="A";bill.proposer="PM_A";
        bill.legal_basis="fixture:trade-law";bill.effective_day=effective;
        bill.proposed_base={"SAKOKU-LAW","A","trade_policy","national",
                            "law_sakoku","fixture:trade-law"};
        live.p2_submit_bill(std::move(bill),effective-2);
        live.p2_approve_bill("SAKOKU-BILL",{});
        live.p2_promulgate_bill("SAKOKU-BILL","A-head",effective-2);
        verify(live.p2_modifier_ppm("A",key)==0,
               "promulgated but not effective Sakoku locked crisis opening");
        s.open_crisis("EXISTING",2,"A","B","PM_A");
    });
    world.advance_day(opening);
    world.advance_day();
    world.advance_day();
    const auto& c2=world.engine().base().core().core();
    verify(c2.p2_modifier_ppm("A",key)>0 &&
           world.engine().diplomacy().crises.at("EXISTING").status==
               CrisisStatus::scheduled &&
           world.engine().base().spatial().claims.at(2).contains("A"),
           "active Sakoku must not cancel an earlier crisis or its real claim");

    const auto before=daily::SnapshotCodec::encode(world);
    World::Plan prohibited;
    prohibited[9].push_back([](Stage& s) {
        s.open_crisis("PROHIBITED",2,"A","B","PM_A");
    });
    bool blocked=false;
    try { world.advance_day(prohibited); }
    catch(const std::invalid_argument& error) {
        blocked=std::string(error.what()).find("H_ERR_AGGRESSIVE_PLAYS_PROHIBITED")
            !=std::string::npos;
    }
    verify(blocked,"active Sakoku did not reject a claimant-initiated crisis");
    verify(daily::SnapshotCodec::encode(world)==before,
           "rejected crisis modified the live claim or its transaction");

    World::Plan permitted;
    permitted[9].push_back([](Stage& s) {
        s.open_crisis("RESPONDENT",1,"B","A","PM_B");
        s.open_crisis("MEDIATION",3,"B","C","PM_B");
    });
    world.advance_day(permitted);
    while(world.date().month==2)world.advance_day();
    World::Plan defense_and_mediation;
    defense_and_mediation[9].push_back([](Stage& s) {
        s.submit_crisis_position("RESPONDENT","A","defends_status_quo",true,"PM_A");
        s.mediate_crisis("MEDIATION","A","PM_A");
        s.mediate_crisis("EXISTING","C","PM_C");
        s.resolve_crisis_without_war("EXISTING","PM_A","PM_B");
    });
    world.advance_day(defense_and_mediation);
    const auto& d=world.engine().diplomacy();
    verify(d.crises.at("RESPONDENT").status==CrisisStatus::active &&
           d.crises.at("MEDIATION").mediator=="A" &&
           d.crises.at("EXISTING").status==CrisisStatus::settled &&
           d.wars.empty(),
           "Sakoku interfered with defense, mediation or an already-open crisis");
}

int main(){
    try{
        sakoku_blocks_only_new_claimant_initiation();
        national_influence_laws_change_real_daily_infamy();
        multilateral_treaty_and_trade_restrictions();
        territory_and_reparations_are_E_and_C_events();
        organization_voting_veto_membership_change();
        four_dimensional_influence_from_real_events();
        crisis_partial_offers_and_compensation();
        std::cout<<"Phase H extended diplomacy: 5 integration scenarios PASS\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"Phase H extended diplomacy FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
