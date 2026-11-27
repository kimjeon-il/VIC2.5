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

inline bool copper_trial_recipe(const phased::IndustryRecipe& recipe) {
    static constexpr std::string_view prefix="history-copper-telephone-";
    return recipe.id==recipe.site&&recipe.id.starts_with(prefix)&&
        recipe.outputs.size()==1&&recipe.outputs.front().good=="telephones"&&
        std::any_of(recipe.inputs.begin(),recipe.inputs.end(),
            [](const auto& input){return input.good=="copper"&&input.per_unit>0;});
}

struct CopperTrialOverview {
    Quantity copper_extracted{},telephones_made{};
    phasec::Money copper_receipts{},telephone_receipts{};
};

inline std::optional<CopperTrialOverview> copper_trial_overview(
        const daily::World& world,const std::string& country) {
    const auto& geo=world.engine().base();
    const auto& d=geo.core();
    const auto& c=d.core();
    std::set<std::string> plants;
    std::optional<CopperTrialOverview> result;
    for(const auto id:geo.sovereign_provinces(country)) {
        const auto plant=history_start::copper_trial_plant(id);
        const auto recipe=d.state().recipes.find(plant);
        if(recipe==d.state().recipes.end()||!copper_trial_recipe(recipe->second))continue;
        plants.insert(plant);
        if(!result)result.emplace();
        result->telephone_receipts=phaseb::add_checked(
            result->telephone_receipts,c.firm_finance(c.facility(plant).owner).receipts);
        const auto mine=d.state().resource_sites.find(history_start::copper_trial_mine(id));
        if(mine==d.state().resource_sites.end())continue;
        result->copper_extracted=phaseb::add_checked(
            result->copper_extracted,mine->second.cumulative_output);
        result->copper_receipts=phaseb::add_checked(
            result->copper_receipts,c.firm_finance(mine->second.owner).receipts);
    }
    if(result)
        for(const auto& actual:d.state().actual_production_runs)
            if(plants.contains(actual.recipe))
                result->telephones_made=phaseb::add_checked(result->telephones_made,
                    phaseb::mul_checked(actual.batches,
                        d.state().recipes.at(actual.recipe).outputs.front().per_unit));
    return result;
}

struct FoodPriceOverview {
    phasec::Money minimum{},maximum{};
    Quantity purchase_gap{};
};

struct GrainFarmOverview {
    std::size_t sites{};
    Quantity harvested{},saleable{};
    phasec::Money cash{};
};

struct FoodOverview {
    std::size_t covered_provinces{},mills{};
    phaseb::People people{};
    phasec::Money household_cash{},seller_cash{};
    vic25::Quantity food_stock{},grain_stock{},grain_in_transit{},mill_output_this_month{};
    std::optional<FoodPriceOverview> prices;
    GrainFarmOverview farms;
};

