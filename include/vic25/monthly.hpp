#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string_view>

namespace vic25 {
enum class MonthStage : std::uint8_t {
    opening_snapshot,
    planning_and_credit,
    labor_clearance,
    intermediate_goods,
    production,
    wages_and_transfers,
    pop_consumption_plans,
    final_goods_market,
    closing_accounts,
    saving_and_investment,
    pop_demography_and_health,
    repricing_and_validation
};
inline constexpr std::array<MonthStage, 12> MONTH_STAGES = {
    MonthStage::opening_snapshot, MonthStage::planning_and_credit, MonthStage::labor_clearance,
    MonthStage::intermediate_goods, MonthStage::production, MonthStage::wages_and_transfers,
    MonthStage::pop_consumption_plans, MonthStage::final_goods_market, MonthStage::closing_accounts,
    MonthStage::saving_and_investment, MonthStage::pop_demography_and_health,
    MonthStage::repricing_and_validation
};
inline constexpr std::array<std::string_view, 12> MONTH_STAGE_NAMES = {
    "opening_snapshot", "planning_and_credit", "labor_clearance", "intermediate_goods",
    "production", "wages_and_transfers", "pop_consumption_plans", "final_goods_market",
    "closing_accounts", "saving_and_investment", "pop_demography_and_health",
    "repricing_and_validation"
};

// Schedule/contract only: this class does NOT implement a working economy.
class MonthlyPipeline {
public:
    explicit MonthlyPipeline(std::int64_t starting_month = 0) : current_month_(starting_month) {}
    template <typename Fn>
    void advance(Fn&& stage_handler) {
        for (MonthStage stage : MONTH_STAGES) stage_handler(stage);
        ++current_month_;
    }
    [[nodiscard]] std::int64_t month_index() const { return current_month_; }
private:
    std::int64_t current_month_{};
};
} // namespace vic25
