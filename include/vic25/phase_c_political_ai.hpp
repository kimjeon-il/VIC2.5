#pragma once
#include <cstdint>
#include <string>
namespace vic25::phasec {
struct PoliticalAgenda {
    std::string country,last_bill_id,last_reform_choice;
    std::int64_t last_review_day{-1},next_review_day{-1};
    std::int64_t last_election_day{-1};
};
} // namespace vic25::phasec
