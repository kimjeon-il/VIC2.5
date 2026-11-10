#pragma once
#include <cstdint>
#include <string>
namespace vic25::phasec {
// The judiciary is a law choice plus a projection over existing staffed
// public services. There are no courts, judges or parallel case ledgers.
enum class P1JudiciaryKind : std::uint8_t {
    executive_subordinate, limited_independence, independent
};
struct P1JudiciaryView {
    std::string country;
    P1JudiciaryKind kind{P1JudiciaryKind::limited_independence};
    bool defined_by_law{},staffed_administration{},staffed_policing{};
    std::int32_t enforceable_contract_ppm{1'000'000};
};
} // namespace vic25::phasec
