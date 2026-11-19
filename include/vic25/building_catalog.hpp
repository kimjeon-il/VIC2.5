#pragma once
// Generated reference to Victoria 3 1.15 building taxonomy.
// Keep aligned with data/reference/vic3_115_building_crosswalk.json.
// The archetypes are descriptors, not businesses, physical sites or assets.
#include <array>
#include <string_view>

namespace vic25::building {
struct IndustryCategory {
    std::string_view id, name_ko;
};
struct Archetype {
    std::string_view id, name_en, name_ko, industry, mapping_kind;
    std::string_view representative_good; // optional compatibility hint; no output coefficient
    std::string_view throughput_modifier; // existing law/trait projection key
    [[nodiscard]] constexpr bool site_eligible()const {
        return mapping_kind=="production_site" ||
               mapping_kind=="resource_extraction" ||
               mapping_kind=="service_site" ||
               mapping_kind=="government_service_site" ||
               mapping_kind=="utility_site" ||
               mapping_kind=="network_and_service_site" ||
               mapping_kind=="commercial_site" ||
               mapping_kind=="multiuse_asset";
    }
    [[nodiscard]] constexpr bool factory_recipe_eligible()const {
        return mapping_kind=="production_site";
    }
    [[nodiscard]] constexpr bool extraction_eligible()const {
        return mapping_kind=="resource_extraction" ||
               (industry=="FISH" && mapping_kind=="production_site");
    }
};
inline constexpr std::array<IndustryCategory,13> industries{{ 
    {"AGRI","농경·원예"},
    {"LIVE","축산·목축"},
    {"FORE","임업·벌목"},
    {"FISH","어업·양식"},
    {"EXTR","광업·채굴"},
    {"MANU","제조·가공"},
    {"ENER","연료전환·전력·열"},
    {"UTIL","상하수·폐기물·환경"},
    {"CONS","건설·수리·토목"},
    {"LOGI","운송·창고·통신"},
    {"TRAD","도소매·중개·숙박"},
    {"FINA","금융·보험·부동산"},
    {"SERV","전문·교육·의료·행정 등 서비스"},
}};
inline constexpr std::array<Archetype,99> archetypes{{
        {"vic3_115.urban.arms_industries", "Arms Industries", "무기공장", "MANU", "production_site", "small_arms", "building_group_bg_manufacturing_throughput_add",
        {"vic3_115.urban.artillery_foundry", "Artillery Foundry", "화포주조소", "MANU", "production_site", "artillery", "building_group_bg_manufacturing_throughput_add",
        {"vic3_115.urban.arts_academy", "Arts Academy", "예술원", "SERV", "service_site", "", "",
        {"vic3_115.urban.automotive_industries", "Automotive Industries", "자동차공장", "MANU", "production_site", "automobiles", "building_group_bg_manufacturing_throughput_add",
        {"vic3_115.urban.company_headquarter", "Company Headquarter", "기업 본사", "FINA", "organizational_view", "", "",
        {"vic3_115.urban.electrics_industries", "Electrics Industries", "전기기기공장", "MANU", "production_site", "", "building_group_bg_manufacturing_throughput_add",
        {"vic3_115.urban.explosives_factory", "Explosives Factory", "폭약공장", "MANU", "production_site", "explosives", "building_group_bg_manufacturing_throughput_add",
        {"vic3_115.urban.fertilizer_plants", "Fertilizer Plants", "비료공장", "MANU", "production_site", "fertilizer", "building_group_bg_manufacturing_throughput_add",
        {"vic3_115.urban.financial_districts", "Financial Districts", "금융지구", "FINA", "ownership_view", "", "",
        {"vic3_115.urban.food_industries", "Food Industries", "식품공장", "MANU", "production_site", "groceries", "building_group_bg_manufacturing_throughput_add",
        {"vic3_115.urban.furniture_manufacturies", "Furniture Manufacturies", "가구공장", "MANU", "production_site", "", "building_group_bg_manufacturing_throughput_add",
        {"vic3_115.urban.glassworks", "Glassworks", "유리·도자기공장", "MANU", "production_site", "glass", "building_group_bg_manufacturing_throughput_add",
        {"vic3_115.urban.manor_houses", "Manor Houses", "장원 저택", "FINA", "ownership_view", "", "",
        {"vic3_115.urban.motor_industries", "Motor Industries", "동력기관공장", "MANU", "production_site", "engines", "building_group_bg_manufacturing_throughput_add",
        {"vic3_115.urban.munition_plants", "Munition Plants", "탄약공장", "MANU", "production_site", "ammunition", "building_group_bg_manufacturing_throughput_add",
        {"vic3_115.urban.paper_mills", "Paper Mills", "제지공장", "MANU", "production_site", "paper", "building_group_bg_manufacturing_throughput_add",
        {"vic3_115.urban.regional_company_headquarter", "Regional Company Headquarter", "지역 기업 본부", "FINA", "organizational_view", "", "",
        {"vic3_115.urban.shipyards", "Shipyards", "조선소", "MANU", "production_site", "", "building_group_bg_manufacturing_throughput_add",
        {"vic3_115.urban.steel_mills", "Steel Mills", "제철소", "MANU", "production_site", "steel", "building_group_bg_manufacturing_throughput_add",
        {"vic3_115.urban.synthetics_plants", "Synthetics Plants", "합성물공장", "MANU", "production_site", "", "building_group_bg_manufacturing_throughput_add",
        {"vic3_115.urban.textile_mills", "Textile Mills", "방직공장", "MANU", "production_site", "clothes", "building_group_bg_manufacturing_throughput_add",
        {"vic3_115.urban.tooling_workshops", "Tooling Workshops", "공구공장", "MANU", "production_site", "tools", "building_group_bg_manufacturing_throughput_add",
        {"vic3_115.urban.urban_center", "Urban Center", "도시 중심지", "SERV", "derived_view", "", "",
        {"vic3_115.rural.banana_plantations", "Banana Plantations", "바나나 농장", "AGRI", "production_site", "", "building_group_bg_plantations_throughput_add",
        {"vic3_115.rural.coal_mines", "Coal Mines", "석탄광산", "EXTR", "resource_extraction", "coal", "building_group_bg_mining_throughput_add",
        {"vic3_115.rural.coffee_plantations", "Coffee Plantations", "커피 농장", "AGRI", "production_site", "", "building_group_bg_plantations_throughput_add",
        {"vic3_115.rural.copper_mines", "Copper Mines", "구리광산", "EXTR", "resource_extraction", "copper", "building_group_bg_mining_throughput_add",
        {"vic3_115.rural.cossack_host", "Cossack Host", "코사크 군단", "NONE", "military_organization", "", "",
        {"vic3_115.rural.cotton_plantations", "Cotton Plantations", "목화 농장", "AGRI", "production_site", "", "building_group_bg_plantations_throughput_add",
        {"vic3_115.rural.dye_plantations", "Dye Plantations", "염료 농장", "AGRI", "production_site", "", "building_group_bg_plantations_throughput_add",
        {"vic3_115.rural.fishing_wharves", "Fishing Wharves", "어항·어업시설", "FISH", "production_site", "fish", "",
        {"vic3_115.rural.gold_fields", "Gold Fields", "노천 금광지", "EXTR", "resource_activity_marker", "", "building_group_bg_mining_throughput_add",
        {"vic3_115.rural.gold_mines", "Gold Mines", "금광산", "EXTR", "resource_extraction", "", "building_group_bg_mining_throughput_add",
        {"vic3_115.rural.iron_mines", "Iron Mines", "철광산", "EXTR", "resource_extraction", "iron", "building_group_bg_mining_throughput_add",
        {"vic3_115.rural.lead_mines", "Lead Mines", "납광산", "EXTR", "resource_extraction", "lead", "building_group_bg_mining_throughput_add",
        {"vic3_115.rural.livestock_ranches", "Livestock Ranches", "목장", "LIVE", "production_site", "", "building_group_bg_ranching_throughput_add",
        {"vic3_115.rural.logging_camps", "Logging Camps", "벌목장", "FORE", "resource_extraction", "wood", "",
        {"vic3_115.rural.maize_farms", "Maize Farms", "옥수수 농장", "AGRI", "production_site", "grain", "building_group_bg_agriculture_throughput_add",
        {"vic3_115.rural.millet_farms", "Millet Farms", "잡곡 농장", "AGRI", "production_site", "grain", "building_group_bg_agriculture_throughput_add",
        {"vic3_115.rural.oil_rigs", "Oil Rigs", "유정", "EXTR", "resource_extraction", "oil", "building_group_bg_mining_throughput_add",
        {"vic3_115.rural.opium_plantations", "Opium Plantations", "아편 농장", "AGRI", "production_site", "", "building_group_bg_plantations_throughput_add",
        {"vic3_115.rural.rice_farms", "Rice Farms", "벼 농장", "AGRI", "production_site", "grain", "building_group_bg_agriculture_throughput_add",
        {"vic3_115.rural.rubber_plantations", "Rubber Plantations", "고무 농장", "AGRI", "production_site", "", "building_group_bg_plantations_throughput_add",
        {"vic3_115.rural.rye_farms", "Rye Farms", "호밀 농장", "AGRI", "production_site", "grain", "building_group_bg_agriculture_throughput_add",
        {"vic3_115.rural.silk_plantations", "Silk Plantations", "양잠 농장", "AGRI", "production_site", "", "building_group_bg_plantations_throughput_add",
        {"vic3_115.rural.subsistence_farms", "Subsistence Farms", "생계 농가", "AGRI", "own_account_activity", "", "building_group_bg_agriculture_throughput_add",
        {"vic3_115.rural.subsistence_fishing_villages", "Subsistence Fishing Villages", "생계 어촌", "FISH", "own_account_activity", "", "",
        {"vic3_115.rural.subsistence_orchards", "Subsistence Orchards", "생계 과수원", "AGRI", "own_account_activity", "", "building_group_bg_agriculture_throughput_add",
        {"vic3_115.rural.subsistence_pastures", "Subsistence Pastures", "생계 목축지", "LIVE", "own_account_activity", "", "building_group_bg_ranching_throughput_add",
        {"vic3_115.rural.subsistence_rice_paddies", "Subsistence Rice Paddies", "생계 논", "AGRI", "own_account_activity", "", "building_group_bg_agriculture_throughput_add",
        {"vic3_115.rural.sugar_plantations", "Sugar Plantations", "설탕 농장", "AGRI", "production_site", "", "building_group_bg_plantations_throughput_add",
        {"vic3_115.rural.sulfur_mines", "Sulfur Mines", "유황광산", "EXTR", "resource_extraction", "sulfur", "building_group_bg_mining_throughput_add",
        {"vic3_115.rural.tea_plantations", "Tea Plantations", "차 농장", "AGRI", "production_site", "", "building_group_bg_plantations_throughput_add",
        {"vic3_115.rural.tobacco_plantations", "Tobacco Plantations", "담배 농장", "AGRI", "production_site", "", "building_group_bg_plantations_throughput_add",
        {"vic3_115.rural.vineyards", "Vineyards", "포도원", "AGRI", "production_site", "", "building_group_bg_plantations_throughput_add",
        {"vic3_115.rural.whaling_stations", "Whaling Stations", "포경기지", "FISH", "production_site", "", "",
        {"vic3_115.rural.wheat_farms", "Wheat Farms", "밀 농장", "AGRI", "production_site", "grain", "building_group_bg_agriculture_throughput_add",
        {"vic3_115.development.army_logistics_center", "Army Logistics Center", "육군 병참 거점", "NONE", "military_service_view", "", "",
        {"vic3_115.development.barracks", "Barracks", "병영", "NONE", "military_asset", "", "",
        {"vic3_115.development.conscription_center", "Conscription Center", "징병소", "NONE", "recruitment_service", "", "",
        {"vic3_115.development.construction_sector", "Construction Sector", "건설 부문", "CONS", "capacity_view", "", "",
        {"vic3_115.development.government_administration", "Government Administration", "정부청사", "SERV", "government_service_site", "", "",
        {"vic3_115.development.naval_administration", "Naval Administration", "해군 행정기관", "NONE", "military_administration_site", "", "",
        {"vic3_115.development.naval_fortification", "Naval Fortification", "해안 요새", "NONE", "military_asset", "", "",
        {"vic3_115.development.naval_logistics_center", "Naval Logistics Center", "해군 병참 거점", "NONE", "military_service_view", "", "",
        {"vic3_115.development.port", "Port", "항만", "LOGI", "network_and_service_site", "", "",
        {"vic3_115.development.power_plants", "Power Plants", "발전소", "ENER", "utility_site", "", "",
        {"vic3_115.development.railway", "Railway", "철도", "LOGI", "network_and_service_site", "", "",
        {"vic3_115.development.skyscraper", "Skyscraper", "마천루", "TRAD", "multiuse_asset", "", "",
        {"vic3_115.development.trade_center", "Trade Center", "무역센터", "TRAD", "commercial_site", "", "",
        {"vic3_115.development.university", "University", "대학", "SERV", "service_site", "", "",
        {"vic3_115.special.angkor_wat.1", "Angkor Wat", "앙코르와트", "SERV", "heritage_asset", "", "",
        {"vic3_115.special.atocha_railway_station.1", "Atocha Railway Station", "아토차역", "LOGI", "unique_transport_asset", "", "",
        {"vic3_115.special.big_ben.1", "Big Ben", "빅벤", "SERV", "heritage_asset", "", "",
        {"vic3_115.special.cristo_redentor.1", "Cristo Redentor", "구세주 그리스도상", "SERV", "heritage_asset", "", "",
        {"vic3_115.special.forbidden_city.1", "Forbidden City", "자금성", "SERV", "heritage_asset", "", "",
        {"vic3_115.special.grand_theater_of_havana.1", "Grand Theater of Havana", "아바나 대극장", "SERV", "heritage_asset", "", "",
        {"vic3_115.special.hagia_sophia.1", "Hagia Sophia", "아야소피아", "SERV", "heritage_asset", "", "",
        {"vic3_115.special.kaiserforum.1", "Kaiserforum", "카이저포룸", "SERV", "heritage_asset", "", "",
        {"vic3_115.special.kaiserforum.2", "Kaiserforum", "카이저포룸", "SERV", "heritage_asset", "", "",
        {"vic3_115.special.kaiserforum.3", "Kaiserforum", "카이저포룸", "SERV", "heritage_asset", "", "",
        {"vic3_115.special.kaiserforum.4", "Kaiserforum", "카이저포룸", "SERV", "heritage_asset", "", "",
        {"vic3_115.special.kiel_canal.1", "Kiel Canal", "킬 운하", "LOGI", "unique_transport_asset", "", "",
        {"vic3_115.special.manila_cathedral.1", "Manila Cathedral", "마닐라 대성당", "SERV", "heritage_asset", "", "",
        {"vic3_115.special.mosque_of_djenne.1", "Mosque of Djenne", "젠네 모스크", "SERV", "heritage_asset", "", "",
        {"vic3_115.special.panama_canal.1", "Panama Canal", "파나마 운하", "LOGI", "unique_transport_asset", "", "",
        {"vic3_115.special.pena_palace.1", "Pena Palace", "페나 궁전", "SERV", "heritage_asset", "", "",
        {"vic3_115.special.power_bloc_statue.1", "Power Bloc Statue", "세력권 조각상", "SERV", "heritage_asset", "", "",
        {"vic3_115.special.sagrada_fami_lia_cathedral.1", "Sagrada Família Cathedral", "사그라다 파밀리아", "SERV", "heritage_asset", "", "",
        {"vic3_115.special.sagrada_fami_lia_cathedral.2", "Sagrada Família Cathedral", "사그라다 파밀리아", "SERV", "heritage_asset", "", "",
        {"vic3_115.special.sagrada_fami_lia_cathedral.3", "Sagrada Família Cathedral", "사그라다 파밀리아", "SERV", "heritage_asset", "", "",
        {"vic3_115.special.saint_basils_cathedral.1", "Saint Basil’s Cathedral", "성 바실리 대성당", "SERV", "heritage_asset", "", "",
        {"vic3_115.special.statue_of_liberty.1", "Statue of Liberty", "자유의 여신상", "SERV", "heritage_asset", "", "",
        {"vic3_115.special.suez_canal.1", "Suez Canal", "수에즈 운하", "LOGI", "unique_transport_asset", "", "",
        {"vic3_115.special.taj_mahal.1", "Taj Mahal", "타지마할", "SERV", "heritage_asset", "", "",
        {"vic3_115.special.the_eiffel_tower.1", "The Eiffel Tower", "에펠탑", "SERV", "heritage_asset", "", "",
        {"vic3_115.special.vatican_city.1", "Vatican City", "바티칸시국", "NONE", "polity_reference", "", "",
        {"vic3_115.special.victoria_terminus.1", "Victoria Terminus", "빅토리아 터미너스", "LOGI", "unique_transport_asset", "", "",
        {"vic3_115.special.white_house.1", "White House", "백악관", "SERV", "government_heritage_asset", "", "",
}};
inline constexpr const Archetype* find(std::string_view id) {
    for(const auto& entry:archetypes)
        if(entry.id==id)return &entry;
    return nullptr;
}
inline constexpr bool industry_exists(std::string_view id) {
    for(const auto& entry:industries)
        if(entry.id==id)return true;
    return false;
}
inline constexpr std::size_t group_count(std::string_view group) {
    std::size_t n=0;
    for(const auto& entry:archetypes) {
        if(entry.id.starts_with("vic3_115.") &&
           entry.id.substr(9,group.size())==group &&
           entry.id.size()>9+group.size() &&
           entry.id[9+group.size()]=='.')++n;
    }
    return n;
}
static_assert(archetypes.size()==99 && industries.size()==13);
static_assert(group_count("urban")==23 && group_count("rural")==34);
static_assert(group_count("development")==14 && group_count("special")==28);
static_assert(find("vic3_115.urban.steel_mills")!=nullptr);
static_assert(find("vic3_115.rural.iron_mines")->extraction_eligible());
static_assert(!find("vic3_115.urban.urban_center")->site_eligible());
} // namespace vic25::building
