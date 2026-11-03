#ifdef NDEBUG
#undef NDEBUG
#endif
#include "vic25/phase_c.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace vic25::phasec;
using vic25::phaseb::Sex;
using vic25::phaseb::Service;
using vic25::phaseb::Id;
using vic25::phaseb::People;
using vic25::phaseb::EventKind;

static void require(bool b,const char* msg){if(!b)throw std::runtime_error(msg);}

struct CrisisScenario {
    IntegratedWorld w{6000};
    Id female{}, reserve{}, active{};
    CrisisScenario(){
        w.seed_actor("F",20000);
        w.seed_actor("H",2000);
        w.seed_actor("G",1000);
        w.seed_actor("S",0);
        w.seed_region("R","A");
        w.seed_region("X","B");
        w.seed_government("A","Head","Minister",3,2);
        w.seed_unit("U");
        w.seed_stock("S","R","ore",100);
        w.seed_stock("S","R","food",5);
        w.seed_stock("F","R","building_materials",4);
        w.seed_stock("U","R","ammunition",10);
        w.seed_market("R","food",4,2,10,1);
        w.seed_production_facility("mill","F","R",4);
        auto& pop=w.seed_population();
        auto h=pop.household_profile({"R","farming-workers","shared",200});
        auto e=pop.economic_profile({"R","worker","wage","employee",pop.default_education()});
        auto child=pop.economic_profile({"R","not_assigned","inactive","ineligible",pop.default_education()});
        auto seed=[&](Sex sex,Service service,int age,People n,Id econ){
            auto demo=pop.demographic_profile({"R",6000-age*12,sex,"c","r","citizen",
                "free",pop.default_health(),service});
            return pop.seed_population(demo,econ,h,n);
        };
        female=seed(Sex::female,Service::civilian,25,120,e);
        seed(Sex::male,Service::civilian,25,80,e);
        reserve=seed(Sex::male,Service::reserve,25,30,e);
        active=seed(Sex::male,Service::active,25,15,e);
        seed(Sex::female,Service::civilian,6,40,child);
        seed(Sex::male,Service::civilian,6,40,child);
        require(w.people().population()==325,"initial headcount mismatch");
        w.seed_active("U",active);
        vic25::phaseb::DemographyRules rules;
        rules.fertility_ppm=4000;
        rules.adult_death_ppm=100;
        rules.child_death_ppm=50;
        rules.infant_death_ppm=100;
        rules.senior_death_ppm=1500;
        rules.very_old_death_ppm=6000;
        rules.illness_incidence_ppm=0;
        rules.illness_recovery_ppm=10000;
        rules.accident_ppm=0;
        rules.injury_recovery_ppm=1000;
        rules.primary_access_ppm=700000;
        rules.secondary_access_ppm=350000;
        rules.tertiary_access_ppm=100000;
        rules.training_ppm=25000;
        w.enable_demography(rules);
        w.validate();
    }
};

