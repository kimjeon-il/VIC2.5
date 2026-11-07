#pragma once
// P5-G: Victoria-style regional guerrilla journal. All people, armed units,
// arms, sovereign territory, market goods and public payroll remain in C.
// This declares references and campaign audit, never independent soldiers.
#include <cstdint>
#include <string>
#include <vector>

namespace vic25::phasec {

enum class P5GuerrillaStage : std::uint8_t {
    active, supply_exhausted, conventional, ceasefire, resolved
};
enum class P5GuerrillaIncidentKind : std::uint8_t {
    disruption, skirmish, region_expansion, conventional_transition,
    return_to_irregular, ceasefire, resolution
};
enum class P5GuerrillaResolution : std::uint8_t {
    negotiated_ceasefire, enacted_reform, disarmed
};
struct P5GuerrillaCampaign {
    std::string id, country, movement;
    // A conventional escalation must reference the *same* P5 political
    // crisis and the already pledged C units, never spawn an extra army.
    std::string crisis_id;
    P5GuerrillaStage stage{P5GuerrillaStage::active};
    std::int64_t started_day{}, next_review_day{}, ended_day{-1};
    std::vector<std::string> regions, units; // real C region/deployment IDs
    std::int32_t verified_skirmishes{};
};
struct P5GuerrillaIncident {
    std::string id, campaign, region, irregular_unit, government_unit;
    P5GuerrillaIncidentKind kind{P5GuerrillaIncidentKind::disruption};
    std::int64_t day{}, ammunition_spent{};
    std::int32_t access_disruption_ppm{};
};
struct P5GuerrillaAccessEffect {
    std::string incident_id, campaign, region;
    std::int64_t from_day{}, through_day{}; // inclusive interval
    std::int32_t penalty_ppm{}; // affects access, does not destroy stock
};
struct P5GuerrillaRegionView {
    std::string campaign, region, sovereign, military_controller;
    std::int64_t population{}, supporters{}, irregular_strength{};
    std::int64_t available_ammunition{}, government_strength{};
    std::int32_t support_ppm{}, security_ppm{}, access_disruption_ppm{};
};
struct P5GuerrillaOutcome {
    std::string campaign, legal_basis;
    P5GuerrillaResolution kind{P5GuerrillaResolution::negotiated_ceasefire};
    std::int64_t day{};
};
} // namespace vic25::phasec
