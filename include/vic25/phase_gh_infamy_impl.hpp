#pragma once
// H is the one authoritative Infamy register. Existing crises and wars are
// the source of adverse diplomatic events; C prestige and Influence determine
// rate modifiers but are not duplicated as spendable currencies.
namespace vic25::phasegh {
inline void Stage::record_infamy(
        const std::string& event_id,const std::string& country,
        const std::string& source,std::int64_t amount_ppm) {
    at(9);
    require(day_index_>=0&&!event_id.empty()&&!source.empty()&&
            amount_ppm>0&&amount_ppm<=1'000'000'000&&
            !diplomacy_.infamy_ids.contains(event_id),
            "H INFAMY requires unique actual dated diplomatic incident");
    (void)phase_c().government(country);
    auto& state=diplomacy_.infamy[country];
    state.value_ppm=std::min<std::int64_t>(1'000'000'000,
        phaseb::add_checked(state.value_ppm,amount_ppm));
    if(state.last_day<0)state.last_day=day_index_;
    diplomacy_.infamy_events.push_back({
        event_id,country,source,amount_ppm,day_index_});
    diplomacy_.infamy_ids.insert(event_id);
    ++diplomacy_.revision;
}
inline void Stage::phase_time_infamy_decay(
        std::int64_t today,int days_in_year,
        const std::map<std::string,std::int32_t>& diplomatic_bonuses) {
    at(0);
    require(today>=0&&(days_in_year==365||days_in_year==366),
            "H daily Infamy requires valid Gregorian calendar");
    for(auto& [country,state]:diplomacy_.infamy) {
        if(state.last_day<0){state.last_day=today;continue;}
        require(today>=state.last_day,
                "H Infamy cannot process days out of order");
        if(today==state.last_day)continue;
        require(today==state.last_day+1,
                "H Infamy calendar gap must not silently lose decay");
        const auto& core=phase_c();
        if(!core.has_government(country))continue;
        const auto legal=phaseb::add_checked(
            core.p2_modifier_ppm(country,"country_infamy_decay_mult"),
            core.p3_trait_modifier_ppm(country,"country_infamy_decay_mult"));
        const auto bonus=diplomatic_bonuses.find(country);
        const auto influence=bonus==diplomatic_bonuses.end()?0:bonus->second;
        const auto multiplier=std::clamp<std::int64_t>(
            1'000'000+phaseb::add_checked(legal,influence),0,5'000'000);
        // Victoria 3 1.15 base: 5 Infamy points per year.
        const auto annual=phaseb::mul_checked(5'000'000,multiplier)/
                          1'000'000;
        const auto numerator=phaseb::add_checked(annual,state.carry);
        const auto decrease=numerator/days_in_year;
        state.carry=numerator%days_in_year;
        state.value_ppm=std::max<std::int64_t>(
            0,state.value_ppm-decrease);
        if(state.value_ppm==0)state.carry=0;
        state.last_day=today;
    }
}
inline InfamyView World::infamy_view(const std::string& country)const {
    (void)core_.core().core().government(country);
    InfamyView result;
    result.country=country;
    auto it=diplomacy_.infamy.find(country);
    if(it==diplomacy_.infamy.end())return result;
    result.value_ppm=it->second.value_ppm;
    result.last_day=it->second.last_day;
    // The tier effects are straight from Victoria 3 1.15 static modifiers;
    // exposure is a lawful H view, not a fictitious POP radicalization event.
    if(result.value_ppm>100'000'000) {
        result.tier=InfamyTier::pariah;
        result.conquest_radicals_ppm=1'000'000;
        result.separatism_resistance_ppm=-500'000;
    } else if(result.value_ppm>50'000'000) {
        result.tier=InfamyTier::notorious;
        result.conquest_radicals_ppm=500'000;
        result.separatism_resistance_ppm=-250'000;
    } else if(result.value_ppm>25'000'000) {
        result.tier=InfamyTier::infamous;
        result.conquest_radicals_ppm=250'000;
        result.separatism_resistance_ppm=-100'000;
    }
    return result;
}
} // namespace vic25::phasegh
