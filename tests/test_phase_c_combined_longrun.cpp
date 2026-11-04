#ifdef NDEBUG
#undef NDEBUG
#endif
#include "vic25/phase_c.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace vic25::phasec;
using vic25::phaseb::Service;
using vic25::phaseb::Sex;
using vic25::phaseb::Id;

static void require(bool b,const char* msg) {if(!b)throw std::runtime_error(msg);}
struct Scenario {
    IntegratedWorld world{1000};
    Id civ{},young{},military{};
    Scenario(){
        world.seed_actor("F",20000);
        world.seed_actor("H",20000);
        world.seed_actor("G",1000);
        world.seed_actor("S",0);
        world.seed_actor("T",0);
        world.seed_region("R","A");
        world.seed_region("X","B");
        world.seed_unit("U");
        world.seed_government("A","Head","Original",3,2);
        world.seed_stock("S","R","ore",1200);
        world.seed_stock("S","R","food",5);
        world.seed_market("R","food",4,2,15,1);
        world.seed_stock("F","R","building_materials",4);
        world.seed_stock("U","R","ammunition",20);
        world.seed_production_facility("mill","F","R",10);
        world.seed_interest_group("Food Union");
        auto& p=world.seed_population();
        auto h=p.household_profile({"R","shared","shared",100});
        auto e=p.economic_profile({"R","worker","wage","employee",p.default_education()});
        auto d=p.demographic_profile({"R",700,Sex::female,"c","r","citizen","free",
                                       p.default_health(),Service::civilian});
        civ=p.seed_population(d,e,h,200);
        auto dy=p.demographic_profile({"R",940,Sex::male,"c","r","citizen","free",
                                       p.default_health(),Service::civilian});
        young=p.seed_population(dy,e,h,50);
        auto dr=p.demographic_profile({"R",700,Sex::male,"c","r","citizen","free",
                                       p.default_health(),Service::reserve});
        p.seed_population(dr,e,h,50);
        auto dm=p.demographic_profile({"R",700,Sex::male,"c","r","citizen","free",
                                       p.default_health(),Service::active});
        military=p.seed_population(dm,e,h,100);
        world.seed_active("U",military);
        require(world.people().population()==400,"initial population");
        world.validate();
    }
};
static void run(Scenario& s){
    for(int i=0;i<1200;i++){
        IntegratedWorld::Plan p;
        bool produced=false;
        if(i==50)p[1].push_back([](auto& w){w.lend("F",50);});
        if(i==0)p[2].push_back([&](auto& w){w.hire(s.civ,10,"F",2);});
        if(i==701)p[2].push_back([&](auto& w){w.hire(s.young,4,"F",2);});
        p[3].push_back([](auto& w){
            bool have_labor=false;
            for(const auto& [id,j]:w.people().jobs()){
                (void)id;
                if(j.employer=="F"&&j.count>0) have_labor=true;
            }
            if(have_labor&&w.stock("S","R","ore")>0&&w.cash("F")>0)
                w.trade("S","F","R","ore",1,1);
        });
        p[4].push_back([&](auto& w){
            if(w.stock("F","R","ore")>0){
                w.produce("F","R","ore","tool",1,1);
                produced=true;
            }
            if(i==701)w.rebuild_facility("mill","building_materials",4,4);
        });
        p[5].push_back([&](auto& w){
            if(produced)w.wages("F","H","G",10,1);
            if(i==901)w.pay_reparation("peace-1",20);
        });
        p[7].push_back([&](auto& w){
            if(produced){
                w.trade("F","H","R","tool",1,10);
                w.consume("H","R","tool",1);
            }
            if(i==0){
                require(w.clear_consumer_market("S","H","R","food",10)==5,
                        "shortage lacked actual supply bound");
            }
        });
        if(i==50)p[8].push_back([](auto& w){w.forgive_defaulted_loan("F",20);});
        p[9].push_back([&](auto& w){
            if(i%48==0){
                auto voters=w.eligible_voters();
                auto reg=std::min<std::int64_t>(voters,100);
                const auto A=reg*3/5,B=reg-A;
                w.hold_election(reg,0,3,{{"A",A},{"B",B}});
                if(i==0)w.appoint_cabinet("A","Head","Coalition",{"A"});
            }
            if(i==0){
                w.join_interest_group(s.civ,"Food Union",5);
                w.protest(s.civ,"Food Union",5);
            }
            if(i==900)w.sign_reparation_treaty("peace-1","F","H",100);
        });
        p[10].push_back([&](auto& w){
            if(i==100||i==300||i==500||i==700||i==900)
                w.battle("U",s.military,"R",CombatLoss{1,0,0,0,0,1});
            if(i==700)w.war_damage_facility("mill",4);
            if(i==800)w.demobilize("U",s.military,1);
            if(i==300)w.occupy("X","A");
        });
        const auto report=s.world.advance(p);
        require(report.finished_month==1001+i,"month indexing");
        require(s.world.people().population()>=395&&s.world.people().population()<=400,
                "population outside known casualty bounds");
        require(s.world.stock("F","R","tool")==0&&s.world.stock("H","R","tool")==0,
                "unsold or duplicated tools");
        require(s.world.cash("F")>=0&&s.world.cash("G")>=0&&s.world.cash("H")>=0,
                "negative account");
        s.world.validate();
    }
    require(s.world.people().population()==395&&s.world.active_strength("U")==94,
            "war casualties and demobilization not conserved");
    require(s.world.military_control("X")=="A"&&s.world.sovereignty("X")=="B",
            "occupation converted sovereignty");
    require(s.world.facility("mill").damaged==0&&
            s.world.stock("F","R","building_materials")==0,"reconstruction");
    require(s.world.bank_insolvent()&&s.world.bank_losses()==20,"bank insolvency");
    require(s.world.reparations_outstanding("peace-1")==80&&
            s.world.financial().balance("F:reparations_payable")==-80,"peace reparations");
    require(s.world.food_grievance("R")==5&&
            s.world.group_members(s.civ,"Food Union")==5,"material shortage and protest");
    require(s.world.government("A").prime_minister=="Coalition",
            "cabinet succession");
}
int main(){
    auto start=std::chrono::steady_clock::now();
    Scenario a,b;
    run(a);run(b);
    require(a.world.commit_id()==1200&&b.world.commit_id()==1200,"commit count");
    require(a.world.people().fingerprint()==b.world.people().fingerprint(),
            "population cross-domain replay mismatch");
    require(a.world.journal()==b.world.journal(),"event order unstable");
    for(const char* n:{"F","H","G","S","T"})
        require(a.world.cash(n)==b.world.cash(n),"finance outcome not repeatable");
    const auto s=std::chrono::duration<double>(
        std::chrono::steady_clock::now()-start).count();
    std::cout<<"C-D4 bounded combined 1,200-month regression PASS"
             <<" (2 independent repeats)"
             <<" people="<<a.world.people().population()
             <<" active="<<a.world.active_strength("U")
             <<" events="<<a.world.journal().size()
             <<" elapsed_s="<<s<<"\n"
             <<"Scope: 1,200 months of economic trades/tax, elections/government, "
               "market shortage/protests, bank default, battle/occupation, treaty/reconstruction."
               " Separate Phase B test covers 1,200-month endogenous births/deaths."
               " This is not a complete historical or multimillion-cell scenario.\n";
}