// Not a national estimate: only explicitly opened provincial food markets.
inline FoodOverview food_overview(const daily::World& world,const std::string& country) {
    const auto& geo=world.engine().base();
    const auto& core=geo.core().core();
    const auto& industry=geo.core().state();
    const auto& facilities=industry.site_archetypes;
    const auto& resources=industry.resource_sites;
    std::set<std::string> regions;
    std::map<std::string,std::string> mill_owners;
    std::set<std::string> mills;
    FoodOverview result;
    for(const auto& id:geo.sovereign_provinces(country)) {
        const auto& region=geo.province(id).region;
        if(const auto farm=resources.find(history_start::grain_farm(id));farm!=resources.end()) {
            const auto& site=farm->second;
            ++result.farms.sites;
            result.farms.harvested=phaseb::add_checked(result.farms.harvested,site.cumulative_output);
            result.farms.saleable=phaseb::add_checked(result.farms.saleable,
                core.stock(site.owner,region,history_start::grain_good));
            result.farms.cash=phaseb::add_checked(result.farms.cash,core.cash(site.owner));
        }
        const auto seller=history_start::food_seller(id);
        const auto mill_id=history_start::food_mill(id);
        const bool mill=facilities.contains(mill_id);
        const std::string good=mill?history_start::processed_food_good:history_start::food_good;
        if(!core.has_actor(seller)||!core.has_market(region,good))continue;
        if(mill) {
            ++result.mills;
            mills.insert(mill_id);
            mill_owners.emplace(region,seller);
            result.grain_stock=phaseb::add_checked(result.grain_stock,
                                                  core.stock(seller,region,history_start::grain_good));
        }
        const std::string alternate=history_start::alternate_food_good(good);
        const bool has_alternate=core.has_market(region,alternate);
        const auto record_food=[&](const std::string& product) {
            result.food_stock=phaseb::add_checked(result.food_stock,
                core.stock(seller,region,product));
            const auto& quote=core.market(region,product);
            if(quote.floor==quote.ceiling&&!has_alternate)return;
            if(!result.prices)
                result.prices=FoodPriceOverview{quote.price,quote.price,0};
            auto& prices=*result.prices;
            prices.minimum=std::min(prices.minimum,quote.price);
            prices.maximum=std::max(prices.maximum,quote.price);
            prices.purchase_gap=phaseb::add_checked(prices.purchase_gap,
                core.previous_daily_solvent_unmet(region,product,world.day_index()));
        };
        record_food(good);
        if(has_alternate)record_food(alternate);
        regions.insert(region);
        ++result.covered_provinces;
        result.seller_cash=phaseb::add_checked(result.seller_cash,core.cash(seller));
    }
    if(regions.empty())return result;
    if(!mills.empty()) {
        // The native run log is appended in simulation order. The calendar
        // can already show the first of the new month before its first run.
        const auto month=world.engine().month();
        const auto& runs=industry.actual_production_runs;
        for(auto run=runs.rbegin();run!=runs.rend()&&run->month>=month;++run) {
            if(run->month!=month||!mills.contains(run->recipe))continue;
            for(const auto& output:industry.recipes.at(run->recipe).outputs)
                if(output.good==history_start::processed_food_good)
                    result.mill_output_this_month=phaseb::add_checked(result.mill_output_this_month,
                        phaseb::mul_checked(output.per_unit,run->batches));
        }
    }
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

struct ScenarioSettings {
    std::optional<history_start::FreightWorks> freight;
    phasec::Money daily_cap{};
    history_start::FoodOwnerReserves food_owners;
    bool operator==(const ScenarioSettings&) const = default;
};
struct Opened {
    daily::World world;
    std::string selected;
    ScenarioSettings works;
};
inline Opened open(const std::filesystem::path& path) {
    ScenarioSettings works;
    auto world=history_start::open(path,&works.freight,&works.food_owners);
    return {std::move(world),{},std::move(works)};
}

inline constexpr std::string_view magic="VIC25-HISTORY-PLAY-1\n";
inline constexpr std::string_view works_magic="VIC25-HISTORY-PLAY-2\n";
inline constexpr std::string_view scenario_magic="VIC25-HISTORY-PLAY-3\n";
inline constexpr std::size_t max_owner_row_bytes=32;
inline constexpr std::uint64_t max_snapshot=850'000'000;

inline void validate(const Opened& state) {
    if(!history_start::matches_checkpoint(state.world))
        throw std::invalid_argument("checkpoint does not belong to historical start");
    if(!state.selected.empty()) {
        if(!history_start::identifier(state.selected)||
           state.world.engine().base().sovereign_provinces(state.selected).empty())
            throw std::invalid_argument("unknown historical country in checkpoint");
    }
    const auto& geo=state.world.engine().base();
    const auto& core=geo.core().core();
    if(state.works.food_owners.size()>history_start::max_food_owners)
        throw std::invalid_argument("too many historical food owners");
    for(const auto& [id,reserve]:state.works.food_owners) {
        const auto seller=history_start::food_seller(id);
        if(!geo.spatial().provinces.contains(id)||!core.has_actor(seller)||
           !core.p2_has_registered_company_equity(seller)||
           reserve<0||reserve>history_start::max_food_owner_reserve)
            throw std::invalid_argument("invalid historical food owner binding");
    }
    if(state.works.freight) {
        const auto& works=*state.works.freight;
        const auto& engine=state.world.engine();
        const auto& edges=engine.commerce().transport_edges;
        const auto edge=edges.find(works.edge());
        const auto& c=engine.base().core().core();
        if(edge==edges.end()||edge->second.capacity_site!=works.site()||
           edge->second.from!=works.source||edge->second.to!=works.destination||
           !c.has_country_treasury(works.sponsor)||!c.has_actor(works.builder()))
            throw std::invalid_argument("invalid historical public works binding");
    }
}

// The checksum covers the selected country as well as the unaltered native
// snapshot. Older history saves without any player selection keep native bytes.
inline std::string encode(const daily::World& world,std::string_view selected,
                          const ScenarioSettings& works={}) {
    auto snapshot=daily::SnapshotCodec::encode(world);
    if(snapshot.size()>max_snapshot)throw std::invalid_argument("historical checkpoint exceeds save limit");
    const bool with_owners=!works.food_owners.empty();
    if(selected.empty()&&!works.freight&&!with_owners)return snapshot;
    std::string bytes(with_owners?scenario_magic:works.freight?works_magic:magic);
    bytes+=selected;
    bytes+='\n';
    if(with_owners)bytes+=works.freight?"1\n":"0\n";
    if(works.freight) {
        const auto& f=*works.freight;
        bytes+=std::to_string(f.source)+' '+std::to_string(f.destination)+' '+f.sponsor+' '+
               std::to_string(f.work)+' '+std::to_string(f.monthly_capacity)+' '+
               std::to_string(f.invoice)+' '+std::to_string(f.reserve)+'\n';
        bytes+=std::to_string(works.daily_cap)+'\n';
    }
    if(with_owners) {
        bytes+=std::to_string(works.food_owners.size())+'\n';
        for(const auto& [id,reserve]:works.food_owners)
            bytes+=std::to_string(id)+' '+std::to_string(reserve)+'\n';
    }
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
    if(size>max_snapshot+scenario_magic.size()+64+1+2+257+17+5+8+
            history_start::max_food_owners*(max_owner_row_bytes+1))
        throw std::invalid_argument("historical checkpoint exceeds save limit");
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::invalid_argument("cannot open historical checkpoint");
    std::string bytes(static_cast<std::size_t>(size),'\0');
    if(!file.read(bytes.data(),static_cast<std::streamsize>(size))||
       file.peek()!=std::char_traits<char>::eof()||file.bad())
        throw std::invalid_argument("historical checkpoint changed or could not be read");
    std::string selected;
    ScenarioSettings works;
    bool with_works=bytes.starts_with(works_magic);
    const bool with_owners=bytes.starts_with(scenario_magic);
    if(with_owners||with_works||bytes.starts_with(magic)) {
        const auto header_size=with_owners?scenario_magic.size():
                               with_works?works_magic.size():magic.size();
        const auto eol=bytes.find('\n',header_size);
        if(eol==bytes.npos||eol-header_size>64||bytes.size()-eol<=9)
            throw std::invalid_argument("invalid historical checkpoint header");
        selected=bytes.substr(header_size,eol-header_size);
        if(((!with_works&&!with_owners)||!selected.empty())&&
           !history_start::identifier(selected))
            throw std::invalid_argument("invalid historical checkpoint country");
        std::istringstream trailer(bytes.substr(bytes.size()-8),std::ios::binary|std::ios::in);
        std::uint64_t expected{};
        phasec::persistence_detail::BinaryReader reader{trailer};
        reader(expected);
        bytes.resize(bytes.size()-8);
        if(phasec::persistence_detail::checksum(bytes)!=expected)
            throw std::invalid_argument("historical checkpoint checksum mismatch");
        auto payload=eol+1;
        if(with_owners) {
            if(bytes.size()-payload<2||bytes[payload+1]!='\n'||
               (bytes[payload]!='0'&&bytes[payload]!='1'))
                throw std::invalid_argument("invalid historical settings flags");
            with_works=bytes[payload]=='1';
            payload+=2;
        }
        if(with_works) {
            const auto end=bytes.find('\n',payload);
            if(end==bytes.npos||end-payload>256)
                throw std::invalid_argument("invalid historical public works header");
            std::istringstream row(bytes.substr(payload,end-payload));
            row.imbue(std::locale::classic());
            std::string source;
            if(!(row>>source))throw std::invalid_argument("missing historical public works source");
            works.freight=history_start::parse_freight_works(
                history_start::positive(source,"invalid historical public works source"),row);
            payload=end+1;
            const auto cap_end=bytes.find('\n',payload);
            if(cap_end==bytes.npos||cap_end-payload>16)
                throw std::invalid_argument("invalid historical public works ceiling");
            works.daily_cap=history_start::start_value(bytes.substr(payload,cap_end-payload),
                0,1'000'000,"invalid historical public works ceiling");
            payload=cap_end+1;
        }
        if(with_owners) {
            const auto count_end=bytes.find('\n',payload);
            if(count_end==bytes.npos||count_end-payload>4)
                throw std::invalid_argument("invalid historical food owner count");
            const auto count=history_start::start_value(bytes.substr(payload,count_end-payload),
                1,history_start::max_food_owners,"invalid historical food owner count");
            payload=count_end+1;
            for(std::int64_t i=0;i<count;++i) {
                const auto end=bytes.find('\n',payload);
                if(end==bytes.npos||end-payload>max_owner_row_bytes)
                    throw std::invalid_argument("invalid historical food owner row");
                std::istringstream row(bytes.substr(payload,end-payload));
                row.imbue(std::locale::classic());
                std::string id_text,reserve_text,extra;
                if(!(row>>id_text>>reserve_text)||row>>extra)
                    throw std::invalid_argument("invalid historical food owner row");
                const auto id=history_start::positive(id_text,"invalid historical food owner");
                const auto reserve=history_start::start_value(reserve_text,0,
                    history_start::max_food_owner_reserve,"invalid historical food owner reserve");
                if(!works.food_owners.emplace(id,reserve).second)
                    throw std::invalid_argument("duplicate historical food owner");
                payload=end+1;
            }
        }
        if(bytes.size()-payload>max_snapshot)
            throw std::invalid_argument("historical checkpoint exceeds save limit");
        bytes.erase(0,payload);
    }
    Opened result{daily::SnapshotCodec::decode(bytes),std::move(selected),std::move(works)};
    validate(result);
    return result;
}

} // namespace vic25::history_play
