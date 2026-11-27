#pragma once

#include "vic25/daily_time.hpp"
#include "vic25/province_contacts.hpp"
#include <charconv>
#include <filesystem>
#include <fstream>
#include <locale>
#include <optional>
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
using FoodOwnerReserves=std::map<phasee::Id,phasec::Money>;
inline constexpr std::size_t max_food_owners=1024;
inline constexpr phasec::Money max_food_owner_reserve=1'000'000'000;

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
    phasec::Money household_cash{},price{},seller_cash{},price_floor{},price_ceiling{};
    vic25::Quantity stock{};
};

struct FoodAlternate {
    phasec::Money price{};
    Quantity stock{};
};

inline const char* alternate_food_good(std::string_view good) {
    return good==food_good?processed_food_good:food_good;
}

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

inline FoodAlternate parse_food_alternate(std::istringstream& row) {
    std::string price,stock,extra;
    if(!(row>>price>>stock)||row>>extra)
        throw std::invalid_argument("malformed food-alternate record");
    constexpr std::int64_t limit=1'000'000'000;
    return {start_value(price,1,limit,"invalid food-alternate price"),
            start_value(stock,0,limit,"invalid food-alternate stock")};
}

inline FoodStart parse_food_start(std::istringstream& row) {
    std::string people, age, sex, culture, faith, wallet, price, stock, seller, extra;
    if(!(row>>people>>age>>sex>>culture>>faith>>wallet>>price>>stock>>seller))
        throw std::invalid_argument("malformed food-start record");
    if(!identifier(culture)||!identifier(faith))
        throw std::invalid_argument("invalid food-start culture or religion");
    const auto recorded_sex=sex=="F"?phaseb::Sex::female:sex=="M"?phaseb::Sex::male:
        phaseb::Sex::unknown;
    if(recorded_sex==phaseb::Sex::unknown&&sex!="U")
        throw std::invalid_argument("invalid food-start sex");
    constexpr std::int64_t limit=1'000'000'000;
    const auto initial=start_value(price,1,limit,"invalid food-start price");
    auto floor=initial,ceiling=initial;
    std::string floor_text,ceiling_text;
    if(row>>floor_text) {
        if(!(row>>ceiling_text)||row>>extra)
            throw std::invalid_argument("malformed food-start price band");
        floor=start_value(floor_text,1,limit,"invalid food-start price floor");
        ceiling=start_value(ceiling_text,1,limit,"invalid food-start price ceiling");
        if(floor>initial||initial>ceiling)
            throw std::invalid_argument("food-start price outside band");
    }
    return {start_value(people,1,limit,"invalid food-start population"),
            start_value(age,0,1440,"invalid food-start age"),recorded_sex,
            std::move(culture),std::move(faith),
            start_value(wallet,0,limit,"invalid food-start household cash"),
            initial,start_value(seller,0,limit,"invalid food-start seller cash"),
            floor,ceiling,start_value(stock,0,limit,"invalid food-start stock")};
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

inline std::string grain_supplier(phasee::Id province) {
    return "history-grain-supply-"+std::to_string(province);
}

inline std::string grain_farm(phasee::Id province) {
    return "history-grain-farm-"+std::to_string(province);
}

struct FarmStart {
    phaseb::People workers{};
    phasec::Money wage{},cash{};
    Quantity capacity{},standing{},renewal{};
};

inline FarmStart parse_farm_start(std::istringstream& row) {
    std::string workers,wage,cash,capacity,standing,renewal,extra;
    if(!(row>>workers>>wage>>cash>>capacity>>standing>>renewal)||row>>extra)
        throw std::invalid_argument("malformed grain-farm record");
    constexpr std::int64_t limit=1'000'000'000;
    return {start_value(workers,1,limit,"invalid grain-farm workers"),
            start_value(wage,1,limit,"invalid grain-farm wage"),
            start_value(cash,0,limit,"invalid grain-farm cash"),
            start_value(capacity,1,limit,"invalid grain-farm capacity"),
            start_value(standing,1,limit,"invalid grain-farm standing crop"),
            start_value(renewal,0,limit,"invalid grain-farm renewal")};
}

struct GrainSupply {
    Quantity stock{};
    phasec::Money price{};
};

inline GrainSupply parse_grain_supply(std::istringstream& row) {
    std::string stock,price,extra;
    if(!(row>>stock>>price)||row>>extra)
        throw std::invalid_argument("malformed grain-supply record");
    return {start_value(stock,0,1'000'000'000,"invalid grain-supply stock"),
            start_value(price,1,1'000'000'000,"invalid grain-supply price")};
}

struct GrainRoad {
    Quantity monthly_capacity{};
    phasec::Money freight_per_unit{};
    int days{};
    std::string evidence;
};

inline GrainRoad parse_grain_road(std::istringstream& row) {
    std::string capacity,freight,days,evidence,extra;
    if(!(row>>capacity>>freight>>days>>evidence)||row>>extra||!identifier(evidence))
        throw std::invalid_argument("malformed grain-road record");
    return {start_value(capacity,1,1'000'000'000,"invalid grain-road capacity"),
            start_value(freight,0,1'000'000'000,"invalid grain-road freight"),
            static_cast<int>(start_value(days,1,366,"invalid grain-road transit days")),
            std::move(evidence)};
}

struct FreightWorks {
    phasee::Id source{},destination{};
    std::string sponsor;
    Quantity work{},monthly_capacity{};
    phasec::Money invoice{},reserve{};
    bool operator==(const FreightWorks&) const = default;

    std::string edge() const {
        return "history-grain-road-"+std::to_string(source)+"-"+std::to_string(destination);
    }
    std::string project() const {return edge()+"-works";}
    std::string site() const {return edge()+"-terminal";}
    std::string builder() const {return edge()+"-builder";}
};
inline constexpr std::string_view freight_material="wood";

inline FreightWorks parse_freight_works(phasee::Id source,std::istringstream& row) {
    std::string destination,sponsor,work,capacity,invoice,reserve,extra;
    if(!(row>>destination>>sponsor>>work>>capacity>>invoice>>reserve)||row>>extra||!identifier(sponsor))
        throw std::invalid_argument("malformed freight-works record");
    return {source,positive(destination,"invalid freight-works destination"),std::move(sponsor),
        start_value(work,1,1'000'000,"invalid freight-works work"),
        start_value(capacity,1,1'000'000,"invalid freight-works capacity"),
        start_value(invoice,1,1'000'000,"invalid freight-works invoice"),
        start_value(reserve,0,1'000'000,"invalid freight-works reserve")};
}

inline void seed_grain_freight(phasegh::World& engine,
        const std::map<phasee::Id,GrainSupply>& supplies,
        const std::map<phasee::Id,FarmStart>& farms,
        const std::map<std::pair<phasee::Id,phasee::Id>,GrainRoad>& roads,
        std::int64_t month,const std::optional<FreightWorks>& works) {
    const auto& geo=engine.base();
    auto& core=engine.seed_geography().seed_core().seed_core();
    for(const auto& [id,supply]:supplies) {
        const auto& region=geo.province(id).region;
        const auto seller=grain_supplier(id);
        if(!farms.contains(id))core.seed_actor(seller,0);
        core.seed_market(region,grain_good,supply.price,supply.price,supply.price);
        if(supply.stock>0)core.seed_stock(seller,region,grain_good,supply.stock);
    }
    for(const auto& [pair,road]:roads) {
        const auto [source,dest]=pair;
        const auto id="history-grain-road-"+std::to_string(source)+"-"+std::to_string(dest);
        const auto carrier=id+"-carrier";
        core.seed_actor(carrier,0);
        engine.seed_verified_geographic_link(id,source,dest,"road");
        phasegh::TransportEdge edge;
        edge.id=id;
        edge.from=source;
        edge.to=dest;
        edge.carrier=carrier;
        edge.mode="road";
        edge.monthly_capacity=road.monthly_capacity;
        edge.freight_per_unit=road.freight_per_unit;
        edge.transit_days=road.days;
        edge.evidence_id=road.evidence;
        edge.valid_from_month=month;
        edge.valid_through_month=daily::legacy_month({9999,12,1});
        if(works&&works->source==source&&works->destination==dest)
            edge.capacity_site=works->site();
        engine.seed_transport_edge(std::move(edge));
    }
}

inline void seed_food_starts(phasee::World& world,const std::map<phasee::Id,FoodStart>& starts,
        const std::map<phasee::Id,MillStart>& mills,const std::map<phasee::Id,FarmStart>& farms,
        const std::map<phasee::Id,FoodAlternate>& alternates,
        const FoodOwnerReserves& food_owners,
        bool government_mode,std::int64_t month,const std::optional<FreightWorks>& works) {
    auto& industry=world.seed_core();
    auto& core=industry.seed_core();
    auto& pop=core.seed_population();
    if(!mills.empty()||!farms.empty()) {
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
        const bool construction_crew=works&&works->source==id;
        const auto slice=pop.seed_population(demographic,economy,home,
                                             seed.people-(construction_crew?1:0));
        core.seed_actor("HH:"+std::to_string(home),seed.household_cash);
        const auto seller=food_seller(id);
        const auto mill=mills.find(id);
        const auto farm=farms.find(id);
        const std::string good=mill==mills.end()?food_good:processed_food_good;
        core.seed_actor(seller,seed.seller_cash);
        if(food_owners.contains(id))
            core.seed_shares("company:"+seller,"HH:"+std::to_string(home),1);
        core.seed_market(region,good,seed.price,seed.price_floor,seed.price_ceiling);
        if(seed.stock>0)core.seed_stock(seller,region,good,seed.stock);
        if(const auto extra=alternates.find(id);extra!=alternates.end()) {
            const std::string alternate=alternate_food_good(good);
            const auto& quote=extra->second;
            core.seed_market(region,alternate,quote.price,quote.price,quote.price);
            if(quote.stock>0)core.seed_stock(seller,region,alternate,quote.stock);
        }
        if(construction_crew) {
            const auto builder=works->builder();
            const std::string material(freight_material);
            core.seed_actor(builder,0);
            core.seed_market(region,material,2,2,2);
            core.seed_stock(builder,region,material,works->work);
            const auto crew=pop.seed_population(demographic,economy,home,1,2);
            pop.hire(crew,1,builder,"worker",0,{});
        }
        if(mill==mills.end()&&farm==farms.end())continue;
        // Normal no-government payroll requires a distinct country actor.
        // It has zero initial funds, and is not registered as a treasury.
        const auto& country=core.sovereignty(region);
        if(!government_mode&&!core.has_actor(country))core.seed_actor(country,0);
        if(farm!=farms.end()) {
            const auto& fields=farm->second;
            const auto owner=grain_supplier(id);
            const auto site=grain_farm(id);
            core.seed_actor(owner,fields.cash);
            // The farm record explicitly grants the scenario's operator
            // the native land title; this asserts no historical ownership.
            const auto title="land:"+region+":grain";
            core.seed_shares(title,owner,1);
            world.seed_building_in_province(site,owner,id,fields.capacity,
                                            "vic3_115.rural.wheat_farms",title);
            phased::ResourceSite crop;
            crop.id=site;crop.site=site;crop.owner=owner;crop.region=region;crop.good=grain_good;
            crop.monthly_capacity=fields.capacity;
            crop.renewable=true;
            crop.standing_biomass=crop.carrying_capacity=fields.standing;
            crop.renewal_per_month=fields.renewal;
            industry.seed_building_resource_site(std::move(crop));
            pop.hire(slice,fields.workers,owner,"farm_worker",fields.wage,{});
            continue;
        }
        const auto& work=mill->second;
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
inline daily::World open(const std::filesystem::path& path,
                         std::optional<FreightWorks>* opened_works=nullptr,
                         FoodOwnerReserves* opened_food_owners=nullptr) {
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
    std::map<Id,FoodAlternate> alternates;
    FoodOwnerReserves food_owners;
    std::map<Id,MillStart> mills;
    std::map<std::string,ExecutiveStart> executives;
    std::map<std::string,int> grain_tariffs;
    std::map<Id,GrainSupply> supplies;
    std::map<Id,FarmStart> farms;
    std::map<std::pair<Id,Id>,GrainRoad> roads;
    std::optional<FreightWorks> works;
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
        if(type=="grain-tariff") {
            if(!economy_allowed)throw std::invalid_argument("grain-tariff requires VIC25-HISTORY-2");
            if(!identifier(id_text)||!(row>>a)||row>>extra)
                throw std::invalid_argument("malformed grain-tariff record");
            const auto ppm=static_cast<int>(start_value(a,0,100,"invalid grain-tariff percent"))*10'000;
            if(!grain_tariffs.emplace(id_text,ppm).second)
                throw std::invalid_argument("duplicate grain-tariff country");
            continue;
        }
        const Id id=positive(id_text,"invalid province ID");
        if(type=="grain-supply") {
            if(!economy_allowed)throw std::invalid_argument("grain-supply requires VIC25-HISTORY-2");
            if(!supplies.emplace(id,parse_grain_supply(row)).second)
                throw std::invalid_argument("duplicate grain-supply province");
            continue;
        }
        if(type=="grain-farm") {
            if(!economy_allowed)throw std::invalid_argument("grain-farm requires VIC25-HISTORY-2");
            if(!farms.emplace(id,parse_farm_start(row)).second)
                throw std::invalid_argument("duplicate grain-farm province");
            continue;
        }
        if(type=="freight-works") {
            if(!economy_allowed||works)throw std::invalid_argument("unexpected or duplicate freight-works");
            works=parse_freight_works(id,row);
            continue;
        }
        if(type=="grain-road") {
            if(!economy_allowed)throw std::invalid_argument("grain-road requires VIC25-HISTORY-2");
            if(!(row>>a))throw std::invalid_argument("malformed grain-road record");
            const Id dest=positive(a,"invalid grain-road destination");
            if(!roads.emplace(std::pair{id,dest},parse_grain_road(row)).second)
                throw std::invalid_argument("duplicate grain-road");
            continue;
        }
        if(type=="food-start") {
            if(!economy_allowed)throw std::invalid_argument("food-start requires VIC25-HISTORY-2");
            auto start=parse_food_start(row);
            if(!food_starts.emplace(id,std::move(start)).second)
                throw std::invalid_argument("duplicate food-start province");
            continue;
        }
        if(type=="food-owner") {
            if(!economy_allowed)throw std::invalid_argument("food-owner requires VIC25-HISTORY-2");
            if(!(row>>a)||row>>extra)
                throw std::invalid_argument("malformed food-owner record");
            const auto reserve=start_value(a,0,max_food_owner_reserve,"invalid food-owner reserve");
            if(!food_owners.emplace(id,reserve).second)
                throw std::invalid_argument("duplicate food-owner province");
            if(food_owners.size()>max_food_owners)
                throw std::invalid_argument("too many food-owner records");
            continue;
        }
        if(type=="food-alternate") {
            if(!economy_allowed)throw std::invalid_argument("food-alternate requires VIC25-HISTORY-2");
            if(!alternates.emplace(id,parse_food_alternate(row)).second)
                throw std::invalid_argument("duplicate food-alternate province");
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
    for(const auto& [id,reserve]:food_owners) {
        (void)reserve;
        if(!food_starts.contains(id))
            throw std::invalid_argument("food-owner requires a matching food-start");
    }
    for(const auto& [id,alternate]:alternates) {
        (void)alternate;
        if(!food_starts.contains(id))
            throw std::invalid_argument("food-alternate requires a matching food-start");
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
    for(const auto& [id,farm]:farms) {
        if(!food_starts.contains(id)||!supplies.contains(id))
            throw std::invalid_argument("grain-farm requires a local food-start and grain-supply");
        const auto& start=food_starts.at(id);
        const phaseb::JobRequirements job;
        if(farm.workers>start.people||start.age_months<job.minimum_age_months||
           start.age_months>job.maximum_age_months)
            throw std::invalid_argument("invalid grain-farm employment");
        if(!executives.empty()&&!executives.contains(nodes.at(id).country))
            throw std::invalid_argument("grain-farm requires a modeled government when an executive-start is used");
    }
    for(const auto& [id,supply]:supplies) {
        if(supply.stock==0&&!farms.contains(id))
            throw std::invalid_argument("zero initial grain requires a grain-farm");
        const auto node=nodes.find(id);
        if(node==nodes.end()||node->second.kind!=geo::Kind::land||mills.contains(id))
            throw std::invalid_argument("grain-supply requires an existing land province without a food mill");
    }
    std::set<std::string> grain_importers;
    for(const auto& [pair,road]:roads) {
        (void)road;
        const auto [source,dest]=pair;
        if(!supplies.contains(source)||!mills.contains(dest))
            throw std::invalid_argument("grain-road requires a grain supplier and food mill");
        const auto& exporter=nodes.at(source).country;
        const auto& importer=nodes.at(dest).country;
        if(exporter!=importer) {
            if(!executives.contains(exporter)||!executives.contains(importer))
                throw std::invalid_argument("cross-border grain-road requires both governments");
            grain_importers.insert(importer);
        }
    }
    if(works) {
        if(!roads.contains({works->source,works->destination})||
           !food_starts.contains(works->source)||!farms.contains(works->source)||
           !executives.contains(works->sponsor)||nodes.at(works->source).country!=works->sponsor)
            throw std::invalid_argument("freight-works requires an authorized source grain road");
        const auto& seed=food_starts.at(works->source);
        const phaseb::JobRequirements job;
        if(seed.people<=farms.at(works->source).workers||seed.age_months<job.minimum_age_months||
           seed.age_months>job.maximum_age_months)
            throw std::invalid_argument("freight-works requires one available construction worker");
    }
    for(const auto& [country,ppm]:grain_tariffs) {
        (void)ppm;
        if(!grain_importers.contains(country))
            throw std::invalid_argument("grain-tariff requires a cross-border grain import");
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
    for(const auto& [pair,road]:roads) {
        (void)road;
        if(!world.province(pair.first).neighbors.contains(pair.second))
            throw std::invalid_argument("grain-road requires adjacent land provinces");
    }
    if(!food_starts.empty())
        seed_food_starts(world,food_starts,mills,farms,alternates,food_owners,!executives.empty(),daily::legacy_month(date),works);
    if(!supplies.empty())
        seed_grain_freight(engine,supplies,farms,roads,daily::legacy_month(date),works);
    for(const auto& [country,ppm]:grain_tariffs)
        engine.seed_tariff(country,grain_good,ppm);
    if(opened_works)*opened_works=works;
    if(opened_food_owners)*opened_food_owners=std::move(food_owners);
    return daily::World::adopt(std::move(engine),0,std::string(origin_prefix)+dataset);
}

} // namespace vic25::history_start
