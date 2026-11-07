#pragma once
// C-P5: political discontent and actual collective actions. Real population,
// labor, law, armed strength, property, and provincial control live in C/M.
#include <cstdint>
#include <string>
#include <vector>

namespace vic25::phasec {

enum class P5ActionKind : std::uint8_t {
    petition, demonstration, strike, negotiation, concession, policing
};
enum class P5CrisisKind : std::uint8_t {
    revolution, coup, autonomy, secession
};
enum class P5CrisisStage : std::uint8_t {
    observed, warning, armed_conflict, deescalated, settled
};
enum class P5SettlementKind : std::uint8_t {
    legal_concession, crisis_recedes, cabinet_overthrow, autonomy_granted,
    independence_recognized
};
struct P5RiskView {
    std::string movement,country;
    std::int32_t support_ppm{},activism_ppm{},radical_ppm{};
    std::int32_t pressure_ppm{}; // warning indicator, not fabricated armed men
    bool institutional_exit{},eligible_for_warning{};
};
struct P5CollectiveAction {
    std::string id,movement,group,region,institution_id;
    P5ActionKind kind{P5ActionKind::petition};
    std::uint64_t slice{};       // real Phase B POP reference
    std::int64_t participants{},strikers{},day{};
};
struct P5Crisis {
    std::string id,country,movement;
    P5CrisisKind kind{P5CrisisKind::revolution};
    P5CrisisStage stage{P5CrisisStage::observed};
    std::int32_t pressure_days{},calm_days{};
    std::int64_t last_checked_day{-1},armed_day{-1},settled_day{-1};
    std::vector<std::string> pledged_units; // actual C army IDs, not new forces
    std::vector<std::string> contested_regions; // legal claims, not title changes
    std::int32_t verified_engagements{};
};
struct P5Settlement {
    std::string id,crisis_id,legal_basis,successor_head,new_sovereign,region;
    P5SettlementKind kind{P5SettlementKind::legal_concession};
    std::int64_t day{};
};
} // namespace vic25::phasec
