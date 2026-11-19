#include "vic25/p0_daily.hpp"
#include "vic25/daily_snapshot.hpp"
#include <stdexcept>
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
    c.seed_market("R","food",15,1,100,0);
    c.seed_market("R","ore",2,1,100,0);
    c.seed_market("R","steel",3,1,100,0);
    c.seed_stock("BUILDER","R","steel",3);
    c.seed_government("A","HEAD","PRIME",5,3);
    const auto slice=add_worker(gh);
    auto& pop=c.seed_population();
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
    check(day1.stock("FACTORY","R","ore")==1 &&
          day1.stock("MINE","R","ore")==4,
          "same-region input procurement created or lost ore");
    game.advance_day();
    const auto& day2=game.world().engine().base().core().core();
    check(!game.world().engine().base().core().state().actual_production_runs.empty(),
          "input bought yesterday could not enter real production");
    check(day2.stock("MINE","R","ore")<=4,
          "industrial materials were multiplied by procurement");
}
int main() {
    priority_and_class_demand();
    endogenous_hiring_production_and_payroll();
    intermediate_goods_are_purchased_not_created();
    greenfield_from_real_cash();
}
