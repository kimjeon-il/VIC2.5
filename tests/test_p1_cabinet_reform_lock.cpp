#include "vic25/daily_snapshot.hpp"
#include <stdexcept>
#include <string>
#include <vector>
using namespace vic25;
using namespace vic25::phasec;

static void require_test(bool value,const char* reason) {
    if(!value)throw std::runtime_error(reason);
}
static const IntegratedWorld& core(const daily::World& world) {
    return world.engine().base().core().core();
}
static void austrian_amendment_blocks_voluntary_cabinet_reformation(
        const std::string& amendment) {
    phasegh::World gh{24000};
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"VIENNA","AUS");
    geo.seed_unit(1,"AUS","AUSTRIA");
    geo.seed_assignment(1,1);
    auto& c=geo.seed_core().seed_core();
    c.seed_actor("AUS",100);
    c.seed_government("AUS","EMPEROR","INCUMBENT",5,3);
    c.seed_p1_constitution("AUS",{P1GovernancePrinciple::monarchy,
        P1PowerDistribution::autocracy,P1LegislatureKind::none,"fixture:monarchy"});
    c.seed_interest_group("LANDOWNERS");
    c.seed_p1_coalition("AUS",{{"LANDOWNERS"},{},false});
    P2BaseLaw autocracy;
    autocracy.id="AUS:AUTOCRACY";autocracy.country="AUS";
    autocracy.domain="distribution_of_power";autocracy.scope="national";
    autocracy.base_choice="law_autocracy";autocracy.legal_basis="fixture:autocracy";
    c.p2_seed_base(autocracy);
    auto world=daily::World::adopt(std::move(gh),43);
    const auto start=world.day_index();
    constexpr const char* lock="country_block_government_reform_bool";
    phasegh::World::Plan legislation;
    legislation[9].push_back([&](phasegh::Stage& stage) {
        auto& live=stage.phase_c();
        P2Bill bill;
        bill.id="CABINET-LOCK";bill.country="AUS";bill.proposer="INCUMBENT";
        bill.legal_basis="fixture:council-amendment";
        bill.action=P2BillAction::attach_amendment;bill.effective_day=start+2;
        bill.proposed_amendment={amendment,"AUS:AUTOCRACY","fixture:council","",
            {{"council-association-clause","AUS",
              P2ClauseKind::political_association_allowed,1}}};
        live.p2_submit_bill(std::move(bill),start);
        live.p2_approve_bill("CABINET-LOCK",{});
        live.p2_promulgate_bill("CABINET-LOCK","EMPEROR",start);
        require_test(live.p2_modifier_ppm("AUS",lock)==0,
                     "pending amendment blocked government reform early");
        live.schedule_p1_cabinet_daily("AUS","EMPEROR","QUEUED",
            {{"LANDOWNERS"},{},false},start+2,"fixture:authorized-before-lock");
        live.schedule_p1_cabinet_daily("AUS","EMPEROR","LATER",
            {{"LANDOWNERS"},{},false},start+5,"fixture:authorized-future-cabinet");
    });
    world.advance_day(legislation);
    world.advance_day();
    require_test(core(world).government("AUS").prime_minister=="INCUMBENT",
                 "queued pre-lock cabinet took effect early");
    world.advance_day();
    require_test(core(world).government("AUS").prime_minister=="QUEUED" &&
                 core(world).p2_modifier_ppm("AUS",lock)>0,
                 "active law must preserve a previously authorized queued cabinet");

    const auto require_blocked=[&](const std::string& prime,P1Coalition coalition,
                                   daily::DayIndex date) {
        const auto prior=daily::SnapshotCodec::encode(world);
        phasegh::World::Plan attempt;
        attempt[9].push_back([&](phasegh::Stage& stage) {
            stage.phase_c().schedule_p1_cabinet_daily("AUS","EMPEROR",prime,
                std::move(coalition),date,"fixture:voluntary-reform");
        });
        bool blocked=false;
        try { world.advance_day(attempt); }
        catch(const std::invalid_argument& error) {
            blocked=std::string(error.what()).find("government reform")!=std::string::npos;
        }
        require_test(blocked,"active Austrian amendment did not block voluntary cabinet reform");
        require_test(daily::SnapshotCodec::encode(world)==prior,
                     "rejected cabinet reform changed pending or committed state");
    };
    const auto next=world.day_index()+1;
    require_blocked("REPLACEMENT",{{"LANDOWNERS"},{},false},next);
    require_blocked("QUEUED",{{},{},false},next);
    require_blocked("REPLACEMENT",{{"LANDOWNERS"},{},true},next);
    require_blocked("QUEUED",{{"LANDOWNERS"},{},true},start+6);

    phasegh::World::Plan succession;
    succession[9].push_back([&](phasegh::Stage& stage) {
        auto& live=stage.phase_c();
        const auto tomorrow=world.day_index()+1;
        live.schedule_p1_head_succession_daily("AUS","EMPEROR","HEIR",
            tomorrow,"fixture:lawful-succession");
        live.schedule_p1_cabinet_daily("AUS","EMPEROR","QUEUED",
            {{"LANDOWNERS"},{},true},tomorrow,"fixture:incumbent-caretaker");
    });
    world.advance_day(succession);
    world.advance_day();
    require_test(core(world).government("AUS").head_of_state=="HEIR" &&
                 core(world).government("AUS").prime_minister=="QUEUED" &&
                 core(world).p1_coalition("AUS").caretaker,
                 "reform lock interfered with head succession or incumbent caretaker");
    while(world.day_index()<=start+6)world.advance_day();
    require_test(core(world).government("AUS").prime_minister=="LATER",
                 "a future caretaker action reversed a previously authorized cabinet");
}

