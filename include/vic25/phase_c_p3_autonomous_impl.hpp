#pragma once
// Dynamic movements depend on C's live issue preferences and real IG clout.
// A small monthly formation pass avoids a world-wide POP candidate search.
namespace vic25::phasec {
inline void IntegratedWorld::p3_autonomous_movements(std::int64_t today) {
    require_stage({9});
    // First reconcile active movements against enacted law and live support.
    for(auto& [id,m]:p3_movements_) {
        if(!m.active)continue;
        bool fulfilled=false;
        for(const auto& [slot,law]:p2_active_base_) {
            (void)slot;
            const auto& row=p2_laws_.at(law);
            if(row.country==m.country&&row.domain==m.issue_domain&&
               row.base_choice==m.desired_choice) {
                fulfilled=true;break;
            }
        }
        if(fulfilled) {
            m.active=false;
            p3_movement_calm_days_.erase(id);
            events_.push_back("p3-movement-goal-met:"+id);
            continue;
        }
        if(today%7!=0)continue;
        const auto observed=p3_movement_view(id);
        auto& quiet=p3_movement_calm_days_[id];
        if(observed.support_ppm<50'000&&observed.activism_ppm<150'000)
            quiet=std::min(1000,quiet+7);
        else quiet=0;
        if(quiet>=28) {
            m.active=false;
            events_.push_back("p3-unbacked-movement-disbanded:"+id);
        }
    }
    if(today%30!=0)return;
    for(const auto& [key,group]:p3_groups_) {
        const auto choice=p1_group_legal_preference(group);
        if(group.issue_domain.empty()||choice.empty())continue;
        const auto view=p3_group_view(key.first,key.second);
        if(view.members<=0||view.clout_ppm<50'000)continue;
        bool satisfied=false;
        for(const auto& [slot,id]:p2_active_base_) {
            (void)slot;
            const auto& law=p2_laws_.at(id);
            if(law.country!=group.country||
               law.domain!=group.issue_domain)continue;
            satisfied=law.base_choice==choice;
            break;
        }
        if(satisfied)continue;
        const auto id="P3:auto:"+group.country+":"+group.id+":"+
                      group.issue_domain+":"+choice;
        const auto previous=p5_crises_.find("P5:"+id);
        if(previous!=p5_crises_.end()&&
           previous->second.stage==P5CrisisStage::settled)continue;
        if(p3_movements_.contains(id))continue;
        P3Movement movement;
        movement.id=id;
        movement.country=group.country;
        movement.issue_domain=group.issue_domain;
        movement.desired_choice=choice;
        movement.groups={group.id};
        movement.goal=P3MovementGoal::legal_reform;
        movement.activism_ppm=std::clamp(view.engagement_ppm,50'000,1'000'000);
        movement.radical_ppm=view.radical_ppm;
        p3_seed_movement(std::move(movement));
        p3_movement_types_[id]=P3MovementType::ideological;
    }
}
} // namespace vic25::phasec
