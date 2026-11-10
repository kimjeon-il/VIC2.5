#pragma once
#include <cstdint>
#include <string>
namespace vic25::phasec {
enum class P1MandateKind : std::uint8_t {
    regional_decree, bolster_movement, suppress_movement, consumption_tax
};
enum class P1DecreeKind : std::uint8_t {
    construction_drive, public_service_reach
};
struct P1Mandate {
    std::string id,country,target;
    P1MandateKind kind{P1MandateKind::bolster_movement};
    P1DecreeKind decree{P1DecreeKind::construction_drive};
    std::int32_t authority_cost{200};
    std::int64_t started_day{-1};
    bool active{true};
};
struct P1AuthorityBudget {
    std::string country;
    std::int32_t supply{},reserved{},available{};
};
enum class P1PoliticalEventKind : std::uint8_t {
    enactment_debate, political_petition, crisis_warning, political_scandal,
    actual_legal_dispute
};
struct P1PoliticalEvent {
    std::string id,country,source;
    P1PoliticalEventKind kind{P1PoliticalEventKind::political_petition};
    std::int64_t day{};
    bool resolved{};
};
enum class P1PoliticalEventDecision : std::uint8_t {
    acknowledge, negotiate, bolster, suppress
};
struct P1Lobby {
    std::string id,source_country,target_country,interest_group,issue_domain;
    std::string desired_choice;
    std::int32_t pressure_ppm{},satisfaction_ppm{};
    std::int64_t created_day{-1},last_review_day{-1};
    bool active{true};
};
} // namespace vic25::phasec
