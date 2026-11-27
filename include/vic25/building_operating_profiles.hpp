#pragma once
// VIC2.5 P1 building operation profiles. These are required commodity names,
// occupation labels and siting requirements, NOT Victoria 3 fixed coefficients.
// Numerical throughput, output ratios, wages, construction prices and yields
// must come from a scenario/ruleset and the real C/D economy ledgers.
#include "vic25/building_catalog.hpp"
#include <array>
#include <string_view>

namespace vic25::building {
enum class Operation { recipe, resource, service, research, transport, commerce, nonstorable };
enum class Siting { urban, coastal, cropland, pasture, woodland, mineral, oil_site, rail_node };
struct OperatingProfile {
    std::string_view id;
    Operation operation{};
    Siting siting{};
    std::array<std::string_view,3> output_goods{};
    std::array<std::string_view,3> required_input_goods{};
    std::string_view occupation_hint; // for later POP occupation matching, not new workers
    std::string_view construction_material_hint; // not a fixed or mandatory construction bill

    [[nodiscard]] constexpr bool produces_inventory()const {
        return operation==Operation::recipe || operation==Operation::resource;
    }
    [[nodiscard]] constexpr bool output_allowed(std::string_view good)const {
        for(const auto g:output_goods)if(!g.empty()&&g==good)return true;
        return false;
    }
};
inline constexpr std::array<OperatingProfile,52> operating_profiles{{
    {"vic3_115.urban.arms_industries", Operation::recipe, Siting::urban, {"small_arms", "", ""}, {"steel", "tools", ""}, "machinist", "steel"},
    {"vic3_115.urban.artillery_foundry", Operation::recipe, Siting::urban, {"artillery", "", ""}, {"steel", "tools", ""}, "machinist", "steel"},
    {"vic3_115.urban.arts_academy", Operation::recipe, Siting::urban, {"fine_art", "", ""}, {"fabric", "dye", ""}, "artist", "wood"},
    {"vic3_115.urban.automotive_industries", Operation::recipe, Siting::urban, {"automobiles", "", ""}, {"engines", "steel", ""}, "machinist", "steel"},
    {"vic3_115.urban.electrics_industries", Operation::recipe, Siting::urban, {"telephones", "radios", ""}, {"tools", "steel", ""}, "engineer", "steel"},
    {"vic3_115.urban.explosives_factory", Operation::recipe, Siting::urban, {"explosives", "", ""}, {"fertilizer", "sulfur", ""}, "chemical_worker", "steel"},
    {"vic3_115.urban.fertilizer_plants", Operation::recipe, Siting::urban, {"fertilizer", "", ""}, {"sulfur", "", ""}, "chemical_worker", "steel"},
    {"vic3_115.urban.food_industries", Operation::recipe, Siting::urban, {"groceries", "liquor", ""}, {"grain", "", ""}, "factory_worker", "wood"},
    {"vic3_115.urban.furniture_manufacturies", Operation::recipe, Siting::urban, {"furniture", "", ""}, {"wood", "", ""}, "craftsperson", "wood"},
    {"vic3_115.urban.glassworks", Operation::recipe, Siting::urban, {"glass", "", ""}, {"coal", "sand", ""}, "factory_worker", "stone"},
    {"vic3_115.urban.motor_industries", Operation::recipe, Siting::urban, {"engines", "", ""}, {"steel", "tools", ""}, "machinist", "steel"},
    {"vic3_115.urban.munition_plants", Operation::recipe, Siting::urban, {"ammunition", "", ""}, {"lead", "explosives", ""}, "factory_worker", "steel"},
    {"vic3_115.urban.paper_mills", Operation::recipe, Siting::urban, {"paper", "", ""}, {"wood", "", ""}, "factory_worker", "wood"},
    {"vic3_115.urban.shipyards", Operation::recipe, Siting::coastal, {"clippers", "steamers", "ironclads"}, {"wood", "tools", ""}, "shipwright", "wood"},
    {"vic3_115.urban.steel_mills", Operation::recipe, Siting::urban, {"steel", "", ""}, {"iron", "coal", ""}, "factory_worker", "steel"},
    {"vic3_115.urban.synthetics_plants", Operation::recipe, Siting::urban, {"dye", "silk", ""}, {"coal", "sulfur", ""}, "chemical_worker", "steel"},
    {"vic3_115.urban.textile_mills", Operation::recipe, Siting::urban, {"clothes", "", ""}, {"fabric", "", ""}, "weaver", "wood"},
    {"vic3_115.urban.tooling_workshops", Operation::recipe, Siting::urban, {"tools", "", ""}, {"iron", "wood", ""}, "craftsperson", "wood"},
    {"vic3_115.rural.banana_plantations", Operation::resource, Siting::cropland, {"fruit", "", ""}, {"", "", ""}, "farm_worker", "wood"},
    {"vic3_115.rural.coal_mines", Operation::resource, Siting::mineral, {"coal", "", ""}, {"", "", ""}, "miner", "wood"},
    {"vic3_115.rural.coffee_plantations", Operation::resource, Siting::cropland, {"coffee", "", ""}, {"", "", ""}, "farm_worker", "wood"},
    {"vic3_115.rural.copper_mines", Operation::resource, Siting::mineral, {"copper", "", ""}, {"", "", ""}, "miner", "wood"},
    {"vic3_115.rural.cotton_plantations", Operation::resource, Siting::cropland, {"fabric", "cotton", ""}, {"", "", ""}, "farm_worker", "wood"},
    {"vic3_115.rural.dye_plantations", Operation::resource, Siting::cropland, {"dye", "", ""}, {"", "", ""}, "farm_worker", "wood"},
    {"vic3_115.rural.fishing_wharves", Operation::resource, Siting::coastal, {"fish", "", ""}, {"", "", ""}, "fisher", "wood"},
    {"vic3_115.rural.gold_mines", Operation::resource, Siting::mineral, {"gold", "", ""}, {"", "", ""}, "miner", "wood"},
    {"vic3_115.rural.iron_mines", Operation::resource, Siting::mineral, {"iron", "", ""}, {"", "", ""}, "miner", "wood"},
    {"vic3_115.rural.lead_mines", Operation::resource, Siting::mineral, {"lead", "", ""}, {"", "", ""}, "miner", "wood"},
    {"vic3_115.rural.livestock_ranches", Operation::resource, Siting::pasture, {"meat", "fabric", ""}, {"", "", ""}, "rancher", "wood"},
    {"vic3_115.rural.logging_camps", Operation::resource, Siting::woodland, {"wood", "", ""}, {"", "", ""}, "lumberjack", "wood"},
    {"vic3_115.rural.maize_farms", Operation::resource, Siting::cropland, {"grain", "", ""}, {"", "", ""}, "farm_worker", "wood"},
    {"vic3_115.rural.millet_farms", Operation::resource, Siting::cropland, {"grain", "", ""}, {"", "", ""}, "farm_worker", "wood"},
    {"vic3_115.rural.oil_rigs", Operation::resource, Siting::oil_site, {"oil", "", ""}, {"", "", ""}, "driller", "steel"},
    {"vic3_115.rural.opium_plantations", Operation::resource, Siting::cropland, {"opium", "", ""}, {"", "", ""}, "farm_worker", "wood"},
    {"vic3_115.rural.rice_farms", Operation::resource, Siting::cropland, {"grain", "", ""}, {"", "", ""}, "farm_worker", "wood"},
    {"vic3_115.rural.rubber_plantations", Operation::resource, Siting::cropland, {"rubber", "", ""}, {"", "", ""}, "farm_worker", "wood"},
    {"vic3_115.rural.rye_farms", Operation::resource, Siting::cropland, {"grain", "", ""}, {"", "", ""}, "farm_worker", "wood"},
    {"vic3_115.rural.silk_plantations", Operation::resource, Siting::cropland, {"silk", "", ""}, {"", "", ""}, "farm_worker", "wood"},
    {"vic3_115.rural.sugar_plantations", Operation::resource, Siting::cropland, {"sugar", "", ""}, {"", "", ""}, "farm_worker", "wood"},
    {"vic3_115.rural.sulfur_mines", Operation::resource, Siting::mineral, {"sulfur", "", ""}, {"", "", ""}, "miner", "wood"},
    {"vic3_115.rural.tea_plantations", Operation::resource, Siting::cropland, {"tea", "", ""}, {"", "", ""}, "farm_worker", "wood"},
    {"vic3_115.rural.tobacco_plantations", Operation::resource, Siting::cropland, {"tobacco", "", ""}, {"", "", ""}, "farm_worker", "wood"},
    {"vic3_115.rural.vineyards", Operation::resource, Siting::cropland, {"fruit", "", ""}, {"", "", ""}, "farm_worker", "wood"},
    {"vic3_115.rural.whaling_stations", Operation::resource, Siting::coastal, {"oil", "", ""}, {"", "", ""}, "fisher", "wood"},
    {"vic3_115.rural.wheat_farms", Operation::resource, Siting::cropland, {"grain", "", ""}, {"", "", ""}, "farm_worker", "wood"},
    {"vic3_115.development.government_administration", Operation::service, Siting::urban, {"", "", ""}, {"", "", ""}, "clerk", "stone"},
    {"vic3_115.development.port", Operation::transport, Siting::coastal, {"", "", ""}, {"", "", ""}, "dockworker", "wood"},
    {"vic3_115.development.power_plants", Operation::nonstorable, Siting::urban, {"", "", ""}, {"coal", "", ""}, "engineer", "steel"},
    {"vic3_115.development.railway", Operation::transport, Siting::rail_node, {"", "", ""}, {"", "", ""}, "rail_worker", "steel"},
    {"vic3_115.development.skyscraper", Operation::service, Siting::urban, {"", "", ""}, {"", "", ""}, "clerk", "steel"},
    {"vic3_115.development.trade_center", Operation::commerce, Siting::urban, {"", "", ""}, {"", "", ""}, "merchant", "wood"},
    {"vic3_115.development.university", Operation::research, Siting::urban, {"", "", ""}, {"paper", "", ""}, "academic", "stone"},
}};
[[nodiscard]] inline constexpr const OperatingProfile* profile(std::string_view id) {
    for(const auto& p:operating_profiles)if(p.id==id)return &p;
    return nullptr;
}
[[nodiscard]] inline constexpr bool valid_profiles() {
    for(std::size_t i=0;i<operating_profiles.size();++i) {
        const auto& p=operating_profiles[i];
        if(!find(p.id)||!find(p.id)->site_eligible())return false;
        if(p.produces_inventory() && p.output_goods[0].empty())return false;
        if((p.operation==Operation::recipe && p.required_input_goods[0].empty())||
           (p.operation==Operation::resource && !p.required_input_goods[0].empty()))
            return false;
        for(std::size_t j=i+1;j<operating_profiles.size();++j)
            if(p.id==operating_profiles[j].id)return false;
    }
    std::size_t number=0;
    for(const auto& a:archetypes)if(a.site_eligible())++number;
    return number==operating_profiles.size();
}
static_assert(valid_profiles(),"all physical building archetypes need unique operating profiles");
static_assert(profile("vic3_115.urban.food_industries")->output_allowed("groceries"));
static_assert(profile("vic3_115.rural.iron_mines")->siting==Siting::mineral);
static_assert(profile("vic3_115.development.port")->operation==Operation::transport);
} // namespace vic25::building
