#include "vic25/phase_d_snapshot.hpp"
#include "vic25/p0_daily.hpp"
#include "vic25/daily_snapshot.hpp"
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
static void enacted_recipient_laws_change_actual_paid_technology_diffusion(){
    using namespace vic25::phasec;
    const auto run=[](const char* group,const char* law,Quantity expected,
                      bool production=false,bool recipient_company=false,
                      bool ambiguous=false,bool law_at_source=false,bool forged=false) {
        World world{5000};
        auto& c=world.seed_core();
        c.seed_region("RA","A");c.seed_region("RB","B");
        c.seed_actor("A",1000);c.seed_actor("B",0);
        c.seed_actor("FIRM",1000);c.seed_actor("THIRD",1000);
        for(const auto* nation:{"A","B"}) {
            c.seed_government(nation,"HEAD","MINISTER",5,3);
            c.seed_p1_constitution(nation,{P1GovernancePrinciple::monarchy,
                P1PowerDistribution::autocracy,P1LegislatureKind::none,"fixture"});
        }
        const auto receiver=recipient_company?"FIRM":"A";
        c.seed_production_facility("MILL",receiver,"RA",10);
        if(ambiguous)c.seed_production_facility("OTHER","FIRM","RB",10);
        const std::string source_owner=law_at_source?"B":"A";
        if(forged) {
            P2BaseLaw local{"LOCAL","A","ordinary_local_policy","RA",law,"fixture"};
            local.tier=P2NormTier::local_ordinance;
            c.p2_seed_base(local);
            c.p2_seed_base({"CUSTOM","A","unrelated_national_group","national",law,"fixture"});
        } else if(*law)
            c.p2_seed_base({"LAW",source_owner,group,"national",law,"fixture"});
        world.seed_technology({"KNOWN",{},false});
        world.seed_knowledge("B","KNOWN",KnowledgeLevel::usable);
        if(production) {
            IndustryRecipe recipe;
            recipe.id="REAL-RECIPE";recipe.site="MILL";recipe.tech_id="KNOWN";
            recipe.inputs={{"ore",1}};recipe.outputs={{"metal",1}};
            recipe.workers_per_batch=recipe.capacity_per_batch=1;
            world.seed_recipe(recipe);
        }
        world.seed_contact({"CONTACT","B",receiver,"KNOWN",100,0,0,1,2});
        auto& pop=c.seed_population();
        const auto h=pop.household_profile({"RB","shared","shared",100});
        const auto e=pop.economic_profile({"RB","worker","wage","employee",pop.default_education()});
        const auto d=pop.demographic_profile({"RB",4640,Sex::male,
            "culture","faith","citizen","free",pop.default_health(),Service::civilian});
        const auto workers=pop.seed_population(d,e,h,200);
        const auto instruction=[&](Quantity amount,bool hire,bool review) {
            World::Plan plan;
            plan[1].push_back([=](Stage& s){s.plan_contact("CONTACT",amount);});
            if(hire)plan[2].push_back([=](Stage& s){s.phase_c().hire(workers,200,"B",1);});
            plan[4].push_back([=](Stage& s){s.instruct_contact("CONTACT",amount);});
            plan[5].push_back([=](Stage& s){s.pay_contact("CONTACT",amount);});
            if(review)plan[9].push_back([](Stage& s){s.review_contact("CONTACT");});
            return plan;
        };
        world.advance(instruction(expected-1,true,false));
        const auto before=vic25::phased::SnapshotCodec::encode(world);
        World::Plan early;
        early[9].push_back([](Stage& s){s.review_contact("CONTACT");});
        need(rejects([&]{world.advance(early);})&&vic25::phased::SnapshotCodec::encode(world)==before,
             "actual received instruction qualified early or failure lost work/cash");
        auto unbacked=instruction(1,false,true);
        unbacked[5].clear();
        need(rejects([&]{world.advance(unbacked);})&&
             vic25::phased::SnapshotCodec::encode(world)==before,
             "unpaid delivered instruction changed the recipient or lost cash");
        // One further instructional unit must be paid and reviewed, and
        // delivery becomes effective only at the next month's opening.
        try {world.advance(instruction(1,false,true));}
        catch(const std::exception& e) {
            throw std::runtime_error(std::string("paid recipient law ")+law+": "+e.what());
        }
        need(world.contact("CONTACT").paid_instruction==expected&&
             world.knowledge(receiver,"KNOWN")==KnowledgeLevel::unknown&&
             world.core().cash(receiver)==1000-2*expected&&world.core().cash("B")==2*expected,
             "actual taught and paid work did not exactly match legal recipient threshold");
        auto replay=vic25::phased::SnapshotCodec::decode(vic25::phased::SnapshotCodec::encode(world));
        world.advance();replay.advance();
        need(world.knowledge(receiver,"KNOWN")==KnowledgeLevel::understood&&
             world.knowledge("B","KNOWN")==KnowledgeLevel::usable&&
             world.knowledge("THIRD","KNOWN")==KnowledgeLevel::unknown&&
             vic25::phased::SnapshotCodec::encode(world)==vic25::phased::SnapshotCodec::encode(replay),
             "paid contact skipped knowledge boundaries or next-month replay");
    };
    run("free_speech","law_protected_speech",80);
    run("trade_policy","law_isolationism",118);
    run("trade_policy","law_canton_system",112,false,true);
    run("trade_policy","law_sakoku",125);
    run("free_speech","law_outlawed_dissent",118);
    run("free_speech","law_censorship",112);
    run("economic_system","law_industry_banned",134,true);
    run("economic_system","law_industry_banned",100);
    // The education law slows original labs, not a foreign donor class.
    run("education_system","law_terakoya",100,true);
    run("free_speech","law_protected_speech",100,false,true,true);
    run("free_speech","law_protected_speech",100,false,false,false,true);
    run("free_speech","law_protected_speech",100,false,false,false,false,true);
}

