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

inline constexpr std::string_view food_good="food";

inline std::string food_seller(phasee::Id province) {
    return "history-food-seller-"+std::to_string(province);
}

struct FoodStart {
    phaseb::People people{};
    std::int64_t age_months{};
    phaseb::Sex sex{phaseb::Sex::unknown};
    std::string culture,faith;
    phasec::Money household_cash{},price{},seller_cash{};
    vic25::Quantity stock{};
};

inline std::int64_t start_value(std::string_view token,std::int64_t min,
                                std::int64_t max,const char* error) {
    std::int64_t value{};
    const auto end=token.data()+token.size();
    if(token.empty()||token.front()<'0'||token.front()>'9')throw std::invalid_argument(error);
    const auto result=std::from_chars(token.data(),end,value);
    if(result.ec!=std::errc{}||result.ptr!=end||value<min||value>max)
        throw std::invalid_argument(error);
    return value;
}

inline FoodStart parse_food_start(std::istringstream& row) {
    std::string people, age, sex, culture, faith, wallet, price, stock, seller, extra;
    if(!(row>>people>>age>>sex>>culture>>faith>>wallet>>price>>stock>>seller)||row>>extra)
        throw std::invalid_argument("malformed food-start record");
    if(!identifier(culture)||!identifier(faith))
        throw std::invalid_argument("invalid food-start culture or religion");
    const auto recorded_sex=sex=="F"?phaseb::Sex::female:sex=="M"?phaseb::Sex::male:
        phaseb::Sex::unknown;
    if(recorded_sex==phaseb::Sex::unknown&&sex!="U")
        throw std::invalid_argument("invalid food-start sex");
    constexpr std::int64_t limit=1'000'000'000;
    return {start_value(people,1,limit,"invalid food-start population"),
            start_value(age,0,1440,"invalid food-start age"),recorded_sex,
            std::move(culture),std::move(faith),
            start_value(wallet,0,limit,"invalid food-start household cash"),
            start_value(price,1,limit,"invalid food-start price"),
            start_value(seller,0,limit,"invalid food-start seller cash"),
            start_value(stock,0,limit,"invalid food-start stock")};
}

inline void seed_food_starts(phasee::World& world,const std::map<phasee::Id,FoodStart>& starts,
                             std::int64_t month) {
    auto& core=world.seed_core().seed_core();
    auto& pop=core.seed_population();
    for(const auto& [id,seed]:starts) {
        const auto& region=world.province(id).region;
        if(seed.age_months>month-12)
            throw std::invalid_argument("food-start age precedes supported calendar");
        const auto demographic=pop.demographic_profile({region,month-seed.age_months,seed.sex,
            seed.culture,seed.faith,"unspecified","unspecified",pop.default_health(),
            phaseb::Service::civilian});
        const auto economy=pop.economic_profile({region,"unassigned","unassigned","unassigned",
                                                  pop.default_education()});
        const auto home=pop.household_profile({region,"scenario-pool","shared",0});
        pop.seed_population(demographic,economy,home,seed.people);
        core.seed_actor("HH:"+std::to_string(home),seed.household_cash);
        const auto seller=food_seller(id);
        const std::string good(food_good);
        core.seed_actor(seller,seed.seller_cash);
        core.seed_market(region,good,seed.price,seed.price,seed.price);
        if(seed.stock>0)core.seed_stock(seller,region,good,seed.stock);
    }
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
       (magic!="VIC25-HISTORY-1"&&magic!="VIC25-HISTORY-2")||!identifier(dataset))
        throw std::invalid_argument("invalid historical start header");
    const bool economy_allowed=magic=="VIC25-HISTORY-2";
    const auto year=positive(year_token,"invalid start year");
    const auto month=positive(month_token,"invalid start month");
    if(year>9999)throw std::invalid_argument("invalid start year");
    if(month>12)throw std::invalid_argument("invalid start month");
    struct Node { geo::Kind kind; std::string admin,country; };
    struct ContactRow { Id a,b; std::uint64_t sides; };
    std::map<Id,Node> nodes;
    std::map<std::string,std::string> owners;
    std::vector<ContactRow> rows;
    std::map<Id,FoodStart> food_starts;
    while(std::getline(input,line)) {
        std::istringstream row(std::move(line));
        row.imbue(std::locale::classic());
        std::string type,id_text,a,b;
        if(!(row>>type>>id_text))
            throw std::invalid_argument("malformed record in historical start");
        const Id id=positive(id_text,"invalid province ID");
        if(type=="food-start") {
            if(!economy_allowed)throw std::invalid_argument("food-start requires VIC25-HISTORY-2");
            auto start=parse_food_start(row);
            if(!food_starts.emplace(id,std::move(start)).second)
                throw std::invalid_argument("duplicate food-start province");
            continue;
        }
        if(!(row>>a>>b)||row>>extra)
            throw std::invalid_argument("malformed record in historical start");
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
    if(economy_allowed&&food_starts.empty())
        throw std::invalid_argument("VIC25-HISTORY-2 requires a food-start record");
    for(const auto& [id,start]:food_starts) {
        (void)start;
        if(!nodes.contains(id)||nodes.at(id).kind!=geo::Kind::land)
            throw std::invalid_argument("food-start requires an existing land province");
    }
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
        if(node.kind!=geo::Kind::land) {
            world.seed_water_zone(id,node.kind==geo::Kind::ocean?
                                    phasee::WaterKind::ocean:phasee::WaterKind::lake);
            continue;
        }
        // Phase E requires a unique economic-region key per province. Land
        // provinces may still share a single administrative unit.
        world.seed_province(id,"history-province-"+std::to_string(id),node.country);
        world.seed_assignment(id,units.at(node.admin));
    }
    geo::seed_contacts(world,dataset,contacts);
    for(const auto& contact:contacts)
        if(contact.a.kind==geo::Kind::ocean&&contact.b.kind==geo::Kind::ocean)
            world.seed_ocean_border(contact.a.id,contact.b.id);
    if(!food_starts.empty())seed_food_starts(world,food_starts,daily::legacy_month(date));
    return daily::World::adopt(std::move(engine),0,std::string(origin_prefix)+dataset);
}

} // namespace vic25::history_start
