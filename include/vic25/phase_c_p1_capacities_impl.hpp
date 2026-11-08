#pragma once
// Pure derived P1 strategic capacities; policy records are distinct from
// capacity balances. No alternate finance, GDP, military or population ledger.
namespace vic25::phasec {

inline P1FlowCapacity IntegratedWorld::p1_authority_capacity(
        const std::string& country)const {
    ensure(governments_.contains(country),"P1 authority requires sovereign government");
    const auto legal=p2_modifier_ppm(country,"country_authority_add");
    const auto base=std::max<std::int64_t>(0,
        phaseb::add_checked(p1power::capacity_base,legal/1'000'000));
    const auto multiplier=std::clamp<std::int64_t>(
        1'000'000+phaseb::add_checked(
            p2_modifier_ppm(country,"country_authority_mult"),
            p3_trait_modifier_ppm(country,"country_authority_mult")),
        0,5'000'000);
    const auto generated=phaseb::mul_checked(base,multiplier)/1'000'000;
    std::int64_t committed=0;
    for(const auto& [id,policy]:p1_active_mandates_) {
        (void)id;
        if(policy.country==country)
            committed=phaseb::add_checked(
                committed,p1power::authority_cost(policy.kind));
    }
    return p1power::flow(generated,committed);
}

inline std::int64_t IntegratedWorld::p1_local_decree_ppm(
        const std::string& country,const std::string& region,
        const std::string& metric)const {
    ensure(governments_.contains(country)&&sovereignty_.contains(region)&&
           sovereignty_.at(region)==country,
           "P1 decree must target actual sovereign region");
    std::int64_t sum=0;
    for(const auto& [id,m]:p1_active_mandates_) {
        (void)id;
        if(m.country!=country||m.target!=region)continue;
        std::int64_t value=0;
        if(m.kind==P1MandateKind::decree_road_maintenance) {
            if(metric=="state_construction_mult")value=100'000;
            else if(metric=="state_infrastructure_from_population_add")
                value=1'000'000;
            else if(metric=="state_infrastructure_from_population_max_add")
                value=20'000'000;
        } else if(m.kind==P1MandateKind::decree_promote_social_mobility) {
            if(metric=="state_education_access_add"||
               metric=="state_pop_qualifications_mult")value=250'000;
        }
        sum=phaseb::add_checked(sum,value);
    }
    return sum;
}

inline void IntegratedWorld::p1_schedule_mandate(
        P1Mandate policy,const std::string& prime,std::int64_t today) {
    require_stage({9});
    ensure(today>=0&&!policy.id.empty()&&!policy.country.empty()&&
           !policy.target.empty()&&!policy.legal_basis.empty()&&
           governments_.contains(policy.country)&&
           governments_.at(policy.country).prime_minister==prime&&
           p1_constitutions_.contains(policy.country)&&
           policy.kind<=P1MandateKind::suppress_interest_group,
           "P1 unauthorized policy or unknown constitutional country");
    // Only decrees whose *real physical effects* have been connected to
    // Phase D/POP are issuable. IG bolstering needs attraction/recruitment
    // mechanics and must not be a fictional +clout shortcut.
    ensure(policy.kind==P1MandateKind::decree_road_maintenance||
           policy.kind==P1MandateKind::decree_promote_social_mobility,
           "P1 political attraction effect not yet backed by a real mechanism");
    ensure(sovereignty_.contains(policy.target)&&
           sovereignty_.at(policy.target)==policy.country,
           "P1 decree must target existing owned region");
    ensure(!p1_active_mandates_.contains(policy.id),
           "P1 duplicate persistent political measure");
    std::int64_t already_pending=0;
    for(const auto& [day,changes]:p1_pending_mandates_) {
        (void)day;
        for(const auto& change:changes) {
            ensure(change.id!=policy.id,"P1 duplicate pending measure");
            if(change.terminate||change.mandate.country!=policy.country)continue;
            already_pending=phaseb::add_checked(
                already_pending,p1power::authority_cost(change.mandate.kind));
            ensure(change.mandate.kind!=policy.kind||
                   change.mandate.target!=policy.target,
                   "P1 duplicate decree in same region");
        }
    }
    for(const auto& [id,active]:p1_active_mandates_) {
        (void)id;
        ensure(active.country!=policy.country||active.kind!=policy.kind||
               active.target!=policy.target,
               "P1 duplicate active decree in same region");
    }
    const auto available=p1_authority_capacity(policy.country).balance;
    ensure(available>=phaseb::add_checked(
               already_pending,p1power::authority_cost(policy.kind)),
           "P1 insufficient continuing Authority generation");
    policy.adopted_day=today;
    policy.effective_day=today+1;
    const auto id=policy.id;
    p1_pending_mandates_[today+1].push_back({id,std::move(policy),false});
    events_.push_back("p1-authority-decree-scheduled:"+id);
}

inline void IntegratedWorld::p1_schedule_mandate_termination(
        const std::string& country,const std::string& id,
        const std::string& prime,std::int64_t today) {
    require_stage({9});
    ensure(governments_.contains(country)&&
           governments_.at(country).prime_minister==prime&&
           p1_active_mandates_.contains(id)&&
           p1_active_mandates_.at(id).country==country&&today>=0,
           "P1 only actual government's decree may be withdrawn");
    for(const auto& [day,changes]:p1_pending_mandates_) {
        (void)day;
        for(const auto& change:changes)
            ensure(change.id!=id,"P1 repeated pending decree change");
    }
    p1_pending_mandates_[today+1].push_back({id,{},true});
    events_.push_back("p1-authority-decree-withdrawal-scheduled:"+id);
}

inline void IntegratedWorld::phase_time_apply_p1_mandates(
        std::int64_t today) {
    require_stage({0});
    const auto found=p1_pending_mandates_.find(today);
    if(found==p1_pending_mandates_.end())return;
    for(const auto& change:found->second) {
        if(change.terminate) {
            ensure(p1_active_mandates_.erase(change.id)==1,
                   "P1 scheduled withdrawal missing active decree");
        } else {
            ensure(change.mandate.effective_day==today&&
                   p1_active_mandates_.emplace(
                       change.id,change.mandate).second,
                   "P1 scheduled decree applied more than once");
        }
    }
    p1_pending_mandates_.erase(found);
}

inline void IntegratedWorld::p1_record_standing(P1StandingEvidence input) {
    ensure((stage_==-1&&commit_id_==0)||stage_==9,
           "P1 source-qualified standing changes require political stage");
    ensure(!input.country.empty()&&governments_.contains(input.country)&&
           !input.source.empty()&&input.observed_day>=0&&
           input.tier<=P1CountryTier::hegemony&&
           input.confirmed_rank<=P1PowerRank::great_power&&
           input.own_gdp>=0&&input.subject_gdp>=0&&
           input.army_projection>=0&&input.navy_projection>=0&&
           input.goods_leadership>=0&&input.monuments>=0&&
           input.other_prestige>=-1'000'000&&
           input.other_prestige<=1'000'000,
           "P1 prestige evidence must have real sovereign and dated provenance");
    auto it=p1_standing_.find(input.country);
    ensure(it==p1_standing_.end()||
           input.observed_day>=it->second.observed_day,
           "P1 standing evidence cannot be replaced by older data");
    p1_standing_[input.country]=std::move(input);
}

inline P1PrestigeView IntegratedWorld::p1_prestige(
        const std::string& country)const {
    ensure(governments_.contains(country),"P1 prestige requires a country");
    P1PrestigeView view;
    view.country=country;
    const auto source=p1_standing_.find(country);
    if(source==p1_standing_.end())return view;
    const auto& data=source->second;
    view.observed=true;
    view.complete=data.complete;
    view.observed_day=data.observed_day;
    view.source=data.source;
    view.tier=p1power::prestige_tier(data.tier);
    // 1.15 source defines: £1m own GDP -> 3 prestige, £1m subject GDP
    // -> 1.5 prestige. These GDP numbers are explicit SOURCE OBSERVATIONS
    // until a GDP authority is implemented; never substitute bank cash.
    view.own_gdp=phaseb::mul_checked(data.own_gdp/1'000'000,3)+
                  phaseb::mul_checked(data.own_gdp%1'000'000,3)/1'000'000;
    view.subject_gdp=phaseb::mul_checked(data.subject_gdp/2'000'000,3)+
                  phaseb::mul_checked(data.subject_gdp%2'000'000,3)/2'000'000;
    view.army=data.army_projection;
    view.navy=data.navy_projection;
    view.goods=data.goods_leadership;
    view.monuments=data.monuments;
    view.other=data.other_prestige;
    view.partial_total=std::max<std::int64_t>(0,
        phaseb::add_checked(
            phaseb::add_checked(
                phaseb::add_checked(view.tier,view.own_gdp),
                phaseb::add_checked(view.subject_gdp,view.army)),
            phaseb::add_checked(
                phaseb::add_checked(view.navy,view.goods),
                phaseb::add_checked(view.monuments,view.other))));
    const auto mult=std::clamp<std::int64_t>(
        1'000'000+phaseb::add_checked(
            p2_modifier_ppm(country,"country_prestige_mult"),
            p3_trait_modifier_ppm(country,"country_prestige_mult")),
        0,5'000'000);
    view.partial_total=phaseb::mul_checked(view.partial_total,mult)/1'000'000;
    if(data.complete)view.total=view.partial_total;
    view.rank=data.confirmed_rank;
    view.rank_confirmed=data.confirmed_rank!=P1PowerRank::unobserved;
    return view;
}

inline P1PowerRank IntegratedWorld::p1_power_rank(
        const std::string& country)const {
    const auto info=p1_prestige(country);
    if(info.rank_confirmed)return info.rank;
    // Dynamic rankings need the WHOLE country universe and complete 1.15
    // prestige evidence. The base game pads small worlds to at least 100
    // participants; refusing unsupported shortcuts avoids fabricated GPs.
    if(governments_.size()<100||p1_standing_.size()!=governments_.size()||
       !info.complete)return P1PowerRank::unobserved;
    std::int64_t maximum=0,total=0;
    for(const auto& [id,gov]:governments_) {
        (void)gov;
        const auto current=p1_prestige(id);
        if(!current.complete)return P1PowerRank::unobserved;
        maximum=std::max(maximum,current.total);
        total=phaseb::add_checked(total,current.total);
    }
    const auto average=total/static_cast<std::int64_t>(governments_.size());
    const auto value=info.total;
    const bool recognized=p1_standing_.at(country).confirmed_rank!=
            P1PowerRank::decentralized_power;
    if(recognized&&value>=phaseb::mul_checked(average,5)&&
       phaseb::mul_checked(value,4)>=phaseb::mul_checked(maximum,3))
        return P1PowerRank::great_power;
    if(phaseb::mul_checked(value,2)>=phaseb::mul_checked(average,5)&&
       phaseb::mul_checked(value,2)>=maximum)
        return recognized?P1PowerRank::major_power:
                          P1PowerRank::unrecognized_major_power;
    if(phaseb::mul_checked(value,5)>=phaseb::mul_checked(average,3)&&
       phaseb::mul_checked(value,20)>=phaseb::mul_checked(maximum,3))
        return recognized?P1PowerRank::minor_power:
                          P1PowerRank::unrecognized_regional_power;
    return recognized?P1PowerRank::insignificant_power:
                      P1PowerRank::unrecognized_power;
}
} // namespace vic25::phasec
