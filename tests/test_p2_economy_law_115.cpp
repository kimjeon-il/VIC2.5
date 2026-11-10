#ifdef NDEBUG
#undef NDEBUG
#endif
#include "vic25/phase_d.hpp"
#include <cassert>
#include <stdexcept>

using namespace vic25;
using namespace vic25::phasec;

int main() {
    // Exact cumulative rounding: no lost tenth output when batches are 1.
    assert(phased::law_verified_yield(0,1,0.1)==1);
    assert(phased::law_verified_yield(9,1,0.1)==2);
    assert(phased::law_verified_yield(0,10,0.1)==11);
    assert(phased::law_verified_yield(5,3,0.0)==3);

    IntegratedWorld c{24000};
    c.seed_region("R","A");
    c.seed_actor("A",1000);
    c.seed_actor("FIRM",100);
    c.seed_actor("HH:100",100);
    c.seed_production_facility("FARM","FIRM","R",10);
    c.seed_market("R","grain",10,1,20,10);
    c.seed_stock("FIRM","R","grain",10);
    c.seed_government("A","HEAD","PM",5,3);
    c.seed_p1_constitution("A",{
        P1GovernancePrinciple::monarchy,
        P1PowerDistribution::autocracy,
        P1LegislatureKind::none,"constitution:opening"});
    P2BaseLaw econ;
    econ.id="ECONOMY";econ.country="A";econ.domain="economic_system";
    econ.scope="national";econ.base_choice="law_industry_banned";
    econ.legal_basis="fixture:industry";c.p2_seed_base(econ);

    phased::DomainState ds;
    ds.industry_classes.emplace("agriculture",
        phased::IndustryClass{"agriculture","","farm"});
    phased::IndustryRecipe r;
    r.id="GRAIN";r.site="FARM";r.industry_class="agriculture";
    r.inputs={{"seed",1}};r.outputs={{"grain",1}};
    r.workers_per_batch=1;r.capacity_per_batch=1;
    ds.recipes.emplace(r.id,r);
    assert(phased::law_industry_throughput(ds,c,r)>0.09);

    // A real factory purchase must conserve bank deposits and change
    // only the existing facility owner. No constructed asset is minted.
    const auto total=c.cash("A")+c.cash("FIRM")+c.cash("HH:100");
    IntegratedWorld::Plan plan;
    plan[7].push_back([](IntegratedWorld& w) {
        w.phase_d_transfer_factory_paid("FARM","A",250,0);
    });
    (void)c.advance_daily(plan,false);
    assert(c.facility("FARM").owner=="A");
    assert(c.cash("A")==750);
    assert(c.cash("FIRM")==350);
    assert(c.cash("A")+c.cash("FIRM")+c.cash("HH:100")==total);

    IntegratedWorld consumer{24000};
    consumer.seed_region("R","A");
    consumer.seed_actor("A",0);
    consumer.seed_actor("MERCHANT",0);
    consumer.seed_actor("HH:1",100);
    consumer.seed_market("R","food",10,1,20,10);
    consumer.seed_stock("MERCHANT","R","food",10);
    consumer.seed_government("A","HEAD","PM",5,3);
    P2BaseLaw taxation;
    taxation.id="TAX";taxation.country="A";taxation.domain="taxation";
    taxation.scope="national";
    taxation.base_choice="law_consumption_based_taxation";
    taxation.legal_basis="fixture:consumption-tax";
    consumer.p2_seed_base(taxation);
    assert(consumer.p2_taxation_choice("A")==
           "law_consumption_based_taxation");
    IntegratedWorld::Plan sale;
    sale[7].push_back([](IntegratedWorld& w) {
        const auto bought=w.clear_consumer_market(
            "MERCHANT","HH:1","R","food",10);
        assert(bought==9);
    });
    (void)consumer.advance_daily(sale,false);
    assert(consumer.cash("A")==9);
    assert(consumer.cash("MERCHANT")==90);
    assert(consumer.cash("HH:1")==1);
    assert(consumer.stock("MERCHANT","R","food")==1);
    assert(consumer.cash("A")+consumer.cash("MERCHANT")+
           consumer.cash("HH:1")==100);
}
