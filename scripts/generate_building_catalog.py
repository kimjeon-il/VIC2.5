#!/usr/bin/env python3
"""Generate the native lookup catalogue from the reviewed crosswalk. Stdlib only."""
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = root / "data/reference/vic3_115_building_crosswalk.json"
target = root / "include/vic25/building_catalog.hpp"
data = json.loads(source.read_text(encoding="utf-8"))
assert len(data["entries"]) == 99 and len(data["industry_categories"]) == 13
goods = {
  "Iron Mines": "iron",
  "Coal Mines": "coal",
  "Copper Mines": "copper",
  "Lead Mines": "lead",
  "Sulfur Mines": "sulfur",
  "Wheat Farms": "grain",
  "Rice Farms": "grain",
  "Rye Farms": "grain",
  "Maize Farms": "grain",
  "Millet Farms": "grain",
  "Fishing Wharves": "fish",
  "Logging Camps": "wood",
  "Food Industries": "groceries",
  "Steel Mills": "steel",
  "Tooling Workshops": "tools",
  "Textile Mills": "clothes",
  "Fertilizer Plants": "fertilizer",
  "Explosives Factory": "explosives",
  "Paper Mills": "paper",
  "Glassworks": "glass",
  "Arms Industries": "small_arms",
  "Artillery Foundry": "artillery",
  "Munition Plants": "ammunition",
  "Motor Industries": "engines",
  "Automotive Industries": "automobiles",
  "Oil Rigs": "oil"
}
modifiers = {
    "MANU": "building_group_bg_manufacturing_throughput_add",
    "AGRI": "building_group_bg_agriculture_throughput_add",
    "LIVE": "building_group_bg_ranching_throughput_add",
    "EXTR": "building_group_bg_mining_throughput_add",
}
allowed = {"production_site", "resource_extraction", "service_site",
           "government_service_site", "utility_site",
           "network_and_service_site", "commercial_site", "multiuse_asset"}
def literal(value):
    return json.dumps(value, ensure_ascii=False)

body = [
    "#pragma once",
    "// Generated reference to Victoria 3 1.15 building taxonomy.",
    "// Keep aligned with data/reference/vic3_115_building_crosswalk.json.",
    "// The archetypes are descriptors, not businesses, physical sites or assets.",
    "#include <array>",
    "#include <string_view>",
    "",
    "namespace vic25::building {",
    "struct IndustryCategory {",
    "    std::string_view id, name_ko;",
    "};",
    "struct Archetype {",
    "    std::string_view id, name_en, name_ko, industry, mapping_kind;",
    "    std::string_view representative_good; // optional compatibility hint; no output coefficient",
    "    std::string_view throughput_modifier; // existing law/trait projection key",
    "    [[nodiscard]] constexpr bool site_eligible()const {",
]
for mode in sorted(allowed):
    pass
body += [
    '        return ' + ' ||\n               '.join(f'mapping_kind=="{mode}"' for mode in [
        "production_site", "resource_extraction", "service_site",
        "government_service_site", "utility_site", "network_and_service_site",
        "commercial_site", "multiuse_asset"]) + ';',
    "    }",
    "    [[nodiscard]] constexpr bool factory_recipe_eligible()const {",
    '        return mapping_kind=="production_site";',
    "    }",
    "    [[nodiscard]] constexpr bool extraction_eligible()const {",
    '        return mapping_kind=="resource_extraction";',
    "    }",
    "};",
    "inline constexpr std::array<IndustryCategory,13> industries{{ ",
]
for item in data["industry_categories"]:
    body.append(f'    {{{literal(item["id"])},{literal(item["label"])}}},')
body += ["}};", "inline constexpr std::array<Archetype,99> archetypes{{"]
for item in data["entries"]:
    industry = item["industry_class"]
    name = item["source_name_en"]
    modifier = modifiers.get(industry, "")
    if industry == "AGRI" and ("Plantations" in name or name=="Vineyards"):
        modifier = "building_group_bg_plantations_throughput_add"
    fields = [item["reference_id"], name, item["name_ko"], industry,
              item["mapping_kind"], goods.get(name, ""), modifier]
    body.append("        {" + ", ".join(map(literal, fields)) + "},")
body += [
    "}};",
    "inline constexpr const Archetype* find(std::string_view id) {",
    "    for(const auto& entry:archetypes)",
    "        if(entry.id==id)return &entry;",
    "    return nullptr;",
    "}",
    "inline constexpr bool industry_exists(std::string_view id) {",
    "    for(const auto& entry:industries)",
    "        if(entry.id==id)return true;",
    "    return false;",
    "}",
    "inline constexpr std::size_t group_count(std::string_view group) {",
    "    std::size_t n=0;",
    "    for(const auto& entry:archetypes) {",
    '        if(entry.id.starts_with("vic3_115.") &&',
    "           entry.id.substr(9,group.size())==group &&",
    "           entry.id.size()>9+group.size() &&",
    "           entry.id[9+group.size()]=='.')++n;",
    "    }",
    "    return n;",
    "}",
    "static_assert(archetypes.size()==99 && industries.size()==13);",
    'static_assert(find("vic3_115.urban.steel_mills")!=nullptr);',
    'static_assert(find("vic3_115.rural.iron_mines")->extraction_eligible());',
    'static_assert(!find("vic3_115.urban.urban_center")->site_eligible());',
    "} // namespace vic25::building",
]
target.write_text("\n".join(body)+"\n",encoding="utf-8")
print(f"{len(data['entries'])} archetypes -> {target}")
