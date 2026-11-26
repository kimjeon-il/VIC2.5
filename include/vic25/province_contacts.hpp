#pragma once

#include "vic25/phase_e.hpp"
#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace vic25::phasee::geography_import {

enum class Kind { land, ocean, lake };
struct Endpoint { Kind kind{}; Id id{}; };
struct Contact {
    Endpoint a, b;
    std::uint64_t shared_sides{};
};

// Apply reviewed full-resolution sides to previously bound scenario IDs at
// setup. This only populates Phase E direct neighbors and coasts. A contact
// cannot identify a port, prove a through-route or create a transport service.
inline void seed_contacts(World& world,std::string_view dataset,
                          std::span<const Contact> contacts) {
    const auto& state=world.spatial();
    if(dataset.empty()||dataset!=state.dataset_hash||world.commit_id()!=0)
        throw std::invalid_argument("contact dataset or setup state mismatch");
    const auto valid=[&](Endpoint e) {
        if(e.id==0)return false;
        switch(e.kind) {
            case Kind::land:return state.provinces.contains(e.id);
            case Kind::ocean:
            case Kind::lake:return !state.provinces.contains(e.id);
        }
        return false;
    };
    std::map<Id,Kind> kinds;
    std::set<std::pair<Id,Id>> unique;
    for(const auto& [a,b,n]:contacts) {
        if(n==0||a.id==b.id||!valid(a)||!valid(b)||
           !unique.emplace(std::min(a.id,b.id),std::max(a.id,b.id)).second)
            throw std::invalid_argument("invalid or repeated province contact");
        for(const auto e:{a,b})
            if(const auto [it,inserted]=kinds.emplace(e.id,e.kind);
               !inserted&&it->second!=e.kind)
                throw std::invalid_argument("conflicting contact ID kind");
        if(a.kind==Kind::land&&b.kind==Kind::land&&
           state.provinces.at(a.id).neighbors.contains(b.id)!=
           state.provinces.at(b.id).neighbors.contains(a.id))
            throw std::invalid_argument("existing land border is not symmetric");
    }
    for(const auto& [a,b,n]:contacts) {
        (void)n;
        if(a.kind==Kind::land&&b.kind==Kind::land) {
            if(!state.provinces.at(a.id).neighbors.contains(b.id))
                world.seed_border(a.id,b.id);
        } else if(a.kind==Kind::land&&b.kind==Kind::ocean) {
            world.seed_coast(a.id,b.id);
        } else if(a.kind==Kind::ocean&&b.kind==Kind::land) {
            world.seed_coast(b.id,a.id);
        }
    }
}

} // namespace vic25::phasee::geography_import
