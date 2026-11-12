#pragma once
// 1.15 Goods: per-commodity production leaderboard prestige factors and
// 0.25 + 2.75*(GlobalGDP/10bn) scaling. Never rank unknown world output.
#include <map>
#include <set>
#include <vector>
namespace vic25::phasec {
namespace p1goods {
inline const std::map<std::string,std::int32_t>& factors_milli() {
    static const std::map<std::string,std::int32_t> data={
        {"ammunition",5000},
        {"small_arms",5000},
        {"artillery",5000},
        {"tanks",10000},
        {"aeroplanes",10000},
        {"manowars",5000},
        {"ironclads",10000},
        {"grain",3000},
        {"fish",3000},
        {"fabric",3000},
        {"wood",3000},
        {"groceries",4000},
        {"clothes",4000},
        {"furniture",4000},
        {"paper",4000},
        {"merchant_marine",8000},
        {"clippers",4000},
        {"steamers",6000},
        {"silk",6000},
        {"dye",6000},
        {"sulfur",3000},
        {"coal",3000},
        {"iron",3000},
        {"lead",3000},
        {"copper",3000},
        {"hardwood",4000},
        {"rubber",4000},
        {"oil",6000},
        {"engines",8000},
        {"steel",5000},
        {"glass",5000},
        {"fertilizer",3000},
        {"tools",5000},
        {"explosives",6000},
        {"porcelain",8000},
        {"meat",4000},
        {"fruit",4000},
        {"liquor",4000},
        {"wine",6000},
        {"tea",6000},
        {"coffee",6000},
        {"sugar",4000},
        {"tobacco",4000},
        {"opium",2000},
        {"automobiles",10000},
        {"telephones",10000},
        {"radios",10000},
        {"luxury_clothes",8000},
        {"luxury_furniture",8000},
        {"gold",5000},
        {"fine_art",10000},
    };
    return data;
}
}
inline P1GoodsLeadershipView IntegratedWorld::p1_goods_leadership(
        const std::string& country,std::int64_t from,
        std::int64_t through)const {
    ensure(governments_.contains(country)&&from>=0&&through>=from,
           "P1 goods prestige needs real country and GDP interval");
    P1GoodsLeadershipView view;
    view.country=country;
    std::map<std::string,std::map<std::string,Quantity>> output;
    std::map<std::string,std::set<std::int64_t>> months;
    for(const auto& e:p1_value_added_) {
        if(e.month<from||e.month>through)continue;
        months[e.country].insert(e.month);
        for(const auto& [good,q]:e.produced_goods)
            output[good][e.country]=phaseb::add_checked(
                output[good][e.country],q);
    }
    bool complete=through-from+1==12;
    std::int64_t world_gdp=0;
    for(const auto& [nation,gov]:governments_) {
        (void)gov;
        const auto gdp=p1_gdp(nation,from,through);
        if(!gdp.complete||months[nation].size()!=12) {
            complete=false;
            continue;
        }
        world_gdp=phaseb::add_checked(world_gdp,
            std::max<std::int64_t>(0,gdp.value_added));
    }
    view.complete=complete;
    if(!complete)return view;
    // Preserve sub-point contributions from multiple small commodity
    // leaders. Integer rounding is applied once to the national total,
    // not once per good (which could erase all low-GDP leader prestige).
    std::int64_t summed_prestige_milli=0;
    const auto scale=phaseb::add_checked(250'000,
        phaseb::mul_checked(
            std::min<std::int64_t>(world_gdp,10'000'000'000LL),
            2'750'000)/10'000'000'000LL);
    for(const auto& [good,factor]:p1goods::factors_milli()) {
        auto candidates=output.find(good);
        if(candidates==output.end())continue;
        std::vector<std::pair<std::string,Quantity>> ordered{
            candidates->second.begin(),candidates->second.end()};
        std::sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){
            return a.second!=b.second?a.second>b.second:a.first<b.first;
        });
        int rank=0;
        for(const auto& [producer,amount]:ordered) {
            if(amount<=0)continue;
            if(++rank>3)break;
            if(producer!=country)continue;
            const auto multiplier=rank==1?4:rank==2?2:1;
            const auto raw=phaseb::mul_checked(factor,multiplier);
            summed_prestige_milli=phaseb::add_checked(
                summed_prestige_milli,
                phaseb::mul_checked(raw,scale)/1'000'000);
            view.rank.emplace(good,rank);
            view.observed=true;
        }
    }
    view.prestige=summed_prestige_milli/1000;
    return view;
}
} // namespace vic25::phasec
