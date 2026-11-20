#pragma once
// P3 theater execution reads the real E coastline, C POP/Inventory/ship
// condition, H legal war and G verified transport. There is no abstract
// fleet, air power stock, hostile shipping point or parallel casualty ledger.
#include "vic25/phase_gh.hpp"
#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace vic25::theaterai {
using Quantity=vic25::Quantity;
using People=phaseb::People;
using Id=phaseb::Id;
inline Id first_active(const phasec::IntegratedWorld& c,const std::string& unit) {
    for(const auto& [id,n]:c.unit_slices(unit))
        if(n>0&&c.people().has_slice(id)&&
           c.people().slice(id).count==n)return id;
    return 0;
}
inline bool same_sea(const phasee::SpatialState& geo,
                     const std::string& a,const std::string& b) {
    if(!geo.regions.contains(a)||!geo.regions.contains(b))return false;
    const auto& coast_a=geo.provinces.at(geo.regions.at(a)).coastal_seas;
    const auto& coast_b=geo.provinces.at(geo.regions.at(b)).coastal_seas;
    for(const auto sea:coast_a)
        if(coast_b.contains(sea))return true;
    return false;
}
inline bool adjacent_or_same(const phasee::SpatialState& geo,
                            const std::string& a,const std::string& b) {
    const auto aa=geo.regions.find(a),bb=geo.regions.find(b);
    if(aa==geo.regions.end()||bb==geo.regions.end())return false;
    return aa->second==bb->second||
           geo.provinces.at(aa->second).neighbors.contains(bb->second);
}
struct Sortie {
    std::string unit,country,region,hull;
    Id slice{};
    Quantity capacity{};
};
inline std::vector<Sortie> ready_naval(phasegh::Stage& gh) {
    const auto& c=gh.phase_c();
    const auto& geo=gh.geography().spatial();
    std::vector<Sortie> result;
    for(const auto& [id,hull]:c.commissioned_naval_hulls()) {
        if(!hull.commissioned||hull.current_hp<=0||
           !c.army_deployments().contains(hull.unit))continue;
        const auto& d=c.army_deployments().at(hull.unit);
        if(!geo.regions.contains(d.region)||
           geo.provinces.at(geo.regions.at(d.region)).coastal_seas.empty()||
           c.military_service_branch(hull.unit)!=
               phasec::MilitaryServiceBranch::navy)continue;
        const Id slice=first_active(c,hull.unit);
        if(!slice)continue;
        const Quantity capacity=std::min<Quantity>({
            c.unit_slice_strength(hull.unit,slice),
            c.stock(hull.unit,d.region,"ammunition"),
            c.stock(hull.unit,d.region,"fuel"),
            hull.current_hp});
        if(capacity>0)
            result.push_back({hull.unit,d.country,d.region,id,slice,capacity});
    }
    return result;
}
inline void resolve_naval_engagements(phasegh::Stage& gh) {
    auto& c=gh.phase_c();
    const auto& geo=gh.geography().spatial();
    const auto available=ready_naval(gh);
    std::set<std::string> engaged;
    for(const auto& [id,war]:gh.diplomacy().wars) {
        if(!war.active||war.armistice)continue;
        for(const auto& a:available) {
            if(a.country!=war.attacker||engaged.contains(a.unit))continue;
            for(const auto& b:available) {
                if(b.country!=war.defender||engaged.contains(b.unit)||
                   !same_sea(geo,a.region,b.region))continue;
                const auto& ah=c.commissioned_naval_hulls().at(a.hull);
                const auto& bh=c.commissioned_naval_hulls().at(b.hull);
                const Quantity fire_a=std::min(a.capacity,
                    c.stock(a.unit,a.region,"ammunition"));
                const Quantity fire_b=std::min(b.capacity,
                    c.stock(b.unit,b.region,"ammunition"));
                if(fire_a<=0||fire_b<=0||ah.current_hp<=0||bh.current_hp<=0)
                    continue;
                const Quantity damage_a=std::min<Quantity>(
                    ah.current_hp,std::max<Quantity>(1,fire_b/2));
                const Quantity damage_b=std::min<Quantity>(
                    bh.current_hp,std::max<Quantity>(1,fire_a/2));
                const People crew_a=c.unit_slice_strength(a.unit,a.slice);
                const People crew_b=c.unit_slice_strength(b.unit,b.slice);
                const People hurt_a=std::min<People>(
                    crew_a,phaseb::add_checked(damage_a/4,People{1}));
                const People hurt_b=std::min<People>(
                    crew_b,phaseb::add_checked(damage_b/4,People{1}));
                // Both sides' ammunition, fuel, casualties and hull damage
                // enter the same daily C transaction, even across two ports.
                c.battle(a.unit,a.slice,a.region,phasec::CombatLoss{
                    hurt_a/5,hurt_a-hurt_a/5,0,0,0,fire_a});
                c.battle(b.unit,b.slice,b.region,phasec::CombatLoss{
                    hurt_b/5,hurt_b-hurt_b/5,0,0,0,fire_b});
                c.consume(a.unit,a.region,"fuel",1);
                c.consume(b.unit,b.region,"fuel",1);
                c.phase_time_damage_naval_hull(a.hull,damage_a);
                c.phase_time_damage_naval_hull(b.hull,damage_b);
                gh.record_actual_war_engagement(id);
                engaged.insert(a.unit);engaged.insert(b.unit);
                break;
            }
        }
    }
}
inline std::vector<Sortie> ready_air(phasegh::Stage& gh) {
    const auto& c=gh.phase_c();
    const auto& geo=gh.geography().spatial();
    std::vector<Sortie> result;
    for(const auto& [unit,d]:c.army_deployments()) {
        if(c.military_service_branch(unit)!=
               phasec::MilitaryServiceBranch::air||
           !geo.regions.contains(d.region))continue;
        const Id slice=first_active(c,unit);
        if(!slice)continue;
        const Quantity available=std::min<Quantity>({
            c.unit_slice_strength(unit,slice),
            c.assigned_equipment(unit,d.region,"aircraft"),
            c.stock(unit,d.region,"ammunition"),
            c.stock(unit,d.region,"fuel")});
        if(available>0)
            result.push_back({unit,d.country,d.region,"",slice,available});
    }
    return result;
}
inline void resolve_air_engagements(phasegh::Stage& gh,
                                   std::set<std::string>& flown) {
    auto& c=gh.phase_c();
    const auto& geo=gh.geography().spatial();
    const auto ready=ready_air(gh);
    for(const auto& [id,war]:gh.diplomacy().wars) {
        if(!war.active||war.armistice)continue;
        for(const auto& a:ready) {
            if(a.country!=war.attacker||flown.contains(a.unit))continue;
            for(const auto& b:ready) {
                if(b.country!=war.defender||flown.contains(b.unit)||
                   !adjacent_or_same(geo,a.region,b.region))continue;
                const auto shots_a=std::min(a.capacity,
                    c.stock(a.unit,a.region,"ammunition"));
                const auto shots_b=std::min(b.capacity,
                    c.stock(b.unit,b.region,"ammunition"));
                if(shots_a<=0||shots_b<=0)continue;
                const People hurt_a=std::min<People>(
                    c.unit_slice_strength(a.unit,a.slice),
                    phaseb::add_checked(shots_b/3,People{1}));
                const People hurt_b=std::min<People>(
                    c.unit_slice_strength(b.unit,b.slice),
                    phaseb::add_checked(shots_a/3,People{1}));
                c.battle(a.unit,a.slice,a.region,phasec::CombatLoss{
                    hurt_a/4,hurt_a-hurt_a/4,0,0,0,shots_a});
                c.battle(b.unit,b.slice,b.region,phasec::CombatLoss{
                    hurt_b/4,hurt_b-hurt_b/4,0,0,0,shots_b});
                c.consume(a.unit,a.region,"fuel",shots_a);
                c.consume(b.unit,b.region,"fuel",shots_b);
                gh.record_actual_war_engagement(id);
                flown.insert(a.unit);flown.insert(b.unit);
                break;
            }
        }
    }
}
// The superiority of actively serviceable aircraft is derived locally.
// Only a paid-for, fueled, ammunition-consuming flight grants land support.
struct AirSupport {std::int64_t attacker_ppm{},defender_ppm{};};
inline AirSupport ground_air_support(phasegh::Stage& gh,
                                    const phasegh::WarState& war,
                                    const std::string& region,
                                    std::set<std::string>& flown) {
    auto& c=gh.phase_c();
    const auto& geo=gh.geography().spatial();
    AirSupport result;
    auto aircraft=ready_air(gh);
    Quantity attacker_available=0,defender_available=0;
    for(const auto& flight:aircraft) {
        if(flown.contains(flight.unit)||
           !adjacent_or_same(geo,flight.region,region))continue;
        if(flight.country==war.attacker)
            attacker_available=phaseb::add_checked(
                attacker_available,flight.capacity);
        else if(flight.country==war.defender)
            defender_available=phaseb::add_checked(
                defender_available,flight.capacity);
    }
    for(const auto& flight:aircraft) {
        if(flown.contains(flight.unit)||
           !adjacent_or_same(geo,flight.region,region))continue;
        const bool own_attacker=flight.country==war.attacker;
        if(!own_attacker&&flight.country!=war.defender)continue;
        const Quantity opposition=own_attacker?
            defender_available:attacker_available;
        const Quantity friendly=own_attacker?
            attacker_available:defender_available;
        if(friendly<opposition)continue;
        const Quantity committed=std::min<Quantity>(flight.capacity,15);
        if(committed<=0)continue;
        c.consume(flight.unit,flight.region,"ammunition",committed);
        c.consume(flight.unit,flight.region,"fuel",committed);
        const auto boost=std::min<std::int64_t>(300'000,committed*20'000);
        if(own_attacker)
            result.attacker_ppm=std::min<std::int64_t>(
                300'000,result.attacker_ppm+boost);
        else
            result.defender_ppm=std::min<std::int64_t>(
                300'000,result.defender_ppm+boost);
        flown.insert(flight.unit);
    }
    return result;
}
// Amphibious movement is possible only on a proven DIRECT one-day sea
// service, in the same real E coastal sea, with a real crewed transport hull,
// assigned landing berths, fuel, ammunition and an unoccupied claimed shore.
// Longer passages require a distinct troop-in-transit state (not invented).
inline std::set<std::string> conduct_verified_landings(phasegh::Stage& gh) {
    auto& c=gh.phase_c();
    const auto& geo=gh.geography().spatial();
    const auto& trade=gh.commerce();
    std::set<std::string> used_armies,used_fleets;
    const auto hulls=ready_naval(gh);
    for(const auto& [id,war]:gh.diplomacy().wars) {
        (void)id;
        if(!war.active||war.armistice)continue;
        for(const auto& country:{war.attacker,war.defender}) {
            const auto& opponent=country==war.attacker?
                                 war.defender:war.attacker;
            for(const auto& [unit,d]:c.army_deployments()) {
                if(d.country!=country||used_armies.contains(unit)||
                   c.military_service_branch(unit)!=
                       phasec::MilitaryServiceBranch::army||
                   c.active_strength(unit)<=0||
                   !geo.regions.contains(d.region)||
                   c.military_control(d.region)!=country)continue;
                const auto source=geo.regions.at(d.region);
                if(geo.provinces.at(source).coastal_seas.empty())continue;
                for(const auto& [target_id,claimants]:geo.claims) {
                    if(used_armies.contains(unit))break;
                    if(!claimants.contains(country)||
                       !geo.provinces.contains(target_id))continue;
                    const auto& shore=geo.provinces.at(target_id).region;
                    if(c.sovereignty(shore)!=opponent||
                       c.military_control(shore)!=opponent||
                       !same_sea(geo,d.region,shore))continue;
                    bool defended=false;
                    for(const auto& [enemy,dep]:c.army_deployments())
                        if(dep.region==shore&&dep.country==opponent&&
                           c.active_strength(enemy)>0&&
                           c.military_service_branch(enemy)==
                               phasec::MilitaryServiceBranch::army) {
                            defended=true;break;
                        }
                    if(defended)continue;
                    bool direct=false;
                    for(const auto& [edge_id,edge]:trade.transport_edges) {
                        (void)edge_id;
                        if(!edge.enabled||edge.mode!="sea"||
                           edge.transit_days!=1||
                           edge.monthly_capacity<c.active_strength(unit))
                            continue;
                        if((edge.from==source&&edge.to==target_id)||
                           (edge.to==source&&edge.from==target_id)) {
                            direct=true;break;
                        }
                    }
                    if(!direct)continue;
                    for(const auto& fleet:hulls) {
                        if(fleet.country!=country||fleet.region!=d.region||
                           used_fleets.contains(fleet.unit)||fleet.capacity<=0||
                           c.assigned_equipment(fleet.unit,d.region,
                                                "landing_berth")<
                                 c.active_strength(unit))continue;
                        const Quantity carried=std::min<Quantity>(
                            c.active_strength(unit),
                            c.stock(unit,d.region,"ammunition"));
                        if(carried<=0)continue;
                        c.consume(fleet.unit,d.region,"fuel",1);
                        c.consume(fleet.unit,d.region,"ammunition",1);
                        c.phase_time_move_army(unit,shore,carried);
                        used_armies.insert(unit);used_fleets.insert(fleet.unit);
                        break;
                    }
                }
            }
        }
    }
    return used_armies;
}
// Postwar troop repatriation: one real adjacent friendly-controlled land
// step per day. The shortest path is rebuilt from E's current geometry and
// C's sovereign/occupied regions, not a second troop-position ledger.
inline void withdraw_after_peace(phasegh::Stage& gh) {
    auto& c=gh.phase_c();
    const auto& geo=gh.geography().spatial();
    std::set<std::string> fighting_countries,return_from_war;
    for(const auto& [id,war]:gh.diplomacy().wars) {
        (void)id;
        if(war.active&&!war.armistice) {
            fighting_countries.insert(war.attacker);
            fighting_countries.insert(war.defender);
        } else if(war.armistice||war.ended_day>=0) {
            return_from_war.insert(war.attacker);
            return_from_war.insert(war.defender);
        }
    }
    std::set<std::string> returning_countries;
    for(const auto& [unit,d]:c.army_deployments()) {
        if(c.military_service_branch(unit)!=
               phasec::MilitaryServiceBranch::army||
           fighting_countries.contains(d.country)||
           !return_from_war.contains(d.country)||
           !geo.regions.contains(d.region)||c.active_strength(unit)<=0)
            continue;
        if(c.sovereignty(d.region)!=d.country)
            returning_countries.insert(d.country);
    }
    for(const auto& country:returning_countries) {
        std::map<phasee::Id,phasee::Id> route;
        std::queue<phasee::Id> pending;
        for(const auto& [id,province]:geo.provinces)
            if(c.sovereignty(province.region)==country&&
               c.military_control(province.region)==country) {
                route.emplace(id,id);
                pending.push(id);
            }
        while(!pending.empty()) {
            const auto current=pending.front();pending.pop();
            for(const auto neighbor:geo.provinces.at(current).neighbors) {
                if(route.contains(neighbor)||
                   !geo.provinces.contains(neighbor))continue;
                const auto& next=geo.provinces.at(neighbor).region;
                if(c.military_control(next)!=country)continue;
                route.emplace(neighbor,current);
                pending.push(neighbor);
            }
        }
        for(const auto& [unit,d]:c.army_deployments()) {
            if(d.country!=country||
               c.military_service_branch(unit)!=
                   phasec::MilitaryServiceBranch::army||
               c.sovereignty(d.region)==country||
               c.active_strength(unit)<=0)continue;
            const auto origin=geo.regions.at(d.region);
            const auto next=route.find(origin);
            phasee::Id destination=0;
            if(next!=route.end()&&next->second!=origin)
                destination=next->second;
            else if(next==route.end()) {
                // A retreating foreign garrison may start on an opposing
                // control tile, provided it can immediately enter a real
                // adjacent friendly-controlled path back to its own land.
                for(const auto neighbor:geo.provinces.at(origin).neighbors)
                    if(route.contains(neighbor)&&
                       geo.provinces.contains(neighbor)&&
                       c.military_control(
                           geo.provinces.at(neighbor).region)==country) {
                        destination=neighbor;break;
                    }
            }
            if(destination==0)continue;
            const auto& region=geo.provinces.at(destination).region;
            const Quantity ammunition=std::min<Quantity>(
                c.active_strength(unit),
                c.stock(unit,d.region,"ammunition"));
            gh.geography().march_unit_adjacent(unit,region,ammunition);
        }
    }
}
} // namespace vic25::theaterai
