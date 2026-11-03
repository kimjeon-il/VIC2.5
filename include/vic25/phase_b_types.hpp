#pragma once

#include "vic25/monthly.hpp"
#include <algorithm>
#include <array>
#include <compare>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace vic25::phaseb {
using Id = std::uint64_t;
using People = std::int64_t;
constexpr std::int64_t PPM = 1'000'000;

inline People add_checked(People a, People b) {
    if ((b > 0 && a > std::numeric_limits<People>::max() - b) ||
        (b < 0 && a < std::numeric_limits<People>::min() - b))
        throw std::overflow_error("people overflow");
    return a + b;
}
inline People mul_checked(People a, People b) {
    if (a < 0 || b < 0 || (b != 0 && a > std::numeric_limits<People>::max() / b))
        throw std::overflow_error("positive multiplication overflow");
    return a * b;
}
inline void require(bool value, const char* message) {
    if (!value) throw std::invalid_argument(message);
}

enum class Sex : std::uint8_t { female, male, another_recorded, unknown };
enum class Service : std::uint8_t { civilian, active, reserve, pow, missing, legacy_ambiguous };
enum class School : std::uint8_t { none, primary, secondary, tertiary };
enum class EventKind : std::uint8_t {
    initial_population, birth, death, migration, occupation, household_change,
    illness, recovery, injury, rehabilitation, schooling, training,
    enlistment, discharge, capture, combat_loss, employment, release
};

// Separate disease, temporary injury and long-term impairments can co-exist.
// Capability mask represents task-specific limitations, not general unemployment.
struct Health {
    bool ill{};
    bool temporary_injury{};
    bool long_term_impairment{};
    std::uint32_t limited_capabilities{};
    std::uint8_t illness_severity{};
    std::uint8_t injury_severity{};
    auto operator<=>(const Health&) const = default;
};
struct Education {
    School school{School::none};
    std::uint8_t literacy{}; // 0-100, distinct from school stage.
    std::uint8_t occupational_skill{}; // 0-100, distinct from school stage.
    auto operator<=>(const Education&) const = default;
};
struct Demographic {
    std::string region;
    std::int64_t birth_month{};
    Sex sex{Sex::unknown};
    std::string culture, religion, citizenship, legal_status;
    Id health{};
    Service service{Service::civilian};
    auto operator<=>(const Demographic&) const = default;
};
struct Economic {
    std::string region, profession, livelihood, labor_class;
    Id education{};
    auto operator<=>(const Economic&) const = default;
};
struct Household {
    std::string region, composition, pooling_policy;
    People household_count_estimate{}; // statistical estimate; not a headcount.
    auto operator<=>(const Household&) const = default;
};
struct SliceKey {
    Id demographic{}, economic{}, household{}, partition{};
    auto operator<=>(const SliceKey&) const = default;
};
struct Slice {
    Id id{};
    SliceKey key{};
    People count{}; // The ONLY stored population headcount.
    std::uint64_t revision{};
};
struct JobRequirements {
    School minimum_school{School::none};
    std::uint8_t minimum_skill{};
    std::uint32_t required_capabilities{};
    std::uint32_t accommodation_mask{};
    std::int64_t minimum_age_months{16 * 12};
    std::int64_t maximum_age_months{80 * 12};
    std::uint8_t allowed_sexes_mask{0x0F}; // explicit legal restriction, not biological capacity.
    auto operator<=>(const JobRequirements&) const = default;
};
struct Employment {
    std::string employer, occupation;
    People count{};
    std::int64_t wage_per_month{}; // Monetary potential; no transfer without FinanceLedger.
    JobRequirements requirements{};
    auto operator<=>(const Employment&) const = default;
};
struct Event {
    Id id{};
    std::int64_t month{};
    EventKind kind{};
    Id from{}, to{};
    People count{};
    People employed_affected{};
    std::string provenance;
};
struct Vacancy {
    std::string employer, profession;
    People slots{};
    std::int64_t wage_per_month{};
    JobRequirements requirements{};
};
struct LaborRule {
    std::int64_t minimum_age_months{16 * 12};
    std::int64_t maximum_age_months{75 * 12};
    std::uint8_t allowed_sexes_mask{0x0F};
};
struct LaborReport {
    People population{}, working_age{}, eligible{}, employed{}, unemployed{}, self_employed{}, inactive{};
};
struct HouseholdReport {
    People people{}, children{}, elder{}, dependents{}, employed{};
    std::int64_t wage_income{}, subsistence_value{}, food_need{}, clothing_need{}, housing_need{}, medical_need{};
    std::int64_t planned_basket_value{}; // No actual market purchases in Phase B.
};

// All modifications occur on an in-memory world. A monthly step stages a full
// copy, validates it, then commits; other economy ledgers are not yet connected.

} // namespace vic25::phaseb
