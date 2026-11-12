#pragma once
// Phase C-P2 cultural-legal evidence, not a second cultural POP simulator.
// Traits and primary-culture/religion membership must come from scenario data;
// never infer heritage, language or religious group from a culture's display name.
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
namespace vic25::phasec {
struct P2CultureTraits {
    std::string culture;
    std::set<std::string> heritages,heritage_groups;
    std::set<std::string> languages,language_groups;
    std::set<std::string> traditions,homelands;
};
struct P2ReligionTraits {
    std::string religion;
    std::set<std::string> traits,trait_groups;
};
struct P2NationalIdentity {
    std::string country;
    std::set<std::string> primary_cultures,state_religions;
};
enum class P2AcceptanceLevel :std::uint8_t {
    violent_hostility=0,cultural_erasure=1,open_prejudice=2,
    second_class=3,full=4
};
struct P2AcceptanceView {
    std::string country,culture,religion,region;
    std::int32_t score{};
    P2AcceptanceLevel level{P2AcceptanceLevel::violent_hostility};
};
} // namespace vic25::phasec
