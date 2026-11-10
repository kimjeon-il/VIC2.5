#ifdef NDEBUG
#undef NDEBUG
#endif
#include "vic25/phase_b.hpp"
#include "vic25/phase_c_p2_catalog_115.hpp"
#include <cassert>
#include <string_view>

using namespace vic25::phaseb;
using namespace vic25::phasec;

int main() {
    assert(p2_catalog_115().laws.size()==148);
    assert(p2_catalog_115().amendments.size()==71);
    const auto* protection=p2_catalog_law("law_worker_protections");
    assert(protection&&protection->group=="labor_rights");
    assert(p2_catalog_law("law_public_health_insurance")!=nullptr);
    assert(p2_catalog_raw_modifier("law_autocracy",
                                  "country_authority_add")==200);
    assert(p2_catalog_raw_modifier("law_isolationism",
                                  "country_disallow_trade_bool")==1);

    World seed{24000};
    const auto h=seed.household_profile({"R","family","shared",100});
    const auto e=seed.economic_profile({"R","farmer","subsistence",
                                         "self_employed",
                                         seed.default_education()});
    const auto d=seed.demographic_profile({"R",23700,Sex::female,
        "culture","religion","citizen","free",seed.default_health(),
        Service::civilian});
    seed.seed_population(d,e,h,120'000);
    DemographyRules rules;
    rules.fertility_ppm=300'000;
    rules.adult_death_ppm=150'000;
    rules.infant_death_ppm=0;
    rules.child_death_ppm=0;
    rules.senior_death_ppm=0;
    rules.very_old_death_ppm=0;
    rules.illness_incidence_ppm=0;
    rules.illness_recovery_ppm=0;
    rules.accident_ppm=0;
    rules.injury_recovery_ppm=0;
    rules.primary_access_ppm=0;
    rules.secondary_access_ppm=0;
    rules.tertiary_access_ppm=0;
    rules.adult_literacy_access_ppm=0;
    rules.training_ppm=0;
    Engine engine{rules};
    auto baseline=seed, high_birth=seed, safer=seed;
    const auto ordinary=engine.advance_day(baseline,1,30);
    const auto boosted=engine.advance_day(high_birth,1,30,
        [](const std::string& region,std::string_view key) {
            assert(region=="R");
            return key=="state_birth_rate_mult"?0.5:0.0;
        });
    const auto protected_people=engine.advance_day(safer,1,30,
        [](const std::string& region,std::string_view key) {
            assert(region=="R");
            return key=="state_mortality_mult"?-0.5:0.0;
        });
    assert(boosted.births>ordinary.births);
    assert(protected_people.deaths<ordinary.deaths);
    assert(baseline.validate()&&high_birth.validate()&&safer.validate());
    assert(high_birth.population()==seed.population()+
           boosted.births-boosted.deaths);
    assert(safer.population()==seed.population()+
           protected_people.births-protected_people.deaths);
    // The law changes actual demographic outcomes, not metadata.
}
