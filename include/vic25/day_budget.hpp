#pragma once
// Shared integer allocation for legacy monthly limits used on native days.
// A day never receives the entire monthly capacity again.
#include "vic25/phase_b_types.hpp"
#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace vic25::daily_budget {
using Amount = std::int64_t;

inline Amount cumulative(Amount monthly, int day, int days_in_month) {
    if(monthly<0 || days_in_month<28 || days_in_month>31 ||
       day<0 || day>days_in_month)
        throw std::invalid_argument("TIME_ERR_INVALID_DAILY_CAPACITY");
    const Amount whole=phaseb::mul_checked(monthly/days_in_month,day);
    const Amount fractional=phaseb::mul_checked(monthly%days_in_month,day)/
                            days_in_month;
    return phaseb::add_checked(whole,fractional);
}

inline Amount increment(Amount monthly, int day, int days_in_month) {
    if(day<1)throw std::invalid_argument("TIME_ERR_INVALID_DAILY_INCREMENT");
    return cumulative(monthly,day,days_in_month)-
           cumulative(monthly,day-1,days_in_month);
}

inline Amount remaining(Amount monthly, Amount already_used,
                        int day, int days_in_month) {
    if(already_used<0)
        throw std::invalid_argument("TIME_ERR_NEGATIVE_CAPACITY_USAGE");
    return std::max<Amount>(0,
        cumulative(monthly,day,days_in_month)-already_used);
}
} // namespace vic25::daily_budget
