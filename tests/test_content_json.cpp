#include "vic25/content_json.hpp"
#include <iostream>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

using namespace vic25::content;
static void check(bool truth,const char* reason) {
    if(!truth)throw std::runtime_error(reason);
}
template<class F>static bool fails(F&& f) {
    try{f();}catch(const std::exception&){return true;}
    return false;
}
static ReadDomain readable() {
    return [](const Scope& scope,const std::string& key)
                ->std::optional<Number> {
        if(key=="country.exists")return scope.id=="A"?1:0;
        if(key=="province_owned:101"||key=="province_owned:102")
            return scope.id=="A"?1:0;
        return std::nullopt;
    };
}
static void parse_and_exercise_ruleset() {
    const auto path=std::filesystem::path(VIC25_SOURCE_ROOT)/
                    "data/content/example_campaign.json";
    auto rules=json::load_file(path);
    check(rules.events.size()==2&&rules.decisions.size()==1&&
          rules.journals.size()==1&&rules.formations.size()==1,
          "loaded ruleset dropped content definitions");

    const Scope scope{"country","A"};
    State state;
    initialize(rules,state);
    int external_count=0;
    const ApplyExternal apply=[&](const Scope& s,const External& external,
                                  const std::string& provenance) {
        check(s==scope&&external.kind==ExternalKind::independence&&
              external.target_country=="NEW_COUNTRY"&&
              external.admin_id==501&&!provenance.empty(),
              "JSON formation effect incorrectly materialized");
        ++external_count;
    };
    decide(state,"launch_reform_program",scope);
    step(rules,state,0,99,readable(),apply);
    check(state.flags.contains("reform_program_started|country:A")&&
          state.active_journals.contains("political_reform|country:A"),
          "loaded decision failed to activate real journal");

    notify(rules,state,"government_changed",scope,1);
    step(rules,state,1,99,readable(),apply);
    check(state.pending_choices.size()==1,"JSON event not delivered");
    choose(state,state.pending_choices.begin()->first,"accept_compromise");
    step(rules,state,2,99,readable(),apply);
    check(state.finished_journals.size()==1&&
          state.finished_journals.at("political_reform|country:A").outcome==
            "negotiated_compromise",
          "multiple scripted outcomes not selected from parsed JSON");

    form(state,"example_founding",scope);
    step(rules,state,3,99,readable(),apply);
    check(external_count==1&&
          state.history.back().kind=="formation",
          "external country formation from JSON did not execute");

    step(rules,state,4,99,readable(),apply);
    check(state.pending_choices.size()==1,
          "scheduled event from decision did not trigger at exact due day");
    step(rules,state,8,99,readable(),apply);
    check(state.flags.contains("reform_celebrated|country:A"),
          "scheduled event default choice was not applied");
}
static void political_effect_schema() {
    auto registry=json::load(R"({
      "ruleset":"politics-commands",
      "decisions":[
        {"id":"reform","effects":[{"kind":"external","external":{
          "kind":"propose_law","law_domain":"economic_system",
          "law_choice":"interventionism"}}]},
        {"id":"movement","effects":[{"kind":"external","external":{
          "kind":"suppress_movement","movement_id":"MOV"}}]}
      ]})");
    check(registry.decisions.size()==2,
          "political content actions not registered");
    check(registry.decisions.at("reform").effects.front().external.kind==
          ExternalKind::propose_law,
          "content law proposal lost typed native action");
    check(registry.decisions.at("movement").effects.front().external.kind==
          ExternalKind::suppress_movement,
          "content political mandate lost typed native action");
    check(fails([]{json::load(R"({"ruleset":"bad","decisions":[
      {"id":"d","effects":[{"kind":"external","external":{
      "kind":"propose_law","law_domain":"economic_system"}}]}]})");}),
          "content may not propose a law without a choice");
    check(fails([]{json::load(R"({"ruleset":"bad","decisions":[
      {"id":"d","effects":[{"kind":"external","external":{
      "kind":"bolster_movement"}}]}]})");}),
          "content may not order a nonexistent movement");
}
static void reject_untrusted_files() {
    check(fails([]{json::load("{");}),"unterminated JSON accepted");
    check(fails([]{json::load("{\"ruleset\":\"x\",\"ruleset\":\"y\"}");}),
          "duplicate JSON keys accepted");
    check(fails([]{json::load("{\"ruleset\":\"x\",\"extra\":5}");}),
          "unknown ruleset field accepted");
    check(fails([]{json::load("{\"ruleset\":99}");}),
          "non-string ruleset accepted");
    check(fails([]{json::load("{\"ruleset\":\"x\",\"events\":[{\"id\":\"e\",\"options\":[]}]}");}),
          "event without choices accepted");
    check(fails([]{json::load("{\"ruleset\":\"x\",\"decisions\":[{\"id\":\"d\",\"effects\":[{\"kind\":\"cash_grant\",\"id\":\"x\"}]}]}");}),
          "unsupported fiscal cheat effect accepted");
    check(fails([]{json::load("{\"ruleset\":\"x\",\"decisions\":[{\"id\":\"d\",\"visible\":{\"all\":[]}}]}");}),
          "empty logical condition accepted");
    check(fails([]{json::load("{\"ruleset\":\"x\",\"formations\":[{\"id\":\"f\",\"target_country\":\"X\",\"required_provinces\":[1,1],\"external\":{\"kind\":\"claim\",\"target_country\":\"X\",\"provinces\":[1]}}]}");}),
          "duplicate province in country formation accepted");
    check(fails([]{json::load("{\"ruleset\":\"x\",\"journals\":[{\"id\":\"j\",\"outcomes\":[{\"id\":\"a\",\"priority\":999999999999999999}]}]}");}),
          "outcome priority larger than native int accepted");
    check(fails([]{json::load("{\"ruleset\":\"x\",\"events\":false}");}),
          "wrong definitions container accepted");
    check(fails([]{json::load("{\"ruleset\":\"x\"}xyz");}),
          "trailing JSON accepted");
    check(fails([]{json::load("{\"ruleset\":\"\\uD800\"}");}),
          "invalid UTF-16 surrogate accepted");
    check(fails([]{json::load("{\"ruleset\":\"x\",\"decisions\":[{\"id\":\"d\",\"effects\":[{\"kind\":\"external\",\"external\":{\"kind\":\"transfer\",\"target_country\":\"B\",\"provinces\":[]}}]}]}");}),
          "incomplete external action accepted");
}
int main() {
    try {
        parse_and_exercise_ruleset();
        political_effect_schema();
        reject_untrusted_files();
        std::cout<<"VIC2.5 CONTENT DATA LOADER PASS\n";
    } catch(const std::exception& e) {
        std::cerr<<"CONTENT JSON FAILURE: "<<e.what()<<"\n";
        return 1;
    }
}
