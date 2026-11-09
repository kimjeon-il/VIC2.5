#pragma once
// GDP statistics are immutable observations of C's REAL goods transformation
// and D's PAID construction. Not a treasury, company profit or price ledger.
#include <cstdint>
#include <string>
namespace vic25::phasec {
struct P1ValueAddedEvent {
    std::string source,country,region;
    std::int64_t month{};
    std::int32_t day{};
    std::int64_t gross_output{},intermediate_input{};
    bool fully_priced{};
};
struct P1GdpView {
    std::string country;
    std::int64_t from_month{},through_month{};
    std::int64_t output{},inputs{},value_added{};
    std::uint64_t observed_events{},unpriced_events{};
    bool complete{},observed{};
};
} // namespace vic25::phasec
