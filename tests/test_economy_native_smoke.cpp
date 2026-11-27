#include "vic25/p0_daily.hpp"
#include "vic25/daily_snapshot.hpp"
#include <stdexcept>
#include <optional>
#include <string>
using namespace vic25;

static void check(bool ok,const char* msg) {
    if(!ok)throw std::runtime_error(msg);
}
static phaseb::Id add_worker(phasegh::World& gh) {
    auto& pop=gh.seed_geography().seed_core().seed_core().seed_population();
    const auto h=pop.household_profile({"R","mixed","shared",1});
    const auto e=pop.economic_profile({"R","worker","wage","employee",
                                       pop.default_education()});
    const auto d=pop.demographic_profile({"R",gh.month()-300,
        phaseb::Sex::female,"culture","faith","citizen","free",
        pop.default_health(),phaseb::Service::civilian});
    return pop.seed_population(d,e,h,1);
}
static void greenfield_from_real_cash() {
    phasegh::World gh(24000);
    auto& spatial=gh.seed_geography();
    spatial.seed_province(1,"R","A");
    spatial.seed_unit(11,"A","ROOT");
    spatial.seed_assignment(1,11);
    auto& c=spatial.seed_core().seed_core();
    c.seed_actor("A",10);
    c.seed_actor("INVESTOR",500);
    c.seed_actor("BUILDER",20);
    c.seed_market("R","food",15,1,100,1);
    c.seed_market("R","ore",2,1,100,0);
    c.seed_market("R","steel",3,1,100,0);
    c.seed_stock("BUILDER","R","steel",3);
    c.seed_stock("BUILDER","R","ore",4);
    c.seed_government("A","HEAD","PRIME",5,3);
    const auto slice=add_worker(gh);
    auto& pop=c.seed_population();
    const auto unemp_h=pop.household_profile({"R","unemployed","shared",1});
    const auto unemp=pop.seed_population(pop.slice(slice).key.demographic,
        pop.slice(slice).key.economic,unemp_h,1);
    c.seed_actor("HH:"+std::to_string(pop.slice(unemp).key.household),0);
    const auto household=pop.slice(slice).key.household;
    c.seed_actor("HH:"+std::to_string(household),45);
    pop.hire(slice,1,"BUILDER","worker",10,phaseb::JobRequirements{});
    p0::Policies policy;
    policy.monthly_clothing_units_per_person=0;
    policy.allow_endogenous_investment_credit=false;
    p0::IndustryOpportunity opportunity;
    opportunity.id="new-food";
    opportunity.founder="INVESTOR";
    opportunity.new_firm_equity=100;
    opportunity.project.contractor="BUILDER";
    opportunity.project.region="R";
    opportunity.project.material_good="steel";
    opportunity.project.work_required=2;
    opportunity.project.material_per_work=1;
    opportunity.project.workers_per_work=1;
    opportunity.project.cash_price_per_work=10;
    opportunity.project.target_capacity=50;
    opportunity.recipe.inputs={{"ore",1}};
    opportunity.recipe.outputs={{"food",2}};
    opportunity.recipe.workers_per_batch=1;
    opportunity.recipe.capacity_per_batch=1;
    policy.industry_opportunities.push_back(opportunity);
    p0::DailyGame game(daily::World::adopt(std::move(gh),9),policy);
    game.advance_day();
    const auto id="P0:ENTRY:new-food:24000";
    const std::string firm="P0:FIRM:new-food:24000";
    const auto& after=game.world().engine().base().core();
    check(after.state().constructions.contains(id),"new project not entered");
    check(after.core().cash("INVESTOR")==400,"founder cash not debited");
    check(after.core().cash(firm)==100,"new firm has fictitious/missing cash");
    check(after.core().shares("company:"+firm,"INVESTOR")==100,
          "founder did not receive real shares");
    // New financed actors, corporate share certificates and unfinished
    // construction must survive the native save/load boundary together.
    const auto snapshot=daily::SnapshotCodec::encode(game.world());
    auto restored=daily::SnapshotCodec::decode(snapshot);
    check(restored.engine().base().core().core().cash(firm)==100 &&
          restored.engine().base().core().core().shares(
              "company:"+firm,"INVESTOR")==100 &&
          restored.engine().base().core().state().constructions.contains(id),
          "new company or real WIP planning lost on save/load");
    p0::DailyGame replay(std::move(restored),policy);
    game.advance_day();
    replay.advance_day();
    check(daily::SnapshotCodec::encode(game.world())==
          daily::SnapshotCodec::encode(replay.world()),
          "the same saved corporate economy must replay deterministically");
    check(game.world().engine().base().core().construction(id).accepted_work==1,
          "new private project did not spend real work and material");
}
static void greenfield_uses_yesterdays_solvent_demand(bool has_ore,bool solvent) {
    phasegh::World gh(24000);
    auto& spatial=gh.seed_geography();
    spatial.seed_province(1,"R","A");
    spatial.seed_unit(11,"A","ROOT");
    spatial.seed_assignment(1,11);
    auto& d=spatial.seed_core();
    auto& c=d.seed_core();
    c.seed_actor("A",10);
    c.seed_actor("INVESTOR",500);
    c.seed_actor("BUILDER",20);
    c.seed_market("R","food",15,1,100,0);
    c.seed_market("R","ore",2,1,100,0);
    c.seed_market("R","steel",3,1,100,0);
    c.seed_stock("BUILDER","R","steel",3);
    if(has_ore)c.seed_stock("BUILDER","R","ore",4);
    c.seed_government("A","HEAD","PRIME",5,3);
    const auto slice=add_worker(gh);
    auto& pop=c.seed_population();
    const auto unemp_h=pop.household_profile({"R","unemployed","shared",1});
    const auto unemp=pop.seed_population(pop.slice(slice).key.demographic,
        pop.slice(slice).key.economic,unemp_h,1);
    c.seed_actor("HH:"+std::to_string(pop.slice(unemp).key.household),0);
    const auto household=pop.slice(slice).key.household;
    c.seed_actor("HH:"+std::to_string(household),solvent?45:0);
    pop.hire(slice,1,"BUILDER","worker",30,phaseb::JobRequirements{});
    p0::Policies policy;
    policy.monthly_clothing_units_per_person=0;
    policy.allow_endogenous_investment_credit=false;
    phased::ConstructionProject project;
    project.id="APPROVED_FOOD";
    project.target_site="FOOD_SITE";
    project.client="INVESTOR";
    project.contractor="BUILDER";
    project.region="R";
    project.material_good="steel";
    project.work_required=2;
    project.material_per_work=1;
    project.workers_per_work=1;
    project.cash_price_per_work=10;
    project.target_capacity=50;
    phased::IndustryRecipe recipe;
    recipe.id="FOOD_RECIPE";
    recipe.site=project.target_site;
    recipe.inputs={{"ore",1}};
    recipe.outputs={{"food",2}};
    recipe.workers_per_batch=recipe.capacity_per_batch=1;
    d.seed_construction(project);
    d.seed_recipe(recipe);
    p0::DailyGame game(daily::World::adopt(std::move(gh),9),policy);
    const auto day1=game.world().day_index();
    const auto opening=daily::SnapshotCodec::encode(game.world());
    check(opening.find("VIC25_PHASEC_DAILY_UNMET_1")==std::string::npos,
          "empty market observation changed the legacy snapshot");
    game.advance_day();
    const auto& id=project.id;
    const auto& after=game.world().engine().base().core();
    check(after.construction(id).accepted_work==0,
          "unsold demand cannot start work before it is observed");
    const auto& market=after.core();
    check(market.market("R","food").unmet==0,
          "previous demand leaked into today's live market");
    check(market.previous_daily_solvent_unmet("R","food",day1+1)==(solvent?1:0),
          "verified solvent demand did not survive market close");
    check(market.previous_daily_solvent_unmet("R","food",day1)==0 &&
          market.previous_daily_solvent_unmet("R","food",day1+2)==0 &&
          market.previous_daily_solvent_unmet("R","food",0)==0,
          "market observation escaped its one-day window");
    const auto snapshot=daily::SnapshotCodec::encode(game.world());
    check((snapshot.find("VIC25_PHASEC_DAILY_UNMET_1")!=std::string::npos)==solvent,
          "only positive unmet demand should be stored in daily snapshots");
    // No new food policy or orders on day two; the retained signal can
    // guide construction but must never create a retroactive household sale.
    policy.daily_food_units_per_person=0;
    auto run_next=[&](daily::World next) {
        p0::DailyGame replay(std::move(next),policy);
        replay.advance_day();
        const auto& final=replay.world().engine().base().core();
        check(final.construction(id).accepted_work==(has_ore&&solvent?1:0),
              "yesterday's demand did not authorize only feasible day-two work");
        check(final.core().previous_daily_solvent_unmet("R","food",day1+2)==0,
              "a zero-unmet day must expire the old observation");
        const auto next_snapshot=daily::SnapshotCodec::encode(replay.world());
        check(next_snapshot.find("VIC25_PHASEC_DAILY_UNMET_1")==std::string::npos,
              "zero-unmet close persisted an obsolete observation");
        return next_snapshot;
    };
    check(run_next(game.world())==run_next(daily::SnapshotCodec::decode(snapshot)),
          "saving discarded previous-day investment demand");
}
static void priority_and_class_demand() {
    phaseb::World pop(24000);
    const auto h=pop.household_profile({"R","mixed","shared",2});
    const auto econ=pop.economic_profile({"R","worker","wage","employee",
                                          pop.default_education()});
    const auto d=pop.demographic_profile({"R",23700,phaseb::Sex::female,
        "culture","faith","citizen","free",pop.default_health(),
        phaseb::Service::civilian});
    pop.seed_population(d,econ,h,2);
    const std::vector<p0::HouseholdBasketRule> rules{
        {"furniture","employee",31,2}};
    const auto basket=p0::household_daily_needs(pop,1,31,1,31,rules);
    check(basket.at({"R",h,"food"}).quantity==2 &&
          basket.at({"R",h,"food"}).priority==0,"food needs wrong");
    check(basket.at({"R",h,"clothing"}).quantity==2 &&
          basket.at({"R",h,"clothing"}).priority==1,"clothing needs wrong");
    check(basket.at({"R",h,"furniture"}).quantity==2 &&
          basket.at({"R",h,"furniture"}).priority==2,"class basket wrong");
    const auto slice=pop.slices().begin()->first;
    const auto explicit_food=p0::household_daily_needs(
        pop,1,31,1,31,rules,{{slice,"groceries"}});
    check(!explicit_food.contains({"R",h,"food"})&&
          !explicit_food.contains({"R",h,"groceries"})&&
          explicit_food.at({"R",h,"clothing"}).quantity==2,
          "explicit groceries must replace only the default food demand");
}
static void endogenous_hiring_production_and_payroll() {
    phasegh::World gh(24000);
    auto& spatial=gh.seed_geography();
    spatial.seed_province(1,"R","A");
    spatial.seed_unit(11,"A","ROOT");
    spatial.seed_assignment(1,11);
    auto& c=spatial.seed_core().seed_core();
    c.seed_actor("A",1);
    c.seed_actor("GROWER",200);
    c.seed_market("R","food",9,1,100,0);
    c.seed_market("R","ore",1,1,100,0);
    c.seed_production_facility("FARM","GROWER","R",31);
    c.seed_stock("GROWER","R","ore",31);
    c.seed_government("A","HEAD","PRIME",5,3);
    const auto slice=add_worker(gh);
    const auto household=c.seed_population().slice(slice).key.household;
    c.seed_actor("HH:"+std::to_string(household),0);
    phased::IndustryRecipe recipe;
    recipe.id="GRAIN";recipe.site="FARM";
    recipe.inputs={{"ore",1}};
    recipe.outputs={{"food",2}};
    recipe.workers_per_batch=1;recipe.capacity_per_batch=1;
    spatial.seed_core().seed_recipe(recipe);
    p0::Policies policy;
    policy.monthly_clothing_units_per_person=0;
    p0::DailyGame game(daily::World::adopt(std::move(gh),8),policy);
    game.advance_day();
    const auto& core=game.world().engine().base().core().core();
    check(core.people().employed(slice)==1,"real unemployed POP not hired");
    check(core.stock("GROWER","R","ore")==30,"factory ignored real inputs");
    check(core.stock("GROWER","R","food")==1,"wage-funded food was not bought");
    check(core.cash("HH:"+std::to_string(household))==0,
          "first-day wage or paid consumption was omitted");
    check(core.cash("GROWER")==200,
          "real wage and goods sale failed to balance");
}