static void incompatible_same_id_cabinet_amendment_does_not_lock() {
    enum class InvalidScope {foreign,wrong_parent,local};
    for(const auto scope:{InvalidScope::foreign,InvalidScope::wrong_parent,
                          InvalidScope::local}) {
        const std::string country=scope==InvalidScope::foreign?"OTHER":"AUS";
        phasegh::World gh{24000};
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"HOME",country);
        geo.seed_unit(1,country,"ADMIN");
        geo.seed_assignment(1,1);
        auto& c=geo.seed_core().seed_core();
        c.seed_actor(country,100);
        c.seed_government(country,"HEAD","INCUMBENT",5,3);
        c.seed_p1_constitution(country,{P1GovernancePrinciple::monarchy,
            P1PowerDistribution::autocracy,P1LegislatureKind::none,"fixture:charter"});
        c.seed_interest_group("LANDOWNERS");
        c.seed_p1_coalition(country,{{"LANDOWNERS"},{},false});
        P2BaseLaw parent{"PARENT",country,"distribution_of_power","",
                         "law_autocracy","fixture:charter"};
        if(scope==InvalidScope::wrong_parent) {
            parent.domain="lawgroup_governance_principles";
            parent.base_choice="law_monarchy";
        }
        if(scope==InvalidScope::local) {
            parent.domain="ordinary_local_policy";
            parent.scope="HOME";parent.tier=P2NormTier::local_ordinance;
        }
        c.p2_seed_base(parent);
        const std::string id="amendment_geheime_staatskonferenz";
        c.p2_seed_amendment({id,parent.id,"fixture:same-id attachment","",
            {{"association",country,P2ClauseKind::political_association_allowed,1}}});
        auto world=daily::World::adopt(std::move(gh),43);
        require_test(core(world).p2_amendment_active(id),
                     "negative fixture did not create a generic active attachment");
        phasegh::World::Plan lawful;
        const auto next=world.day_index()+1;
        lawful[9].push_back([&](phasegh::Stage& stage) {
            stage.phase_c().schedule_p1_cabinet_daily(country,"HEAD","REFORMED",
                {{"LANDOWNERS"},{},false},next,"fixture:voluntary-reformation");
        });
        world.advance_day(lawful);
        world.advance_day();
        require_test(core(world).government(country).prime_minister=="REFORMED",
                     "unqualified Austrian attachment prevented actual constitutional reform");
    }
}

int main() {
    for(const auto* variant:{"amendment_geheime_staatskonferenz",
                             "amendment_geheime_staatskonferenz_metternich",
                             "amendment_geheime_staatskonferenz_kolowrat"})
        austrian_amendment_blocks_voluntary_cabinet_reformation(variant);
    incompatible_same_id_cabinet_amendment_does_not_lock();
}
