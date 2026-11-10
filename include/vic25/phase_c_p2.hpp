#pragma once
// Phase C-P2: legal rules are not an independent resource or economic ledger.
// P1 owns current government, P4 election schedules and POP/C own real results.
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace vic25::phasec {
enum class P2NormTier : std::uint8_t {
    constitution, statute, delegated_regulation, local_ordinance
};
// One deterministic chamber threshold for player, AI and upper/lower votes.
// Legal assent is not a second random enactment-success roll.
[[nodiscard]] constexpr std::int64_t p2_chamber_quorum(
        std::int64_t seats,std::int64_t ordinary_quorum,
        P2NormTier tier) {
    return tier==P2NormTier::constitution?
        seats-seats/3:ordinary_quorum;
}
enum class P2ClauseKind : std::uint8_t {
    term_months, max_consecutive_terms, max_lifetime_terms,
    term_cycle_mode, dissolution_authorized, dissolution_max_delay_days,
    voting_age_years, income_tax_min_ppm, income_tax_max_ppm,
    public_healthcare_right, conscription_authorized,
    minimum_monthly_contract_wage, public_institution_level,
    maximum_import_tariff_ppm,
    female_suffrage_allowed, political_association_allowed,
    peaceful_strike_allowed,
    constitutional_veto_rule, constitutional_override_ppm,
    constitutional_assent_days, constitutional_party_initiative
};
enum class P2BillAction : std::uint8_t {
    enact_or_replace_base, attach_amendment,
    repeal_amendment, repeal_base
};
enum class P2BillStatus : std::uint8_t {
    submitted, approved, promulgated, effective, rejected
};
enum class P2TermTransition : std::uint8_t {
    future_term_only, incumbent_as_well
};

struct P2Clause {
    std::string id, target; // contest ID or country ID, never a new entity
    P2ClauseKind kind{P2ClauseKind::term_months};
    std::int64_t value{};
};

struct P2BaseLaw {
    std::string id,country,domain,scope,base_choice,legal_basis;
    P2NormTier tier{P2NormTier::statute};
    std::vector<P2Clause> clauses;
};

struct P2Amendment {
    std::string id,law_id,legal_basis,exclusivity_group;
    // Additional or overriding provisions, not an arbitrary bonus modifier.
    std::vector<P2Clause> clauses;
};

struct P2ElectionTransition {
    std::string contest_id;
    P2TermTransition mode{P2TermTransition::future_term_only};
    // Only used for incumbent_as_well. P4 owns the actual next election.
    std::int64_t next_poll_day{-1};
};

struct P2Bill {
    std::string id,country,proposer,legal_basis;
    P2BillAction action{P2BillAction::enact_or_replace_base};
    P2BillStatus status{P2BillStatus::submitted};
    P2BaseLaw proposed_base;
    P2Amendment proposed_amendment;
    std::string target_id; // base law or amendment being repealed
    std::vector<P2ElectionTransition> transitions;
    std::int64_t introduced_day{-1},promulgated_day{-1},effective_day{-1};
};

struct P2Enactment {
    std::string bill_id,country,law_key;
    std::uint8_t phase{1}; // 1..3; fourth state is ready for a legal vote
    std::int32_t success_bonus_ppm{},setbacks{},checks{};
    std::int64_t next_review_day{-1},last_review_day{-1};
    bool ready{};
};
struct P2Assent {
    std::string bill_id,country;
    std::int64_t deadline{-1},veto_day{-1};
    bool signed_by_head{},vetoed{},overridden{};
};
struct P2LawEvent {
    std::string bill_id,country,law_id,amendment_id,legal_basis;
    P2BillAction action{P2BillAction::enact_or_replace_base};
    std::int64_t effective_day{};
};

struct P2GroupDemand {
    std::string id,country,interest_group,law_domain;
    P2Amendment amendment;
    // Backed by actual P3 population; 0..1e6 of a sovereign's inhabitants.
    std::int32_t min_population_support_ppm{50'000};
};
struct P2NegotiatedAmendment {
    std::string bill_id,demand_id,group;
    P2Amendment amendment;
    std::int64_t support_population{},accepted_day{};
};
struct P2ChamberDecision {
    std::string bill_id,upper_contest_id;
    std::int64_t lower_poll_day{-1},upper_poll_day{-1};
    bool lower_passed{},upper_passed{};
};
struct P2TermView {
    std::string contest_id, legal_basis;
    std::int32_t term_months{}, max_consecutive_terms{}, max_lifetime_terms{};
    std::int32_t dissolution_max_delay_days{};
    bool dissolution_authorized{};
    P4TermMode cycle_mode{P4TermMode::restart_after_poll};
    bool has_legal_term{};
};
} // namespace vic25::phasec
