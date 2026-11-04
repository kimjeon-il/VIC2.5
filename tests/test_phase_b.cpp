#ifdef NDEBUG
#undef NDEBUG // Unit-test asserts remain active even in Release.
#endif
#include "vic25/phase_b.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
using namespace vic25::phaseb;

static bool fails(auto f){try{f();}catch(const std::exception&){return true;}return false;}
struct F {
 World w{24000};
 Id h{},eh{},ch{},ad{},cm{},female{},male{},child{},s1{},s2{},s3{};
 F(){
  h=w.household_profile({"R1","family","shared",2});
  eh=w.economic_profile({"R1","farmer","subsistence","self_employed",w.default_education()});
  ch=w.economic_profile({"R1","not_assigned","inactive","ineligible",w.default_education()});
  female=w.demographic_profile({"R1",23700,Sex::female,"C","T","N","citizen",w.default_health(),Service::civilian});
  male=w.demographic_profile({"R1",23700,Sex::male,"C","T","N","citizen",w.default_health(),Service::civilian});
  child=w.demographic_profile({"R1",23940,Sex::female,"C","T","N","citizen",w.default_health(),Service::civilian});
  s1=w.seed_population(female,eh,h,2); s2=w.seed_population(male,eh,h,1); s3=w.seed_population(child,ch,h,3);
 }
};
int main(){
 F f; auto& w=f.w;
 assert(w.population()==6 && w.group_population(0,f.eh)==3 && w.group_population(0,0,f.h)==6);
 assert(w.validate());
 assert(w.labor().self_employed==3);
 assert(w.household_report(f.h).children==3);
 assert(fails([&]{w.hire(f.s3,1,"bad","farmer",20,{}); }));
 JobRequirements job;job.minimum_school=School::none; job.minimum_skill=0;
 w.hire(f.s1,1,"farm-a","farmer",100,job); assert(w.employed(f.s1)==1);
 assert(w.household_report(f.h).wage_income==100);
 assert(fails([&]{w.transfer(f.s1,1,f.male,f.eh,f.h,EventKind::occupation); }));
 auto saved=w.population();
 auto hp=w.health(w.demographic(f.female).health);hp.ill=true;
 Id ill=w.change_health(f.s1,1,hp,EventKind::illness,1);
 assert(w.employed(ill)==1 && w.employed(f.s1)==0 && w.population()==saved);
 auto hp2=hp;hp2.temporary_injury=true;hp2.limited_capabilities=1;
 Id injured=w.change_health(ill,1,hp2,EventKind::injury,1);
 assert(w.population()==saved);
 JobRequirements physical;physical.required_capabilities=1;
 assert(!w.eligible_for(injured,physical));
 physical.accommodation_mask=1;assert(w.eligible_for(injured,physical));
 assert(fails([&]{w.death(f.s3,4); }));assert(w.population()==saved);
 w.death(f.s3,1);assert(w.population()==5);
 // No automatic job created in newborn's nonworker POP.
 Demographic d=w.demographic(f.female);
 w.birth(f.h,d,f.ch,Sex::male,2);
 assert(w.population()==7);
 // Transactional monthly rollback even after staged event modifications.
 const auto before=w.fingerprint(); const auto month=w.month(); const auto ecount=w.events().size();
 Engine engine;
 assert(fails([&]{engine.advance(w,[](vic25::MonthStage s){if(s==vic25::MonthStage::repricing_and_validation)throw std::runtime_error("fault");});}));
 assert(w.fingerprint()==before&&w.month()==month&&w.events().size()==ecount);
 // Full deterministic age/health/migration/education simulation with isolated economy.
 auto replica=w;
 People total_births=0,total_deaths=0;
 for(int i=0;i<1200;i++){
   const auto a=engine.advance(w),b=engine.advance(replica);
   assert(a.end_population==a.start_population+a.births-a.deaths);
   assert(a.digest==b.digest && w.validate() && replica.validate());
   assert(a.active_slices==b.active_slices);
   total_births+=a.births;total_deaths+=a.deaths;
 }
 assert(w.month()==month+1200);
 assert(w.population()==7+total_births-total_deaths);
 std::cout<<"Phase B tests pass. Population "<<w.population()<<" at 1200 months, births "<<total_births<<" deaths "<<total_deaths<<" active "<<w.slices().size()<<"\n";
}
