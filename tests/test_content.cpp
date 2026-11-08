#include "vic25/content_snapshot.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <set>

using namespace vic25;
using namespace vic25::content;

static void check(bool result,const char* why) {
    if(!result)throw std::runtime_error(why);
}
template<class F>static bool rejects(F&& f) {
    try{f();}catch(const std::exception&){return true;}
    return false;
}
static std::shared_ptr<Registry> rules() {
    auto r=std::make_shared<Registry>("ruleset-2026-11-08");
    Event e;e.id="policy";e.on_action="government_reformed";
    e.trigger=Condition::domain("country.exists");
    e.once_scope=true;e.choice_timeout=8;
    Option o;o.id="support";
    o.effects.push_back({EffectKind::add_counter,"support",2});
    e.options.push_back(o);
    r->add(std::move(e));

    Event e2;e2.id="world-once";e2.on_action="world_crisis";
    e2.once_world=true;e2.choice_timeout=1;e2.default_option="ok";
    Option o2;o2.id="ok";
    o2.effects.push_back({EffectKind::set_flag,"global-event-happened"});
    e2.options.push_back(o2);
    r->add(std::move(e2));

    Event e3;e3.id="followup";e3.choice_timeout=1;e3.default_option="ok";
    Option o3;o3.id="ok";
    o3.effects.push_back({EffectKind::add_counter,"followup",1});
    e3.options.push_back(o3);
    r->add(std::move(e3));

    Decision d;d.id="modernize";
    d.visible=Condition::domain("country.exists");
    d.available=Condition::domain("country.cash",1);
    d.once_scope=false;d.cooldown_days=3;d.ai_weight=7;
    d.effects.push_back({EffectKind::set_flag,"investment_plan"});
    d.effects.push_back({EffectKind::schedule_event,"followup",0,1});
    r->add(std::move(d));

    Journal j;j.id="political-reform";j.group="politics";
    j.activation=Condition::domain("country.exists");
    j.progress_metric="local:support";j.progress_target=4;
    j.timeout_days=10;
    Outcome good;good.id="reform";good.priority=10;
    good.trigger={Test::counter_ge,"support",4,{}};
    Outcome partial;partial.id="compromise";partial.priority=1;
    partial.trigger=Condition::flag("investment_plan");
    Outcome late;late.id="expired";late.on_timeout=true;
    j.outcomes={good,partial,late};
    r->add(std::move(j));

    Journal worldwide;worldwide.id="world-revolution";worldwide.group="politics";
    worldwide.world=true;
    worldwide.activation=Condition{};
    worldwide.timeout_days=1;
    Outcome end;end.id="transformed";end.on_timeout=true;
    worldwide.outcomes={end};
    r->add(std::move(worldwide));

    Formation f;f.id="independence";f.target_country="B";
    f.eligibility=Condition::domain("country.exists");
    f.required_provinces={1,2};f.needed_ppm=1'000'000;
    f.external.kind=ExternalKind::independence;
    f.external.target_country="B";
    f.external.admin_id=201;f.external.admin_name="NEW_STATE";
    f.external.head="HEAD_B";f.external.prime="PRIME_B";
    f.external.provinces={2};f.external.seats=7;f.external.quorum=4;
    r->add(std::move(f));

    return r;
}
static phasegh::World seed() {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.set_dataset("content-synthetic");
    geo.seed_province(1,"R1","A");
    geo.seed_province(2,"R2","A");
    geo.seed_unit(101,"A","A_STATE");
    geo.seed_assignment(1,101);
    geo.seed_assignment(2,101);
    auto& c=geo.seed_core().seed_core();
    c.seed_actor("A",1000);
    c.seed_government("A","HEAD_A","PRIME_A",5,3);
    gh.validate();
    return gh;
}
static World world(std::shared_ptr<const Registry> r) {
    return World::adopt(daily::World::adopt(seed(),123),std::move(r));
}
static void content_local_rules() {
    Registry r("local");
    Event event;event.id="E";event.on_action="x";
    event.once_scope=true;event.choice_timeout=2;
    Option option;option.id="yes";
    option.effects.push_back({EffectKind::add_counter,"votes",1});
    event.options.push_back(option);
    r.add(event);
    State state;initialize(r,state);
    const auto scope=Scope{"country","A"};
    notify(r,state,"x",scope,5);
    notify(r,state,"x",scope,5);
    const ReadDomain read=[](const Scope&,const std::string&) {
        return std::optional<Number>{};
    };
    const ApplyExternal effect=[](const Scope&,const External&,
                                  const std::string&) {
        throw std::runtime_error("no external effects expected");
    };
    step(r,state,5,98,read,effect);
    check(state.pending_choices.size()==1,"once-per-scope event duplicate");
    const auto occurrence=state.pending_choices.begin()->first;
    choose(state,occurrence,"yes");
    step(r,state,6,98,read,effect);
    check(state.counters.at(scoped("votes",scope))==1&&
          state.pending_choices.empty(),"content event choice missing");
    notify(r,state,"x",scope,7);
    step(r,state,7,98,read,effect);
    check(state.pending_choices.empty(),"one-shot event fired again");

    State a,b;initialize(r,a);initialize(r,b);
    a.ai_scopes.insert(scope);b.ai_scopes.insert(scope);
    notify(r,a,"x",scope,10);notify(r,b,"x",scope,10);
    step(r,a,10,42,read,effect);step(r,b,10,42,read,effect);
    check(a.pending_choices.empty()&&a.history.size()==b.history.size()&&
          a.counters==b.counters,"AI event choice nondeterministic");

    check(rejects([&]{r.add(event);}),"duplicate content ID accepted");
    Condition recursion;
    recursion.kind=Test::all;
    recursion.children.push_back(Condition{});
    r.add(Decision{"VALID",recursion,Condition{},{}});
    check(rejects([&]{r.add(Decision{"BROKEN",
          Condition{Test::all,"",0,{}},Condition{}, {}});}),
          "empty AST branch accepted");
}
static void event_choice_and_checkpoint() {
    const auto reg=rules();
    auto w=world(reg);
    w.notify("government_reformed",{"country","A"});
    w.advance_day();
    check(w.content_state().pending_choices.size()==1,
          "event did not surface a player choice");
    const auto snapshot=SnapshotCodec::encode(w);
    auto copy=SnapshotCodec::decode(snapshot,reg);
    check(SnapshotCodec::encode(copy)==snapshot,
          "pending event choices not restored");
    const auto occ=w.content_state().pending_choices.begin()->first;
    w.choose(occ,"support");
    copy.choose(occ,"support");
    w.advance_day();copy.advance_day();
    check(w.content_state().counters.at("support|country:A")==2 &&
          SnapshotCodec::encode(w)==SnapshotCodec::encode(copy),
          "chosen event did not replay or mutated native snapshot");

    w.notify("government_reformed",{"country","A"});
    w.advance_day();
    check(w.content_state().pending_choices.empty(),
          "once-scoped event returned after save");

    auto bad=snapshot;
    bad[bad.size()/2]=static_cast<char>(bad[bad.size()/2]^1);
    check(rejects([&]{(void)SnapshotCodec::decode(bad,reg);}),
          "corrupted content checkpoint passed checksum");
    auto changed=std::make_shared<Registry>("ruleset-wrong");
    check(rejects([&]{(void)SnapshotCodec::decode(snapshot,changed);}),
          "changed content ruleset silently accepted");
}
static void decision_cooldown_ai_and_followup() {
    auto reg=rules();
    auto w=world(reg);
    const Scope a{"country","A"};
    w.decide("modernize",a);
    w.advance_day();
    check(w.content_state().flags.contains("investment_plan|country:A"),
          "decision effect missing");
    w.decide("modernize",a);
    const auto before=SnapshotCodec::encode(w);
    check(rejects([&]{w.advance_day();}) &&
          SnapshotCodec::encode(w)==before,
          "invalid repeat decision failed to roll back whole day");
    // Restore to a clean world without the deliberately queued invalid action.
    auto ai=world(reg);ai.register_ai(a);ai.ai_check(a);
    ai.advance_day();
    check(ai.content_state().last_decisions.contains("modernize|country:A"),
          "AI did not use valid decision");
    ai.advance_day();
    check(ai.content_state().pending_choices.empty() &&
          ai.content_state().counters.at("followup|country:A")==1,
          "AI did not resolve next-day followup using actual choice rules");
    ai.advance_day();
    check(ai.content_state().counters.at("followup|country:A")==1,
          "AI repeated an already consumed followup");
}
static void journal_multibranch_and_world() {
    auto reg=rules();
    auto w=world(reg);
    const Scope a{"country","A"};
    w.start("political-reform",a);
    w.start("world-revolution",a);
    w.start("world-revolution",{"country","OTHER"});
    w.advance_day();
    check(w.content_state().active_journals.size()==2,
          "global journal was duplicated for each participant");
    const auto jp=scoped("political-reform",a);
    check(progress(*reg,w.content_state(),jp,
          [](const Scope&,const std::string&)->std::optional<Number> {
              return std::nullopt;
          }).current==0,"journal progress source invented");

    // Win condition outranks compromise and uses the same real local counter.
    w.notify("government_reformed",a);
    w.advance_day();
    auto occ=w.content_state().pending_choices.begin()->first;
    w.choose(occ,"support");w.advance_day();
    w.notify("government_reformed",{"country","OTHER"});
    // OTHER has no government, so the second event must not fire.
    w.advance_day();
    check(w.content_state().pending_choices.empty(),
          "historically invalid event fired in a country without government");
    w.decide("modernize",a);
    w.advance_day();
    check(w.content_state().finished_journals.at(jp).outcome=="compromise",
          "journal's outcome priority/trigger incorrect");
    check(w.content_state().finished_journals.contains(
          "world-revolution|world:*"),
          "world-wide journal did not terminate once");

    auto timed=world(reg);
    timed.start("political-reform",a);
    timed.advance_day();
    for(int n=0;n<10;++n)timed.advance_day();
    check(timed.content_state().finished_journals.at(jp).outcome=="expired",
          "journal timeout branch not reached");
}
static void actual_country_formation_and_protection() {
    const auto reg=rules();
    auto w=world(reg);
    const Scope a{"country","A"};
    check(w.read_world(a,"province_owned:2")==1,
          "initial sovereignty lookup incorrect");
    w.form("independence",a);
    w.advance_day();
    check(w.content_state().history.back().kind=="formation"&&
          w.native().engine().base().core().core().sovereignty("R2")=="A",
          "formation changed sovereignty before native E effective period");
    const auto saved=SnapshotCodec::encode(w);
    auto loaded=SnapshotCodec::decode(saved,reg);
    check(SnapshotCodec::encode(loaded)==saved,
          "queued country foundation not saved in native E queue");
    for(int day=0;day<36;++day) {
        w.advance_day();
        loaded.advance_day();
    }
    check(SnapshotCodec::encode(w)==SnapshotCodec::encode(loaded),
          "formed country replay differs after calendar month transition");
    const auto& c=w.native().engine().base().core().core();
    check(c.sovereignty("R2")=="B"&&
          c.government("B").prime_minister=="PRIME_B"&&
          w.read_world(a,"province_owned:2")==0,
          "real new country and government were not founded");
    check(w.content_state().one_shot_world.contains("formation:independence"),
          "formed country may be repeated");

    auto deny=world(reg);
    deny.notify("government_reformed",{"country","A"});
    deny.advance_day();
    deny.form("independence",{"country","MISSING"});
    const auto before=SnapshotCodec::encode(deny);
    check(rejects([&]{deny.advance_day();})&&
          SnapshotCodec::encode(deny)==before,
          "ineligible formation failed to roll back daily native world");
}
static void unsupported_native_effect_and_legacy_upgrade() {
    const auto base=daily::World::adopt(seed(),35);
    auto reg=rules();
    const auto raw=daily::SnapshotCodec::encode(base);
    auto converted=SnapshotCodec::migrate_legacy_daily(raw,reg);
    check(converted.native().date()==base.date()&&
          converted.content_state().initialized,
          "legacy native save migration invented a past event");
    auto new_bytes=SnapshotCodec::encode(converted);
    check(SnapshotCodec::encode(SnapshotCodec::decode(new_bytes,reg))==new_bytes,
          "legacy upgrade content checksum failed");
    check(rejects([&]{
        Registry bad("bad");
        Formation invalid;
        invalid.id="invalid";invalid.target_country="Z";
        invalid.required_provinces={1,1};
        bad.add(invalid);
    }),"invalid formation unexpectedly registered");
}
int main() {
    try {
        content_local_rules();
        event_choice_and_checkpoint();
        decision_cooldown_ai_and_followup();
        journal_multibranch_and_world();
        actual_country_formation_and_protection();
        unsupported_native_effect_and_legacy_upgrade();
        std::cout<<"VIC2.5 CONTENT EVENTS DECISIONS FORMATION JOURNALS PASS\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"CONTENT TEST FAILURE: "<<e.what()<<"\n";
        return 1;
    }
}
