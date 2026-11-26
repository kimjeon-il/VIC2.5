#pragma once

#include "vic25/daily_time.hpp"
#include "vic25/province_contacts.hpp"
#include <charconv>
#include <filesystem>
#include <fstream>
#include <locale>
#include <sstream>

namespace vic25::history_start {

inline constexpr std::string_view origin_prefix="history-v1:";
inline constexpr std::size_t size_limit=8*1024*1024;

inline std::uint64_t positive(std::string_view token,const char* error) {
    std::uint64_t value{};
    if(token.empty())throw std::invalid_argument(error);
    const auto end=token.data()+token.size();
    const auto result=std::from_chars(token.data(),end,value);
    if(result.ec!=std::errc{}||result.ptr!=end||value==0)
        throw std::invalid_argument(error);
    return value;
}

inline bool identifier(std::string_view token) {
    if(token.empty()||token.size()>64)return false;
    for(const char ch:token)
        if(!((ch>='A'&&ch<='Z')||(ch>='a'&&ch<='z')||
             (ch>='0'&&ch<='9')||ch=='_'||ch=='-'||ch=='.'))
            return false;
    return token!="-";
}

inline bool matches_checkpoint(const daily::World& world) {
    const auto& dataset=world.engine().base().spatial().dataset_hash;
    return identifier(dataset)&&world.migration_origin()==std::string(origin_prefix)+dataset&&
           !world.engine().base().spatial().provinces.empty();
}

// Read a bounded, explicit startup dataset. Geographic contacts never
// bootstrap ports, markets or transport services, even across the same sea.
inline daily::World open(const std::filesystem::path& path) {
    namespace geo=phasee::geography_import;
    namespace fs=std::filesystem;
    using phasee::Id;
    if(!fs::is_regular_file(path))
        throw std::invalid_argument("historical start must use a regular file");
    const auto size=fs::file_size(path);
    if(size>size_limit)throw std::invalid_argument("historical start exceeds the 8 MiB limit");
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::invalid_argument("cannot open historical start");
    std::string payload(static_cast<std::size_t>(size),'\0');
    if(!file.read(payload.data(),static_cast<std::streamsize>(size))||
       file.peek()!=std::char_traits<char>::eof()||file.bad())
        throw std::invalid_argument("historical start changed or could not be read");
    std::istringstream input(std::move(payload));
    std::string line,magic,dataset,year_token,month_token,extra;
    std::getline(input,line);
    std::istringstream header(line);
    header.imbue(std::locale::classic());
    if(!(header>>magic>>dataset>>year_token>>month_token)||header>>extra||
       magic!="VIC25-HISTORY-1"||!identifier(dataset))
        throw std::invalid_argument("invalid historical start header");
    const auto year=positive(year_token,"invalid start year");
    const auto month=positive(month_token,"invalid start month");
    if(year>9999)throw std::invalid_argument("invalid start year");
    if(month>12)throw std::invalid_argument("invalid start month");
    struct Node { geo::Kind kind; std::string admin,country; };
    struct ContactRow { Id a,b; std::uint64_t sides; };
    std::map<Id,Node> nodes;
    std::map<std::string,std::string> owners;
    std::vector<ContactRow> rows;
    while(std::getline(input,line)) {
        std::istringstream row(std::move(line));
        row.imbue(std::locale::classic());
        std::string type,id_text,a,b;
        if(!(row>>type>>id_text>>a>>b)||row>>extra)
            throw std::invalid_argument("malformed record in historical start");
        const Id id=positive(id_text,"invalid province ID");
        if(type=="contact") {
            rows.push_back({id,positive(a,"invalid province ID"),positive(b,"invalid shared sides")});
            continue;
        }
        geo::Kind kind;
        if(type=="land") {
            kind=geo::Kind::land;
            if(!identifier(a)||!identifier(b))
                throw std::invalid_argument("invalid land administration or country");
            const auto [it,inserted]=owners.emplace(a,b);
            if(!inserted&&it->second!=b)
                throw std::invalid_argument("conflicting country for one administration");
        } else if(type=="ocean"||type=="lake") {
            kind=type=="ocean"?geo::Kind::ocean:geo::Kind::lake;
            if(a!="-"||b!="-")
                throw std::invalid_argument("water records carry no land ownership");
        } else throw std::invalid_argument("unknown record in historical start");
        if(!nodes.emplace(id,Node{kind,std::move(a),std::move(b)}).second)
            throw std::invalid_argument("duplicate province ID in historical start");
    }
    if(owners.empty())throw std::invalid_argument("historical start has no land provinces");
    std::vector<geo::Contact> contacts;
    contacts.reserve(rows.size());
    for(const auto& [a,b,sides]:rows) {
        const auto from=nodes.find(a),to=nodes.find(b);
        if(from==nodes.end()||to==nodes.end())
            throw std::invalid_argument("contact refers to an unknown province");
        contacts.push_back({{from->second.kind,a},{to->second.kind,b},sides});
    }

    const daily::Date date{static_cast<int>(year),static_cast<int>(month),1};
    phasegh::World engine(daily::legacy_month(date));
    auto& world=engine.seed_geography();
    world.set_dataset(dataset);
    std::map<std::string,Id> units;
    for(const auto& [group,owner]:owners) {
        const Id unit=units.size()+1;
        world.seed_unit(unit,owner,group);
        units.emplace(group,unit);
    }
    for(const auto& [id,node]:nodes) {
        if(node.kind!=geo::Kind::land)continue;
        // Phase E requires a unique economic-region key per province. Land
        // provinces may still share a single administrative unit.
        world.seed_province(id,"history-province-"+std::to_string(id),node.country);
        world.seed_assignment(id,units.at(node.admin));
    }
    geo::seed_contacts(world,dataset,contacts);
    return daily::World::adopt(std::move(engine),0,std::string(origin_prefix)+dataset);
}

} // namespace vic25::history_start
