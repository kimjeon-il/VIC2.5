#pragma once

#include "vic25/daily_snapshot.hpp"
#include "vic25/history_start.hpp"

namespace vic25::history_play {

struct Territory {
    std::size_t land{}, administrations{}, coastal{};
};
using Countries=std::map<std::string,Territory>;

// The historical preview can advance dates but cannot change sovereignty.
inline Countries country_catalog(const daily::World& world) {
    const auto& geo=world.engine().base();
    Countries result;
    std::map<std::string,std::set<phasee::Id>> administrations;
    for(const auto& [id,province]:geo.spatial().provinces) {
        const auto country=geo.sovereign(id);
        auto& entry=result[country];
        ++entry.land;
        if(!province.coastal_seas.empty())++entry.coastal;
        administrations[country].insert(geo.leaf(id));
    }
    for(auto& [tag,entry]:result)entry.administrations=administrations.at(tag).size();
    return result;
}

struct FoodPriceOverview {
    phasec::Money minimum{},maximum{};
    Quantity purchase_gap{};
};

struct FoodOverview {
    std::size_t covered_provinces{},mills{};
    phaseb::People people{};
    phasec::Money household_cash{},seller_cash{};
    vic25::Quantity food_stock{},grain_stock{},grain_in_transit{};
    std::optional<FoodPriceOverview> prices;
};

// Not a national estimate: only explicitly opened provincial food markets.
inline FoodOverview food_overview(const daily::World& world,const std::string& country) {
    const auto& geo=world.engine().base();
    const auto& core=geo.core().core();
    const auto& facilities=geo.core().state().site_archetypes;
    std::set<std::string> regions;
    std::map<std::string,std::string> mill_owners;
    FoodOverview result;
    for(const auto& id:geo.sovereign_provinces(country)) {
        const auto& region=geo.province(id).region;
        const auto seller=history_start::food_seller(id);
        const bool mill=facilities.contains(history_start::food_mill(id));
        const std::string good=mill?history_start::processed_food_good:history_start::food_good;
        if(!core.has_actor(seller)||!core.has_market(region,good))continue;
        if(mill) {
            ++result.mills;
            mill_owners.emplace(region,seller);
            result.grain_stock=phaseb::add_checked(result.grain_stock,
                                                  core.stock(seller,region,history_start::grain_good));
        }
        const auto& quote=core.market(region,good);
        if(quote.floor<quote.ceiling) {
            if(!result.prices)
                result.prices=FoodPriceOverview{quote.price,quote.price,0};
            auto& prices=*result.prices;
            prices.minimum=std::min(prices.minimum,quote.price);
            prices.maximum=std::max(prices.maximum,quote.price);
            prices.purchase_gap=phaseb::add_checked(prices.purchase_gap,
                core.previous_daily_solvent_unmet(region,good,world.day_index()));
        }
        regions.insert(region);
        ++result.covered_provinces;
        result.food_stock=phaseb::add_checked(result.food_stock,core.stock(seller,region,good));
        result.seller_cash=phaseb::add_checked(result.seller_cash,core.cash(seller));
    }
    if(regions.empty())return result;
    if(!mill_owners.empty())
        for(const auto& [id,cargo]:world.engine().commerce().daily_shipments) {
            (void)id;
            const auto mill=mill_owners.find(cargo.destination);
            if(cargo.good==history_start::grain_good&&mill!=mill_owners.end()&&mill->second==cargo.buyer)
                result.grain_in_transit=phaseb::add_checked(result.grain_in_transit,cargo.quantity);
        }
    const auto& people=core.people();
    std::set<phaseb::Id> households;
    for(const auto& [id,slice]:people.slices()) {
        (void)id;
        if(!regions.contains(people.demographic(slice.key.demographic).region))continue;
        result.people=phaseb::add_checked(result.people,slice.count);
        households.insert(slice.key.household);
    }
    for(const auto home:households) {
        const auto wallet="HH:"+std::to_string(home);
        if(core.has_actor(wallet))
            result.household_cash=phaseb::add_checked(result.household_cash,core.cash(wallet));
    }
    return result;
}

struct Opened {
    daily::World world;
    std::string selected;
};

inline constexpr std::string_view magic="VIC25-HISTORY-PLAY-1\n";
inline constexpr std::uint64_t max_snapshot=850'000'000;

inline void validate(const Opened& state) {
    if(!history_start::matches_checkpoint(state.world))
        throw std::invalid_argument("checkpoint does not belong to historical start");
    if(!state.selected.empty()) {
        if(!history_start::identifier(state.selected)||
           state.world.engine().base().sovereign_provinces(state.selected).empty())
            throw std::invalid_argument("unknown historical country in checkpoint");
    }
}

// The checksum covers the selected country as well as the unaltered native
// snapshot. Older history saves without any player selection keep native bytes.
inline std::string encode(const daily::World& world,std::string_view selected) {
    auto snapshot=daily::SnapshotCodec::encode(world);
    if(snapshot.size()>max_snapshot)throw std::invalid_argument("historical checkpoint exceeds save limit");
    if(selected.empty())return snapshot;
    std::string bytes(magic);
    bytes+=selected;
    bytes+='\n';
    bytes+=snapshot;
    std::ostringstream trailer(std::ios::binary|std::ios::out);
    phasec::persistence_detail::BinaryWriter writer{trailer};
    writer(phasec::persistence_detail::checksum(bytes));
    bytes+=trailer.str();
    return bytes;
}

inline Opened load(const std::filesystem::path& path) {
    namespace fs=std::filesystem;
    if(!fs::is_regular_file(path))
        throw std::invalid_argument("historical checkpoint must use a regular file");
    const auto size=fs::file_size(path);
    if(size>max_snapshot+magic.size()+64+1+8)
        throw std::invalid_argument("historical checkpoint exceeds save limit");
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::invalid_argument("cannot open historical checkpoint");
    std::string bytes(static_cast<std::size_t>(size),'\0');
    if(!file.read(bytes.data(),static_cast<std::streamsize>(size))||
       file.peek()!=std::char_traits<char>::eof()||file.bad())
        throw std::invalid_argument("historical checkpoint changed or could not be read");
    std::string selected;
    if(bytes.starts_with(magic)) {
        const auto eol=bytes.find('\n',magic.size());
        if(eol==bytes.npos||eol-magic.size()>64||bytes.size()-eol<=9)
            throw std::invalid_argument("invalid historical checkpoint header");
        selected=bytes.substr(magic.size(),eol-magic.size());
        if(!history_start::identifier(selected))
            throw std::invalid_argument("invalid historical checkpoint country");
        std::istringstream trailer(bytes.substr(bytes.size()-8),std::ios::binary|std::ios::in);
        std::uint64_t expected{};
        phasec::persistence_detail::BinaryReader reader{trailer};
        reader(expected);
        bytes.resize(bytes.size()-8);
        if(phasec::persistence_detail::checksum(bytes)!=expected)
            throw std::invalid_argument("historical checkpoint checksum mismatch");
        bytes.erase(0,eol+1);
    }
    Opened result{daily::SnapshotCodec::decode(bytes),std::move(selected)};
    validate(result);
    return result;
}

} // namespace vic25::history_play
