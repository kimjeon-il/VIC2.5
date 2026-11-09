#pragma once
// Military power projection reads actual assigned soldiers and registered
// commissioned hulls. A hull is backed by an existing military unit and
// real POP crew; no new manpower, ammunition or financial ledger exists.
namespace vic25::phasec {
inline void IntegratedWorld::p1_register_army_rating(P1ArmyRating rating) {
    ensure((stage_==-1&&commit_id_==0)||stage_==9,
           "P1 verified army rating requires setup or military command stage");
    ensure(!rating.unit.empty()&&!rating.source.empty()&&
           rating.power_per_person_ppm>0&&
           rating.power_per_person_ppm<=10'000'000&&
           army_deployments_.contains(rating.unit),
           "P1 military rating requires real deployed unit");
    p1_army_ratings_[rating.unit]=std::move(rating);
}
inline void IntegratedWorld::p1_register_ship(P1NavalHull hull) {
    ensure((stage_==-1&&commit_id_==0)||stage_==9,
           "P1 ship commissioning requires setup or lawful military stage");
    ensure(!hull.id.empty()&&!hull.unit.empty()&&!hull.type.empty()&&
           !hull.source.empty()&&hull.projection>0&&
           hull.projection<=1'000'000&&hull.max_hp>0&&
           hull.max_hp<=1'000'000&&hull.current_hp>0&&
           hull.current_hp<=hull.max_hp&&hull.required_crew>0&&
           army_deployments_.contains(hull.unit)&&units_.contains(hull.unit)&&
           !p1_naval_hulls_.contains(hull.id),
           "P1 ship must have real military backing and hull data");
    People assigned_crew=0;
    for(const auto& [slice,count]:units_.at(hull.unit)) {
        ensure(people_.has_slice(slice)&&count>=0,
               "P1 ship has missing crew population");
        const auto& demographic=people_.demographic(
            people_.slice(slice).key.demographic);
        ensure(demographic.service==phaseb::Service::active,
               "P1 civilian cannot be naval crew");
        assigned_crew=phaseb::add_checked(assigned_crew,count);
    }
    ensure(assigned_crew>=hull.required_crew,
           "P1 commissioned vessel has no backed crew");
    for(const auto& [id,existing]:p1_naval_hulls_) {
        (void)id;
        ensure(existing.unit!=hull.unit,
               "P1 military unit cannot crew duplicate hulls");
    }
    p1_naval_hulls_.emplace(hull.id,std::move(hull));
}
inline void IntegratedWorld::p1_set_navy_inventory_verified(
        const std::string& country,bool verified) {
    ensure((stage_==-1&&commit_id_==0)||stage_==9,
           "P1 fleet inventory must be entered at setup/political stage");
    ensure(governments_.contains(country),"P1 verified fleet needs country");
    if(verified)p1_verified_navies_.insert(country);
    else p1_verified_navies_.erase(country);
}
inline void IntegratedWorld::p1_retire_ship(const std::string& id) {
    require_stage({9});
    ensure(p1_naval_hulls_.contains(id)&&
           p1_naval_hulls_.at(id).commissioned,
           "P1 retired vessel must actually be commissioned");
    p1_naval_hulls_.at(id).commissioned=false;
}
inline P1MilitaryPowerView IntegratedWorld::p1_military_power(
        const std::string& country)const {
    ensure(governments_.contains(country),
           "P1 military prestige requires an existing country");
    P1MilitaryPowerView result;
    result.army_complete=true;
    result.navy_complete=p1_verified_navies_.contains(country);
    std::set<std::string> crew_units;
    for(const auto& [id,hull]:p1_naval_hulls_) {
        (void)id;
        crew_units.insert(hull.unit);
        if(!hull.commissioned)continue;
        auto deployment=army_deployments_.find(hull.unit);
        if(deployment==army_deployments_.end()||
           deployment->second.country!=country)continue;
        result.navy_observed=true;
        People crew=0;
        if(auto unit=units_.find(hull.unit);unit!=units_.end())
            for(const auto& [slice,count]:unit->second) {
                if(!people_.has_slice(slice))continue;
                const auto& dem=people_.demographic(
                    people_.slice(slice).key.demographic);
                if(dem.service==phaseb::Service::active)
                    crew=phaseb::add_checked(crew,count);
            }
        if(crew<=0||hull.current_hp<=0)continue;
        // Victoria 3 1.15: projection * maxHP /1150 * .0077.
        // Only crew-backed commissioned hulls count. Fractional crew
        // staffing scales available naval power (a VIC2.5 constraint).
        const auto power=phaseb::mul_checked(
            phaseb::mul_checked(hull.projection,hull.max_hp),77)/
            (1150*10'000);
        const auto real_crew=std::min(crew,hull.required_crew);
        result.navy=phaseb::add_checked(result.navy,
            phaseb::mul_checked(power,real_crew)/hull.required_crew);
        ++result.commissioned_ships;
    }
    for(const auto& [id,deployment]:army_deployments_) {
        if(deployment.country!=country||crew_units.contains(id))continue;
        const auto rating=p1_army_ratings_.find(id);
        if(rating==p1_army_ratings_.end()) {
            result.army_complete=false;
            continue;
        }
        if(auto unit=units_.find(id);unit!=units_.end()) {
            People people=0;
            for(const auto& [slice,count]:unit->second) {
                if(!people_.has_slice(slice))continue;
                const auto& dem=people_.demographic(
                    people_.slice(slice).key.demographic);
                if(dem.service==phaseb::Service::active)
                    people=phaseb::add_checked(people,count);
            }
            // 1.15 army prestige coefficient is 0.03 per verified point.
            const auto power=phaseb::mul_checked(
                people,rating->second.power_per_person_ppm)/1'000'000;
            result.army=phaseb::add_checked(result.army,
                phaseb::mul_checked(power,3)/100);
            result.army_observed=true;
        }
    }
    return result;
}
} // namespace vic25::phasec
