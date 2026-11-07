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
    std::string war,unit,origin,destination,country;
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
inline std::vector<March> choices(phasegh::Stage& gh) {
    const auto& geo=gh.geography();
    const auto& c=gh.phase_c();
    const auto& diplomatic=gh.diplomacy();
    std::vector<March> candidates;
    for(const auto& [war_id,war]:diplomatic.wars) {
        if(!war.active||war.armistice)continue;
        const auto crisis=diplomatic.crises.find(war.crisis);
        if(crisis==diplomatic.crises.end()||
           !geo.provinces.contains(crisis->second.province))
            continue;
        const auto& front=geo.provinces.at(crisis->second.province).region;
        for(const auto& country:{war.attacker,war.defender}) {
            phaseb::People own=0,enemy=0;
            for(const auto& [unit,d]:c.army_deployments()) {
                if(d.region!=front||c.active_strength(unit)<=0)continue;
                if(d.country==country)own=phaseb::add_checked(
                    own,c.active_strength(unit));
                else if(d.country==war.attacker||d.country==war.defender)
                    enemy=phaseb::add_checked(enemy,c.active_strength(unit));
            }
            // One unit per actor and actual front. Prioritize materially
            // threatened fronts over reinforcements to empty safe provinces.
            for(const auto& [unit,d]:c.army_deployments()) {
                if(d.country!=country||d.region==front||
                   c.active_strength(unit)<=0||
                   c.stock(unit,d.region,"ammunition")<=0)continue;
                auto step=next_step(geo,c,d.region,front,country);
                if(!step)continue;
                candidates.push_back({war_id,unit,d.region,step->first,
                                     country,step->second,own,enemy});
            }
        }
    }
    std::sort(candidates.begin(),candidates.end(),
        [](const March& a,const March& b) {
            const auto a_disadvantage=a.enemy_front>a.own_front;
            const auto b_disadvantage=b.enemy_front>b.own_front;
            return std::tuple{!a_disadvantage,a.hops,a.war,a.unit}<
                   std::tuple{!b_disadvantage,b.hops,b.war,b.unit};
        });
    return candidates;
}
inline void allocate_marches(phasegh::Stage& gh,
                             const std::set<std::string>& manual_units={}) {
    auto candidates=choices(gh);
    std::set<std::string> moved=manual_units;
    std::set<std::pair<std::string,std::string>> covered_fronts;
    for(const auto& c:candidates) {
        if(moved.contains(c.unit)||!covered_fronts.insert(
                {c.war,c.country}).second)continue;
        const auto& present=gh.phase_c().army_deployments().at(c.unit);
        if(present.region!=c.origin||present.country!=c.country)continue;
        gh.march_army_for_front(c.war,c.unit,c.destination);
        moved.insert(c.unit);
    }
}
// Military occupation changes temporary control only after actual opposing
// units are no longer operational at the province and a victorious active
// country retains at least one armed soldier. No sovereignty/title transfer.
inline void occupy_decisive_fronts(phasegh::Stage& gh) {
    auto& c=gh.phase_c();
    for(const auto& [id,war]:gh.diplomacy().wars) {
        (void)id;
        if(!war.active||war.armistice)continue;
        const auto crisis=gh.diplomacy().crises.find(war.crisis);
        if(crisis==gh.diplomacy().crises.end()||
           !gh.geography().provinces.contains(crisis->second.province))
            continue;
        const auto& front=gh.geography().provinces.at(
            crisis->second.province).region;
        bool attacker=false,defender=false;
        for(const auto& [unit,d]:c.army_deployments()) {
            if(d.region!=front||c.active_strength(unit)<=0||
               c.stock(unit,front,"ammunition")<=0)continue;
            if(d.country==war.attacker)attacker=true;
            else if(d.country==war.defender)defender=true;
        }
        if(attacker==defender)continue; // contested or neither can act
        const auto& victor=attacker?war.attacker:war.defender;
        if(c.military_control(front)!=victor)c.occupy(front,victor);
    }
}
} // namespace vic25::frontai
