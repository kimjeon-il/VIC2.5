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
