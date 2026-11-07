#pragma once
// P5-G: regional guerrilla politics share existing C/Phase B actual manpower,
// inventory, services, government, sovereignty and army-deployment registries.
namespace vic25::phasec {

inline P5GuerrillaRegionView IntegratedWorld::p5_guerrilla_region_view(
        const std::string& campaign_id,const std::string& region)const {
    const auto& campaign=p5_guerrilla_campaigns_.at(campaign_id);
    ensure(std::find(campaign.regions.begin(),campaign.regions.end(),region)!=
               campaign.regions.end(),
           "P5-G region not attached to existing guerrilla campaign");
    const auto& movement=p3_movements_.at(campaign.movement);
    P5GuerrillaRegionView result;
    result.campaign=campaign_id;
    result.region=region;
    result.sovereign=sovereignty_.at(region);
    result.military_controller=military_control_.at(region);
    result.access_disruption_ppm=p5_guerrilla_disruption_ppm(region);

    // Explicit regional political opinions and membership are projections
    // onto living POPs, not additional recruitment or combat personnel.
    std::map<Id,People> support;
    for(const auto& [id,row]:people_.slices()) {
        const auto& dem=people_.demographic(row.key.demographic);
        if(dem.region!=region)continue;
        result.population=phaseb::add_checked(result.population,row.count);
        const auto attitude=p3_attitudes_.find(id);
        if(attitude==p3_attitudes_.end())continue;
        const auto preference=attitude->second.issues.find(
            movement.issue_domain);
        if(preference==attitude->second.issues.end()||
           preference->second.preference_ppm<=0)continue;
        const People interested=p4_scaled(row.count,
            preference->second.preference_ppm,1'000'000);
        const People selected=p4_scaled(interested,
            preference->second.importance_ppm,1'000'000);
        if(selected>0)support.emplace(id,selected);
    }
    for(const auto& [key,n]:membership_) {
        if(n<=0||!people_.has_slice(key.first)||
           std::find(movement.groups.begin(),movement.groups.end(),
                     key.second)==movement.groups.end())continue;
        const auto& dem=people_.demographic(
            people_.slice(key.first).key.demographic);
        if(dem.region!=region)continue;
        auto& existing=support[key.first];
        existing=std::max(existing,std::min(n,
                               people_.slice(key.first).count));
    }
    for(const auto& [id,n]:support) {
        (void)id;
        result.supporters=phaseb::add_checked(result.supporters,n);
    }
    if(result.population>0)
        result.support_ppm=p3_fraction(result.supporters,result.population);

    // Political irregulars must be actual deployed C units. These queries
    // do not create armies, food, money, guns or rival sovereignty.
    for(const auto& unit:campaign.units) {
        const auto army=army_deployments_.find(unit);
        if(army==army_deployments_.end()||army->second.region!=region)continue;
        result.irregular_strength=phaseb::add_checked(
            result.irregular_strength,active_strength(unit));
        result.available_ammunition=phaseb::add_checked(
            result.available_ammunition,stock(unit,region,"ammunition"));
    }
    for(const auto& [unit,army]:army_deployments_) {
        if(army.region!=region||army.country!=campaign.country||
           std::find(campaign.units.begin(),campaign.units.end(),unit)!=
               campaign.units.end())continue;
        result.government_strength=phaseb::add_checked(
            result.government_strength,active_strength(unit));
    }
    for(const auto& [id,institution]:p1_institutions_) {
        if(institution.country!=campaign.country||
           institution.kind!=P1InstitutionKind::policing||
           !public_institutions_.contains(id)||
           public_institutions_.at(id).region!=region)continue;
        result.government_strength=phaseb::add_checked(
            result.government_strength,
            p1_institution_capacity(id).usable_slots);
    }
    const People total_strength=phaseb::add_checked(
        result.government_strength,result.irregular_strength);
    if(total_strength>0)
        result.security_ppm=p3_fraction(result.government_strength,
                                        total_strength);
    return result;
}

inline void IntegratedWorld::p5_begin_guerrilla(
        P5GuerrillaCampaign campaign) {
    ensure((stage_==-1&&commit_id_==0)||stage_==9,
           "P5-G campaign creation needs initial scenario or political stage");
    ensure(!campaign.id.empty()&&!campaign.country.empty()&&
           !campaign.movement.empty()&&campaign.started_day>=0&&
           campaign.ended_day==-1&&campaign.regions.size()>0&&
           campaign.regions.size()<=1024&&campaign.units.size()<=1024&&
           !p5_guerrilla_campaigns_.contains(campaign.id)&&
           p3_movements_.contains(campaign.movement)&&
           p3_movements_.at(campaign.movement).active&&
           p3_movements_.at(campaign.movement).country==campaign.country&&
           governments_.contains(campaign.country)&&
           campaign.crisis_id.empty(),
           "P5-G must follow an existing active political movement");
    const auto& movement=p3_movements_.at(campaign.movement);
    std::set<std::string> unique_regions,unique_units;
    bool political_base=false;
    for(const auto& region:campaign.regions) {
        ensure(unique_regions.insert(region).second&&
               sovereignty_.contains(region)&&
               sovereignty_.at(region)==campaign.country&&
               (movement.region.empty()||movement.region==region),
               "P5-G region must be a unique real sovereign locality");
        for(const auto& [key,n]:membership_) {
            if(n<=0||!people_.has_slice(key.first)||
               std::find(movement.groups.begin(),movement.groups.end(),
                         key.second)==movement.groups.end())continue;
            const auto& dem=people_.demographic(
                people_.slice(key.first).key.demographic);
            if(dem.region==region)political_base=true;
        }
        for(const auto& [id,attitude]:p3_attitudes_) {
            if(!people_.has_slice(id))continue;
            const auto& dem=people_.demographic(
                people_.slice(id).key.demographic);
            if(dem.region!=region)continue;
            const auto stance=attitude.issues.find(movement.issue_domain);
            if(stance!=attitude.issues.end()&&
               stance->second.preference_ppm>0&&
               stance->second.importance_ppm>0)
                political_base=true;
        }
    }
    ensure(political_base,"P5-G region has no actual living political base");
    for(const auto& unit:campaign.units) {
        ensure(unique_units.insert(unit).second&&
               army_deployments_.contains(unit)&&
               army_deployments_.at(unit).country==campaign.country&&
               unique_regions.contains(army_deployments_.at(unit).region)&&
               active_strength(unit)>0,
               "P5-G cannot manufacture a regional irregular unit");
        const auto& region=army_deployments_.at(unit).region;
        ensure(stock(unit,region,"ammunition")>0&&
               (stock(unit,region,"rifle")>0||
                assigned_equipment(unit,region,"rifle")>0),
               "P5-G active militia needs real ammunition and rifles");
        for(const auto& [other_id,other]:p5_guerrilla_campaigns_) {
            (void)other_id;
            if(other.stage==P5GuerrillaStage::resolved||
               other.stage==P5GuerrillaStage::ceasefire)continue;
            ensure(std::find(other.units.begin(),other.units.end(),unit)==
                       other.units.end(),
                   "P5-G real unit already belongs to an active campaign");
        }
        for(const auto& [id,crisis]:p5_crises_) {
            (void)id;
            if(crisis.stage==P5CrisisStage::settled||
               crisis.movement==campaign.movement)continue;
            ensure(std::find(crisis.pledged_units.begin(),
                             crisis.pledged_units.end(),unit)==
                       crisis.pledged_units.end(),
                   "P5-G real unit pledged to an unrelated civil conflict");
        }
    }
    campaign.stage=campaign.units.empty()?
        P5GuerrillaStage::organizing:P5GuerrillaStage::active;
    campaign.next_review_day=campaign.started_day+7;
    const auto id=campaign.id,review=campaign.next_review_day;
    p5_guerrilla_campaigns_.emplace(id,std::move(campaign));
    p5_guerrilla_review_due_[review].insert(id);
    events_.push_back("p5-guerrilla-regional-journal-created:"+id);
}

inline void IntegratedWorld::p5_guerrilla_extend(
        const std::string& id,const std::string& region,
        const std::string& unit,std::int64_t today) {
    require_stage({9});
    auto& campaign=p5_guerrilla_campaigns_.at(id);
    ensure(today>=campaign.started_day&&
           (campaign.stage==P5GuerrillaStage::active||
            campaign.stage==P5GuerrillaStage::organizing||
            campaign.stage==P5GuerrillaStage::supply_exhausted)&&
           !unit.empty()&&army_deployments_.contains(unit)&&
           army_deployments_.at(unit).country==campaign.country&&
           army_deployments_.at(unit).region==region&&
           sovereignty_.at(region)==campaign.country&&
           active_strength(unit)>0&&
           stock(unit,region,"ammunition")>0&&
           (stock(unit,region,"rifle")>0||
            assigned_equipment(unit,region,"rifle")>0)&&
           std::find(campaign.units.begin(),campaign.units.end(),unit)==
               campaign.units.end(),
           "P5-G new area requires a real deployed armed unit");
    const auto& movement=p3_movements_.at(campaign.movement);
    ensure(movement.region.empty()||movement.region==region,
           "P5-G movement cannot expand outside its documented jurisdiction");
    for(const auto& [other_id,other]:p5_guerrilla_campaigns_) {
        if(other_id==id||other.stage==P5GuerrillaStage::resolved||
           other.stage==P5GuerrillaStage::ceasefire)continue;
        ensure(std::find(other.units.begin(),other.units.end(),unit)==
                   other.units.end(),
               "P5-G independent campaigns cannot share real armed personnel");
    }
    for(const auto& [other_id,crisis]:p5_crises_) {
        (void)other_id;
        if(crisis.stage==P5CrisisStage::settled||
           crisis.movement==campaign.movement)continue;
        ensure(std::find(crisis.pledged_units.begin(),
                         crisis.pledged_units.end(),unit)==
                   crisis.pledged_units.end(),
               "P5-G unit already pledged to different armed faction");
    }
    const bool new_region=std::find(campaign.regions.begin(),
        campaign.regions.end(),region)==campaign.regions.end();
    if(new_region)campaign.regions.push_back(region);
    campaign.units.push_back(unit);
    ensure(p5_guerrilla_region_view(id,region).supporters>0,
           "P5-G cannot expand without a genuine regional movement base");
    campaign.stage=P5GuerrillaStage::active;
    P5GuerrillaIncident event;
    event.id="P5-G:expand:"+id+":"+unit+":"+std::to_string(today);
    ensure(p5_guerrilla_incident_ids_.insert(event.id).second,
           "P5-G duplicate regional expansion");
    event.campaign=id;event.region=region;event.irregular_unit=unit;
    event.kind=P5GuerrillaIncidentKind::region_expansion;event.day=today;
    p5_guerrilla_incidents_.push_back(std::move(event));
    events_.push_back("p5-guerrilla-really-armed-region-expanded:"+region);
}

inline int IntegratedWorld::p5_guerrilla_disruption_ppm(
        const std::string& region)const {
    const auto it=p5_guerrilla_region_penalty_ppm_.find(region);
    return it==p5_guerrilla_region_penalty_ppm_.end()?0:it->second;
}

inline bool IntegratedWorld::p5_guerrilla_has_calendar_work(
        std::int64_t today)const {
    return p5_guerrilla_review_due_.contains(today)||
           p5_guerrilla_effect_due_.contains(today)||
           !p5_guerrilla_effect_active_.empty();
}

inline void IntegratedWorld::phase_time_apply_p5_guerrilla_access(
        std::int64_t today) {
    require_stage({0});
    ensure(today>=0,"P5-G access disruption needs real calendar date");
    for(auto it=p5_guerrilla_effect_active_.begin();
        it!=p5_guerrilla_effect_active_.end();) {
        auto& effects=it->second;
        std::erase_if(effects,[&](const P5GuerrillaAccessEffect& effect){
            return effect.through_day<today;
        });
        if(effects.empty())it=p5_guerrilla_effect_active_.erase(it);
        else ++it;
    }
    const auto due=p5_guerrilla_effect_due_.find(today);
    if(due!=p5_guerrilla_effect_due_.end()) {
        for(const auto& effect:due->second) {
            ensure(effect.from_day==today&&effect.through_day>=today&&
                   p5_guerrilla_campaigns_.contains(effect.campaign)&&
                   sovereignty_.contains(effect.region),
                   "P5-G scheduled regional access effect has no real source");
            p5_guerrilla_effect_active_[effect.region].push_back(effect);
        }
        p5_guerrilla_effect_due_.erase(due);
    }
    p5_guerrilla_region_penalty_ppm_.clear();
    for(const auto& [region,effects]:p5_guerrilla_effect_active_) {
        std::int64_t loss=0;
        for(const auto& effect:effects)
            loss=std::min<std::int64_t>(500'000,
                 loss+effect.penalty_ppm);
        if(loss>0)
            p5_guerrilla_region_penalty_ppm_[region]=
                static_cast<std::int32_t>(loss);
    }
}

inline void IntegratedWorld::p5_guerrilla_daily_review(
        std::int64_t today) {
    require_stage({9});
    const auto it=p5_guerrilla_review_due_.find(today);
    if(it==p5_guerrilla_review_due_.end())return;
    const auto ids=it->second;
    p5_guerrilla_review_due_.erase(it);
    for(const auto& id:ids) {
        auto& campaign=p5_guerrilla_campaigns_.at(id);
        if(campaign.stage==P5GuerrillaStage::conventional||
           campaign.stage==P5GuerrillaStage::ceasefire||
           campaign.stage==P5GuerrillaStage::resolved)continue;
        bool real_supply=false;
        for(const auto& unit:campaign.units) {
            const auto deployment=army_deployments_.find(unit);
            if(deployment==army_deployments_.end()||
               deployment->second.country!=campaign.country||
               std::find(campaign.regions.begin(),campaign.regions.end(),
                         deployment->second.region)==campaign.regions.end()||
               active_strength(unit)<=0)continue;
            const auto& region=deployment->second.region;
            if(stock(unit,region,"ammunition")>0&&
               (stock(unit,region,"rifle")>0||
                assigned_equipment(unit,region,"rifle")>0))
                real_supply=true;
        }
        const auto next=campaign.units.empty()?
            P5GuerrillaStage::organizing:
            real_supply?P5GuerrillaStage::active:
                        P5GuerrillaStage::supply_exhausted;
        if(next!=campaign.stage) {
            campaign.stage=next;
            events_.push_back("p5-guerrilla-supply-condition-changed:"+id);
        }
        ensure(today<=std::numeric_limits<std::int64_t>::max()-7,
               "P5-G calendar exhausted");
        campaign.next_review_day=today+7;
        p5_guerrilla_review_due_[campaign.next_review_day].insert(id);
    }
}

} // namespace vic25::phasec