static void intermediate_goods_are_purchased_not_created() {
    phasegh::World gh(24000);
    auto& spatial=gh.seed_geography();
    spatial.seed_province(1,"R","A");
    spatial.seed_unit(11,"A","ROOT");
    spatial.seed_assignment(1,11);
    auto& c=spatial.seed_core().seed_core();
    c.seed_actor("A",1);
    c.seed_actor("FACTORY",200);
    c.seed_actor("MINE",0);
    c.seed_market("R","food",9,1,100,0);
    c.seed_market("R","ore",1,1,100,0);
    c.seed_stock("MINE","R","ore",5);
    c.seed_production_facility("PLANT","FACTORY","R",31);
    c.seed_government("A","HEAD","PRIME",5,3);
    const auto slice=add_worker(gh);
    const auto house=c.seed_population().slice(slice).key.household;
    c.seed_actor("HH:"+std::to_string(house),0);
    phased::IndustryRecipe recipe;
    recipe.id="PROCESS";recipe.site="PLANT";
    recipe.inputs={{"ore",1}};recipe.outputs={{"food",2}};
    recipe.workers_per_batch=1;recipe.capacity_per_batch=1;
    spatial.seed_core().seed_recipe(recipe);
    p0::Policies policy;
    policy.monthly_clothing_units_per_person=0;
    p0::DailyGame game(daily::World::adopt(std::move(gh),8),policy);
    game.advance_day();
    const auto& day1=game.world().engine().base().core().core();
    check(day1.people().employed(slice)==1,"industry did not hire real labor");
    // The first purchase supplies today's run; the late purchase restocks
    // one input that can be used tomorrow. Both must reach the real seller.
    const auto verify_procurement = [&](Quantity expected_batches) {
        const auto& industrial=game.world().engine().base().core();
        const auto& ledger=industrial.core();
        Quantity used_ore=0;
        for(const auto& run:industrial.state().actual_production_runs)
            if(run.recipe=="PROCESS")used_ore+=run.batches;
        const auto factory_ore=ledger.stock("FACTORY","R","ore");
        const auto mine_ore=ledger.stock("MINE","R","ore");
        check(used_ore==expected_batches,"purchased ore did not feed real production");
        check(factory_ore+mine_ore+used_ore==5,
              "same-region input procurement created or lost ore");
        check(ledger.cash("MINE")==5-mine_ore,
              "mine did not receive actual payment for the ore sold");
    };
    check(day1.stock("FACTORY","R","ore")==1 && day1.stock("MINE","R","ore")==3,
          "same-day production and replenishment did not each buy one input");
    verify_procurement(1);
    game.advance_day();
    verify_procurement(2);
}
// Both outputs are real catalog-bound manufactured goods; a daily test
// with only legacy "food" does not exercise either consumer market.
static void real_food_and_textile_sales(bool legacy_quotes,bool custom_goods=false) {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"R","A");
    geo.seed_unit(11,"A","ROOT");
    geo.seed_assignment(1,11);
    auto& d=geo.seed_core();
    auto& c=d.seed_core();
    c.seed_actor("A",0);
    c.seed_government("A","HEAD","PRIME",5,3);
    c.seed_actor("FOOD_CO",200);
    c.seed_actor("TEXTILE_CO",200);
    c.seed_market("R","grain",1,1,100,0);
    c.seed_market("R","fabric",1,1,100,0);
    c.seed_market("R","groceries",9,1,100,0);
    c.seed_market("R","clothes",8,1,100,0);
    if(legacy_quotes) {
        c.seed_market("R","food",9,1,100,0);
        c.seed_market("R","clothing",8,1,100,0);
    }
    c.seed_stock("FOOD_CO","R","grain",2);
    c.seed_stock("TEXTILE_CO","R","fabric",2);
    d.seed_standard_industries();
    d.seed_building_facility("FOOD","FOOD_CO","R",31,
                            "vic3_115.urban.food_industries");
    d.seed_building_facility("TEXTILE","TEXTILE_CO","R",31,
                            "vic3_115.urban.textile_mills");
    phased::IndustryRecipe r;
    r.id="PROCESS";r.site="FOOD";r.industry_class="MANU";
    r.inputs={{"grain",1}};r.outputs={{"groceries",3}};
    r.workers_per_batch=1;
    d.seed_building_recipe(r);
    r.id="WEAVE";r.site="TEXTILE";
    r.inputs={{"fabric",1}};r.outputs={{"clothes",3}};
    d.seed_building_recipe(r);
    const auto food_worker=add_worker(gh);
    auto& people=c.seed_population();
    const auto& first=people.slice(food_worker);
    const auto home=first.key.household;
    const auto economic=first.key.economic;
    const auto second=people.demographic_profile({"R",gh.month()-330,
        phaseb::Sex::male,"culture","faith","citizen","free",
        people.default_health(),phaseb::Service::civilian});
    const auto textile_worker=people.seed_population(second,economic,home,1,2);
    const auto wallet="HH:"+std::to_string(home);
    c.seed_actor(wallet,0);
    people.hire(food_worker,1,"FOOD_CO","worker",620,{});
    people.hire(textile_worker,1,"TEXTILE_CO","worker",620,{});
    p0::Policies policies;
    policies.monthly_clothing_units_per_person=31;
    if(custom_goods)policies.household_basket={
        {"groceries","employee",31,0},{"clothes","employee",31,1}};
    p0::DailyGame game(daily::World::adopt(std::move(gh),17),policies);
    p0::DailyGame replay(daily::SnapshotCodec::decode(
        daily::SnapshotCodec::encode(game.world())),policies);
    game.advance_day();replay.advance_day();
    const auto& live=game.world().engine().base().core().core();
    check(live.stock("FOOD_CO","R","grain")==1&&
          live.stock("TEXTILE_CO","R","fabric")==1,
          "catalog-bound plants did not run real-input production");
    if(!legacy_quotes||custom_goods) {
        check(live.stock("FOOD_CO","R","groceries")==1&&
              live.stock("TEXTILE_CO","R","clothes")==1,
              "household demand did not buy both real catalog products");
        check(live.cash(wallet)==2&&live.cash("FOOD_CO")==198&&
              live.cash("TEXTILE_CO")==196&&live.cash("A")==4,
              "the two wages, sales and tax did not conserve household, firm and treasury cash");
        check(live.regional_food_grievances().empty(),
              "fulfilled groceries falsely left a second legacy food gap");
    } else {
        check(live.stock("FOOD_CO","R","groceries")==1&&
              live.stock("TEXTILE_CO","R","clothes")==3&&live.cash(wallet)==18&&
              live.cash("A")==4,
              "basic food should substitute without opening a second clothing need");
        check(live.regional_food_grievances().empty(),
              "fulfilled groceries should not leave a separate legacy food gap");
    }
    check(daily::SnapshotCodec::encode(game.world())==
          daily::SnapshotCodec::encode(replay.world()),
          "catalog household demand changed after save and restore");
}
static void grocery_shortage_cannot_cancel_a_different_goods_tariff() {
    for(const bool legacy_need:{false,true}) {
        phasegh::World gh(24000);
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"R","A");
        geo.seed_unit(11,"A","ROOT");
        geo.seed_assignment(1,11);
        auto& c=geo.seed_core().seed_core();
        c.seed_actor("A",0);
        c.seed_government("A","HEAD","PRIME",5,3);
        c.seed_market("R","food",9,1,100,0);
        c.seed_market("R","groceries",9,1,100,0);
        gh.seed_tariff("A","food",100'000);
        const auto slice=add_worker(gh);
        const auto home=c.people().slice(slice).key.household;
        c.seed_actor("HH:"+std::to_string(home),100);
        p0::Policies p;
        p.monthly_clothing_units_per_person=0;
        if(!legacy_need)p.household_basket={{"groceries","employee",62,0}};
        else p.daily_food_units_per_person=2;
        p0::DailyGame game(daily::World::adopt(std::move(gh),29),p);
        game.advance_day();
        check(game.world().engine().base().core().core()
                  .regional_food_grievances().at("R")==1,
              "both physical foods must register genuine deprivation");
        game.advance_day();
        check(game.world().engine().diplomacy().tariffs.at({"A","food"}).ppm==
              (legacy_need?0:100'000),
              "mixed-market relief must follow observed need for the tariffed good");
    }
}
static void staple_food_substitutes_share_one_need() {
    struct Case { Quantity available_food; phasec::Money funds; Quantity expected; };
    for(const Case trial:std::array{Case{4,30,2},Case{1,30,1},Case{4,14,1}}) {
        phasegh::World gh(24000);
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"R","A");
        geo.seed_unit(11,"A","ROOT");
        geo.seed_assignment(1,11);
        auto& c=geo.seed_core().seed_core();
        c.seed_actor("A",0);
        c.seed_government("A","HEAD","PRIME",5,3);
        c.seed_actor("FARM",0);
        c.seed_actor("MILL",0);
        c.seed_market("R","food",9,1,100,0);
        c.seed_market("R","groceries",5,1,100,0);
        c.seed_stock("FARM","R","food",trial.available_food);
        c.seed_stock("MILL","R","groceries",1);
        const auto slice=add_worker(gh);
        const auto wallet="HH:"+std::to_string(c.people().slice(slice).key.household);
        c.seed_actor(wallet,trial.funds);
        p0::Policies p;
        p.daily_food_units_per_person=3;
        p.monthly_clothing_units_per_person=0;
        p0::DailyGame game(daily::World::adopt(std::move(gh),29),p);
        p0::DailyGame replay(daily::SnapshotCodec::decode(
            daily::SnapshotCodec::encode(game.world())),p);
        bool checked_before_close=false;
        phasegh::World::Plan observe{};
        observe[7].push_back([&](phasegh::Stage& stage) {
            const auto& fresh=stage.phase_c();
            const auto& staple=fresh.market("R","food");
            const auto& cheap=fresh.market("R","groceries");
            const Quantity shortfall=trial.available_food==1?1:0;
            check(staple.delivered==trial.expected&&staple.solvent_demand==trial.expected&&
                  staple.unmet==0,"alternative recorded unmet or duplicated demand");
            check(cheap.delivered==1&&cheap.solvent_demand==1+shortfall&&
                  cheap.unmet==shortfall,"cheap food recorded wrong live demand");
            checked_before_close=true;
        });
        game.advance_day(observe);replay.advance_day();
        check(checked_before_close,"the observation must run before market close");
        const auto& live=game.world().engine().base().core().core();
        const auto bought_food=trial.expected;
        check(live.cash("MILL")==5&&live.cash("FARM")==9*bought_food&&
              live.cash(wallet)==trial.funds-5-9*bought_food,
              "one food need should split between the cheaper and fallback sellers");
        check(live.stock("MILL","R","groceries")==0&&
              live.stock("FARM","R","food")==trial.available_food-bought_food,
              "the two purchases must use distinct real inventories without extra demand");
        check(live.regional_food_grievances().contains("R")==(bought_food<2),
              "only combined food shortage may become a regional grievance");
        check(live.previous_daily_solvent_unmet("R","groceries",game.world().day_index())==
                  (trial.available_food==1?1:0)&&
              live.previous_daily_solvent_unmet("R","food",game.world().day_index())==0,
              "only one market should receive the single unmet unit");
        check(daily::SnapshotCodec::encode(game.world())==
              daily::SnapshotCodec::encode(replay.world()),
              "food substitution changed after save and restore");
    }
}
static void substitute_food_sales_redirect_next_day_production() {
    phasegh::World gh(24000);
    auto& spatial=gh.seed_geography();
    spatial.seed_province(1,"R","A");
    spatial.seed_unit(11,"A","ROOT");
    spatial.seed_assignment(1,11);
    auto& c=spatial.seed_core().seed_core();
    c.seed_actor("A",0);
    c.seed_actor("MILL",0);
    c.seed_actor("GROWER",100);
    c.seed_market("R","groceries",5,5,5,0);
    c.seed_market("R","food",9,9,9,2);
    c.seed_market("R","grain",1,1,1,0);
    c.seed_stock("MILL","R","groceries",3);
    c.seed_stock("GROWER","R","food",2);
    c.seed_stock("GROWER","R","grain",10);
    c.seed_production_facility("FARM","GROWER","R",31);
    c.seed_government("A","HEAD","PRIME",5,3);
    const auto slice=add_worker(gh);
    auto& pop=c.seed_population();
    const auto home=pop.slice(slice).key.household;
    c.seed_actor("HH:"+std::to_string(home),100);
    pop.hire(slice,1,"GROWER","worker",31,{});
    phased::IndustryRecipe recipe;
    recipe.id="MORE-FOOD";recipe.site="FARM";
    recipe.inputs={{"grain",1}};recipe.outputs={{"food",2}};
    recipe.workers_per_batch=1;recipe.capacity_per_batch=1;
    spatial.seed_core().seed_recipe(recipe);
    p0::Policies p;
    p.monthly_clothing_units_per_person=0;
    p.daily_food_units_per_person=3;
    p0::DailyGame game(daily::World::adopt(std::move(gh),8),p);
    phasegh::World::Plan observe{};
    std::int64_t cheap=0,alt=0,unmet=0;
    observe[7].push_back([&](phasegh::Stage& stage){
        const auto& market=stage.phase_c();
        cheap=market.market("R","groceries").delivered;
        alt=market.market("R","food").delivered;
        unmet=market.market("R","groceries").unmet+market.market("R","food").unmet;
    });
    game.advance_day(observe);
    auto get=[&]()->const phasec::IntegratedWorld&{return game.world().engine().base().core().core();};
    check(cheap==3&&alt==0&&unmet==0&&get().stock("GROWER","R","grain")==10,
         "producer funded and staffed but shouldn't run on cheap-only day");
    game.advance_day(observe);
    check(cheap==0&&alt==3&&unmet==0&&get().stock("GROWER","R","grain")==9&&
         get().stock("GROWER","R","food")==1,
         "depleted cheap food should redirect forecast to real alternative production and sales");
}
enum class DividendTrial { enabled, disabled, no_title, no_income, indebted, forgiven, reserve };
static void registered_household_dividend(DividendTrial trial) {
    phasegh::World engine(24000);
    auto& geo=engine.seed_geography();
    geo.seed_province(1,"R","A");geo.seed_unit(11,"A","ROOT");geo.seed_assignment(1,11);
    auto& c=geo.seed_core().seed_core();
    const bool support_only=trial==DividendTrial::no_income;
    c.seed_actor("A",support_only?10:0);c.seed_actor("SELLER",4);
    c.seed_government("A","HEAD","PRIME",5,3);
    c.seed_market("R","food",5,5,5,0);c.seed_stock("SELLER","R","food",2);
    auto& pop=c.seed_population();
    const auto payer=pop.household_profile({"R","payer","shared",1});
    const auto owner=pop.household_profile({"R","worker-owner","shared",1});
    const auto d=pop.demographic_profile({"R",engine.month()-300,phaseb::Sex::female,
        "culture","faith","citizen","free",pop.default_health(),phaseb::Service::civilian});
    const auto e=pop.economic_profile({"R","worker","wage","employee",pop.default_education()});
    pop.seed_population(d,e,payer,1);pop.seed_population(d,e,owner,1);
    const auto payer_actor="HH:"+std::to_string(payer);
    const auto owner_actor="HH:"+std::to_string(owner);
    const phasec::Money payer_open=(support_only||trial==DividendTrial::forgiven)?0:5;
    c.seed_actor(payer_actor,payer_open);c.seed_actor(owner_actor,0);
    if(trial!=DividendTrial::no_title)c.seed_shares("company:SELLER",owner_actor,1);
    p0::Policies p;p.monthly_clothing_units_per_person=0;
    p.allow_endogenous_debt_repayment=false;
    if(trial!=DividendTrial::disabled)
        p.registered_dividend_reserves["SELLER"]=trial==DividendTrial::reserve?10:4;
    p0::DailyGame game(daily::World::adopt(std::move(engine),8),p);
    const auto get=[&]()->const phasec::IntegratedWorld&{return game.world().engine().base().core().core();};
    phasegh::World::Plan borrow{};
    const phasec::Money borrowing=trial==DividendTrial::forgiven?7:
        (trial==DividendTrial::indebted?2:0);
    if(borrowing>0)borrow[1].push_back([borrowing](phasegh::Stage& stage){
        stage.phase_c().lend("SELLER",borrowing);
    });
    if(trial==DividendTrial::forgiven)borrow[8].push_back([borrowing](phasegh::Stage& stage){
        stage.phase_c().forgive_defaulted_loan("SELLER",borrowing);
    });
    if(support_only)borrow[3].push_back([](phasegh::Stage& stage){
        stage.phase_c().pay("A","SELLER",10);
    });
    game.advance_day(borrow);
    while(game.world().date().day<31)game.advance_day();
    game.advance_day();
    p0::DailyGame restored(daily::SnapshotCodec::decode(daily::SnapshotCodec::encode(game.world())),p);
    const Quantity initial_sales=payer_open>0?1:0;
    check(get().stock("SELLER","R","food")==2-initial_sales,"new funds or food were fabricated before month close");
    game.advance_day();restored.advance_day();
    const auto extra=trial==DividendTrial::enabled?1:0;
    check(get().stock("SELLER","R","food")==2-initial_sales-extra,
          "real share dividend should fund exactly one extra purchase");
    check(daily::SnapshotCodec::encode(game.world())==daily::SnapshotCodec::encode(restored.world()),
          "automatic payout changed across save and replay");
    check(get().cash("SELLER")+get().cash(payer_actor)+get().cash(owner_actor)==
          4+payer_open+(support_only?10:0)+borrowing,"dividend or purchase minted money");
    if(trial==DividendTrial::enabled) {
        // The February purchase earned one new real receipt. The next
        // payout can transfer that receipt, but the idle fourth month
        // must not distribute it again simply because the firm has cash.
        while(game.world().date().month==2)game.advance_day();
        game.advance_day();
        check(get().cash(owner_actor)==5&&get().cash("SELLER")==4,
              "only newly realized sales may be distributed a second time");
        while(game.world().date().month==3)game.advance_day();
        game.advance_day();
        check(get().cash(owner_actor)==5&&get().cash("SELLER")==4,
              "the same monthly earnings must not be paid repeatedly");
    }
}

