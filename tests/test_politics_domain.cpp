#include "vic25/daily_snapshot.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

using namespace vic25;
using namespace vic25::phasec;
static void check(bool yes,const char* reason) {
    if(!yes)throw std::runtime_error(reason);
}
static phaseb::DemographyRules isolated() {
    phaseb::DemographyRules r;
    r.fertility_ppm=0;r.infant_death_ppm=0;r.child_death_ppm=0;
    r.adult_death_ppm=0;r.senior_death_ppm=0;r.very_old_death_ppm=0;
    r.illness_incidence_ppm=0;r.illness_recovery_ppm=0;
    r.accident_ppm=0;r.injury_recovery_ppm=0;
    r.primary_access_ppm=0;r.secondary_access_ppm=0;
    r.tertiary_access_ppm=0;r.adult_literacy_access_ppm=0;
    r.training_ppm=0;r.migration_ppm=0;
    return r;
}
static phaseb::World people(std::int64_t age_months) {
    phaseb::World w{24000};
    auto hh=w.household_profile({"R","shared","shared",100});
    auto econ=w.economic_profile({"R","not_assigned","inactive","ineligible",
                                   w.default_education()});
    auto dem=w.demographic_profile({
        "R",24000-age_months,phaseb::Sex::female,"c","r","citizen","free",
        w.default_health(),phaseb::Service::civilian});
    w.seed_population(dem,econ,hh,100);
    return w;
}
static phaseb::People schooling(const phaseb::World& w) {
    phaseb::People n=0;
    for(const auto& [id,row]:w.slices()) {
        (void)id;
        if(w.education(w.economic(row.key.economic).education).school!=
           phaseb::School::none)n+=row.count;
    }
    return n;
}
static void native_rate_callback_changes_real_pop_only() {
    auto w=people(8*12);
    auto control=w;
    auto no=isolated();
    phaseb::Engine engine(no);
    (void)engine.advance_day(control,1,31);
    check(schooling(control)==0&&control.population()==100,
          "zero school hazard changed a real POP");
    (void)engine.advance_day(w,1,31,
        [](const phaseb::World&,phaseb::Id,
           const char* metric,phaseb::People rate) {
            return std::string(metric)=="school"?phaseb::People{1'000'000}:rate;
        });
    check(schooling(w)==100&&w.population()==100,
          "actual Phase B education was not adjusted by daily law callback");

    auto mothers=people(30*12);
    const auto first=mothers.population();
    (void)engine.advance_day(mothers,1,31,
        [](const phaseb::World&,phaseb::Id,
           const char* metric,phaseb::People rate) {
            return std::string(metric)=="fertility"?phaseb::People{1'000'000}:rate;
        });
    check(mothers.population()>first,
          "actual Phase B birth hazard did not generate real babies");
    check(mothers.validate(),"daily political birth event broke POP accounting");

    auto migrants=people(30*12);
    auto migrules=isolated();
    migrules.destination_by_region["R"]="R2";
    phaseb::Engine move(migrules);
    (void)move.advance_day(migrants,1,31,
        [](const phaseb::World&,phaseb::Id,
           const char* metric,phaseb::People rate) {
            return std::string(metric)=="migration"?phaseb::People{1'000'000}:rate;
        });
    phaseb::People moved=0;
    for(const auto& [id,row]:migrants.slices()) {
        (void)id;
        if(migrants.demographic(row.key.demographic).region=="R2")
            moved+=row.count;
    }
    check(moved==100&&migrants.population()==100,
          "per-slice migration hazard invented or lost actual population");
}
static daily::World country_school(const std::string& school_law) {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.set_dataset("political-school-actual-POP");
    geo.seed_province(1,"R","A");
    geo.seed_unit(1,"A","DISTRICT");
    geo.seed_assignment(1,1);
    auto& c=geo.seed_core().seed_core();
    c.seed_actor("A",200);
    c.seed_government("A","HEAD","PRIME",5,3);
    c.seed_p1_constitution("A",{
        P1GovernancePrinciple::monarchy,
        P1PowerDistribution::autocracy,
        P1LegislatureKind::none,"test:law"});
    P2BaseLaw law;
    law.id="A:education";law.country="A";
    law.domain="lawgroup_education_system";law.scope="national";
    law.base_choice="law_"+school_law;law.legal_basis="test:law";
    c.p2_seed_base(law);
    auto& pop=c.seed_population();
    auto hh=pop.household_profile({"R","shared","shared",100});
    auto econ=pop.economic_profile({"R","not_assigned","inactive","ineligible",
                                      pop.default_education()});
    auto dem=pop.demographic_profile({
        "R",24000-8*12,phaseb::Sex::female,"c","r","citizen","free",
        pop.default_health(),phaseb::Service::civilian});
    pop.seed_population(dem,econ,hh,1000);
    auto rules=isolated();
    rules.primary_access_ppm=1'000'000;
    c.enable_demography(rules);
    gh.validate();
    return daily::World::adopt(std::move(gh),2);
}
static void enacted_law_changes_only_its_own_real_daily_schooling() {
    auto restricted=country_school("no_schools");
    auto permitted=country_school("public_schools");
    for(int day=1;day<=31;++day){
        restricted.advance_day();
        permitted.advance_day();
    }
    const auto& a=restricted.engine().base().core().core();
    const auto& b=permitted.engine().base().core().core();
    check(a.people().population()==1000&&b.people().population()==1000,
          "education law generated population");
    check(schooling(a.people())==0,
          "country no_schools law failed to prohibit real schooling");
    check(schooling(b.people())>0,
          "public_schools law failed to permit real schooling");
    const auto saved=daily::SnapshotCodec::encode(permitted);
    const auto copy=daily::SnapshotCodec::decode(saved);
    check(daily::SnapshotCodec::encode(copy)==saved,
          "lawful day-native school demographics did not save and replay");
}
int main() {
    try {
        native_rate_callback_changes_real_pop_only();
        enacted_law_changes_only_its_own_real_daily_schooling();
        std::cout<<"VIC2.5 POLITICS REAL POP DOMAIN EFFECTS PASS\n";
    }catch(const std::exception& e) {
        std::cerr<<"POLITICS DOMAIN FAILURE: "<<e.what()<<"\n";
        return 1;
    }
}
