#pragma once
#include <cstdint>
#include <map>
#include <string>
namespace vic25::phasec {
enum class PoliticalRole : std::uint8_t {
    head_of_state=1, interest_group_leader=2, commander=4,
    business_leader=8, agitator=16
};
struct PoliticalCharacter {
    std::string person_id,country,interest_group,ideology,movement;
    std::map<std::string,std::string> law_preferences; // domain -> choice
    std::int32_t popularity_ppm{};
    std::uint8_t roles{};
    bool active{true},politically_exiled{};
    std::int64_t last_review_day{-1};
};
} // namespace vic25::phasec
