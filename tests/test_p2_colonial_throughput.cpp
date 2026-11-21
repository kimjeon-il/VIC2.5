#ifdef NDEBUG
#undef NDEBUG
#endif
#include "vic25/phase_d.hpp"
#include <cassert>
#include <stdexcept>

using namespace vic25;
using namespace vic25::phasec;

static Quantity manufacture_under_colonial_law(
        const std::string& industry, const std::string& archetype,
        const std::string& law, int incorporation, Quantity base_output,
        const std::string& jurisdiction="national") {
    phased::World world{24000};
    auto& c=world.seed_core();
    c.seed_region("R","A");
    c.seed_actor("A",0);
    c.seed_actor("FACTORY",100);
    c.seed_actor("WALLET",0);
    c.seed_government("A","HEAD","PM",5,3);
    c.seed_p1_constitution("A",{
        P1GovernancePrinciple::monarchy,P1PowerDistribution::autocracy,
        P1LegislatureKind::none,"fixture:constitution"});
    if(!law.empty()) {
        P2BaseLaw rule;
        rule.id="COLONIZATION";rule.country="A";rule.domain="colonization";
        rule.scope="national";rule.base_choice=law;rule.legal_basis="fixture:colonization";
        if(jurisdiction=="foreign") {
            c.seed_actor("B",0);
            c.seed_region("R_B","B");
            c.seed_government("B","HEAD_B","PM_B",5,3);
            c.seed_p1_constitution("B",{
                P1GovernancePrinciple::monarchy,P1PowerDistribution::autocracy,
                P1LegislatureKind::none,"fixture:foreign_constitution"});
            rule.country="B";
        }
        if(jurisdiction=="custom"||jurisdiction=="local")
            rule.domain="custom_colonization";
        if(jurisdiction=="local") {
            rule.tier=P2NormTier::local_ordinance;
            rule.scope="R";
        }
        c.p2_seed_base(rule);
    }
    if(incorporation>=0)c.p1_set_incorporation("R",incorporation>0);
    c.seed_stock("FACTORY","R","grain",1);
    c.seed_market("R","grain",2,2,2);
    c.seed_market("R","groceries",7,7,7);
    auto& pop=c.seed_population();
    const auto h=pop.household_profile({"R","mixed","shared",1});
    const auto econ=pop.economic_profile({"R","worker","wage","employee",
                                         pop.default_education()});
    const auto demo=pop.demographic_profile({"R",23700,phaseb::Sex::female,
        "culture","faith","citizen","free",pop.default_health(),
        phaseb::Service::civilian});
    const auto worker=pop.seed_population(demo,econ,h,1);
    pop.hire(worker,1,"FACTORY","worker",31,phaseb::JobRequirements{});
    world.seed_standard_industries();
    if(!world.state().industry_classes.contains(industry))
        world.seed_industry_class({industry,"","fixture industry"});
    if(archetype.empty())c.seed_production_facility("PLANT","FACTORY","R",31);
    else world.seed_building_facility("PLANT","FACTORY","R",31,archetype);
    phased::IndustryRecipe recipe;
    recipe.id="PRODUCT";recipe.site="PLANT";recipe.industry_class=industry;
    recipe.inputs={{"grain",1}};recipe.outputs={{"groceries",base_output}};
    recipe.workers_per_batch=recipe.capacity_per_batch=1;
    if(archetype.empty())world.seed_recipe(recipe);
    else world.seed_building_recipe(recipe);
    phased::World::Plan commands;
    commands[4].push_back([](phased::Stage& stage) {
        stage.manufacture_daily("PRODUCT",1,1,31);
    });
    commands[5].push_back([](phased::Stage& stage) {
        stage.phase_c().wages_under_law("FACTORY","WALLET","A",1,"A");
    });
    (void)world.advance_daily(commands,true,false,1,31);
    const auto& ledger=world.core();
    assert(ledger.cash("FACTORY")==99 && ledger.cash("WALLET")==1);
    assert(ledger.stock("FACTORY","R","grain")==0);
    assert(world.state().actual_production_runs.back().batches==1);
    return ledger.stock("FACTORY","R","groceries");
}

static void colonial_exploitation_affects_only_verified_unincorporated_recipes() {
    const std::string exploitation="law_colonial_exploitation";
    const std::string factory="vic3_115.urban.food_industries";
    assert(manufacture_under_colonial_law("MANU",factory,exploitation,0,10)==9);
    assert(manufacture_under_colonial_law("MANU",factory,exploitation,1,10)==10);
    assert(manufacture_under_colonial_law("MANU",factory,exploitation,-1,10)==10);
    assert(manufacture_under_colonial_law("MANU",factory,
                                         "law_colonial_resettlement",0,10)==10);
    for(const auto* sector:{"plantations","rubber","logging","mining"})
        assert(manufacture_under_colonial_law(sector,"",exploitation,0,10)==12);
    for(const auto* scope:{"foreign","custom","local"}) {
        assert(manufacture_under_colonial_law("MANU",factory,exploitation,0,10,scope)==10);
        for(const auto* sector:{"plantations","rubber","logging","mining"})
            assert(manufacture_under_colonial_law(sector,"",exploitation,0,10,scope)==10);
    }
    for(const auto* sector:{"agriculture","AGRI","FORE","EXTR","extraction"})
        assert(manufacture_under_colonial_law(sector,"",exploitation,0,10)==10);
    assert(manufacture_under_colonial_law("manufacturing","",exploitation,0,1)==1);
}

int main() {
    colonial_exploitation_affects_only_verified_unincorporated_recipes();
}
