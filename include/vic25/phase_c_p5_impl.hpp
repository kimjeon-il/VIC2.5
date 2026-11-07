#pragma once
// P5 political activity, warning, armed-unit evidence and constitutional
// outcomes. All combat, ammunition, POP and territorial control stay in C.
namespace vic25::phasec {

inline P5RiskView IntegratedWorld::p5_risk(const std::string& movement_id)const {
    const auto& movement=p3_movements_.at(movement_id);
    const auto observed=p3_movement_view(movement_id);
    P5RiskView risk;
    risk.movement=movement_id;
    risk.country=movement.country;
    risk.support_ppm=observed.support_ppm;
    risk.activism_ppm=observed.activism_ppm;
    risk.radical_ppm=observed.radicalism_ppm;
    const auto constitution=p1_constitutions_.find(movement.country);
    risk.institutional_exit=(constitution!=p1_constitutions_.end()&&
                            constitution->second.legislature!=
                                P1LegislatureKind::none);
    bool demand_fulfilled=false;
    for(const auto& [slot,id]:p2_active_base_) {
        (void)slot;
        const auto& law=p2_laws_.at(id);
        if(law.country==movement.country&&
           law.domain==movement.issue_domain&&
           law.base_choice==movement.desired_choice)
            demand_fulfilled=true;
    }
    if(!movement.active||demand_fulfilled) {
        risk.pressure_ppm=0;
        return risk;
    }
    std::int64_t pressure=static_cast<std::int64_t>(
        risk.support_ppm)/3+
        static_cast<std::int64_t>(risk.activism_ppm)/3+
        static_cast<std::int64_t>(risk.radical_ppm)/3;
    if(risk.institutional_exit)pressure=std::max<std::int64_t>(0,pressure-50'000);
    if(p1_constitutions_.contains(movement.country)&&
       p1_legitimacy(movement.country).score<40)
        pressure=std::min<std::int64_t>(1'000'000,pressure+50'000);
    risk.pressure_ppm=static_cast<std::int32_t>(
        std::clamp<std::int64_t>(pressure,0,1'000'000));
    risk.eligible_for_warning=risk.pressure_ppm>=550'000&&
        risk.support_ppm>=150'000&&risk.activism_ppm>=200'000;
    return risk;
}

inline bool IntegratedWorld::p5_needs_daily_tick()const {
    for(const auto& [id,movement]:p3_movements_) {
        (void)id;
        if(movement.active&&
           (movement.goal==P3MovementGoal::replace_government||
            movement.goal==P3MovementGoal::autonomy||
            movement.goal==P3MovementGoal::secession))
            return true;
    }
    for(const auto& [id,crisis]:p5_crises_) {
        (void)id;
        if(crisis.stage!=P5CrisisStage::settled)return true;
    }
    return false;
}

inline bool IntegratedWorld::p5_has_due_daily_strike(
        std::int64_t day)const {
    return !p5_active_daily_strikes_.empty()||
           p5_scheduled_daily_strikes_.contains(day);
}

inline void IntegratedWorld::phase_time_apply_p5_strikes(
        std::int64_t day) {
    require_stage({0});
    ensure(day>=0,"P5 daily labor action needs calendar evidence");
    p5_active_daily_strikes_.clear();
    const auto due=p5_scheduled_daily_strikes_.find(day);
    if(due==p5_scheduled_daily_strikes_.end())return;
    for(const auto& [slice,planned]:due->second) {
        if(!people_.has_slice(slice))continue;
        const auto real_job=people_.employed(slice);
        const auto monthly=strike_active_.find(slice);
        const People legacy=monthly==strike_active_.end()?0:monthly->second;
        if(legacy>=real_job)continue;
        const People actual=std::min(planned,real_job-legacy);
        if(actual>0)p5_active_daily_strikes_[slice]=actual;
    }
    p5_scheduled_daily_strikes_.erase(due);
    events_.push_back("p5-real-day-strike-allocation-effective");
}

inline void IntegratedWorld::p5_daily_advance(std::int64_t day) {
    require_stage({9});
    ensure(day>=0,"P5 crisis observation needs a real day");
    for(const auto& [id,movement]:p3_movements_) {
        if(!movement.active)continue;
        if(movement.goal!=P3MovementGoal::replace_government&&
           movement.goal!=P3MovementGoal::autonomy&&
           movement.goal!=P3MovementGoal::secession)continue;
        const auto observation=p5_risk(id);
        const std::string crisis_id="P5:"+id;
        const auto previous=p5_crises_.find(crisis_id);
        if(previous==p5_crises_.end()&&!observation.eligible_for_warning)
            continue;
        auto& crisis=p5_crises_[crisis_id];
        if(crisis.id.empty()) {
            crisis.id=crisis_id;
            crisis.country=movement.country;
            crisis.movement=id;
            crisis.kind=movement.goal==P3MovementGoal::secession?
                P5CrisisKind::secession:
                movement.goal==P3MovementGoal::autonomy?
                    P5CrisisKind::autonomy:P5CrisisKind::revolution;
            events_.push_back("p5-real-political-movement-crisis-observed:"+id);
        }
        if(crisis.stage==P5CrisisStage::settled)continue;
        ensure(day>crisis.last_checked_day,
               "P5 crisis tick replayed in one calendar day");
        if(crisis.stage!=P5CrisisStage::armed_conflict) {
            if(observation.eligible_for_warning) {
                crisis.pressure_days=crisis.last_checked_day==day-1?
                    std::min(365,crisis.pressure_days+1):1;
                crisis.calm_days=0;
                if(crisis.pressure_days>=7&&
                   crisis.stage!=P5CrisisStage::warning) {
                    crisis.stage=P5CrisisStage::warning;
                    events_.push_back("p5-political-crisis-warning:"+crisis.id);
                    // A newly credible social movement first takes a bounded
                    // peaceful, real-person petition route if legally possible.
                    // It does NOT conjure troops or start civil war on warning.
                    if(p2_rule(movement.country,movement.country,
                               P2ClauseKind::political_association_allowed)
                               .value_or(1)!=0) {
                        for(const auto& [key,member_count]:membership_) {
                            if(member_count<=0||
                               std::find(movement.groups.begin(),
                                         movement.groups.end(),key.second)==
                                   movement.groups.end()||
                               !people_.has_slice(key.first))continue;
                            const auto& population=people_.slice(key.first);
                            const auto& demo=people_.demographic(
                                population.key.demographic);
                            const auto sovereign=sovereignty_.find(demo.region);
                            if(sovereign==sovereignty_.end()||
                               sovereign->second!=movement.country||
                               (!movement.region.empty()&&
                                movement.region!=demo.region))continue;
                            const auto capacity=std::max<People>(0,
                                population.count-
                                participation_this_month_[key.first]);
                            const People n=std::min<People>(
                                {5,member_count,capacity,observation.support_ppm>0?
                                    p3_movement_view(id).potential_activists:0});
                            if(n<=0)continue;
                            P5CollectiveAction petition;
                            petition.id="P5:petition:"+crisis.id+":"+
                                std::to_string(day);
                            petition.movement=id;
                            petition.group=key.second;
                            petition.region=demo.region;
                            petition.kind=P5ActionKind::petition;
                            petition.slice=key.first;
                            petition.participants=n;
                            petition.day=day;
                            p5_perform_action(std::move(petition));
                            events_.push_back(
                                "p5-autonomous-peaceful-petition:"+id);
                            break;
                        }
                    }
                }
            } else {
                crisis.calm_days=crisis.last_checked_day==day-1?
                    std::min(365,crisis.calm_days+1):1;
                crisis.pressure_days=0;
                if(crisis.calm_days>=7&&
                   crisis.stage==P5CrisisStage::warning) {
                    crisis.stage=P5CrisisStage::deescalated;
                    events_.push_back("p5-crisis-deescalated:"+crisis.id);
                }
            }
        }
        crisis.last_checked_day=day;
    }
}

inline void IntegratedWorld::p5_perform_action(P5CollectiveAction action) {
    require_stage({9});
    ensure(!action.id.empty()&&action.day>=0&&
           p3_movements_.contains(action.movement)&&
           static_cast<unsigned>(action.kind)<=
               static_cast<unsigned>(P5ActionKind::policing)&&
           action.participants>=0&&action.strikers>=0&&
           action.strikers<=action.participants,
           "P5 action lacks a real movement, timestamp, or participants");
    ensure(!p5_action_ids_.contains(action.id),
           "P5 duplicate collective political action");
    const auto& movement=p3_movements_.at(action.movement);
    ensure(movement.active,"P5 inactive movement cannot launch fresh action");
    if(action.kind==P5ActionKind::policing) {
        ensure(p1_institutions_.contains(action.institution_id)&&
               p1_institutions_.at(action.institution_id).country==
                   movement.country&&
               p1_institutions_.at(action.institution_id).kind==
                   P1InstitutionKind::policing,
               "P5 policing lacks lawful national public institution");
        const auto capacity=p1_institution_capacity(action.institution_id);
        ensure(capacity.cash_backed&&action.participants>0&&
               action.participants<=capacity.usable_slots&&
               action.strikers==0,
               "P5 police response exceeds actually funded and staffed capacity");
        // The action is an institutional response, not synthetic arrests,
        // wounded protesters, or a free order-stability point.
    } else if(action.kind==P5ActionKind::negotiation||
              action.kind==P5ActionKind::concession) {
        ensure(!action.institution_id.empty()&&action.participants==0&&
               action.strikers==0,
               "P5 negotiation requires legal bill/law reference without fabricated crowds");
        if(action.kind==P5ActionKind::negotiation) {
            const auto it=p2_bills_.find(action.institution_id);
            ensure(it!=p2_bills_.end()&&it->second.country==movement.country,
                   "P5 negotiation requires an actually introduced national bill");
        } else {
            bool fulfilled=false;
            for(const auto& [slot,law_id]:p2_active_base_) {
                (void)slot;
                const auto& law=p2_laws_.at(law_id);
                if(law.country==movement.country&&
                   law.id==action.institution_id&&
                   law.domain==movement.issue_domain&&
                   law.base_choice==movement.desired_choice)
                    fulfilled=true;
            }
            ensure(fulfilled,
                   "P5 concession must refer to a real enacted law satisfying demand");
        }
    } else {
        ensure(action.participants>0&&people_.has_slice(action.slice)&&
               !action.group.empty()&&
               std::find(movement.groups.begin(),movement.groups.end(),
                         action.group)!=movement.groups.end()&&
               action.participants<=group_members(action.slice,action.group),
               "P5 petition/demonstration requires real group POP participation");
        const auto& dem=people_.demographic(
            people_.slice(action.slice).key.demographic);
        const auto sovereign=sovereignty_.find(dem.region);
        ensure(sovereign!=sovereignty_.end()&&
               sovereign->second==movement.country&&
               (movement.region.empty()||dem.region==movement.region)&&
               (action.region.empty()||action.region==dem.region),
               "P5 movement activity crosses real jurisdiction");
        const auto report=p3_movement_view(movement.id);
        ensure(action.participants<=report.potential_activists||
               action.kind==P5ActionKind::petition,
               "P5 actual street mobilization exceeds organized potential");
        if(action.kind==P5ActionKind::demonstration||
           action.kind==P5ActionKind::strike) {
            ensure(action.strikers==0||
                   action.kind==P5ActionKind::strike,
                   "P5 peaceful demonstration cannot fabricate strike allocation");
            if(action.kind==P5ActionKind::strike) {
                ensure(action.strikers>0&&
                       action.strikers<=people_.employed(action.slice)&&
                       p2_rule(movement.country,movement.country,
                               P2ClauseKind::peaceful_strike_allowed)
                               .value_or(1)!=0,
                       "P5 native strike requires contracted employees and a lawful strike right");
                const auto tomorrow=action.day+1;
                const auto old=p5_scheduled_daily_strikes_[tomorrow][action.slice];
                const auto sum=phaseb::add_checked(old,action.strikers);
                ensure(sum<=people_.employed(action.slice),
                       "P5 repeated next-day strikes overbook actual employees");
                p5_scheduled_daily_strikes_[tomorrow][action.slice]=sum;
            }
            // Attendance uses the existing C real-POP participant journal,
            // while native strike hours are deducted only tomorrow, not on
            // a production stage that was already committed earlier today.
            protest(action.slice,action.group,action.participants,0);
        } else {
            ensure(action.strikers==0&&
                   participation_this_month_[action.slice]+
                       action.participants<=people_.slice(action.slice).count,
                   "P5 petition overallocates actual available persons");
            participation_this_month_[action.slice]+=action.participants;
        }
    }
    const auto stable_id=action.id;
    p5_actions_.push_back(std::move(action));
    p5_action_ids_.insert(stable_id);
    events_.push_back("p5-actual-political-collective-action");
}

inline void IntegratedWorld::p5_seed_capital(
        const std::string& country,const std::string& region) {
    ensure(stage_==-1&&commit_id_==0&&
           governments_.contains(country)&&
           sovereignty_.contains(region)&&
           sovereignty_.at(region)==country&&
           !p5_capitals_.contains(country),
           "P5 political capital must bind a sovereign region before start");
    p5_capitals_.emplace(country,region);
}

inline void IntegratedWorld::p5_pledge_army_unit(
        const std::string& crisis_id,const std::string& unit) {
    require_stage({9});
    auto& crisis=p5_crises_.at(crisis_id);
    ensure(crisis.stage==P5CrisisStage::warning&&
           army_deployments_.contains(unit)&&
           army_deployments_.at(unit).country==crisis.country&&
           active_strength(unit)>0,
           "P5 pledged faction must already command a deployed living army");
    const auto& region=army_deployments_.at(unit).region;
    ensure(stock(unit,region,"ammunition")>0&&
           (stock(unit,region,"rifle")>0||
            assigned_equipment(unit,region,"rifle")>0),
           "P5 armed faction requires actual stock and equipment");
    for(const auto& [id,other]:p5_crises_) {
        if(id==crisis_id||other.stage==P5CrisisStage::settled)continue;
        ensure(std::find(other.pledged_units.begin(),other.pledged_units.end(),
                         unit)==other.pledged_units.end(),
               "P5 same real army cannot pledge to competing factions");
    }
    for(const auto& [campaign_id,campaign]:p5_guerrilla_campaigns_) {
        (void)campaign_id;
        if(campaign.stage==P5GuerrillaStage::resolved||
           campaign.stage==P5GuerrillaStage::ceasefire||
           campaign.movement==crisis.movement)continue;
        ensure(std::find(campaign.units.begin(),campaign.units.end(),unit)==
                   campaign.units.end(),
               "P5 military unit already serves an unrelated guerrilla faction");
    }
    ensure(std::find(crisis.pledged_units.begin(),
                     crisis.pledged_units.end(),unit)==
                     crisis.pledged_units.end(),
           "P5 same unit pledged twice");
    crisis.pledged_units.push_back(unit);
    events_.push_back("p5-existing-military-unit-pledged:"+crisis_id);
}

inline void IntegratedWorld::p5_designate_coup(const std::string& crisis_id) {
    require_stage({9});
    auto& crisis=p5_crises_.at(crisis_id);
    ensure(crisis.stage==P5CrisisStage::warning&&
           crisis.kind==P5CrisisKind::revolution&&
           !crisis.pledged_units.empty(),
           "P5 coup needs organized internal faction and real pledged military");
    crisis.kind=P5CrisisKind::coup;
}

inline void IntegratedWorld::p5_begin_armed_conflict(
        const std::string& crisis_id,std::int64_t today) {
    require_stage({9});
    auto& crisis=p5_crises_.at(crisis_id);
    ensure(today>=0&&crisis.stage==P5CrisisStage::warning&&
           crisis.last_checked_day<=today&&
           !crisis.pledged_units.empty(),
           "P5 armed action requires warning and actual pledged forces");
    for(const auto& unit:crisis.pledged_units) {
        const auto& army=army_deployments_.at(unit);
        ensure(active_strength(unit)>0&&
               stock(unit,army.region,"ammunition")>0&&
               (stock(unit,army.region,"rifle")>0||
                assigned_equipment(unit,army.region,"rifle")>0),
               "P5 real army lost weapons, ammunition or deployed troops");
        if(std::find(crisis.contested_regions.begin(),
                     crisis.contested_regions.end(),army.region)==
                     crisis.contested_regions.end())
            crisis.contested_regions.push_back(army.region);
    }
    crisis.stage=P5CrisisStage::armed_conflict;
    crisis.armed_day=today;
    events_.push_back("p5-verified-armed-political-crisis:"+crisis_id);
}

inline void IntegratedWorld::p5_record_real_engagement(
        const std::string& crisis_id,
        const std::string& government_unit,Id government_slice,
        const std::string& rebel_unit,Id rebel_slice,
        const std::string& region,CombatLoss government_losses,
        CombatLoss rebel_losses,std::int64_t today) {
    require_stage({10});
    auto& crisis=p5_crises_.at(crisis_id);
    ensure(crisis.stage==P5CrisisStage::armed_conflict&&
           today>=crisis.armed_day&&
           army_deployments_.contains(government_unit)&&
           army_deployments_.contains(rebel_unit)&&
           government_unit!=rebel_unit&&
           army_deployments_.at(government_unit).country==crisis.country&&
           army_deployments_.at(rebel_unit).country==crisis.country&&
           army_deployments_.at(government_unit).region==region&&
           army_deployments_.at(rebel_unit).region==region,
           "P5 civil clash requires two actual deployed armies at same region");
    const auto rebel=std::find(crisis.pledged_units.begin(),
                               crisis.pledged_units.end(),rebel_unit);
    ensure(rebel!=crisis.pledged_units.end()&&
           std::find(crisis.pledged_units.begin(),
                     crisis.pledged_units.end(),government_unit)==
                     crisis.pledged_units.end(),
           "P5 combat must oppose a real pledged faction to government troops");
    for(const auto& [id,other]:p5_crises_) {
        if(id==crisis_id||other.stage==P5CrisisStage::settled)continue;
        ensure(std::find(other.pledged_units.begin(),
                         other.pledged_units.end(),government_unit)==
                         other.pledged_units.end(),
               "P5 government army assigned to another active rebellion");
    }
    ensure(government_losses.ammunition>0&&rebel_losses.ammunition>0&&
           stock(government_unit,region,"ammunition")>=
               government_losses.ammunition&&
           stock(rebel_unit,region,"ammunition")>=rebel_losses.ammunition&&
           assigned_equipment(government_unit,region,"rifle")>0&&
           assigned_equipment(rebel_unit,region,"rifle")>0,
           "P5 combat needs existing arms and consumed real ammunition");
    // The existing C/Phase B battle routine is authoritative for losses,
    // POP deaths/injuries and inventory; no proxy civil-war headcount.
    battle(government_unit,government_slice,region,government_losses);
    battle(rebel_unit,rebel_slice,region,rebel_losses);
    ensure(crisis.verified_engagements<
               std::numeric_limits<std::int32_t>::max(),
           "P5 combat-event counter overflow");
    ++crisis.verified_engagements;
    events_.push_back("p5-actual-civil-conflict-engagement:"+crisis_id);
}

inline void IntegratedWorld::p5_confirm_actual_control(
        const std::string& crisis_id,const std::string& region,
        std::int64_t today) {
    require_stage({10});
    auto& crisis=p5_crises_.at(crisis_id);
    const auto label="P5:"+crisis.id;
    ensure(today>=crisis.armed_day&&
           crisis.stage==P5CrisisStage::armed_conflict&&
           sovereignty_.contains(region)&&
           sovereignty_.at(region)==crisis.country&&
           military_control_.at(region)==label,
           "P5 cannot claim an unoccupied or unrelated sovereign region");
    bool real_army=false;
    for(const auto& unit:crisis.pledged_units)
        if(army_deployments_.contains(unit)&&
           army_deployments_.at(unit).region==region&&
           active_strength(unit)>0)real_army=true;
    ensure(real_army,
           "P5 contested military control needs a deployed living rebel unit");
    p5_control_evidence_[crisis_id].insert(region);
    events_.push_back("p5-real-rebel-military-control-confirmed:"+region);
}

inline void IntegratedWorld::phase_time_apply_p5_settlement(
        std::int64_t today) {
    require_stage({0});
    const auto due=p5_scheduled_control_reconciliation_.find(today);
    if(due==p5_scheduled_control_reconciliation_.end())return;
    for(const auto& agreement:due->second) {
        ensure(governments_.at(agreement.country).head_of_state==
                   agreement.successor_head,
               "P5 legitimate new commander must already be seated by P1");
        for(const auto& region:agreement.verified_regions) {
            ensure(sovereignty_.at(region)==agreement.country&&
                   military_control_.at(region)==
                       "P5:"+agreement.crisis_id,
                   "P5 may normalize only previously rebel-controlled real territory");
            military_control_[region]=agreement.country;
        }
        events_.push_back(
            "p5-real-military-command-unified-after-settlement:"+
            agreement.crisis_id);
    }
    p5_scheduled_control_reconciliation_.erase(due);
}

inline void IntegratedWorld::p5_settle(P5Settlement settlement) {
    require_stage({9});
    ensure(!settlement.id.empty()&&!settlement.legal_basis.empty()&&
           settlement.day>=0&&
           p5_crises_.contains(settlement.crisis_id)&&
           static_cast<unsigned>(settlement.kind)<=
               static_cast<unsigned>(P5SettlementKind::independence_recognized),
           "P5 settlement requires identifiable actual political crisis");
    for(const auto& history:p5_settlements_)
        ensure(history.id!=settlement.id,
               "P5 duplicate negotiated/verified political settlement");
    auto& crisis=p5_crises_.at(settlement.crisis_id);
    ensure(crisis.stage!=P5CrisisStage::settled,
           "P5 cannot settle the same crisis twice");
    const auto& movement=p3_movements_.at(crisis.movement);
    if(settlement.kind==P5SettlementKind::legal_concession||
       settlement.kind==P5SettlementKind::autonomy_granted) {
        bool actual_law=false;
        for(const auto& [slot,id]:p2_active_base_) {
            (void)slot;
            const auto& law=p2_laws_.at(id);
            if(law.country==crisis.country&&
               law.domain==movement.issue_domain&&
               law.base_choice==movement.desired_choice)
                actual_law=true;
        }
        ensure(actual_law,
               "P5 concession must be backed by a law that actually took effect");
    } else if(settlement.kind==P5SettlementKind::crisis_recedes) {
        ensure(crisis.stage==P5CrisisStage::deescalated||
               p5_risk(crisis.movement).pressure_ppm<250'000,
               "P5 unsupported assertion that political movement faded");
    } else if(settlement.kind==P5SettlementKind::cabinet_overthrow) {
        ensure((crisis.kind==P5CrisisKind::revolution||
                crisis.kind==P5CrisisKind::coup)&&
               crisis.stage==P5CrisisStage::armed_conflict&&
               (crisis.kind==P5CrisisKind::coup||
                crisis.verified_engagements>0)&&
               p5_capitals_.contains(crisis.country)&&
               p5_control_evidence_[crisis.id].contains(
                   p5_capitals_.at(crisis.country))&&
               !settlement.successor_head.empty(),
               "P5 regime overthrow requires real armed control of actual capital");
        for(const auto& region:p5_control_evidence_[crisis.id])
            ensure(military_control_.at(region)=="P5:"+crisis.id,
                   "P5 outdated armed control evidence cannot replace government");
        auto& change=p1_pending_[settlement.day+1][crisis.country];
        ensure(!change.replace_head,
               "P5 cannot override already scheduled lawful office succession");
        change.country=crisis.country;
        change.new_head=settlement.successor_head;
        change.replace_head=true;
        change.legal_basis="p5-extraconstitutional:"+settlement.legal_basis;
        change.coalition=p1_coalitions_.at(crisis.country);
        const auto& evidence=p5_control_evidence_.at(crisis.id);
        p5_scheduled_control_reconciliation_[settlement.day+1].push_back({
            crisis.id,crisis.country,settlement.successor_head,
            {evidence.begin(),evidence.end()}
        });
    } else {
        ensure(crisis.kind==P5CrisisKind::secession&&
               !settlement.region.empty()&&
               !settlement.new_sovereign.empty()&&
               settlement.new_sovereign!=crisis.country&&
               sovereignty_.contains(settlement.region)&&
               sovereignty_.at(settlement.region)==settlement.new_sovereign&&
               governments_.contains(settlement.new_sovereign),
               "P5 independence must follow a real Phase E sovereignty transition");
    }
    crisis.stage=P5CrisisStage::settled;
    crisis.settled_day=settlement.day;
    p3_movements_.at(crisis.movement).active=false;
    p5_settlements_.push_back(std::move(settlement));
    events_.push_back("p5-verified-political-settlement:"+crisis.id);
}

} // namespace vic25::phasec
