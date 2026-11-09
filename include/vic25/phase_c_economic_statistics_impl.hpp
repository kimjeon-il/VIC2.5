#pragma once
// One GDP event per actual material transformation/extraction/paid construction.
// Physical production writes only its original goods/finance transaction;
// the valuation here is a deterministic statistical readout at the price
// observable at that time. Cross-border trade does NOT generate GDP again.
namespace vic25::phasec {
inline void IntegratedWorld::p1_observe_value_added(
        const std::string& source,const std::string& region,
        const std::vector<std::pair<std::string,Quantity>>& inputs,
        const std::vector<std::pair<std::string,Quantity>>& outputs) {
    require_stage({4,5});
    ensure(!source.empty()&&sovereignty_.contains(region)&&
           !outputs.empty(),"GDP observation requires real production");
    P1ValueAddedEvent row;
    row.source=source;
    row.region=region;
    row.country=sovereignty_.at(region);
    row.month=month();
    row.day=people_.native_calendar_day();
    row.fully_priced=true;
    const auto price=[&](const std::string& good,Quantity amount,
                         std::int64_t& target) {
        ensure(!good.empty()&&amount>0,"GDP requires positive observed goods");
        const auto it=markets_.find({region,good});
        if(it==markets_.end()||it->second.price<=0) {
            row.fully_priced=false;
            return;
        }
        target=phaseb::add_checked(target,
            phaseb::mul_checked(amount,it->second.price));
    };
    for(const auto& [good,amount]:inputs)
        price(good,amount,row.intermediate_input);
    for(const auto& [good,amount]:outputs)
        price(good,amount,row.gross_output);
    p1_value_added_.push_back(std::move(row));
}
inline void IntegratedWorld::phase_d_record_paid_construction_value(
        const std::string& project,const std::string& region,
        const std::string& material,Quantity consumed,
        Money actual_payment) {
    require_stage({5});
    ensure(!project.empty()&&!region.empty()&&
           !material.empty()&&consumed>0&&actual_payment>0&&
           sovereignty_.contains(region),
           "GDP construction requires real paid project and materials");
    P1ValueAddedEvent row;
    row.source="paid-construction:"+project;
    row.country=sovereignty_.at(region);
    row.region=region;
    row.month=month();
    row.day=people_.native_calendar_day();
    row.gross_output=actual_payment;
    const auto quote=markets_.find({region,material});
    row.fully_priced=quote!=markets_.end()&&quote->second.price>0;
    if(row.fully_priced)
        row.intermediate_input=phaseb::mul_checked(
            consumed,quote->second.price);
    p1_value_added_.push_back(std::move(row));
}
inline P1GdpView IntegratedWorld::p1_gdp(
        const std::string& country,std::int64_t from_month,
        std::int64_t through_month)const {
    ensure(governments_.contains(country)&&from_month>=0&&
           through_month>=from_month,
           "GDP projection requires real country and valid observation period");
    P1GdpView result;
    result.country=country;
    result.from_month=from_month;
    result.through_month=through_month;
    for(const auto& row:p1_value_added_) {
        if(row.country!=country||row.month<from_month||
           row.month>through_month)continue;
        ++result.observed_events;
        if(!row.fully_priced) {
            ++result.unpriced_events;
            continue;
        }
        result.output=phaseb::add_checked(result.output,row.gross_output);
        result.inputs=phaseb::add_checked(
            result.inputs,row.intermediate_input);
    }
    result.observed=result.observed_events>0;
    // A negative industry value-added remains visible rather than being
    // converted into positive revenue or hidden in state treasury cash.
    result.value_added=phaseb::add_checked(result.output,-result.inputs);
    result.complete=result.observed&&result.unpriced_events==0;
    return result;
}
} // namespace vic25::phasec
