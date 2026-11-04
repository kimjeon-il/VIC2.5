#ifdef NDEBUG
#undef NDEBUG // Unit-test asserts remain active even in Release.
#endif
#include "vic25/phase_b.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
using namespace vic25::phaseb;

template<class F> bool fails(F fn){try{fn();}catch(const std::exception&){return true;}return false;}
static DemographyRules no_change(){
 DemographyRules r;
 r.fertility_ppm=0;r.infant_death_ppm=0;r.child_death_ppm=0;
 r.adult_death_ppm=0;r.senior_death_ppm=0;r.very_old_death_ppm=0;
 r.illness_incidence_ppm=0;r.illness_recovery_ppm=0;r.accident_ppm=0;r.injury_recovery_ppm=0;
 r.primary_access_ppm=0;r.secondary_access_ppm=0;r.tertiary_access_ppm=0;
 r.adult_literacy_access_ppm=0;r.training_ppm=0;return r;
}

int main(){
 { // POP-D0/D1: same adults can have different job ownership allocation partitions.
  World w{24000};auto h=w.household_profile({"R1","extended","share",3});
  auto ed=w.education_profile({School::secondary,90,40});
  auto econ=w.economic_profile({"R1","technician","wage","employee",ed});
  auto demo=w.demographic_profile({"R1",23640,Sex::male,"C","R","N","citizen",w.default_health(),Service::civilian});
  auto slice=w.seed_population(demo,econ,h,10);
  JobRequirements qualified;qualified.minimum_school=School::secondary;qualified.minimum_skill=30;
  std::vector<Vacancy> jobs{{"factoryA","technician",4,200,qualified},{"factoryB","technician",3,120,qualified}};
  assert(w.match_jobs(jobs)==7);
  assert(w.population()==10 && w.validate());
  auto rep=w.labor();assert(rep.employed==7 && rep.unemployed==3);
  assert(w.household_report(h).wage_income==4*200+3*120);
  assert(w.jobs().size()==2);
  // A job specification with an unsupported limitation is rejected; an accommodation is allowed.
  auto hh=w.health(w.demographic(demo).health);
  hh.long_term_impairment=true;hh.limited_capabilities=1U;
  auto target=w.change_health(slice,1,hh,EventKind::injury,0);assert(w.population()==10);
  JobRequirements physical;physical.required_capabilities=1U;
  assert(!w.eligible_for(target,physical));physical.accommodation_mask=1U;
  assert(w.eligible_for(target,physical));
  // Identical event key is applied exactly once.
  auto old=w.fingerprint();
  w.apply_once("jobshock-1",[&](World& stage){ stage.release(slice,1); });
  assert(w.fingerprint()!=old);
  old=w.fingerprint();assert(fails([&]{w.apply_once("jobshock-1",[&](World& stage){stage.release(slice,1);});}));
  assert(w.fingerprint()==old);
  // Staged command rollback on over-withdraw, including previous staged changes.
  assert(fails([&]{w.apply_once("bad-1",[&](World& stage){
      stage.release(slice,1);stage.death(slice,100); });}));
  assert(w.fingerprint()==old && w.validate());
 }
 { // D2: military casualties, POW/missing, active-service exclusion from civilian labor.
  World w{24000};auto h=w.household_profile({"R1","military-family","shared",200});
  auto e=w.economic_profile({"R1","soldier","wage","employee",w.default_education()});
  auto d=w.demographic_profile({"R1",23700,Sex::male,"C","R","N","citizen",w.default_health(),Service::active});
  auto s=w.seed_population(d,e,h,1000);
  assert(w.labor().employed==0 && w.labor().inactive==1000);
  w.battle_losses(s,50,180,70,5,3);
  assert(w.population()==950 && w.validate());
  People healthy=0,acute=0,disabled=0,prisoners=0,missing=0;
  Id chronic_id=0;
  for(const auto& [id,sl]:w.slices()){
      const auto &dem=w.demographic(sl.key.demographic); const auto &hp=w.health(dem.health);
      if(dem.service==Service::pow)prisoners+=sl.count;
      else if(dem.service==Service::missing)missing+=sl.count;
      else if(hp.long_term_impairment){disabled+=sl.count;chronic_id=id;}
      else if(hp.temporary_injury)acute+=sl.count;
      else healthy+=sl.count;
  }
  assert(healthy==692&&acute==180&&disabled==70&&prisoners==5&&missing==3);
  assert(fails([&]{w.battle_losses(chronic_id,100,0,0);}));
  auto civilian=w.set_service(chronic_id,20,Service::civilian,0);
  JobRequirements physical;physical.required_capabilities=1;
  assert(!w.eligible_for(civilian,physical));
  assert(w.eligible_for(civilian,JobRequirements{}));
  assert(w.population()==950);
 }
 { // D2: migration, cultural/legal change, households. Population does not multiply by 3.
  World w{24000};auto a=w.household_profile({"R1","family","shared",2});
  auto b=w.household_profile({"R2","family","shared",0});
  auto e=w.economic_profile({"R1","farmer","subsistence","self_employed",w.default_education()});
  auto d=w.demographic_profile({"R1",23600,Sex::female,"C","R","N","citizen",w.default_health(),Service::civilian});
  auto s=w.seed_population(d,e,a,9);
  auto shifted=w.change_identity(s,3,"C2","R2","naturalized",0);
  assert(w.population()==9 && w.slice(shifted).count==3);
  auto moved=w.migrate(shifted,2,"R2",b,0);
  assert(w.population()==9 && w.demographic(w.slice(moved).key.demographic).region=="R2");
  auto here=w.household_profile({"R2","workers","shared",0});
  auto split=w.reassign_household(moved,1,here,0);assert(w.slice(split).count==1);
  assert(w.population()==9 && w.validate());
 }
 { // Phase B autonomous migration, age-in-place, school access by sex/policy.
  World w{24000}; auto h=w.household_profile({"R1","family","shared",50});
  auto e=w.economic_profile({"R1","not_assigned","inactive","ineligible",w.default_education()});
  auto d=w.demographic_profile({"R1",23904,Sex::female,"C","R","N","citizen",w.default_health(),Service::civilian});
  w.seed_population(d,e,h,200);
  auto rules=no_change();rules.primary_access_ppm=PPM;
  rules.education_access_multiplier_ppm={PPM,0,PPM,PPM};
  rules.migration_ppm=PPM;rules.destination_by_region["R1"]="R2";
  Engine engine{rules};auto report=engine.advance(w);
  assert(report.migrants==200 && report.end_population==200 && w.validate());
  People region2=0; for(const auto& [id,s]:w.slices()){
     (void)id;if(w.demographic(s.key.demographic).region=="R2")region2+=s.count;
  }
  assert(region2==200);
 }
 { // Exactly 100% primary schooling at 8th birthday, then next year secondary.
  World w{24000};auto h=w.household_profile({"R1","family","shared",100});
  auto e=w.economic_profile({"R1","not_assigned","inactive","ineligible",w.default_education()});
  auto d=w.demographic_profile({"R1",23904,Sex::female,"C","R","N","citizen",w.default_health(),Service::civilian});
  w.seed_population(d,e,h,1000);
  auto r=no_change();r.primary_access_ppm=PPM;r.secondary_access_ppm=PPM;
  Engine engine{r};auto report=engine.advance(w);
  assert(report.graduated==1000);
  People primary=0;for(const auto& [id,s]:w.slices()){
   (void)id;if(w.education(w.economic(s.key.economic).education).school==School::primary)primary+=s.count;
  }
  assert(primary==1000);
 }
 { // Existing injured worker loses a physical job; accommodations are explicit.
  World w{24000};auto h=w.household_profile({"R1","household","shared",5});
  auto e=w.economic_profile({"R1","miner","wage","employee",w.default_education()});
  auto d=w.demographic_profile({"R1",23700,Sex::female,"C","R","N","citizen",w.default_health(),Service::civilian});
  auto id=w.seed_population(d,e,h,10);
  JobRequirements physical;physical.required_capabilities=1U;
  w.hire(id,10,"mine","miner",50,physical);
  auto hp=w.health(w.demographic(d).health);hp.temporary_injury=true;hp.injury_severity=1;
  auto injured=w.change_health(id,4,hp,EventKind::injury,4);
  assert(w.population()==10 && w.employed(injured)==0 && w.employed(id)==6);
  assert(w.validate());
  // Hiring law may restrict a job; it is not a biological attribute of the sex.
  JobRequirements sex_restricted;sex_restricted.allowed_sexes_mask=0x02;
  assert(!w.eligible_for(id,sex_restricted));
  sex_restricted.allowed_sexes_mask=0x0F;
  assert(w.eligible_for(id,sex_restricted));
 }
 { // Childhood transition to wage-seeking adults and subsequent training.
  World w{24000};auto h=w.household_profile({"R1","family","shared",200});
  auto e=w.economic_profile({"R1","not_assigned","inactive","ineligible",w.default_education()});
  // 16th birthday is next month.
  auto d=w.demographic_profile({"R1",24000-(16*12-1),Sex::male,"C","R","N","citizen",w.default_health(),Service::civilian});
  w.seed_population(d,e,h,100);
  auto rules=no_change();rules.primary_access_ppm=PPM;
  Engine eng{rules};
  eng.advance(w);assert(w.population()==100);
  eng.advance(w);
  People new_workers=0;
  for(const auto& [id,s]:w.slices()){
     (void)id;if(w.economic(s.key.economic).profession=="unskilled_worker")new_workers+=s.count;
  }
  assert(new_workers==100 && w.labor().eligible==100);
  std::vector<Vacancy> offer{{"mill","unskilled_worker",40,50,JobRequirements{}}};
  assert(w.match_jobs(offer)==40);
  // 100% annual training: jump forward to the next birthday without other changes.
  auto x=w; auto training_rules=no_change();training_rules.training_ppm=PPM;
  Engine training{training_rules};
  People trained=0;
  for(int i=0;i<24;i++)trained+=training.advance(x).trained;
  assert(trained==40 && x.population()==100 && x.labor().employed==40);
 }
 std::cout<<"Phase B features pass (partition, combat, migration, health, policy, idempotence).\n";
}
