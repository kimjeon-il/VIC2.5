#pragma once

#include "phase_gh.hpp"

namespace vic25::play_status {

struct CurrentConflict {
    std::string opponent;
    std::string_view state;
    bool operator==(const CurrentConflict&) const = default;
};

[[nodiscard]] inline std::vector<CurrentConflict> current_conflicts(
        const phasegh::DiplomacyState& diplomacy,std::string_view country) {
    std::vector<CurrentConflict> conflicts;
    for(const auto& [id,war]:diplomacy.wars) {
        (void)id;
        if(war.ended_day>=0||(!war.active&&!war.armistice)||
           (war.attacker!=country&&war.defender!=country))continue;
        conflicts.push_back({war.attacker==country?war.defender:war.attacker,
                             war.armistice?"armistice":"active"});
    }
    return conflicts;
}

} // namespace vic25::play_status
