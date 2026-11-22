#include "vic25/phase_gh.hpp"
#include "vic25/daily_snapshot.hpp"
#include "vic25/p0_daily.hpp"
#include <iostream>
#include <sstream>
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
static vic25::daily::World two_recorded_wars_between_the_same_countries() {
    using namespace vic25;
    auto gh=fixture(false,24000);
    auto& c=gh.seed_geography().seed_core().seed_core();
    for(const auto& [actor,region]:std::array<std::pair<const char*,const char*>,2>{{
                        {"A","RA1"},{"B","RB2"}}}) {
        const std::string unit=std::string("TREATY-EXAMPLE-UNIT-")+actor;
        c.seed_unit(unit);
        c.seed_army_deployment(unit,actor,region);
        c.seed_stock(unit,region,"ammunition",2);
        auto& pop=c.seed_population();
        const auto h=pop.household_profile({region,"shared","shared",2});
        const auto e=pop.economic_profile({region,"soldier","wage","employee",
                                          pop.default_education()});
        const auto d=pop.demographic_profile({region,23640,phaseb::Sex::male,
                "culture","faith","citizen","free",pop.default_health(),phaseb::Service::active});
        c.seed_active(unit,pop.seed_population(d,e,h,2));
    }
    auto world=daily::World::adopt(std::move(gh),21);
    World::Plan claim;
    claim[9].push_back([](Stage& h) {
        h.geography().queue_claim("DISPUTED-TITLE",2,"A",true);
    });
    world.advance_day(claim);
    while(world.date()<daily::Date{2000,2,1})world.advance_day();
    World::Plan disputes;
    disputes[9].push_back([](Stage& h) {
        h.open_crisis("OLD-CONFLICT",2,"A","B","PM_A");
        h.open_crisis("OTHER-CONFLICT",2,"A","B","PM_A");
    });
    world.advance_day(disputes);
    while(world.date()<daily::Date{2000,3,1})world.advance_day();
    World::Plan go;
    go[9].push_back([](Stage& h) {
        verify(h.begin_war_from_crisis("OLD-CONFLICT","A","PM_A")&&
               h.begin_war_from_crisis("OTHER-CONFLICT","A","PM_A"),
               "fixture could not create two real active conflict records");
    });
    world.advance_day(go);
    return world;
}

enum class LegacyTermination { none, partial, pending, applied };
static std::string legacy_war_treaty_checkpoint(
        const World& world,const std::string& treaty_id,ClauseKind kind,
        LegacyTermination termination=LegacyTermination::none) {
    // Old approved terms lacked a conflict field. Encode their exact 0.6 shape,
    // using the live two-war journal and unchanged underlying C/D/E checkpoint.
    auto h=world.diplomacy();
    auto g=world.commerce();
    Treaty old;
    old.id=treaty_id;old.country_a="A";old.country_b="B";
    old.parties={"A","B"};old.ratifications=old.parties;
    old.ratified_a=old.ratified_b=old.active=true;
    if(termination==LegacyTermination::partial)old.termination_approvals={"A"};
    if(termination==LegacyTermination::pending||termination==LegacyTermination::applied)
        old.termination_approvals=old.parties;
    old.pending_termination=termination==LegacyTermination::pending;
    old.agreed_month=world.month()-1;old.effective_month=world.month();
    const bool automatic=treaty_id.starts_with("AUTO:");
    auto war=clause("end-hostilities",kind,automatic?"A":"B",automatic?"B":"A");
    war.effective=old.effective_month;war.queued=true;
    old.clauses={war};
    if(!automatic) {
        auto aid=clause("independent-aid",ClauseKind::financial_aid,"A","B","",3);
        aid.effective=old.effective_month;aid.queued=true;
        old.clauses.push_back(aid);
    }
    h.treaties.emplace(old.id,std::move(old));
    std::ostringstream out(std::ios::binary|std::ios::out);
    persisted::Writer a{out};
    a(std::string(SnapshotCodec::magic));a(std::string(SnapshotCodec::war_version));
    a(std::string(g_version));a(std::string(h_version));
    a(vic25::phasee::SnapshotCodec::encode(world.base()));
    persisted::g_state(a,g,true,true,false,false);
    persisted::h_state(a,h,true,true);
    if(!h.infamy.empty()||!h.infamy_events.empty()) {
        a(std::string("VIC25_PHASE_GH_INFAMY_1"));
        persisted::checked_map(a,h.infamy,persisted::infamy_record<decltype(a)>);
        persisted::checked_vector(a,h.infamy_events,persisted::infamy_event<decltype(a)>);
    }
    const auto payload=out.str();
    a(vic25::phasec::persistence_detail::checksum(payload));
    return out.str();
}

