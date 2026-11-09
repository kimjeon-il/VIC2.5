#pragma once
// Three-step law progress; C/P3 political support is the source of probability,
// while existing P2/P4 chambers retain the final legal vote and law effects.
namespace vic25::phasec {
inline bool IntegratedWorld::p2_has_review_due(std::int64_t day)const {
    return p2_review_due_.contains(day);
}
inline bool IntegratedWorld::p2_enactment_ready(const std::string& id)const {
    const auto it=p2_enactments_.find(id);
    return it==p2_enactments_.end()||it->second.ready; // legacy saves
}
inline void IntegratedWorld::p2_rebuild_review_due_index() {
    p2_review_due_.clear();
    for(const auto& [id,e]:p2_enactments_) {
        ensure(p2_bills_.contains(id)&&e.bill_id==id&&
               e.country==p2_bills_.at(id).country&&
               e.phase>=1&&e.phase<=4&&e.setbacks>=0&&
               e.setbacks<=3&&e.checks>=0,
               "P2 invalid saved enactment");
        if(!e.ready&&p2_bills_.at(id).status==P2BillStatus::submitted) {
            ensure(e.next_review_day>=0,"P2 pending enactment has no due day");
            p2_review_due_[e.next_review_day].insert(id);
        }
    }
}
inline void IntegratedWorld::p2_review_due(std::int64_t today) {
    require_stage({9});
    const auto it=p2_review_due_.find(today);
    if(it==p2_review_due_.end())return;
    const auto due=it->second;
    p2_review_due_.erase(it);
    for(const auto& id:due) {
        auto& e=p2_enactments_.at(id);
        auto& bill=p2_bills_.at(id);
        if(e.ready||bill.status!=P2BillStatus::submitted)continue;
        ensure(e.next_review_day==today&&e.last_review_day<today,
               "P2 enactment checkpoint replayed");
        const auto legit=p1_legitimacy(bill.country).score;
        const auto law_domain=bill.action==P2BillAction::enact_or_replace_base?
            bill.proposed_base.domain:
            p2_laws_.at(bill.action==P2BillAction::attach_amendment?
                bill.proposed_amendment.law_id:
                bill.action==P2BillAction::repeal_base?
                bill.target_id:p2_amendments_.at(bill.target_id).law_id).domain;
        const auto desired=bill.action==P2BillAction::enact_or_replace_base?
            bill.proposed_base.base_choice:std::string{};
        std::int64_t support=0,opposition=0,movement_activism=0;
        for(const auto& [key,group]:p3_groups_) {
            if(key.first!=bill.country||group.issue_domain!=law_domain)continue;
            const auto clout=p3_group_view(key.first,key.second).clout_ppm;
            const auto& coalition=p1_coalitions_.at(bill.country);
            const bool governing=std::find(coalition.ruling_groups.begin(),
              coalition.ruling_groups.end(),key.second)!=coalition.ruling_groups.end();
            if(!desired.empty()&&group.preferred_choice==desired) {
                if(governing)support+=clout;
            } else if(!group.preferred_choice.empty()&&
                      group.preferred_choice!=desired)opposition+=clout;
        }
        for(const auto& [mid,m]:p3_movements_) {
            if(!m.active||m.country!=bill.country||
               m.issue_domain!=law_domain)continue;
            const auto view=p3_movement_view(mid);
            if((!desired.empty()&&m.desired_choice==desired)||
               (desired.empty()&&m.goal==P3MovementGoal::legal_reform)) {
                support+=view.support_ppm/2;
                movement_activism=std::max<std::int64_t>(
                    movement_activism,view.activism_ppm);
            } else opposition+=view.support_ppm/2;
        }
        e.last_review_day=today;
        if(legit<25&&movement_activism<250'000) {
            e.next_review_day=today+7;
            p2_review_due_[e.next_review_day].insert(id);
            events_.push_back("p2-legitimacy-pauses-law:"+id);
            continue;
        }
        std::int64_t success=std::clamp<std::int64_t>(
            support/2+e.success_bonus_ppm,0,900'000);
        std::int64_t stall=std::clamp<std::int64_t>(
            opposition/2+e.setbacks*25'000,0,900'000);
        if(success+stall>1'000'000){
            const auto total=success+stall;
            success=success*1'000'000/total;
            stall=1'000'000-success;
        }
        const auto advance=std::clamp<std::int64_t>(
            2*success-stall,0,1'000'000-success-stall);
        std::uint64_t seed=14695981039346656037ULL;
        const auto key=e.country+":"+id+":"+std::to_string(today)+":"+
                       std::to_string(e.checks);
        for(const unsigned char ch:key){ seed^=ch;seed*=1099511628211ULL; }
        const auto roll=static_cast<std::int64_t>(seed%1'000'000);
        ++e.checks;
        if(success==0) {
            ++e.setbacks;
            events_.push_back("p2-zero-success:"+id);
        } else if(roll<success) {
            if(e.phase==3) {
                e.phase=4;e.ready=true;e.next_review_day=-1;
                events_.push_back("p2-ready-for-chamber-vote:"+id);
                continue;
            }
            ++e.phase;
            events_.push_back("p2-phase-success:"+id);
        } else if(roll<success+advance) {
            e.success_bonus_ppm=std::min(500'000,e.success_bonus_ppm+50'000);
            events_.push_back("p2-advance-debate:"+id);
        } else if(roll<success+advance+stall) {
            e.success_bonus_ppm=std::max(-500'000,e.success_bonus_ppm-40'000);
            ++e.setbacks;
            events_.push_back("p2-stalled:"+id);
        } else {
            e.success_bonus_ppm=std::max(-500'000,e.success_bonus_ppm-10'000);
            events_.push_back("p2-debate:"+id);
        }
        if(e.setbacks>=3) {
            bill.status=P2BillStatus::rejected;
            e.next_review_day=-1;
            p2_retry_after_[e.law_key]=today+730;
            p2_negotiation_watch_.erase(id);
            events_.push_back("p2-rejected:"+id);
            continue;
        }
        const int days=law_domain=="governance_principles"?200:
           (law_domain=="distribution_of_power"||
            law_domain=="economic_system"||law_domain=="slavery")?150:100;
        e.next_review_day=today+std::clamp(
            days*(150-std::clamp(legit,0,100))/100,30,300);
        p2_review_due_[e.next_review_day].insert(id);
    }
}
} // namespace vic25::phasec
