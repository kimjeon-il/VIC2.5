#pragma once

#include <compare>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>

namespace vic25 {
using People = std::int64_t;

enum class Sex : std::uint8_t { female, male, other_or_unspecified };
enum class Health : std::uint8_t { well, temporary_injury, long_term_impairment, illness };
enum class MilitaryStatus : std::uint8_t { civilian, active_service, reserve, prisoner_or_missing };

struct CohortKey {
    // Calendar-month index; retains exact birth month for monthly age transitions.
    std::int32_t birth_month_index{};
    Sex sex{Sex::other_or_unspecified};
    Health health{Health::well};
    std::uint32_t functional_limitations{};  // Bitmask; not an automatic employment ban.
    MilitaryStatus military{MilitaryStatus::civilian};
    auto operator<=>(const CohortKey&) const = default;
};

struct PopKey {
    std::string region, culture, religion, profession;
    std::string livelihood, wealth_band, legal_status;
    auto operator<=>(const PopKey&) const = default;
};

inline People checked_add(People a, People b) {
    if ((b > 0 && a > std::numeric_limits<People>::max() - b) ||
        (b < 0 && a < std::numeric_limits<People>::min() - b)) {
        throw std::overflow_error("integer overflow");
    }
    return a + b;
}

struct Pop {
    std::string id;
    PopKey key;
    std::map<CohortKey, People> cohorts;

    [[nodiscard]] People population() const {
        People sum = 0;
        for (const auto& [key, count] : cohorts) {
            (void)key;
            if (count < 0) throw std::logic_error("negative cohort population");
            sum = checked_add(sum, count);
        }
        return sum;
    }
};

inline void add_people(Pop& pop, const CohortKey& cohort, People count) {
    if (count <= 0) throw std::invalid_argument("population delta must be positive");
    auto current = pop.cohorts.contains(cohort) ? pop.cohorts.at(cohort) : 0;
    pop.cohorts[cohort] = checked_add(current, count);
}

inline void remove_people(Pop& pop, const CohortKey& cohort, People count) {
    if (count <= 0) throw std::invalid_argument("population delta must be positive");
    auto it = pop.cohorts.find(cohort);
    if (it == pop.cohorts.end() || it->second < count) {
        throw std::invalid_argument("insufficient cohort population");
    }
    it->second -= count;
    if (it->second == 0) pop.cohorts.erase(it);
}

inline void transfer_people(Pop& from, Pop& to, const CohortKey& cohort, People count) {
    if (&from == &to || from.id == to.id) throw std::invalid_argument("same POP");
    if (count <= 0) throw std::invalid_argument("count must be positive");
    // Precheck both sides to make the operation atomic on expected failures.
    const auto it = from.cohorts.find(cohort);
    if (it == from.cohorts.end() || it->second < count) {
        throw std::invalid_argument("insufficient cohort population");
    }
    const People target = to.cohorts.contains(cohort) ? to.cohorts.at(cohort) : 0;
    (void)checked_add(target, count);
    remove_people(from, cohort, count);
    add_people(to, cohort, count);
}

struct BattleLoss {
    People killed{};
    People temporarily_injured{};
    People long_term_impairment{};
};

inline void apply_battle_loss(Pop& pop, CohortKey source, BattleLoss loss) {
    if (source.military != MilitaryStatus::active_service || source.health != Health::well) {
        throw std::invalid_argument("battle source must be a healthy active-service cohort");
    }
    if (loss.killed < 0 || loss.temporarily_injured < 0 || loss.long_term_impairment < 0) {
        throw std::invalid_argument("negative battle loss");
    }
    const People changed = checked_add(checked_add(loss.killed, loss.temporarily_injured), loss.long_term_impairment);
    if (changed == 0) return;
    const auto it = pop.cohorts.find(source);
    if (it == pop.cohorts.end() || it->second < changed) {
        throw std::invalid_argument("battle losses exceed active service cohort");
    }
    CohortKey temp = source;
    temp.health = Health::temporary_injury;
    CohortKey impaired = source;
    impaired.health = Health::long_term_impairment;
    const People target_temp = pop.cohorts.contains(temp) ? pop.cohorts.at(temp) : 0;
    const People target_impaired = pop.cohorts.contains(impaired) ? pop.cohorts.at(impaired) : 0;
    (void)checked_add(target_temp, loss.temporarily_injured);
    (void)checked_add(target_impaired, loss.long_term_impairment);
    remove_people(pop, source, changed);
    if (loss.temporarily_injured) add_people(pop, temp, loss.temporarily_injured);
    if (loss.long_term_impairment) add_people(pop, impaired, loss.long_term_impairment);
    // Only deaths reduce the population; medical transitions conserve persons.
}
} // namespace vic25