static void old_unbound_ordinary_treaty_cannot_suspend_or_end_a_real_war() {
    auto source=two_recorded_wars_between_the_same_countries();
    for(const auto kind:{ClauseKind::ceasefire,ClauseKind::peace_settlement}) {
        auto replay=SnapshotCodec::decode(
            legacy_war_treaty_checkpoint(source.engine(),"ORDINARY-UNBOUND",kind));
        World::Plan next;
        const auto before=replay.base().core().core().cash("B");
        next[5].push_back([](Stage& h) {
            h.disburse_treaty_aid("ORDINARY-UNBOUND","independent-aid",3);
        });
        replay.advance_daily(next,source.day_index(),false,source.date().day,31,false,366);
        verify(replay.base().core().core().cash("B")==before+3,
               "historic war term incorrectly discarded an independent financial obligation");
        for(const auto& [id,war]:replay.diplomacy().wars) {
            (void)id;
            verify(war.active&&!war.armistice&&war.ended_day<0,
                   "legacy unbound ordinary treaty wrongly controlled a real war between the same countries");
        }
    }
}

static void legacy_automatic_war_scope_is_preserved_without_guessing() {
    auto source=two_recorded_wars_between_the_same_countries();
    const auto run=[&](const std::string& name,ClauseKind kind,bool trusted) {
        auto restored=SnapshotCodec::decode(
            legacy_war_treaty_checkpoint(source.engine(),name,kind));
        verify(restored.diplomacy().treaties.at(name).clauses.front().war_id==
               (trusted?"WAR:OLD-CONFLICT":""),
               "legacy automatic treaty resolved an unproven conflict identity");
        restored.advance_daily({},source.day_index(),false,source.date().day,31,false,366);
        const auto& selected=restored.diplomacy().wars.at("WAR:OLD-CONFLICT");
        const auto& other=restored.diplomacy().wars.at("WAR:OTHER-CONFLICT");
        verify(other.active&&!other.armistice&&other.ended_day<0,
               "legacy automatic treaty spilled into another conflict");
        verify(selected.active==!trusted&&
               selected.armistice==(trusted&&kind==ClauseKind::ceasefire)&&
               (selected.ended_day>=0)==(trusted&&kind==ClauseKind::peace_settlement),
               "old proven automatic settlement failed to reconcile its recorded war");
        auto check=SnapshotCodec::decode(SnapshotCodec::encode(restored));
        verify(check.diplomacy().treaties.at(name).clauses.front().war_id==
                   (trusted?"WAR:OLD-CONFLICT":""),
               "automatic migration scope did not roundtrip");
    };
    run("AUTO:CEASEFIRE:WAR:OLD-CONFLICT",ClauseKind::ceasefire,true);
    run("AUTO:PEACE:WAR:OLD-CONFLICT",ClauseKind::peace_settlement,true);
    run("AUTO:PEACE:WAR:OLD-CONFLICT",ClauseKind::ceasefire,false);
    run("AUTO:PEACE:WAR:DOES-NOT-EXIST",ClauseKind::peace_settlement,false);
}

