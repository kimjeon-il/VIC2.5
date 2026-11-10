#pragma once
// Opt-in daily coordinator for content rules. Phase G/H is still the sole
// staged C/D/E/G/H world; native content effects use Stage 9 geography APIs.
#include "vic25/content.hpp"
#include "vic25/daily_time.hpp"
#include <memory>
#include <set>
#include <string>
#include <functional>
#include <optional>
#include <utility>

namespace vic25::content {
class World {
    daily::World native_;
    std::shared_ptr<const Registry> rules_;
    State state_;

    static std::optional<Number> native_read(
            const Scope& scope,const std::string& key,
            const phasegh::Stage& gh) {
        const auto& c=const_cast<phasegh::Stage&>(gh).phase_c();
        if(key=="country.exists")
            return c.has_government(scope.id)?Number{1}:Number{0};
        if(key=="country.cash") {
            if(!c.has_government(scope.id))return std::nullopt;
            return c.cash(c.country_financial_wallet(scope.id));
        }
        if(key.rfind("province_owned:",0)==0||
           key.rfind("province_controlled:",0)==0) {
            const auto off=key.find(':');
            const auto value=key.substr(off+1);
            if(value.empty()||
               !std::all_of(value.begin(),value.end(),
                    [](unsigned char x){return x>='0'&&x<='9';}))
                return std::nullopt;
            std::uint64_t id{};
            try{id=std::stoull(value);}
            catch(const std::exception&){return std::nullopt;}
            const auto& provinces=gh.geography().provinces;
            const auto it=provinces.find(id);
            if(it==provinces.end())return std::nullopt;
            if(key.rfind("province_owned:",0)==0)
                return c.sovereignty(it->second.region)==scope.id ?
                       Number{1}:Number{0};
            return c.military_control(it->second.region)==scope.id ?
                   Number{1}:Number{0};
        }
        if(key.rfind("p5.crisis_stage:",0)==0) {
            const auto id=key.substr(std::string("p5.crisis_stage:").size());
            const auto& crises=c.p5_crises();
            if(auto it=crises.find(id);it!=crises.end())
                return static_cast<Number>(it->second.stage);
            return std::nullopt;
        }
        if(key.rfind("p5.guerrilla_stage:",0)==0) {
            const auto id=key.substr(std::string("p5.guerrilla_stage:").size());
            const auto& campaigns=c.p5_guerrilla_campaigns();
            if(auto it=campaigns.find(id);it!=campaigns.end())
                return static_cast<Number>(it->second.stage);
            return std::nullopt;
        }
        if(key.rfind("p5.movement_pressure:",0)==0) {
            const auto id=key.substr(std::string("p5.movement_pressure:").size());
            try {return c.p5_risk(id).pressure_ppm;}
            catch(const std::out_of_range&){return std::nullopt;}
        }
        if(key=="p1.legitimacy"&&c.has_government(scope.id)) {
            try{return c.p1_legitimacy(scope.id).score;}
            catch(const std::out_of_range&){return std::nullopt;}
        }
        if(key.rfind("p2.law:",0)==0) {
            const auto name=key.substr(7);
            const auto mid=name.find(':');
            if(mid==std::string::npos)return std::nullopt;
            const auto selected=c.p2_selected_law(scope.id,name.substr(0,mid));
            if(!selected)return std::nullopt;
            return *selected==name.substr(mid+1)?Number{1}:Number{0};
        }
        if(key.rfind("p3.movement_active:",0)==0) {
            const auto id=key.substr(std::string("p3.movement_active:").size());
            const auto it=c.p3_movements().find(id);
            if(it==c.p3_movements().end()||it->second.country!=scope.id)
                return std::nullopt;
            return it->second.active?Number{1}:Number{0};
        }
        if(key.rfind("p3.movement_support:",0)==0) {
            const auto id=key.substr(std::string("p3.movement_support:").size());
            const auto it=c.p3_movements().find(id);
            if(it==c.p3_movements().end()||it->second.country!=scope.id)
                return std::nullopt;
            return c.p3_movement_view(id).support_ppm;
        }
        return std::nullopt; // Unsupported is not silently treated as zero.
    }
    static void native_write(const Scope& scope,const External& effect,
                 const std::string& provenance,Day today,
                 phasegh::Stage& gh) {
        auto& geo=gh.geography();
        require(gh.phase()==9,"CONTENT_NATIVE_EFFECT_WRONG_STAGE");
        require(!provenance.empty(),"CONTENT_MISSING_CAUSAL_PROVENANCE");
        const auto& provinces=geo.spatial().provinces;
        const auto& core=gh.phase_c();
        require(scope.kind=="country"&&core.has_government(scope.id),
                "CONTENT_NATIVE_COUNTRY_AUTHORITY_REQUIRED");
        switch(effect.kind) {
            case ExternalKind::independence:
                require(scope.kind=="country"&&
                        !effect.target_country.empty()&&
                        effect.target_country!=scope.id &&
                        effect.admin_id!=0&&!effect.admin_name.empty()&&
                        !effect.head.empty()&&!effect.prime.empty()&&
                        effect.seats>0&&effect.quorum>0&&
                        effect.quorum<=effect.seats,
                        "CONTENT_INCOMPLETE_INDEPENDENCE");
                for(const auto id:effect.provinces) {
                    const auto it=provinces.find(id);
                    require(it!=provinces.end()&&
                            core.sovereignty(it->second.region)==scope.id,
                            "CONTENT_INDEPENDENCE_REQUIRES_SOURCE_SOVEREIGNTY");
                }
                geo.queue_independence(provenance,effect.provinces,
                    effect.target_country,effect.admin_id,effect.admin_name,
                    effect.head,effect.prime,effect.seats,effect.quorum);
                return;
            case ExternalKind::transfer:
                require(scope.kind=="country"&&effect.admin_id!=0&&
                        !effect.target_country.empty()&&
                        effect.target_country==scope.id,
                        "CONTENT_INVALID_UNIFICATION_TRANSFER");
                for(const auto id:effect.provinces) {
                    const auto it=provinces.find(id);
                    require(it!=provinces.end()&&
                            core.sovereignty(it->second.region)==scope.id,
                            "CONTENT_UNIFICATION_REQUIRES_COMPLETED_CESSION");
                }
                // Legitimacy, consent or a peace settlement must be expressed
                // in the formation's eligibility and actual C/H domain.
                geo.queue_transfer(provenance,effect.provinces,
                                   effect.target_country,effect.admin_id);
                return;
            case ExternalKind::claim:
                require(effect.provinces.size()==1&&
                        effect.target_country==scope.id,
                        "CONTENT_INVALID_PROVINCE_CLAIM");
                geo.queue_claim(provenance,effect.provinces.front(),
                                effect.target_country,true);
                return;
            case ExternalKind::colonize: {
                const auto rule=core.p2_selected_law(
                    scope.id,"colonial_affairs");
                require(rule.has_value()&&
                        (*rule=="frontier_colonization"||
                         *rule=="colonial_resettlement"||
                         *rule=="colonial_exploitation")&&
                        effect.target_country==scope.id,
                        "CONTENT_COLONIZATION_REQUIRES_LAWFUL_POLICY");
                for(const auto target:effect.provinces) {
                    const auto it=provinces.find(target);
                    require(it!=provinces.end(),
                            "CONTENT_COLONIZATION_UNKNOWN_TARGET");
                    const auto owner=core.sovereignty(it->second.region);
                    require(owner!=scope.id&&!core.has_government(owner),
                            "CONTENT_COLONIZATION_CANNOT_ANNEX_SOVEREIGN");
                    if(*rule=="frontier_colonization") {
                        bool frontier=false;
                        for(const auto adjacent:it->second.neighbors) {
                            const auto near=provinces.find(adjacent);
                            if(near!=provinces.end()&&
                               core.sovereignty(near->second.region)==scope.id)
                                frontier=true;
                        }
                        require(frontier,
                                "CONTENT_FRONTIER_COLONIZATION_NOT_ADJACENT");
                    }
                    // The original E sovereignty and administrative title
                    // do not change merely because a claim was registered.
                    // Settlement and actual statehood require later verified
                    // C/E/H commands, never ghost settlers.
                    geo.queue_claim(provenance+"#"+std::to_string(target),
                                    target,scope.id,true);
                }
                return;
            }
            case ExternalKind::political_bill: {
                require(effect.target_country.empty()||
                        effect.target_country==scope.id,
                        "CONTENT_BILL_FOREIGN_AUTHORITY");
                const auto& government=core.government(scope.id);
                const auto proposer=effect.actor.empty()?
                    government.prime_minister:effect.actor;
                core.p2_propose_catalogue_reform(scope.id,effect.law_group,
                    effect.law_choice,proposer,provenance,
                    "CONTENT:bill:"+provenance,today);
                return;
            }
            case ExternalKind::political_cabinet: {
                require(effect.target_country.empty()||
                        effect.target_country==scope.id,
                        "CONTENT_CABINET_FOREIGN_AUTHORITY");
                const auto& incumbent=core.government(scope.id);
                // A scripted office change must refer to an actual incumbent
                // or a living anchored political figure, not create people.
                require(effect.prime==incumbent.prime_minister||
                        (core.p1_characters().contains(effect.prime)&&
                         core.p1_character_available(effect.prime)),
                        "CONTENT_CABINET_NEW_PM_NOT_AN_ACTUAL_PERSON");
                core.schedule_p1_cabinet_daily(scope.id,incumbent.head_of_state,
                    effect.prime,
                    phasec::P1Coalition{effect.groups,effect.parties,false},
                    today+1,provenance);
                return;
            }
            case ExternalKind::political_movement: {
                require(effect.target_country.empty()||
                        effect.target_country==scope.id,
                        "CONTENT_MOVEMENT_FOREIGN_AUTHORITY");
                phasec::P3Movement m;
                m.id=effect.subject_id;m.country=scope.id;
                m.issue_domain=effect.law_group;
                m.desired_choice=effect.law_choice;
                m.groups=effect.groups;
                m.region=effect.region;
                if(effect.action=="replace_government")
                    m.goal=phasec::P3MovementGoal::replace_government;
                else if(effect.action=="autonomy")
                    m.goal=phasec::P3MovementGoal::autonomy;
                else if(effect.action=="secession")
                    m.goal=phasec::P3MovementGoal::secession;
                else
                    require(effect.action.empty()||effect.action=="reform",
                            "CONTENT_MOVEMENT_INVALID_POLITICAL_GOAL");
                bool real_backing=false;
                for(const auto& group:effect.groups) {
                    if(core.p3_groups().contains({scope.id,group})) {
                        const auto v=core.p3_group_view(scope.id,group);
                        if(v.members>0) {
                            real_backing=true;
                            m.activism_ppm=std::max(m.activism_ppm,
                                v.engagement_ppm);
                            m.radical_ppm=std::max(m.radical_ppm,
                                v.radical_ppm);
                        }
                    }
                }
                require(real_backing,
                        "CONTENT_MOVEMENT_REQUIRES_REAL_POP_GROUP");
                core.p3_seed_movement(std::move(m));
                return;
            }
            case ExternalKind::political_mandate: {
                require(effect.target_country.empty()||
                        effect.target_country==scope.id,
                        "CONTENT_MANDATE_FOREIGN_AUTHORITY");
                phasec::P1PoliticalMandate directive;
                directive.id="CONTENT:mandate:"+provenance;
                directive.country=scope.id;
                directive.target=effect.subject_id;
                directive.authority_cost=200;
                directive.kind=effect.action=="bolster"?
                    phasec::P1PoliticalMandateKind::bolster_movement:
                    phasec::P1PoliticalMandateKind::suppress_movement;
                core.p1_activate_mandate(std::move(directive),today);
                return;
            }
            case ExternalKind::political_settlement: {
                phasec::P5Settlement agreement;
                agreement.id="CONTENT:settlement:"+provenance;
                agreement.crisis_id=effect.subject_id;
                agreement.legal_basis=provenance;
                agreement.successor_head=effect.head;
                agreement.new_sovereign=effect.target_country;
                agreement.region=effect.region;
                agreement.day=today;
                if(effect.action=="legal_concession")
                    agreement.kind=phasec::P5SettlementKind::legal_concession;
                else if(effect.action=="autonomy_granted")
                    agreement.kind=phasec::P5SettlementKind::autonomy_granted;
                else if(effect.action=="crisis_recedes")
                    agreement.kind=phasec::P5SettlementKind::crisis_recedes;
                else if(effect.action=="cabinet_overthrow")
                    agreement.kind=phasec::P5SettlementKind::cabinet_overthrow;
                else
                    agreement.kind=phasec::P5SettlementKind::independence_recognized;
                // Actual law, military control and territorial transfer are
                // checked inside the original P5 settlement transaction.
                require(core.p5_crises().contains(effect.subject_id)&&
                        core.p5_crises().at(effect.subject_id).country==scope.id,
                        "CONTENT_SETTLEMENT_MISSING_SOVEREIGN_CRISIS");
                core.p5_settle(std::move(agreement));
                return;
            }
            case ExternalKind::none:
                throw std::invalid_argument("CONTENT_UNSUPPORTED_NATIVE_EFFECT");
        }
    }
    explicit World(daily::World native,std::shared_ptr<const Registry> rules,
                   State state)
        :native_(std::move(native)),rules_(std::move(rules)),
         state_(std::move(state)) {
        require(static_cast<bool>(rules_),"CONTENT_MISSING_RULESET");
        initialize(*rules_,state_);
    }
public:
    static World adopt(daily::World native,std::shared_ptr<const Registry> rules){
        return World(std::move(native),std::move(rules),{});
    }
    static World restore(daily::World native,std::shared_ptr<const Registry> rules,
                         State state) {
        return World(std::move(native),std::move(rules),std::move(state));
    }
    [[nodiscard]] const daily::World& native() const{return native_;}
    [[nodiscard]] const State& content_state()const{return state_;}
    [[nodiscard]] const Registry& rules()const{return *rules_;}
    [[nodiscard]] daily::Date date()const{return native_.date();}
    void notify(const std::string& action,Scope scope,Day due=-1) {
        if(due<0)due=native_.day_index();
        require(due>=native_.day_index(),"CONTENT_PAST_NOTIFICATION");
        content::notify(*rules_,state_,action,scope,due);
    }
    void schedule(const std::string& event,Scope scope,Day due) {
        require(rules_->events.contains(event)&&due>=native_.day_index(),
                "CONTENT_SCHEDULE_INVALID");
        state_.scheduled[due].push_back({event,std::move(scope)});
    }
    void choose(std::string occurrence,std::string option) {
        const auto it=state_.pending_choices.find(occurrence);
        require(it!=state_.pending_choices.end()&&
                native_.day_index()<=it->second.expires,
                "CONTENT_CHOICE_NO_LONGER_AVAILABLE");
        content::choose(state_,std::move(occurrence),std::move(option));
    }
    void decide(std::string decision,Scope scope) {
        require(rules_->decisions.contains(decision),"CONTENT_UNKNOWN_DECISION");
        content::decide(state_,std::move(decision),std::move(scope));
    }
    void form(std::string formation,Scope scope) {
        require(rules_->formations.contains(formation),"CONTENT_UNKNOWN_FORMATION");
        content::form(state_,std::move(formation),std::move(scope));
    }
    void start(std::string journal,Scope scope) {
        require(rules_->journals.contains(journal),"CONTENT_UNKNOWN_JOURNAL");
        content::start_journal(state_,std::move(journal),std::move(scope));
    }
    void register_ai(Scope scope) {state_.ai_scopes.insert(std::move(scope));}
    void ai_check(Scope scope) {content::consider_ai(state_,std::move(scope));}
    [[nodiscard]] std::optional<Number> read_world(const Scope& scope,
                const std::string& metric) const {
        const auto& c=native_.engine().base().core().core();
        const auto& geo=native_.engine().base().spatial();
        if(metric=="country.exists")return c.has_government(scope.id)?1:0;
        if(metric.rfind("province_owned:",0)==0) {
            const auto value=metric.substr(std::string("province_owned:").size());
            std::uint64_t id{};
            try{id=std::stoull(value);}
            catch(const std::exception&){return std::nullopt;}
            const auto p=geo.provinces.find(id);
            if(p==geo.provinces.end())return std::nullopt;
            return c.sovereignty(p->second.region)==scope.id?1:0;
        }
        if(metric=="p1.legitimacy"&&c.has_government(scope.id)) {
            try{return c.p1_legitimacy(scope.id).score;}
            catch(const std::out_of_range&){return std::nullopt;}
        }
        if(metric.rfind("p2.law:",0)==0) {
            const auto law=metric.substr(7);
            const auto split=law.find(':');
            if(split==std::string::npos)return std::nullopt;
            const auto selected=c.p2_selected_law(scope.id,law.substr(0,split));
            if(!selected)return std::nullopt;
            return *selected==law.substr(split+1)?1:0;
        }
        if(metric.rfind("p3.movement_support:",0)==0) {
            const auto id=metric.substr(std::string("p3.movement_support:").size());
            if(!c.p3_movements().contains(id)||
               c.p3_movements().at(id).country!=scope.id)
                return std::nullopt;
            return c.p3_movement_view(id).support_ppm;
        }
        return std::nullopt;
    }
    // The content state is staged in a copy. Only after C/D/E/G/H's
    // authoritative transaction commits may the content copy become live.
    // The native Stage9 callback does not create a second world.
    daily::DayReport advance_day(const phasegh::World::Plan& native_plan={}) {
        State staged=state_;
        auto plan=native_plan;
        const Day today=native_.day_index();
        const auto seed=native_.world_seed();
        auto rules=rules_;
        const auto journal_cursor=
            native_.engine().base().core().core().journal().size();
        plan[9].push_back([&staged,today,seed,rules](phasegh::Stage& gh){
            const ReadDomain read=[&gh](const Scope& scope,
                                       const std::string& metric){
                return native_read(scope,metric,gh);
            };
            const ApplyExternal apply=[&gh,today](const Scope& scope,
                                          const External& effect,
                                          const std::string& provenance){
                native_write(scope,effect,provenance,today,gh);
            };
            step(*rules,staged,today,seed,read,apply);
        });
        // C's native event journal is the authoritative source for
        // next-day on_action notifications. Stage11 runs after all the
        // political AI and real Stage10 military work, but inside the
        // same C/D/E/G/H atomic transaction as the content staging copy.
        plan[11].push_back([&staged,today,rules,journal_cursor]
                           (phasegh::Stage& gh) {
            const auto& c=gh.phase_c();
            const auto& events=c.journal();
            if(events.size()<journal_cursor)return;
            for(std::size_t i=journal_cursor;i<events.size();++i) {
                const auto& event=events[i];
                auto relay=[&](const std::string& action,
                               const std::string& country) {
                    if(country.empty())return;
                    content::notify(*rules,staged,action,
                                    {"country",country},today+1);
                };
                const auto with_id=[&](const std::string& prefix)->
                      std::optional<std::string> {
                    if(event.rfind(prefix,0)==0)
                        return event.substr(prefix.size());
                    return std::nullopt;
                };
                if(auto id=with_id("p2-law-effective:")) {
                    const auto bill=c.p2_bills().find(*id);
                    if(bill!=c.p2_bills().end())
                        relay(                              "politics.law_effective",bill->second.country);
                } else if(auto id=with_id("p5-political-crisis-warning:")) {
                    const auto row=c.p5_crises().find(*id);
                    if(row!=c.p5_crises().end())
                        relay(                              "politics.crisis_warning",row->second.country);
                } else if(auto id=with_id("p5-verified-political-settlement:")) {
                    const auto row=c.p5_crises().find(*id);
                    if(row!=c.p5_crises().end())
                        relay(                              "politics.crisis_settled",row->second.country);
                } else if(auto id=with_id("p4-contest-counted:")) {
                    const auto contest=c.p4_contests().find(*id);
                    if(contest!=c.p4_contests().end())
                        relay(                              "politics.election_held",contest->second.country);
                } else if(auto id=with_id("p3-persistent-movement-created:")) {
                    const auto row=c.p3_movements().find(*id);
                    if(row!=c.p3_movements().end())
                        relay(                              "politics.movement_created",row->second.country);
                } else if(auto id=with_id("p1-daily-government-effective:")) {
                    relay(                          "politics.government_changed",*id);
                }
            }
        });
        auto report=native_.advance_day(plan);
        state_=std::move(staged);
        return report;
    }
    // Native P5 crisis and P5-G are not duplicated as content instances.
    // Their actual status is always read from the original C ledger.
    struct NativeJournalView {
        std::string id,kind,country;
        Number stage{};
    };
    [[nodiscard]] std::vector<NativeJournalView> political_journals()const {
        std::vector<NativeJournalView> result;
        const auto& c=native_.engine().base().core().core();
        for(const auto& [id,x]:c.p5_crises())
            result.push_back({id,"p5_crisis",x.country,
                              static_cast<Number>(x.stage)});
        for(const auto& [id,x]:c.p5_guerrilla_campaigns())
            result.push_back({id,"p5_guerrilla",x.country,
                              static_cast<Number>(x.stage)});
        return result;
    }
};
} // namespace vic25::content
