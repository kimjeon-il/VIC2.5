#ifdef NDEBUG
#undef NDEBUG // Unit-test asserts remain active even in Release.
#endif
#include "vic25/phase_b.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
using namespace vic25::phaseb;

static World make_world(){
 World w{24000};
 auto h=w.household_profile({"R1","mixed","shared",500});
 auto farm=w.economic_profile({"R1","farmer","subsistence","self_employed",w.default_education()});
 auto child=w.economic_profile({"R1","not_assigned","inactive","ineligible",w.default_education()});
 const auto add=[&](std::int64_t birth,Sex sex,Id econ,People count){
    auto d=w.demographic_profile({"R1",birth,sex,"culture","religion","citizen","free",w.default_health(),Service::civilian});
    w.seed_population(d,econ,h,count);
 };
 add(23640,Sex::female,farm,500);add(23640,Sex::male,farm,500);
 add(23880,Sex::female,child,500);add(23880,Sex::male,child,500);
 add(23280,Sex::female,child,250);add(23280,Sex::male,child,250);
 assert(w.population()==2500);
 return w;
}
int main(){
 auto w=make_world();auto w2=w;
 DemographyRules r;
 r.fertility_ppm=3000; r.illness_incidence_ppm=80; r.accident_ppm=50;
 r.primary_access_ppm=900000; r.secondary_access_ppm=600000;
 Engine e{r};
 People births=0,deaths=0,injuries=0, ill=0,educated=0, migrants=0;
 const auto start=std::chrono::steady_clock::now();
 for(int i=0;i<1200;i++){
   const auto result=e.advance(w);
   births+=result.births;deaths+=result.deaths;injuries+=result.injuries;
   ill+=result.new_illness;educated+=result.graduated;migrants+=result.migrants;
   assert(w.validate());
 }
 const auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
 const auto start2=std::chrono::steady_clock::now();
 for(int i=0;i<1200;i++){
   const auto result=e.advance(w2);
   assert(result.end_population==result.start_population+result.births-result.deaths);
 }
 const auto elapsed2=std::chrono::duration<double>(std::chrono::steady_clock::now()-start2).count();
 assert(w.population()==2500+births-deaths && w.fingerprint()==w2.fingerprint());
 assert(w.population()>=0 && !w.events().empty());
 std::cout<<"POP-I1-4 1200-month simulation; initial=2500 final="<<w.population()
          <<" births="<<births<<" deaths="<<deaths<<" illness="<<ill<<" injuries="<<injuries
          <<" education="<<educated<<" migrants="<<migrants
          <<" active_slices="<<w.slices().size()<<" events="<<w.events().size()
          <<" time_first_s="<<elapsed<<" time_repeat_s="<<elapsed2<<"\n";
}
