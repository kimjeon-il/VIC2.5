#include "vic25/p0_daily.hpp"
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
    const auto firm="P0:FIRM:new-food:24000";
    const auto& after=game.world().engine().base().core();
    check(after.state().constructions.contains(id),"new project not entered");
    check(after.core().cash("INVESTOR")==400,"founder cash not debited");
    check(after.core().cash(firm)==100,"new firm has fictitious/missing cash");
    check(after.core().shares("company:"+firm,"INVESTOR")==100,
          "founder did not receive real shares");
    game.advance_day();
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
int main() {
    priority_and_class_demand();
    greenfield_from_real_cash();
}
