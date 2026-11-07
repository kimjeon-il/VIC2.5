#pragma once
// C-P3: POP attitudes, interest-group clout and persistent movements.
// No duplicated population, party ballot, treasury or military ledger.
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace vic25::phasec {

enum class P3GroupType : std::uint8_t {
    landowners, industrialists, armed_forces, devout, intelligentsia,
    petite_bourgeoisie, rural_folk, trade_unions, custom
};
enum class P3MovementGoal : std::uint8_t {
    legal_reform, defend_status_quo, replace_government, autonomy, secession
};
struct P3Stance {
    std::int32_t preference_ppm{}; // [-1e6,+1e6], issue-specific; not group identity
    std::int32_t importance_ppm{}; // 0..1e6, salience independent of support
};
struct P3Attitude {
    std::uint64_t slice{};         // actual Phase B POP slice reference
    std::int32_t radical_ppm{}, loyal_ppm{}, engagement_ppm{100'000};
    std::int64_t observed_day{-1};
    std::map<std::string,P3Stance> issues;
};
struct P3Group {
    std::string country,id,leader_anchor,issue_domain,preferred_choice;
    P3GroupType type{P3GroupType::custom};
    std::int32_t organization_ppm{500'000},institutional_access_ppm{};
};
struct P3GroupView {
    std::string country,id;
    std::int64_t members{};
    std::int32_t clout_ppm{},approval{},engagement_ppm{},radical_ppm{};
};
struct P3Movement {
    std::string id,country,issue_domain,desired_choice,region;
    P3MovementGoal goal{P3MovementGoal::legal_reform};
    std::vector<std::string> groups,parties;
    std::int32_t activism_ppm{300'000},radical_ppm{100'000};
    bool active{true};
};
struct P3MovementView {
    std::string id,country;
    std::int64_t supporters{},potential_activists{};
    std::int32_t support_ppm{},activism_ppm{},radical_ppm{};
};
} // namespace vic25::phasec
