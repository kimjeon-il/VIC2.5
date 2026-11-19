#ifdef NDEBUG
#undef NDEBUG
#endif
#include "vic25/phase_d.hpp"
#include <cassert>
#include <string>

using namespace vic25;
using namespace vic25::phasec;

static void seed_economy(IntegratedWorld& c,const char* choice) {
    P2BaseLaw law;
    law.id="ACT";law.country="A";law.domain="economic_system";
    law.scope="national";law.base_choice=choice;
    law.legal_basis="fixture:existing-1.15-economy";
    c.p2_seed_base(law);
}

static void actual_aristocratic_contribution() {
    IntegratedWorld c{24000};
    c.seed_region("R","A");
    c.seed_actor("A",0);
    c.seed_actor("FIRM",100);
    auto& people=c.seed_population();
    const auto h=people.household_profile({"R","family","shared",10});
    const auto e=people.economic_profile({"R","aristocrats",
        "self_employed","self_employed",people.default_education()});
    const auto d=people.demographic_profile({"R",23700,
        phaseb::Sex::female,"C","R","A","citizen",
        people.default_health(),phaseb::Service::civilian});
    people.seed_population(d,e,h,10);
    const auto household="HH:"+std::to_string(h);
    c.seed_actor(household,100);
    c.seed_shares("company:FIRM",household,100);
    c.seed_government("A","HEAD","PM",5,3);
    seed_economy(c,"law_industry_banned");
    assert(c.p2_region_modifier_value(
        "R","state_aristocrats_investment_pool_contribution_add")==0.25);
    IntegratedWorld::Plan plan;
    plan[7].push_back([](IntegratedWorld& w) {
        assert(w.p2_invest_from_aristocratic_shareholders("FIRM")==25);
    });
    c.advance_daily(plan,false);
    assert(c.cash("FIRM")==125&&c.cash(household)==75);
    assert(c.shares("company:FIRM",household)==100);
}

static void compensation_requirement() {
    auto build=[](const char* choice) {
        IntegratedWorld c{24000};
        c.seed_region("R","A");
        c.seed_actor("A",0);
        c.seed_actor("FIRM",100);
        c.seed_production_facility("MILL","FIRM","R",10);
        c.seed_government("A","HEAD","PM",5,3);
        seed_economy(c,choice);
        return c;
    };
    auto traditional=build("law_traditionalism");
    IntegratedWorld::Plan forbidden;
    forbidden[7].push_back([](IntegratedWorld& w) {
        w.phase_d_transfer_factory_uncompensated("MILL","A",100);
    });
    bool rejected=false;
    try{traditional.advance_daily(forbidden,false);}
    catch(const std::exception&){rejected=true;}
    assert(rejected&&traditional.facility("MILL").owner=="FIRM");
    assert(traditional.cash("A")==0&&traditional.cash("FIRM")==100);
    auto free_market=build("law_laissez_faire");
    rejected=false;
    try{free_market.advance_daily(forbidden,false);}
    catch(const std::exception&){rejected=true;}
    assert(rejected&&free_market.facility("MILL").owner=="FIRM");
    auto command=build("law_command_economy");
    command.advance_daily(forbidden,false);
    assert(command.facility("MILL").owner=="A");
    assert(command.cash("A")==0&&command.cash("FIRM")==100);
}

static void real_cooperative_share_purchase() {
    IntegratedWorld c{24000};
    c.seed_region("R","A");
    c.seed_actor("A",0);
    c.seed_actor("FIRM",200);
    c.seed_actor("OWNER",100);
    c.seed_production_facility("FACTORY","FIRM","R",20);
    auto& people=c.seed_population();
    const auto h=people.household_profile({"R","family","shared",1});
    const auto e=people.economic_profile({"R","worker","wage",
        "employee",people.default_education()});
    const auto d=people.demographic_profile({"R",23700,
        phaseb::Sex::male,"C","R","A","citizen",
        people.default_health(),phaseb::Service::civilian});
    const auto slice=people.seed_population(d,e,h,1);
    people.hire(slice,1,"FIRM","worker",20,{});
    const auto worker="HH:"+std::to_string(h);
    c.seed_actor(worker,10);
    c.seed_shares("company:FIRM","OWNER",5);
    c.seed_government("A","HEAD","PM",5,3);
    seed_economy(c,"law_cooperative_ownership");
    phased::DomainState domain;
    phased::IndustryRecipe recipe;
    recipe.id="TEXTILES";recipe.site="FACTORY";
    recipe.industry_class="manufacturing";
    recipe.inputs={{"cotton",1}};
    recipe.outputs={{"cloth",1}};
    recipe.workers_per_batch=1;
    domain.recipes.emplace(recipe.id,recipe);
    IntegratedWorld::Plan plan;
    plan[7].push_back([&](IntegratedWorld& w) {
        phased::Stage d(w,domain,7);
        assert(d.purchase_collective_shares(
            "FACTORY","OWNER",worker,1));
    });
    c.advance_daily(plan,false);
    assert(c.shares("company:FIRM","OWNER")==4);
    assert(c.shares("company:FIRM",worker)==1);
    assert(c.cash(worker)==9&&c.cash("OWNER")==101);
    assert(c.cash("A")+c.cash("FIRM")+c.cash("OWNER")+
           c.cash(worker)==310);
}
int main() {
    actual_aristocratic_contribution();
    compensation_requirement();
    real_cooperative_share_purchase();
}
