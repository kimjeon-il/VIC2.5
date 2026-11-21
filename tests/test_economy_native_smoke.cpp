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
int main() {
    priority_and_class_demand();
    endogenous_hiring_production_and_payroll();
    intermediate_goods_are_purchased_not_created();
    greenfield_from_real_cash();
    greenfield_uses_yesterdays_solvent_demand(true,true);
    greenfield_uses_yesterdays_solvent_demand(false,true);
    greenfield_uses_yesterdays_solvent_demand(true,false);
}