static void perform(CrisisScenario& sc){
    for(int m=0;m<1200;m++){
        IntegratedWorld::Plan p;
        bool produced=false;
        if(m==0)p[2].push_back([&](auto& x){x.hire(sc.female,10,"F",2);});
        p[3].push_back([](auto& x){
            bool hired=false;
            for(const auto& [id,job]:x.people().jobs()){
                (void)id;
                if(job.employer=="F"&&job.count>0)hired=true;
            }
            if(hired&&x.stock("S","R","ore")>0&&x.cash("F")>=1)
                x.trade("S","F","R","ore",1,1);
        });
        p[4].push_back([&](auto& x){
            bool hired=false;
            for(const auto& [id,job]:x.people().jobs()){
                (void)id;
                if(job.employer=="F"&&job.count>0)hired=true;
            }
            if(hired&&x.stock("F","R","ore")>0){
                x.produce("F","R","ore","tool",1,1);
                produced=true;
            }
            if(m==25)x.rebuild_facility("mill","building_materials",4,4);
        });
        p[5].push_back([&](auto& x){
            if(produced)x.wages("F","H","G",5,1);
            if(m==37)x.pay_reparation("peace","F",20);
        });
        p[7].push_back([&](auto& x){
            if(produced){
                x.trade("F","H","R","tool",1,6);
                x.consume("H","R","tool",1);
            }
            if(m==0)
                require(x.clear_consumer_market("S","H","R","food",10)==5,
                        "famine must have real shortages");
        });
        if(m==20)p[1].push_back([](auto& x){x.lend("F",50);});
        if(m==20)p[8].push_back([](auto& x){x.forgive_defaulted_loan("F",30);});
        p[9].push_back([&](auto& x){
            if(m%48==0){
                auto voters=x.eligible_voters();
                auto registered=std::min<People>(voters,100);
                if(registered>0) {
                    auto votes_A=registered*3/5;
                    x.hold_election(registered,0,3,
                                    {{"A",votes_A},{"B",registered-votes_A}});
                    if(m==0)x.appoint_cabinet("A","Head","Coalition",{"A"});
                }
            }
            if(m==36)x.sign_reparation_treaty("peace","F","H",100);
        });
        p[10].push_back([&](auto& x){
            if(m==0)x.battle("U",sc.active,"R",CombatLoss{1,2,0,0,0,1});
            if(m==12)x.epidemic(sc.female,8,2);
            if(m==24)x.war_damage_facility("mill",4);
            if(m==100)x.mobilize(sc.reserve,5,"U");
            if(m==200)x.occupy("X","A");
        });
        const auto report=sc.w.advance(p);
        require(report.finished_month==6001+m,"month sequence");
        require(report.population>0&&sc.w.cash("F")>=0&&sc.w.cash("H")>=0,
                "negative headcount or cash");
        sc.w.validate();
    }
    People born=0,dead=0,illness=0;
    for(const auto& e:sc.w.people().events()){
        if(e.kind==EventKind::birth)born+=e.count;
        if(e.kind==EventKind::death)dead+=e.count;
        if(e.kind==EventKind::illness)illness+=e.count;
    }
    require(born>0&&dead>=3&&illness>=8,"birth/death/epidemic shocks not processed");
    require(sc.w.people().population()==325+born-dead,
            "entire 100-year headcount not conserved");
    require(sc.w.bank_insolvent()&&sc.w.bank_losses()==30,
            "credit shock not preserved");
    require(sc.w.military_control("X")=="A"&&sc.w.sovereignty("X")=="B",
            "military occupation violated sovereignty");
    require(sc.w.reparations_outstanding("peace")==80,
            "war treaty was not partially settled");
    require(sc.w.facility("mill").damaged==0,
            "material-backed rebuilding was not completed");
    require(sc.w.market("R","food").price>4 &&
            sc.w.food_grievance("R")==5,"crop failure did not affect market");
    require(sc.w.government("A").prime_minister=="Coalition",
            "constitutional cabinet was lost");
    require(sc.w.commit_id()==1200,"1200 full commits missing");
}

int main(){
    auto start=std::chrono::steady_clock::now();
    CrisisScenario a,b;
    perform(a);perform(b);
    require(a.w.people().fingerprint()==b.w.people().fingerprint(),
            "population/demography differs on repeated independent 1200-month run");
    require(a.w.journal()==b.w.journal(),"cross-domain journal replay not deterministic");
    for(auto actor:{"F","H","G","S"})
        require(a.w.cash(actor)==b.w.cash(actor),"financial outcomes diverge");
    require(a.w.active_strength("U")==b.w.active_strength("U"),
            "military assignments diverge");
    const auto duration=std::chrono::duration<double>(
        std::chrono::steady_clock::now()-start).count();
    std::cout<<"D4-T46 full-scope representative synthetic 1200 month ×2 regression PASS"
             <<" total_people="<<a.w.people().population()
             <<" active_slices="<<a.w.people().slices().size()
             <<" active_units="<<a.w.active_strength("U")
             <<" committed_months="<<a.w.commit_id()
             <<" journal_events="<<a.w.journal().size()
             <<" seconds_both="<<duration<<"\n";
    return 0;
}