// A fixture ratio, not a claim about the official 1.15 production values.
static void copper_mine_supplies_paid_telephone_sales(bool has_mine) {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"R","A");
    geo.seed_unit(1,"A","Copper fixture");
    geo.seed_assignment(1,1);
    auto& d=geo.seed_core();
    auto& c=d.seed_core();
    c.seed_actor("A",0);
    c.seed_government("A","HEAD","PM",5,3);
    c.seed_actor("PLANT",100);
    c.seed_actor("MINER",100);
    c.seed_market("R","copper",3,3,3,3);
    c.seed_market("R","tools",2,2,2,0);
    c.seed_market("R","steel",2,2,2,0);
    c.seed_market("R","telephones",11,11,11,2);
    c.seed_stock("PLANT","R","tools",4);
    c.seed_stock("PLANT","R","steel",4);
    d.seed_standard_industries();
    d.seed_building_facility("ELECTRICS","PLANT","R",31,
                             "vic3_115.urban.electrics_industries");
    phased::IndustryRecipe recipe;
    recipe.id="WIRED";recipe.site="ELECTRICS";recipe.industry_class="MANU";
    recipe.inputs={{"tools",1},{"steel",1},{"copper",1}};
    recipe.outputs={{"telephones",1}};
    recipe.workers_per_batch=recipe.capacity_per_batch=1;
    d.seed_building_recipe(recipe);
    if(has_mine) {
        d.seed_building_facility("MINE","MINER","R",31,
                                 "vic3_115.rural.copper_mines");
        phased::ResourceSite mine;
        mine.id="EXTRACT";mine.site="MINE";mine.owner="MINER";mine.region="R";
        mine.good="copper";mine.monthly_capacity=31;
        d.seed_building_resource_site(mine);
    }
    auto& pop=c.seed_population();
    auto hire=[&](const std::string& actor) {
        const auto home=pop.household_profile({"R",actor,"shared",1});
        const auto eco=pop.economic_profile({"R","worker","wage","employee",
                                             pop.default_education()});
        const auto demo=pop.demographic_profile({"R",23640,phaseb::Sex::female,
            "culture","faith","citizen","free",pop.default_health(),phaseb::Service::civilian});
        const auto worker=pop.seed_population(demo,eco,home,1);
        const auto wallet="HH:"+std::to_string(home);
        c.seed_actor(wallet,actor=="PLANT"?22:0);
        pop.hire(worker,1,actor,"worker",31,{});
        return wallet;
    };
    const auto buyer=hire("PLANT");
    const auto miner=hire("MINER");
    p0::Policies rules;
    rules.daily_food_units_per_person=0;
    rules.monthly_clothing_units_per_person=0;
    rules.allow_endogenous_staffing=false;
    rules.household_basket={{"telephones","employee",31,0}};
    p0::DailyGame game(daily::World::adopt(std::move(gh),17),rules);
    game.advance_day();
    const auto state=[&]() -> const phased::World& {
        return game.world().engine().base().core();
    };
    check(state().core().firm_finance("PLANT").receipts==0,
          "telephones appeared before extraction made copper available");
    const auto saved=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame replay(daily::SnapshotCodec::decode(saved),rules);
    for(int day=2;day<=3;++day) {
        game.advance_day();
        replay.advance_day();
        const auto& ledger=state().core();
        const auto made=state().realized_batches("MANU",24000,24001);
        const auto extracted=has_mine?state().resource_site("EXTRACT").cumulative_output:0;
        const auto mine_stock=ledger.stock("MINER","R","copper");
        const auto plant_stock=ledger.stock("PLANT","R","copper");
        const auto sales=ledger.firm_finance("PLANT").receipts;
        check(mine_stock+plant_stock+made==extracted &&
              ledger.firm_finance("MINER").receipts==3*(plant_stock+made),
              "copper moved or disappeared without purchase and conversion");
        check(ledger.stock("PLANT","R","tools")==4-made &&
              ledger.stock("PLANT","R","steel")==4-made &&
              sales==11*(made-ledger.stock("PLANT","R","telephones")),
              "real components and telephone purchases do not reconcile");
        check(ledger.cash("A")+ledger.cash("MINER")+ledger.cash("PLANT")+
              ledger.cash(buyer)+ledger.cash(miner)==222&&ledger.financial().balanced(),
              "cash was created or lost in the copper-to-telephone chain");
        check(daily::SnapshotCodec::encode(game.world())==
              daily::SnapshotCodec::encode(replay.world()),"copper replay diverged");
        if(has_mine)check(made>0 && sales>0,"extracted copper never reached a real telephone buyer");
        else check(made==0 && sales==0,"telephone recipe bypassed its copper requirement");
    }
}