static void signed_and_dated_war_terms_bind_only_the_named_conflict() {
    using namespace vic25;
    auto world=two_recorded_wars_between_the_same_countries();
    constexpr auto war_id="WAR:OLD-CONFLICT";
    constexpr auto truce_id="MANUAL-TRUCE";
    constexpr auto peace_id="MANUAL-PEACE";
    World::Plan observe_only;
    observe_only[9].push_back([](Stage& h) {
        verify(h.diplomacy().wars.size()==2,"fixture lost a recorded conflict");
    });
    const auto until=[&](daily::Date date) {
        while(world.date()<date)world.advance_day(observe_only);
        verify(world.date()==date,"fixture advanced past the legal date");
    };
    const auto other=[&]() -> const WarState& {
        return world.engine().diplomacy().wars.at("WAR:OTHER-CONFLICT");
    };
    const auto selected=[&]() -> const WarState& {
        return world.engine().diplomacy().wars.at(war_id);
    };
    const auto unaffected=[&] {
        verify(other().active&&!other().armistice&&other().ended_day<0,
               "treaty changed another conflict with the same states");
    };
    const auto assert_rejected=[&](World::Task task) {
        const auto save=daily::SnapshotCodec::encode(world);
        World::Plan plan;
        plan[9].push_back(std::move(task));
        verify(rejected([&]{world.advance_day(plan);})&&
               daily::SnapshotCodec::encode(world)==save,
               "invalid war treaty or signature accepted, or left partial effects");
    };
    assert_rejected([](Stage& h) {
        auto c=clause("unbound",ClauseKind::peace_settlement,"B","A");
        h.propose_treaty_package("UNBOUND",{"A","B"},{c},"A","PM_A");
    });
    assert_rejected([](Stage& h) {
        auto c=clause("unknown",ClauseKind::ceasefire,"B","A");
        c.war_id="WAR:UNKNOWN";
        h.propose_treaty_package("UNKNOWN",{"A","B"},{c},"A","PM_A");
    });
    assert_rejected([&](Stage& h) {
        auto c=clause("wrong-country",ClauseKind::ceasefire,"C","A");
        c.war_id=war_id;
        h.propose_treaty_package("WRONG-COUNTRY",{"A","C"},{c},"A","PM_A");
    });
    assert_rejected([&](Stage& h) {
        auto c=clause("wrong-kind",ClauseKind::financial_aid,"B","A","",10);
        c.war_id=war_id;
        h.propose_treaty_package("WRONG-KIND",{"A","B"},{c},"A","PM_A");
    });
    World::Plan offer;
    offer[9].push_back([&](Stage& h) {
        auto c=clause("specific-ceasefire",ClauseKind::ceasefire,"B","A");
        c.war_id=war_id;c.expires=24004; // through May
        h.propose_treaty_package(truce_id,{"A","B"},{c},"A","PM_A");
    });
    world.advance_day(offer);
    until({2000,4,1});world.advance_day();
    verify(!world.engine().diplomacy().treaties.at(truce_id).active&&selected().active,
           "a single signatory activated a ceasefire");
    assert_rejected([&](Stage& h){h.ratify_package_member(truce_id,"B","PM_A");});
    World::Plan consent;consent[9].push_back([&](Stage& h) {
        h.ratify_package_member(truce_id,"B","PM_B");
    });
    world.advance_day(consent);
    until({2000,5,1});
    verify(selected().active&&!selected().armistice,
           "lawful ratification applied prior to its effective month");
    auto replay=daily::SnapshotCodec::decode(daily::SnapshotCodec::encode(world));
    world.advance_day();replay.advance_day();
    verify(daily::SnapshotCodec::encode(world)==daily::SnapshotCodec::encode(replay)&&
           !selected().active&&selected().armistice&&selected().ended_day<0,
           "current conflict ceasefire failed on its effective date or replay");
    unaffected();
    until({2000,6,1});
    verify(selected().armistice,"ceasefire expired too early");
    World::Plan peace;
    peace[9].push_back([&](Stage& h) {
        auto c=clause("specific-peace",ClauseKind::peace_settlement,"B","A");
        c.war_id=war_id;
        h.propose_treaty_package(peace_id,{"A","B"},{c},"B","PM_B");
    });
    world.advance_day(peace);
    verify(selected().active&&!selected().armistice&&selected().ended_day<0,
           "expiry did not resume the underlying war");
    World::Plan consent_a;consent_a[9].push_back([&](Stage& h) {
        h.ratify_package_member(peace_id,"A","PM_A");
    });
    world.advance_day(consent_a);
    until({2000,7,1});
    const auto effect=world.day_index();
    auto resume=daily::SnapshotCodec::decode(daily::SnapshotCodec::encode(world));
    world.advance_day();resume.advance_day();
    verify(daily::SnapshotCodec::encode(world)==daily::SnapshotCodec::encode(resume)&&
           !selected().active&&!selected().armistice&&selected().ended_day==effect&&
           world.engine().base().sovereign(2)=="B",
           "scoped ratified peace failed on due date, or transferred sovereignty");
    unaffected();
    assert_rejected([&](Stage& h) {
        auto c=clause("ended-war",ClauseKind::ceasefire,"A","B");
        c.war_id=war_id;
        h.propose_treaty_package("ENDED-WAR",{"A","B"},{c},"A","PM_A");
    });
}

