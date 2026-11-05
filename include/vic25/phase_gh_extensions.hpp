#pragma once
// Included only by phase_gh.hpp, AFTER class Stage and World definitions.
// These methods never own a second Money/Inventory ledger.

namespace vic25::phasegh {

inline std::optional<PathQuote> Stage::quote_path(
        Id from, Id to, Month month) const {
    if(from==to)return PathQuote{0,0,std::numeric_limits<Quantity>::max(),{}};
    if(trade_.transport_edges.empty())return std::nullopt;
    using Item=std::tuple<Money,Month,Id>;
    std::priority_queue<Item,std::vector<Item>,std::greater<Item>> queue;
    std::map<Id,std::pair<Money,Month>> best;
    std::map<Id,std::pair<Id,std::string>> parent;
    best[from]={0,0};
    queue.push({0,0,from});
    while(!queue.empty()) {
        const auto [cost,travel,node]=queue.top();
        queue.pop();
        if(best.at(node)!=std::pair<Money,Month>{cost,travel})continue;
        if(node==to)break;
        auto it=trade_.transport_adjacency.find(node);
        if(it==trade_.transport_adjacency.end())continue;
        // World::seed_transport_edge inserts in deterministic ID order.
        for(const auto& edge_id:it->second) {
            const auto& edge=trade_.transport_edges.at(edge_id);
            const Quantity used=trade_.edge_usage.contains({month,edge_id})?
                                trade_.edge_usage.at({month,edge_id}):0;
            const Quantity ceiling=day_of_month_>0?
                day_budget(edge.monthly_capacity,0):edge.monthly_capacity;
            if(!edge.enabled || used>=ceiling)continue;
            require(edge.from==node && used>=0,
                    "G_ERR_LOGISTICS_INDEX_CORRUPT");
            const Money next_cost=phaseb::add_checked(cost,edge.freight_per_unit);
            const Month next_travel=phaseb::add_checked(travel,edge.transit_months);
            const std::pair<Money,Month> candidate{next_cost,next_travel};
            auto old=best.find(edge.to);
            if(old==best.end() || candidate<old->second) {
                best[edge.to]=candidate;
                parent[edge.to]={node,edge_id};
                queue.push({next_cost,next_travel,edge.to});
            }
        }
    }
    if(!best.contains(to))return std::nullopt;
    PathQuote result;
    result.freight_per_unit=best.at(to).first;
    result.travel_months=best.at(to).second;
    result.capacity=std::numeric_limits<Quantity>::max();
    for(Id p=to;p!=from;) {
        auto it=parent.find(p);
        require(it!=parent.end(),"G_ERR_LOGISTICS_PARENT_MISSING");
        const auto& edge=trade_.transport_edges.at(it->second.second);
        const Quantity used=trade_.edge_usage.contains({month,edge.id})?
                                trade_.edge_usage.at({month,edge.id}):0;
        const Quantity ceiling=day_of_month_>0?
            day_budget(edge.monthly_capacity,0):edge.monthly_capacity;
        require(used<ceiling,"G_ERR_LOGISTICS_CAPACITY");
        result.capacity=std::min(result.capacity,ceiling-used);
        result.edges.push_back(edge.id);
        p=it->second.first;
    }
    std::reverse(result.edges.begin(),result.edges.end());
    return result;
}
inline void Stage::create_trade_contract(TradeContract c) {
    at(9);
    require(!c.id.empty()&&!c.seller.empty()&&!c.buyer.empty()&&
            c.seller!=c.buyer&&!c.good.empty()&&!c.currency.empty()&&
            c.origin>0&&c.destination>0&&c.contracted>0&&c.unit_price>0&&
            c.monthly_limit>0&&c.expires>=phase_c().month()+1&&
            c.effective<=c.expires&&!trade_.contracts.contains(c.id)&&
            spatial_.spatial().provinces.contains(c.origin)&&
            spatial_.spatial().provinces.contains(c.destination),
            "G_ERR_INVALID_CONTRACT");
    // Both legal counterparties must own real C financial identities.
    (void)phase_c().cash(c.seller);
    (void)phase_c().cash(c.buyer);
    c.effective=std::max(c.effective,phase_c().month()+1);
    trade_.contracts.emplace(c.id,std::move(c));
    ++trade_.revision;
}
inline void Stage::offer_trade_credit(CreditFacility credit) {
    at(9);
    require(!credit.id.empty()&&!credit.borrower.empty()&&
            !credit.pledge_region.empty()&&!credit.pledge_good.empty()&&
            credit.pledged_goods>0&&credit.limit>0&&credit.drawn==0&&
            !trade_.credit.contains(credit.id),"G_ERR_INVALID_CREDIT");
    (void)phase_c().cash(credit.borrower);
    const auto stock=phase_c().stock(credit.borrower,
                                     credit.pledge_region,credit.pledge_good);
    const auto price=phase_c().market(credit.pledge_region,
                                      credit.pledge_good).price;
    const Money value=phaseb::mul_checked(credit.pledged_goods,price);
    require(credit.limit<=value,"G_ERR_UNSECURED_CREDIT");
    Quantity pledged=0;
    for(const auto& [id,old]:trade_.credit) {
        (void)id;
        if(old.active&&old.borrower==credit.borrower&&
           old.pledge_region==credit.pledge_region&&
           old.pledge_good==credit.pledge_good)
           pledged=phaseb::add_checked(pledged,old.pledged_goods);
    }
    require(credit.pledged_goods<=stock-pledged,
            "G_ERR_COLLATERAL_DOUBLE_PLEDGE");
    credit.effective=phase_c().month()+1;
    trade_.credit.emplace(credit.id,std::move(credit));
    ++trade_.revision;
}
inline void Stage::reserve_collateral() {
    at(1);
    for(const auto& [id,line]:trade_.credit) {
        if(!line.active||line.effective>phase_c().month()||line.drawn==0)
            continue;
        phase_c().phase_d_reserve_material(line.borrower,line.pledge_region,
           line.pledge_good,"G-COLLATERAL:"+id,line.pledged_goods);
    }
}
inline void Stage::draw_trade_credit(const std::string& id,Money amount) {
    at(1);
    auto& line=trade_.credit.at(id);
    require(line.active&&line.effective<=phase_c().month()&&amount>0&&
            line.drawn<=line.limit && amount<=line.limit-line.drawn,
            "G_ERR_CREDIT_LIMIT");
    const auto actual_stock=phase_c().stock(line.borrower,line.pledge_region,
                                             line.pledge_good);
    require(actual_stock>=line.pledged_goods,"G_ERR_MISSING_COLLATERAL");
    // C owns both the BANK loan asset and the matched borrower deposit/debt.
    phase_c().lend(line.borrower,amount);
    line.drawn=phaseb::add_checked(line.drawn,amount);
    ++trade_.revision;
}
inline void Stage::repay_trade_credit(const std::string& id,Money amount) {
    at(8);
    auto& line=trade_.credit.at(id);
    require(line.active&&amount>0&&amount<=line.drawn,
            "G_ERR_INVALID_CREDIT_REPAYMENT");
    phase_c().repay(line.borrower,amount);
    line.drawn-=amount;
    ++trade_.revision;
}
inline void Stage::insure_cargo(CargoInsurance policy) {
    at(9);
    require(!policy.id.empty()&&!policy.shipment_id.empty()&&
            !policy.insurer.empty()&&!policy.beneficiary.empty()&&
            policy.insurer!=policy.beneficiary&&policy.premium>0&&
            policy.maximum_claim>0&&policy.insured_quantity>0&&
            !trade_.insurance.contains(policy.id),"G_ERR_INVALID_INSURANCE");
    (void)phase_c().cash(policy.insurer);
    (void)phase_c().cash(policy.beneficiary);
    trade_.insurance.emplace(policy.id,std::move(policy));
    ++trade_.revision;
}
inline void Stage::register_arrived_losses() {
    at(0);
    for(auto& [id,policy]:trade_.insurance) {
        (void)id;
        if(!policy.premium_paid||policy.claim_registered)continue;
        const auto& ship=phase_c().phase_g_shipment(policy.shipment_id);
        if(!ship.delivered)continue;
        require(ship.quantity>=policy.insured_quantity&&ship.loss<=ship.quantity,
                "G_ERR_INVALID_INSURANCE_EXPOSURE");
        policy.claim_due=phaseb::mul_checked(policy.maximum_claim,
                       std::min(ship.loss,policy.insured_quantity))/
                       policy.insured_quantity;
        policy.claim_registered=true;
        ++trade_.revision;
    }
}
inline void Stage::pay_insurance_premiums() {
    at(7);
    for(auto& [id,policy]:trade_.insurance) {
        (void)id;
        if(policy.premium_paid||!trade_.executed.contains(policy.shipment_id))
            continue;
        // The insurer receives actual money; coverage is a legal promise,
        // never a free cash asset for the beneficiary.
        phase_c().pay(policy.beneficiary,policy.insurer,policy.premium);
        policy.premium_paid=true;
        ++trade_.revision;
    }
}
inline void Stage::settle_insurance_claims() {
    at(5);
    for(auto& [id,policy]:trade_.insurance) {
        (void)id;
        if(!policy.claim_registered||policy.claim_paid>=policy.claim_due)
            continue;
        const Money amount=std::min(policy.claim_due-policy.claim_paid,
                                     phase_c().phase_d_cash_available(policy.insurer));
        if(amount<=0)continue; // valid insurance arrears, not a fictitious payment
        phase_c().pay(policy.insurer,policy.beneficiary,amount);
        policy.claim_paid=phaseb::add_checked(policy.claim_paid,amount);
        ++trade_.revision;
    }
}
inline void Stage::exchange_currencies(const std::string& a,
                const std::string& b,const std::string& sold,Money sold_amount,
                const std::string& bought,Money bought_amount) {
    at(7);
    phase_c().exchange_fx(a,b,sold,sold_amount,bought,bought_amount);
}

} // namespace vic25::phasegh