static void plain_clothing_has_a_bounded_fabric_fallback() {
    struct Case {
        Quantity clothes; phasec::Money funds; bool exact_basket;
        Quantity bought_clothes,bought_fabric,unmet;
    };
    for(const auto trial:std::array{Case{1,40,false,1,2,1},Case{4,40,false,4,0,0},
                                  Case{0,5,false,0,0,0},Case{1,40,true,1,0,3}}) {
        phasegh::World gh(24000);
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"R","A");geo.seed_unit(1,"A","Clothing fixture");
        geo.seed_assignment(1,1);
        auto& c=geo.seed_core().seed_core();
        c.seed_actor("A",0);c.seed_government("A","HEAD","PM",5,3);
        for(const auto seller:{"BAKER","MILL","FARM"})c.seed_actor(seller,0);
        c.seed_market("R","groceries",5,5,5,0);
        c.seed_market("R","clothes",8,8,8,0);
        c.seed_market("R","fabric",3,3,3,0);
        c.seed_stock("BAKER","R","groceries",1);
        c.seed_stock("FARM","R","fabric",10);
        if(trial.clothes>0)c.seed_stock("MILL","R","clothes",trial.clothes);
        const auto pop=add_worker(gh);
        const auto wallet="HH:"+std::to_string(c.people().slice(pop).key.household);
        c.seed_actor(wallet,trial.funds);
        p0::Policies p;
        p.monthly_clothing_units_per_person=124; // four integer fixture units each January day
        if(trial.exact_basket)p.household_basket={{"clothes","employee",124,1}};
        p0::DailyGame game(daily::World::adopt(std::move(gh),29),p);
        p0::DailyGame replay(daily::SnapshotCodec::decode(
            daily::SnapshotCodec::encode(game.world())),p);
        phasegh::World::Plan audit;
        audit[7].push_back([&](phasegh::Stage& stage) {
            const auto& live=stage.phase_c();
            const auto& clothes=live.market("R","clothes");
            const auto& fabric=live.market("R","fabric");
            check(fabric.delivered==trial.bought_fabric&&fabric.unmet==0,
                  "fabric did not fill just the allowed clothing shortage");
            check(clothes.delivered==trial.bought_clothes&&clothes.unmet==trial.unmet,
                  "substituted demand still counted as unmet clothes");
        });
        game.advance_day(audit);replay.advance_day();
        const auto& live=game.world().engine().base().core().core();
        check(live.cash("BAKER")==5&&live.cash("MILL")==8*trial.bought_clothes&&
              live.cash("FARM")==3*trial.bought_fabric&&
              live.cash(wallet)+live.cash("BAKER")+live.cash("MILL")+live.cash("FARM")==trial.funds&&
              live.stock("FARM","R","fabric")==10-trial.bought_fabric&&
              live.stock(wallet,"R","fabric")==0&&live.financial().balanced()&&
              daily::SnapshotCodec::encode(game.world())==daily::SnapshotCodec::encode(replay.world()),
              "food priority, paid cloth purchases or replay changed");
    }
}