static void fiscal_treaty_mandates_do_not_sign_named_war_terms() {
    using namespace vic25;
    const auto run=[](ClauseKind kind,bool mandate_is_obligor,bool permitted) {
        auto world=two_recorded_wars_between_the_same_countries();
        constexpr auto war_id="WAR:OLD-CONFLICT";
        constexpr auto other_id="WAR:OTHER-CONFLICT";
        World::Plan offer;
        offer[9].push_back([kind,mandate_is_obligor](Stage& h) {
            auto term=clause("war-term",kind,mandate_is_obligor?"B":"A",
                                              mandate_is_obligor?"A":"B");
            term.war_id="WAR:OLD-CONFLICT";
            h.propose_treaty_package("WAR-OFFER",{"A","B","C"},{term},"A","PM_A");
            const auto aid=clause("fiscal-aid",ClauseKind::financial_aid,"A","B","",3);
            h.propose_treaty_package("FISCAL-OFFER",{"A","B"},{aid},"A","PM_A");
        });
        world.advance_day(offer);
        p0::Policies policy;
        policy.allow_autonomous_war_peace=false;
        policy.allow_endogenous_crisis_wars=false;
        policy.allow_autonomous_naval_air_ai=false;
        policy.daily_food_units_per_person=0;
        policy.monthly_clothing_units_per_person=0;
        policy.treaty_ratifications.push_back({"B","PM_B",10,false,permitted,false});
        policy.treaty_ratifications.push_back({"C","PM_C",10,false,false,false});
        p0::DailyGame game(std::move(world),policy);
        game.advance_day();
        const auto& signed_pacts=game.world().engine().diplomacy().treaties;
        verify(signed_pacts.at("FISCAL-OFFER").ratifications.contains("B")&&
               signed_pacts.at("WAR-OFFER").ratifications.contains("C"),
               "military opt-in disturbed fiscal action or the neutral signatory");
        verify(signed_pacts.at("WAR-OFFER").ratifications.contains("B")==permitted,
               "fiscal-only mandate signed a real war term as a named signatory");
        while(game.world().date()<=daily::Date{2000,4,1}) game.advance_day();
        const auto& h=game.world().engine().diplomacy();
        const auto& war=h.wars.at(war_id);
        const auto& other=h.wars.at(other_id);
        verify(h.treaties.at("FISCAL-OFFER").active&&other.active&&
               !other.armistice&&other.ended_day<0,
               "treaty AI disturbed fiscal activation or an unrelated conflict");
        if(!permitted)
            verify(war.active&&!war.armistice&&war.ended_day<0,
                   "a refused mandate nevertheless changed named war state");
        else if(kind==ClauseKind::ceasefire)
            verify(!war.active&&war.armistice&&war.ended_day<0,
                   "authorized ceasefire failed to pause the named war");
        else
            verify(!war.active&&!war.armistice&&war.ended_day>=0,
                   "authorized peace failed to close the named war");
    };
    for(const auto kind:{ClauseKind::ceasefire,ClauseKind::peace_settlement})
        for(const bool named_obligor:{false,true})
            for(const bool military_authority:{false,true})
                run(kind,named_obligor,military_authority);
}

