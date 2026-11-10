#pragma once
// Authority is a national capacity view. Mandates reserve it while active;
// economic and foreign effects continue to belong to their native ledgers.
namespace vic25::phasec {
inline P1AuthorityBudget IntegratedWorld::p1_authority_budget(
        const std::string& country)const {
    const auto& c=p1_constitution(country);
    int supply=200;
    // The original 1.15 law is authoritative when present; P1 defaults
    // only cover imported worlds without an initialized law-group catalog.
    if(!p2_catalog_group_active(country,"governance_principles"))
    switch(c.governance) {
      case P1GovernancePrinciple::chiefdom:supply+=100;break;
      case P1GovernancePrinciple::monarchy:
      case P1GovernancePrinciple::theocracy:supply+=200;break;
      case P1GovernancePrinciple::presidential_republic:supply+=100;break;
      case P1GovernancePrinciple::parliamentary_republic:supply+=50;break;
      default:break;
    }
    if(!p2_catalog_group_active(country,"distribution_of_power"))
    switch(c.power) {
      case P1PowerDistribution::autocracy:supply+=500;break;
      case P1PowerDistribution::oligarchy:supply+=200;break;
      case P1PowerDistribution::landed_vote:supply+=200;break;
      case P1PowerDistribution::wealth_vote:supply+=100;break;
      case P1PowerDistribution::single_party:supply+=250;break;
      case P1PowerDistribution::technocracy:supply+=100;break;
      default:break;
    }
    supply+=static_cast<int>(p2_catalog_modifier_value(
        country,"country_authority_add"));
    const auto law_authority_mult=p2_catalog_modifier_value(
        country,"country_authority_mult");
    supply=std::max(0,static_cast<int>(supply*(1.0+law_authority_mult)));
    if(const auto it=p1_favorites_.find(country);
       it!=p1_favorites_.end()&&
       p1_characters_.contains(it->second)&&
       p1_characters_.at(it->second).active&&
       p1_character_available(it->second))
        supply+=50;
    int reserved=0;
    for(const auto& [id,m]:p1_mandates_)
        if(m.country==country&&m.active) {
            ensure(m.authority_cost>=0&&
                   reserved<=1'000'000-m.authority_cost,
                   "invalid active authority reservation");
            reserved+=m.authority_cost;
        }
    reserved+=std::max(0,static_cast<int>(p2_catalog_modifier_value(
        country,"country_authority_cost_add")));
    return {country,supply,reserved,supply-reserved};
}
inline std::int32_t IntegratedWorld::p1_region_decree_bonus_ppm(
        const std::string& country,const std::string& region,
        P1DecreeKind type)const {
    // Real edicts modify only paid/staffed services or material-backed work.
    for(const auto& [id,mandate]:p1_mandates_) {
        (void)id;
        if(mandate.active&&mandate.kind==P1MandateKind::regional_decree&&
           mandate.country==country&&mandate.target==region&&
           mandate.decree==type)
            return 200'000;
    }
    return 0;
}
inline void IntegratedWorld::p1_activate_mandate(
        P1Mandate mandate,std::int64_t today) {
    require_stage({9});
    ensure(!mandate.id.empty()&&!mandate.country.empty()&&
           !mandate.target.empty()&&mandate.active&&
           !p1_mandates_.contains(mandate.id)&&
           p1_constitutions_.contains(mandate.country)&&
           today>=0&&
           mandate.authority_cost>=0&&mandate.authority_cost<=1000,
           "P1 invalid national authority mandate");
    if(mandate.kind==P1MandateKind::bolster_movement||
       mandate.kind==P1MandateKind::suppress_movement) {
        ensure(mandate.authority_cost==200&&
               p3_movements_.contains(mandate.target)&&
               p3_movements_.at(mandate.target).country==mandate.country&&
               p3_movements_.at(mandate.target).active,
               "P1 political movement order needs 200 authority and real movement");
    } else if(mandate.kind==P1MandateKind::regional_decree) {
        ensure(sovereignty_.contains(mandate.target)&&
               sovereignty_.at(mandate.target)==mandate.country&&
               mandate.authority_cost==100&&
               static_cast<unsigned>(mandate.decree)<=
                 static_cast<unsigned>(P1DecreeKind::public_service_reach),
               "P1 decree needs a real region and 100 authority");
        for(const auto& [other_id,existing]:p1_mandates_) {
            (void)other_id;
            ensure(!existing.active||
                   existing.kind!=P1MandateKind::regional_decree||
                   existing.country!=mandate.country||
                   existing.target!=mandate.target||
                   existing.decree!=mandate.decree,
                   "P1 regional edict of each kind cannot stack");
        }
    } else {
        // Do not allow phantom consumption-tax revenue. Finance owns the
        // real consumption tax and must provide an executable fiscal order.
        throw std::invalid_argument(
            "P1 consumption tax requires native fiscal collection");
    }
    ensure(p1_authority_budget(mandate.country).available>=
               mandate.authority_cost,
           "P1 insufficient unreserved national authority");
    mandate.started_day=today;
    const auto id=mandate.id;
    p1_mandates_.emplace(id,std::move(mandate));
    events_.push_back("p1-real-authority-reserved:"+id);
}
inline void IntegratedWorld::p1_deactivate_mandate(const std::string& id) {
    require_stage({9});
    auto& m=p1_mandates_.at(id);
    ensure(m.active,"P1 cannot release an inactive mandate twice");
    m.active=false;
    events_.push_back("p1-authority-capacity-released:"+id);
}
inline void IntegratedWorld::p1_register_lobby(
        P1Lobby lobby,std::int64_t today) {
    require_stage({9});
    ensure(!lobby.id.empty()&&!p1_lobbies_.contains(lobby.id)&&
           lobby.active&&today>=0&&
           governments_.contains(lobby.source_country)&&
           governments_.contains(lobby.target_country)&&
           lobby.source_country!=lobby.target_country&&
           p3_groups_.contains({lobby.source_country,lobby.interest_group})&&
           !lobby.issue_domain.empty()&&!lobby.desired_choice.empty(),
           "P1 foreign lobby needs two real governments and backing IG");
    lobby.created_day=today;
    p1_lobbies_.emplace(lobby.id,std::move(lobby));
}
inline void IntegratedWorld::p1_resolve_political_event(
        const std::string& event_id,P1PoliticalEventDecision decision,
        std::int64_t today) {
    require_stage({9});
    auto& event=p1_political_events_.at(event_id);
    ensure(!event.resolved&&today>=event.day&&
           static_cast<unsigned>(decision)<=
               static_cast<unsigned>(P1PoliticalEventDecision::suppress),
           "P1 political choice must be timely and not repeated");
    if(event.kind==P1PoliticalEventKind::enactment_debate) {
        ensure(decision==P1PoliticalEventDecision::acknowledge||
               decision==P1PoliticalEventDecision::negotiate,
               "P1 debate may only be observed or lawfully negotiated");
        ensure(p2_enactments_.contains(event.source)&&
               p2_bills_.contains(event.source)&&
               p2_bills_.at(event.source).country==event.country,
               "P1 debate must reference a real national enactment");
        if(decision==P1PoliticalEventDecision::negotiate&&
           p2_bills_.at(event.source).status==P2BillStatus::submitted) {
            p2_auto_negotiate_pending(today);
            // Only an actually accepted P3 group demand can improve the
            // procedural chances of the bill; no invented support points.
            if(const auto it=p2_negotiated_.find(event.source);
               it!=p2_negotiated_.end()&&!it->second.empty()) {
                auto& process=p2_enactments_.at(event.source);
                process.success_bonus_ppm=std::min(500'000,
                                        process.success_bonus_ppm+25'000);
            }
        }
    } else if(event.kind==P1PoliticalEventKind::crisis_warning) {
        ensure(p5_crises_.contains(event.source)&&
               p5_crises_.at(event.source).country==event.country,
               "P1 political crisis event lacks a real movement");
        if(decision==P1PoliticalEventDecision::bolster||
           decision==P1PoliticalEventDecision::suppress) {
            const auto& crisis=p5_crises_.at(event.source);
            const auto& m=p3_movements_.at(crisis.movement);
            ensure(m.active&&
                   crisis.stage!=P5CrisisStage::settled,
                   "P1 may not issue a movement mandate for resolved crisis");
            P1Mandate mandate;
            mandate.id="P1:event:mandate:"+event_id;
            mandate.country=event.country;
            mandate.target=crisis.movement;
            mandate.kind=decision==P1PoliticalEventDecision::bolster?
                P1MandateKind::bolster_movement:
                P1MandateKind::suppress_movement;
            mandate.authority_cost=200;
            p1_activate_mandate(std::move(mandate),today);
        } else ensure(decision==P1PoliticalEventDecision::acknowledge,
                      "P1 warning requires lawful movement response");
    } else {
        ensure(decision==P1PoliticalEventDecision::acknowledge,
               "P1 event lacks lawful executable political decision");
    }
    event.resolved=true;
    events_.push_back("p1-resolved-lawful-political-event:"+event_id);
}
inline bool IntegratedWorld::p1_has_policy_work()const {
    for(const auto& [id,m]:p1_mandates_)if(m.active)return true;
    for(const auto& [id,l]:p1_lobbies_)if(l.active)return true;
    return false;
}
inline void IntegratedWorld::p1_advance_political_policies(std::int64_t today) {
    require_stage({9});
    for(auto& [id,m]:p1_mandates_) {
        if(!m.active)continue;
        if(m.kind==P1MandateKind::regional_decree) {
            if(!sovereignty_.contains(m.target)||
               sovereignty_.at(m.target)!=m.country) {
                m.active=false;
                events_.push_back("p1-regional-decree-right-expired:"+id);
            }
            continue;
        }
        const auto it=p3_movements_.find(m.target);
        if(it==p3_movements_.end()||!it->second.active) {
            m.active=false;
            events_.push_back("p1-political-movement-directive-expired:"+id);
            continue;
        }
        const auto& movement=it->second;
        // Apply persuasion to actual POP political opinions, not imaginary
        // protester headcount or an Interest Group's political power stock.
        for(const auto& [key,real_members]:membership_) {
            if(real_members<=0||!people_.has_slice(key.first)||
               std::find(movement.groups.begin(),movement.groups.end(),
                   key.second)==movement.groups.end())continue;
            const auto& d=people_.demographic(
                people_.slice(key.first).key.demographic);
            if(!sovereignty_.contains(d.region)||
               sovereignty_.at(d.region)!=m.country)continue;
            auto& attitude=p3_attitudes_[key.first];
            attitude.slice=key.first;
            auto& stance=attitude.issues[movement.issue_domain];
            // The law affects a genuinely supported movement policy.
            // Suppression-attraction penalties reduce support more strongly;
            // no new phantom interest-group membership is created.
            const auto law_key=m.kind==P1MandateKind::bolster_movement?
                "country_bolster_attraction_factor":
                "country_suppression_attraction_factor";
            const double modifier=p2_region_modifier_value(d.region,law_key);
            const double scale=std::clamp(
                m.kind==P1MandateKind::bolster_movement?
                    1.0+modifier:1.0-modifier,0.0,3.0);
            const int magnitude=static_cast<int>(2'000*scale);
            const int direction=m.kind==P1MandateKind::bolster_movement?
                magnitude:-magnitude;
            stance.preference_ppm=std::clamp(
                stance.preference_ppm+direction,-1'000'000,1'000'000);
            stance.importance_ppm=std::max(stance.importance_ppm,100'000);
        }
    }
    for(auto& [id,lobby]:p1_lobbies_) {
        if(!lobby.active||lobby.last_review_day>=today||today%7!=0)
            continue;
        const auto group=p3_groups_.find(
            {lobby.source_country,lobby.interest_group});
        if(group==p3_groups_.end()){lobby.active=false;continue;}
        const auto view=p3_group_view(
            lobby.source_country,lobby.interest_group);
        lobby.last_review_day=today;
        bool satisfied=false;
        for(const auto& [slot,law_id]:p2_active_base_) {
            (void)slot;
            const auto& law=p2_laws_.at(law_id);
            if(law.country==lobby.target_country&&
               law.domain==lobby.issue_domain&&
               law.base_choice==lobby.desired_choice)satisfied=true;
        }
        lobby.satisfaction_ppm=satisfied?1'000'000:0;
        lobby.pressure_ppm=satisfied?0:
            std::clamp(view.clout_ppm/2,0,500'000);
        // A lobby tracks domestic pressure. Any treaty/relationship action
        // must be emitted by the Phase H diplomacy owner, not this ledger.
    }
    // Generate at most one persistent informational event per real source/day.
    for(const auto& [id,entry]:p2_enactments_) {
        if(entry.last_review_day!=today||entry.ready)continue;
        const auto event_id="P1:event:law:"+id+":"+std::to_string(today);
        if(!p1_political_events_.contains(event_id))
            p1_political_events_.emplace(event_id,P1PoliticalEvent{
                event_id,entry.country,id,
                P1PoliticalEventKind::enactment_debate,today,false});
    }
    for(const auto& [id,crisis]:p5_crises_) {
        if(crisis.stage!=P5CrisisStage::warning||crisis.last_checked_day!=today)
            continue;
        const auto event_id="P1:event:crisis:"+id;
        if(!p1_political_events_.contains(event_id))
            p1_political_events_.emplace(event_id,P1PoliticalEvent{
                event_id,crisis.country,id,
                P1PoliticalEventKind::crisis_warning,today,false});
    }
}
} // namespace vic25::phasec
