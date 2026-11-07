#pragma once
// Phase F: autonomous physical freight planner layered over G's one canonical
// transport-edge network and daily::World's saved physical cargo journal.
// No duplicated goods, cash, capacities or phantom logistics sidecar.
#include "vic25/daily_time.hpp"
#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <vector>
#include <stdexcept>
#include <string>

namespace vic25::phasef {
using Quantity=vic25::Quantity;
using Money=phasec::Money;

// Ephemeral within one day only. The authoritative reservation and inventory
// ledgers still live in C/G; this prevents independent AI freight lanes from
// scheduling the same owner's goods/cash or the same edge twice in one tick.
struct DayPlanningReservations {
    std::map<std::tuple<std::string,std::string,std::string>,Quantity> goods;
    std::map<std::string,Quantity> edge_units;
    std::map<std::string,Money> cash;
};

struct FreightLane {
    std::string id,seller,buyer,good,origin_region,destination_region;
    Quantity target_inventory{},maximum_daily_units{};
    // Historically grounded transit times in actual days: do NOT derive
    // days by multiplying the legacy G transit_months by 30/31.
    std::map<std::string,std::int32_t> edge_transit_days;
    Quantity confirmed_loss_per_hop{};
};

inline void validate(const FreightLane& lane) {
    if(lane.id.empty()||lane.id.find(':')!=std::string::npos||
       lane.seller.empty()||lane.buyer.empty()||lane.good.empty()||
       lane.origin_region.empty()||lane.destination_region.empty()||
       lane.origin_region==lane.destination_region||
       lane.target_inventory<=0||lane.maximum_daily_units<=0||
       lane.confirmed_loss_per_hop<0)
        throw std::invalid_argument("F_INVALID_AUTONOMOUS_FREIGHT_LANE");
    for(const auto& [edge,days]:lane.edge_transit_days)
        if(edge.empty()||days<=0)
            throw std::invalid_argument("F_INVALID_EDGE_TRANSIT_DAYS");
}

// A single policy can maintain a chain of real C inventory lots in flight.
// It forwards one delivered hop on the next game day, then starts another
// initial shipment only after the previous chain reaches the final receiver.
// This avoids duplicate purchases at transit hubs and speculative teleport.
inline std::optional<daily::Command> plan_next_hop(
        const daily::World& world,const FreightLane& lane,
        DayPlanningReservations& provisional) {
    validate(lane);
    const auto& gh=world.engine();
    const auto& geo=gh.base().spatial();
    const auto& ledger=gh.base().core().core();
    const auto& trade=gh.commerce();
    const auto current=world.date();
    const auto dim=daily::days_in_month(current.year,current.month);
    const std::string prefix="F:"+lane.id+":";
    const daily::DailyCargo* ready=nullptr;
    for(const auto& [id,c]:world.cargo()) {
        if(!id.starts_with(prefix))continue;
        if(c.sender.empty()||c.recipient!=lane.buyer||c.good!=lane.good)
            throw std::runtime_error("F_CORRUPTED_LANE_CARGO_IDENTITY");
        if(!c.arrived)return std::nullopt; // one active chain per policy
        if(c.destination!=lane.destination_region &&
           c.quantity>c.loss && !world.cargo().contains(id+":NEXT")) {
            if(ready)throw std::runtime_error("F_AMBIGUOUS_LANE_CONTINUATION");
            ready=&c;
        }
    }

    std::string shipper,origin,id;
    Quantity wanted=0;
    if(ready) {
        origin=ready->destination;
        shipper=lane.buyer; // recipient owns real cargo at intermediate hub
        id=ready->id+":NEXT";
        wanted=ready->quantity-ready->loss;
    } else {
        const Quantity destination_stock=ledger.stock(
            lane.buyer,lane.destination_region,lane.good);
        if(destination_stock>=lane.target_inventory)return std::nullopt;
        wanted=std::min(lane.maximum_daily_units,
                         lane.target_inventory-destination_stock);
        origin=lane.origin_region;
        shipper=lane.seller;
        id=prefix+std::to_string(world.day_index());
    }
    const auto from=geo.regions.at(origin);
    const auto to=geo.regions.at(lane.destination_region);
    const auto route=phasegh::quote_daily_freight_path(
        trade,from,to,gh.month(),current.day,dim,&provisional.edge_units,
        true,&lane.edge_transit_days);
    if(!route||route->edges.empty()||route->capacity<=0)
        return std::nullopt;
    const auto& edge=trade.transport_edges.at(route->edges.front());
    const auto configured=lane.edge_transit_days.find(edge.id);
    std::int32_t transit_days=edge.transit_days;
    if(configured!=lane.edge_transit_days.end()) {
        if(transit_days>0&&configured->second!=transit_days)
            throw std::invalid_argument("F_CONFLICTING_TRANSIT_DAY_SOURCES");
        transit_days=configured->second;
    }
    if(transit_days<=0)
        throw std::invalid_argument("F_MISSING_VERIFIED_EDGE_DAYS");
    const auto& hop_region=geo.provinces.at(edge.to).region;
    const auto stock_key=std::make_tuple(shipper,origin,lane.good);
    const Quantity stock=ledger.stock(shipper,origin,lane.good);
    const Quantity held=provisional.goods[stock_key];
    if(held>stock)throw std::logic_error("F_PROVISIONAL_GOODS_OVERRESERVED");
    Quantity units=std::min({wanted,route->capacity,stock-held});
    const Money available_cash=ledger.cash(shipper);
    const Money cash_held=provisional.cash[shipper];
    if(cash_held>available_cash)
        throw std::logic_error("F_PROVISIONAL_CASH_OVERRESERVED");
    if(shipper!=edge.carrier&&edge.freight_per_unit>0)
        units=std::min(units,
           (available_cash-cash_held)/edge.freight_per_unit);
    if(units<=0)return std::nullopt;
    const Money freight=shipper==edge.carrier?0:
         phaseb::mul_checked(units,edge.freight_per_unit);
    provisional.goods[stock_key]=phaseb::add_checked(held,units);
    provisional.edge_units[edge.id]=phaseb::add_checked(
           provisional.edge_units[edge.id],units);
    provisional.cash[shipper]=phaseb::add_checked(cash_held,freight);

    daily::Command command;
    command.id=std::move(id);
    command.kind=daily::Kind::cargo_dispatch;
    command.from=std::move(shipper);
    command.to=lane.buyer;
    command.region=std::move(origin);
    command.destination=hop_region;
    command.good=lane.good;
    command.route_id=edge.id;
    command.amount=units;
    command.unit_price=transit_days; // actual travel days
    command.auxiliary=std::min(units,lane.confirmed_loss_per_hop);
    return command;
}
// Single-lane callers can still obtain a quote without retaining the local
// projection; multi-lane P0 runs pass the shared day-local reservation set.
inline std::optional<daily::Command> plan_next_hop(
        const daily::World& world,const FreightLane& lane) {
    DayPlanningReservations provisional;
    return plan_next_hop(world,lane,provisional);
}
// Supplies are acquired from state-owned physical arsenals (the existing
// sovereign treasury wallet), not created when a military unit is underarmed.
// Direct verified transport links only: if the army is not connected, remain
// out of ammunition until a real service exists. The resulting shipment uses
// the exact same dated World cargo command and C/G reservations as civilian F.
inline std::vector<daily::Command> plan_direct_army_supply(
        const daily::World& world,DayPlanningReservations& provisional) {
    const auto& gh=world.engine();
    const auto& c=gh.base().core().core();
    const auto& geography=gh.base().spatial();
    const auto& transport=gh.commerce();
    const auto current=world.date();
    const int dim=daily::days_in_month(current.year,current.month);
    std::set<std::string> belligerents;
    for(const auto& [id,war]:gh.diplomacy().wars) {
        (void)id;
        if(!war.active||war.armistice)continue;
        belligerents.insert(war.attacker);
        belligerents.insert(war.defender);
    }
    std::vector<daily::Command> orders;
    for(const auto& [unit,deployment]:c.army_deployments()) {
        const auto& country=deployment.country;
        if(!belligerents.contains(country)||!c.has_country_treasury(country)||
           !geography.regions.contains(deployment.region))continue;
        const auto& arsenal=c.country_treasury(country);
        const Quantity desired=c.active_strength(unit);
        if(desired<=0)continue;
        const Quantity current_ammo=c.stock(unit,deployment.region,"ammunition");
        Quantity in_transit=0;
        for(const auto& [shipment_id,cargo]:world.cargo()) {
            (void)shipment_id;
            if(cargo.arrived||cargo.recipient!=unit||
               cargo.destination!=deployment.region||
               cargo.good!="ammunition")continue;
            in_transit=phaseb::add_checked(in_transit,
                                           cargo.quantity-cargo.loss);
        }
        const Quantity already_supplied=phaseb::add_checked(
            current_ammo,in_transit);
        if(already_supplied>=desired)continue;
        Quantity lacking=desired-already_supplied;
        const auto destination=geography.regions.at(deployment.region);
        const std::string id="F:ARMY:"+unit+":"+std::to_string(world.day_index());
        for(const auto& [stock_key,real_qty]:c.physical_inventory()) {
            if(lacking<=0)break;
            if(stock_key.holder_id!=arsenal||
               stock_key.good_id!="ammunition"||
               stock_key.region_id==deployment.region||real_qty<=0||
               !geography.regions.contains(stock_key.region_id))continue;
            if(c.military_control(stock_key.region_id)!=country)continue;
            const auto origin=geography.regions.at(stock_key.region_id);
            const auto stock_identity=std::make_tuple(
                arsenal,stock_key.region_id,std::string("ammunition"));
            const Quantity already=provisional.goods[stock_identity];
            const Quantity available=real_qty>already?real_qty-already:0;
            if(available<=0)continue;
            const phasegh::TransportEdge* best=nullptr;
            Quantity capacity=0;
            for(const auto& [edge_id,edge]:transport.transport_edges) {
                if(!edge.enabled||edge.from!=origin||edge.to!=destination||
                   edge.transit_days<=0||edge.mode=="air"||
                   edge.valid_from_month>=0&&gh.month()<edge.valid_from_month||
                   edge.valid_through_month>=0&&gh.month()>edge.valid_through_month)
                    continue;
                const Quantity used=transport.edge_usage.contains(
                    {gh.month(),edge_id})?
                    transport.edge_usage.at({gh.month(),edge_id}):0;
                const Quantity free=daily_budget::remaining(
                    edge.monthly_capacity,used,current.day,dim);
                const Quantity already_on_route=provisional.edge_units[edge_id];
                if(free<=already_on_route)continue;
                if(!best ||
                   std::tuple{edge.freight_per_unit,edge.transit_days,edge.id}<
                   std::tuple{best->freight_per_unit,best->transit_days,best->id}) {
                    best=&edge;
                    capacity=free-already_on_route;
                }
            }
            if(!best)continue;
            Quantity qty=std::min({lacking,available,capacity});
            const Money free_cash=std::max<Money>(0,
                c.cash(arsenal)-provisional.cash[arsenal]);
            if(best->carrier!=arsenal&&best->freight_per_unit>0)
                qty=std::min(qty,free_cash/best->freight_per_unit);
            if(qty<=0)continue;
            const Money freight=best->carrier==arsenal?0:
                phaseb::mul_checked(qty,best->freight_per_unit);
            provisional.goods[stock_identity]=phaseb::add_checked(already,qty);
            provisional.edge_units[best->id]=phaseb::add_checked(
                provisional.edge_units[best->id],qty);
            provisional.cash[arsenal]=phaseb::add_checked(
                provisional.cash[arsenal],freight);
            daily::Command command;
            command.id=id+":"+std::to_string(orders.size());
            command.kind=daily::Kind::cargo_dispatch;
            command.from=arsenal;
            command.to=unit;
            command.region=stock_key.region_id;
            command.destination=deployment.region;
            command.good="ammunition";
            command.route_id=best->id;
            command.amount=qty;
            command.unit_price=best->transit_days;
            command.auxiliary=0; // no unverified loss model
            orders.push_back(std::move(command));
            lacking-=qty;
        }
    }
    return orders;
}
} // namespace vic25::phasef
