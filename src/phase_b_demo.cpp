#include "vic25/phase_b.hpp"
#include <iostream>
using namespace vic25::phaseb;

int main(){
    World world{24000};
    const Id families=world.household_profile({"R1","mixed","shared",300});
    const Id agriculture=world.economic_profile({"R1","farmer","subsistence","self_employed",world.default_education()});
    const Id children=world.economic_profile({"R1","not_assigned","inactive","ineligible",world.default_education()});
    const Id adults_f=world.demographic_profile({"R1",23700,Sex::female,"C","R","N","citizen",world.default_health(),Service::civilian});
    const Id adults_m=world.demographic_profile({"R1",23700,Sex::male,"C","R","N","citizen",world.default_health(),Service::civilian});
    const Id minors_f=world.demographic_profile({"R1",23880,Sex::female,"C","R","N","citizen",world.default_health(),Service::civilian});
    const Id minors_m=world.demographic_profile({"R1",23880,Sex::male,"C","R","N","citizen",world.default_health(),Service::civilian});
    world.seed_population(adults_f,agriculture,families,500);
    world.seed_population(adults_m,agriculture,families,500);
    world.seed_population(minors_f,children,families,300);
    world.seed_population(minors_m,children,families,300);
    const People starting_people=world.population();
    DemographyRules assumptions;
    assumptions.fertility_ppm=4'000;  // synthetic validation inputs, NOT historical calibration.
    assumptions.monthly_vacancies={{"public_workshop","farmer",5,30,JobRequirements{}}};
    Engine engine{assumptions};
    People births=0,deaths=0;
    for(int month=0;month<120;month++){
        const auto report=engine.advance(world);
        births+=report.births;deaths+=report.deaths;
    }
    const auto labor=world.labor();
    const auto household=world.household_report(families);
    std::cout << "VIC2.5 Phase B — synthetic 10-year population/health/household/labor run\n"
              << "start_population=" << starting_people << " end_population=" << world.population()
              << " births=" << births << " deaths=" << deaths << '\n'
              << "employed=" << labor.employed << " self_employed=" << labor.self_employed
              << " household_food_need=" << household.food_need
              << " active_population_slices=" << world.slices().size() << '\n'
              << "NOTE: Housing and consumption are estimated needs; there is no product trade/financial settlement in Phase B.\n";
}
