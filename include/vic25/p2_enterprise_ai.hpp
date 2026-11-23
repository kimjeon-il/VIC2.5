#pragma once
// P2 enterprise investment decision support, read-only over canonical C/D/E/G.
#include "vic25/phase_gh.hpp"
#include "vic25/day_budget.hpp"
#include <algorithm>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace vic25::enterprise_ai {
using Money=phasec::Money;
using Quantity=vic25::Quantity;
using People=phaseb::People;
struct Quote {
    std::string recipe, owner, region, product;
    Money revenue_per_batch{}, inputs_per_batch{}, wages_per_batch{};
    Money freight_per_batch{}, margin_per_batch{};
    Quantity demand_batches{}, feasible_supply_batches{};
    bool valid{};
};
inline Money positive_mult(Money a,Quantity b) {
    return phaseb::mul_checked(a,b);
}
inline Money observed_wage(const phasec::IntegratedWorld& c,
                          const std::string& owner,const std::string& region,
                          int day,int days) {
    Money wage=0,regional=0;
    for(const auto& [slice,job]:c.people().jobs()) {
        if(job.count<=0||job.wage_per_month<=0)continue;
        const auto& demo=c.people().demographic(c.people().slice(slice).key.demographic);
        if(demo.region!=region)continue;
        const Money period=(day>0 && days>=28 && days<=31)?days:30;
        // Conservative forecast: the observed monthly wage is positive even
        // on dates whose actual payable rounded increment is temporarily 0.
        const Money daily=job.wage_per_month/period+
                          (job.wage_per_month%period!=0);
        regional=std::max(regional,daily);
        if(job.employer==owner)wage=std::max(wage,daily);
    }
    // Recruits are never free if a paid comparable occupation is observed.
    return wage>0?wage:regional;
}
inline Quantity regional_workers(const phasec::IntegratedWorld& c,
                                 const std::string& owner,const std::string& region) {
    Quantity available=0;
    for(const auto& [slice,job]:c.people().jobs()) {
        if(job.employer==owner&&job.count>0 &&
           c.people().demographic(c.people().slice(slice).key.demographic).region==region)
            available=phaseb::add_checked(available,job.count);
    }
    for(const auto& [id,pop]:c.people().slices()) {
        if(c.people().demographic(pop.key.demographic).region!=region ||
           !c.people().eligible_for(id,phaseb::JobRequirements{}))continue;
        const auto unused=pop.count-c.people().employed(id);
        if(unused>0)available=phaseb::add_checked(available,unused);
    }
    return available;
}
struct Supply {
    Money price_per_unit{};
    Money freight_per_unit{};
    Quantity actual_units{};
};
// An existing stock lot in this region is a credible short-run source. If it
// is elsewhere, require a dated, verified G route with spare physical capacity.
// This is an INVESTMENT QUOTE: no purchase/shipment is booked by looking it up.
inline std::optional<Supply> observed_supply(
        const phasec::IntegratedWorld& c,
        const phasee::SpatialState& spatial,
        const phasegh::TradeState& trade,
        const std::string& target,const std::string& good,
        Quantity needed, int day,int dim) {
    if(needed<=0)return std::nullopt;
    Quantity local=0;
    for(const auto& [key,units]:c.physical_inventory())
        if(key.region_id==target && key.good_id==good && units>0 &&
           c.has_actor(key.holder_id))
            local=phaseb::add_checked(local,units);
    if(local>=needed && c.has_market(target,good))
        return Supply{c.market(target,good).price,0,local};
    if(!spatial.regions.contains(target))return std::nullopt;
    std::optional<Supply> best;
    for(const auto& [key,units]:c.physical_inventory()) {
        if(key.good_id!=good||key.region_id==target||units<needed||
           !c.has_actor(key.holder_id)||
           !spatial.regions.contains(key.region_id)||
           !c.has_market(key.region_id,good)||
           c.sovereignty(target)!=c.sovereignty(key.region_id))continue;
        const auto quote=phasegh::quote_daily_freight_path(
            trade,spatial.regions.at(key.region_id),
            spatial.regions.at(target),c.month(),0,0,nullptr,true);
        if(!quote || quote->capacity<needed)continue;
        bool verified=true;
        for(const auto& edge_id:quote->edges) {
            const auto& edge=trade.transport_edges.at(edge_id);
            if(edge.mode=="legacy"||edge.evidence_id.empty()||
               edge.transit_days<=0)verified=false;
        }
        if(!verified)continue;
        const auto priced=phaseb::add_checked(
            c.market(key.region_id,good).price,quote->freight_per_unit);
        if(!best || priced<phaseb::add_checked(best->price_per_unit,
                                              best->freight_per_unit))
            best=Supply{c.market(key.region_id,good).price,
                        quote->freight_per_unit,units};
    }
    (void)day;(void)dim;
    return best;
}
inline Quote evaluate(const phasec::IntegratedWorld& c,
                      const phased::DomainState& domain,
                      const phasee::SpatialState& spatial,
                      const phasegh::TradeState& trade,
                      const phased::IndustryRecipe& recipe,
                      int day=0,int days=0,
                      const phased::ConstructionProject* planned=nullptr,
                      std::optional<std::int64_t> investment_today=std::nullopt) {
    Quote q;
    q.recipe=recipe.id;
    if(recipe.outputs.empty()||recipe.inputs.empty()||
       recipe.workers_per_batch<=0||recipe.capacity_per_batch<=0||
       recipe.needs_retrofit||recipe.requires_same_stage_nonstorable_energy)
        return q;
    const auto site=c.facilities().find(recipe.site);
    if(site==c.facilities().end() &&
       (!planned || planned->target_site!=recipe.site ||
        planned->ready || planned->commissioned ||
        planned->target_capacity<=0))return q;
    q.owner=site==c.facilities().end()?planned->client:site->second.owner;
    q.region=site==c.facilities().end()?planned->region:site->second.region;
    if(c.firm_bankrupt(q.owner))return q;
    if(!recipe.tech_id.empty()) {
        if(site==c.facilities().end())return q; // no free adoption in a planned site
        const auto tech=domain.active_site_tech.find(recipe.site);
        if(tech==domain.active_site_tech.end()||
           tech->second!=recipe.tech_id)return q;
    }
    // A real declared building must match this production recipe.
    if(const auto it=domain.site_archetypes.find(recipe.site);
       it!=domain.site_archetypes.end()) {
        const auto* archetype=building::find(it->second);
        const auto* profile=building::profile(it->second);
        if(!archetype||!profile||
           profile->operation!=building::Operation::recipe||
           archetype->industry!=recipe.industry_class)return q;
    }
    for(const auto& out:recipe.outputs) {
        if(!c.has_market(q.region,out.good))return q;
        q.revenue_per_batch=phaseb::add_checked(q.revenue_per_batch,
                          positive_mult(c.market(q.region,out.good).price,out.per_unit));
    }
    const auto& primary=recipe.outputs.front();
    q.product=primary.good;
    const auto& output_quote=c.market(q.region,primary.good);
    const auto previous=investment_today?c.previous_daily_solvent_unmet(
        q.region,primary.good,*investment_today):0;
    const Quantity shortage=std::max({
        output_quote.unmet,previous,
        output_quote.target_stock-c.stock(q.owner,q.region,primary.good)});
    q.demand_batches=shortage/primary.per_unit+
                     (shortage%primary.per_unit?1:0);
    if(q.demand_batches<=0)return q;
    // evaluate also accepts drafts not yet validated by Phase D.
    if(std::any_of(recipe.inputs.begin(),recipe.inputs.end(),
                   [](const phased::Input& row){return row.per_unit<=0;}))
        return q;
    for(const auto& in:phased::recipe_input_totals(recipe)) {
        const auto supply=observed_supply(c,spatial,trade,
                          q.region,in.good,in.per_unit,day,days);
        if(!supply)return q;
        q.inputs_per_batch=phaseb::add_checked(q.inputs_per_batch,
             positive_mult(supply->price_per_unit,in.per_unit));
        q.freight_per_batch=phaseb::add_checked(q.freight_per_batch,
             positive_mult(supply->freight_per_unit,in.per_unit));
        if(q.feasible_supply_batches==0)q.feasible_supply_batches=std::numeric_limits<Quantity>::max();
        q.feasible_supply_batches=std::min(q.feasible_supply_batches,
                                           supply->actual_units/in.per_unit);
    }
    const Money wage=observed_wage(c,q.owner,q.region,day,days);
    if(wage<=0 || regional_workers(c,q.owner,q.region)<recipe.workers_per_batch)
        return q; // unknown wage or genuinely absent recruitment pool
    q.wages_per_batch=positive_mult(wage,recipe.workers_per_batch);
    const Money costs=phaseb::add_checked(phaseb::add_checked(
        q.inputs_per_batch,q.freight_per_batch),q.wages_per_batch);
    q.margin_per_batch=q.revenue_per_batch-costs;
    q.valid=q.margin_per_batch>0 && q.feasible_supply_batches>0;
    return q;
}
// Draw only an existing, fully pledged G line. No speculative bank
// lending or new balance-sheet replica is authorized by the AI.
inline Money draw_documented_credit(phasegh::Stage& gh,
                                   const std::string& borrower,Money requested,
                                   const std::string& spending_region="",
                                   const std::string& spending_good="",
                                   Quantity required_spending_units=0) {
    auto& c=gh.phase_c();
    if(requested<=0 || c.bank_insolvent() ||
       !c.has_actor(borrower) || c.firm_bankrupt(borrower))return 0;
    Money borrowed=0;
    for(const auto& [id,line]:gh.commerce().credit) {
        if(borrowed>=requested)break;
        if(!line.active||line.borrower!=borrower ||
           line.effective>c.month()||line.drawn>=line.limit)continue;
        const auto keep_free=line.pledge_region==spending_region&&
            line.pledge_good==spending_good?required_spending_units:0;
        if(!gh.trade_credit_collateral_available(id,keep_free))continue;
        const Money draw=std::min(requested-borrowed,line.limit-line.drawn);
        if(draw<=0)continue;
        gh.draw_trade_credit(id,draw);
        borrowed=phaseb::add_checked(borrowed,draw);
    }
    return borrowed;
}
inline Money approved_credit_remaining(const phasec::IntegratedWorld& c,
                                        const phasegh::TradeState& trade,
                                        const std::string& owner) {
    Money total=0;
    for(const auto& [id,line]:trade.credit) {
        (void)id;
        if(!line.active||line.borrower!=owner||
           line.effective>c.month()||line.drawn>=line.limit||
           c.stock(owner,line.pledge_region,line.pledge_good)<line.pledged_goods)
            continue;
        total=phaseb::add_checked(total,line.limit-line.drawn);
    }
    return total;
}
} // namespace vic25::enterprise_ai
