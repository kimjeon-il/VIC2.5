#pragma once
// P3 derived politics over the existing Phase B/C/P1/P2/P4 sources of truth.
namespace vic25::phasec {

inline std::int32_t IntegratedWorld::p3_fraction(People part,People total) {
    ensure(part>=0&&total>0&&part<=total,"P3 invalid actual POP share");
    std::int32_t low=0,high=1'000'000;
    while(low<high) {
        const auto mid=low+(high-low+1)/2;
        if(compare_nonnegative_ratios(
            static_cast<std::uint64_t>(part),
            static_cast<std::uint64_t>(total),
            static_cast<std::uint64_t>(mid),1'000'000)>=0)
            low=mid;
        else high=mid-1;
    }
    return low;
}

inline void IntegratedWorld::p3_seed_group(P3Group group) {
    ensure(stage_==-1&&commit_id_==0&&
           governments_.contains(group.country)&&
           interest_groups_.contains(group.id)&&
           !p3_groups_.contains({group.country,group.id})&&
           static_cast<unsigned>(group.type)<=
               static_cast<unsigned>(P3GroupType::custom)&&
           group.organization_ppm>=0&&group.organization_ppm<=1'000'000&&
           group.institutional_access_ppm>=0&&
           group.institutional_access_ppm<=1'000'000&&
           (group.leader_anchor.empty()||
            person_anchors_.contains(group.leader_anchor)),
           "P3 invalid national group or unavailable named leader");
    const auto key=std::make_pair(group.country,group.id);
    p3_groups_.emplace(key,std::move(group));
}

inline void IntegratedWorld::p3_set_ideologies(
        const std::string& country,const std::string& group,
        std::vector<std::string> ideologies) {
    ensure((stage_==-1&&commit_id_==0)||stage_==9,
           "P3 ideologies can change only at opening or politics stage");
    ensure(p3_groups_.contains({country,group})&&!ideologies.empty()&&
           ideologies.size()<=8,"P3 missing interest-group ideology authority");
    std::set<std::string> unique;
    for(const auto& ideology:ideologies)
        ensure(politics::catalogue().knows_ideology(ideology)&&
               unique.insert(ideology).second,
               "P3 unsupported or duplicate political ideology");
    p3_ideology_overrides_[{country,group}]=std::move(ideologies);
    events_.push_back("p3-ideological-group-profile-changed:"+country+":"+group);
}
inline void IntegratedWorld::p3_set_leader_ideology(
        const std::string& country,const std::string& group,
        const std::string& ideology) {
    ensure((stage_==-1&&commit_id_==0)||stage_==9,
           "P3 leader ideology changes require political stage");
    const auto it=p3_groups_.find({country,group});
    ensure(it!=p3_groups_.end()&&!it->second.leader_anchor.empty()&&
           person_anchors_.contains(it->second.leader_anchor)&&
           politics::catalogue().knows_ideology(ideology),
           "P3 political leader ideology must bind existing named person");
    p3_leader_ideologies_[{country,group}]=ideology;
    events_.push_back("p3-named-leader-ideology-changed:"+country+":"+group);
}
inline std::vector<std::string> IntegratedWorld::p3_ideologies(
        const std::string& country,const std::string& group)const {
    const auto& profile=p3_groups_.at({country,group});
    if(auto it=p3_ideology_overrides_.find({country,group});
       it!=p3_ideology_overrides_.end())return it->second;
    return politics::default_ideologies(profile.type);
}
inline std::optional<int> IntegratedWorld::p3_law_stance(
        const std::string& country,const std::string& group,
        const std::string& law_group,const std::string& law)const {
    if(!politics::catalogue().has(law_group,law))return std::nullopt;
    int sum=0,weight=0;
    for(const auto& ideology:p3_ideologies(country,group)) {
        if(const auto stance=politics::catalogue().stance(
               ideology,law_group,law)) {
            sum+=*stance;
            ++weight;
        }
    }
    if(const auto leader=p3_leader_ideologies_.find({country,group});
       leader!=p3_leader_ideologies_.end()) {
        if(const auto stance=politics::catalogue().stance(
               leader->second,law_group,law)) {
            sum+=2*(*stance);
            weight+=2;
        }
    }
    if(!weight)return std::nullopt;
    return std::clamp(sum/weight,-2,2);
}

inline P2PoliticalSupportView IntegratedWorld::p2_political_support(
        const std::string& country,const std::string& law_group,
        const std::string& proposed_law)const {
    const auto group=politics::bare_group(law_group);
    const auto proposed=politics::bare_law(proposed_law);
    ensure(politics::catalogue().has(group,proposed)&&
           governments_.contains(country),
           "P2 legislative support requires known national proposed law");
    P2PoliticalSupportView view{country,group,proposed};
    const auto adopted=p2_selected_law(country,group);
    std::int64_t total=0,support=0,oppose=0,governing=0;
    const auto coalition=p1_coalitions_.find(country);
    for(const auto& [key,profile]:p3_groups_) {
        (void)profile;
        if(key.first!=country)continue;
        const auto weight=p3_group_view(country,key.second).clout_ppm;
        if(weight<=0)continue;
        total+=weight;
        if(adopted&&*adopted==proposed)continue;
        const auto current=adopted?
            p3_law_stance(country,key.second,group,*adopted):
            std::optional<int>{};
        const auto candidate=p3_law_stance(
            country,key.second,group,proposed);
        if(!current&&!candidate)continue;
        const auto delta=candidate.value_or(0)-current.value_or(0);
        if(delta>0) {
            support+=weight;
            if(coalition!=p1_coalitions_.end()&&
               std::find(coalition->second.ruling_groups.begin(),
                         coalition->second.ruling_groups.end(),key.second)!=
                         coalition->second.ruling_groups.end())
                governing+=weight;
        } else if(delta<0)oppose+=weight;
    }
    if(total>0) {
        view.observed=true;
        view.support_ppm=p3_fraction(support,total);
        view.oppose_ppm=p3_fraction(oppose,total);
        view.neutral_ppm=1'000'000-view.support_ppm-view.oppose_ppm;
        view.governing_support_ppm=p3_fraction(governing,total);
    }
    return view;
}

inline void IntegratedWorld::p3_set_stance(
        Id slice,const std::string& issue,P3Stance stance) {
    ensure((stage_==-1&&commit_id_==0)||stage_==9,
           "P3 issue preference requires opening data or political stage");
    ensure(people_.has_slice(slice)&&!issue.empty()&&
           stance.preference_ppm>=-1'000'000&&
           stance.preference_ppm<=1'000'000&&
           stance.importance_ppm>=0&&stance.importance_ppm<=1'000'000,
           "P3 issue stance must reference real POP");
    auto& attitude=p3_attitudes_[slice];
    attitude.slice=slice;
    attitude.issues[issue]=stance;
    events_.push_back("p3-observed-issue-stance:"+issue);
}

inline void IntegratedWorld::p3_seed_movement(P3Movement movement) {
    ensure((stage_==-1&&commit_id_==0)||stage_==9,
           "P3 political movement creation outside lawful political stage");
    ensure(!movement.id.empty()&&!movement.country.empty()&&
           !movement.issue_domain.empty()&&
           governments_.contains(movement.country)&&
           !p3_movements_.contains(movement.id)&&
           movement.activism_ppm>=0&&movement.activism_ppm<=1'000'000&&
           movement.radical_ppm>=0&&movement.radical_ppm<=1'000'000&&
           static_cast<unsigned>(movement.goal)<=
               static_cast<unsigned>(P3MovementGoal::secession)&&
           !movement.groups.empty()&&
           (movement.region.empty()||
            (sovereignty_.contains(movement.region)&&
             sovereignty_.at(movement.region)==movement.country)),
           "P3 movement needs real sovereign political issue and organization");
    std::set<std::string> seen;
    for(const auto& group:movement.groups)
        ensure(seen.insert(group).second&&
               p3_groups_.contains({movement.country,group}),
               "P3 duplicate or foreign interest group in movement");
    seen.clear();
    for(const auto& party:movement.parties)
        ensure(seen.insert(party).second&&p4_parties_.contains(party)&&
               p4_parties_.at(party).country==movement.country,
               "P3 movement supports unregistered or foreign party");
    const auto id=movement.id;
    p3_movements_.emplace(id,std::move(movement));
    events_.push_back("p3-persistent-movement-created:"+id);
}

inline P3GroupView IntegratedWorld::p3_group_view(
        const std::string& country,const std::string& group)const {
    const auto& profile=p3_groups_.at({country,group});
    P3GroupView view{country,group};
    People country_group_members=0;
    People affiliated_weight=0,group_weight=0;
    for(const auto& [key,n]:membership_) {
        if(n<=0||!people_.has_slice(key.first))continue;
        const auto organization=p3_groups_.find({country,key.second});
        if(organization==p3_groups_.end())continue;
        const auto& dem=people_.demographic(
            people_.slice(key.first).key.demographic);
        const auto nation=sovereignty_.find(dem.region);
        if(nation==sovereignty_.end()||nation->second!=country)continue;
        // Organizational efficiency is applied to REAL affiliated people,
        // not minted into a separate political-point or membership ledger.
        const People organized=phaseb::add_checked(n,
             p4_scaled(n,organization->second.organization_ppm,1'000'000));
        const auto legal_strength=p2_political_strength_ppm(
            country,organization->second.type);
        People weighted=p4_scaled(organized,
            std::min(legal_strength,1'000'000),1'000'000);
        if(legal_strength>1'000'000)
            weighted=phaseb::add_checked(weighted,p4_scaled(
                organized,std::min(legal_strength-1'000'000,1'000'000),
                1'000'000));
        if(legal_strength>2'000'000)
            weighted=phaseb::add_checked(weighted,p4_scaled(
                organized,legal_strength-2'000'000,1'000'000));
        affiliated_weight=phaseb::add_checked(affiliated_weight,weighted);
        if(key.second==group) {
            country_group_members=phaseb::add_checked(country_group_members,n);
            group_weight=phaseb::add_checked(group_weight,weighted);
        }
    }
    view.members=country_group_members;
    if(affiliated_weight>0&&group_weight>0) {
        const auto share=p3_fraction(group_weight,affiliated_weight);
        bool government_access=false;
        if(const auto coalition=p1_coalitions_.find(country);
           coalition!=p1_coalitions_.end())
            government_access=std::find(
               coalition->second.ruling_groups.begin(),
               coalition->second.ruling_groups.end(),group)!=
               coalition->second.ruling_groups.end();
        // Cabinet access only applies when this group ACTUALLY governs.
        const auto institutional=government_access?
            profile.institutional_access_ppm/10:0;
        view.clout_ppm=static_cast<std::int32_t>(
             std::min<std::int64_t>(1'000'000,
                 static_cast<std::int64_t>(share)*9/10+institutional));
    }
    int approval=50;
    if(!profile.issue_domain.empty()&&!profile.preferred_choice.empty()) {
        std::string current_choice;
        for(const auto& [slot,id]:p2_active_base_) {
            (void)slot;
            const auto& law=p2_laws_.at(id);
            if(law.country==country&&law.domain==profile.issue_domain) {
                current_choice=law.base_choice;
                break;
            }
        }
        if(!current_choice.empty())
            approval=current_choice==profile.preferred_choice?75:30;
    }
    // A group's live laws are evaluated through ideology preferences.
    // The legacy single-issue preference remains a fallback for custom
    // groups with no stance on any adopted catalogued law.
    std::int64_t ideological_score=0,ideological_observations=0;
    for(const auto& [slot,id]:p2_active_base_) {
        (void)slot;
        const auto& adopted=p2_laws_.at(id);
        if(adopted.country!=country)continue;
        const auto stance=p3_law_stance(country,group,
                         adopted.domain,adopted.base_choice);
        if(!stance)continue;
        ideological_score+=*stance;
        ++ideological_observations;
    }
    if(ideological_observations>0)
        approval=static_cast<int>(std::clamp<std::int64_t>(
            50+12*ideological_score/ideological_observations,0,100));
    People members=0;
    std::int64_t sum_radical=0,sum_engagement=0;
    for(const auto& [key,n]:membership_) {
        if(key.second!=group||n<=0||!people_.has_slice(key.first))continue;
        const auto& dem=people_.demographic(
            people_.slice(key.first).key.demographic);
        auto sovereign=sovereignty_.find(dem.region);
        if(sovereign==sovereignty_.end()||sovereign->second!=country)continue;
        members=phaseb::add_checked(members,n);
    }
    if(members>0)
        for(const auto& [key,n]:membership_) {
            if(key.second!=group||n<=0||!people_.has_slice(key.first))continue;
            const auto& dem=people_.demographic(
                people_.slice(key.first).key.demographic);
            auto sovereign=sovereignty_.find(dem.region);
            if(sovereign==sovereignty_.end()||sovereign->second!=country)continue;
            const auto att=p3_attitudes_.find(key.first);
            if(att==p3_attitudes_.end())continue;
            const auto fraction=p3_fraction(n,members);
            sum_radical+=static_cast<std::int64_t>(fraction)*
                         att->second.radical_ppm/1'000'000;
            sum_engagement+=static_cast<std::int64_t>(fraction)*
                            att->second.engagement_ppm/1'000'000;
        }
    view.radical_ppm=static_cast<std::int32_t>(
        std::clamp<std::int64_t>(sum_radical,0,1'000'000));
    view.engagement_ppm=static_cast<std::int32_t>(
        std::clamp<std::int64_t>(sum_engagement,0,1'000'000));
    view.approval=std::clamp(approval-view.radical_ppm/20'000,0,100);
    return view;
}

inline std::vector<politics::IGEffect>
IntegratedWorld::p3_active_traits(
        const std::string& country,const std::string& group)const {
    const auto& profile=p3_groups_.at({country,group});
    const auto view=p3_group_view(country,group);
    return politics::active_ig_effects(profile.type,view.approval,view.members);
}
inline std::int64_t IntegratedWorld::p3_trait_modifier_ppm(
        const std::string& country,const std::string& effect_key)const {
    ensure(governments_.contains(country),
           "P3 IG trait requires existing sovereign government");
    std::int64_t sum=0;
    for(const auto& [key,profile]:p3_groups_) {
        (void)profile;
        if(key.first!=country)continue;
        for(const auto& trait:p3_active_traits(key.first,key.second))
            if(trait.modifier==effect_key)
                sum=phaseb::add_checked(sum,trait.value_ppm);
    }
    return sum;
}

inline P3MovementView IntegratedWorld::p3_movement_view(
        const std::string& id)const {
    const auto& movement=p3_movements_.at(id);
    P3MovementView view{id,movement.country};
    view.activism_ppm=movement.activism_ppm;
    view.radical_ppm=movement.radical_ppm;
    People population=0;
    std::map<Id,People> support;
    for(const auto& [slice,row]:people_.slices()) {
        const auto& dem=people_.demographic(row.key.demographic);
        const auto sovereign=sovereignty_.find(dem.region);
        if(sovereign==sovereignty_.end()||
           sovereign->second!=movement.country||
           (!movement.region.empty()&&
            dem.region!=movement.region))continue;
        population=phaseb::add_checked(population,row.count);
        const auto att=p3_attitudes_.find(slice);
        if(att!=p3_attitudes_.end()) {
            const auto pref=att->second.issues.find(movement.issue_domain);
            if(pref!=att->second.issues.end()&&
               pref->second.preference_ppm>0) {
                const auto interested=p4_scaled(
                    row.count,pref->second.preference_ppm,1'000'000);
                const auto intended=p4_scaled(interested,
                    pref->second.importance_ppm,1'000'000);
                if(intended>0)support[slice]=intended;
            }
        }
    }
    // Multiple organizational affiliations overlap. Count each actual POP
    // slice only up to the largest single available support allocation.
    for(const auto& [key,n]:membership_) {
        if(n<=0||!people_.has_slice(key.first)||
           std::find(movement.groups.begin(),movement.groups.end(),
                     key.second)==movement.groups.end())continue;
        const auto& demo=people_.demographic(
            people_.slice(key.first).key.demographic);
        const auto nation=sovereignty_.find(demo.region);
        if(nation==sovereignty_.end()||nation->second!=movement.country||
           (!movement.region.empty()&&movement.region!=demo.region))continue;
        auto& already=support[key.first];
        already=std::max(already,n);
    }
    for(const auto& [slice,n]:support) {
        (void)slice;
        view.supporters=phaseb::add_checked(view.supporters,n);
    }
    if(population>0)view.support_ppm=p3_fraction(view.supporters,population);
    std::int64_t observed_radical=0,observed_engagement=0;
    if(view.supporters>0)
        for(const auto& [slice,n]:support) {
            const auto opinion=p3_attitudes_.find(slice);
            if(opinion==p3_attitudes_.end()||n<=0)continue;
            const auto share=p3_fraction(n,view.supporters);
            observed_radical+=static_cast<std::int64_t>(share)*
                    opinion->second.radical_ppm/1'000'000;
            observed_engagement+=static_cast<std::int64_t>(share)*
                    opinion->second.engagement_ppm/1'000'000;
        }
    // Persistent organizational ideology defines a floor, not a fixed
    // national radicalism tag. Observed POP hardship can exceed that floor.
    view.radical_ppm=std::max(movement.radical_ppm,
        static_cast<std::int32_t>(
            std::clamp<std::int64_t>(observed_radical,0,1'000'000)));
    view.activism_ppm=std::max(movement.activism_ppm,
        static_cast<std::int32_t>(
            std::clamp<std::int64_t>(observed_engagement,0,1'000'000)));
    view.potential_activists=p4_scaled(
        view.supporters,view.activism_ppm,1'000'000);
    return view;
}

inline bool IntegratedWorld::p3_needs_daily_tick()const {
    if(!p3_attitudes_.empty()||!p3_movements_.empty())return true;
    if(p3_groups_.empty())return false;
    // Opt-in national group members have economically grounded attitudes
    // even without a food shortage; membership is a real POP reference.
    for(const auto& [key,n]:membership_) {
        if(n<=0||!people_.has_slice(key.first))continue;
        const auto& dem=people_.demographic(
            people_.slice(key.first).key.demographic);
        const auto nation=sovereignty_.find(dem.region);
        if(nation!=sovereignty_.end()&&
           p3_groups_.contains({nation->second,key.second}))return true;
    }
    for(const auto& [region,unmet]:regional_food_grievance_) {
        (void)region;
        if(unmet>0)return true;
    }
    return false;
}

inline void IntegratedWorld::p3_daily_react(std::int64_t today) {
    require_stage({9});
    ensure(today>=0&&today>p3_last_reaction_day_,
           "P3 political attitudes updated twice or out of calendar order");
    if(!p3_needs_daily_tick())return;
    std::set<Id> candidates;
    for(const auto& [id,att]:p3_attitudes_) {
        (void)att;
        if(people_.has_slice(id))candidates.insert(id);
    }
    for(const auto& [key,n]:membership_) {
        if(n<=0||!people_.has_slice(key.first))continue;
        const auto& dem=people_.demographic(
            people_.slice(key.first).key.demographic);
        const auto sovereignty=sovereignty_.find(dem.region);
        if(sovereignty!=sovereignty_.end()&&
           p3_groups_.contains({sovereignty->second,key.second}))
            candidates.insert(key.first);
    }
    // Real food shortage directly affects otherwise-unaffiliated POPs too.
    bool shortage=false;
    for(const auto& [region,n]:regional_food_grievance_) {
        (void)region;
        if(n>0){shortage=true;break;}
    }
    if(shortage) {
        for(const auto& [id,row]:people_.slices()) {
            const auto& dem=people_.demographic(row.key.demographic);
            if(food_grievance(dem.region)>0)
                candidates.insert(id);
        }
    }
    // Unaffiliated, unshocked POPs keep their aggregate existing profile.
    // One national movement cannot allocate millions of duplicate, mostly
    // neutral per-slice records on every tick. Explicit issue opinions and
    // actual organization membership remain the political contact surface.
    for(const auto id:candidates) {
        if(!people_.has_slice(id))continue;
        const auto& row=people_.slice(id);
        if(row.count<=0)continue;
        const auto& dem=people_.demographic(row.key.demographic);
        const auto& economic=people_.economic(row.key.economic);
        auto& attitude=p3_attitudes_[id];
        attitude.slice=id;
        if(attitude.observed_day>=today)continue;
        const bool job_seeking=economic.labor_class=="employee" ||
                               economic.labor_class=="worker";
        const People employed=people_.employed(id);
        const People unemployed=job_seeking?
            row.count-employed:0;
        const auto unemployment=unemployed>0?
             p3_fraction(unemployed,row.count):0;
        const People shortage=std::min(row.count,
             std::max<People>(0,food_grievance(dem.region)));
        const auto scarcity=shortage>0?
             p3_fraction(shortage,row.count):0;
        // Both changes use actually observed economic conditions. Political
        // memory evolves gradually, not a fixed trait of religion/culture.
        const std::int64_t pressure=static_cast<std::int64_t>(
            unemployment)/400+scarcity/250;
        const std::int64_t relief=(employed>0?900:250)+
                                    (scarcity==0?200:0);
        attitude.radical_ppm=static_cast<std::int32_t>(
            std::clamp<std::int64_t>(
                static_cast<std::int64_t>(attitude.radical_ppm)+
                pressure-relief,0,1'000'000));
        const auto loyalty_delta=pressure>0?
            -static_cast<std::int64_t>(pressure/2):
            static_cast<std::int64_t>(employed>0?500:100);
        attitude.loyal_ppm=static_cast<std::int32_t>(
            std::clamp<std::int64_t>(
                static_cast<std::int64_t>(attitude.loyal_ppm)+
                loyalty_delta,0,1'000'000-attitude.radical_ppm));
        attitude.engagement_ppm=static_cast<std::int32_t>(
            std::clamp<std::int64_t>(
                100'000+static_cast<std::int64_t>(attitude.radical_ppm)/2+
                static_cast<std::int64_t>(attitude.loyal_ppm)/4,
                0,1'000'000));
        attitude.observed_day=today;
    }
    // A scenario may define a *dormant* reform movement. It becomes
    // active from real observed demand, not a random ideology factory. A
    // previously settled P5 movement cannot silently restart in this pass.
    for(auto& [id,movement]:p3_movements_) {
        if(movement.active)continue;
        const auto past=p5_crises_.find("P5:"+id);
        if(past!=p5_crises_.end()&&
           past->second.stage==P5CrisisStage::settled)continue;
        const auto evidence=p3_movement_view(id);
        bool already_satisfied=false;
        for(const auto& [slot,law_id]:p2_active_base_) {
            (void)slot;
            const auto& law=p2_laws_.at(law_id);
            if(law.country==movement.country&&
               law.domain==movement.issue_domain&&
               law.base_choice==movement.desired_choice)
                already_satisfied=true;
        }
        if(!already_satisfied&&evidence.support_ppm>=150'000&&
           evidence.potential_activists>0) {
            movement.active=true;
            events_.push_back("p3-social-demand-formed-persistent-movement:"+id);
        }
    }
    p3_last_reaction_day_=today;
    events_.push_back("p3-real-pop-daily-political-reaction");
}

inline void IntegratedWorld::p3_reconcile_population() {
    require_stage({10});
    // Political affiliations and opinions are references into Phase B; never
    // preserve orphaned numbers after real deaths, employment splits or aging.
    for(auto it=p3_attitudes_.begin();it!=p3_attitudes_.end();) {
        if(!people_.has_slice(it->first))it=p3_attitudes_.erase(it);
        else ++it;
    }
    for(auto it=membership_.begin();it!=membership_.end();) {
        if(!people_.has_slice(it->first.first))
            it=membership_.erase(it);
        else {
            it->second=std::min(it->second,
                   people_.slice(it->first.first).count);
            if(it->second==0)it=membership_.erase(it);
            else ++it;
        }
    }
    for(auto it=p5_active_daily_strikes_.begin();
        it!=p5_active_daily_strikes_.end();) {
        if(!people_.has_slice(it->first))
            it=p5_active_daily_strikes_.erase(it);
        else {
            const auto base=strike_active_.find(it->first);
            const People monthly=base==strike_active_.end()?0:base->second;
            const People employed=people_.employed(it->first);
            it->second=std::min(it->second,
                               std::max<People>(0,employed-monthly));
            if(it->second==0)it=p5_active_daily_strikes_.erase(it);
            else ++it;
        }
    }
    for(auto& [when,allocations]:p5_scheduled_daily_strikes_) {
        (void)when;
        for(auto it=allocations.begin();it!=allocations.end();) {
            if(!people_.has_slice(it->first))
                it=allocations.erase(it);
            else {
                it->second=std::min(it->second,
                    people_.slice(it->first).count);
                if(it->second==0)it=allocations.erase(it);
                else ++it;
            }
        }
    }
    for(auto it=p5_scheduled_daily_strikes_.begin();
        it!=p5_scheduled_daily_strikes_.end();) {
        if(it->second.empty())it=p5_scheduled_daily_strikes_.erase(it);
        else ++it;
    }
}
} // namespace vic25::phasec
