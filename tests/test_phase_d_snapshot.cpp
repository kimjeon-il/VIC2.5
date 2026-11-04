#include "vic25/phase_d_snapshot.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace vic25::phased;
using vic25::phaseb::Sex;
using vic25::phaseb::Service;

static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class F> static bool rejected(F&& f){
    try{f();}catch(const std::exception&){return true;}
    return false;
}
static World seed() {
    World w{5000};
    auto& c=w.seed_core();
    c.seed_actor("CLIENT",100);
    c.seed_actor("BUILDER",0);
    c.seed_region("R","STATE");
    c.seed_stock("BUILDER","R","steel",2);
    c.seed_production_facility("OLD","BUILDER","R",1);
    auto& pop=c.seed_population();
    const auto h=pop.household_profile({"R","shared","shared",2});
    const auto e=pop.economic_profile({"R","worker","wage","employee",pop.default_education()});
    const auto d=pop.demographic_profile({"R",w.month()-300,Sex::female,"culture","faith",
                                          "citizen","free",pop.default_health(),Service::civilian});
    const auto id=pop.seed_population(d,e,h,2);
    ConstructionProject p;
    p.id="P";p.client="CLIENT";p.contractor="BUILDER";p.region="R";
    p.material_good="steel";p.target_site="NEW";p.work_required=1;
    p.material_per_work=1;p.target_capacity=2;p.workers_per_work=1;
    p.cash_price_per_work=20;
    w.seed_construction(p);
    IndustryRecipe rec;
    rec.id="R";rec.site="OLD";rec.inputs={{"steel",1}};
    rec.outputs={{"machinery",1}};rec.workers_per_batch=1;
    w.seed_recipe(rec);
    w.seed_knowledge("CLIENT","TEST_KNOWLEDGE",KnowledgeLevel::observed);
    World::Plan plan;
    plan[1].push_back([](Stage& s){s.plan_work("P",1);});
    plan[2].push_back([id](Stage& s){s.phase_c().hire(id,2,"BUILDER",1);});
    plan[4].push_back([](Stage& s){s.perform_work("P",1);});
    plan[5].push_back([](Stage& s){s.pay_work("P",1);});
    plan[8].push_back([](Stage& s){s.capitalize_work("P",1);});
    w.advance(plan);
    return w;
}
int main(){
    try {
        World original=seed();
        const auto bytes=SnapshotCodec::encode(original);
        auto restored=SnapshotCodec::decode(bytes);
        check(restored.month()==original.month() &&
              restored.commit_id()==original.commit_id() &&
              restored.construction("P").ready &&
              restored.state().recipes.size()==1 &&
              restored.knowledge("CLIENT","TEST_KNOWLEDGE")==KnowledgeLevel::observed &&
              restored.core().phase_d_wip("CLIENT")==20 &&
              restored.state().journal==original.state().journal,
              "roundtrip lost phase D contract state or Phase C WIP");

        const auto path=std::filesystem::temp_directory_path()/
            "vic25-phase-d-snapshot-atomic-test.bin";
        std::filesystem::remove(path);
        SnapshotCodec::save(original,path);
        check(rejected([&]{SnapshotCodec::save(original,path);}),
              "checkpoint overwrite was not prevented");
        auto loaded=SnapshotCodec::load(path);
        std::filesystem::remove(path);
        check(loaded.construction("P").ready && loaded.core().cash("CLIENT")==80,
              "disk checkpoint failed to recover both worlds");

        original.advance();restored.advance();loaded.advance();
        check(original.construction("P").commissioned &&
              restored.construction("P").commissioned &&
              loaded.construction("P").commissioned &&
              original.core().phase_d_wip("CLIENT")==0 &&
              restored.core().facility("NEW").capacity==2 &&
              original.core().financial().balance("CLIENT:fixed_asset")==20 &&
              SnapshotCodec::encode(original)==SnapshotCodec::encode(restored) &&
              SnapshotCodec::encode(original)==SnapshotCodec::encode(loaded),
              "reload/replay diverged or staged facility not commissioned");

        std::string tampered=bytes;
        tampered[tampered.size()/2]=static_cast<char>(tampered[tampered.size()/2]^0x31);
        check(rejected([&]{(void)SnapshotCodec::decode(tampered);}),
              "corrupted snapshot silently accepted");
        std::string old_contract=bytes;
        const std::string correct="phase-d-cpp/0.3",invalid="phase-d-cpp/0.0";
        const auto pos=old_contract.find(correct);
        check(pos!=std::string::npos,"missing contract version in binary snapshot");
        old_contract.replace(pos,correct.size(),invalid);
        check(rejected([&]{(void)SnapshotCodec::decode(old_contract);}),
              "old/incompatible contract accepted");

        const auto before=SnapshotCodec::encode(original);
        World::Plan broken;
        broken[11].push_back([](Stage&){throw std::runtime_error("late rollback");});
        check(rejected([&]{original.advance(broken);}) &&
              SnapshotCodec::encode(original)==before,
              "late failure after restoring checkpoint mutated committed World");
        std::cout<<"Phase D atomic checkpoint/replay: 5 checks PASS.\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"Phase D snapshot FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
