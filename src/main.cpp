#include "vic25/population.hpp"
#include "vic25/monthly.hpp"
#include <iostream>

int main() {
    using namespace vic25;
    Pop pop{"soldiers-01", {"region-01", "culture-01", "religion-01", "soldier", "employed", "mid", "citizen"}, {}};
    CohortKey cohort{-300, Sex::male, Health::well, 0, MilitaryStatus::active_service};
    add_people(pop, cohort, 1000);
    apply_battle_loss(pop, cohort, {50, 180, 70});
    MonthlyPipeline simulation;
    unsigned stages = 0;
    simulation.advance([&](MonthStage) { ++stages; });
    std::cout << "VIC2.5 S0-POP scaffold (not a full economic engine)\n"
              << "population_after_battle=" << pop.population() << "\n"
              << "monthly_stage_hooks=" << stages << "\n";
}