static void monthly_clothing_cap_is_shared_across_household_slices() {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"R","A");
    geo.seed_unit(1,"A","Clothing rounding fixture");
    geo.seed_assignment(1,1);
    auto& c=geo.seed_core().seed_core();
    c.seed_actor("A",0);
    c.seed_actor("SUPPLIER",0);
    c.seed_market("R","fabric",3,3,3,0);
    c.seed_stock("SUPPLIER","R","fabric",10);
    const auto slice=add_worker(gh);
    auto& pop=c.seed_population();
    const auto original=pop.slice(slice).key;
    pop.seed_population(original.demographic,original.economic,original.household,1,2);
    const auto wallet="HH:"+std::to_string(original.household);
    c.seed_actor(wallet,20);
    p0::Policies p;
    p.daily_food_units_per_person=0;
    p.monthly_clothing_units_per_person=1;
    p0::DailyGame game(daily::World::adopt(std::move(gh),29),p);
    const auto ledger=[&]() -> const phasec::IntegratedWorld& {
        return game.world().engine().base().core().core();
    };
    for(int day=1;day<=30;++day)game.advance_day();
    check(ledger().cash(wallet)==20&&ledger().stock("SUPPLIER","R","fabric")==10,
          "fabric purchased before the household's second monthly clothing unit was due");
    const auto saved=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame replay(daily::SnapshotCodec::decode(saved),p);
    game.advance_day();replay.advance_day();
    check(ledger().cash(wallet)==17&&ledger().cash("SUPPLIER")==3&&
          ledger().stock("SUPPLIER","R","fabric")==9&&
          daily::SnapshotCodec::encode(game.world())==daily::SnapshotCodec::encode(replay.world()),
          "half of two monthly household clothing units must be replaceable after splitting the POP");
}

