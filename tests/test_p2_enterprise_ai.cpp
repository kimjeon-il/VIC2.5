#include "vic25/p0_daily.hpp"
#include "vic25/p2_enterprise_ai.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

using namespace vic25;
static void check(bool condition,const char* message) {
    if(!condition)throw std::runtime_error(message);
}
static phaseb::Id worker(phasegh::World& gh,
                         const std::string& region,
                         const std::string& employer,
                         phaseb::People size=1,
                         bool employed=true) {
    auto& c=gh.seed_geography().seed_core().seed_core();
    auto& p=c.seed_population();
    const auto h=p.household_profile({region,"shared","shared",size});
    const auto econ=p.economic_profile({region,"worker","wage","employee",
                                        p.default_education()});
    const auto dem=p.demographic_profile({region,gh.month()-360,
        phaseb::Sex::female,"culture","faith","citizen","free",
        p.default_health(),phaseb::Service::civilian});
    const auto id=p.seed_population(dem,econ,h,size,
                                     static_cast<phaseb::Id>(p.slices().size()+1));
    const auto wallet="HH:"+std::to_string(p.slice(id).key.household);
    if(!c.has_actor(wallet))c.seed_actor(wallet,0);
    if(employed)p.hire(id,size,employer,"worker",30,phaseb::JobRequirements{});
    return id;
}
static void quote_requires_real_supply_and_verified_route() {
    phasegh::World gh{24000};
    auto& e=gh.seed_geography();
    e.seed_province(1,"R","STATE");
    e.seed_province(2,"B","STATE");
    e.seed_border(1,2);
    e.seed_unit(11,"STATE","STATE-ROOT");
    e.seed_assignment(1,11);
    e.seed_assignment(2,11);
    auto& d=e.seed_core();
    auto& c=d.seed_core();
    c.seed_actor("STATE",0);
    c.seed_actor("OWNER",80);
    c.seed_actor("SELLER",100);
    c.seed_actor("CARRIER",50);
    c.seed_government("STATE","HEAD","PM",5,3);
    c.seed_market("R","steel",20,1,60,20);
    c.seed_market("R","iron",2,1,10);
    c.seed_market("R","coal",2,1,10);
    c.seed_market("B","copper",3,1,12);
    c.seed_stock("SELLER","R","iron",10);
    c.seed_stock("SELLER","R","coal",10);
    c.seed_stock("SELLER","B","copper",10);
    d.seed_standard_industries();
    d.seed_building_facility("MILL","OWNER","R",10,"vic3_115.urban.steel_mills");
    phased::IndustryRecipe steel;
    steel.id="STEEL";steel.site="MILL";steel.industry_class="MANU";
    steel.inputs={{"iron",1},{"coal",1}};
    steel.outputs={{"steel",2}};steel.workers_per_batch=1;
    d.seed_building_recipe(steel);
    worker(gh,"R","OWNER");
    const auto& real=gh.base().core().core();
    const auto& domain=gh.base().core().state();
    const auto local=enterprise_ai::evaluate(
        real,domain,gh.base().spatial(),gh.commerce(),steel,1,31);
    check(local.valid && local.margin_per_batch>0 &&
          local.freight_per_batch==0,"local observed profits incorrect");
    steel.inputs.push_back({"copper",1});
    const auto absent=enterprise_ai::evaluate(
        real,domain,gh.base().spatial(),gh.commerce(),steel,1,31);
    check(!absent.valid,"remote goods treated as free instant inventory");
    gh.seed_verified_geographic_link("R2-R1",2,1,"road");
    phasegh::TransportEdge edge;
    edge.id="ROAD";edge.carrier="CARRIER";edge.from=2;edge.to=1;
    edge.monthly_capacity=10;edge.freight_per_unit=2;edge.transit_days=2;
    edge.mode="road";edge.evidence_id="TEST-EVIDENCE";
    edge.valid_from_month=24000;edge.valid_through_month=24100;
    gh.seed_transport_edge(edge);
    const auto routed=enterprise_ai::evaluate(
        real,domain,gh.base().spatial(),gh.commerce(),steel,1,31);
    check(routed.valid && routed.freight_per_batch==2 &&
          routed.margin_per_batch<local.margin_per_batch,
          "actual route tariff/freight failed to affect enterprise margin");
}
static void collateralized_autonomous_new_factory() {
    phasegh::World gh{24000};
    auto& e=gh.seed_geography();
    e.seed_province(1,"R","STATE");
    e.seed_unit(11,"STATE","STATE-ROOT");
    e.seed_assignment(1,11);
    auto& d=e.seed_core();
    auto& c=d.seed_core();
    c.seed_actor("STATE",0);c.seed_actor("OWNER",1);
    c.seed_actor("BUILDER",100);c.seed_actor("SUPPLIER",10);
    c.seed_government("STATE","HEAD","PM",5,3);
    c.seed_market("R","grain",2,1,10);
    c.seed_market("R","groceries",20,1,60,10);
    c.seed_market("R","steel",5,1,20);
    c.seed_market("R","tools",10,1,30);
    c.seed_stock("SUPPLIER","R","grain",15);
    c.seed_stock("OWNER","R","tools",5);
    c.seed_stock("BUILDER","R","steel",1);
    worker(gh,"R","BUILDER");
    worker(gh,"R","OWNER",1,false);
    d.seed_standard_industries();
    phased::ConstructionProject p;
    p.id="FOOD_PLAN";p.client="OWNER";p.contractor="BUILDER";
    p.region="R";p.target_site="NEW_FOOD";p.material_good="steel";
    p.work_required=1;p.material_per_work=1;p.workers_per_work=1;
    p.cash_price_per_work=25;p.target_capacity=10;
    d.seed_building_construction(p,"vic3_115.urban.food_industries");
    phased::IndustryRecipe recipe;
    recipe.id="MAKE_GROCERIES";recipe.site="NEW_FOOD";
    recipe.industry_class="MANU";recipe.inputs={{"grain",1}};
    recipe.outputs={{"groceries",2}};recipe.workers_per_batch=1;
    d.seed_building_recipe(recipe);
    // Credit is issued ONLY after a G Stage9 secured offer, backed by real
    // inventory, and is effective in the next month (not immediately).
    phasegh::World::Plan offer;
    offer[9].push_back([](phasegh::Stage& stage) {
        phasegh::CreditFacility credit;
        credit.id="FOOD_CREDIT";credit.borrower="OWNER";
        credit.pledge_region="R";credit.pledge_good="tools";
        credit.pledged_goods=5;credit.limit=40;
        stage.offer_trade_credit(credit);
    });
    gh.advance(offer);
    check(gh.month()==24001 &&
          enterprise_ai::approved_credit_remaining(
              gh.base().core().core(),gh.commerce(),"OWNER")==40,
          "credit availability was not backed by the live G collateral ledger");
    p0::DailyGame game(daily::World::adopt(std::move(gh),25));
    game.advance_day(); // February 1, autonomous builder pays real construction
    const auto& gh1=game.world().engine();
    const auto& live=gh1.base().core().core();
    const auto& project=gh1.base().core().construction("FOOD_PLAN");
    check(project.accepted_work==1 && project.ready &&
          live.outstanding_loan("OWNER")>=24 &&
          live.stock("BUILDER","R","steel")==0 &&
          !live.facilities().contains("NEW_FOOD"),
          "profitable new factory must use actual bank loan, contractor and input stock");
    game.advance_day();
    check(game.world().engine().base().core().construction("FOOD_PLAN").commissioned &&
          game.world().engine().base().core().core()
            .facilities().contains("NEW_FOOD"),
          "unpaid or immediate site creation incorrectly bypassed commissioning");
    game.advance_day(); // factory hires real POP, buys real grain and produces
    const auto& live_factory=game.world().engine().base().core().core();
    check(live_factory.stock("OWNER","R","groceries")>=2 &&
          live_factory.stock("SUPPLIER","R","grain")<=14 &&
          live_factory.outstanding_loan("OWNER")>=25,
          "new enterprise did not finance and consume physical working capital");
}
static void selects_best_compatible_factory_recipe() {
    phasegh::World gh{24000};
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"R","STATE");
    geo.seed_unit(11,"STATE","STATE-ROOT");
    geo.seed_assignment(1,11);
    auto& d=geo.seed_core();
    auto& c=d.seed_core();
    c.seed_actor("STATE",0);c.seed_actor("OWNER",150);
    c.seed_government("STATE","HEAD","PM",5,3);
    c.seed_market("R","groceries",30,1,100,10);
    c.seed_market("R","grain",2,1,10);
    c.seed_market("R","meat",20,1,50);
    c.seed_stock("OWNER","R","grain",10);
    c.seed_stock("OWNER","R","meat",5);
    d.seed_standard_industries();
    d.seed_building_facility("FOOD","OWNER","R",40,
                            "vic3_115.urban.food_industries");
    phased::IndustryRecipe costly;
    costly.id="COSTLY";costly.site="FOOD";costly.industry_class="MANU";
    costly.inputs={{"grain",1},{"meat",1}};
    costly.outputs={{"groceries",2}};costly.workers_per_batch=1;
    phased::IndustryRecipe cheap=costly;
    cheap.id="CHEAP";cheap.inputs={{"grain",1}};
    d.seed_building_recipe(costly);
    d.seed_building_recipe(cheap);
    worker(gh,"R","OWNER");
    p0::DailyGame game(daily::World::adopt(std::move(gh),9));
    game.advance_day();
    bool cheap_produced=false,costly_produced=false;
    const auto& runs=game.world().engine().base().core().state().actual_production_runs;
    for(const auto& run:runs) {
        if(run.recipe=="CHEAP")cheap_produced=true;
        if(run.recipe=="COSTLY")costly_produced=true;
    }
    check(cheap_produced&&!costly_produced,
          "enterprise AI did not choose the more profitable installed recipe");
}
int main() {
    try {
        quote_requires_real_supply_and_verified_route();
        collateralized_autonomous_new_factory();
        selects_best_compatible_factory_recipe();
        std::cout<<"P2 enterprise market/logistics/secured construction cases PASS\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"P2 enterprise FAIL: "<<e.what()<<"\n";return 1;
    }
}