static void jointly_terminated_war_term_stays_terminated_after_monthly_rollover() {
    using namespace vic25;
    auto world=two_recorded_wars_between_the_same_countries();
    constexpr auto named="WAR:OLD-CONFLICT";
    constexpr auto distinct="WAR:OTHER-CONFLICT";
    constexpr auto pact="JOINT-TRUCE";
    World::Plan observe;
    observe[9].push_back([](Stage& h) {
        verify(h.diplomacy().wars.size()==2,"termination fixture lost real conflicts");
    });
    const auto reach=[&](daily::Date date) {
        while(world.date()<date)world.advance_day(observe);
        verify(world.date()==date,"termination fixture skipped its legal date");
    };
    const auto assess=[&](bool paused) {
        const auto& d=world.engine().diplomacy();
        const auto& named_war=d.wars.at(named);
        verify(d.treaties.at(pact).active==paused&&named_war.active==!paused&&
               named_war.armistice==paused&&named_war.ended_day<0,
               "signed termination or renewal did not control the actual named war");
        const auto& other=d.wars.at(distinct);
        verify(other.active&&!other.armistice&&other.ended_day<0,
               "joint termination controlled a different war with the same countries");
    };
    const auto propose=[&](const std::string& id) {
        World::Plan action;
        action[9].push_back([&](Stage& h) {
            auto term=clause("truce",ClauseKind::ceasefire,"A","B");
            term.war_id=named;
            h.propose_treaty_package(id,{"A","B"},{term},"A","PM_A");
        });
        world.advance_day(action);
    };
    const auto ratify=[&](const std::string& id) {
        World::Plan action;
        action[9].push_back([&](Stage& h){h.ratify_package_member(id,"B","PM_B");});
        world.advance_day(action);
    };
    const auto terminate=[&](const char* country,const char* prime) {
        World::Plan action;
        action[9].push_back([&](Stage& h){h.terminate_treaty(pact,country,prime);});
        world.advance_day(action);
    };
    propose(pact);ratify(pact);
    reach({2000,4,1});world.advance_day(observe);assess(true);
    terminate("A","PM_A");assess(true);
    terminate("B","PM_B");assess(true); // no retroactive same-day effect
    const std::set<std::string> parties={"A","B"};
    reach({2000,5,1});world.advance_day(observe);assess(false);
    reach({2000,6,1});
    auto replay=daily::SnapshotCodec::decode(daily::SnapshotCodec::encode(world));
    world.advance_day(observe);replay.advance_day(observe);assess(false);
    const auto& old=world.engine().diplomacy().treaties.at(pact);
    verify(daily::SnapshotCodec::encode(world)==daily::SnapshotCodec::encode(replay)&&
           old.termination_approvals==parties&&old.ratifications==parties,
           "termination signature history could not replay unchanged");
    propose("VALID-RENEWAL");ratify("VALID-RENEWAL");
    reach({2000,7,1});world.advance_day(observe);
    const auto& d=world.engine().diplomacy();
    verify(!d.treaties.at(pact).active&&d.treaties.at("VALID-RENEWAL").active&&
           d.wars.at(named).armistice&&d.wars.at(distinct).active,
           "new independent consent did not renew the specifically named war term");
}

static void old_saves_keep_only_proven_termination_state() {
    const auto source=two_recorded_wars_between_the_same_countries();
    constexpr auto pact="AUTO:CEASEFIRE:WAR:OLD-CONFLICT";
    for(const auto state:{LegacyTermination::none,LegacyTermination::partial,
                          LegacyTermination::pending,LegacyTermination::applied}) {
        auto world=SnapshotCodec::decode(legacy_war_treaty_checkpoint(
            source.engine(),pact,ClauseKind::ceasefire,state));
        const auto& saved=world.diplomacy().treaties.at(pact);
        const bool terminal=state==LegacyTermination::applied;
        verify(saved.active==!terminal&&
               saved.pending_termination==(state==LegacyTermination::pending)&&
               saved.ratifications==std::set<std::string>{"A","B"},
               "legacy termination was inferred without complete dated evidence");
        world.advance_daily({},source.day_index(),false,source.date().day,31,false,366);
        verify(world.diplomacy().wars.at("WAR:OLD-CONFLICT").active==terminal&&
               world.diplomacy().wars.at("WAR:OTHER-CONFLICT").active,
               "legacy authority changed the wrong war or overrode genuine terms");
    }
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
        old_unbound_ordinary_treaty_cannot_suspend_or_end_a_real_war();
        legacy_automatic_war_scope_is_preserved_without_guessing();
        signed_and_dated_war_terms_bind_only_the_named_conflict();
        fiscal_treaty_mandates_do_not_sign_named_war_terms();
        jointly_terminated_war_term_stays_terminated_after_monthly_rollover();
        old_saves_keep_only_proven_termination_state();
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
