#include "vic25/content_snapshot.hpp"
#include "vic25/content_json.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
using namespace vic25;
using namespace vic25::content;

static void need(bool ok,const char* message) {
    if(!ok)throw std::runtime_error(message);
}
struct Fixture {
    phasegh::World gh{24000};
    phaseb::Id citizens{};
    Fixture() {
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"R","A");
        geo.seed_unit(1,"A","ADMIN");
        geo.seed_assignment(1,1);
        auto& c=geo.seed_core().seed_core();
        c.seed_actor("A",1000);
        c.seed_government("A","HEAD","PM",5,3);
        c.seed_p1_constitution("A",{
            phasec::P1GovernancePrinciple::monarchy,
            phasec::P1PowerDistribution::autocracy,
            phasec::P1LegislatureKind::none,"test:constitution"});
        c.seed_interest_group("LABOR");
        c.seed_p1_coalition("A",{{"LABOR"},{},false});
        c.p1_mark_player_country("A",true);
        c.p3_seed_group({"A","LABOR","","labor_rights",
                         "worker_protections",
                         phasec::P3GroupType::trade_unions,800'000,0});
        auto& pop=c.seed_population();
        const auto hh=pop.household_profile({"R","shared","shared",100});
        const auto econ=pop.economic_profile({
            "R","worker","wage","employee",pop.default_education()});
        const auto dem=pop.demographic_profile({
            "R",24000-360,phaseb::Sex::male,"c","r","citizen","free",
            pop.default_health(),phaseb::Service::civilian});
        citizens=pop.seed_population(dem,econ,hh,100,1);
        phasec::P2BaseLaw law;
        law.id="BASE-LABOR";law.country="A";law.domain="labor_rights";
        law.scope="national";law.base_choice="no_workers_rights";
        law.legal_basis="test:original_labor_act";
        c.p2_seed_base(std::move(law));
        gh.validate();
    }
};
static std::shared_ptr<Registry> definitions() {
    auto rules=std::make_shared<Registry>("native-politics-1");
    Decision law;law.id="REFORM_LABOR";
    Effect legal;legal.kind=EffectKind::external;
    legal.external.kind=ExternalKind::political_bill;
    legal.external.law_group="labor_rights";
    legal.external.law_choice="worker_protections";
    law.effects.push_back(legal);
    rules->add(std::move(law));
    Decision mobilize;mobilize.id="MOBILIZE";
    Effect movement;movement.kind=EffectKind::external;
    movement.external.kind=ExternalKind::political_movement;
    movement.external.subject_id="MOV_NATIVE";
    movement.external.law_group="labor_rights";
    movement.external.law_choice="worker_protections";
    movement.external.groups={"LABOR"};
    mobilize.effects.push_back(movement);
    rules->add(std::move(mobilize));
    Event observed;observed.id="MOVEMENT_NOTICED";
    observed.on_action="politics.movement_created";
    observed.choice_timeout=0;observed.default_option="ok";
    Option choice;choice.id="ok";
    choice.effects.push_back({EffectKind::set_flag,"movement_observed"});
    observed.options.push_back(std::move(choice));
    rules->add(std::move(observed));
    Decision invalid;invalid.id="INVALID_LAW";
    Effect broken;broken.kind=EffectKind::external;
    broken.external.kind=ExternalKind::political_bill;
    broken.external.law_group="labor_rights";
    broken.external.law_choice="not_a_law";
    invalid.effects.push_back(broken);
    rules->add(std::move(invalid));
    return rules;
}
int main() {
    try {
        const auto rules=definitions();
        Fixture f;
        auto w=World::adopt(daily::World::adopt(std::move(f.gh),73),rules);
        const Scope scope{"country","A"};
        w.decide("REFORM_LABOR",scope);
        w.advance_day();
        const auto& c=w.native().engine().base().core().core();
        need(c.p2_bills().size()==1&&c.p2_enactments().size()==1,
             "content reform did not create a real staged P2 bill");
        need(c.p2_bills().begin()->second.proposed_base.base_choice==
                 "worker_protections"&&
             c.p2_bills().begin()->second.proposed_base.clauses.empty(),
             "content relabelled an old law instead of selecting the catalogue");

        phasegh::World::Plan join;
        join[9].push_back([slice=f.citizens](phasegh::Stage& gh) {
            gh.phase_c().join_interest_group(slice,"LABOR",70);
        });
        w.advance_day(join);
        w.decide("MOBILIZE",scope);
        w.advance_day();
        need(w.native().engine().base().core().core().p3_movements()
                 .contains("MOV_NATIVE"),
             "content did not create a movement from real IG population");
        w.advance_day();
        need(w.content_state().flags.contains(
                  "movement_observed|country:A"),
             "native P3 movement event did not reach next-day content");
        const auto bytes=SnapshotCodec::encode(w);
        auto replay=SnapshotCodec::decode(bytes,rules);
        need(SnapshotCodec::encode(replay)==bytes,
             "political content lost its native state on snapshot roundtrip");
        replay.decide("INVALID_LAW",scope);
        const auto before=SnapshotCodec::encode(replay);
        bool rejected=false;
        try {replay.advance_day();}
        catch(const std::exception&) {rejected=true;}
        need(rejected&&SnapshotCodec::encode(replay)==before,
             "invalid political content effect bypassed atomic rollback");
        std::cout<<"VIC2.5 POLITICAL CONTENT EXECUTION PASS\n";
    } catch(const std::exception& error) {
        std::cerr<<"POLITICAL CONTENT FAILURE: "<<error.what()<<"\n";
        return 1;
    }
}
