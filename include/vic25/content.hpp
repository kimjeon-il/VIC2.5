#pragma once
// VIC2.5: shared, deterministic event/decision/journal/formation content rules.
// No independent population, finance, military, law or sovereignty ledgers.
#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace vic25::content {
using Day=std::int64_t;
using Number=std::int64_t;
inline void require(bool p,const char* error) {
    if(!p)throw std::invalid_argument(error);
}
inline constexpr const char* contract="vic25-content/1.0";
struct Scope {
    std::string kind{"country"},id;
    auto operator<=>(const Scope&)const=default;
    [[nodiscard]] std::string key()const {
        return kind+":"+id;
    }
};
inline Scope world_scope() {return {"world","*"};}
enum class Test : std::uint8_t {
    yes,no,flag,counter_ge,counter_le,domain_ge,domain_le,
    all,any,none
};
struct Condition {
    Test kind{Test::yes};
    std::string name;
    Number value{};
    std::vector<Condition> children;
    static Condition domain(std::string key,Number minimum=1) {
        return {Test::domain_ge,std::move(key),minimum,{}};
    }
    static Condition flag(std::string name) {
        return {Test::flag,std::move(name),0,{}};
    }
};
enum class ExternalKind : std::uint8_t {
    none,independence,transfer,claim
};
struct External {
    ExternalKind kind{ExternalKind::none};
    std::string target_country,admin_name,head,prime;
    std::uint64_t admin_id{};
    std::vector<std::uint64_t> provinces;
    Number seats{},quorum{};
};
enum class EffectKind : std::uint8_t {
    set_flag,clear_flag,add_counter,schedule_event,open_journal,
    stop_journal,external
};
struct Effect {
    EffectKind kind{EffectKind::set_flag};
    std::string id;
    Number amount{};
    Day delay{};
    External external;
};
struct Option {
    std::string id;
    Condition available;
    std::vector<Effect> effects;
    Number ai_weight{1};
};
struct Event {
    std::string id,on_action;
    Condition trigger;
    std::vector<Option> options;
    bool once_scope{false},once_world{false};
    Day fixed_day{-1},cooldown_days{};
    Day choice_timeout{7};
    std::string default_option; // "" = do not auto-select for a human country
};
struct Decision {
    std::string id;
    Condition visible,available;
    std::vector<Effect> effects;
    bool once_scope{true},once_world{false};
    Day cooldown_days{};
    Number ai_weight{}; // zero means AI never runs this decision
};
struct Outcome {
    std::string id;
    Condition trigger;
    std::vector<Effect> effects;
    int priority{};
    bool on_timeout{};
};
struct Journal {
    std::string id,group;
    Condition activation;
    std::string progress_metric;
    Number progress_target{100};
    Day timeout_days{-1};
    bool automatic{false},world{false};
    std::vector<Outcome> outcomes;
};
struct Formation {
    std::string id,target_country;
    Condition eligibility;
    std::vector<std::uint64_t> required_provinces;
    Number needed_ppm{1'000'000};
    External external;
    bool grand_unification{false};
};
struct Registry {
    std::string ruleset;
    std::map<std::string,Event> events;
    std::map<std::string,Decision> decisions;
    std::map<std::string,Journal> journals;
    std::map<std::string,Formation> formations;
    explicit Registry(std::string version):ruleset(std::move(version)) {
        require(!ruleset.empty(),"CONTENT_EMPTY_RULESET");
    }
    static void validate_condition(const Condition& c,int depth=0) {
        require(depth<=16,"CONTENT_TRIGGER_DEPTH");
        if(c.kind==Test::all||c.kind==Test::any||c.kind==Test::none) {
            require(!c.children.empty(),"CONTENT_EMPTY_LOGICAL_TRIGGER");
            for(const auto& child:c.children)validate_condition(child,depth+1);
        } else {
            require(c.children.empty(),"CONTENT_INVALID_TRIGGER_CHILD");
            if(c.kind!=Test::yes&&c.kind!=Test::no)
                require(!c.name.empty(),"CONTENT_MISSING_TRIGGER_NAME");
        }
    }
    static void validate_effects(const std::vector<Effect>& effects) {
        for(const auto& e:effects) {
            require(e.delay>=0,"CONTENT_NEGATIVE_EFFECT_DELAY");
            if(e.kind!=EffectKind::external)
                require(!e.id.empty(),"CONTENT_EMPTY_EFFECT_TARGET");
            if(e.kind==EffectKind::external) {
                require(e.external.kind!=ExternalKind::none,
                        "CONTENT_UNSUPPORTED_NATIVE_EFFECT");
                require(!e.external.provinces.empty()&&
                        !e.external.target_country.empty(),
                        "CONTENT_INCOMPLETE_NATIVE_EFFECT");
            }
        }
    }
    void add(Event e) {
        require(!e.id.empty()&&!e.options.empty()&&e.choice_timeout>=0&&
                e.cooldown_days>=0,"CONTENT_INVALID_EVENT");
        validate_condition(e.trigger);
        std::set<std::string> option_ids;
        for(const auto& o:e.options) {
            require(!o.id.empty()&&option_ids.insert(o.id).second&&
                    o.ai_weight>=0,"CONTENT_INVALID_EVENT_OPTION");
            validate_condition(o.available);
            validate_effects(o.effects);
        }
        if(!e.default_option.empty())
            require(option_ids.contains(e.default_option),
                    "CONTENT_INVALID_DEFAULT_OPTION");
        require(events.emplace(e.id,std::move(e)).second,"CONTENT_DUPLICATE_EVENT");
    }
    void add(Decision d) {
        require(!d.id.empty()&&d.cooldown_days>=0&&d.ai_weight>=0,
                "CONTENT_INVALID_DECISION");
        validate_condition(d.visible);validate_condition(d.available);
        validate_effects(d.effects);
        require(decisions.emplace(d.id,std::move(d)).second,
                "CONTENT_DUPLICATE_DECISION");
    }
    void add(Journal j) {
        require(!j.id.empty()&&j.progress_target>0&&j.timeout_days>=-1 &&
                !j.outcomes.empty(),"CONTENT_INVALID_JOURNAL");
        validate_condition(j.activation);
        std::set<std::string> result_ids;
        for(const auto& out:j.outcomes) {
            require(!out.id.empty()&&result_ids.insert(out.id).second,
                    "CONTENT_DUPLICATE_JOURNAL_OUTCOME");
            validate_condition(out.trigger);validate_effects(out.effects);
        }
        require(journals.emplace(j.id,std::move(j)).second,
                "CONTENT_DUPLICATE_JOURNAL");
    }
    void add(Formation f) {
        require(!f.id.empty()&&!f.target_country.empty()&&
                f.needed_ppm>=0&&f.needed_ppm<=1'000'000&&
                !f.required_provinces.empty()&&
                f.external.kind!=ExternalKind::none,
                "CONTENT_INVALID_FORMATION");
        validate_condition(f.eligibility);
        const auto ids=f.required_provinces;
        std::set<std::uint64_t> uniq(ids.begin(),ids.end());
        require(uniq.size()==ids.size(),"CONTENT_DUPLICATE_REQUIRED_PROVINCE");
        require(formations.emplace(f.id,std::move(f)).second,
                "CONTENT_DUPLICATE_FORMATION");
    }
};
struct EventInstance {
    std::string occurrence,definition;
    Scope scope;
    Day opened{},expires{};
};
struct JournalInstance {
    std::string definition;
    Scope scope;
    Day opened{},closed{-1};
    std::string outcome;
};
enum class RequestKind : std::uint8_t {
    event_choice,decision,formation,start_journal,ai_consideration
};
struct Request {
    RequestKind kind{};
    std::string definition,option;
    Scope scope;
};
struct History {
    Day day{};
    std::string kind,definition,scope,outcome;
};
struct State {
    std::string ruleset;
    bool initialized{};
    std::uint64_t next_occurrence{};
    std::map<Day,std::vector<std::pair<std::string,Scope>>> scheduled;
    std::map<std::string,EventInstance> pending_choices;
    std::map<std::string,JournalInstance> active_journals,finished_journals;
    std::map<std::string,Number> counters;
    std::set<std::string> flags,one_shot_world,one_shot_scope;
    std::map<std::string,Day> last_events,last_decisions;
    std::vector<Request> requests;
    std::vector<History> history;
    std::set<Scope> ai_scopes;
};
using ReadDomain=std::function<std::optional<Number>(const Scope&,const std::string&)>;
using ApplyExternal=std::function<void(const Scope&,const External&,const std::string&)>;
inline std::string scoped(const std::string& id,const Scope& s) {
    return id+"|"+s.key();
}
inline bool test(const Condition& c,const State& st,const Scope& s,
                 const ReadDomain& read,int depth=0) {
    require(depth<=16,"CONTENT_TRIGGER_DEPTH");
    const auto local=scoped(c.name,s);
    switch(c.kind) {
        case Test::yes:return true;
        case Test::no:return false;
        case Test::flag:return st.flags.contains(local);
        case Test::counter_ge: {
            const auto p=st.counters.find(local);
            return (p==st.counters.end()?0:p->second)>=c.value;
        }
        case Test::counter_le: {
            const auto p=st.counters.find(local);
            return (p==st.counters.end()?0:p->second)<=c.value;
        }
        case Test::domain_ge: {
            const auto v=read(s,c.name);
            return v.has_value()&&*v>=c.value;
        }
        case Test::domain_le: {
            const auto v=read(s,c.name);
            return v.has_value()&&*v<=c.value;
        }
        case Test::all:
            return std::all_of(c.children.begin(),c.children.end(),
                [&](const auto& child){return test(child,st,s,read,depth+1);});
        case Test::any:
            return std::any_of(c.children.begin(),c.children.end(),
                [&](const auto& child){return test(child,st,s,read,depth+1);});
        case Test::none:
            return std::none_of(c.children.begin(),c.children.end(),
                [&](const auto& child){return test(child,st,s,read,depth+1);});
    }
    throw std::logic_error("CONTENT_UNKNOWN_CONDITION");
}
inline std::uint64_t mix(std::uint64_t x) {
    x+=0x9e3779b97f4a7c15ULL;
    x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;
    x=(x^(x>>27))*0x94d049bb133111ebULL;
    return x^(x>>31);
}
inline std::uint64_t deterministic_roll(std::uint64_t seed,Day day,
                  const std::string& id,const Scope& s) {
    // FNV-1a: never use std::hash (not stable across runtimes).
    std::uint64_t h=14695981039346656037ULL;
    for(const unsigned char c: id+"|"+s.key()) {
        h^=c;h*=1099511628211ULL;
    }
    return mix(seed^mix(static_cast<std::uint64_t>(day))^h);
}
struct Progress {Number current{},target{};};
inline Progress progress(const Registry& rules,const State& st,
             const std::string& journal_instance_key,const ReadDomain& read) {
    const auto& instance=st.active_journals.at(journal_instance_key);
    const auto& definition=rules.journals.at(instance.definition);
    Number current=0;
    if(!definition.progress_metric.empty()) {
        if(definition.progress_metric.rfind("local:",0)==0) {
            const auto key=scoped(definition.progress_metric.substr(6),
                                  instance.scope);
            if(auto it=st.counters.find(key);it!=st.counters.end())
                current=it->second;
        } else {
            if(auto v=read(instance.scope,definition.progress_metric);v)
                current=*v;
        }
    }
    return {std::clamp(current,Number{0},definition.progress_target),
            definition.progress_target};
}
inline void initialize(const Registry& rules,State& st) {
    require(st.ruleset.empty()||st.ruleset==rules.ruleset,
            "CONTENT_RULESET_MIGRATION_REQUIRED");
    st.ruleset=rules.ruleset;
    if(st.initialized)return;
    for(const auto& [id,e]:rules.events)
        if(e.fixed_day>=0)
            st.scheduled[e.fixed_day].push_back({id,world_scope()});
    st.initialized=true;
}
inline void notify(const Registry& rules,State& st,const std::string& action,
                   const Scope& scope,Day due) {
    require(due>=0&&!scope.id.empty(),"CONTENT_INVALID_NOTIFICATION");
    for(const auto& [id,e]:rules.events)
        if(!e.on_action.empty()&&e.on_action==action)
            st.scheduled[due].push_back({id,scope});
    // This opt-in dirty-scope check avoids country x every rule x every day.
    for(const auto& [id,j]:rules.journals)
        if(j.automatic&&j.world==(scope.kind=="world"))
            st.requests.push_back({RequestKind::start_journal,id,"",scope});
}
inline void choose(State& st,std::string occurrence,std::string option) {
    require(!occurrence.empty()&&!option.empty(),"CONTENT_INVALID_CHOICE");
    const auto it=st.pending_choices.find(occurrence);
    require(it!=st.pending_choices.end(),"CONTENT_UNKNOWN_CHOICE");
    st.requests.push_back({RequestKind::event_choice,
                           std::move(occurrence),std::move(option),it->second.scope});
}
inline void decide(State& st,std::string definition,Scope scope) {
    require(!definition.empty()&&!scope.id.empty(),"CONTENT_INVALID_REQUEST");
    st.requests.push_back({RequestKind::decision,std::move(definition),"",std::move(scope)});
}
inline void form(State& st,std::string definition,Scope scope) {
    require(!definition.empty()&&scope.kind=="country"&&!scope.id.empty(),
            "CONTENT_INVALID_FORMATION_SCOPE");
    st.requests.push_back({RequestKind::formation,std::move(definition),"",std::move(scope)});
}
inline void start_journal(State& st,std::string definition,Scope scope) {
    require(!definition.empty()&&!scope.id.empty(),"CONTENT_INVALID_JOURNAL_REQUEST");
    st.requests.push_back({RequestKind::start_journal,
                            std::move(definition),"",std::move(scope)});
}
inline void consider_ai(State& st,Scope scope) {
    require(!scope.id.empty(),"CONTENT_INVALID_AI_SCOPE");
    st.requests.push_back({RequestKind::ai_consideration,"","",std::move(scope)});
}
inline void record(State& st,Day day,std::string kind,
                   std::string id,const Scope& s,std::string outcome="") {
    st.history.push_back({day,std::move(kind),std::move(id),s.key(),
                          std::move(outcome)});
    require(st.history.size()<=2'000'000,"CONTENT_HISTORY_LIMIT");
}
inline void apply_effects(const Registry& rules,State& st,const Scope& s,
            const std::vector<Effect>& effects,Day day,
            const ApplyExternal& external,const std::string& source) {
    for(const auto& e:effects) {
        const auto key=scoped(e.id,s);
        switch(e.kind) {
            case EffectKind::set_flag:st.flags.insert(key);break;
            case EffectKind::clear_flag:st.flags.erase(key);break;
            case EffectKind::add_counter: {
                auto& v=st.counters[key];
                const auto sum=static_cast<__int128>(v)+e.amount;
                require(sum>=std::numeric_limits<Number>::min()&&
                        sum<=std::numeric_limits<Number>::max(),
                        "CONTENT_COUNTER_OVERFLOW");
                v=static_cast<Number>(sum);
                break;
            }
            case EffectKind::schedule_event:
                require(rules.events.contains(e.id),"CONTENT_UNKNOWN_FOLLOWUP");
                require(day<=std::numeric_limits<Day>::max()-e.delay,
                        "CONTENT_DUE_OVERFLOW");
                // Stage9 cannot re-enter the current stage recursively.
                st.scheduled[day+std::max<Day>(1,e.delay)].push_back({e.id,s});
                break;
            case EffectKind::open_journal: {
                const auto& rule=rules.journals.at(e.id);
                const auto target=rule.world?world_scope():s;
                const auto local=scoped(e.id,target);
                if(!st.active_journals.contains(local) &&
                   !st.finished_journals.contains(local))
                    st.active_journals.emplace(local,JournalInstance{
                        e.id,target,day,-1,""});
                break;
            }
            case EffectKind::stop_journal: {
                auto it=st.active_journals.find(key);
                if(it!=st.active_journals.end()) {
                    it->second.closed=day;it->second.outcome="terminated";
                    st.finished_journals.emplace(key,it->second);
                    st.active_journals.erase(it);
                }
                break;
            }
            case EffectKind::external:
                external(s,e.external,source);
                break;
        }
    }
}
inline bool cooldown_ok(const std::map<std::string,Day>& last,
                        const std::string& key,Day current,Day days) {
    const auto it=last.find(key);
    return it==last.end()||(days>0&&current>=it->second &&
                            current-it->second>=days);
}
inline void step(const Registry& rules,State& st,Day day,
            std::uint64_t seed,const ReadDomain& read,
            const ApplyExternal& external) {
    require(day>=0,"CONTENT_INVALID_DAY");
    initialize(rules,st);
    // All mutations to st must occur in a *copy* passed from the day's
    // integration coordinator, so a later domain failure rolls them back.
    auto due=st.scheduled.begin();
    while(due!=st.scheduled.end()&&due->first<=day) {
        const auto scheduled=std::move(due->second);
        due=st.scheduled.erase(due);
        for(const auto& [id,scope]:scheduled) {
            const auto it=rules.events.find(id);
            require(it!=rules.events.end(),"CONTENT_UNKNOWN_SCHEDULED_EVENT");
            const auto& e=it->second;
            const auto local=scoped(id,scope);
            if((e.once_world&&st.one_shot_world.contains(id))||
               (e.once_scope&&st.one_shot_scope.contains(local))||
               !cooldown_ok(st.last_events,local,day,e.cooldown_days)||
               !test(e.trigger,st,scope,read))continue;
            const auto instance=id+":"+scope.key()+":"+
                    std::to_string(day)+":"+std::to_string(st.next_occurrence++);
            st.pending_choices.emplace(instance,
                EventInstance{instance,id,scope,day,day+e.choice_timeout});
            st.last_events[local]=day;
            if(e.once_world)st.one_shot_world.insert(id);
            if(e.once_scope)st.one_shot_scope.insert(local);
            record(st,day,"event_opened",id,scope,instance);
        }
    }
    auto requests=std::move(st.requests);
    st.requests.clear();
    std::set<std::string> acted;
    for(const auto& r:requests) {
        const auto claim=scoped(r.definition,r.scope);
        if(r.kind==RequestKind::event_choice) {
            auto it=st.pending_choices.find(r.definition);
            require(it!=st.pending_choices.end(),"CONTENT_CHOICE_ALREADY_RESOLVED");
            const auto instance=it->second;
            require(day<=instance.expires,"CONTENT_CHOICE_EXPIRED");
            const auto& def=rules.events.at(instance.definition);
            auto op=std::find_if(def.options.begin(),def.options.end(),
                [&](const Option& o){return o.id==r.option;});
            require(op!=def.options.end()&&test(op->available,st,instance.scope,read),
                    "CONTENT_OPTION_UNAVAILABLE");
            apply_effects(rules,st,instance.scope,op->effects,day,external,
                          "event:"+instance.occurrence);
            record(st,day,"event_choice",instance.definition,
                   instance.scope,op->id);
            st.pending_choices.erase(it);
        } else if(r.kind==RequestKind::decision) {
            require(acted.insert("decision:"+claim).second,
                    "CONTENT_DUPLICATE_SAME_DAY_REQUEST");
            const auto& def=rules.decisions.at(r.definition);
            const auto key=scoped(def.id,r.scope);
            require(test(def.visible,st,r.scope,read)&&
                    test(def.available,st,r.scope,read),
                    "CONTENT_DECISION_UNAVAILABLE");
            require(!(def.once_world&&st.one_shot_world.contains(def.id))&&
                    !(def.once_scope&&st.one_shot_scope.contains(key))&&
                    cooldown_ok(st.last_decisions,key,day,def.cooldown_days),
                    "CONTENT_DECISION_COOLDOWN");
            apply_effects(rules,st,r.scope,def.effects,day,external,
                          "decision:"+key+":"+std::to_string(day));
            if(def.once_world)st.one_shot_world.insert(def.id);
            if(def.once_scope)st.one_shot_scope.insert(key);
            st.last_decisions[key]=day;
            record(st,day,"decision",def.id,r.scope);
        } else if(r.kind==RequestKind::formation) {
            require(acted.insert("formation:"+claim).second,
                    "CONTENT_DUPLICATE_SAME_DAY_REQUEST");
            const auto& def=rules.formations.at(r.definition);
            require(r.scope.kind=="country"&&
                    test(def.eligibility,st,r.scope,read),
                    "CONTENT_FORMATION_INELIGIBLE");
            std::size_t controlled=0;
            for(auto province:def.required_provinces) {
                const auto known=read(r.scope,"province_owned:"+
                                                  std::to_string(province));
                if(known&&*known!=0)++controlled;
            }
            require(controlled*1'000'000ULL>=
                    def.needed_ppm*static_cast<std::uint64_t>(
                        def.required_provinces.size()),
                    "CONTENT_FORMATION_TERRITORY_INCOMPLETE");
            require(!st.one_shot_world.contains("formation:"+def.id),
                    "CONTENT_FORMATION_ALREADY_COMPLETED");
            external(r.scope,def.external,
                     "formation:"+def.id+":"+std::to_string(day));
            st.one_shot_world.insert("formation:"+def.id);
            record(st,day,"formation",def.id,r.scope,def.target_country);
        } else if(r.kind==RequestKind::start_journal) {
            const auto& def=rules.journals.at(r.definition);
            const auto target=def.world?world_scope():r.scope;
            require(test(def.activation,st,target,read),
                    "CONTENT_JOURNAL_NOT_ACTIVE");
            const auto key=scoped(def.id,target);
            if(!st.active_journals.contains(key)&&
               !st.finished_journals.contains(key)) {
                st.active_journals.emplace(key,JournalInstance{
                    def.id,target,day,-1,""});
                record(st,day,"journal_opened",def.id,target);
            }
        } else if(r.kind==RequestKind::ai_consideration) {
            if(!st.ai_scopes.contains(r.scope))continue;
            const Decision* best=nullptr;
            for(const auto& [id,def]:rules.decisions) {
                const auto key=scoped(id,r.scope);
                if(def.ai_weight<=0||
                   (def.once_scope&&st.one_shot_scope.contains(key))||
                   (def.once_world&&st.one_shot_world.contains(id))||
                   !cooldown_ok(st.last_decisions,key,day,def.cooldown_days)||
                   !test(def.visible,st,r.scope,read)||
                   !test(def.available,st,r.scope,read))continue;
                if(!best||def.ai_weight>best->ai_weight)best=&def;
            }
            if(best) {
                apply_effects(rules,st,r.scope,best->effects,day,external,
                              "ai-decision:"+best->id+":"+r.scope.key());
                const auto key=scoped(best->id,r.scope);
                if(best->once_world)st.one_shot_world.insert(best->id);
                if(best->once_scope)st.one_shot_scope.insert(key);
                st.last_decisions[key]=day;
                record(st,day,"ai_decision",best->id,r.scope);
            }
        }
    }
    // Expired choices may receive an explicitly configured AI/fallback
    // option. No unapproved human choices are fabricated.
    for(auto it=st.pending_choices.begin();it!=st.pending_choices.end();) {
        const auto inst=it->second;
        const auto& def=rules.events.at(inst.definition);
        const bool ai=st.ai_scopes.contains(inst.scope);
        if(!ai&&day<inst.expires){++it;continue;}
        const Option* selected=nullptr;
        if(ai) {
            Number total=0;
            for(const auto& op:def.options)
                if(op.ai_weight>0&&test(op.available,st,inst.scope,read)) {
                    require(total<=std::numeric_limits<Number>::max()-op.ai_weight,
                            "CONTENT_AI_WEIGHT_OVERFLOW");
                    total+=op.ai_weight;
                }
            if(total>0) {
                const auto roll=deterministic_roll(seed,day,inst.occurrence,
                                                  inst.scope)%
                                                static_cast<std::uint64_t>(total);
                std::uint64_t sum=0;
                for(const auto& op:def.options) {
                    if(op.ai_weight<=0||!test(op.available,st,inst.scope,read))
                        continue;
                    sum+=static_cast<std::uint64_t>(op.ai_weight);
                    if(roll<sum){selected=&op;break;}
                }
            }
        }
        if(!selected&&!def.default_option.empty()) {
            const auto op=std::find_if(def.options.begin(),def.options.end(),
                [&](const Option& o){return o.id==def.default_option;});
            if(op!=def.options.end()&&test(op->available,st,inst.scope,read))
                selected=&*op;
        }
        if(selected) {
            apply_effects(rules,st,inst.scope,selected->effects,day,external,
                          "event-timeout:"+inst.occurrence);
            record(st,day,ai?"ai_event":"event_timeout",
                   inst.definition,inst.scope,selected->id);
        } else if(day<inst.expires) {++it;continue;}
        else record(st,day,"event_expired",inst.definition,inst.scope);
        it=st.pending_choices.erase(it);
    }
    // Evaluation checks only active journal instances, not the Cartesian
    // product of every country and every registered journal.
    for(auto it=st.active_journals.begin();it!=st.active_journals.end();) {
        auto& instance=it->second;
        const auto& def=rules.journals.at(instance.definition);
        const bool timeout=def.timeout_days>=0&&
                           day-instance.opened>=def.timeout_days;
        std::vector<const Outcome*> candidates;
        for(const auto& out:def.outcomes)
            if(out.on_timeout==timeout &&
               test(out.trigger,st,instance.scope,read))
                candidates.push_back(&out);
        std::sort(candidates.begin(),candidates.end(),
            [](const Outcome* a,const Outcome* b) {
                if(a->priority!=b->priority)return a->priority>b->priority;
                return a->id<b->id;
            });
        if(candidates.empty()){++it;continue;}
        auto completed=instance;
        const auto* outcome=candidates.front();
        completed.closed=day;completed.outcome=outcome->id;
        apply_effects(rules,st,completed.scope,outcome->effects,day,external,
                      "journal:"+it->first+":"+outcome->id);
        record(st,day,"journal_outcome",def.id,completed.scope,outcome->id);
        st.finished_journals.emplace(it->first,std::move(completed));
        it=st.active_journals.erase(it);
    }
}
} // namespace vic25::content
