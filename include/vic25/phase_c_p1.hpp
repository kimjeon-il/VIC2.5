#pragma once
// Phase C-P1: compact governance contracts; no duplicate POP, money,
// legislative seat, party, public-service, or political-power ledgers.
#include <cstdint>
#include <string>
#include <vector>

namespace vic25::phasec {

enum class P1GovernancePrinciple : std::uint8_t {
    chiefdom, monarchy, theocracy, presidential_republic,
    parliamentary_republic, council_republic
};
enum class P1PowerDistribution : std::uint8_t {
    autocracy, oligarchy, landed_vote, wealth_vote,
    universal_vote, single_party, technocracy
};
enum class P1LegislatureKind : std::uint8_t { none, unicameral, bicameral };
enum class P1FormationRule : std::uint8_t {
    head_appointment, parliamentary_confidence, council_confidence
};
enum class P1InstitutionKind : std::uint8_t {
    administration, policing, education, healthcare, welfare, elections
};

struct P1Constitution {
    P1GovernancePrinciple governance{P1GovernancePrinciple::monarchy};
    P1PowerDistribution power{P1PowerDistribution::autocracy};
    P1LegislatureKind legislature{P1LegislatureKind::none};
    // A scenario/law event identifier, not a separately simulated law record.
    std::string legal_basis;
};

struct P1Coalition {
    // Groups belong to the existing Phase C/P3 membership registry.
    // Parties belong to the existing Phase C/P4 national election result.
    std::vector<std::string> ruling_groups, ruling_parties;
    bool caretaker{};
};

struct P1GovernmentChange {
    std::string country, new_head, new_prime;
    P1Coalition coalition;
    std::string legal_basis;
    bool replace_head{}, replace_prime{};
};

struct P1Institution {
    std::string country, public_institution_id;
    P1InstitutionKind kind{P1InstitutionKind::administration};
    std::uint8_t level{}; // 0..5: entitlement ceiling, not fictitious capacity
};

struct P1AuthorityView {
    P1FormationRule formation{P1FormationRule::head_appointment};
    bool legislature_exists{}, assembly_confidence_required{};
    bool head_can_appoint{}, direct_executive{};
};

struct P1LegitimacyView {
    int score{50}; // 0..100; missing components use a transparent neutral prior
    int group_support{50}, electoral_support{50}, legal_compatibility{100};
    int ideological_incompatibility{}; // observed opposing coalition stances
    bool observed_groups{}, observed_election{}, caretaker{};
};

struct P1InstitutionView {
    std::string country, public_institution_id;
    P1InstitutionKind kind{P1InstitutionKind::administration};
    std::uint8_t level{};
    std::int64_t employed_staff{}, legal_slots{}, staffed_slots{}, usable_slots{};
    bool cash_backed{};
};

[[nodiscard]] constexpr bool valid_p1_constitution(const P1Constitution& c) {
    if(static_cast<unsigned>(c.governance)>
           static_cast<unsigned>(P1GovernancePrinciple::council_republic)||
       static_cast<unsigned>(c.power)>
           static_cast<unsigned>(P1PowerDistribution::technocracy)||
       static_cast<unsigned>(c.legislature)>
           static_cast<unsigned>(P1LegislatureKind::bicameral)||
       c.legal_basis.empty())return false;
    if((c.governance==P1GovernancePrinciple::parliamentary_republic ||
        c.governance==P1GovernancePrinciple::council_republic) &&
       c.legislature==P1LegislatureKind::none)return false;
    return true;
}

[[nodiscard]] constexpr P1FormationRule p1_formation_rule(
        const P1Constitution& c) {
    if(c.governance==P1GovernancePrinciple::council_republic)
        return P1FormationRule::council_confidence;
    if(c.governance==P1GovernancePrinciple::parliamentary_republic ||
       (c.governance==P1GovernancePrinciple::monarchy &&
        c.legislature!=P1LegislatureKind::none &&
        (c.power==P1PowerDistribution::landed_vote ||
         c.power==P1PowerDistribution::wealth_vote ||
         c.power==P1PowerDistribution::universal_vote)))
        return P1FormationRule::parliamentary_confidence;
    return P1FormationRule::head_appointment;
}
} // namespace vic25::phasec
