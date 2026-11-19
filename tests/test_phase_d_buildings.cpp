#include "vic25/phase_d_snapshot.hpp"
#include "vic25/building_catalog.hpp"
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
    const auto worker=seed_worker(w,2);
    World::Plan p;
    p[2].push_back([worker](Stage& s){
        s.phase_c().hire(worker,2,"FIRM",1);
    });
    p[4].push_back([](Stage& s){s.manufacture("BREAD",2);});
    w.advance(p);
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
}
int main() {
    try {
        test_native_catalog_and_actual_production();
        test_construction_and_recipe_before_commission();
        std::cout<<"Vic3 1.15 / VIC2.5 building P0/P1 catalogue, recipe, snapshot PASS\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"Building P0/P1 FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
