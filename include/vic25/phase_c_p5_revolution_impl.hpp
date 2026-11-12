#pragma once
// Political crisis clock may authorize a rebellion, but cannot create soldiers,
// equipment, new sovereignty or war assets from a progress percentage.
namespace vic25::phasec {
inline void IntegratedWorld::p5_auto_prepare_revolutions(std::int64_t day) {
    require_stage({9});
    for(auto& [id,crisis]:p5_crises_) {
        if(crisis.stage==P5CrisisStage::settled) {
            p5_revolution_preparations_.erase(id);
            continue;
        }
        if(crisis.stage!=P5CrisisStage::warning||
           (crisis.kind!=P5CrisisKind::revolution&&
            crisis.kind!=P5CrisisKind::secession&&
            crisis.kind!=P5CrisisKind::coup))continue;
        const auto risk=p5_risk(crisis.movement);
        auto& p=p5_revolution_preparations_[id];
        if(p.crisis_id.empty()) {
            p.crisis_id=id;p.started_day=day;p.last_review_day=-1;
        }
        if(p.last_review_day>=day)continue;
        p.last_review_day=day;
        if(!risk.eligible_for_warning) {
            p.progress_ppm=std::max(0,p.progress_ppm-10'000);
            continue;
        }
        const auto delta=std::clamp(
             1'000+risk.support_ppm/200+
             risk.activism_ppm/200+risk.radical_ppm/200,1'000,20'000);
        p.progress_ppm=std::min(1'000'000,p.progress_ppm+delta);
        if(p.progress_ppm<600'000)continue;
        if(crisis.pledged_units.empty()) {
            const auto& movement=p3_movements_.at(crisis.movement);
            for(const auto& [unit,assignment]:army_deployments_) {
                if(assignment.country!=crisis.country||
                   !units_.contains(unit)||active_strength(unit)<=0||
                   stock(unit,assignment.region,"ammunition")<=0||
                   (stock(unit,assignment.region,"rifle")<=0&&
                    assigned_equipment(unit,assignment.region,"rifle")<=0))
                    continue;
                bool competing=false;
                for(const auto& [other_id,other]:p5_crises_)
                    if(other_id!=id&&other.stage!=P5CrisisStage::settled&&
                       std::find(other.pledged_units.begin(),
                                 other.pledged_units.end(),unit)!=
                                 other.pledged_units.end())
                        competing=true;
                if(competing)continue;
                People eligible=0;
                for(const auto& [slice,count]:units_.at(unit)) {
                    if(count<=0||!people_.has_slice(slice))continue;
                    const auto& demo=people_.demographic(
                        people_.slice(slice).key.demographic);
                    if(demo.service!=phaseb::Service::active)continue;
                    People backing=0;
                    for(const auto& group:movement.groups) {
                        const auto it=membership_.find({slice,group});
                        if(it!=membership_.end())
                            backing=std::max(backing,
                                    std::min(count,it->second));
                    }
                    const auto att=p3_attitudes_.find(slice);
                    if(att!=p3_attitudes_.end()) {
                        const auto stance=att->second.issues.find(
                            movement.issue_domain);
                        if(stance!=att->second.issues.end()&&
                           stance->second.preference_ppm>0)
                            backing=std::max(backing,p4_scaled(
                               count,stance->second.preference_ppm,1'000'000));
                    }
                    eligible=phaseb::add_checked(eligible,backing);
                }
                if(eligible>0&&eligible>=active_strength(unit)/2) {
                    p5_pledge_army_unit(id,unit);
                    events_.push_back("p5-real-soldiers-joined-faction:"+unit);
                    break;
                }
            }
        }
        if(p.progress_ppm==1'000'000&&!crisis.pledged_units.empty()) {
            p5_begin_armed_conflict(id,day);
            events_.push_back("p5-revolution-clock-real-armed-trigger:"+id);
        } else if(p.progress_ppm==1'000'000) {
            p.progress_ppm=999'000; // cannot produce a fictitious rebel unit
            events_.push_back("p5-armed-clock-blocked-no-real-forces:"+id);
        }
    }
}
// Resolve only cases with *existing* legal or military evidence.
// Do not fabricate victories, sovereign entities or capital occupation.
inline void IntegratedWorld::p5_auto_resolve_crises(std::int64_t today) {
    require_stage({9});
    for(auto& [id,crisis]:p5_crises_) {
        if(crisis.stage==P5CrisisStage::settled)continue;
        const auto& movement=p3_movements_.at(crisis.movement);
        P5Settlement settlement;
        settlement.id="P5:auto:peace:"+id+":"+std::to_string(today);
        settlement.crisis_id=id;
        settlement.day=today;
        settlement.legal_basis="ai-verified-existing-evidence";
        if(crisis.stage==P5CrisisStage::deescalated&&
           p5_risk(crisis.movement).pressure_ppm<250'000) {
            settlement.kind=P5SettlementKind::crisis_recedes;
            p5_settle(std::move(settlement));
            continue;
        }
        bool effective_reform=false;
        for(const auto& [slot,law_id]:p2_active_base_) {
            const auto& law=p2_laws_.at(law_id);
            if(law.country==crisis.country&&
               law.domain==movement.issue_domain&&
               law.base_choice==movement.desired_choice) {
                effective_reform=true;break;
            }
        }
        if(effective_reform&&crisis.stage!=P5CrisisStage::armed_conflict) {
            settlement.kind=P5SettlementKind::legal_concession;
            p5_settle(std::move(settlement));
            continue;
        }
        if(crisis.stage!=P5CrisisStage::armed_conflict||
           (crisis.kind!=P5CrisisKind::revolution&&
            crisis.kind!=P5CrisisKind::coup)||
           !p5_capitals_.contains(crisis.country))continue;
        const auto& capital=p5_capitals_.at(crisis.country);
        const auto evidence=p5_control_evidence_.find(id);
        if(evidence==p5_control_evidence_.end()||
           !evidence->second.contains(capital)||
           military_control_.at(capital)!="P5:"+id||
           (crisis.kind!=P5CrisisKind::coup&&
            crisis.verified_engagements==0))continue;
        // Only a real living political character may become the new head.
        std::string successor;
        for(const auto& [person,character]:p1_characters_)
            if(character.country==crisis.country&&character.active&&
               !character.politically_exiled&&
               p1_character_available(person)&&
               person!=governments_.at(crisis.country).head_of_state&&
               (character.roles&static_cast<std::uint8_t>(
                   PoliticalRole::interest_group_leader))!=0) {
                successor=person;break;
            }
        if(successor.empty()||
           (p1_pending_.contains(today+1)&&
            p1_pending_.at(today+1).contains(crisis.country)))continue;
        bool verified=true;
        for(const auto& region:evidence->second)
            if(!sovereignty_.contains(region)||
               sovereignty_.at(region)!=crisis.country||
               !military_control_.contains(region)||
               military_control_.at(region)!="P5:"+id)
                verified=false;
        if(!verified)continue;
        settlement.kind=P5SettlementKind::cabinet_overthrow;
        settlement.successor_head=successor;
        p5_settle(std::move(settlement));
    }
}
} // namespace vic25::phasec
