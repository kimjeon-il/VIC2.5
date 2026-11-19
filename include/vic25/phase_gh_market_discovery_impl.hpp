#pragma once
// Autonomous merchant market search. Intentionally included after Phase G/H
// implementation: all money, physical stock, legal quota and logistics
// reservations continue to be owned by canonical Phase C/G/H.
namespace vic25::phasegh {
inline void Stage::plan_market_discovery() {
    at(1);
    if(day_of_month_<=0 || trade_.transport_edges.empty()) {
        trade_.daily_merchant_offices.clear();
        return;
    }
    auto& c=phase_c();
    const auto& geo=spatial_.spatial();
    // Merchant offices may be scenario-specified, but never need a registered
    // Route, origin market, good, supplier or freight carrier in advance.
    std::map<std::pair<std::string,std::string>,MerchantOffice> offices;
    for(const auto& [site,f]:c.facilities()) {
        (void)site;
        if(!c.firm_bankrupt(f.owner)&&!c.has_government(f.owner)&&
           geo.regions.contains(f.region))
            offices.try_emplace({f.owner,f.region},
                MerchantOffice{f.owner,f.region,100,0,50'000});
    }
    for(const auto& office:trade_.daily_merchant_offices)
        offices[{office.trader,office.market_region}]=office;
    trade_.daily_merchant_offices.clear();
    struct Source {std::string seller,region,good;};
    // Group real seller holdings by commodity once per day.
    std::map<std::string,std::vector<Source>> supply;
    for(const auto& [stock_key,qty]:c.physical_inventory()) {
        if(qty<=0||!c.has_actor(stock_key.holder_id)||
           c.firm_bankrupt(stock_key.holder_id)||
           stock_key.holder_id.starts_with("HH:")||
           !geo.regions.contains(stock_key.region_id)||
           !c.has_market(stock_key.region_id,stock_key.good_id))
            continue;
        supply[stock_key.good_id].push_back({
            stock_key.holder_id,stock_key.region_id,stock_key.good_id});
    }
    for(const auto& [key,merchant]:offices) {
        (void)key;
        if(!c.has_actor(merchant.trader)||
           c.firm_bankrupt(merchant.trader))continue;
        const Money free=c.phase_d_cash_available(merchant.trader);
        const Money reserve=std::max(merchant.minimum_cash_reserve,free/4);
        if(free<=reserve)continue;
        const auto destination=geo.regions.at(merchant.market_region);
        // Choose the highest observed unit margin, tie-broken by shorter
        // documented delivery and stable source IDs. One order per office
        // and calendar day bounds path searches and speculative repeat buys.
        bool found=false;
        Source chosen{};
        PathQuote path_chosen{};
        Money price_chosen=0,duty_chosen=0,spread_chosen=0;
        Quantity quantity_chosen=0;
        std::string exporting,importing;
        std::map<std::string,std::optional<PathQuote>> path_quotes;
        for(const auto& [good,offer_group]:supply) {
            if(!c.has_market(merchant.market_region,good)||
               c.market(merchant.market_region,good).target_stock<=0)
                continue;
            for(const auto& offer:offer_group) {
            if(offer.seller==merchant.trader||
               offer.region==merchant.market_region)continue;
            const auto& source_price=c.market(offer.region,offer.good);
            const auto& dest_price=c.market(merchant.market_region,offer.good);
            if(source_price.price<=0||dest_price.price<=source_price.price||
               dest_price.target_stock<=0)continue;
            Quantity en_route=0;
            for(const auto& [id,shipment]:trade_.daily_shipments) {
                (void)id;
                if(shipment.buyer==merchant.trader&&
                   shipment.destination==merchant.market_region&&
                   shipment.good==offer.good)
                    en_route=phaseb::add_checked(en_route,
                               shipment.quantity-shipment.loss);
            }
            for(const auto& p:trade_.pending)
                if(p.buyer==merchant.trader&&
                   p.destination_region==merchant.market_region&&
                   p.good==offer.good)
                    en_route=phaseb::add_checked(en_route,p.quantity);
            const Quantity held=c.phase_d_material_available(
                merchant.trader,merchant.market_region,offer.good);
            if(held>=dest_price.target_stock||
               en_route>=dest_price.target_stock-held)continue;
            const Quantity gap=dest_price.target_stock-held-en_route;
            const Quantity actual_stock=c.phase_d_material_available(
                offer.seller,offer.region,offer.good);
            if(actual_stock<=0)continue;
            auto cached_path=path_quotes.find(offer.region);
            if(cached_path==path_quotes.end())
                cached_path=path_quotes.emplace(
                    offer.region,quote_daily_freight_path(
                        trade_,geo.regions.at(offer.region),destination,c.month(),
                        day_of_month_,days_in_month_,nullptr,true,nullptr,nullptr,true))
                    .first;
            const auto& candidate=cached_path->second;
            if(!candidate||candidate->edges.empty()||
               candidate->capacity<=0)continue;
            bool documented=true;
            for(const auto& edge_id:candidate->edges) {
                const auto& e=trade_.transport_edges.at(edge_id);
                if(e.mode=="legacy"||e.evidence_id.empty()||
                   e.transit_days<=0||e.valid_from_month<0||
                   c.month()<e.valid_from_month||
                   c.month()>e.valid_through_month) {
                    documented=false;break;
                }
            }
            if(!documented)continue;
            const auto& first=trade_.transport_edges.at(candidate->edges.front());
            if(first.carrier==merchant.trader&&first.freight_per_unit>0)
                continue; // no self-billed freight on existing G settlement
            const auto from=c.sovereignty(offer.region);
            const auto to=c.sovereignty(merchant.market_region);
            const Money rate=duty_rate(from,to,offer.good);
            const Money tax_per_unit=phaseb::add_checked(
                phaseb::mul_checked(source_price.price/1'000'000,rate),
                (phaseb::mul_checked(source_price.price%1'000'000,rate)+
                999'999)/1'000'000);
            const Money landed=phaseb::add_checked(
                phaseb::add_checked(source_price.price,tax_per_unit),
                candidate->freight_per_unit);
            if(landed<=0||landed>=dest_price.price)continue;
            const Money margin=dest_price.price-landed;
            const Money minimum_spread=phaseb::add_checked(
                phaseb::mul_checked(dest_price.price/1'000'000,
                    merchant.minimum_margin_ppm),
                phaseb::mul_checked(dest_price.price%1'000'000,
                    merchant.minimum_margin_ppm)/1'000'000);
            if(margin<minimum_spread)continue;
            const Quantity available=lawful_trade_quantity(from,to,offer.good,
                std::min({gap,actual_stock,candidate->capacity,
                     merchant.maximum_daily_units,(free-reserve)/landed}));
            if(available<=0)continue;
            if(!found||std::tuple{margin,-candidate->travel_days,
                                  offer.seller,offer.region,offer.good}>
                       std::tuple{spread_chosen,-path_chosen.travel_days,
                                  chosen.seller,chosen.region,chosen.good}) {
                found=true;chosen=offer;path_chosen=*candidate;
                quantity_chosen=available;price_chosen=source_price.price;
                duty_chosen=rate;spread_chosen=margin;
                exporting=from;importing=to;
            }
            }
        }
        if(!found||quantity_chosen<=0)continue;
        const Money goods_cost=phaseb::mul_checked(quantity_chosen,price_chosen);
        const Money duty=phaseb::mul_checked(goods_cost,duty_chosen)/1'000'000;
        const Money freight=phaseb::mul_checked(
            quantity_chosen,path_chosen.freight_per_unit);
        const Money cost=phaseb::add_checked(
            goods_cost,phaseb::add_checked(duty,freight));
        if(cost<=0||c.phase_d_cash_available(merchant.trader)<
            phaseb::add_checked(cost,reserve))continue;
        const std::string eid="G:DISCOVERY:"+std::to_string(day_index_)+":"+
           merchant.trader+":"+merchant.market_region+":"+chosen.good;
        if(trade_.executed.contains(eid))continue;
        c.phase_d_reserve_cash(merchant.trader,eid,cost);
        c.phase_d_reserve_material(chosen.seller,chosen.region,chosen.good,
                                   eid,quantity_chosen);
        reserve_trade_quota(exporting,importing,chosen.good,quantity_chosen);
        const auto edge_id=path_chosen.edges.front();
        const auto edge_key=std::make_pair(c.month(),edge_id);
        trade_.edge_usage[edge_key]=phaseb::add_checked(
            trade_.edge_usage[edge_key],quantity_chosen);
        trade_.pending.push_back({
            eid,eid,chosen.seller,merchant.trader,chosen.good,
            chosen.region,merchant.market_region,exporting,importing,
            quantity_chosen,path_chosen.travel_months,price_chosen,cost,duty,
            freight,"","TEST",path_chosen.edges,0});
    }
}
} // namespace vic25::phasegh