enum class StapleTrial { viable, break_even, supply_interruption };
static void linked_staples_keep_running_across_payroll_rounding(StapleTrial trial) {
    constexpr phasec::Money grain_quote=4;
    const bool shock=trial==StapleTrial::supply_interruption;
    const bool break_even=trial==StapleTrial::break_even;
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"R","A");geo.seed_unit(1,"A","Regional food and clothing");
    geo.seed_assignment(1,1);
    auto& d=geo.seed_core();
    auto& c=d.seed_core();
    c.seed_actor("GOV",0);
    c.seed_government("A","HEAD","PM",5,3);
    c.seed_country_treasury("A","GOV");
    c.seed_country_income_tax("A",0);
    const std::array actors={"WHEAT","COTTON","BAKERY","WEAVER"};
    for(const auto firm:actors)c.seed_actor(firm,500);
    c.seed_market("R","grain",grain_quote,grain_quote,grain_quote,2);
    c.seed_market("R","fabric",6,6,6,2);
    c.seed_market("R","groceries",2,2,2,4);
    c.seed_market("R","clothes",5,5,5,2);
    c.seed_stock("BAKERY","R","grain",1);
    c.seed_stock("WEAVER","R","fabric",1);
    c.seed_stock("WHEAT","R","tools",31);
    d.seed_standard_industries();
    auto crop=[&](std::string actor,std::string commodity,std::string archetype) {
        auto title="land:R:"+commodity;
        c.seed_shares(title,actor,1);
        geo.seed_building_in_province(actor,actor,1,31,archetype,title);
        phased::ResourceSite source;
        source.id=source.site=source.owner=actor;
        source.region="R";source.good=commodity;source.monthly_capacity=31;
        source.renewable=true;source.standing_biomass=source.carrying_capacity=100;
        source.renewal_per_month=31;
        d.seed_building_resource_site(source);
    };
    auto factory=[&](std::string actor,std::string input,std::string output,Quantity count,std::string archetype) {
        geo.seed_building_in_province(actor,actor,1,31,archetype);
        phased::IndustryRecipe r;
        r.id=r.site=actor;r.industry_class="MANU";
        r.inputs={{input,1}};r.outputs={{output,count}};
        r.workers_per_batch=r.capacity_per_batch=1;
        d.seed_building_recipe(r);
    };
    crop("WHEAT","grain","vic3_115.rural.wheat_farms");
    crop("COTTON","fabric","vic3_115.rural.cotton_plantations");
    factory("BAKERY","grain","groceries",4,"vic3_115.urban.food_industries");
    factory("WEAVER","fabric","clothes",2,"vic3_115.urban.textile_mills");
    auto& pop=c.seed_population();
    const auto home=pop.household_profile({"R","workers","shared",4});
    const auto econ=pop.economic_profile({"R","worker","wage","employee",pop.default_education()});
    const auto demo=pop.demographic_profile({"R",23640,phaseb::Sex::female,
        "culture","faith","citizen","free",pop.default_health(),phaseb::Service::civilian});
    const auto wallet="HH:"+std::to_string(home);
    c.seed_actor(wallet,150);
    for(std::size_t i=0;i<actors.size();++i) {
        const auto worker=pop.seed_population(demo,econ,home,1,static_cast<phaseb::Id>(i+1));
        pop.hire(worker,1,actors[i],"worker",std::string_view(actors[i])=="BAKERY"&&break_even?116:93,{});
    }
    p0::Policies rules;
    rules.monthly_clothing_units_per_person=8;
    rules.allow_endogenous_staffing=false;
    rules.allow_endogenous_investment_credit=false;
    rules.allow_endogenous_factory_expansion=false;
    p0::DailyGame game(daily::World::adopt(std::move(gh),17),rules);
    std::optional<p0::DailyGame> replay;
    phasec::Money prev_sales=0;
    for(int day=1;day<=65;++day) {
        phasegh::World::Plan events{};
        if(shock&&day==35) events[10].push_back([](phasegh::Stage& gh){
            gh.phase_c().war_damage_facility("WHEAT",31);
        });
        if(shock&&day==40) events[4].push_back([](phasegh::Stage& gh){
            gh.phase_c().phase_time_maintain_facility("WHEAT","tools",31,1,1);
        });
        game.advance_day(events);
        const auto& d=game.world().engine().base().core();
        const auto& c=d.core();
        if(replay) {
            replay->advance_day(events);
            check(daily::SnapshotCodec::encode(game.world())==
                  daily::SnapshotCodec::encode(replay->world()),"replay changed");
        }
        if(day==31) replay.emplace(daily::SnapshotCodec::decode(daily::SnapshotCodec::encode(game.world())),rules);
        auto cash=c.cash(wallet)+c.cash("GOV");
        for(const auto firm:actors)cash+=c.cash(firm);
        check(cash==2150&&c.financial().balanced(),"cash changed");
        const auto sales=c.firm_finance("BAKERY").receipts;
        if(day>=32&&day<=60) {
            // Damage enters at worker-day close. Existing grain feeds the
            // next batch; replacement grain arrives only after real repair.
            const bool no_sale=break_even||(shock&&day>=37&&day<=41);
            check(sales-prev_sales==(no_sale?0:8),
                  "linked food chain did not match its viable, break-even or interrupted market");
            if(shock&&day>=35&&day<40)
                check(c.facility("WHEAT").damaged==31&&c.stock("WHEAT","R","tools")==31,"damage did not persist");
            if(shock&&day>=40)
                check(c.facility("WHEAT").damaged==0&&c.stock("WHEAT","R","tools")==0,"restoration without inputs");
            check(c.firm_finance("BAKERY").outstanding_wages==0,"bakery wages unpaid");
            check(c.firm_finance("WHEAT").outstanding_wages==0,"wheat wages unpaid");
        }
        if(day==60)
            check(c.firm_finance("BAKERY").expenses>0&&c.firm_finance("WEAVER").receipts>100,
                  "linked market failed to pay wages or sell clothes after the opening fabric");
        prev_sales=sales;
    }
}

