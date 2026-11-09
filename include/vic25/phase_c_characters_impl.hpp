#pragma once
// Political identity is strictly an annotation on a living PersonAnchor.
// Neither succession nor agitation creates POPs, candidates or commanders.
namespace vic25::phasec {
inline bool IntegratedWorld::p1_character_available(
        const std::string& person)const {
    const auto it=person_anchors_.find(person);
    if(it==person_anchors_.end()||!people_.has_slice(it->second)||
       people_.slice(it->second).count<=0)return false;
    const auto& demo=people_.demographic(
        people_.slice(it->second).key.demographic);
    if(!sovereignty_.contains(demo.region)||
       people_.age_months_slice(it->second)<16*12)return false;
    const auto recorded=p1_characters_.find(person);
    return recorded==p1_characters_.end()||
           recorded->second.country==sovereignty_.at(demo.region);
}
inline void IntegratedWorld::p1_register_political_character(
        PoliticalCharacter person) {
    ensure((stage_==-1&&commit_id_==0)||stage_==9,
           "political person registration needs lawful stage");
    ensure(!person.person_id.empty()&&p1_character_available(person.person_id)&&
           !p1_characters_.contains(person.person_id)&&
           governments_.contains(person.country)&&
           person.roles>0&&person.roles<=31&&
           person.popularity_ppm>=-1'000'000&&
           person.popularity_ppm<=1'000'000&&
           (person.interest_group.empty()||
            p3_groups_.contains({person.country,person.interest_group}))&&
           (person.movement.empty()||
            (p3_movements_.contains(person.movement)&&
             p3_movements_.at(person.movement).country==person.country)),
           "political person is not a valid actual resident/role");
    const auto id=person_anchors_.at(person.person_id);
    const auto& demo=people_.demographic(people_.slice(id).key.demographic);
    ensure(sovereignty_.at(demo.region)==person.country,
           "political character resident in another country");
    p1_characters_.emplace(person.person_id,std::move(person));
    events_.push_back("p1-real-person-political-role:"+demo.region);
}
inline void IntegratedWorld::p1_set_political_exile(
        const std::string& person,bool exiled) {
    require_stage({9});
    auto& character=p1_characters_.at(person);
    ensure(p1_character_available(person),
           "political exile may not revive a dead or missing person");
    // This changes political eligibility only. Any physical migration and
    // citizenship change must be executed by the population domain.
    character.politically_exiled=exiled;
    character.active=!exiled;
    events_.push_back("p1-political-exile-status:"+person);
}
inline void IntegratedWorld::p1_set_favorite(
        const std::string& country,const std::string& person) {
    require_stage({9});
    const auto power=p1_constitution(country).power;
    ensure(power==P1PowerDistribution::autocracy||
           power==P1PowerDistribution::oligarchy||
           power==P1PowerDistribution::technocracy||
           power==P1PowerDistribution::landed_vote,
           "favorites prohibited by current distribution of power");
    ensure(p1_characters_.contains(person)&&
           p1_characters_.at(person).country==country&&
           p1_characters_.at(person).active&&
           !p1_characters_.at(person).politically_exiled&&
           p1_character_available(person),
           "favorite must be an active domestic actual character");
    p1_favorites_[country]=person;
    events_.push_back("p1-legally-appointed-favorite:"+country);
}
inline std::string IntegratedWorld::p1_group_legal_preference(
        const P3Group& group)const {
    if(!group.leader_anchor.empty()) {
        const auto it=p1_characters_.find(group.leader_anchor);
        if(it!=p1_characters_.end()&&it->second.active&&
           !it->second.politically_exiled&&
           p1_character_available(group.leader_anchor)) {
            const auto personal=it->second.law_preferences.find(
                 group.issue_domain);
            if(personal!=it->second.law_preferences.end())
                return personal->second;
        }
    }
    return group.preferred_choice;
}
inline bool IntegratedWorld::p1_has_character_work()const {
    return !p1_characters_.empty();
}
inline void IntegratedWorld::p1_advance_characters(std::int64_t day) {
    require_stage({9});
    for(auto& [id,c]:p1_characters_) {
        if(!c.active||c.last_review_day>=day)continue;
        if(!p1_character_available(id)) {
            c.active=false;
            events_.push_back("p1-actual-POP-political-person-inactive:"+id);
            continue;
        }
        c.last_review_day=day;
        if(c.politically_exiled||
           (c.roles&static_cast<std::uint8_t>(PoliticalRole::agitator))==0||
           c.movement.empty()||!p3_movements_.contains(c.movement))
            continue;
        auto& movement=p3_movements_.at(c.movement);
        if(movement.active&&movement.country==c.country&&day%7==0)
            movement.activism_ppm=std::min(1'000'000,
                                         movement.activism_ppm+5'000);
    }
    // Leadership succession selects an already existing, viable POP anchor.
    for(auto& [key,g]:p3_groups_) {
        if(g.leader_anchor.empty())continue;
        const auto leadership=p1_characters_.find(g.leader_anchor);
        if(p1_character_available(g.leader_anchor)&&
           (leadership==p1_characters_.end()||
            (leadership->second.active&&
             !leadership->second.politically_exiled)))
            continue;
        std::string best;std::int32_t approval=-1'000'001;
        for(const auto& [id,c]:p1_characters_)
            if(c.active&&!c.politically_exiled&&
               c.country==g.country&&c.interest_group==g.id&&
               (c.roles&static_cast<std::uint8_t>(
                   PoliticalRole::interest_group_leader))!=0&&
               p1_character_available(id)&&c.popularity_ppm>approval) {
                best=id;approval=c.popularity_ppm;
            }
        g.leader_anchor=best;
        events_.push_back("p1-interest-group-successor:"+key.second);
    }
    for(auto it=p1_favorites_.begin();it!=p1_favorites_.end();) {
        if(!p1_characters_.contains(it->second)||
           !p1_characters_.at(it->second).active||
           !p1_character_available(it->second))
            it=p1_favorites_.erase(it);
        else ++it;
    }
}
} // namespace vic25::phasec
