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

inline constexpr const char* food_good="food";
inline constexpr const char* processed_food_good="groceries";
inline constexpr const char* grain_good="grain";

inline std::string food_seller(phasee::Id province) {
    return "history-food-seller-"+std::to_string(province);
}

inline std::string food_mill(phasee::Id province) {
    return "history-food-mill-"+std::to_string(province);
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

struct MillStart {
    phaseb::People workers{};
    phasec::Money wage{},grain_price{};
    Quantity monthly_capacity{},grain_stock{};
};

inline MillStart parse_mill_start(std::istringstream& row) {
    std::string workers,wage,capacity,grain,quote,extra;
    if(!(row>>workers>>wage>>capacity>>grain>>quote)||row>>extra)
        throw std::invalid_argument("malformed food-mill record");
    constexpr std::int64_t limit=1'000'000'000;
    return {start_value(workers,1,limit,"invalid food-mill workers"),
            start_value(wage,1,limit,"invalid food-mill wage"),
            start_value(quote,1,limit,"invalid food-mill grain price"),
            start_value(capacity,1,limit,"invalid food-mill monthly capacity"),
            start_value(grain,0,limit,"invalid food-mill grain stock")};
}

struct ExecutiveStart {
    std::string head, minister;
    phasec::Money treasury{};
    int income_tax_ppm{};
};

inline ExecutiveStart parse_executive_start(std::istringstream& row) {
    std::string head,minister,treasury,percent,extra;
    if(!(row>>head>>minister>>treasury>>percent)||row>>extra||
       !identifier(head)||!identifier(minister))
        throw std::invalid_argument("malformed executive-start record");
    return {std::move(head),std::move(minister),
            start_value(treasury,0,1'000'000'000,"invalid executive-start treasury"),
            static_cast<int>(start_value(percent,0,100,"invalid executive-start income tax"))*10'000};
}

inline void seed_executives(phasec::IntegratedWorld& core,const std::map<std::string,ExecutiveStart>& starts) {
    using namespace phasec;
    for(const auto& [country,seed]:starts) {
        const auto treasury="history-treasury:"+country;
        core.seed_actor(treasury,seed.treasury);
        // No elected chamber. The native government record still requires
        // positive seat and quorum placeholders; neither is a fictional vote.
        core.seed_government(country,seed.head,seed.minister,1,1);
        core.seed_p1_constitution(country,{P1GovernancePrinciple::monarchy,
            P1PowerDistribution::autocracy,P1LegislatureKind::none,"history:executive-start"});
        core.seed_country_treasury(country,treasury);
        core.seed_country_income_tax(country,seed.income_tax_ppm);
    }
}

inline void seed_food_starts(phasee::World& world,const std::map<phasee::Id,FoodStart>& starts,
        const std::map<phasee::Id,MillStart>& mills,bool government_mode,std::int64_t month) {
    auto& industry=world.seed_core();
    auto& core=industry.seed_core();
    auto& pop=core.seed_population();
    if(!mills.empty()) {
        industry.seed_standard_industries();
        // Without a modeled public authority, this pilot charges no
        // withholding. Do not implicitly reuse the legacy 10% tax default.
        if(!government_mode)core.seed_legacy_income_tax(0);
    }
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
        const auto slice=pop.seed_population(demographic,economy,home,seed.people);
        core.seed_actor("HH:"+std::to_string(home),seed.household_cash);
        const auto seller=food_seller(id);
        const auto mill=mills.find(id);
        const std::string good=mill==mills.end()?food_good:processed_food_good;
        core.seed_actor(seller,seed.seller_cash);
        core.seed_market(region,good,seed.price,seed.price,seed.price);
        if(seed.stock>0)core.seed_stock(seller,region,good,seed.stock);
        if(mill==mills.end())continue;

        const auto& work=mill->second;
        // Normal no-government payroll requires a distinct country actor.
        // It has zero initial funds, and is not registered as a treasury.
        const auto& country=core.sovereignty(region);
        if(!government_mode&&!core.has_actor(country))core.seed_actor(country,0);
        const auto site=food_mill(id);
        industry.seed_building_facility(site,seller,region,work.monthly_capacity,
                                       "vic3_115.urban.food_industries");
        core.seed_market(region,grain_good,work.grain_price,work.grain_price,work.grain_price);
        if(work.grain_stock>0)core.seed_stock(seller,region,grain_good,work.grain_stock);
        phased::IndustryRecipe recipe;
        recipe.id=site;
        recipe.site=site;
        recipe.industry_class="MANU";
        recipe.inputs={{grain_good,1}};
        recipe.outputs={{processed_food_good,3}};
        recipe.workers_per_batch=1;
        industry.seed_building_recipe(std::move(recipe));
        pop.hire(slice,work.workers,seller,"worker",work.wage,{});
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
    std::map<Id,MillStart> mills;
    std::map<std::string,ExecutiveStart> executives;
    while(std::getline(input,line)) {
        std::istringstream row(std::move(line));
        row.imbue(std::locale::classic());
        std::string type,id_text,a,b;
        if(!(row>>type>>id_text))
            throw std::invalid_argument("malformed record in historical start");
        if(type=="executive-start") {
            if(!economy_allowed)throw std::invalid_argument("executive-start requires VIC25-HISTORY-2");
            if(!identifier(id_text))throw std::invalid_argument("invalid executive-start country");
            if(!executives.emplace(id_text,parse_executive_start(row)).second)
                throw std::invalid_argument("duplicate executive-start country");
            continue;
        }
        const Id id=positive(id_text,"invalid province ID");
        if(type=="food-start") {
            if(!economy_allowed)throw std::invalid_argument("food-start requires VIC25-HISTORY-2");
            auto start=parse_food_start(row);
            if(!food_starts.emplace(id,std::move(start)).second)
                throw std::invalid_argument("duplicate food-start province");
            continue;
        }
        if(type=="food-mill") {
            if(!economy_allowed)throw std::invalid_argument("food-mill requires VIC25-HISTORY-2");
            const auto mill=parse_mill_start(row);
            if(!mills.emplace(id,mill).second)
                throw std::invalid_argument("duplicate food-mill province");
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
    for(const auto& [id,mill]:mills) {
        const auto start=food_starts.find(id);
        if(start==food_starts.end())
            throw std::invalid_argument("food-mill requires a matching food-start");
        const phaseb::JobRequirements job;
        const auto& seed=start->second;
        if(mill.workers>seed.people||seed.age_months<job.minimum_age_months||
           seed.age_months>job.maximum_age_months)
            throw std::invalid_argument("invalid food-mill employment");
    }
    if(economy_allowed&&food_starts.empty())
        throw std::invalid_argument("VIC25-HISTORY-2 requires a food-start record");
    for(const auto& [id,start]:food_starts) {
        (void)start;
        if(!nodes.contains(id)||nodes.at(id).kind!=geo::Kind::land)
            throw std::invalid_argument("food-start requires an existing land province");
    }
    if(!executives.empty()) {
        std::set<std::string> countries;
        for(const auto& [admin,country]:owners) {
            (void)admin;
            countries.insert(country);
        }
        for(const auto& [country,seed]:executives) {
            (void)seed;
            if(!countries.contains(country))
                throw std::invalid_argument("executive-start requires a country with land");
        }
        for(const auto& [id,mill]:mills) {
            (void)mill;
            if(!executives.contains(nodes.at(id).country))
                throw std::invalid_argument("food-mill requires a modeled government when an executive-start is used");
        }
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
    if(!executives.empty())seed_executives(world.seed_core().seed_core(),executives);
    if(!food_starts.empty())
        seed_food_starts(world,food_starts,mills,!executives.empty(),daily::legacy_month(date));
    return daily::World::adopt(std::move(engine),0,std::string(origin_prefix)+dataset);
}

} // namespace vic25::history_start
