#include "vic25/phase_d_snapshot.hpp"
#include <iostream>
#include <stdexcept>
using namespace vic25::phased;
using vic25::phaseb::Sex; using vic25::phaseb::Service;
static void need(bool x,const char* m){if(!x)throw std::runtime_error(m);}
template<class F>static bool rejects(F&& f){try{f();}catch(const std::exception&){return true;}return false;}
static Id worker(World& w,People n,Id partition){
    auto& p=w.seed_core().seed_population();
    auto h=p.household_profile({"R","shared","shared",n});
    auto e=p.economic_profile({"R","worker","wage","employee",p.default_education()});
    auto d=p.demographic_profile({"R",w.month()-360,Sex::male,
        "culture","faith","citizen","free",p.default_health(),Service::civilian});
    return p.seed_population(d,e,h,n,partition);
}
static void test_paid_contact_and_licensed_nonautomatic_diffusion(){
    World w{5000};auto& c=w.seed_core();
    c.seed_actor("A",0);c.seed_actor("B",50);c.seed_actor("ISOLATED",0);
    c.seed_region("R","STATE");
    c.seed_shares("PATENT:STEAM","A",1);
    w.seed_technology({"STEAM",{},true});
    w.seed_knowledge("A","STEAM",KnowledgeLevel::usable);
    KnowledgeContact contact;
    contact.id="CONTACT";contact.source="A";contact.target="B";contact.tech="STEAM";
    contact.required_instruction=2;contact.teachers_per_work=1;
    contact.tuition_per_work=2;
    w.seed_contact(contact);
    w.seed_license({"LICENSE","STEAM","A","B",10,5000,-1,false});
    auto teacher=worker(w,1,101);
    World::Plan first;
    first[1].push_back([](Stage& s){s.plan_contact("CONTACT",1);});
    first[2].push_back([teacher](Stage& s){s.phase_c().hire(teacher,1,"A",1);});
    first[4].push_back([](Stage& s){s.instruct_contact("CONTACT",1);});
    first[5].push_back([](Stage& s){s.pay_contact("CONTACT",1);});
    first[7].push_back([](Stage& s){s.purchase_license("LICENSE");});
    w.advance(first);
    need(w.license("LICENSE").paid && w.knowledge("B","STEAM")==KnowledgeLevel::unknown &&
         w.knowledge("ISOLATED","STEAM")==KnowledgeLevel::unknown &&
         w.core().cash("B")==38,
         "license is not free usable knowledge or global diffusion");
    World::Plan second;
    second[1].push_back([](Stage& s){s.plan_contact("CONTACT",1);});
    second[4].push_back([](Stage& s){s.instruct_contact("CONTACT",1);});
    second[5].push_back([](Stage& s){s.pay_contact("CONTACT",1);});
    second[9].push_back([](Stage& s){s.review_contact("CONTACT");});
    w.advance(second);
    need(w.knowledge("B","STEAM")==KnowledgeLevel::unknown &&
         w.contact("CONTACT").verified_instruction==2,
         "second month work must be reviewed but not yet active");
    w.advance();
    need(w.knowledge("B","STEAM")==KnowledgeLevel::understood &&
         w.knowledge("A","STEAM")==KnowledgeLevel::usable &&
         w.knowledge("ISOLATED","STEAM")==KnowledgeLevel::unknown &&
         w.core().cash("A")==14 && w.core().cash("B")==36,
         "evidence-based diffusion failed or patent fee unbalanced");
    auto saved=SnapshotCodec::encode(w);
    need(SnapshotCodec::encode(SnapshotCodec::decode(saved))==saved,
         "contract and intellectual-property state lost in snapshot");
}
static void test_occupational_training_changes_real_pop_next_month(){
    World w{5200};auto& c=w.seed_core();
    c.seed_actor("TRAINER",0);c.seed_actor("SPONSOR",20);
    c.seed_region("R","S");
    auto teacher=worker(w,1,110);
    auto trainees=worker(w,2,111);
    TrainingProgram p;
    p.id="PROGRAM";p.trainer="TRAINER";p.sponsor="SPONSOR";
    p.trainee_slice=trainees;p.trainee_count=2;
    p.target_occupational_skill=70;p.required_service=2;p.fee_per_service=3;
    w.seed_training(p);
    World::Plan m1;
    m1[1].push_back([](Stage& s){s.plan_training("PROGRAM",1);});
    m1[2].push_back([teacher](Stage& s){s.phase_c().hire(teacher,1,"TRAINER",1);});
    m1[4].push_back([](Stage& s){s.deliver_training("PROGRAM",1);});
    m1[5].push_back([](Stage& s){s.pay_training("PROGRAM",1);});
    w.advance(m1);
    need(w.training("PROGRAM").verified_service==1 &&
         w.core().people().population()==3 &&
         w.core().people().education(
            w.core().people().economic(
                w.core().people().slice(trainees).key.economic).education).occupational_skill<70,
         "unfinished training improperly altered POP");
    World::Plan m2;
    m2[1].push_back([](Stage& s){s.plan_training("PROGRAM",1);});
    m2[4].push_back([](Stage& s){s.deliver_training("PROGRAM",1);});
    m2[5].push_back([](Stage& s){s.pay_training("PROGRAM",1);});
    m2[10].push_back([](Stage& s){s.certify_training("PROGRAM");});
    w.advance(m2);
    const auto result=w.training("PROGRAM").resulting_slice;
    need(w.training("PROGRAM").certified &&
         w.core().people().has_slice(result) &&
         w.core().people().population()==3 &&
         w.core().people().education(
             w.core().people().economic(
                 w.core().people().slice(result).key.economic).education).occupational_skill==70 &&
         w.core().cash("SPONSOR")==14 && w.core().cash("TRAINER")==6,
         "real occupational education/finance transition did not conserve POP");
    w.advance();
    vic25::phaseb::JobRequirements req;
    req.minimum_skill=60;
    need(w.core().people().eligible_for(result,req),
         "trained population not eligible for skilled employment next month");
}
static void test_commercial_adoption_requires_paid_license(){
    World w{5400};auto& c=w.seed_core();
    c.seed_actor("A",0);c.seed_actor("B",40);
    c.seed_region("R","S");c.seed_shares("PATENT:STEAM","A",1);
    c.seed_production_facility("MILL","B","R",1);
    c.seed_stock("B","R","ore",1);
    w.seed_technology({"STEAM",{},true});
    w.seed_knowledge("B","STEAM",KnowledgeLevel::usable);
    IndustryRecipe r;r.id="STEAM_PROCESS";r.site="MILL";r.tech_id="STEAM";
    r.inputs={{"ore",1}};r.outputs={{"bars",2}};r.workers_per_batch=1;
    w.seed_recipe(r);
    w.seed_license({"LICENSE","STEAM","A","B",10,5400,-1,false});
    const auto operator_worker=worker(w,1,115);
    World::Plan bad;
    bad[9].push_back([](Stage& s){s.propose_adoption("MILL","STEAM");});
    need(rejects([&]{w.advance(bad);})&&w.month()==5400,
         "unlicensed patented process adopted");
    World::Plan lawful;
    lawful[7].push_back([](Stage& s){s.purchase_license("LICENSE");});
    lawful[9].push_back([](Stage& s){s.propose_adoption("MILL","STEAM");});
    w.advance(lawful);
    need(!w.state().active_site_tech.contains("MILL") &&
         w.core().cash("B")==30,"license cash settlement/current month adoption");
    World::Plan next;
    next[2].push_back([operator_worker](Stage& s){
        s.phase_c().hire(operator_worker,1,"B",1);
    });
    next[4].push_back([](Stage& s){s.manufacture("STEAM_PROCESS",1);});
    w.advance(next);
    need(w.state().active_site_tech.at("MILL")=="STEAM" &&
         w.core().stock("B","R","ore")==0 &&
         w.core().stock("B","R","bars")==2 &&
         w.core().cash("A")==10 &&
         w.core().cash("B")==30,
         "real site adoption requires correct next-month license and inputs");
    w.validate();
}
int main(){
    try{
        test_paid_contact_and_licensed_nonautomatic_diffusion();
        test_occupational_training_changes_real_pop_next_month();
        test_commercial_adoption_requires_paid_license();
        std::cout<<"Technology transfer, lawful licensing and POP skill tests PASS\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"Technology FAIL: "<<e.what()<<"\n";return 1;
    }
}
