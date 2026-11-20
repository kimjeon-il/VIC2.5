#pragma once
// P2 day-native operational planner. Every formation, border, casualty,
// ammunition lot, and change of occupation references original C/E/H data.
// No alternate armies, combat stock, front ownership or war score ledger.
#include "vic25/phase_gh.hpp"
#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace vic25::frontai {
using People=phaseb::People;
struct March {
    std::string war,unit,origin,destination,country,front;
    std::size_t hops{};
    People own_front{},enemy_front{};
    bool withdrawal{};
};
inline People operational_force(const phasec::IntegratedWorld& c,
                                const std::string& country,
                                const std::string& region) {
    People ready=0;
    for(const auto& [id,d]:c.army_deployments()) {
        if(d.country!=country||d.region!=region||
           c.military_service_branch(id)!=
               phasec::MilitaryServiceBranch::army||
           c.p5_guerrilla_is_insurgent_unit(id))continue;
        ready=phaseb::add_checked(
            ready,c.military_readiness(id,region).combat_capacity());
    }
    return ready;
}
inline bool is_active_ground_war(const phasegh::WarState& war) {
    return war.active&&!war.armistice;
}
// Each target requires one reverse BFS across friendly-controlled transit,
// except its (possibly enemy-controlled) root. No teleport, no third party.
inline std::map<phasee::Id,std::pair<phasee::Id,std::size_t>>
routes_to_front(const phasee::SpatialState& geo,
                const phasec::IntegratedWorld& c,
                const std::string& front,const std::string& country) {
    std::map<phasee::Id,std::pair<phasee::Id,std::size_t>> routes;
    const auto found=geo.regions.find(front);
    if(found==geo.regions.end())return routes;
    const auto root=found->second;
    routes[root]={root,0};
    std::queue<phasee::Id> pending;
    pending.push(root);
    while(!pending.empty()) {
        const auto node=pending.front();pending.pop();
        for(const auto neighbor:geo.provinces.at(node).neighbors) {
            if(routes.contains(neighbor)||
               !geo.provinces.contains(neighbor))continue;
            const auto& region=geo.provinces.at(neighbor).region;
            if(c.military_control(region)!=country)continue;
            routes[neighbor]={node,routes.at(node).second+1};
            pending.push(neighbor);
        }
    }
    return routes;
}
inline std::vector<March> choices(phasegh::Stage& gh) {
    const auto& geo=gh.geography().spatial();
    const auto& c=gh.phase_c();
    std::vector<March> choices;
    for(const auto& [war_id,war]:gh.diplomacy().wars) {
        if(!is_active_ground_war(war))continue;
        const auto fronts=gh.war_front_regions(war);
        for(const auto& country:{war.attacker,war.defender}) {
            const auto& opponent=country==war.attacker?
                                 war.defender:war.attacker;
            std::map<std::string,March> best;
            // A commander withdraws from a lost engagement, including when
            // out of ammunition. Friendly adjacency and a real unit remain
            // mandatory, even for a retreat.
            for(const auto& [unit,d]:c.army_deployments()) {
                if(d.country!=country||!geo.regions.contains(d.region)||
                   c.military_service_branch(unit)!=
                       phasec::MilitaryServiceBranch::army||
                   c.p5_guerrilla_is_insurgent_unit(unit)||
                   c.active_strength(unit)<=0)continue;
                const auto self=c.military_readiness(unit,d.region);
                const People hostile=operational_force(c,opponent,d.region);
                if(hostile>phaseb::add_checked(
                     phaseb::mul_checked(self.combat_capacity(),2),
                     People{1})) {
                    std::string fallback;
                    People least_enemy=std::numeric_limits<People>::max();
                    const auto province=geo.regions.at(d.region);
                    for(const auto neighbor:geo.provinces.at(province).neighbors) {
                        const auto it=geo.provinces.find(neighbor);
                        if(it==geo.provinces.end())continue;
                        const auto& next=it->second.region;
                        if(c.military_control(next)!=country)continue;
                        const People threat=operational_force(c,opponent,next);
                        if(threat<least_enemy||
                           (threat==least_enemy&&next<fallback)) {
                            fallback=next;least_enemy=threat;
                        }
                    }
                    if(!fallback.empty()) {
                        best[unit]={war_id,unit,d.region,fallback,
                                    country,d.region,1,
                                    self.combat_capacity(),hostile,true};
                        continue;
                    }
                }
            }
            for(const auto& front:fronts) {
                const auto found=geo.regions.find(front);
                if(found==geo.regions.end())continue;
                const auto holder=c.military_control(front);
                if(holder!=country&&holder!=opponent)continue;
                const auto routed=routes_to_front(geo,c,front,country);
                const People enemy=operational_force(c,opponent,front);
                for(const auto& [unit,d]:c.army_deployments()) {
                    if(d.country!=country||d.region==front||
                       best.contains(unit)&&best.at(unit).withdrawal||
                       c.military_service_branch(unit)!=
                           phasec::MilitaryServiceBranch::army||
                       c.p5_guerrilla_is_insurgent_unit(unit)||
                       !geo.regions.contains(d.region))continue;
                    const auto ready=c.military_readiness(unit,d.region);
                    if(!ready.combat_capable())continue;
                    const auto path=routed.find(geo.regions.at(d.region));
                    if(path==routed.end()||path->second.second==0)continue;
                    const auto next=geo.provinces.find(path->second.first);
                    if(next==geo.provinces.end())continue;
                    const bool attack=c.military_control(next->second.region)==opponent;
                    // No blind assault with a force clearly below the
                    // currently observed opposition; preserve defensive
                    // formations until reinforcements/stock arrive.
                    if(attack&&enemy>phaseb::mul_checked(
                        ready.combat_capacity(true),2))continue;
                    const March candidate{war_id,unit,d.region,
                        next->second.region,country,front,path->second.second,
                        ready.combat_capacity(attack),enemy,false};
                    const auto previous=best.find(unit);
                    if(previous==best.end()||
                       std::tuple{candidate.hops,
                           candidate.enemy_front==0?1:0,candidate.front}<
                       std::tuple{previous->second.hops,
                           previous->second.enemy_front==0?1:0,
                           previous->second.front})
                        best[unit]=candidate;
                }
            }
            for(const auto& [unit,order]:best) {
                (void)unit;
                choices.push_back(order);
            }
        }
    }
    std::sort(choices.begin(),choices.end(),
        [](const March& a,const March& b) {
            return std::tuple{!a.withdrawal,a.war,a.country,a.unit,a.front}<
                   std::tuple{!b.withdrawal,b.war,b.country,b.unit,b.front};
        });
    return choices;
}
inline void allocate_marches(
        phasegh::Stage& gh,const std::set<std::string>& manual_units={}) {
    const auto proposed=choices(gh);
    std::set<std::string> moved=manual_units;
    for(const auto& p:proposed) {
        if(moved.contains(p.unit))continue;
        const auto& current=gh.phase_c().army_deployments().at(p.unit);
        if(current.region!=p.origin||current.country!=p.country)continue;
        // Every chosen formation may march once, rather than limiting the
        // entire theater to a single movement per front per day.
        gh.march_army_for_front(p.war,p.unit,p.destination);
        moved.insert(p.unit);
    }
}
inline void occupy_decisive_fronts(phasegh::Stage& gh) {
    auto& c=gh.phase_c();
    std::set<std::string> resolved;
    for(const auto& [id,war]:gh.diplomacy().wars) {
        (void)id;
        if(!is_active_ground_war(war))continue;
        for(const auto& front:gh.war_front_regions(war)) {
            if(!resolved.insert(front).second)continue;
            bool a=false,b=false,other=false;
            bool armed_a=false,armed_b=false;
            for(const auto& [unit,d]:c.army_deployments()) {
                if(d.region!=front||c.active_strength(unit)<=0||
                   c.military_service_branch(unit)!=
                       phasec::MilitaryServiceBranch::army)continue;
                if(d.country==war.attacker) {
                    a=true;
                    armed_a|=c.military_readiness(unit,front).combat_capable();
                } else if(d.country==war.defender) {
                    b=true;
                    armed_b|=c.military_readiness(unit,front).combat_capable();
                } else other=true;
            }
            // Disarmed enemy is still physically present and prevents a
            // free change of control. Sovereignty/title never changes here.
            if(other||a==b||a&&!armed_a||b&&!armed_b)continue;
            const auto& victor=a?war.attacker:war.defender;
            if(c.military_control(front)!=victor)c.occupy(front,victor);
        }
    }
}
} // namespace vic25::frontai
