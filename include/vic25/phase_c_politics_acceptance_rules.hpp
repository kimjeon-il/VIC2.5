#pragma once
// Extracted from Victoria 3 wiki (1.13 verified), source:
// https://vic3.paradoxwikis.com/wiki/Acceptance
// https://github.com/maxzxc0110/Victoria3/blob/main/Victoria_3_Wiki/knowledge_base/08_Other_Systems/Acceptance.md
// Only the six documented citizenship laws are mapped here. 1.15
// Official Nationality and any DLC override are *not* silently guessed.
#include <array>
#include <map>
#include <optional>
#include <string>
namespace vic25::phasec {
struct P2AcceptanceLawEffect {
    bool can_vote{};
    std::int32_t political_strength_ppm{1'000'000};
    std::int32_t wage_ppm{1'000'000};
    std::int32_t vote_strength_ppm{1'000'000};
};
inline std::optional<P2AcceptanceLawEffect> p2_acceptance_law_effect(
    const std::string& law,P2AcceptanceLevel level) {
    static const std::map<std::string,std::array<P2AcceptanceLawEffect,5>> table{
        {"subjecthood", std::array<P2AcceptanceLawEffect,5>{P2AcceptanceLawEffect{false,600000,600000,1000000},P2AcceptanceLawEffect{false,700000,700000,1000000},P2AcceptanceLawEffect{true,800000,800000,750000},P2AcceptanceLawEffect{true,900000,900000,1000000},P2AcceptanceLawEffect{true,1000000,1000000,1000000}}},
        {"ethnostate", std::array<P2AcceptanceLawEffect,5>{P2AcceptanceLawEffect{false,500000,500000,1000000},P2AcceptanceLawEffect{false,600000,600000,1000000},P2AcceptanceLawEffect{false,700000,750000,1000000},P2AcceptanceLawEffect{true,850000,850000,500000},P2AcceptanceLawEffect{true,1250000,1250000,1000000}}},
        {"national_supremacy", std::array<P2AcceptanceLawEffect,5>{P2AcceptanceLawEffect{false,600000,600000,1000000},P2AcceptanceLawEffect{false,700000,700000,1000000},P2AcceptanceLawEffect{true,800000,800000,500000},P2AcceptanceLawEffect{true,900000,900000,750000},P2AcceptanceLawEffect{true,1200000,1200000,1000000}}},
        {"racial_segregation", std::array<P2AcceptanceLawEffect,5>{P2AcceptanceLawEffect{false,700000,700000,1000000},P2AcceptanceLawEffect{false,800000,800000,1000000},P2AcceptanceLawEffect{true,900000,900000,750000},P2AcceptanceLawEffect{true,1100000,1050000,1000000},P2AcceptanceLawEffect{true,1100000,1150000,1000000}}},
        {"cultural_exclusion", std::array<P2AcceptanceLawEffect,5>{P2AcceptanceLawEffect{false,700000,700000,1000000},P2AcceptanceLawEffect{true,800000,800000,750000},P2AcceptanceLawEffect{true,900000,900000,1000000},P2AcceptanceLawEffect{true,1100000,1000000,1000000},P2AcceptanceLawEffect{true,1100000,1050000,1000000}}},
        {"multicultural", std::array<P2AcceptanceLawEffect,5>{P2AcceptanceLawEffect{true,1000000,800000,1000000},P2AcceptanceLawEffect{true,1000000,900000,1000000},P2AcceptanceLawEffect{true,1000000,950000,1000000},P2AcceptanceLawEffect{true,1000000,1000000,1000000},P2AcceptanceLawEffect{true,1000000,1000000,1000000}}},
    };
    const auto it=table.find(law);
    if(it==table.end())return std::nullopt;
    const auto index=static_cast<std::size_t>(level);
    if(index>=5)return std::nullopt;
    return it->second[index];
}
} // namespace vic25::phasec