static void test_daily_paid_training_waits_for_next_month() {
    using namespace vic25;
    const auto ledger=[](const p0::DailyGame& game)->const phasec::IntegratedWorld& {
        return game.world().engine().base().core().core();
    };
    const auto course=[](const p0::DailyGame& game)->const TrainingProgram& {
        return game.world().engine().base().core().training("COURSE");
    };
    phasegh::World gh{24000};
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"R","A");geo.seed_unit(1,"A","ROOT");geo.seed_assignment(1,1);
    auto& d=geo.seed_core();auto& c=d.seed_core();
    c.seed_actor("A",0);c.seed_actor("FACTORY",100);
    c.seed_actor("TRAINER",0);c.seed_actor("SPONSOR",20);
    geo.seed_province(2,"DEST","A");
    geo.seed_assignment(2,1);
    const auto dest_house = c.seed_population().household_profile({"DEST","migration","shared",1});
    const auto dest_wallet = "HH:"+std::to_string(dest_house);
    c.seed_actor(dest_wallet,0);
    auto& pop=c.seed_population();
    const auto cohort=[&](std::string profession,std::string family,People count=1){
        const auto h=pop.household_profile({"R",std::move(family),"shared",1});
        const auto e=pop.economic_profile({"R",profession,"wage","unemployed",pop.default_education()});
        const auto b=pop.demographic_profile({"R",23640,phaseb::Sex::male,
            "culture","faith","citizen","free",pop.default_health(),phaseb::Service::civilian});
        const auto id=pop.seed_population(b,e,h,count);
        const auto wallet="HH:"+std::to_string(h);
        if(profession!="teacher")c.seed_actor(wallet,0);
        return std::pair{id,wallet};
    };
    const auto [teacher,teacher_wallet]=cohort("teacher","educator");
    const auto [untaught,untaught_wallet]=cohort("machinist","comparison");
    const auto [trainee,trainee_wallet]=cohort("machinist","training",2);
    pop.hire(teacher,1,"TRAINER","teacher",310,{});
    phased::TrainingProgram t;
    t.id="COURSE";t.trainer="TRAINER";t.sponsor="SPONSOR";
    t.trainee_slice=trainee;t.trainee_count=1;t.target_occupational_skill=70;
    t.required_service=2;t.fee_per_service=3;
    d.seed_training(t);
    phaseb::DemographyRules r;
    r.fertility_ppm=r.infant_death_ppm=r.child_death_ppm=r.adult_death_ppm=
        r.senior_death_ppm=r.very_old_death_ppm=r.illness_incidence_ppm=r.accident_ppm=0;
    r.primary_access_ppm=r.secondary_access_ppm=r.tertiary_access_ppm=
        r.adult_literacy_access_ppm=r.training_ppm=0;
    phaseb::JobRequirements req;req.minimum_skill=60;
    r.monthly_vacancies.push_back({"FACTORY","machinist",31,310,req});
    c.enable_demography(r);
    p0::Policies policy;policy.daily_food_units_per_person=0;
    p0::DailyGame game(daily::World::adopt(std::move(gh),999),policy);
    const auto training=[](bool delivered,bool paid,bool certified){
        phasegh::World::Plan plan;
        plan[1].push_back([](auto& s){s.phase_d().plan_training("COURSE",1);});
        if(delivered)plan[4].push_back([](auto& s){s.phase_d().deliver_training("COURSE",1);});
        if(paid)plan[5].push_back([](auto& s){s.phase_d().pay_training("COURSE",1);});
        if(certified)plan[10].push_back([](auto& s){s.phase_d().certify_training("COURSE");});
        return plan;
    };
    game.advance_day(training(true,true,false));
    need(course(game).verified_service==1&&course(game).paid_service==1&&
          !course(game).certified&&ledger(game).phase_d_workforce("FACTORY")==0&&
          !ledger(game).people().eligible_for(trainee,req)&&!ledger(game).people().eligible_for(untaught,req)&&
          ledger(game).cash("SPONSOR")==17&&ledger(game).cash("TRAINER")==3,
          "partial paid course must not change skill or employment");
    const auto saved=daily::SnapshotCodec::encode(game.world());
    for(const bool delivered:{false,true}) {
        bool blocked=false;
        try{game.advance_day(training(delivered,false,true));}
        catch(const std::exception& e){
            blocked=std::string(e.what()).find("lacks verified training input")!=std::string::npos;
        }
        need(blocked&&daily::SnapshotCodec::encode(game.world())==saved,
              "incomplete delivery/payment must roll certification back without any invented money or people");
    }
    game.advance_day(training(true,true,true));
    const auto trained=course(game).resulting_slice;
    need(course(game).certified&&trained!=0&&trained!=trainee&&
         !ledger(game).people().eligible_for(trained,req)&&
         ledger(game).people().eligible_for(trained,phaseb::JobRequirements{})&&
         !ledger(game).people().eligible_for(trainee,req)&&
         !ledger(game).people().eligible_for(untaught,req)&&
         ledger(game).cash("SPONSOR")==14&&ledger(game).cash("TRAINER")==6&&
         ledger(game).people().population()==4,
         "paid Jan2 course must still withhold elevated skill during January");
    constexpr std::string_view marker="VIC25_PHASEC_POP_OCCUPATIONAL_EDUCATION_1";
    const auto pending_checkpoint=daily::SnapshotCodec::encode(game.world());
    need(pending_checkpoint.find(marker)!=std::string::npos,
         "only unfinished credentials need saved scheduling metadata");
    auto replay=p0::DailyGame(daily::SnapshotCodec::decode(pending_checkpoint),policy);
    Id arrived=0;
    const auto move=[&](Id& result) {
        phasegh::World::Plan plan;
        plan[10].push_back([&](auto& stage) {
            result=stage.phase_c().migrate_with_explicit_property_allocation(
                trained,1,"DEST",dest_house,trainee_wallet,dest_wallet,0,"",0);
        });
        return plan;
    };
    game.advance_day(move(arrived));
    Id replay_arrived=0;
    replay.advance_day(move(replay_arrived));
    need(arrived==replay_arrived&&arrived!=trained&&!ledger(game).people().has_slice(trained)&&
         !ledger(game).people().eligible_for(arrived,req)&&
         ledger(game).cash(dest_wallet)==0&&ledger(game).cash("FACTORY")==100,
         "Jan3 real migration must neither shed the pending credential nor start skilled wages");
    while(game.world().date().month==1) {
        game.advance_day();
        replay.advance_day();
    }
    need(game.world().date().day==1&&ledger(game).people().eligible_for(arrived,req)&&
         !ledger(game).people().eligible_for(trainee,req)&&
         !ledger(game).people().eligible_for(untaught,req)&&
         ledger(game).cash(dest_wallet)==0&&ledger(game).cash("FACTORY")==100&&
         !ledger(game).people().job(arrived)&&
         daily::SnapshotCodec::encode(game.world()).find(marker)==std::string::npos,
         "only the certified migrant must become eligible at February opening and clear scheduling");
    phasegh::World::Plan february_open;
    february_open[2].push_back([&](auto& stage) {
        need(stage.phase_c().people().eligible_for(arrived,req),
             "February Stage 2 must already see earned occupation skill");
    });
    game.advance_day(february_open);
    replay.advance_day(february_open);
    need(ledger(game).phase_d_workforce("FACTORY")==1&&ledger(game).people().job(arrived)&&
         ledger(game).people().job(arrived)->requirements.minimum_skill==60&&
         ledger(game).financial().balanced()&&ledger(game).people().population()==4&&
         daily::SnapshotCodec::encode(game.world())==daily::SnapshotCodec::encode(replay.world()),
         "February typed hiring must use earned skill in current and resumed games");
    game.advance_day();replay.advance_day();
    need(ledger(game).cash(dest_wallet)>0&&ledger(game).cash("A")>0&&
         ledger(game).cash(trainee_wallet)==0&&ledger(game).cash(untaught_wallet)==0&&
         ledger(game).financial().balanced()&&
         daily::SnapshotCodec::encode(game.world())==daily::SnapshotCodec::encode(replay.world()),
         "only resulting qualified labor may earn wages, with actual tax and deterministic replay");
}

int main(){
    try{
        test_paid_contact_and_licensed_nonautomatic_diffusion();
        test_occupational_training_changes_real_pop_next_month();
        test_daily_paid_training_waits_for_next_month();
        test_commercial_adoption_requires_paid_license();
        enacted_recipient_laws_change_actual_paid_technology_diffusion();
        std::cout<<"Technology transfer, lawful licensing and POP skill tests PASS\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"Technology FAIL: "<<e.what()<<"\n";return 1;
    }
}
