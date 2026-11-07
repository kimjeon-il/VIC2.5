#pragma once
// Phase C-P4: party affiliations and dated election contracts.
// These are political references, not population, banking or asset ledgers.
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace vic25::phasec {
enum class P4Office : std::uint8_t { legislature, local, head_of_state };
enum class P4BallotMethod : std::uint8_t { list_dhondt, plurality };
enum class P4TermMode : std::uint8_t { restart_after_poll, fixed_cycle };
enum class P4Competition : std::uint8_t {
    unobserved, legally_single_party, dominant_party, two_party, multi_party
};
struct P4Party {
    std::string id,country,head_candidate;
    // Organization is a relative election-time support weight, not a cash
    // account or an independent population count.
    std::int32_t organization_ppm{100'000};
    bool registered{true},active{true};
};
struct P4PartyGroup {
    std::string party,interest_group;
    std::int32_t affinity_ppm{}; // 0..1e6; non-exclusive policy affinity
};
struct P4Contest {
    std::string id,country,jurisdiction,law_id;
    P4Office office{P4Office::legislature};
    P4BallotMethod method{P4BallotMethod::list_dhondt};
    P4TermMode term_mode{P4TermMode::restart_after_poll};
    std::int32_t term_months{48},turnout_ppm{750'000};
    std::int64_t seats{1},next_poll_day{},regular_poll_day{};
    std::int64_t last_poll_day{-1};
    bool dissolution_authorized{},active{true},dissolved{};
};
struct P4Result {
    std::string contest_id,law_id;
    std::int64_t day{};
    std::int64_t eligible{},registered{},cast{},invalid{},valid{};
    std::map<std::string,std::int64_t> votes,seats;
    std::string winning_party,office_holder;
    bool early{};
};
struct P4TermAmendment {
    std::string contest_id,law_id;
    std::int64_t effective_day{},next_poll_day{};
    std::int32_t term_months{};
    bool applies_to_incumbent{};
    P4TermMode term_mode{P4TermMode::restart_after_poll};
};
struct P4CompetitionView {
    P4Competition pattern{P4Competition::unobserved};
    std::int32_t effective_parties_x100{};
    std::int32_t observed_elections{};
    std::int32_t leading_share_percent{},two_leaders_share_percent{};
};
} // namespace vic25::phasec
