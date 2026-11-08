#pragma once
// A bounded multi-front operational planner. It only moves actual C armies
// over authenticated E province adjacency within their own controlled land,
// or onto the documented crisis front of an already active legal H war.
// It never invents a general, equipment, stock, supply or war goal.
#include "vic25/phase_gh.hpp"
#include <algorithm>
#include <map>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace vic25::frontai {
struct March {
    std::string war,unit,origin,destination,country,front;
    std::size_t hops{};
    phaseb::People own_front{},enemy_front{};
};
inline std::optional<std::pair<std::string,std::size_t>> next_step(
        const phasee::SpatialState& geo,
        const phasec::IntegratedWorld& c,
        const std::string& origin,const std::string& front,
        const std::string& country) {
    if(!geo.regions.contains(origin)||!geo.regions.contains(front)||
       origin==front)return std::nullopt;
    const auto start=geo.regions.at(origin);
    const auto target=geo.regions.at(front);
    std::queue<phasee::Id> pending;
    std::map<phasee::Id,phasee::Id> parent;
    pending.push(start);
    parent.emplace(start,start);
    while(!pending.empty()&&!parent.contains(target)) {
        const auto from=pending.front();
        pending.pop();
        for(const auto neighbor:geo.provinces.at(from).neighbors) {
            if(parent.contains(neighbor)||
               !geo.provinces.contains(neighbor))continue;
            const auto& region=geo.provinces.at(neighbor).region;
            if(neighbor!=target&&c.military_control(region)!=country)
                continue; // no transit through a third country
            parent.emplace(neighbor,from);
            pending.push(neighbor);
        }
    }
    if(!parent.contains(target))return std::nullopt;
    std::size_t hops=0;
    auto cursor=target;
    while(cursor!=start) {
        const auto it=parent.find(cursor);
        if(it==parent.end())return std::nullopt;
        ++hops;
        if(it->second==start)
            return {{geo.provinces.at(cursor).region,hops}};
        cursor=it->second;
    }
    return std::nullopt;
}
// Invert each supported front once and reuse the shortest lawful next hop
// for all armies of the same belligerent. O(fronts * (V+E) + armies) rather
// than running a province-wide search for every regiment.
inline std::map<phasee::Id,std::pair<phasee::Id,std::size_t>>
    routes_to_front(const phasee::SpatialState& geo,
                    const phasec::IntegratedWorld& c,
                    const std::string& front,const std::string& country) {
    std::map<phasee::Id,std::pair<phasee::Id,std::size_t>> routes;
    auto target=geo.regions.find(front);
    if(target==geo.regions.end())return routes;
    const auto root=target->second;
    routes[root]={root,0};
    std::queue<phasee::Id> queue;
    queue.push(root);
    while(!queue.empty()) {
        const auto node=queue.front();
        queue.pop();
        for(const auto neighbor:geo.provinces.at(node).neighbors) {
            if(!geo.provinces.contains(neighbor)||
               routes.contains(neighbor))continue;
            const auto& region=geo.provinces.at(neighbor).region;
            if(neighbor!=root&&c.military_control(region)!=country)
                continue;
            routes[neighbor]={node,routes.at(node).second+1};
            queue.push(neighbor);
        }
    }
    return routes;
}
inline std::vector<March> choices(phasegh::Stage& gh) {
    const auto& geo=gh.geography().spatial();
    const auto& c=gh.phase_c();
    const auto& diplomatic=gh.diplomacy();
    std::vector<March> candidates;
    for(const auto& [war_id,war]:diplomatic.wars) {
        if(!war.active||war.armistice)continue;
        for(const auto& front:gh.war_front_regions(war)) {
            if(!geo.regions.contains(front))continue;
            const auto target=geo.regions.at(front);
            for(const auto& country:{war.attacker,war.defender}) {
                phaseb::People own=0,enemy=0;
                for(const auto& [unit,d]:c.army_deployments()) {
                    if(d.region!=front||c.active_strength(unit)<=0)
                        continue;
                    if(d.country==country)
                        own=phaseb::add_checked(own,c.active_strength(unit));
                    else if(d.country==war.attacker||
                            d.country==war.defender)
                        enemy=phaseb::add_checked(enemy,c.active_strength(unit));
                }
                const auto routes=routes_to_front(geo,c,front,country);
                for(const auto& [unit,d]:c.army_deployments()) {
                    if(d.country!=country||d.region==front||
                       c.active_strength(unit)<=0||
                       c.stock(unit,d.region,"ammunition")<=0||
                       !geo.regions.contains(d.region))continue;
                    const auto origin=geo.regions.at(d.region);
                    auto next=routes.find(origin);
                    if(next==routes.end()||next->second.second==0||
                       !geo.provinces.contains(next->second.first))continue;
                    candidates.push_back({
                        war_id,unit,d.region,
                        geo.provinces.at(next->second.first).region,
                        country,front,next->second.second,own,enemy});
                }
            }
        }
    }
    std::sort(candidates.begin(),candidates.end(),
        [](const March& a,const March& b) {
            const bool a_disadvantage=a.enemy_front>a.own_front;
            const bool b_disadvantage=b.enemy_front>b.own_front;
            return std::tuple{!a_disadvantage,a.hops,a.war,a.front,a.unit}<
                   std::tuple{!b_disadvantage,b.hops,b.war,b.front,b.unit};
        });
    return candidates;
}
inline void allocate_marches(phasegh::Stage& gh,
                             const std::set<std::string>& manual_units={}) {
    auto candidates=choices(gh);
    std::set<std::string> moved=manual_units;
    std::set<std::pair<std::string,std::string>> covered_fronts;
    for(const auto& c:candidates) {
        const auto front=std::make_pair(c.war,
                                      c.country+":"+c.front);
        if(moved.contains(c.unit)||covered_fronts.contains(front))continue;
        const auto& present=gh.phase_c().army_deployments().at(c.unit);
        if(present.region!=c.origin||present.country!=c.country)continue;
        gh.march_army_for_front(c.war,c.unit,c.destination);
        moved.insert(c.unit);
        covered_fronts.insert(front);
    }
}
// Military occupation changes temporary control only after actual opposing
// units are no longer operational at the province and a victorious active
// country retains at least one armed soldier. No sovereignty/title transfer.
inline void occupy_decisive_fronts(phasegh::Stage& gh) {
    auto& c=gh.phase_c();
    std::set<std::string> resolved_today;
    for(const auto& [id,war]:gh.diplomacy().wars) {
        (void)id;
        if(!war.active||war.armistice)continue;
        for(const auto& front:gh.war_front_regions(war)) {
            if(!resolved_today.insert(front).second)continue;
            bool attacker=false,defender=false,neutral=false;
            bool armed_attacker=false,armed_defender=false;
            for(const auto& [unit,d]:c.army_deployments()) {
                if(d.region!=front||c.active_strength(unit)<=0)continue;
                if(d.country==war.attacker) {
                    attacker=true;
                    if(c.stock(unit,front,"ammunition")>0)armed_attacker=true;
                } else if(d.country==war.defender) {
                    defender=true;
                    if(c.stock(unit,front,"ammunition")>0)armed_defender=true;
                } else neutral=true;
            }
            // Out-of-ammo enemy soldiers are still physically present.
            // Capture requires uncontested real presence and actual arms,
            // never an arbitrary morale or occupation points balance.
            if(neutral||attacker==defender)continue;
            if(attacker&&!armed_attacker)continue;
            if(defender&&!armed_defender)continue;
            const auto& victor=attacker?war.attacker:war.defender;
            if(c.military_control(front)!=victor)c.occupy(front,victor);
        }
    }
}
} // namespace vic25::frontai