int main() {
    for(const auto trial:{StapleTrial::viable,StapleTrial::break_even,StapleTrial::supply_interruption})
        linked_staples_keep_running_across_payroll_rounding(trial);
    plain_clothing_has_a_bounded_fabric_fallback();
    monthly_clothing_cap_is_shared_across_household_slices();
    copper_mine_supplies_paid_telephone_sales(false);
    copper_mine_supplies_paid_telephone_sales(true);
    for(const auto trial:{DividendTrial::enabled,DividendTrial::disabled,
                          DividendTrial::no_title,DividendTrial::no_income,
                          DividendTrial::indebted,DividendTrial::forgiven,
                          DividendTrial::reserve})
        registered_household_dividend(trial);
    staple_food_substitutes_share_one_need();
    substitute_food_sales_redirect_next_day_production();
    grocery_shortage_cannot_cancel_a_different_goods_tariff();
    real_food_and_textile_sales(false);
    real_food_and_textile_sales(true);
    real_food_and_textile_sales(true,true);
    priority_and_class_demand();
    endogenous_hiring_production_and_payroll();
    intermediate_goods_are_purchased_not_created();
    greenfield_from_real_cash();
    greenfield_uses_yesterdays_solvent_demand(true,true);
    greenfield_uses_yesterdays_solvent_demand(false,true);
    greenfield_uses_yesterdays_solvent_demand(true,false);
}
