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
static void test_legacy_paid_courses_are_retired_without_changing_pop_or_postings(){
    constexpr std::string_view frozen_hex =
#include "fixtures/phase_d_v8_paid_courses.inc"
        ;
    const auto nibble=[](char c){return c>='a'?c-'a'+10:c-'0';};
    std::string old;
    old.reserve(frozen_hex.size()/2);
    for(std::size_t i=0;i<frozen_hex.size();i+=2)
        old.push_back(static_cast<char>(16*nibble(frozen_hex[i])+nibble(frozen_hex[i+1])));
    auto w=SnapshotCodec::decode(old);
    const auto canonical=SnapshotCodec::encode(w);
    need(canonical.size()<old.size(),"retired course rows must not be written back");
    need(w.core().cash("SPONSOR")==14&&w.core().cash("TRAINER")==6&&
         w.core().financial().balanced(),"historical course payments must remain posted exactly once");
    vic25::phaseb::JobRequirements req;req.minimum_skill=60;
    need(w.core().people().population()==5&&w.core().people().eligible_for(15,req)&&
         !w.core().people().eligible_for(8,req),"earned skill must remain; partial course must grant none");
    need(w.knowledge("SPONSOR","STEAM")==KnowledgeLevel::usable&&
         canonical.find("next-month-valid-POP-skill:COMPLETE")!=std::string::npos,
         "later fields and historical events must survive the retired snapshot slot");
    need(SnapshotCodec::encode(SnapshotCodec::decode(canonical))==canonical,
         "retired snapshot normalization must be deterministic");
}
static void test_pending_earned_skill_survives_daily_migration_and_resume(){
    using namespace vic25;
    phasegh::World gh{24000};
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"R","A");geo.seed_province(2,"DEST","A");
    geo.seed_unit(1,"A","ROOT");geo.seed_assignment(1,1);geo.seed_assignment(2,1);
    auto& d=geo.seed_core();auto& c=d.seed_core();
    c.seed_actor("A",0);c.seed_actor("FACTORY",100);
    auto& pop=c.seed_population();
    const auto dest=pop.household_profile({"DEST","migration","shared",1});
    const auto family=pop.household_profile({"R","shared","shared",2});
    const auto wallet=[](Id house) {return "HH:"+std::to_string(house);};
    c.seed_actor(wallet(family),0);c.seed_actor(wallet(dest),0);
    const auto econ=pop.economic_profile({"R","machinist","wage","unemployed",pop.default_education()});
    const auto demo=pop.demographic_profile({"R",23640,phaseb::Sex::male,"culture","faith","citizen",
                                         "free",pop.default_health(),Service::civilian});
    const auto source=pop.seed_population(demo,econ,family,2);
    const auto earned=pop.schedule_occupational_education(source,1,70);
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
    constexpr std::string_view marker="VIC25_PHASEC_POP_OCCUPATIONAL_EDUCATION_1";
    const auto saved=daily::SnapshotCodec::encode(game.world());
    need(saved.find(marker)!=std::string::npos,"the Phase C payload must save pending skill");
    auto replay=p0::DailyGame(daily::SnapshotCodec::decode(saved),policy);
    const auto ledger=[](const p0::DailyGame& current)->const phasec::IntegratedWorld& {
        return current.world().engine().base().core().core();
    };
    const auto move=[&](Id& arrived) {
        phasegh::World::Plan plan;
        plan[10].push_back([&](auto& stage) {
            arrived=stage.phase_c().migrate_with_explicit_property_allocation(
                earned,1,"DEST",dest,wallet(family),wallet(dest),0,"",0);
        });
        return plan;
    };
    Id arrived{},replayed{};
    game.advance_day(move(arrived));replay.advance_day(move(replayed));
    need(arrived!=earned&&arrived==replayed&&!ledger(game).people().eligible_for(arrived,req)&&
         ledger(game).phase_d_workforce("FACTORY")==0,
         "migration before the due month must not make the pending skill usable");
    while(game.world().date().month==1){
        game.advance_day();replay.advance_day();
    }
    need(game.world().date().day==1&&ledger(game).people().eligible_for(arrived,req)&&
         !ledger(game).people().eligible_for(source,req)&&!ledger(game).people().job(arrived)&&
         daily::SnapshotCodec::encode(game.world()).find(marker)==std::string::npos,
         "only the entitled migrant's skill becomes effective at the month boundary");
    phasegh::World::Plan open;
    open[2].push_back([&](auto& stage) {
        need(stage.phase_c().people().eligible_for(arrived,req),
             "Stage 2 must see the already-earned skill");
    });
    game.advance_day(open);replay.advance_day(open);
    need(ledger(game).phase_d_workforce("FACTORY")==1&&ledger(game).people().job(arrived)&&
         ledger(game).people().job(arrived)->requirements.minimum_skill==60&&
         ledger(game).people().population()==2&&
         daily::SnapshotCodec::encode(game.world())==daily::SnapshotCodec::encode(replay.world()),
         "new and resumed games must hire the same eligible worker without losing POP");
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

int main(){
    try{
        test_paid_contact_and_licensed_nonautomatic_diffusion();
        test_legacy_paid_courses_are_retired_without_changing_pop_or_postings();
        test_pending_earned_skill_survives_daily_migration_and_resume();
        test_commercial_adoption_requires_paid_license();
        enacted_recipient_laws_change_actual_paid_technology_diffusion();
        std::cout<<"Technology transfer, lawful licensing and POP skill tests PASS\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"Technology FAIL: "<<e.what()<<"\n";return 1;
    }
}
