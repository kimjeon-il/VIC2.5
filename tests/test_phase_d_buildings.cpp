#include "vic25/phase_d_snapshot.hpp"
#include "vic25/building_catalog.hpp"
#include "vic25/building_operating_profiles.hpp"
#include "vic25/phase_e.hpp"
#include <iostream>
#include <stdexcept>
#include <string_view>

using namespace vic25::phased;
using vic25::phaseb::Sex;
using vic25::phaseb::Service;
static void check(bool ok,const char* message) {
    if(!ok)throw std::runtime_error(message);
}
template<class F> static bool rejects(F&& f) {
    try { f(); } catch(const std::exception&) { return true; }
    return false;
}
static Id seed_worker(World& w,People count) {
    auto& pop=w.seed_core().seed_population();
    const auto hh=pop.household_profile({"R","shared","shared",count});
    const auto eco=pop.economic_profile({"R","worker","wage","employee",pop.default_education()});
    const auto dem=pop.demographic_profile({"R",w.month()-300,Sex::female,
        "culture","faith","citizen","free",pop.default_health(),Service::civilian});
    return pop.seed_population(dem,eco,hh,count);
}
static void test_native_catalog_and_actual_production() {
    check(vic25::building::archetypes.size()==99 &&
          vic25::building::operating_profiles.size()==52 &&
          vic25::building::valid_profiles() &&
          vic25::building::industries.size()==13 &&
          vic25::building::group_count("urban")==23 &&
          vic25::building::group_count("rural")==34 &&
          vic25::building::group_count("development")==14 &&
          vic25::building::group_count("special")==28,
          "1.15 source counts do not match native catalog");

    World w{4200};auto& core=w.seed_core();
    core.seed_actor("FIRM",50);core.seed_actor("MINER",0);
    core.seed_actor("SHIPPING",10);core.seed_region("R","STATE");
    w.seed_standard_industries();
    check(w.state().industry_classes.size()==13,"13 industry roots not initialized");
    w.seed_building_facility("FOOD","FIRM","R",3,
                            "vic3_115.urban.food_industries");
    w.seed_building_facility("MINE","MINER","R",3,
                            "vic3_115.rural.iron_mines");
    w.seed_building_facility("PORT","SHIPPING","R",2,
                            "vic3_115.development.port");
    check(rejects([&]{
        w.seed_building_facility("GHOST","FIRM","R",1,
                                  "vic3_115.urban.urban_center");
    }) && !core.facilities().contains("GHOST"),
    "derived urban-center must not create an asset");
    check(rejects([&]{
        w.seed_building_facility("MONEY","FIRM","R",1,
                                  "vic3_115.urban.financial_districts");
    }) && !core.facilities().contains("MONEY"),
    "financial district is an ownership view, not a free factory");

    ResourceSite ore;
    ore.id="ORE";ore.site="MINE";ore.owner="MINER";ore.region="R";
    ore.good="coal";ore.monthly_capacity=3;
    check(rejects([&]{w.seed_building_resource_site(ore);}) &&
          w.state().resource_sites.empty(),
          "iron mine accepted a mismatched coal resource");
    ore.good="iron";w.seed_building_resource_site(ore);
    IndustryRecipe wrong;
    wrong.id="PORT_GROCERIES";wrong.site="PORT";wrong.industry_class="LOGI";
    wrong.inputs={{"grain",1}};wrong.outputs={{"groceries",1}};
    wrong.workers_per_batch=1;
    check(rejects([&]{w.seed_building_recipe(wrong);}),
          "port incorrectly became a manufacturing recipe");

    IndustryRecipe food;
    food.id="BREAD";food.site="FOOD";food.industry_class="MANU";
    food.inputs={{"grain",1}};food.outputs={{"groceries",2}};
    food.workers_per_batch=1;
    w.seed_building_recipe(food);
    core.seed_stock("FIRM","R","grain",2);
    check(w.building_production_capacity("BREAD").feasible_batches==0,
          "unemployed staff incorrectly created product capacity");
    const auto worker=seed_worker(w,2);
    World::Plan p;
    p[2].push_back([worker](Stage& s){
        s.phase_c().hire(worker,2,"FIRM",1);
    });
    p[4].push_back([](Stage& s){s.manufacture("BREAD",2);});
    w.advance(p);
    check(w.building_production_capacity("BREAD").feasible_batches==0,
          "exhausted physical inputs should cap future production");
    check(w.core().stock("FIRM","R","groceries")==4 &&
          w.core().stock("FIRM","R","grain")==0 &&
          w.realized_batches("MANU",4200,4200)==2,
          "building recipe did not perform real inventory-backed production");
    check(w.site_archetype("PORT")->industry=="LOGI" &&
          !w.state().recipes.contains("PORT_GROCERIES"),
          "transport facility has acquired unsupported free service output");
    const auto before=SnapshotCodec::encode(w);
    auto after=SnapshotCodec::decode(before);
    check(SnapshotCodec::encode(after)==before &&
          after.site_archetype("FOOD") &&
          after.site_archetype("FOOD")->name_en=="Food Industries" &&
          after.site_archetype("MINE")->representative_good=="iron",
          "building site provenance did not round-trip in snapshot");
}
static void test_construction_and_recipe_before_commission() {
    World w{4300};auto& c=w.seed_core();
    c.seed_actor("CLIENT",100);c.seed_actor("BUILDER",20);
    c.seed_region("R","STATE");
    w.seed_standard_industries();
    ConstructionProject project;
    project.id="NEW_STEEL";project.client="CLIENT";project.contractor="BUILDER";
    project.region="R";project.target_site="STEEL_SITE";
    project.material_good="stone";project.work_required=1;
    project.material_per_work=1;project.workers_per_work=1;
    project.target_capacity=4;project.cash_price_per_work=10;
    w.seed_building_construction(project,"vic3_115.urban.steel_mills");
    IndustryRecipe recipe;
    recipe.id="MAKE_STEEL";recipe.site="STEEL_SITE";recipe.industry_class="MANU";
    recipe.inputs={{"iron",1},{"coal",1}};
    recipe.outputs={{"steel",1}};recipe.workers_per_batch=1;
    auto missing_coal=recipe;
    missing_coal.id="BAD_STEEL";
    missing_coal.inputs={{"iron",1}};
    check(rejects([&]{w.seed_building_recipe(missing_coal);}) &&
          !w.state().recipes.contains("BAD_STEEL"),
          "steel archetype accepted recipe without physical coal input");
    w.seed_building_recipe(recipe);
    check(!c.facilities().contains("STEEL_SITE") &&
          !w.construction("NEW_STEEL").commissioned &&
          w.site_archetype("STEEL_SITE")->name_en=="Steel Mills",
          "planned factory was prematurely materialized");
    const auto data=SnapshotCodec::encode(w);
    auto restored=SnapshotCodec::decode(data);
    check(SnapshotCodec::encode(restored)==data &&
          restored.state().recipes.contains("MAKE_STEEL") &&
          !restored.core().facilities().contains("STEEL_SITE"),
          "planned building or recipe lost across save/load");
    // Completion is not inferred from a building type. It still costs real
    // construction labor, materials and a settled construction invoice.
    c.seed_stock("BUILDER","R","stone",1);
    const auto worker=seed_worker(w,1);
    World::Plan construction;
    construction[1].push_back([](Stage& s){s.plan_work("NEW_STEEL",1);});
    construction[2].push_back([worker](Stage& s){
        s.phase_c().hire(worker,1,"BUILDER",1);
    });
    construction[4].push_back([](Stage& s){s.perform_work("NEW_STEEL",1);});
    construction[5].push_back([](Stage& s){s.pay_work("NEW_STEEL",1);});
    construction[8].push_back([](Stage& s){s.capitalize_work("NEW_STEEL",1);});
    w.advance(construction);
    check(w.construction("NEW_STEEL").ready &&
          !w.core().facilities().contains("STEEL_SITE"),
          "proposed industrial site started before construction completion");
    w.advance();
    check(w.construction("NEW_STEEL").commissioned &&
          w.core().facility("STEEL_SITE").capacity==4 &&
          w.site_archetype("STEEL_SITE")->industry=="MANU" &&
          w.core().phase_d_wip("CLIENT")==0,
          "commissioning lost archetype or bypassed real WIP");
}
static void test_farm_with_real_material_and_renewable_crop() {
    World w{4400};auto& c=w.seed_core();
    c.seed_actor("FARMER",50);c.seed_region("R","STATE");
    w.seed_standard_industries();
    w.seed_building_facility("FIELD","FARMER","R",4,
                            "vic3_115.rural.wheat_farms");
    ResourceSite crop;
    crop.id="WHEAT";crop.site="FIELD";crop.owner="FARMER";crop.region="R";
    crop.good="grain";crop.monthly_capacity=4;
    crop.renewable=true;crop.standing_biomass=4;crop.carrying_capacity=6;
    crop.renewal_per_month=1;crop.operational_inputs_per_unit={{"seed_material",1}};
    w.seed_building_resource_site(crop);
    c.seed_stock("FARMER","R","seed_material",3);
    const auto worker=seed_worker(w,3);
    World::Plan p;
    p[2].push_back([worker](Stage& s){s.phase_c().hire(worker,3,"FARMER",1);});
    p[4].push_back([](Stage& s){s.extract_resource("WHEAT",2);});
    w.advance(p);
    check(w.building_resource_capacity("WHEAT").feasible_units==1,
          "crop capacity ignored remaining paid inputs or physical resource flow");
    check(w.core().stock("FARMER","R","grain")==2 &&
          w.core().stock("FARMER","R","seed_material")==1 &&
          w.resource_site("WHEAT").standing_biomass==3,
          "real crop resources/materials not consumed or generated atomically");
    const auto bytes=SnapshotCodec::encode(w);
    check(SnapshotCodec::encode(SnapshotCodec::decode(bytes))==bytes,
          "farm operational input was lost by snapshot codec");
    World::Plan unsupported;
    unsupported[4].push_back([](Stage& s){s.extract_resource("WHEAT",2);});
    check(rejects([&]{w.advance(unsupported);}) &&
          w.core().stock("FARMER","R","seed_material")==1 &&
          w.core().stock("FARMER","R","grain")==2,
          "farm material shortage must rollback all actual production");
}
static void test_actual_geographic_site_conditions() {
    vic25::phasee::World map{4500};
    auto& c=map.seed_core().seed_core();
    c.seed_actor("FARMER",0);c.seed_actor("CARRIER",0);
    c.seed_actor("MINER",0);
    map.seed_province(10,"INLAND","STATE",1000);
    map.seed_province(11,"COAST","STATE",1000);
    check(rejects([&]{
        map.seed_building_in_province("FALSE_PORT","CARRIER",10,2,
                                     "vic3_115.development.port");
    }),"inland port accepted without coastal topology");
    map.seed_coast(11,55);
    map.seed_building_in_province("PORT","CARRIER",11,2,
                                 "vic3_115.development.port");
    check(rejects([&]{
        map.seed_building_in_province("UNOWNED_FIELD","FARMER",10,2,
                                     "vic3_115.rural.wheat_farms");
    }),"unowned farmland treated as a cultivated parcel");
    c.seed_shares("land:INLAND:field1","FARMER",1);
    map.seed_building_in_province("FARM","FARMER",10,2,
                                 "vic3_115.rural.wheat_farms",
                                 "land:INLAND:field1");
    check(rejects([&]{
        map.seed_building_in_province("FALSE_MINE","MINER",10,2,
                                     "vic3_115.rural.iron_mines");
    }),"mineral facility accepted with no established extraction title");
    c.seed_shares("mining_right:INLAND:ore1","MINER",1);
    map.seed_building_in_province("MINE","MINER",10,2,
                                 "vic3_115.rural.iron_mines",
                                 "mining_right:INLAND:ore1");
    check(rejects([&]{
        map.seed_building_in_province("NO_RAIL","CARRIER",10,2,
                                     "vic3_115.development.railway");
    }),"rail hub without physical rail link accepted");
    map.seed_geographic_link("RAIL1",10,11,"rail");
    map.seed_building_in_province("RAIL","CARRIER",10,2,
                                 "vic3_115.development.railway");
    check(map.core().site_archetype("PORT")->industry=="LOGI" &&
          map.core().site_archetype("MINE")->industry=="EXTR",
          "siting eligibility did not create validated physical buildings");
}
int main() {
    try {
        test_native_catalog_and_actual_production();
        test_construction_and_recipe_before_commission();
        test_farm_with_real_material_and_renewable_crop();
        test_actual_geographic_site_conditions();
        std::cout<<"Vic3 1.15 / VIC2.5 building P0/P1 catalogue, recipe, snapshot PASS\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"Building P0/P1 FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
