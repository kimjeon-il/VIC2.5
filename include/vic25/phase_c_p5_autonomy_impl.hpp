#pragma once
// Autonomous internal conflict over the same authoritative army, population,
// inventory, law and sovereignty ledgers as manually executed P5 operations.
namespace vic25::phasec {
inline void IntegratedWorld::p5_autonomous_equip_factions() {
    require_stage({7});
    std::set<std::string> examined;
    for(const auto& [id,crisis]:p5_crises_) {
        (void)id;
        if(crisis.stage!=P5CrisisStage::armed_conflict||
           p1_player_countries_.contains(crisis.country))continue;
        std::set<std::string> eligible(crisis.pledged_units.begin(),
                                       crisis.pledged_units.end());
        for(const auto& [unit,assignment]:army_deployments_)
            if(assignment.country==crisis.country&&
               std::find(crisis.contested_regions.begin(),
                         crisis.contested_regions.end(),assignment.region)!=
                         crisis.contested_regions.end())
                eligible.insert(unit);
        for(const auto& unit:eligible) {
            if(!examined.insert(unit).second||
               !units_.contains(unit)||!army_deployments_.contains(unit)||
               active_strength(unit)<=0)continue;
            const auto& region=army_deployments_.at(unit).region;
            const auto physical=stock(unit,region,"rifle");
            const auto assigned=assigned_equipment(unit,region,"rifle");
            const auto desired=std::min<Quantity>(physical,
                                                  active_strength(unit));
            if(desired>assigned)
                assign_equipment(unit,region,"rifle",desired-assigned);
        }
    }
}
inline void IntegratedWorld::p5_autonomous_civil_battles(std::int64_t today) {
    require_stage({10});
    std::set<std::string> fought_today;
    for(auto& [id,crisis]:p5_crises_) {
        if(crisis.stage!=P5CrisisStage::armed_conflict||
           p1_player_countries_.contains(crisis.country))continue;
        const auto label="P5:"+id;
        for(const auto& rebel:crisis.pledged_units) {
            if(fought_today.contains(rebel)||
               !army_deployments_.contains(rebel)||
               !units_.contains(rebel)||active_strength(rebel)<=0)continue;
            const auto region=army_deployments_.at(rebel).region;
            if(!sovereignty_.contains(region)||
               sovereignty_.at(region)!=crisis.country||
               (military_control_.at(region)!=crisis.country&&
                military_control_.at(region)!=label)||
               assigned_equipment(rebel,region,"rifle")<=0||
               stock(rebel,region,"ammunition")<=0)continue;
            Id rebel_slice=0;
            for(const auto& [slice,n]:units_.at(rebel))
                if(n>0&&people_.has_slice(slice)&&
                   n==people_.slice(slice).count) {
                    rebel_slice=slice;break;
                }
            if(!rebel_slice)continue;
            for(const auto& [unit,assignment]:army_deployments_) {
                if(unit==rebel||fought_today.contains(unit)||
                   assignment.country!=crisis.country||
                   assignment.region!=region||
                   !units_.contains(unit)||active_strength(unit)<=0||
                   assigned_equipment(unit,region,"rifle")<=0||
                   stock(unit,region,"ammunition")<=0||
                   std::find(crisis.pledged_units.begin(),
                             crisis.pledged_units.end(),unit)!=
                       crisis.pledged_units.end())continue;
                bool pledged_elsewhere=false;
                for(const auto& [other_id,other]:p5_crises_)
                    if(other_id!=id&&other.stage!=P5CrisisStage::settled&&
                       std::find(other.pledged_units.begin(),
                                 other.pledged_units.end(),unit)!=
                           other.pledged_units.end())
                        pledged_elsewhere=true;
                if(pledged_elsewhere)continue;
                Id defender_slice=0;
                for(const auto& [slice,n]:units_.at(unit))
                    if(n>0&&people_.has_slice(slice)&&
                       n==people_.slice(slice).count) {
                        defender_slice=slice;break;
                    }
                if(!defender_slice)continue;
                CombatLoss government_loss{},rebel_loss{};
                government_loss.killed=1;government_loss.ammunition=1;
                rebel_loss.killed=1;rebel_loss.ammunition=1;
                // The native battle transaction performs all actual
                // munitions, casualty and military-slice accounting.
                p5_record_real_engagement(id,unit,defender_slice,rebel,
                    rebel_slice,region,government_loss,rebel_loss,today);
                fought_today.insert(unit);fought_today.insert(rebel);
                break;
            }
        }
        // An unopposed, surviving rebel deployment can occupy only its
        // actual sovereign region, not invent land, a state or a foreign war.
        for(const auto& rebel:crisis.pledged_units) {
            if(!army_deployments_.contains(rebel)||
               !units_.contains(rebel)||active_strength(rebel)<=0)continue;
            const auto region=army_deployments_.at(rebel).region;
            if(!sovereignty_.contains(region)||
               sovereignty_.at(region)!=crisis.country)continue;
            bool government_defender=false;
            for(const auto& [unit,assignment]:army_deployments_) {
                if(assignment.region!=region||
                   assignment.country!=crisis.country||
                   !units_.contains(unit)||active_strength(unit)<=0)continue;
                if(std::find(crisis.pledged_units.begin(),
                             crisis.pledged_units.end(),unit)==
                    crisis.pledged_units.end())government_defender=true;
            }
            if(government_defender)continue;
            if(military_control_.at(region)==crisis.country)
                occupy(region,label);
            if(military_control_.at(region)==label&&
               !p5_control_evidence_[id].contains(region))
                p5_confirm_actual_control(id,region,today);
        }
        // Government reoccupation likewise needs surviving real government
        // troops and no living pledged formation in this region.
        for(const auto& region:crisis.contested_regions) {
            if(!sovereignty_.contains(region)||
               sovereignty_.at(region)!=crisis.country||
               military_control_.at(region)!=label)continue;
            bool rebel_here=false,government_here=false;
            for(const auto& [unit,assignment]:army_deployments_) {
                if(assignment.region!=region||
                   assignment.country!=crisis.country||
                   !units_.contains(unit)||active_strength(unit)<=0)continue;
                const bool pledged=std::find(crisis.pledged_units.begin(),
                    crisis.pledged_units.end(),unit)!=crisis.pledged_units.end();
                if(pledged)rebel_here=true;
                else government_here=true;
            }
            if(!rebel_here&&government_here)
                occupy(region,crisis.country);
        }
    }
}
inline void IntegratedWorld::p5_autonomous_crisis_settlements(
        std::int64_t today) {
    require_stage({9});
    for(auto& [id,crisis]:p5_crises_) {
        if(crisis.stage==P5CrisisStage::settled||
           p1_player_countries_.contains(crisis.country)||
           !p3_movements_.contains(crisis.movement))continue;
        const auto& movement=p3_movements_.at(crisis.movement);
        bool law_enacted=false;
        for(const auto& [slot,law_id]:p2_active_base_) {
            (void)slot;
            const auto& law=p2_laws_.at(law_id);
            if(law.country==crisis.country&&
               law.domain==movement.issue_domain&&
               law.base_choice==movement.desired_choice)
                law_enacted=true;
        }
        if(law_enacted) {
            P5Settlement s;
            s.id="P5:auto:law:"+id;s.crisis_id=id;
            s.legal_basis="effective-law:"+movement.desired_choice;
            s.day=today;
            s.kind=crisis.kind==P5CrisisKind::autonomy?
                P5SettlementKind::autonomy_granted:
                P5SettlementKind::legal_concession;
            p5_settle(std::move(s));continue;
        }
        if(crisis.stage==P5CrisisStage::deescalated&&
           p5_risk(crisis.movement).pressure_ppm<250'000) {
            P5Settlement s;
            s.id="P5:auto:calm:"+id;s.crisis_id=id;
            s.legal_basis="observed-low-revolutionary-pressure";
            s.kind=P5SettlementKind::crisis_recedes;s.day=today;
            p5_settle(std::move(s));continue;
        }
        if(crisis.stage!=P5CrisisStage::armed_conflict)continue;
        const auto capital=p5_capitals_.find(crisis.country);
        if(capital!=p5_capitals_.end()&&
           (crisis.kind==P5CrisisKind::revolution||
            crisis.kind==P5CrisisKind::coup)&&
           p5_control_evidence_.contains(id)&&
           p5_control_evidence_.at(id).contains(capital->second)&&
           military_control_.at(capital->second)=="P5:"+id&&
           (crisis.kind==P5CrisisKind::coup||
            crisis.verified_engagements>0)) {
            const auto next=p1_pending_.find(today+1);
            if(next!=p1_pending_.end()&&
               next->second.contains(crisis.country)&&
               next->second.at(crisis.country).replace_head)continue;
            std::string successor;
            int best=-1'000'001;
            for(const auto& [person,politician]:p1_characters_) {
                if(!politician.active||politician.politically_exiled||
                   politician.country!=crisis.country||
                   !p1_character_available(person)||
                   politician.interest_group.empty()||
                   std::find(movement.groups.begin(),movement.groups.end(),
                             politician.interest_group)==movement.groups.end())
                    continue;
                if(politician.popularity_ppm>best) {
                    best=politician.popularity_ppm;successor=person;
                }
            }
            if(!successor.empty()&&
               successor!=governments_.at(crisis.country).head_of_state) {
                P5Settlement s;
                s.id="P5:auto:capital:"+id;s.crisis_id=id;
                s.legal_basis="actual-rebel-capital-occupation";
                s.successor_head=successor;s.day=today;
                s.kind=P5SettlementKind::cabinet_overthrow;
                p5_settle(std::move(s));continue;
            }
        }
        bool armed=false,occupied=false;
        for(const auto& unit:crisis.pledged_units) {
            const auto deployment=army_deployments_.find(unit);
            if(deployment==army_deployments_.end()||
               !units_.contains(unit)||active_strength(unit)<=0)continue;
            const auto& region=deployment->second.region;
            if(stock(unit,region,"ammunition")>0&&
               (stock(unit,region,"rifle")>0||
                assigned_equipment(unit,region,"rifle")>0))armed=true;
        }
        for(const auto& [region,holder]:military_control_) {
            (void)region;
            if(holder=="P5:"+id)occupied=true;
        }
        if(!armed&&!occupied) {
            P5Settlement s;
            s.id="P5:auto:disarmed:"+id;s.crisis_id=id;
            s.legal_basis="actual-rebel-formations-disarmed";
            s.kind=P5SettlementKind::rebel_disarmed;s.day=today;
            p5_settle(std::move(s));
        }
    }
}
} // namespace vic25::phasec
