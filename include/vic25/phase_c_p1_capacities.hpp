#pragma once
// Phase C-P1 capacity and country-standing contracts, based on Victoria 3 1.15.
// Unlike money, Authority/Influence are concurrent flows: production minus
// ACTIVE commitments. Prestige is a derived international ranking INPUT, not
// spendable currency. All referenced actors, IGs, territories and enterprises
// remain in the original C/D/E/G/H ledgers.
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace vic25::phasec {
enum class P1CountryTier : std::uint8_t {
    city_state, principality, grand_principality, kingdom, empire, hegemony
};
enum class P1PowerRank : std::uint8_t {
    unobserved, decentralized_power, unrecognized_power,
    insignificant_power, unrecognized_regional_power, minor_power,
    unrecognized_major_power, major_power, great_power
};
struct P1StandingEvidence {
    std::string country,source;
    P1CountryTier tier{P1CountryTier::kingdom};
    bool recognized{true};
    P1PowerRank confirmed_rank{P1PowerRank::unobserved};
    // Published scenario/statistic observations, NEVER fabricated GDP from
    // cash balances or duplicated military personnel in a prestige ledger.
    std::int64_t own_gdp{},subject_gdp{},army_projection{},navy_projection{};
    std::int64_t goods_leadership{},monuments{},other_prestige{};
    std::int64_t observed_day{-1};
    bool complete{}; // all required inputs individually source-qualified
};
struct P1PrestigeView {
    std::string country,source;
    std::int64_t observed_day{-1};
    std::int64_t tier{},own_gdp{},subject_gdp{},army{},navy{},goods{},monuments{},other{};
    std::int64_t partial_total{},total{};
    bool observed{},complete{};
    P1PowerRank rank{P1PowerRank::unobserved};
    bool rank_confirmed{};
};
struct P1FlowCapacity {
    std::int64_t generated{},committed{},balance{};
    // 0..1,000,000: 2x generation compared with commitment provides the
    // maximum excess bonus; 2x commitment compared with generation provides
    // the maximum shortage penalty (Victoria 3 1.15 static modifiers).
    std::int32_t excess_ppm{},shortage_ppm{};
    bool complete{true}; // false if any existing commitment lacks a known cost
};
enum class P1MandateKind : std::uint8_t {
    decree_road_maintenance, decree_promote_social_mobility,
    bolster_interest_group, suppress_interest_group
};
struct P1Mandate {
    std::string id,country,target,legal_basis;
    P1MandateKind kind{P1MandateKind::decree_road_maintenance};
    std::int64_t adopted_day{},effective_day{};
};
struct P1MandateChange {
    std::string id;
    P1Mandate mandate;
    bool terminate{};
};
namespace p1power {
inline constexpr std::int64_t capacity_base=100;
inline constexpr std::int64_t decree_cost=100;
inline constexpr std::int64_t ig_action_cost=200;
inline std::int64_t rank_influence(P1PowerRank rank) {
    switch(rank) {
        case P1PowerRank::great_power:return 1000;
        case P1PowerRank::major_power:
        case P1PowerRank::unrecognized_major_power:return 750;
        case P1PowerRank::minor_power:
        case P1PowerRank::unrecognized_regional_power:return 600;
        case P1PowerRank::insignificant_power:
        case P1PowerRank::unrecognized_power:return 500;
        case P1PowerRank::decentralized_power:return 0;
        case P1PowerRank::unobserved:return 0;
    }
    return 0;
}
inline std::int64_t prestige_tier(P1CountryTier tier) {
    switch(tier) {
        case P1CountryTier::city_state:return 0;
        case P1CountryTier::principality:return 5;
        case P1CountryTier::grand_principality:return 10;
        case P1CountryTier::kingdom:return 15;
        case P1CountryTier::empire:return 25;
        case P1CountryTier::hegemony:return 50;
    }
    return 0;
}
inline std::int64_t authority_cost(P1MandateKind kind) {
    switch(kind) {
        case P1MandateKind::decree_road_maintenance:
        case P1MandateKind::decree_promote_social_mobility:return decree_cost;
        case P1MandateKind::bolster_interest_group:
        case P1MandateKind::suppress_interest_group:return ig_action_cost;
    }
    return 0;
}
inline P1FlowCapacity flow(std::int64_t generated,std::int64_t commitment,
                           bool complete=true) {
    P1FlowCapacity v;
    v.generated=generated;
    v.committed=commitment;
    v.balance=generated-commitment;
    v.complete=complete;
    if(generated>commitment) {
        v.excess_ppm=commitment<=0?1'000'000:
           static_cast<std::int32_t>(std::min<std::int64_t>(
               1'000'000,(generated-commitment)*1'000'000/commitment));
    } else if(generated<commitment) {
        v.shortage_ppm=generated<=0?1'000'000:
           static_cast<std::int32_t>(std::min<std::int64_t>(
               1'000'000,(commitment-generated)*1'000'000/generated));
    }
    return v;
}
} // namespace p1power
} // namespace vic25::phasec
