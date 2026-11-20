#pragma once
// P2 day-native operational planner. Every formation, border, casualty,
// ammunition lot, and change of occupation references original C/E/H data.
// No alternate armies, combat stock, front ownership or war score ledger.
#include "vic25/phase_gh.hpp"
#include <algorithm>
#include <map>
#include <limits>
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
// A single multi-source reverse BFS per belligerent assigns the nearest
// military front, rather than O(fronts * (provinces + armies)) searches.
inline std::vector<March> choices(phasegh::Stage& gh) {
    const auto& geo=gh.geography().spatial();
    const auto& c=gh.phase_c();
    std::map<std::pair<std::string,std::string>,People> force;
    for(const auto& [unit,d]:c.army_deployments()) {
        if(c.active_strength(unit)<=0||
           c.military_service_branch(unit)!=
               phasec::MilitaryServiceBranch::army||
           c.p5_guerrilla_is_insurgent_unit(unit))continue;
        const auto read=c.military_readiness(unit,d.region);
        const auto key=std::make_pair(d.country,d.region);
        force[key]=phaseb::add_checked(force[key],
                                       read.combat_capacity());
    }
    const auto military=[&](const std::string& country,
                             const std::string& region)->People {
        auto it=force.find({country,region});
        return it==force.end()?0:it->second;
    };
    std::vector<March> orders;
    for(const auto& [war_id,war]:gh.diplomacy().wars) {
        if(!is_active_ground_war(war))continue;
        const auto fronts=gh.war_front_regions(war);
        for(const auto& country:{war.attacker,war.defender}) {
            const auto& opponent=country==war.attacker?
                                 war.defender:war.attacker;
            struct Route {
                phasee::Id next{};
                std::size_t hops{};
                std::string target;
            };
            std::map<phasee::Id,Route> routes;
            std::queue<phasee::Id> pending;
            // Prefer enemy-held border provinces. Use own lines only if
            // no opponent-controlled frontier is reachable in this theater.
            bool hostile_roots=false;
            for(const auto& front:fronts)
                if(geo.regions.contains(front)&&
                   c.military_control(front)==opponent) {
                    const auto province=geo.regions.at(front);
                    if(routes.emplace(province,
                             Route{province,0,front}).second)
                        pending.push(province);
                    hostile_roots=true;
                }
            if(!hostile_roots)
                for(const auto& front:fronts)
                    if(geo.regions.contains(front)&&
                       c.military_control(front)==country) {
                        const auto province=geo.regions.at(front);
                        if(routes.emplace(province,
                                 Route{province,0,front}).second)
                            pending.push(province);
                    }
            while(!pending.empty()) {
                const auto node=pending.front();pending.pop();
                for(const auto neighbor:geo.provinces.at(node).neighbors) {
                    if(routes.contains(neighbor)||
                       !geo.provinces.contains(neighbor))continue;
                    const auto& region=geo.provinces.at(neighbor).region;
                    if(c.military_control(region)!=country)continue;
                    const auto& current=routes.at(node);
                    routes.emplace(neighbor,Route{
                        node,current.hops+1,current.target});
                    pending.push(neighbor);
                }
            }
            for(const auto& [unit,d]:c.army_deployments()) {
                if(d.country!=country||!geo.regions.contains(d.region)||
                   c.military_service_branch(unit)!=
                       phasec::MilitaryServiceBranch::army||
                   c.p5_guerrilla_is_insurgent_unit(unit)||
                   c.active_strength(unit)<=0)continue;
                const auto ready=c.military_readiness(unit,d.region);
                const auto own=ready.combat_capacity();
                const People enemy_here=military(opponent,d.region);
                // The physical army may retreat even with empty cartridges.
                if(enemy_here>phaseb::add_checked(
                       phaseb::mul_checked(own,2),People{1})) {
                    std::string fallback;
                    People least_threat=std::numeric_limits<People>::max();
                    const auto from=geo.regions.at(d.region);
                    for(const auto neighbor:geo.provinces.at(from).neighbors) {
                        const auto next=geo.provinces.find(neighbor);
                        if(next==geo.provinces.end()||
                           c.military_control(next->second.region)!=country)
                            continue;
                        const People threat=military(
                            opponent,next->second.region);
                        if(threat<least_threat||
                           threat==least_threat&&
                           (fallback.empty()||
                            next->second.region<fallback)) {
                            least_threat=threat;
                            fallback=next->second.region;
                        }
                    }
                    if(!fallback.empty()) {
                        orders.push_back({war_id,unit,d.region,fallback,
                            country,d.region,1,own,enemy_here,true});
                        continue;
                    }
                }
                if(!ready.combat_capable())continue;
                const auto path=routes.find(geo.regions.at(d.region));
                if(path==routes.end()||path->second.hops==0)continue;
                const auto next=geo.provinces.find(path->second.next);
                if(next==geo.provinces.end())continue;
                const auto& destination=next->second.region;
                const bool offensive=c.military_control(destination)==opponent;
                const People hostile=military(opponent,destination);
                // Do not swap opposing formations across the same border
                // on one tick. The defender holds a manned defensive line
                // unless the opposite position is actually vacant.
                if(offensive&&country==war.defender&&hostile>0)continue;
                // No independent tactical player order; the AI weighs
                // observed strength, real issued arms and learned doctrine.
                if(offensive&&hostile>phaseb::mul_checked(
                    ready.combat_capacity(true),2))continue;
                orders.push_back({
                    war_id,unit,d.region,destination,country,
                    path->second.target,path->second.hops,own,hostile,false});
            }
        }
    }
    std::sort(orders.begin(),orders.end(),
              [](const March& a,const March& b) {
        return std::tuple{!a.withdrawal,a.war,a.country,a.hops,a.unit}<
               std::tuple{!b.withdrawal,b.war,b.country,b.hops,b.unit};
    });
    return orders;
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
