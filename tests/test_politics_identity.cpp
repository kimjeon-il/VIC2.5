#include "vic25/daily_snapshot.hpp"
#include "vic25/phase_c_politics_catalog.hpp"
#include "vic25/phase_c_politics_effect_audit.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace vic25;
using namespace vic25::phasec;

static void check(bool yes,const char* message) {
    if(!yes)throw std::runtime_error(message);
}
template<class F>static bool rejects(F&& fn) {
    try{fn();}catch(const std::exception&){return true;}
    return false;
}
static void law(IntegratedWorld& core,const std::string& country,
                const std::string& group,const std::string& choice) {
    P2BaseLaw entry;
    entry.id=country+":"+group;
    entry.country=country;
    entry.domain="lawgroup_"+group;
    entry.scope="national";
    entry.base_choice="law_"+choice;
    entry.legal_basis="scenario:constitution";
    core.p2_seed_base(entry);
}
struct Scenario {
    phasegh::World gh{24000};
    phaseb::Id primary{},foreign{},slave{},serf{},other{};
    phaseb::Id foreign_household{};
    explicit Scenario(std::string b_migration="migration_controls",
                      std::string a_citizenship="ethnostate",
                      bool abolition=true,bool allow_serfdom=true) {
        auto& geo=gh.seed_geography();
        geo.set_dataset("political-identity-fixture");
        geo.seed_province(1,"RA","A");
        geo.seed_province(2,"RB","B");
        geo.seed_unit(10,"A","A-DISTRICT");
        geo.seed_unit(20,"B","B-DISTRICT");
        geo.seed_assignment(1,10);
        geo.seed_assignment(2,20);
        auto& c=geo.seed_core().seed_core();
        c.seed_actor("A",500);
        c.seed_actor("B",400);
        c.seed_actor("EMP",500);
        c.seed_government("A","HEAD_A","PM_A",5,3);
        c.seed_government("B","HEAD_B","PM_B",5,3);
        c.seed_p1_constitution("A",{
            P1GovernancePrinciple::monarchy,
            P1PowerDistribution::autocracy,
            P1LegislatureKind::none,"scenario:constitution"});
        c.seed_p1_constitution("B",{
            P1GovernancePrinciple::monarchy,
            P1PowerDistribution::autocracy,
            P1LegislatureKind::none,"scenario:constitution"});
        c.p2_seed_culture({"AAA",{"h_aaa"},{"hgrp_aaa"},{"lang_aaa"},
            {"langgroup_aaa"},{"trad_aaa"},{"RA"}});
        c.p2_seed_culture({"BBB",{"h_bbb"},{"hgrp_bbb"},{"lang_bbb"},
            {"langgroup_bbb"},{"trad_bbb"},{"RB"}});
        c.p2_seed_religion({"FAITH_A",{"relig_a"},{"rgrp_a"}});
        c.p2_seed_religion({"FAITH_B",{"relig_b"},{"rgrp_b"}});
        c.p2_seed_national_identity({"A",{"AAA"},{"FAITH_A"}});
        c.p2_seed_national_identity({"B",{"BBB"},{"FAITH_B"}});
        auto& pop=c.seed_population();
        const auto hha=pop.household_profile({"RA","mixed","shared",50});
        foreign_household=pop.household_profile({"RB","mixed","shared",50});
        const auto eca=pop.economic_profile({"RA","worker","wage","employee",
                                            pop.default_education()});
        const auto ecb=pop.economic_profile({"RB","worker","wage","employee",
                                            pop.default_education()});
        const auto demographic=[&](std::string region,std::string culture,
                                   std::string religion,std::string status) {
            return pop.demographic_profile({region,24000-350,
                phaseb::Sex::male,culture,religion,"citizen",status,
                pop.default_health(),phaseb::Service::civilian});
        };
        primary=pop.seed_population(
            demographic("RA","AAA","FAITH_A","free"),eca,hha,30,1);
        foreign=pop.seed_population(
            demographic("RA","BBB","FAITH_B","free"),eca,hha,20,2);
        slave=pop.seed_population(
            demographic("RA","AAA","FAITH_A","slave"),eca,hha,10,3);
        serf=pop.seed_population(
            demographic("RA","AAA","FAITH_A","serf"),eca,hha,12,4);
        other=pop.seed_population(
            demographic("RB","BBB","FAITH_B","free"),ecb,foreign_household,30,5);
        c.seed_interest_group("WORKERS");
        c.p3_seed_group({"A","WORKERS","","","",P3GroupType::trade_unions,
                         800'000,0});
        law(c,"A","citizenship",a_citizenship);
        law(c,"A","church_and_state","state_religion");
        law(c,"A","migration","no_migration_controls");
        law(c,"A","land_reform",allow_serfdom?"serfdom":"tenant_farmers");
        if(abolition)law(c,"A","slavery","slavery_banned");
        law(c,"B","citizenship","multicultural");
        law(c,"B","migration",b_migration);
        gh.validate();
    }
};
static const IntegratedWorld& view(const daily::World& w){
    return w.engine().base().core().core();
}
static void source_ideologies_laws_traits(){
    const auto& c=politics::catalogue();
    check(c.by_group.size()==26&&c.laws.size()==148,
          "1.15 law catalogue not complete");
    check(c.ideologies.size()==190,
          "1.13.11 verified ideology references omitted");
    check(c.stance("paternalistic","governance_principles","monarchy").value_or(0)>0,
          "imported source ideology positions missing");
    check(c.stance("feminist","rights_of_women","womens_suffrage").value_or(0)>0,
          "verified leader ideology position missing");
    std::size_t effects=0;
    for(const auto& [law,items]:c.modifiers) {
        check(c.laws.contains(law),"unmapped verified law modifier");
        effects+=items.size();
    }
    check(c.modifiers.size()==148&&effects==812,
          "verified 1.15 modifier count changed without source review");
    const auto serf=c.modifier("serfdom","state_peasants_internal_migration_disallowed_bool");
    check(serf.has_value()&&serf->boolean&&serf->value_ppm==1'000'000,
          "V3 serf mobility restriction missing from sourced modifiers");
    const auto tax=c.modifier("traditionalism","state_tax_capacity_mult");
    check(tax.has_value()&&!tax->boolean,
          "real traditionalism tax-capacity modifier not imported");
    check(politics::ig_trait_rules().size()==24,
          "eight interest groups must have all three baseline traits");
    const auto loyal=politics::active_ig_effects(P3GroupType::devout,80,100);
    bool fertility=false;
    for(const auto& x:loyal)
        if(x.modifier=="state_birth_rate_mult")
            fertility=x.value_ppm==30'000;
    check(fertility,"1.15 loyal Devout fertility trait source mismatch");
    check(politics::active_ig_effects(P3GroupType::devout,80,0).empty(),
          "fictional IG trait from zero real members");
}
static void acceptance_and_five_tier_rights() {
    Scenario s;
    const auto& c=s.gh.base().core().core();
    const auto a=c.p2_acceptance("A",s.primary);
    const auto b=c.p2_acceptance("A",s.foreign);
    check(a.has_value()&&b.has_value()&&a->score==100&&b->score<=19,
          "cultural acceptance did not read explicit primary culture and law");
    check(c.p2_citizenship_vote_allowed("A",s.primary) &&
          !c.p2_citizenship_vote_allowed("A",s.foreign),
          "ethnostate enfranchisement not tied to actual POP acceptance");
    const auto disallowed=c.p2_acceptance_effect("A",s.foreign);
    check(disallowed&&disallowed->political_strength_ppm==500'000&&
          disallowed->wage_ppm==500'000,
          "ethnostate hostility penalties not applied");
    const auto allowed=c.p2_acceptance_effect("A",s.primary);
    check(allowed&&allowed->wage_ppm==1'250'000,
          "fully accepted primary culture wage rights incorrect");
    const auto no_data=c.p2_acceptance("A",99999);
    check(!no_data.has_value(),"unknown POP acceptance falsely inferred");
    const auto existing=s.gh.base().core().core().people().population();
    auto w=daily::World::adopt(std::move(s.gh),42);
    phasegh::World::Plan contract;
    const auto worker=s.foreign;
    contract[2].push_back([worker](phasegh::Stage& stage){
        stage.phase_c().hire(worker,2,"EMP",100);
    });
    w.advance_day(contract);
    const auto& c2=view(w);
    const auto job=c2.people().job(worker);
    check(job&&job->wage_per_month==50&&
          c2.people().population()==existing,
          "discrimination didn't affect actual new wage contract exactly once");
    const auto bytes=daily::SnapshotCodec::encode(w);
    check(daily::SnapshotCodec::encode(
                daily::SnapshotCodec::decode(bytes))==bytes,
          "source-qualified cultural identities broke old daily checkpoint");
}
static void minimum_wage_protects_accepted_new_contract_and_payroll() {
    const auto run=[](bool minimum,bool deeply_unaccepted,Money offer,Money expected) {
        Scenario s;
        auto& core=s.gh.seed_geography().seed_core().seed_core();
        if(minimum)
            core.p2_seed_base({"REAL-WAGE-FLOOR","A","wages_trade","national",
                "statutory-wage-floor","scenario:absolute-contract-minimum",
                P2NormTier::statute,
                {{"contract-minimum","A",P2ClauseKind::minimum_monthly_contract_wage,62}}});
        const auto worker=deeply_unaccepted?s.foreign:s.primary;
        const auto factor=core.p2_acceptance_effect("A",worker);
        check(factor&&factor->wage_ppm==(deeply_unaccepted?500'000:1'250'000),
              "fixture needs a real acceptance wage modifier");
        const auto wallet="HH:"+std::to_string(core.people().slice(worker).key.household);
        auto world=daily::World::adopt(std::move(s.gh),42);
        phasegh::World::Plan day;
        day[2].push_back([=](phasegh::Stage& stage){
            stage.phase_c().hire(worker,2,"EMP",offer);
        });
        day[5].push_back([=,today=world.day_index()](phasegh::Stage& stage){
            auto& live=stage.phase_c();
            live.phase_time_accrue_live_contract_wage(worker,"EMP","new-wage",1,31,today);
            live.phase_time_settle_wage_arrears(today);
        });
        if(expected<0) {
            const auto before=daily::SnapshotCodec::encode(world);
            check(rejects([&]{world.advance_day(day);})&&
                  daily::SnapshotCodec::encode(world)==before,
                  "an offered wage below the floor must still reject atomically");
            return;
        }
        world.advance_day(day);
        const auto& live=view(world);
        const auto* job=live.people().job(worker);
        const auto gross=vic25::daily_budget::increment(2*expected,1,31);
        check(job&&job->wage_per_month==expected&&job->count==2&&
              live.cash("EMP")==500-gross&&live.cash(wallet)==gross&&
              live.outstanding_worker_wages("EMP")==0,
              "actual new contract and paid payroll must respect the legal floor");
    };
    run(false,true,62,31);
    run(true,false,62,77);
    run(true,true,61,-1);
    run(true,true,62,62);
}

static void exact_church_laws_change_real_religious_acceptance() {
    using Scores=std::array<int,5>;
    enum class Scope { national, foreign, custom, local };
    const auto run=[](const std::string& church,Scores points,
                      Scope scope=Scope::national,bool empty_references=false,
                      bool dated=false) {
        phasegh::World gh{24000};
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"SOURCE","A");geo.seed_unit(1,"A","ORIGIN");
        geo.seed_province(2,"TARGET","B");geo.seed_unit(2,"B","DESTINATION");
        geo.seed_assignment(1,1);geo.seed_assignment(2,2);
        auto& industry=geo.seed_core();
        auto& c=industry.seed_core();
        industry.seed_technology({"rationalism",{},false});
        industry.seed_technology({"empiricism",{"rationalism"},false});
        for(const char* country:{"A","B"}) {
            industry.seed_knowledge(country,"rationalism",phased::KnowledgeLevel::usable);
            industry.seed_knowledge(country,"empiricism",phased::KnowledgeLevel::usable);
            c.seed_actor(country,100);
            c.seed_government(country,"HEAD","PRIME",5,3);
            c.seed_p1_constitution(country,{P1GovernancePrinciple::monarchy,
                P1PowerDistribution::autocracy,P1LegislatureKind::none,
                "fixture:constitution"});
            // Millet and People of the Book require a recognized qualifying
            // governance law, not merely the monarchy executive profile.
            law(c,country,"governance_principles","monarchy");
            law(c,country,"citizenship","subjecthood");
        }
        law(c,"B","migration","migration_controls");
        if(!church.empty()&&!dated) {
            if(scope==Scope::national)law(c,"B","church_and_state",church);
            else if(scope==Scope::foreign)law(c,"A","church_and_state",church);
            else {
                P2BaseLaw lookalike;
                lookalike.id="NOT-CHURCH";lookalike.country="B";
                lookalike.domain="custom_faith_policy";lookalike.base_choice="law_"+church;
                lookalike.scope=scope==Scope::local?"TARGET":"national";
                lookalike.legal_basis="fixture:noncanonical";
                if(scope==Scope::local)lookalike.tier=P2NormTier::local_ordinance;
                c.p2_seed_base(lookalike);
            }
        }
        c.p2_seed_culture({"primary",{"heritage"},{"heritage-family"},{"language"},
                            {"language-family"},{},{}});
        // Subjecthood: exact heritage +20 and language family +15 = 35.
        c.p2_seed_culture({"migrant",{"heritage"},{"heritage-family"},{"other-language"},
                            {"language-family"},{},{}});
        c.p2_seed_culture({"independent",{"other-heritage"},{},{"independent-language"},
                            {},{},{}});
        c.p2_seed_religion({"REF-1",{"shared-trait"},{"family-1"}});
        c.p2_seed_religion({"REF-2",{"different-trait"},{"family-2"}});
        // Direct trait via REF-1, family via REF-2. Count only one branch.
        c.p2_seed_religion({"TRAIT",{"shared-trait"},{"family-2"}});
        c.p2_seed_religion({"FAMILY",{"no-direct-trait"},{"family-1","family-2"}});
        c.p2_seed_religion({"UNRELATED",{"unshared-trait"},{"unshared-family"}});
        const std::set<std::string> refs=empty_references?
            std::set<std::string>{}:std::set<std::string>{"REF-1","REF-2"};
        c.p2_seed_national_identity({"B",{"primary"},refs});
        c.p2_seed_national_identity({"A",{"primary"},{"REF-1","REF-2"}});
        auto& pop=c.seed_population();
        const auto from=pop.household_profile({"SOURCE","shared","shared",50'000});
        const auto to=pop.household_profile({"TARGET","shared","shared",0});
        const auto e=pop.economic_profile({"SOURCE","worker","wage","employee",
                                            pop.default_education()});
        const auto seed=[&](const char* faith,const char* culture) {
            const auto d=pop.demographic_profile({"SOURCE",23640,phaseb::Sex::male,
                culture,faith,"citizen","free",pop.default_health(),phaseb::Service::civilian});
            return pop.seed_population(d,e,from,100'000);
        };
        constexpr std::array faiths{"REF-2","TRAIT","FAMILY","UNRELATED","UNKNOWN"};
        std::array<Id,faiths.size()> slices{};
        for(std::size_t i=0;i<faiths.size();++i)slices[i]=seed(faiths[i],"migrant");
        const auto low=seed("UNRELATED","independent");
        const auto high=seed("REF-2","primary");
        phaseb::DemographyRules rules;
        rules.fertility_ppm=rules.infant_death_ppm=rules.child_death_ppm=0;
        rules.adult_death_ppm=rules.senior_death_ppm=rules.very_old_death_ppm=0;
        rules.illness_incidence_ppm=rules.accident_ppm=0;
        rules.primary_access_ppm=rules.secondary_access_ppm=rules.tertiary_access_ppm=0;
        rules.adult_literacy_access_ppm=rules.training_ppm=0;
        rules.migration_ppm=310'000;rules.destination_by_region["SOURCE"]="TARGET";
        c.enable_demography(rules);
        auto world=daily::World::adopt(std::move(gh),19);
        constexpr Scores neutral{35,35,35,35,35};
        const auto check_scores=[&](const Scores& expected) {
            const auto& live=view(world);
            for(std::size_t i=0;i<slices.size();++i) {
                const auto score=live.p2_acceptance("B",slices[i]);
                if(!score||score->score!=expected[i]||
                   live.p2_migration_permitted(slices[i],"TARGET")!=(expected[i]>=40))
                    throw std::runtime_error("church="+church+" candidate="+faiths[i]+
                        " expected="+std::to_string(expected[i])+
                        " observed="+std::to_string(score?score->score:-1));
                const auto original=live.p2_acceptance("A",slices[i]);
                constexpr Scores foreign_freedom{55,50,40,35,35};
                check(original&&original->score==(scope==Scope::foreign?foreign_freedom[i]:35),
                    "church law was not isolated to the country evaluating the candidate");
            }
            const auto low_score=live.p2_acceptance("B",low);
            const auto high_score=live.p2_acceptance("B",high);
            check(low_score&&low_score->score==std::max(0,expected[3]-35)&&
                  high_score&&high_score->score==std::clamp(100+expected[0]-35,0,100),
                  "religious acceptance failed lower/upper score clamp");
        };
        const auto arrival_counts=[&](const Scores& expected,std::size_t opening) {
            const auto& people=view(world).people();
            std::array<People,faiths.size()> arrived{};
            for(std::size_t i=opening;i<people.events().size();++i) {
                const auto& event=people.events()[i];
                if(event.kind!=phaseb::EventKind::migration)continue;
                const auto& d=people.demographic(people.slice(event.to).key.demographic);
                if(d.region!="TARGET"||d.culture!="migrant")continue;
                for(std::size_t j=0;j<faiths.size();++j)
                    if(d.religion==faiths[j])arrived[j]+=event.count;
            }
            for(std::size_t i=0;i<faiths.size();++i)
                check(arrived[i]==(expected[i]>=40?1000:0),
                      "religious acceptance did not gate actual destination arrivals");
            check(people.population()==700'000,"church law changed real total headcount");
        };
        if(dated) {
            check_scores(neutral);
            const auto day=world.day_index();
            const auto opening=view(world).people().events().size();
            phasegh::World::Plan submit;
            submit[9].push_back([day,church](phasegh::Stage& stage) {
                auto& live=stage.phase_c();
                P2Bill bill;
                bill.id="CHURCH-BILL";bill.country="B";bill.proposer="PRIME";
                bill.legal_basis="fixture:legislation";bill.effective_day=day+2;
                bill.action=P2BillAction::enact_or_replace_base;
                bill.proposed_base.id="CHURCH";bill.proposed_base.country="B";
                bill.proposed_base.domain="church_and_state";bill.proposed_base.scope="national";
                bill.proposed_base.base_choice="law_"+church;
                bill.proposed_base.legal_basis="fixture:church";
                live.p2_submit_bill(std::move(bill),day);
                live.p2_approve_bill("CHURCH-BILL",{});
                live.p2_promulgate_bill("CHURCH-BILL","HEAD",day);
            });
            world.advance_day(submit);world.advance_day();
            check_scores(neutral);arrival_counts(neutral,opening);
            auto replay=daily::SnapshotCodec::decode(daily::SnapshotCodec::encode(world));
            world.advance_day();replay.advance_day();
            check_scores(points);arrival_counts(points,opening);
            check(daily::SnapshotCodec::encode(world)==daily::SnapshotCodec::encode(replay),
                  "dated religious acceptance failed deterministic actual migration replay");
            // The otherwise matched unrelated POP is still at 35: property
            // and both ledgers roll back if explicit entry is attempted.
            const auto saved=daily::SnapshotCodec::encode(world);
            phasegh::World::Plan denied;
            denied[10].push_back([id=slices[3],to](phasegh::Stage& stage) {
                stage.phase_c().migrate_with_explicit_property_allocation(
                    id,7,"TARGET",to,"A","B",11,"",0);
            });
            check(rejects([&]{world.advance_day(denied);})&&
                  daily::SnapshotCodec::encode(world)==saved,
                  "unrelated religion bypassed lawful explicit border/property rollback");
        } else {
            check_scores(points);
            const auto opening=view(world).people().events().size();
            world.advance_day();
            arrival_counts(points,opening);
        }
    };
    run("",{35,35,35,35,35});
    run("state_religion",{60,35,25,10,35});
    run("caesaropapism",{60,35,25,10,35});
    run("millet_system",{60,45,45,10,35});
    run("people_of_the_book",{60,40,40,10,35});
    run("freedom_of_conscience",{55,50,40,35,35});
    run("total_separation",{50,50,50,50,35});
    run("state_atheism",{50,35,30,25,35});
    run("state_religion",{10,10,10,10,35},Scope::national,true);
    run("freedom_of_conscience",{35,35,35,35,35},Scope::foreign);
    run("freedom_of_conscience",{35,35,35,35,35},Scope::custom);
    run("freedom_of_conscience",{35,35,35,35,35},Scope::local);
    run("freedom_of_conscience",{55,50,40,35,35},Scope::national,false,true);
}

static void religious_migration_amendment_changes_acceptance_not_border_law() {
    using Scores=std::array<int,5>;
    enum class Scenario { dated, open, no_church, source_only, wrong_parent, local };
    const auto run=[](Scenario mode) {
        phasegh::World gh{24000};
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"HOME","A");geo.seed_unit(1,"A","ORIGIN");
        geo.seed_province(2,"DEST","B");geo.seed_unit(2,"B","DESTINATION");
        geo.seed_assignment(1,1);geo.seed_assignment(2,2);
        auto& industry=geo.seed_core();
        industry.seed_technology({"rationalism",{},false});
        industry.seed_knowledge("B","rationalism",phased::KnowledgeLevel::usable);
        auto& c=industry.seed_core();
        const bool open=mode==Scenario::open||mode==Scenario::no_church;
        for(const char* country:{"A","B"}) {
            c.seed_actor(country,100);c.seed_government(country,"HEAD","PRIME",5,3);
            c.seed_p1_constitution(country,{P1GovernancePrinciple::monarchy,
                P1PowerDistribution::autocracy,P1LegislatureKind::none,
                "fixture:constitution"});
            law(c,country,"citizenship","subjecthood");
            law(c,country,"migration",std::string_view(country)=="A"||open?
                "no_migration_controls":"migration_controls");
        }
        if(mode!=Scenario::no_church)law(c,"B","church_and_state",
            mode==Scenario::wrong_parent?"state_religion":"freedom_of_conscience");
        std::string parent=mode==Scenario::source_only?"A:migration":"B:migration";
        if(mode==Scenario::wrong_parent)parent="B:church_and_state";
        if(mode==Scenario::local) {
            P2BaseLaw custom;
            custom.id="LOCAL";custom.country="B";custom.domain="custom_border";
            custom.scope="DEST";custom.tier=P2NormTier::local_ordinance;
            custom.base_choice="local_border";custom.legal_basis="fixture:local";
            c.p2_seed_base(custom);
            parent="LOCAL";
        }
        const bool from_only=mode==Scenario::source_only;
        P2Amendment amendment;
        amendment.id="amendment_migration_religious_restrictions";
        amendment.law_id=parent;amendment.legal_basis="fixture:religious_restrictions";
        // Nonconflicting synthetic clause carries the tested native source ID;
        // it does not add a migration ban or change a vote.
        amendment.clauses.push_back({"AMENDMENT-CLAUSE",from_only?"A":"B",
                                      P2ClauseKind::political_association_allowed,1});
        if(mode!=Scenario::dated)c.p2_seed_amendment(amendment);
        c.p2_seed_culture({"primary",{"heritage"},{"heritage-family"},{"language"},
                           {"language-family"},{},{}});
        c.p2_seed_culture({"migrant",{"heritage"},{"heritage-family"},{"other-language"},
                           {"language-family"},{},{}});
        c.p2_seed_culture({"independent",{"other-heritage"},{},{"other-language"},{},{},{}});
        c.p2_seed_religion({"REF",{"common-trait"},{"common-family"}});
        c.p2_seed_religion({"TRAIT",{"common-trait"},{"common-family"}});
        c.p2_seed_religion({"FAMILY",{"other-trait"},{"common-family"}});
        c.p2_seed_religion({"UNRELATED",{"other-trait"},{"other-family"}});
        for(const char* country:{"A","B"})
            c.p2_seed_national_identity({country,{"primary"},{"REF"}});
        auto& pop=c.seed_population();
        const auto h=pop.household_profile({"HOME","shared","shared",50'000});
        const auto destination=pop.household_profile({"DEST","shared","shared",0});
        const auto e=pop.economic_profile({"HOME","worker","wage","employee",
                                            pop.default_education()});
        constexpr std::array faiths{"REF","TRAIT","FAMILY","UNRELATED","UNKNOWN"};
        std::array<Id,faiths.size()> slices{};
        const auto seed=[&](const char* culture,const char* faith) {
            const auto d=pop.demographic_profile({"HOME",23640,phaseb::Sex::male,
                culture,faith,"citizen","free",pop.default_health(),phaseb::Service::civilian});
            return pop.seed_population(d,e,h,100'000);
        };
        for(std::size_t i=0;i<faiths.size();++i)slices[i]=seed("migrant",faiths[i]);
        const auto upper=seed("primary","TRAIT");
        const auto lower=seed("independent","UNRELATED");
        phaseb::DemographyRules rules;
        rules.fertility_ppm=rules.infant_death_ppm=rules.child_death_ppm=0;
        rules.adult_death_ppm=rules.senior_death_ppm=rules.very_old_death_ppm=0;
        rules.illness_incidence_ppm=rules.accident_ppm=0;
        rules.primary_access_ppm=rules.secondary_access_ppm=rules.tertiary_access_ppm=0;
        rules.adult_literacy_access_ppm=rules.training_ppm=0;
        rules.migration_ppm=310'000;rules.destination_by_region["HOME"]="DEST";
        c.enable_demography(rules);
        auto world=daily::World::adopt(std::move(gh),19);
        constexpr Scores normal{55,50,40,35,35}, restricted{55,45,35,30,35};
        const auto scores=[&](const Scores& expected) {
            const auto& core=view(world);
            check(core.p2_selected_law("B","migration")==
                  (open?"no_migration_controls":"migration_controls"),
                  "religious acceptance changed the controlling border law");
            for(std::size_t i=0;i<slices.size();++i) {
                const auto candidate=core.p2_acceptance("B",slices[i]);
                if(!candidate||candidate->score!=expected[i]||
                   core.p2_migration_permitted(slices[i],"DEST")!=(open||expected[i]>=40))
                    throw std::runtime_error("religious migration amendment mode="+
                        std::to_string(static_cast<int>(mode))+" faith="+faiths[i]+
                        " expected="+std::to_string(expected[i])+" actual="+
                        std::to_string(candidate?candidate->score:-1));
                const auto origin=core.p2_acceptance("A",slices[i]);
                check(origin&&origin->score==(from_only&&i>=1&&i<=3?30:35),
                      "religious migration amendment affected the wrong evaluating country");
            }
            const auto upper_score=core.p2_acceptance("B",upper);
            const auto lower_score=core.p2_acceptance("B",lower);
            check(upper_score&&upper_score->score==std::clamp(100+expected[1]-35,0,100)&&
                  lower_score&&lower_score->score==std::max(0,expected[3]-35),
                  "religious amendment points must precede the existing final score clamp");
        };
        const auto observe=[&](const Scores& expected,phasegh::World::Plan plan={}) {
            const auto opening=view(world).people().events().size();
            world.advance_day(plan);
            scores(expected);
            const auto& people=view(world).people();
            std::array<People,faiths.size()> arrivals{};
            for(std::size_t i=opening;i<people.events().size();++i) {
                const auto& event=people.events()[i];
                if(event.kind!=phaseb::EventKind::migration)continue;
                const auto& d=people.demographic(people.slice(event.to).key.demographic);
                if(d.region!="DEST"||d.culture!="migrant")continue;
                for(std::size_t j=0;j<faiths.size();++j)
                    if(d.religion==faiths[j])arrivals[j]+=event.count;
            }
            for(std::size_t i=0;i<faiths.size();++i)
                check((arrivals[i]>0)==(open||expected[i]>=40),
                      "religious acceptance did not decide real daily arrivals");
            check(people.population()==700'000,"migration admission lost or created people");
        };
        if(mode==Scenario::dated) {
            scores(normal);
            const auto day=world.day_index();
            phasegh::World::Plan enact;
            enact[9].push_back([day,amendment](phasegh::Stage& stage) {
                auto& live=stage.phase_c();
                P2Bill bill;
                bill.id="RELIGIOUS-RESTRICTIONS";bill.country="B";bill.proposer="PRIME";
                bill.action=P2BillAction::attach_amendment;bill.effective_day=day+2;
                bill.legal_basis="fixture:legislation";bill.proposed_amendment=amendment;
                live.p2_submit_bill(std::move(bill),day);
                live.p2_approve_bill("RELIGIOUS-RESTRICTIONS",{});
                live.p2_promulgate_bill("RELIGIOUS-RESTRICTIONS","HEAD",day);
            });
            observe(normal,std::move(enact));
            observe(normal);
            auto replay=daily::SnapshotCodec::decode(daily::SnapshotCodec::encode(world));
            observe(restricted);replay.advance_day();
            const auto saved=daily::SnapshotCodec::encode(world);
            check(daily::SnapshotCodec::encode(replay)==saved,
                  "religious acceptance effective date failed save/replay");
            phasegh::World::Plan denied;
            denied[10].push_back([id=slices[2],destination](phasegh::Stage& stage) {
                stage.phase_c().migrate_with_explicit_property_allocation(
                    id,7,"DEST",destination,"A","B",11,"",0);
            });
            check(rejects([&]{world.advance_day(denied);})&&
                  daily::SnapshotCodec::encode(world)==saved,
                  "religious family below 40 bypassed the existing atomic border rule");
        } else observe(mode==Scenario::open?restricted:
                      mode==Scenario::no_church?Scores{35,30,30,30,35}:
                      mode==Scenario::wrong_parent?Scores{60,35,25,10,35}:normal);
    };
    run(Scenario::dated);
    run(Scenario::open);
    run(Scenario::no_church);
    run(Scenario::source_only);
    run(Scenario::wrong_parent);
    run(Scenario::local);
}

static void physical_migration_and_law_block() {
    Scenario closed("closed_borders");
    auto denied=daily::World::adopt(std::move(closed.gh),19);
    const auto before=daily::SnapshotCodec::encode(denied);
    phasegh::World::Plan blocked;
    const auto foreign=closed.foreign,household=closed.foreign_household;
    blocked[10].push_back([foreign,household](phasegh::Stage& stage) {
        stage.phase_c().migrate_with_explicit_property_allocation(
            foreign,4,"RB",household,"A","B",20,"",0);
    });
    check(rejects([&]{denied.advance_day(blocked);}) &&
          daily::SnapshotCodec::encode(denied)==before,
          "closed borders must roll back physical POP/cash migration");

    Scenario open("migration_controls");
    auto permitted=daily::World::adopt(std::move(open.gh),19);
    const auto count=view(permitted).people().population();
    const auto startCashA=view(permitted).cash("A");
    const auto startCashB=view(permitted).cash("B");
    phasegh::World::Plan plan;
    const auto permittedPop=open.foreign;
    const auto dest=open.foreign_household;
    plan[10].push_back([permittedPop,dest](phasegh::Stage& stage) {
        stage.phase_c().migrate_with_explicit_property_allocation(
            permittedPop,4,"RB",dest,"A","B",20,"",0);
    });
    permitted.advance_day(plan);
    check(view(permitted).people().population()==count&&
          view(permitted).cash("A")==startCashA-20&&
          view(permitted).cash("B")==startCashB+20,
          "lawful migration failed to preserve real headcount and money");
    auto snapshot=daily::SnapshotCodec::encode(permitted);
    auto replay=daily::SnapshotCodec::decode(snapshot);
    check(daily::SnapshotCodec::encode(replay)==snapshot,
          "actual permitted cross-border POP/property transfer not saved");

    Scenario bound("migration_controls");
    check(!bound.gh.base().core().core().p2_migration_permitted(
             bound.serf,"RB"),
          "serfdom not restricting actual bonded agricultural mobility");
}
static void abolition_preserves_real_people_and_membership() {
    Scenario s;
    auto world=daily::World::adopt(std::move(s.gh),22);
    const auto initialPeople=view(world).people().population();
    const auto initCash=view(world).cash("A");
    phasegh::World::Plan action;
    const auto slave=s.slave;
    action[9].push_back([slave](phasegh::Stage& stage){
        stage.phase_c().join_interest_group(slave,"WORKERS",5);
    });
    world.advance_day(action);
    const auto& c=view(world);
    bool bonded=false,freed=false,serfStill=false;
    for(const auto& [id,row]:c.people().slices()) {
        const auto& pop=c.people().demographic(row.key.demographic);
        if(pop.region!="RA")continue;
        if(pop.legal_status=="slave")bonded=true;
        if(pop.legal_status=="serf")serfStill=true;
        if(pop.legal_status=="free"&&
           c.group_members(id,"WORKERS")==5)freed=true;
    }
    check(!bonded&&freed&&serfStill&&
          c.people().population()==initialPeople&&
          c.cash("A")==initCash,
          "abolition created POP/money or lost emancipation movement membership");
    auto bytes=daily::SnapshotCodec::encode(world);
    check(daily::SnapshotCodec::encode(
        daily::SnapshotCodec::decode(bytes))==bytes,
          "new legal-status transition lost saving deterministic");
}
static void economic_law_restricts_real_company_ownership() {
    // Source 1.15 forbids Command Economy while serfdom remains enacted.
    // Use lawful Tenant Farmers instead of an impossible mixed-law scenario.
    Scenario fixture("migration_controls","ethnostate",true,false);
    auto& c=fixture.gh.seed_geography().seed_core().seed_core();
    c.seed_actor("OWNER",100);
    c.seed_actor("PRIVATE",100);
    c.seed_production_facility("MILL","OWNER","RA",10);
    c.seed_shares("company:OWNER","OWNER",10);
    law(c,"A","economic_system","command_economy");
    fixture.gh.validate();
    auto world=daily::World::adopt(std::move(fixture.gh),77);
    const auto before=daily::SnapshotCodec::encode(world);
    phasegh::World::Plan forbidden;
    forbidden[7].push_back([](phasegh::Stage& stage) {
        stage.phase_c().sell_shares("company:OWNER","OWNER","PRIVATE",2);
    });
    check(rejects([&]{world.advance_day(forbidden);})&&
          daily::SnapshotCodec::encode(world)==before,
          "command economy illegally transferred physical company to private buyer");
    phasegh::World::Plan state_acquisition;
    state_acquisition[7].push_back([](phasegh::Stage& stage) {
        stage.phase_c().sell_shares("company:OWNER","OWNER","A",2);
    });
    world.advance_day(state_acquisition);
    check(view(world).shares("company:OWNER","OWNER")==8&&
          view(world).shares("company:OWNER","A")==2,
          "real state acquisition under command economy did not settle shares");
    const auto saved=daily::SnapshotCodec::encode(world);
    check(daily::SnapshotCodec::encode(
        daily::SnapshotCodec::decode(saved))==saved,
        "state-owned company title changed on checkpoint replay");
}

static void military_peasant_levy_restricts_real_conscripts() {
    for(const std::string model:{"peasant_levies","warrior_caste","mass_conscription"}) {
        const bool restricted=model!="mass_conscription";
        Scenario fixture;
        auto& core=fixture.gh.seed_geography().seed_core().seed_core();
        core.seed_unit("LEVY");
        core.seed_army_deployment("LEVY","A","RA");
        law(core,"A","army_model",model);
        P2BaseLaw conscription;
        conscription.id="A:lawful_conscription";
        conscription.country="A";conscription.domain="conscription_fixture";
        conscription.scope="national";conscription.base_choice="military-law";
        conscription.legal_basis="fixture:conscription";
        conscription.clauses.push_back({
            "conscription-permitted","A",
            P2ClauseKind::conscription_authorized,1});
        core.p2_seed_base(conscription);
        auto& pop=core.seed_population();
        const auto row=pop.slice(fixture.primary);
        const auto economic=pop.economic_profile({
            "RA","peasant","subsistence","self_employed",pop.default_education()});
        const auto proxy_economic=pop.economic_profile({
            "RA","laborer","subsistence","self_employed",pop.default_education()});
        const auto peasant=pop.seed_population(
            row.key.demographic,economic,row.key.household,100,7001);
        const auto proxy=pop.seed_population(
            row.key.demographic,proxy_economic,row.key.household,100,7002);
        const auto foreign=pop.slice(fixture.other);
        const auto foreign_economic=pop.economic_profile({
            "RB","peasant","subsistence","self_employed",pop.default_education()});
        const auto foreign_peasant=pop.seed_population(
            foreign.key.demographic,foreign_economic,foreign.key.household,100,7003);
        auto active=pop.demographic(row.key.demographic);
        active.service=phaseb::Service::active;
        const auto standing=pop.seed_population(pop.demographic_profile(active),
            proxy_economic,row.key.household,3,7004);
        core.seed_active("LEVY",standing);
        PeacetimeMilitaryProgram program;
        program.unit="LEVY";program.country="A";
        program.standing_target=4;program.daily_enlist_limit=1;
        core.set_peacetime_military_program(program);
        fixture.gh.validate();
        auto world=daily::World::adopt(std::move(fixture.gh),12);
        const auto before=daily::SnapshotCodec::encode(world);
        const auto draft=[](phaseb::Id source) {
            phasegh::World::Plan plan;
            plan[10].push_back([source](phasegh::Stage& gh) {
                gh.phase_c().mobilize(source,1,"LEVY");
            });
            return plan;
        };
        if(restricted) {
            auto scheduled=world;
            phasegh::World::Plan staff;
            staff[10].push_back([](phasegh::Stage& gh) {
                (void)gh.phase_c().peacetime_military_staff("LEVY");
            });
            scheduled.advance_day(staff);
            const auto& result=view(scheduled);
            check(result.active_strength("LEVY")==4&&
                  result.unit_slices("LEVY").at(standing)==3&&
                  result.people().population()==view(world).people().population(),
                  "automatic draft must skip disallowed POP without losing standing force");
            for(const auto& [id,n]:result.unit_slices("LEVY")) {
                if(id==standing)continue;
                const auto& drafted=result.people().slice(id);
                check(n==1&&result.people().economic(drafted.key.economic).profession=="peasant",
                      "automatic draft bypassed exact peasant profession");
            }
            check(rejects([&]{world.advance_day(draft(proxy));})&&
                  daily::SnapshotCodec::encode(world)==before,
                  "the peasant draft used livelihood instead of actual POP profession");
            check(rejects([&]{world.advance_day(draft(foreign_peasant));})&&
                  daily::SnapshotCodec::encode(world)==before,
                  "foreign jurisdiction bypassed the conscripting country's law");
        }
        const auto total=view(world).people().population();
        world.advance_day(draft(restricted?peasant:proxy));
        check(view(world).people().population()==total&&
              view(world).active_strength("LEVY")==4,
              "legal draft did not conserve standing troops and actual POP");
        auto saved=daily::SnapshotCodec::encode(world);
        world=daily::SnapshotCodec::decode(saved);
        check(daily::SnapshotCodec::encode(world)==saved,
              "verified conscripts did not survive save and reload");
        phasegh::World::Plan leave;
        leave[10].push_back([standing](phasegh::Stage& gh) {
            gh.phase_c().demobilize("LEVY",standing,1);
        });
        world.advance_day(leave);
        check(view(world).active_strength("LEVY")==3&&
              view(world).people().population()==total,
              "the draft qualification must not interfere with real discharge");
    }
}

static void effective_workhouse_amendment_reduces_only_welfare_cost(
        const std::string& amendment_id,int invalid_scope=0) {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"R","A");
    geo.seed_unit(1,"A","ADMIN");
    geo.seed_assignment(1,1);
    auto& c=geo.seed_core().seed_core();
    c.seed_actor("A",1000);
    c.seed_actor("SERVICES",1000);
    c.seed_government("A","HEAD","PRIME",5,3);
    c.seed_p1_constitution("A",{P1GovernancePrinciple::monarchy,
        P1PowerDistribution::autocracy,P1LegislatureKind::none,
        "test:welfare-authority"});
    c.p1_set_incorporation("R",true);
    auto& pop=c.seed_population();
    const auto hh=pop.household_profile({"R","mixed","shared",25'000});
    const auto worker=pop.economic_profile({
        "R","worker","wage","employee",pop.default_education()});
    const auto dem=pop.demographic_profile({
        "R",23650,phaseb::Sex::female,"culture","faith","citizen",
        "free",pop.default_health(),phaseb::Service::civilian});
    pop.seed_population(dem,worker,hh,100'000);
    c.seed_public_institution("WELFARE","SERVICES","R","food",100,1);
    c.seed_p1_institution("A","WELFARE",P1InstitutionKind::welfare,2);
    c.seed_public_institution("POLICE","SERVICES","R","paper",100,1);
    c.seed_p1_institution("A","POLICE",P1InstitutionKind::policing,1);
    law(c,"A","welfare","poor_laws");
    if(amendment_id=="amendment_religious_workhouses")
        law(c,"A","health_system","charitable_health_system");
    else
        law(c,"A","labor_rights","regulatory_bodies");
    if(invalid_scope) {
        auto parent=amendment_id=="amendment_religious_workhouses"?
            std::string("A:health_system"):std::string("A:labor_rights");
        if(invalid_scope==2) {
            P2BaseLaw local{"LOCAL","A","custom_welfare_rule","R",
                "law_poor_laws","fixture:non-national ordinance"};
            local.tier=P2NormTier::local_ordinance;
            c.p2_seed_base(local);
            parent="LOCAL";
        }
        c.p2_seed_amendment({amendment_id,parent,"fixture:incorrect source","",
            {{"configured-level","WELFARE",P2ClauseKind::public_institution_level,2}}});
    }
    auto world=daily::World::adopt(std::move(gh),42);
    const auto state=[&]() -> const IntegratedWorld& {
        return world.engine().base().core().core();
    };
    if(invalid_scope) {
        check(state().p2_amendment_active(amendment_id)&&
              state().p1_bureaucracy("A").institution_cost==30,
              "active non-welfare or local workhouse lookalike altered real institution cost");
        return;
    }
    check(state().p1_bureaucracy("A").institution_cost==30,
          "base welfare (20) and police (10) must be charged normally");
    phasegh::World::Plan enact;
    const auto start=world.day_index();
    enact[9].push_back([&](phasegh::Stage& stage) {
        auto& live=stage.phase_c();
        P2Bill bill;
        bill.id="WORKHOUSE-BILL";
        bill.country="A";bill.proposer="PRIME";bill.legal_basis="test:workhouse";
        bill.action=P2BillAction::attach_amendment;
        bill.effective_day=start+2;
        bill.proposed_amendment={amendment_id,"A:welfare",
            "test:workhouse","",{{"welfare-level","WELFARE",
                P2ClauseKind::public_institution_level,2}}};
        live.p2_submit_bill(std::move(bill),start);
        live.p2_approve_bill("WORKHOUSE-BILL",{});
        live.p2_promulgate_bill("WORKHOUSE-BILL","HEAD",start);
    });
    world.advance_day(enact);
    world.advance_day();
    check(!state().p2_amendment_active(amendment_id)&&
          state().p1_bureaucracy("A").institution_cost==30,
          "not-yet-effective workhouse amendment changed bureaucracy");
    world.advance_day();
    static const std::string key=
        "country_institution_cost_institution_social_security_mult";
    check(state().p2_amendment_active(amendment_id)&&
          state().p2_modifier_ppm("A",key)==-100'000,
          "canonical amendment modifier did not become effective");
    check(state().p1_bureaucracy("A").institution_cost==28,
          "effective workhouse law must lower welfare cost to 18 and leave police at 10");
    check(politics::wired_law_effects().at(key)==
              "phasec::IntegratedWorld::p1_bureaucracy",
          "welfare cost consumer missing from law coverage audit");
}

static void institutional_bureaucracy_prices_ongoing_commitments() {
    const auto scenario=[](std::string bureaucracy,bool attach,
                           int expected,const char* mismatch) {
        phasegh::World gh(24000);
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"R","A");
        geo.seed_unit(1,"A","ADMIN");
        geo.seed_assignment(1,1);
        auto& c=geo.seed_core().seed_core();
        c.seed_actor("A",1000);
        c.seed_actor("SERVICES",1000);
        c.seed_government("A","HEAD","PRIME",5,3);
        c.seed_p1_constitution("A",{P1GovernancePrinciple::monarchy,
            P1PowerDistribution::autocracy,P1LegislatureKind::none,
            "scenario:law"});
        c.p1_set_incorporation("R",true);
        auto& pop=c.seed_population();
        const auto hh=pop.household_profile({"R","mixed","shared",25'000});
        const auto worker=pop.economic_profile({
            "R","worker","wage","employee",pop.default_education()});
        const auto dem=pop.demographic_profile({
            "R",23650,phaseb::Sex::female,"culture","faith","citizen",
            "free",pop.default_health(),phaseb::Service::civilian});
        const auto slice=pop.seed_population(dem,worker,hh,1'000'000);
        pop.hire(slice,10,"SERVICES","worker",10,{});
        c.seed_public_institution("SCHOOL","SERVICES","R","paper",100,1);
        c.seed_p1_institution("A","SCHOOL",P1InstitutionKind::education,3);
        c.seed_public_institution("CLINIC","SERVICES","R","food",100,1);
        c.seed_p1_institution("A","CLINIC",P1InstitutionKind::healthcare,1);
        law(c,"A","education_system","private_schools");
        law(c,"A","bureaucracy",bureaucracy);
        const std::string amendment="amendment_bourgeois_specialists";
        if(attach)
            c.p2_seed_amendment({amendment,"A:bureaucracy","scenario:law","",
                {{"unchanged-school-level","SCHOOL",
                  P2ClauseKind::public_institution_level,3}}});
        auto world=daily::World::adopt(std::move(gh),42);
        const auto state=[&]() -> const IntegratedWorld& {
            return world.engine().base().core().core();
        };
        const auto verify=[&] {
            const auto v=state().p1_bureaucracy("A");
            const auto svc=state().p1_institution_capacity("SCHOOL");
            check(v.institution_cost==expected,mismatch);
            check(v.capacity.committed==v.state_cost+expected&&
                  v.office_output==0&&v.nationalized_cost==0&&
                  v.commander_cost==0&&
                  svc.level==3&&svc.employed_staff==10&&svc.cash_backed,
                  "institution price altered unrelated commitments or staffing");
            return v.state_cost;
        };
        const auto state_cost=verify();
        const auto snap=daily::SnapshotCodec::encode(world);
        world=daily::SnapshotCodec::decode(snap);
        check(daily::SnapshotCodec::encode(world)==snap&&verify()==state_cost,
              "institution bureaucracy did not retain canonical source on reload");
        return state_cost;
    };
    const auto baseline=scenario("appointed_bureaucrats",false,280,
        "private-school specific cost must precede the test");
    check(scenario("elected_bureaucrats",false,220,
                   "elected bureaucrats must reduce ongoing institution cost")==
          baseline,"national institution discount changed independent state cost");
    check(scenario("appointed_bureaucrats",true,280,
                   "authored origin is not the allowed Bourgeois parent")==
          baseline,"invalid Bourgeois parent altered state cost");
    const auto communal=scenario("communal_administration",false,280,
        "communal population discount must not reduce institution cost");
    check(scenario("communal_administration",true,260,
                   "compatible Bourgeois amendment must reduce institution cost")==
          communal,"Bourgeois institution discount changed independent state cost");
}

static void ordinary_police_do_not_receive_home_affairs_operating_discounts() {
    const auto run=[](const char* security,bool amend,bool general,int nominal_slots) {
        phasegh::World gh(24000);
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"R","A");
        geo.seed_unit(1,"A","ADMIN");
        geo.seed_assignment(1,1);
        auto& c=geo.seed_core().seed_core();
        c.seed_actor("A",1000);
        c.seed_actor("OPERATOR",0);
        c.seed_actor("WORKERS",0);
        c.seed_government("A","HEAD","PRIME",5,3);
        c.seed_p1_constitution("A",{P1GovernancePrinciple::monarchy,
            P1PowerDistribution::autocracy,P1LegislatureKind::none,"fixture:constitution"});
        c.p1_set_incorporation("R",true);
        auto& pop=c.seed_population();
        const auto hh=pop.household_profile({"R","mixed","shared",25'000});
        const auto worker=pop.economic_profile({"R","worker","wage","employee",
                                                 pop.default_education()});
        const auto dem=pop.demographic_profile({"R",23650,phaseb::Sex::female,
            "culture","faith","citizen","free",pop.default_health(),phaseb::Service::civilian});
        const auto slice=pop.seed_population(dem,worker,hh,1'000'000);
        pop.hire(slice,10,"OPERATOR","worker",10,{});
        c.seed_public_institution("POLICE","OPERATOR","R","paper",100,1);
        c.seed_p1_institution("A","POLICE",P1InstitutionKind::policing,2);
        law(c,"A","policing","dedicated_police");
        law(c,"A","internal_security",security);
        if(general)law(c,"A","bureaucracy","elected_bureaucrats");
        c.seed_interest_group("WORKERS-IG");
        c.p3_seed_group({"A","WORKERS-IG","","","",P3GroupType::custom,0,0});
        c.p3_seed_movement({"MOV","A","fixture:ordinary","reform","R",
            P3MovementGoal::legal_reform,{"WORKERS-IG"},{},0,0,true});
        auto world=daily::World::adopt(std::move(gh),42);
        const auto start=world.day_index();
        phasegh::World::Plan first;
        first[5].push_back([](phasegh::Stage& stage) {
            auto& live=stage.phase_c();
            live.spend_country_budget("A","OPERATOR",100);
            live.wages("OPERATOR","WORKERS","A",61,0);
        });
        first[9].push_back([&](phasegh::Stage& stage) {
            auto& live=stage.phase_c();
            live.p5_perform_action({"POLICE-RESPONSE","MOV","","R","POLICE",
                P5ActionKind::policing,0,5,0,start});
            if(!amend)return;
            P2Bill bill;
            bill.id="SHINSENGUMI-AMENDMENT";bill.country="A";bill.proposer="PRIME";
            bill.legal_basis="fixture:secret-police-amendment";bill.effective_day=start+2;
            bill.action=P2BillAction::attach_amendment;
            bill.proposed_amendment={"amendment_reformed_shinsengumi","A:internal_security",
                "fixture:amendment","",{{"association","A",
                    P2ClauseKind::political_association_allowed,1}}};
            live.p2_submit_bill(std::move(bill),start);
            live.p2_approve_bill("SHINSENGUMI-AMENDMENT",{});
            live.p2_promulgate_bill("SHINSENGUMI-AMENDMENT","HEAD",start);
        });
        world.advance_day(first);
        const auto live=[&]() -> const IntegratedWorld& {
            return world.engine().base().core().core();
        };
        constexpr auto wrong_key="country_institution_cost_institution_home_affairs_mult";
        const auto verify=[&](int authored_ppm) {
            const auto v=live().p1_bureaucracy("A");
            const auto s=live().p1_institution_capacity("POLICE");
            const auto expected=general?170:200;
            check(live().p2_modifier_ppm("A",wrong_key)==authored_ppm,
                  "test must keep the genuine Home Affairs source effective where authored");
            if(v.institution_cost!=expected)
                throw std::runtime_error(std::string(security)+(amend?" + enacted amendment":"")+
                    ": Home Affairs rate changed ordinary police administration: expected "+
                    std::to_string(expected)+" got "+std::to_string(v.institution_cost));
            check(v.capacity.committed==v.state_cost+expected&&s.level==2&&
                  s.employed_staff==10&&s.usable_slots==nominal_slots&&s.cash_backed&&
                  live().cash("A")==900&&live().cash("OPERATOR")==39&&
                  live().cash("WORKERS")==61&&live().p5_actions().size()==1,
                  "scope fix interfered with real funded police, standard costs, or responses");
        };
        if(amend) {
            verify(0);
            world.advance_day();
            verify(0);
            auto replay=daily::SnapshotCodec::decode(daily::SnapshotCodec::encode(world));
            world.advance_day();replay.advance_day();
            check(live().p2_amendment_active("amendment_reformed_shinsengumi")&&
                  daily::SnapshotCodec::encode(world)==daily::SnapshotCodec::encode(replay),
                  "the legal Secret Police amendment failed its dated scope or replay");
            verify(-50'000);
        } else verify(std::string_view(security)=="third_section"?-250'000:0);
    };
    run("national_guard",false,false,10);
    run("no_home_affairs",false,false,8);
    run("third_section",false,false,10);
    run("third_section",false,true,10);
    run("secret_police",true,false,10);
    run("secret_police",true,true,10);
}

static void national_school_and_health_laws_change_legal_administration_cost() {
    // 0 eligible; 1 local; 2 custom domain; 3 wrong parent; 4 foreign; 5 no school; 6 higher unfunded level.
    const auto run=[](const char* group,const char* choice,const char* amendment,
                     int scope,int expected) {
        phasegh::World gh(24000);
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"R","A");
        geo.seed_unit(1,"A","ADMIN");
        geo.seed_assignment(1,1);
        if(scope==4) {
            geo.seed_province(2,"S","B");
            geo.seed_unit(2,"B","B-ADMIN");
            geo.seed_assignment(2,2);
        }
        auto& c=geo.seed_core().seed_core();
        c.seed_actor("A",0);
        c.seed_actor("SERVICES",1000);
        if(scope==4) {
            c.seed_actor("B",1000);
            c.seed_government("B","B-HEAD","B-PRIME",5,3);
            c.seed_p1_constitution("B",{P1GovernancePrinciple::monarchy,
                P1PowerDistribution::autocracy,P1LegislatureKind::none,
                "scenario:cost"});
        }
        c.seed_government("A","HEAD","PRIME",5,3);
        c.seed_p1_constitution("A",{P1GovernancePrinciple::monarchy,
            P1PowerDistribution::autocracy,P1LegislatureKind::none,
            "scenario:law"});
        c.p1_set_incorporation("R",true);
        auto& pop=c.seed_population();
        const auto hh=pop.household_profile({"R","mixed","shared",25'000});
        const auto worker=pop.economic_profile({
            "R","worker","wage","employee",pop.default_education()});
        const auto dem=pop.demographic_profile({
            "R",23650,phaseb::Sex::female,"culture","faith","citizen",
            "free",pop.default_health(),phaseb::Service::civilian});
        const auto slice=pop.seed_population(dem,worker,hh,1'000'000);
        pop.hire(slice,10,"SERVICES","worker",10,{});
        if(scope!=5) {
            c.seed_public_institution("SCHOOL","SERVICES","R","paper",100,1);
            c.seed_p1_institution("A","SCHOOL",P1InstitutionKind::education,2);
            c.seed_actor("UNFUNDED",0);
            c.seed_public_institution("SECOND","UNFUNDED","R","paper",100,1);
            c.seed_p1_institution("A","SECOND",P1InstitutionKind::education,scope==6?3:1);
        }
        c.seed_public_institution("POLICE","SERVICES","R","paper",100,1);
        c.seed_p1_institution("A","POLICE",P1InstitutionKind::policing,1);
        c.seed_public_institution("CLINIC","SERVICES","R","food",100,1);
        c.seed_p1_institution("A","CLINIC",P1InstitutionKind::healthcare,1);
        P2BaseLaw base{"cost-fixture",scope==4?"B":"A",scope==1||scope==2?"local_costs":group,
            scope==1?"R":"national",choice,"scenario:cost"};
        if(scope==1)base.tier=P2NormTier::local_ordinance;
        c.p2_seed_base(base);
        if(*amendment)c.p2_seed_amendment({amendment,base.id,"scenario:cost","",
            {{"existing-rule","A",P2ClauseKind::political_association_allowed,1}}});
        auto world=daily::World::adopt(std::move(gh),42);
        const auto& live=world.engine().base().core().core();
        const auto v=live.p1_bureaucracy("A");
        const auto clinic=live.p1_institution_capacity("CLINIC");
        if(v.institution_cost!=expected)
            throw std::runtime_error(std::string("incorrect committed institution cost: ")+
                choice+" "+amendment+" scope "+std::to_string(scope)+" expected "+
                std::to_string(expected)+" got "+std::to_string(v.institution_cost));
        check(v.capacity.committed==v.state_cost+expected&&clinic.level==1&&
              clinic.employed_staff==10&&clinic.cash_backed&&
              live.cash("SERVICES")==1000&&live.cash("A")==0,
              "law affected unrelated obligations, service staff or real cash");
        if(scope!=5) {
            const auto school=live.p1_institution_capacity("SCHOOL");
            const auto second=live.p1_institution_capacity("SECOND");
            check(school.level==2&&school.employed_staff==10&&school.cash_backed&&
                  second.level==(scope==6?3:1)&&second.employed_staff==0&&!second.cash_backed&&
                  live.cash("UNFUNDED")==0,
                  "cost uses max legal level, not summed delivery or payer cash");
        }
    };
    constexpr auto school="education_system",health="health_system";
    run(school,"religious_schools","",1,400);
    run(health,"charitable_health_system","",2,400);
    run(school,"religious_schools","",0,360);
    run(school,"private_schools","",0,320);
    run(health,"charitable_health_system","",0,380);
    run(health,"private_health_insurance","",0,360);
    run("childrens_rights","compulsory_primary_school","amendment_ragged_schools",0,380);
    run(school,"public_schools","amendment_co_ed_schools",0,380);
    run(school,"public_schools","amendment_single_sex_education",0,420);
    run(school,"public_schools","amendment_provided_school_supplies",0,410);
    run(school,"public_schools","amendment_unfunded_school_supplies",0,390);
    run(school,"public_schools","amendment_secular_school_system",0,420);
    run(school,"public_schools","amendment_secular_school_system",2,400);
    run(school,"private_schools","amendment_unfunded_school_supplies",3,320);
    run(school,"private_schools","",4,400);
    run(school,"religious_schools","",5,200);
    run(school,"religious_schools","",6,440);
    run(school,"religious_schools","amendment_provided_school_supplies",0,370);
}

enum class BlasphemyParent { valid, local, custom_national, incompatible };

static daily::World mortality_fixture(
        const std::string& health_law,int legal_level,bool funded,
        bool staffed,bool blasphemy=false,bool empty_scope=false,
        bool staff_outside_clinic=false,
        BlasphemyParent parent=BlasphemyParent::valid) {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    const std::vector<std::string> regions={"CLINIC","CORE","DEPENDENCY","UNKNOWN"};
    geo.seed_unit(1,"A","ADMIN");
    for(std::size_t i=0;i<regions.size();++i) {
        geo.seed_province(i+1,regions[i],"A");
        geo.seed_assignment(i+1,1);
    }
    auto& c=geo.seed_core().seed_core();
    c.seed_actor("A",funded?100:0);
    c.seed_actor("HOSPITAL",funded?100:0);
    c.seed_government("A","HEAD","PRIME",5,3);
    c.seed_p1_constitution("A",{P1GovernancePrinciple::theocracy,
        P1PowerDistribution::autocracy,P1LegislatureKind::none,"scenario:law"});
    c.p1_set_incorporation("CLINIC",true);
    c.p1_set_incorporation("CORE",true);
    c.p1_set_incorporation("DEPENDENCY",false);
    auto& pop=c.seed_population();
    for(const auto& region:regions) {
        const auto h=pop.household_profile({region,"mixed","shared",25'000});
        const auto e=pop.economic_profile({region,"worker","wage","employee",
                                         pop.default_education()});
        const auto d=pop.demographic_profile({region,23650,phaseb::Sex::male,
            "culture","faith","citizen","free",pop.default_health(),
            phaseb::Service::civilian});
        const auto slice=pop.seed_population(d,e,h,100'000);
        if(staffed&&region==(staff_outside_clinic?"CORE":"CLINIC"))
            pop.hire(slice,10,"HOSPITAL","worker",10,{});
    }
    law(c,"A","governance_principles","theocracy");
    law(c,"A","church_and_state","state_religion");
    if(!health_law.empty()) {
        if(empty_scope)
            c.p2_seed_base({"A:health_system","A","lawgroup_health_system","",
                "law_"+health_law,"scenario:constitution"});
        else law(c,"A","health_system",health_law);
    }
    if(legal_level>=0) {
        c.seed_public_institution("HEALTH","HOSPITAL","CLINIC","medicine",50,5);
        c.seed_p1_institution("A","HEALTH",P1InstitutionKind::healthcare,5);
        if(health_law!="")
            c.p2_seed_amendment({"effective-health-funding","A:health_system",
                "scenario:budget","",{{"approved-health-level","HEALTH",
                    P2ClauseKind::public_institution_level,legal_level}}});
    }
    if(blasphemy) {
        std::string parent_id="A:church_and_state";
        if(parent==BlasphemyParent::local||parent==BlasphemyParent::custom_national) {
            const bool local=parent==BlasphemyParent::local;
            P2BaseLaw misleading{"INVALID_PARENT","A","custom_public_order",
                local?"CLINIC":"national","law_state_religion","scenario:law"};
            if(local)misleading.tier=P2NormTier::local_ordinance;
            c.p2_seed_base(std::move(misleading));
            parent_id="INVALID_PARENT";
        } else if(parent==BlasphemyParent::incompatible) {
            parent_id="A:health_system";
        }
        c.p2_seed_amendment({"amendment_draconian_blasphemy_laws",
            parent_id,"scenario:law","",{{"civil-law","A",
                P2ClauseKind::political_association_allowed,1}}});
    }
    phaseb::DemographyRules rates;
    rates.fertility_ppm=0;
    rates.adult_death_ppm=310'000; // January: exactly 10,000 ppm/day.
    rates.illness_incidence_ppm=0;
    rates.accident_ppm=0;
    rates.migration_ppm=0;
    c.enable_demography(rates);
    return daily::World::adopt(std::move(gh),42);
}
static phaseb::People residents(const daily::World& world,const std::string& region) {
    const auto& pop=world.engine().base().core().core().people();
    phaseb::People total=0;
    for(const auto& [id,slice]:pop.slices()) {
        (void)id;
        if(pop.demographic(slice.key.demographic).region==region)
            total+=slice.count;
    }
    return total;
}
static void childrens_rights_changes_only_exact_profession_mortality() {
    const auto run=[](const std::string& choice,bool stacked,
                      const std::array<phaseb::People,7>& expected) {
        phasegh::World gh(24000);
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"HOME","A");geo.seed_province(2,"FOREIGN","B");
        geo.seed_unit(1,"A","ROOT");geo.seed_unit(2,"B","ROOT");
        geo.seed_assignment(1,1);geo.seed_assignment(2,2);
        auto& c=geo.seed_core().seed_core();
        for(const auto* owner:{"A","B"}) {
            c.seed_actor(owner,100);
            c.seed_government(owner,"HEAD","PRIME",5,3);
            c.seed_p1_constitution(owner,{P1GovernancePrinciple::theocracy,
                P1PowerDistribution::autocracy,P1LegislatureKind::none,"scenario:law"});
        }
        c.seed_actor("EMPLOYER",101);
        if(!choice.empty())law(c,"A","childrens_rights",choice);
        if(stacked) {
            law(c,"A","governance_principles","theocracy");
            law(c,"A","church_and_state","state_religion");
            c.p2_seed_amendment({"amendment_draconian_blasphemy_laws",
                "A:church_and_state","scenario:law","",{{"civil-law","A",
                    P2ClauseKind::political_association_allowed,1}}});
        }
        const std::array<std::string,7> professions{
            "laborer","machinist","farmer","peasant","clerk","worker","labourer"};
        auto& pop=c.seed_population();
        for(const auto* region:{"HOME","FOREIGN"}) {
            const auto d=pop.demographic_profile({region,23650,phaseb::Sex::male,
                "culture","faith","citizen","free",pop.default_health(),
                phaseb::Service::civilian});
            const auto h=pop.household_profile({region,"mixed","shared",25'000});
            for(const auto& profession:professions) {
                const auto e=pop.economic_profile({region,profession,"wage","employee",
                                                   pop.default_education()});
                const auto slice=pop.seed_population(d,e,h,100'000);
                pop.hire(slice,1,"EMPLOYER",profession,1,{});
            }
        }
        phaseb::DemographyRules rules;
        rules.fertility_ppm=0;rules.adult_death_ppm=310'000;
        rules.illness_incidence_ppm=0;rules.accident_ppm=0;rules.migration_ppm=0;
        c.enable_demography(rules);
        auto world=daily::World::adopt(std::move(gh),43);
        world.advance_day();
        const auto& live=world.engine().base().core().core();
        const auto& people=live.people();
        for(const auto* region:{"HOME","FOREIGN"}) {
            std::size_t seen=0;
            for(const auto& [id,slice]:people.slices()) {
                if(people.demographic(slice.key.demographic).region!=region)continue;
                const auto& profession=people.economic(slice.key.economic).profession;
                const auto found=std::find(professions.begin(),professions.end(),profession);
                check(found!=professions.end(),"mortality invented/reassigned profession");
                const auto idx=static_cast<std::size_t>(found-professions.begin());
                const auto deaths=std::string_view(region)=="HOME"?expected[idx]:1000;
                check(slice.count==100'000-deaths && people.employed(id)==1,
                      "occupation or jurisdiction mortality differs from exact effective law");
                ++seen;
            }
            check(seen==professions.size(),"occupation test lost a real POP cohort");
        }
        check(live.cash("EMPLOYER")==101 && live.cash("A")==100 && live.cash("B")==100,
              "mortality changed employer or treasury cash");
    };
    run("",false,{1000,1000,1000,1000,1000,1000,1000});
    run("child_labor_allowed",false,{1050,1050,1050,1050,1000,1000,1000});
    run("restricted_child_labor",false,{1020,1000,1020,1020,1000,1000,1000});
    run("child_labor_allowed",true,{1070,1070,1070,1070,1020,1020,1020});
}

static void effective_health_institution_and_flat_law_change_daily_mortality() {
    const auto fixture=[](const std::string& law,int level,bool funded,bool staffed,
                          bool flat,phaseb::People expected_core,
                          phaseb::People expected_outside) {
        auto world=mortality_fixture(law,level,funded,staffed,flat);
        world.advance_day();
        check(residents(world,"DEPENDENCY")==100'000-expected_outside &&
              residents(world,"UNKNOWN")==100'000-expected_outside,
              "health institution leaked outside known incorporated territory or flat law was lost");
        check(residents(world,"CLINIC")==100'000-expected_core &&
              residents(world,"CORE")==100'000-expected_core,
              "effective national health law did not change actual mortality at its legal level");
    };
    fixture("no_health_system",2,true,true,false,1000,1000);
    fixture("charitable_health_system",2,true,true,false,940,1000);
    fixture("public_health_insurance",2,true,true,false,900,1000);
    fixture("public_health_insurance",0,true,true,false,1000,1000);
    fixture("public_health_insurance",2,false,true,false,1000,1000);
    fixture("public_health_insurance",2,true,false,false,1000,1000);
    fixture("public_health_insurance",-1,true,true,false,1000,1000);
    fixture("",-1,false,false,true,1020,1020);
    fixture("public_health_insurance",2,true,true,true,920,1020);
    auto implicit_national=mortality_fixture("public_health_insurance",2,
        true,true,false,true);
    implicit_national.advance_day();
    check(residents(implicit_national,"CORE")==99'100,
          "supported empty national scope lost healthcare mortality benefit");
    auto nonlocal_staff=mortality_fixture("public_health_insurance",2,
        true,true,false,false,true);
    nonlocal_staff.advance_day();
    check(residents(nonlocal_staff,"CORE")==99'000 &&
          residents(nonlocal_staff,"CLINIC")==99'000,
          "a clinic without local effective staff certified national healthcare");

    for(const auto parent:{BlasphemyParent::local,BlasphemyParent::custom_national,
                           BlasphemyParent::incompatible}) {
        auto invalid=mortality_fixture("no_health_system",-1,false,false,
                                        true,false,false,parent);
        check(invalid.engine().base().core().core().p2_amendment_active(
                  "amendment_draconian_blasphemy_laws"),
              "fixture did not retain a permissively recorded amendment");
        invalid.advance_day();
        check(residents(invalid,"CORE")==99'000&&
              residents(invalid,"UNKNOWN")==99'000,
              "invalid national parent incorrectly altered actual flat mortality");
    }

    auto world=mortality_fixture("",-1,false,false);
    const auto start=world.day_index();
    phasegh::World::Plan enact;
    enact[9].push_back([start](phasegh::Stage& stage) {
        auto& c=stage.phase_c();
        P2Bill bill;
        bill.id="BLASPHEMY";bill.country="A";bill.proposer="PRIME";
        bill.legal_basis="scenario:act";bill.action=P2BillAction::attach_amendment;
        bill.effective_day=start+2;
        bill.proposed_amendment={"amendment_draconian_blasphemy_laws",
            "A:church_and_state","scenario:act","",{{"civil-law","A",
                P2ClauseKind::political_association_allowed,1}}};
        c.p2_submit_bill(std::move(bill),start);
        c.p2_approve_bill("BLASPHEMY",{});
        c.p2_promulgate_bill("BLASPHEMY","HEAD",start);
    });
    world.advance_day(enact);
    world.advance_day();
    check(residents(world,"UNKNOWN")==98'010,
          "pending flat amendment changed mortality before legal effect");
    auto replay=daily::SnapshotCodec::decode(daily::SnapshotCodec::encode(world));
    world.advance_day();replay.advance_day();
    check(residents(world,"UNKNOWN")==97'011 &&
          daily::SnapshotCodec::encode(world)==daily::SnapshotCodec::encode(replay),
          "dated flat mortality effect did not start on actual effective day or survive replay");
}

static void enacted_education_changes_real_daily_schooling() {
    auto scenario=[](const std::string& law_choice,int legal_level,
                     bool funded,const std::string& staff_region,
                     phaseb::People expected_core,phaseb::People expected_other) {
        phasegh::World gh(24000);
        auto& geo=gh.seed_geography();
        const std::vector<std::string> regions={"SCHOOL","CORE","DEPENDENCY","UNKNOWN"};
        geo.seed_unit(1,"A","ADMIN");
        for(std::size_t i=0;i<regions.size();++i) {
            geo.seed_province(i+1,regions[i],"A");
            geo.seed_assignment(i+1,1);
        }
        auto& c=geo.seed_core().seed_core();
        c.seed_actor("A",funded?100:0);
        c.seed_actor("TEACHERS",funded?100:0);
        c.seed_government("A","HEAD","PRIME",5,3);
        c.seed_p1_constitution("A",{P1GovernancePrinciple::monarchy,
            P1PowerDistribution::autocracy,P1LegislatureKind::none,"scenario:school"});
        c.p1_set_incorporation("SCHOOL",true);
        c.p1_set_incorporation("CORE",true);
        c.p1_set_incorporation("DEPENDENCY",false);
        auto& pop=c.seed_population();
        for(const auto& region:regions) {
            const auto h=pop.household_profile({region,"mixed","shared",25'000});
            const auto e=pop.economic_profile({region,"not_assigned","inactive",
                                              "ineligible",pop.default_education()});
            const auto d=pop.demographic_profile({region,23904,phaseb::Sex::male,
                "culture","faith","citizen","free",pop.default_health(),
                phaseb::Service::civilian});
            pop.seed_population(d,e,h,100'000);
            if(region==staff_region) {
                const auto paid=pop.economic_profile({region,"teacher","wage","employee",
                                                     pop.default_education()});
                const auto adult=pop.demographic_profile({region,23651,phaseb::Sex::male,
                    "culture","faith","citizen","free",pop.default_health(),
                    phaseb::Service::civilian});
                const auto staff=pop.seed_population(adult,paid,h,10);
                pop.hire(staff,10,"TEACHERS","teacher",10,{});
            }
        }
        law(c,"A","education_system",law_choice);
        if(legal_level>=0) {
            c.seed_public_institution("SCHOOLS","TEACHERS","SCHOOL","paper",50,5);
            c.seed_p1_institution("A","SCHOOLS",P1InstitutionKind::education,5);
            c.p2_seed_amendment({"effective-school-budget","A:education_system",
                "scenario:budget","",{{"approved-school-level","SCHOOLS",
                    P2ClauseKind::public_institution_level,legal_level}}});
        }
        phaseb::DemographyRules rates;
        rates.fertility_ppm=0;
        rates.child_death_ppm=0;
        rates.adult_death_ppm=0;
        rates.illness_incidence_ppm=0;
        rates.accident_ppm=0;
        rates.training_ppm=0;
        rates.primary_access_ppm=310'000;
        c.enable_demography(rates);
        auto world=daily::World::adopt(std::move(gh),43);
        auto replay=daily::SnapshotCodec::decode(daily::SnapshotCodec::encode(world));
        world.advance_day();
        replay.advance_day();
        check(daily::SnapshotCodec::encode(world)==daily::SnapshotCodec::encode(replay),
              "education law hazard was not reproducible after reload");
        const auto& people=world.engine().base().core().core().people();
        std::map<std::string,phaseb::People> primary;
        for(const auto& [id,row]:people.slices()) {
            (void)id;
            const auto& dem=people.demographic(row.key.demographic);
            const auto& economic=people.economic(row.key.economic);
            if(dem.birth_month==23904&&
               people.education(economic.education).school==phaseb::School::primary)
                primary[dem.region]+=row.count;
        }
        check(primary["DEPENDENCY"]==expected_other&&primary["UNKNOWN"]==expected_other,
              "education institution leaked outside incorporated areas or lost existing flat access");
        check(primary["SCHOOL"]==expected_core&&primary["CORE"]==expected_core,
              "effective national education law did not advance actual cohorts at its legal level");
    };
    scenario("no_schools",2,true,"SCHOOL",0,0);
    scenario("religious_schools",2,true,"SCHOOL",1645,1000);
    scenario("public_schools",2,true,"SCHOOL",1806,1000);
    scenario("public_schools",0,true,"SCHOOL",1000,1000);
    scenario("public_schools",-1,true,"SCHOOL",1000,1000);
    scenario("public_schools",2,false,"SCHOOL",1000,1000);
    scenario("public_schools",2,true,"",1000,1000);
    scenario("public_schools",2,true,"CORE",1000,1000);
    scenario("terakoya",2,true,"SCHOOL",1806,1806);
}

static void school_staff_capacity_requires_workers_in_the_school_region() {
    const auto run=[](int local,int distant,phaseb::People seats,
                      phaseb::People graduates) {
        phasegh::World opening(24000);
        auto& geo=opening.seed_geography();
        geo.seed_unit(1,"A","A-ADMIN");
        geo.seed_province(1,"SCHOOL","A");geo.seed_assignment(1,1);
        geo.seed_province(2,"DISTANT","A");geo.seed_assignment(2,1);
        auto& c=geo.seed_core().seed_core();
        c.seed_actor("A",100);
        c.seed_actor("TEACHERS",100);
        c.seed_government("A","HEAD","PRIME",5,3);
        c.seed_p1_constitution("A",{P1GovernancePrinciple::monarchy,
            P1PowerDistribution::autocracy,P1LegislatureKind::none,"scenario:school"});
        c.p1_set_incorporation("SCHOOL",true);
        c.p1_set_incorporation("DISTANT",true);
        c.seed_interest_group("Devout");
        c.p3_seed_group({"A","Devout","","","",P3GroupType::devout,0,0});
        c.p3_set_ideologies("A","Devout",{"materialist"});
        law(c,"A","education_system","religious_schools");
        c.seed_public_institution("SCHOOLS","TEACHERS","SCHOOL","paper",50,1);
        c.seed_p1_institution("A","SCHOOLS",P1InstitutionKind::education,2);
        auto& pop=c.seed_population();
        phaseb::Id member{};
        std::vector<std::pair<std::string,Money>> payroll;
        for(const auto* region:{"SCHOOL","DISTANT"}) {
            const bool at_school=std::string_view(region)=="SCHOOL";
            const auto home=pop.household_profile({region,"mixed","shared",100});
            const auto youth=pop.economic_profile({region,"not_assigned","inactive",
                                                   "ineligible",pop.default_education()});
            const auto child=pop.demographic_profile({region,23904,phaseb::Sex::male,
                "culture","faith","citizen","free",pop.default_health(),phaseb::Service::civilian});
            if(at_school)pop.seed_population(child,youth,home,100'000);
            const auto adult=pop.demographic_profile({region,23651,phaseb::Sex::male,
                "culture","faith","citizen","free",pop.default_health(),phaseb::Service::civilian});
            const auto trained=pop.economic_profile({region,"teacher","wage","employee",
                                                      pop.default_education()});
            const auto n=at_school?local:distant;
            if(n) {
                const auto employee=pop.seed_population(adult,trained,home,n);
                pop.hire(employee,n,"TEACHERS","teacher",31,{});
                const auto wallet="HH:"+std::to_string(home);
                c.seed_actor(wallet,0);
                payroll.emplace_back(wallet,n);
            }
            if(at_school) {
                const auto observer=pop.economic_profile({region,"observer","wage","unemployed",
                                                           pop.default_education()});
                member=pop.seed_population(adult,observer,home,100);
            }
        }
        phaseb::DemographyRules rates;
        rates.fertility_ppm=0; rates.child_death_ppm=0; rates.adult_death_ppm=0;
        rates.illness_incidence_ppm=0; rates.accident_ppm=0; rates.training_ppm=0;
        rates.primary_access_ppm=310'000;
        c.enable_demography(rates);
        auto world=daily::World::adopt(std::move(opening),43);
        phasegh::World::Plan plan;
        plan[5].push_back([payroll](phasegh::Stage& stage) {
            for(const auto& [wallet,gross]:payroll)
                stage.phase_c().wages_under_law("TEACHERS",wallet,"A",gross,"A");
        });
        plan[9].push_back([member](phasegh::Stage& stage) {
            auto& core=stage.phase_c();
            core.join_interest_group(member,"Devout",100);
            check(core.p3_trait_modifier_ppm("A","state_education_access_add")==-100'000,
                  "fixture did not activate the existing Devout school delivery penalty");
        });
        auto replay=daily::SnapshotCodec::decode(daily::SnapshotCodec::encode(world));
        world.advance_day(plan);replay.advance_day(plan);
        const auto& core=world.engine().base().core().core();
        check(daily::SnapshotCodec::encode(world)==daily::SnapshotCodec::encode(replay),
              "same-school staff coverage was not reproducible after reload");
        phaseb::People educated=0;
        for(const auto& [id,row]:core.people().slices()) {
            (void)id;
            const auto& dem=core.people().demographic(row.key.demographic);
            const auto& econ=core.people().economic(row.key.economic);
            if(dem.region=="SCHOOL"&&dem.birth_month==23904&&
               core.people().education(econ.education).school==phaseb::School::primary)
                educated+=row.count;
        }
        const auto capacity=core.p1_institution_capacity("SCHOOLS");
        if(capacity.usable_slots!=seats||educated!=graduates)
            throw std::runtime_error("school seats/actual graduates: "+
                std::to_string(capacity.usable_slots)+"/"+std::to_string(educated)+
                " expected "+std::to_string(seats)+"/"+std::to_string(graduates));
        check(core.cash("TEACHERS")==98&&core.cash("A")==100,
              "school's same-day wage postings changed between matched payrolls");
        for(const auto& [wallet,gross]:payroll)
            check(core.cash(wallet)==gross,"actual school employment wages were not paid");
        check(core.people().population()==100'102&&capacity.cash_backed&&capacity.level==2,
              "school staffing changed population, funding or statutory level");
    };
    run(2,0,1,1322);
    run(1,1,0,677);
}

static void chiefs_aid_uses_only_effective_welfare_bureaucracy() {
    const auto scenario=[](const std::string& choice,P1InstitutionKind kind,
                           const std::string& service_region,
                           const std::string& staff_region,
                           bool funded,bool dated,std::int64_t expected) {
        phasegh::World gh(24000);
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"CORE","A");
        geo.seed_province(2,"DEPENDENCY","A");
        geo.seed_province(3,"FOREIGN","B");
        geo.seed_unit(1,"A","A-ADMIN");geo.seed_unit(2,"B","B-ADMIN");
        geo.seed_assignment(1,1);geo.seed_assignment(2,1);geo.seed_assignment(3,2);
        auto& c=geo.seed_core().seed_core();
        c.seed_actor("A",0);c.seed_actor("B",0);
        c.seed_actor("SERVICES",funded?100:0);
        c.seed_government("A","HEAD","PRIME",5,3);
        c.seed_government("B","HEAD-B","PRIME-B",5,3);
        c.seed_p1_constitution("A",{P1GovernancePrinciple::monarchy,
            P1PowerDistribution::autocracy,P1LegislatureKind::none,
            "fixture:executive"});
        c.seed_p1_constitution("B",{P1GovernancePrinciple::monarchy,
            P1PowerDistribution::autocracy,P1LegislatureKind::none,
            "fixture:foreign-executive"});
        c.p1_set_incorporation("CORE",true);
        c.p1_set_incorporation("DEPENDENCY",false);
        c.p1_set_incorporation("FOREIGN",true);
        auto& pop=c.seed_population();
        for(const auto& region:{"CORE","DEPENDENCY","FOREIGN"}) {
            const auto hh=pop.household_profile({region,"mixed","shared",25'000});
            const auto econ=pop.economic_profile({region,"worker","wage","employee",
                                                 pop.default_education()});
            const auto dem=pop.demographic_profile({region,23650,phaseb::Sex::female,
                "culture","faith","citizen","free",pop.default_health(),
                phaseb::Service::civilian});
            const auto id=pop.seed_population(dem,econ,hh,100'000);
            if(region==staff_region)pop.hire(id,10,"SERVICES","worker",10,{});
        }
        const auto country=service_region=="FOREIGN"?"B":"A";
        c.seed_public_institution("AID-ONE","SERVICES",service_region,"food",100,1);
        c.seed_public_institution("AID-TWO","SERVICES",service_region,"food",100,1);
        c.seed_p1_institution(country,"AID-ONE",kind,1);
        c.seed_p1_institution(country,"AID-TWO",kind,2);
        if(!choice.empty())law(c,"A","welfare",dated?"poor_laws":choice);
        auto world=daily::World::adopt(std::move(gh),42);
        const auto& init=world.engine().base().core().core();
        const auto opening_cost=init.p1_bureaucracy("A").institution_cost;
        if(dated) {
            const auto day=world.day_index();
            phasegh::World::Plan plan;
            plan[9].push_back([&](phasegh::Stage& stage) {
                auto& live=stage.phase_c();
                phasec::P2Bill bill;
                bill.id="CHIEFS-BILL";bill.country="A";bill.proposer="PRIME";
                bill.legal_basis="fixture:chiefs";bill.effective_day=day+2;
                bill.proposed_base={"CHIEFS-LAW","A","lawgroup_welfare","national",
                    "law_chiefs_distribute_aid","fixture:chiefs"};
                live.p2_submit_bill(std::move(bill),day);
                live.p2_approve_bill("CHIEFS-BILL",{});
                live.p2_promulgate_bill("CHIEFS-BILL","HEAD",day);
            });
            world.advance_day(plan);
            world.advance_day();
            check(world.engine().base().core().core().p1_bureaucracy("A")
                      .capacity.generated==100,
                  "pending chiefs law created administrative output early");
            world.advance_day();
        }
        const auto& live=world.engine().base().core().core();
        const auto value=live.p1_bureaucracy("A");
        if(value.capacity.generated!=expected)
            throw std::runtime_error("wrong chiefs aid administration output for "+
                choice+"/"+service_region+" (dated="+std::to_string(dated)+
                "): expected "+std::to_string(expected)+", got "+
                std::to_string(value.capacity.generated));
        check(value.institution_cost==opening_cost &&
              live.p2_modifier_ppm("A","country_bureaucracy_add")==0,
              "institutional output altered nominal cost or leaked into flat modifiers");
    };
    scenario("chiefs_distribute_aid",P1InstitutionKind::welfare,"CORE","CORE",true,false,120);
    scenario("chiefs_distribute_aid",P1InstitutionKind::welfare,"CORE","CORE",true,true,120);
    scenario("poor_laws",P1InstitutionKind::welfare,"CORE","CORE",true,false,100);
    scenario("",P1InstitutionKind::welfare,"CORE","CORE",true,false,100);
    scenario("chiefs_distribute_aid",P1InstitutionKind::welfare,"CORE","",true,false,100);
    scenario("chiefs_distribute_aid",P1InstitutionKind::welfare,"CORE","CORE",false,false,100);
    scenario("chiefs_distribute_aid",P1InstitutionKind::welfare,"CORE","DEPENDENCY",true,false,100);
    scenario("chiefs_distribute_aid",P1InstitutionKind::education,"CORE","CORE",true,false,100);
    scenario("chiefs_distribute_aid",P1InstitutionKind::welfare,"DEPENDENCY","DEPENDENCY",true,false,100);
    scenario("chiefs_distribute_aid",P1InstitutionKind::welfare,"FOREIGN","FOREIGN",true,false,100);
}

static void canonical_named_acceptance_changes_only_verified_pop_and_country() {
    const auto run=[](const std::string& named,const std::string& target,
                      const std::string& citizenship,const std::string& amendment,
                      int before,int after,int invalid_parent=0) {
        phasegh::World gh{24000};
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"SOURCE","ORIGIN");
        geo.seed_province(2,"TARGET",target);
        geo.seed_unit(1,"ORIGIN","SOURCE-UNIT");
        geo.seed_unit(2,target,"TARGET-UNIT");
        geo.seed_assignment(1,1);geo.seed_assignment(2,2);
        auto& c=geo.seed_core().seed_core();
        for(const auto& tag:{std::string("ORIGIN"),target}) {
            c.seed_actor(tag,0);
            c.seed_government(tag,"HEAD","PRIME",5,3);
            c.seed_p1_constitution(tag,{P1GovernancePrinciple::monarchy,
                P1PowerDistribution::autocracy,P1LegislatureKind::none,
                "fixture:constitution"});
            law(c,tag,"citizenship",citizenship);
            law(c,tag,"migration",tag==target?"migration_controls":"no_migration_controls");
        }
        const std::string heritage=named=="basque"?"PRIMARY_H":"OTHER_H";
        c.p2_seed_culture({"primary",{"PRIMARY_H"},{"PRIMARY_HG"},
                            {"PRIMARY_L"},{"PRIMARY_LG"},{},{}});
        c.p2_seed_culture({named,{heritage},{"SHARED_HG"},{"OTHER_L"},{"SHARED_LG"},{},{}});
        const auto alias=named=="basque"?"Basque":"Polish";
        c.p2_seed_culture({alias,{heritage},{"SHARED_HG"},{"OTHER_L"},{"SHARED_LG"},{},{}});
        c.p2_seed_national_identity({"ORIGIN",{"primary"},{}});
        c.p2_seed_national_identity({target,{"primary"},{}});
        auto& pop=c.seed_population();
        const auto h=pop.household_profile({"SOURCE","mixed","shared",25'000});
        const auto e=pop.economic_profile({"SOURCE","worker","wage","employee",
                                            pop.default_education()});
        const auto seed=[&](const std::string& culture) {
            const auto d=pop.demographic_profile({"SOURCE",23640,phaseb::Sex::male,
                culture,"faith","citizen","free",pop.default_health(),
                phaseb::Service::civilian});
            return pop.seed_population(d,e,h,100'000);
        };
        const Id named_slice=seed(named), alias_slice=seed(alias), unknown=seed("unknown");
        phaseb::DemographyRules rules;
        rules.fertility_ppm=rules.infant_death_ppm=rules.child_death_ppm=0;
        rules.adult_death_ppm=rules.senior_death_ppm=rules.very_old_death_ppm=0;
        rules.illness_incidence_ppm=rules.accident_ppm=0;
        rules.primary_access_ppm=rules.secondary_access_ppm=rules.tertiary_access_ppm=0;
        rules.adult_literacy_access_ppm=rules.training_ppm=0;
        rules.migration_ppm=310'000;
        rules.destination_by_region["SOURCE"]="TARGET";
        c.enable_demography(rules);
        std::string parent=target+":citizenship";
        if(invalid_parent==1) {
            P2BaseLaw local;
            local.id="LOCAL";local.country=target;local.domain="regional_rules";
            local.scope="TARGET";local.tier=P2NormTier::local_ordinance;
            local.base_choice="local_rule";local.legal_basis="fixture:local";
            c.p2_seed_base(local);
            parent=local.id;
        } else if(invalid_parent==2)parent=target+":migration";
        const P2Amendment addition{amendment,parent,"fixture:amendment","",
            {{"verified-amendment-clause",target,
              P2ClauseKind::political_association_allowed,1}}};
        if(invalid_parent)c.p2_seed_amendment(addition);
        auto world=daily::World::adopt(std::move(gh),17);
        const auto check_score=[&](int expected,bool allowed) {
            const auto& live=view(world);
            const auto actual=live.p2_acceptance(target,named_slice);
            const auto other=live.p2_acceptance(target,alias_slice);
            const auto origin=live.p2_acceptance("ORIGIN",named_slice);
            const auto level=expected>=80?P2AcceptanceLevel::full:
                expected>=60?P2AcceptanceLevel::second_class:
                expected>=40?P2AcceptanceLevel::open_prejudice:
                expected>=20?P2AcceptanceLevel::cultural_erasure:
                              P2AcceptanceLevel::violent_hostility;
            const auto score=[](const auto& candidate) {
                return candidate?candidate->score:-1;
            };
            if(!(actual&&actual->score==expected&&actual->level==level&&
                 actual->region=="SOURCE"&&other&&other->score==before&&
                 origin&&origin->score==before&&!live.p2_acceptance(target,unknown)))
                throw std::runtime_error("named culture acceptance scope: "+named+
                    ", expected="+std::to_string(expected)+", target="+
                    std::to_string(score(actual))+", decoy="+std::to_string(score(other))+
                    ", origin="+std::to_string(score(origin))+
                    ", date="+std::to_string(world.day_index()));
            check(live.p2_migration_permitted(named_slice,"TARGET")==allowed&&
                  !live.p2_migration_permitted(alias_slice,"TARGET")&&
                  !live.p2_migration_permitted(unknown,"TARGET"),
                  "named culture acceptance did not feed the actual destination gate");
        };
        const auto arrived=[&] {
            const auto& people=view(world).people();
            phaseb::People count=0;
            for(const auto& [id,row]:people.slices()) {
                (void)id;
                const auto& demo=people.demographic(row.key.demographic);
                if(demo.region!="TARGET")continue;
                check(demo.culture==named,"an ineligible culture crossed the controlled border");
                count+=row.count;
            }
            return count;
        };
        check_score(before,false);
        if(invalid_parent) {
            world.advance_day();
            check(arrived()==0,"incompatible or local parent allowed real migration");
            return;
        }
        const auto enact=[&](bool repeal) {
            const auto start=world.day_index();
            phasegh::World::Plan plan;
            plan[9].push_back([&,start,repeal](phasegh::Stage& stage) {
                auto& live=stage.phase_c();
                P2Bill b;
                b.id=repeal?"REPEAL":"ADOPT";b.country=target;b.proposer="PRIME";
                b.legal_basis="fixture:legislation";b.effective_day=start+2;
                b.action=repeal?P2BillAction::repeal_amendment:P2BillAction::attach_amendment;
                if(repeal)b.target_id=amendment;
                else b.proposed_amendment=addition;
                const auto id=b.id;
                live.p2_submit_bill(std::move(b),start);
                live.p2_approve_bill(id,{});
                live.p2_promulgate_bill(id,"HEAD",start);
            });
            world.advance_day(plan);
            world.advance_day();
        };
        enact(false);
        check_score(before,false);
        check(arrived()==0,"pending law changed migration before effective day");
        world.advance_day();
        check_score(after,true);
        check(arrived()==1000,"the named eligible POP did not really cross on effective day");
        const auto bytes=daily::SnapshotCodec::encode(world);
        world=daily::SnapshotCodec::decode(bytes);
        check_score(after,true);
        enact(true);
        const auto before_repeal=arrived();
        world.advance_day();
        check_score(before,false);
        check(arrived()==before_repeal&&view(world).people().population()==300'000,
              "repealed enactment still moved people or violated POP balance");
    };
    run("basque","ESP","subjecthood","amendment_reinstated_fueros",20,40);
    run("polish","HAI","cultural_exclusion","amendment_haiti_polish_naturalization",20,80);
    run("basque","ESP","subjecthood","amendment_reinstated_fueros",20,40,1);
    run("polish","HAI","cultural_exclusion","amendment_haiti_polish_naturalization",20,80,2);
}

static void no_colonial_affairs_adds_only_verified_homeland_acceptance() {
    phasegh::World gh{24000};
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"SOURCE","ORIGIN");
    geo.seed_province(2,"TARGET","DEST");
    geo.seed_province(3,"OTHER","ORIGIN");
    geo.seed_unit(1,"ORIGIN","HOME");
    geo.seed_unit(2,"DEST","DEST-UNIT");
    geo.seed_assignment(1,1);geo.seed_assignment(2,2);geo.seed_assignment(3,1);
    auto& c=geo.seed_core().seed_core();
    for(const auto* tag:{"ORIGIN","DEST"}) {
        c.seed_actor(tag,0);
        c.seed_government(tag,"HEAD","PRIME",5,3);
        c.seed_p1_constitution(tag,{P1GovernancePrinciple::monarchy,
            P1PowerDistribution::autocracy,P1LegislatureKind::none,
            "fixture:constitution"});
        law(c,tag,"citizenship","subjecthood");
        law(c,tag,"colonization","colonial_exploitation");
        law(c,tag,"migration",std::string(tag)=="DEST"?
                            "migration_controls":"no_migration_controls");
    }
    c.p2_seed_culture({"primary",{"primary-H"},{"primary-HG"},
                        {"primary-L"},{"primary-LG"},{},{}});
    c.p2_seed_culture({"homeland",{"H"},{"HG"},{"L"},{"LG"},{},{"SOURCE"}});
    c.p2_seed_culture({"elsewhere",{"H"},{"HG"},{"L"},{"LG"},{},{"OTHER"}});
    c.p2_seed_culture({"unverified-home",{"H"},{"HG"},{"L"},{"LG"},{},{}});
    c.p2_seed_national_identity({"ORIGIN",{"primary"},{}});
    c.p2_seed_national_identity({"DEST",{"primary"},{}});
    auto& pop=c.seed_population();
    const auto h=pop.household_profile({"SOURCE","mixed","shared",100'000});
    const auto e=pop.economic_profile({"SOURCE","worker","wage","employee",
                                       pop.default_education()});
    const auto seed=[&](const std::string& culture) {
        const auto d=pop.demographic_profile({"SOURCE",23640,phaseb::Sex::male,
            culture,"faith","citizen","free",pop.default_health(),
            phaseb::Service::civilian});
        return pop.seed_population(d,e,h,100'000);
    };
    const auto home=seed("homeland"), other=seed("elsewhere");
    const auto no_home=seed("unverified-home"), unknown=seed("missing-traits");
    phaseb::DemographyRules rules;
    rules.fertility_ppm=rules.infant_death_ppm=rules.child_death_ppm=0;
    rules.adult_death_ppm=rules.senior_death_ppm=rules.very_old_death_ppm=0;
    rules.illness_incidence_ppm=rules.accident_ppm=0;
    rules.primary_access_ppm=rules.secondary_access_ppm=rules.tertiary_access_ppm=0;
    rules.adult_literacy_access_ppm=rules.training_ppm=0;
    rules.migration_ppm=310'000;
    rules.destination_by_region["SOURCE"]="TARGET";
    c.enable_demography(rules);
    auto world=daily::World::adopt(std::move(gh),21);
    const auto snapshot_counts=[&](int expected,bool allowed) {
        const auto& live=view(world);
        const auto score=[&](const std::string& tag,Id id) {
            const auto evaluated=live.p2_acceptance(tag,id);
            return evaluated?evaluated->score:-1;
        };
        check(score("DEST",home)==expected&&score("ORIGIN",home)==30&&
              score("DEST",other)==0&&score("DEST",no_home)==0&&
              score("DEST",unknown)==-1,
              "colonization law did not add only verified homeland points in its own country");
        check(live.p2_migration_permitted(home,"TARGET")==allowed&&
              !live.p2_migration_permitted(other,"TARGET")&&
              !live.p2_migration_permitted(no_home,"TARGET")&&
              !live.p2_migration_permitted(unknown,"TARGET"),
              "colonization acceptance did not preserve actual migration control scope");
    };
    const auto arrived=[&] {
        phaseb::People count=0;
        const auto& people=view(world).people();
        for(const auto& [id,row]:people.slices()) {
            (void)id;
            const auto& dem=people.demographic(row.key.demographic);
            if(dem.region!="TARGET")continue;
            check(dem.culture=="homeland","an unverified homeland passed migration control");
            count+=row.count;
        }
        return count;
    };
    const auto replace=[&](const std::string& choice,const std::string& id) {
        const auto start=world.day_index();
        phasegh::World::Plan act;
        act[9].push_back([&](phasegh::Stage& stage) {
            auto& live=stage.phase_c();
            P2Bill bill;
            bill.id=id;bill.country="DEST";bill.proposer="PRIME";
            bill.legal_basis="fixture:enactment";bill.effective_day=start+2;
            bill.action=P2BillAction::enact_or_replace_base;
            bill.proposed_base.id=id+":LAW";bill.proposed_base.country="DEST";
            bill.proposed_base.domain="lawgroup_colonization";
            bill.proposed_base.scope="national";bill.proposed_base.base_choice=choice;
            bill.proposed_base.legal_basis="fixture:colonization";
            live.p2_submit_bill(std::move(bill),start);
            live.p2_approve_bill(id,{});
            live.p2_promulgate_bill(id,"HEAD",start);
        });
        world.advance_day(act);
        world.advance_day();
    };
    snapshot_counts(30,false);
    replace("law_no_colonial_affairs","ADOPT");
    snapshot_counts(30,false);
    check(arrived()==0,"pending colonization law affected movement early");
    world.advance_day();
    snapshot_counts(40,true);
    check(arrived()==1000,"verified homeland POP did not actually move under the enacted law");
    const auto saved=daily::SnapshotCodec::encode(world);
    world=daily::SnapshotCodec::decode(saved);
    snapshot_counts(40,true);
    replace("law_colonial_exploitation","REPLACE");
    const auto previous=arrived();
    world.advance_day();
    snapshot_counts(30,false);
    check(arrived()==previous&&view(world).people().population()==400'000,
          "replaced colonization law retained a stale effect on real migration");
}

static void daily_migration_checks_receiving_law_without_an_origin_government() {
    const auto run=[](bool receiving_government,bool closed) {
        phasegh::World gh{24000};
        auto& geo=gh.seed_geography();
        geo.set_dataset("receiving-border-with-unseeded-origin-government");
        geo.seed_province(1,"SOURCE","A");
        geo.seed_province(2,"TARGET","B");
        geo.seed_unit(1,"A","SOURCE-UNIT");geo.seed_assignment(1,1);
        geo.seed_unit(2,"B","TARGET-UNIT");geo.seed_assignment(2,2);
        auto& c=geo.seed_core().seed_core();
        c.seed_actor("A",50);c.seed_actor("B",0);
        c.seed_shares("company:A","A",10);
        if(receiving_government) {
            c.seed_government("B","HEAD","PRIME",5,3);
            c.seed_p1_constitution("B",{P1GovernancePrinciple::monarchy,
                P1PowerDistribution::autocracy,P1LegislatureKind::none,
                "fixture:constitution"});
            law(c,"B","migration",closed?"closed_borders":"no_migration_controls");
        }
        auto& pop=c.seed_population();
        const auto home=pop.household_profile({"SOURCE","mixed","shared",50});
        const auto away=pop.household_profile({"TARGET","mixed","shared",0});
        const auto dem=pop.demographic_profile({"SOURCE",23640,phaseb::Sex::male,
            "culture","faith","citizen","free",pop.default_health(),
            phaseb::Service::civilian});
        const auto econ=pop.economic_profile({"SOURCE","worker","wage",
            "employee",pop.default_education()});
        const auto worker=pop.seed_population(dem,econ,home,100);
        phaseb::DemographyRules rules;
        rules.fertility_ppm=rules.infant_death_ppm=rules.child_death_ppm=0;
        rules.adult_death_ppm=rules.senior_death_ppm=rules.very_old_death_ppm=0;
        rules.illness_incidence_ppm=rules.accident_ppm=0;
        rules.primary_access_ppm=rules.secondary_access_ppm=rules.tertiary_access_ppm=0;
        rules.adult_literacy_access_ppm=rules.training_ppm=0;
        rules.migration_ppm=310'000; // exactly one worker on January 1
        rules.destination_by_region["SOURCE"]="TARGET";
        c.enable_demography(rules);
        auto world=daily::World::adopt(std::move(gh),19);
        const auto saved=daily::SnapshotCodec::encode(world);
        check(view(world).p2_migration_permitted(worker,"TARGET")==!closed,
              "the explicit migration law did not identify the receiving border");
        if(closed) {
            phasegh::World::Plan explicit_move;
            explicit_move[10].push_back([=](phasegh::Stage& stage) {
                stage.phase_c().migrate_with_explicit_property_allocation(
                    worker,1,"TARGET",away,"A","B",11,"company:A",1);
            });
            auto explicit_world=daily::SnapshotCodec::decode(saved);
            check(rejects([&]{explicit_world.advance_day(explicit_move);})&&
                  daily::SnapshotCodec::encode(explicit_world)==saved,
                  "forbidden explicit migration did not preserve people and assets");
        }
        world.advance_day();
        const auto& live=view(world);
        People source=0,target=0;
        for(const auto& [id,row]:live.people().slices()) {
            const auto& region=live.people().demographic(row.key.demographic).region;
            if(region=="SOURCE")source+=row.count;
            if(region=="TARGET")target+=row.count;
        }
        check(source==(closed?100:99)&&target==(closed?0:1)&&
              live.people().population()==100&&live.people().validate(),
              closed?"daily POP crossed a closed border from a registered origin without a government":
                     "daily POP did not move across a lawful open route");
        check(live.cash("A")==50&&live.cash("B")==0&&
              live.shares("company:A","A")==10&&live.shares("company:A","B")==0,
              "daily movement changed explicit cash or securities custody");
    };
    run(true,true);
    run(true,false);
    run(false,false); // registered route with no political rate rows at all
}

static void effective_receiving_law_opens_actual_daily_migration() {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.set_dataset("fixture:dated-receiving-border");
    geo.seed_province(1,"SOURCE","A");
    geo.seed_province(2,"TARGET","B");
    geo.seed_unit(1,"A","ORIGIN");geo.seed_assignment(1,1);
    geo.seed_unit(2,"B","RECEIVER");geo.seed_assignment(2,2);
    auto& c=geo.seed_core().seed_core();
    c.seed_actor("B",0);
    c.seed_government("B","HEAD","PRIME",5,3);
    c.seed_p1_constitution("B",{P1GovernancePrinciple::monarchy,
        P1PowerDistribution::autocracy,P1LegislatureKind::none,"fixture:constitution"});
    law(c,"B","migration","closed_borders");

    auto& pop=c.seed_population();
    const auto home=pop.household_profile({"SOURCE","mixed","shared",50});
    const auto dem=pop.demographic_profile({"SOURCE",23640,phaseb::Sex::male,
        "culture","faith","citizen","free",pop.default_health(),phaseb::Service::civilian});
    const auto econ=pop.economic_profile({"SOURCE","worker","wage","employee",
                                          pop.default_education()});
    const auto worker=pop.seed_population(dem,econ,home,100);
    const auto wallet="HH:"+std::to_string(home);
    c.seed_actor(wallet,37);
    c.seed_stock(wallet,"SOURCE","grain",6);
    c.seed_shares("company:origin",wallet,10);
    phaseb::DemographyRules rates;
    rates.fertility_ppm=rates.infant_death_ppm=rates.child_death_ppm=0;
    rates.adult_death_ppm=rates.senior_death_ppm=rates.very_old_death_ppm=0;
    rates.illness_incidence_ppm=rates.accident_ppm=0;
    rates.primary_access_ppm=rates.secondary_access_ppm=rates.tertiary_access_ppm=0;
    rates.adult_literacy_access_ppm=rates.training_ppm=0;
    rates.migration_ppm=310'000;
    rates.destination_by_region["SOURCE"]="TARGET";
    c.enable_demography(rates);

    const auto opening=daily::World::adopt(std::move(gh),19);
    const auto save=daily::SnapshotCodec::encode(opening);
    auto reform=daily::SnapshotCodec::decode(save);
    auto closed=daily::SnapshotCodec::decode(save);
    const auto january1=reform.day_index();
    const auto due=january1+2;
    const auto opening_events=view(reform).people().events().size();
    const auto verify=[&](const daily::World& world,bool open) {
        const auto& live=view(world);
        const auto& people=live.people();
        People source=0,target=0;
        phaseb::Id destination{};
        for(const auto& [id,slice]:people.slices()) {
            const auto& d=people.demographic(slice.key.demographic);
            const auto& h=people.household(slice.key.household);
            check(h.region==d.region,"migration must preserve matched household residence");
            if(d.region=="SOURCE") {
                source+=slice.count;
                check(slice.key.household==home,"origin household custody changed");
            } else if(d.region=="TARGET") {
                target+=slice.count;
                destination=id;
                check(slice.key.household!=home&&d.citizenship=="citizen"&&
                      !live.has_actor("HH:"+std::to_string(slice.key.household)),
                      "residence alone must not create a destination financial wallet");
            }
        }
        check(live.sovereignty("SOURCE")=="A"&&live.sovereignty("TARGET")=="B"&&
              live.p2_migration_permitted(worker,"TARGET")==open&&
              source==(open?99:100)&&target==(open?1:0)&&
              people.population()==100&&people.validate(),
              "dated receiving law failed to govern real cross-sovereign residence");
        const auto& events=people.events();
        check(events.size()==opening_events+(open?1:0),
              "dated border created a POP event before lawful native migration");
        if(open) {
            const auto& event=events.back();
            check(event.kind==phaseb::EventKind::migration&&event.from==worker&&
                  event.to==destination&&event.count==1,
                  "receiving law did not trigger the expected native move");
        }
        check(live.cash(wallet)==37&&live.cash("B")==0&&live.financial().balanced()&&
              live.stock(wallet,"SOURCE","grain")==6&&live.stock(wallet,"TARGET","grain")==0&&
              live.shares("company:origin",wallet)==10&&live.shares("company:origin","B")==0,
              "ordinary relocation must not allocate savings, property, or goods");
    };

    phasegh::World::Plan schedule;
    schedule[9].push_back([=](phasegh::Stage& stage) {
        auto& live=stage.phase_c();
        P2Bill bill;
        bill.id="OPEN-RECEIVING-BORDER";bill.country="B";bill.proposer="PRIME";
        bill.legal_basis="fixture:dated receiving border";bill.effective_day=due;
        bill.proposed_base={"B-OPEN-BORDER","B","migration","national",
            "no_migration_controls","fixture:scheduled receiving reform"};
        live.p2_submit_bill(std::move(bill),january1);
        live.p2_approve_bill("OPEN-RECEIVING-BORDER",{});
        live.p2_promulgate_bill("OPEN-RECEIVING-BORDER","HEAD",january1);
    });
    reform.advance_day(schedule);closed.advance_day();
    verify(reform,false);verify(closed,false);
    reform.advance_day();closed.advance_day();
    verify(reform,false);verify(closed,false);
    check(reform.day_index()==due&&closed.day_index()==due&&
          view(reform).p2_has_due(due)&&!view(closed).p2_has_due(due),
          "the same unprocessed effective day must retain a pending reform only in the authorized branch");
    const auto pending=daily::SnapshotCodec::encode(reform);
    auto replay=daily::SnapshotCodec::decode(pending);
    check(daily::SnapshotCodec::encode(replay)==pending,
          "opening the effective morning changed the native checkpoint");
    reform.advance_day();closed.advance_day();replay.advance_day();
    verify(reform,true);verify(closed,false);verify(replay,true);
    check(reform.date()==closed.date()&&
          view(reform).p2_bills().at("OPEN-RECEIVING-BORDER").status==P2BillStatus::effective&&
          daily::SnapshotCodec::encode(reform)==daily::SnapshotCodec::encode(replay),
          "receiving bill did not take effect on its calendar date and replay exactly");
}

static void land_reform_restricts_only_peasant_internal_migration() {
    const auto run=[](const std::string& choice,bool domestic,bool restricted,
                      const std::string& borders="no_migration_controls") {
        const std::string target=domestic?"A":"B";
        phasegh::World gh{24000};
        auto& geo=gh.seed_geography();
        geo.set_dataset("peasant-internal-migration-fixture");
        geo.seed_province(1,"SOURCE","A");
        geo.seed_province(2,"TARGET",target);
        geo.seed_unit(1,"A","SOURCE-UNIT");
        geo.seed_unit(2,target,"TARGET-UNIT");
        geo.seed_assignment(1,1);geo.seed_assignment(2,2);
        auto& c=geo.seed_core().seed_core();
        c.seed_actor("A",50);c.seed_actor("HOUSE",0);
        c.seed_government("A","HEAD","PRIME",5,3);
        const auto constitution=[&](const std::string& tag) {
            c.seed_p1_constitution(tag,{P1GovernancePrinciple::monarchy,
                P1PowerDistribution::autocracy,P1LegislatureKind::none,
                "fixture:constitution"});
        };
        constitution("A");
        if(!domestic) {
            c.seed_actor("B",0);
            c.seed_government("B","HEAD","PRIME",5,3);
            constitution("B");
            law(c,"B","migration",borders);
            law(c,"B","land_reform","tenant_farmers");
        } else law(c,"A","migration",borders);
        if(!choice.empty())law(c,"A","land_reform",choice);
        auto& pop=c.seed_population();
        const auto home=pop.household_profile({"SOURCE","mixed","shared",50});
        const auto away=pop.household_profile({"TARGET","mixed","shared",0});
        const auto dem=pop.demographic_profile({"SOURCE",23640,phaseb::Sex::male,
            "culture","faith","citizen","free",pop.default_health(),
            phaseb::Service::civilian});
        const auto seed=[&](const std::string& profession) {
            const auto economic=pop.economic_profile({"SOURCE",profession,"wage",
                "employee",pop.default_education()});
            return pop.seed_population(dem,economic,home,100'000);
        };
        const auto peasant=seed("peasant"),worker=seed("worker");
        phaseb::DemographyRules rules;
        rules.fertility_ppm=rules.infant_death_ppm=rules.child_death_ppm=0;
        rules.adult_death_ppm=rules.senior_death_ppm=rules.very_old_death_ppm=0;
        rules.illness_incidence_ppm=rules.accident_ppm=0;
        rules.primary_access_ppm=rules.secondary_access_ppm=rules.tertiary_access_ppm=0;
        rules.adult_literacy_access_ppm=rules.training_ppm=0;
        rules.migration_ppm=310'000;
        rules.destination_by_region["SOURCE"]="TARGET";
        c.enable_demography(rules);
        auto world=daily::World::adopt(std::move(gh),19);
        const auto start=daily::SnapshotCodec::encode(world);
        const bool closed=!domestic&&borders=="closed_borders";
        check(view(world).p2_migration_permitted(peasant,"SOURCE"),
              "same-region movement is not internal migration");
        phasegh::World::Plan explicit_move;
        explicit_move[10].push_back([=](phasegh::Stage& stage) {
            stage.phase_c().migrate_with_explicit_property_allocation(
                peasant,7,"TARGET",away,"A","HOUSE",11,"",0);
        });
        if(restricted||closed) {
            check(rejects([&]{world.advance_day(explicit_move);})&&
                  daily::SnapshotCodec::encode(world)==start,
                  choice=="serfdom"?
                    "Serfdom did not atomically block free peasants moving domestically":
                    "Tenant Farmers or ordinary closed border did not atomically block movement");
        } else {
            auto explicit_world=daily::SnapshotCodec::decode(start);
            explicit_world.advance_day(explicit_move);
            check(view(explicit_world).cash("A")==39&&
                  view(explicit_world).cash("HOUSE")==11,
                  "allowed explicit peasant migration failed to move actual property");
        }
        check(view(world).p2_migration_permitted(worker,"TARGET")==!closed,
              "land reform restricted a non-peasant or bypassed an ordinary closed border");
        world.advance_day();
        People peasants=0,workers=0;
        const auto& people=view(world).people();
        for(const auto& [id,row]:people.slices()) {
            if(people.demographic(row.key.demographic).region!="TARGET")continue;
            if(people.economic(row.key.economic).profession=="peasant")peasants+=row.count;
            else workers+=row.count;
        }
        check(peasants==((restricted||closed)?0:1000)&&
              workers==(closed?0:1000)&&people.population()==200'000,
              "daily peasant/internal law did not change actual saved POP transfer only");
    };
    run("serfdom",true,true);
    run("tenant_farmers",true,true);
    run("peasant_proprietorship",true,false,"closed_borders");
    run("",true,false);
    run("serfdom",false,false);
    run("tenant_farmers",false,false);
    run("tenant_farmers",false,false,"closed_borders");
}


static daily::World population_bureaucracy_fixture(
        const std::string& group,const std::string& selected,
        const std::string& authority="national",
        const std::string& amendment="",bool preexisting=false) {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"CORE","A");
    geo.seed_province(2,"DEPENDENCY","A");
    geo.seed_province(3,"FOREIGN","B");
    geo.seed_unit(1,"A","HOME");
    geo.seed_unit(2,"B","OTHER");
    geo.seed_assignment(1,1);
    geo.seed_assignment(2,1);
    geo.seed_assignment(3,2);
    auto& c=geo.seed_core().seed_core();
    for(const auto* nation:{"A","B"}) {
        c.seed_actor(nation,1000);
        c.seed_government(nation,"HEAD","PRIME",5,3);
        c.seed_p1_constitution(nation,{P1GovernancePrinciple::monarchy,
            P1PowerDistribution::autocracy,P1LegislatureKind::none,
            "test:legitimate-office"});
    }
    c.seed_actor("OFFICE",1000);
    c.p1_set_incorporation("CORE",true);
    c.p1_set_incorporation("DEPENDENCY",false);
    c.p1_set_incorporation("FOREIGN",true);
    auto& pop=c.seed_population();
    for(const auto* region:{"CORE","DEPENDENCY","FOREIGN"}) {
        const auto h=pop.household_profile({region,"mixed","shared",25'000});
        const auto e=pop.economic_profile({region,"worker","wage","employee",
                                          pop.default_education()});
        const auto d=pop.demographic_profile({region,23650,phaseb::Sex::male,
            "culture","faith","citizen","free",pop.default_health(),
            phaseb::Service::civilian});
        const auto slice=pop.seed_population(d,e,h,3'000'000);
        if(std::string_view(region)=="CORE")
            pop.hire(slice,10,"OFFICE","worker",10,{});
    }
    c.seed_public_institution("ADMIN","OFFICE","CORE","paper",100,1);
    c.seed_p1_institution("A","ADMIN",P1InstitutionKind::administration,1);
    c.seed_production_facility("ADMIN_BUILDING","OFFICE","CORE",1);
    c.p1_register_office({"ADMIN","ADMIN_BUILDING",P1OfficeMethod::simple,1,10});
    if(authority=="foreign"&&preexisting) {
        c.seed_public_institution("FOREIGN-ADMIN","B","FOREIGN","paper",100,1);
        c.seed_p1_institution("B","FOREIGN-ADMIN",P1InstitutionKind::administration,1);
    }
    if(!selected.empty()) {
        P2BaseLaw entry;
        entry.id="FOUNDATION";entry.country=authority=="foreign"?"B":"A";
        entry.domain=authority=="custom"||authority=="local"?
            "custom_bureaucracy":"lawgroup_"+group;
        entry.scope=authority=="local"?"CORE":"";
        entry.base_choice="law_"+selected;
        entry.legal_basis="test:bureaucracy-source";
        if(authority=="local")entry.tier=P2NormTier::local_ordinance;
        c.p2_seed_base(entry);
        if(preexisting)
            c.p2_seed_amendment({amendment,"FOUNDATION","test:bureaucracy-source","",
                {{"level",authority=="foreign"?"FOREIGN-ADMIN":"ADMIN",
                  P2ClauseKind::public_institution_level,1}}});
    }
    return daily::World::adopt(std::move(gh),42);
}

static void national_base_laws_change_only_incorporated_pop_administration() {
    const auto view=[](const daily::World& world) {
        return world.engine().base().core().core().p1_bureaucracy("A");
    };
    const auto verify=[&](const std::string& group,const std::string& choice,
                          const std::string& authority,int expected) {
        const auto world=population_bureaucracy_fixture(group,choice,authority);
        const auto effect=view(world);
        check(effect.state_cost==expected&&effect.capacity.committed==expected,
              "state administration population cost accepted an ineligible legal source");
        check(effect.incorporated_population==3'000'000&&
              effect.incorporated_regions==1&&
              effect.capacity.generated==110&&effect.capacity.complete&&
              effect.office_output==10&&effect.institution_cost==0&&
              effect.nationalized_cost==0&&effect.commander_cost==0,
              "state population rate touched base office output or another commitment");
        check((expected>110)==(effect.capacity.shortage_ppm>0)&&
              effect.tax_waste_ppm==effect.capacity.shortage_ppm,
              "actual administrative balance does not reflect reduced population cost");
    };
    struct Row {const char* group;const char* choice;int expected;};
    for(const Row& row:std::array<Row,8>{{
            {"bureaucracy","hereditary_bureaucrats",100},
            {"bureaucracy","crownland_diets",100},
            {"bureaucracy","military_bureaucrats",100},
            {"bureaucracy","communal_administration",100},
            {"caste_hegemony","hindu_caste_enforced",100},
            {"caste_hegemony","hindu_caste_codified",112},
            {"taxation","consumption_based_taxation",100},
            {"economic_system","traditionalism",100}}}) {
        verify(row.group,row.choice,"national",row.expected);
        verify(row.group,row.choice,"local",130);
        verify(row.group,row.choice,"custom",130);
        verify(row.group,row.choice,"foreign",130);
    }
}

static void national_bureaucracy_amendments_change_only_funded_generation() {
    const auto verify=[](const daily::World& world,int generated) {
        const auto& core=world.engine().base().core().core();
        const auto effect=core.p1_bureaucracy("A");
        check(effect.capacity.generated==generated,
              "bureaucracy generation accepted an ineligible or pending amendment");
        check(effect.state_cost==130&&effect.capacity.committed==130&&
              effect.capacity.complete&&effect.office_output==10&&
              effect.institution_cost==0&&effect.nationalized_cost==0&&
              effect.commander_cost==0&&effect.tax_waste_ppm>0&&core.cash("OFFICE")==1000,
              "national generation rate touched costs, physical output, or funds");
        return effect.tax_waste_ppm;
    };
    const auto enact=[&](const std::string& group,const std::string& choice,
                         const std::string& amendment) {
        auto world=population_bureaucracy_fixture(group,choice);
        const auto start=world.day_index();
        const auto baseline=verify(world,110);
        phasegh::World::Plan due;
        due[9].push_back([&](phasegh::Stage& stage) {
            auto& live=stage.phase_c();
            P2Bill bill;
            bill.id="BUREAUCRACY-AMENDMENT";
            bill.country="A";bill.proposer="PRIME";bill.legal_basis="test:bureaucracy-source";
            bill.action=P2BillAction::attach_amendment;
            bill.effective_day=start+2;
            bill.proposed_amendment={amendment,"FOUNDATION","test:bureaucracy-source","",
                {{"level","ADMIN",P2ClauseKind::public_institution_level,1}}};
            live.p2_submit_bill(std::move(bill),start);
            live.p2_approve_bill("BUREAUCRACY-AMENDMENT",{});
            live.p2_promulgate_bill("BUREAUCRACY-AMENDMENT","HEAD",start);
        });
        world.advance_day(due);
        check(verify(world,110)==baseline,"promulgation changed the bureaucracy balance early");
        world.advance_day();
        check(verify(world,110)==baseline,"pending bureaucracy law changed the balance early");
        const auto saved=daily::SnapshotCodec::encode(world);
        auto replay=daily::SnapshotCodec::decode(saved);
        world.advance_day();
        replay.advance_day();
        check(world.engine().base().core().core().p2_amendment_active(amendment)&&
              verify(world,99)>baseline&&verify(replay,99)>baseline&&
              daily::SnapshotCodec::encode(world)==daily::SnapshotCodec::encode(replay),
              "effective bureaucracy generation did not increase actual shortage deterministically");
    };
    const std::string cult="amendment_cult_of_personality";
    const std::string open="amendment_open_legal_questions";
    enact("distribution_of_power","autocracy",cult);
    enact("land_reform","commercialized_agriculture",open);
    for(const auto* jurisdiction:{"local","custom","foreign"}) {
        verify(population_bureaucracy_fixture("distribution_of_power","autocracy",
                                             jurisdiction,cult,true),110);
        verify(population_bureaucracy_fixture("land_reform","commercialized_agriculture",
                                             jurisdiction,open,true),110);
    }
    verify(population_bureaucracy_fixture("bureaucracy","appointed_bureaucrats",
                                         "national",cult,true),110);
    verify(population_bureaucracy_fixture("economic_system","laissez_faire",
                                         "national",open,true),110);
}

static void canonical_citizenship_traits_change_real_pop_rights() {
    struct Source {
        const char* law;
        int primary;
        std::array<int,3> heritage,language;
        int tradition;
        std::array<int,5> wage;
        std::array<bool,5> vote;
    };
    // Independent expectations. Unrelated/exact/family are separate authored
    // branches; a positive shared trait never stacks with its own family.
    const Source sources[]={
        {"subjecthood",100,{0,20,15},{0,20,15},25,
            {60,70,80,90,100},{false,false,true,true,true}},
        {"ethnostate",100,{-10,20,10},{0,15,5},10,
            {50,60,75,85,125},{false,false,false,true,true}},
        {"national_supremacy",100,{-10,40,20},{-5,30,15},15,
            {60,70,80,90,120},{false,false,true,true,true}},
        {"racial_segregation",100,{-20,60,50},{-5,30,15},20,
            {70,80,90,105,115},{false,false,true,true,true}},
        {"cultural_exclusion",100,{10,60,50},{10,50,25},25,
            {70,80,90,100,105},{false,true,true,true,true}},
        {"multicultural",100,{20,60,50},{20,50,30},10,
            {80,90,95,100,100},{true,true,true,true,true}},
        {"official_nationality",50,{10,10,10},{5,10,5},25,
            {100,100,100,100,100},{true,true,true,true,true}},
    };
    for(const auto& source:sources) {
        const bool official=std::string_view(source.law)=="official_nationality";
        phasegh::World gh(24000);
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"HOME","A");
        geo.seed_unit(1,"A","DISTRICT");
        geo.seed_assignment(1,1);
        auto& c=geo.seed_core().seed_core();
        c.seed_actor("A",1000);
        c.seed_actor("EMPLOYER",1000);
        c.seed_government("A","HEAD","PM",5,3);
        c.seed_p1_constitution("A",{P1GovernancePrinciple::monarchy,
            P1PowerDistribution::autocracy,P1LegislatureKind::none,"fixture:citizenship"});
        law(c,"A","citizenship",source.law);
        law(c,"A","church_and_state","state_religion");
        P2BaseLaw local{"LOCAL","A","local_nationality","HOME",
            "law_multicultural","fixture:local"};
        local.tier=P2NormTier::local_ordinance;
        c.p2_seed_base(local);
        c.p2_seed_base({"CUSTOM","A","custom_nationality","national",
                        "law_racial_segregation","fixture:custom"});
        c.p2_seed_culture({"PRIMARY",{"H"},{"HF"},{"L"},{"LF"},{"T"},{}});
        c.p2_seed_religion({"STATE",{"F"},{"FF"}});
        c.p2_seed_religion({"UNRELATED",{"U"},{"UF"}});
        c.p2_seed_national_identity({"A",{"PRIMARY"},{"STATE"}});
        auto& pop=c.seed_population();
        const auto home=pop.household_profile({"HOME","mixed","shared",100});
        const auto economy=pop.economic_profile({"HOME","worker","wage","employee",
                                                pop.default_education()});
        struct Case {phaseb::Id id; int score;};
        std::vector<Case> cases;
        const auto make_pop=[&](const std::string& culture,
                                const std::string& faith,int points) {
            const auto dem=pop.demographic_profile({"HOME",23650,phaseb::Sex::male,
                culture,faith,"citizen","free",pop.default_health(),phaseb::Service::civilian});
            cases.push_back({pop.seed_population(dem,economy,home,10),
                             std::clamp(points,0,100)});
        };
        for(const auto& [faith,score]:std::array<std::pair<const char*,int>,3>{{
                {"STATE",25},{"UNSOURCED",0},{"UNRELATED",-25}}})
            make_pop("PRIMARY",faith,source.primary+score);
        for(int h=0;h<3;++h)for(int l=0;l<3;++l)for(int t=0;t<2;++t) {
            const auto name="POP"+std::to_string(h)+std::to_string(l)+std::to_string(t);
            c.p2_seed_culture({name,{h==1?"H":"OTHER_H"},{h?"HF":"OTHER_HF"},
                {l==1?"L":"OTHER_L"},{l?"LF":"OTHER_LF"},
                {t?"T":"OTHER_T"},{}});
            for(const auto& [faith,score]:std::array<std::pair<const char*,int>,3>{{
                    {"STATE",25},{"UNSOURCED",0},{"UNRELATED",-25}}})
                make_pop(name,faith,source.heritage[h]+source.language[l]+t*source.tradition+score);
        }
        auto world=daily::World::adopt(std::move(gh),37);
        phasegh::World::Plan jobs;
        const auto old_population=view(world).people().population();
        const auto tier=[](int n){return n>=80?4:n>=60?3:n>=40?2:n>=20?1:0;};
        for(const auto [id,score]:cases) {
            const auto actual=view(world).p2_acceptance("A",id);
            const auto rights=view(world).p2_acceptance_effect("A",id);
            if(!actual||actual->score!=score||
               static_cast<int>(actual->level)!=tier(score))
                throw std::runtime_error(std::string(source.law)+
                    ": wrong acceptance from verified live POP trait combination");
            check(rights.has_value()!=official&&
                  view(world).p2_citizenship_vote_allowed("A",id)==source.vote[tier(score)],
                  "citizenship POP tier has wrong votes or invented an undocumented rights tier");
            jobs[2].push_back([id](phasegh::Stage& stage) {
                stage.phase_c().hire(id,1,"EMPLOYER",100);
            });
        }
        world.advance_day(jobs);
        check(view(world).people().population()==old_population,
              "citizenship contracts created or destroyed people");
        for(const auto [id,score]:cases) {
            const auto hired=view(world).people().job(id);
            check(hired&&hired->wage_per_month==source.wage[tier(score)],
                  "real new wage contract missed the matching citizenship acceptance tier");
        }
    }
}

static void canonical_citizenship_homeland_allows_actual_border_movement() {
    for(const auto* choice:{"subjecthood","official_nationality"}) {
        const bool official=std::string_view(choice)=="official_nationality";
        phasegh::World gh(24000);
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"TARGET","A");
        geo.seed_province(2,"HOMELAND","B");
        geo.seed_province(3,"ELSEWHERE","B");
        geo.seed_unit(1,"A","DESTINATION");geo.seed_unit(2,"B","ORIGIN");
        geo.seed_assignment(1,1);geo.seed_assignment(2,2);geo.seed_assignment(3,2);
        auto& c=geo.seed_core().seed_core();
        for(const auto* country:{"A","B"}) {
            c.seed_actor(country,100);
            c.seed_government(country,"HEAD","PM",5,3);
            c.seed_p1_constitution(country,{P1GovernancePrinciple::monarchy,
                P1PowerDistribution::autocracy,P1LegislatureKind::none,
                "fixture:citizenship-homeland"});
        }
        law(c,"A","citizenship",choice);
        law(c,"A","migration","migration_controls");
        law(c,"B","migration","no_migration_controls");
        c.p2_seed_culture({"PRIMARY",{"H"},{"HF"},{"L"},{"LF"},{"T"},{}});
        c.p2_seed_culture({"MIGRANT",{"OTHER_H"},{"OTHER_HF"},
            {"OTHER_L"},{"LF"},{"OTHER_T"},{"HOMELAND"}});
        c.p2_seed_national_identity({"A",{"PRIMARY"},{}});
        auto& pop=c.seed_population();
        const auto destination=pop.household_profile({"TARGET","mixed","shared",100});
        const auto seed=[&](const std::string& region) {
            const auto home=pop.household_profile({region,"mixed","shared",100});
            const auto economy=pop.economic_profile({region,"worker","wage","employee",
                                                    pop.default_education()});
            const auto pop_dem=pop.demographic_profile({region,23650,phaseb::Sex::male,
                "MIGRANT","UNKNOWN","citizen","free",pop.default_health(),
                phaseb::Service::civilian});
            return pop.seed_population(pop_dem,economy,home,10);
        };
        const auto homeland=seed("HOMELAND"),elsewhere=seed("ELSEWHERE");
        auto world=daily::World::adopt(std::move(gh),37);
        check(view(world).p2_acceptance("A",homeland)->score==45&&
              view(world).p2_acceptance("A",elsewhere)->score==15&&
              !view(world).p2_acceptance("B",homeland)&&
              view(world).p2_acceptance_effect("A",homeland).has_value()!=official,
              "citizenship homeland bonus not specific to the verified POP origin and evaluating nation");
        const auto attempt=[&](phaseb::Id id) {
            phasegh::World::Plan movement;
            movement[10].push_back([=](phasegh::Stage& stage) {
                stage.phase_c().migrate_with_explicit_property_allocation(
                    id,4,"TARGET",destination,"B","A",20,"",0);
            });
            world.advance_day(movement);
        };
        const auto old=daily::SnapshotCodec::encode(world);
        check(rejects([&]{attempt(elsewhere);})&&daily::SnapshotCodec::encode(world)==old,
              "non-homeland origin wrongly crossed the controlled border or mutated the checkpoint");
        attempt(homeland);
        People arrived=0;
        const auto& residents=view(world).people();
        for(const auto& [id,p]:residents.slices()) {
            (void)id;
            const auto& d=residents.demographic(p.key.demographic);
            if(d.region=="TARGET"&&d.culture=="MIGRANT")arrived+=p.count;
        }
        check(arrived==4&&residents.population()==20&&
              view(world).cash("A")==120&&view(world).cash("B")==80,
              "verified homeland rights did not enable actual POP and property transfer");
    }
}

static void national_heritage_amendments_change_real_pop_wages() {
    struct Case {const char* jurisdiction;bool racial,homestead;};
    for(const auto& scenario:std::array<Case,7>{{
        {"national",false,false},{"national",true,false},
        {"national",false,true},{"national",true,true},
        {"local",true,true},{"wrong",true,true},{"foreign",true,true}}}) {
        const bool legal=std::string_view(scenario.jurisdiction)=="national";
        const bool foreign=std::string_view(scenario.jurisdiction)=="foreign";
        const bool racial=legal&&scenario.racial,homestead=legal&&scenario.homestead;
        phasegh::World gh(24000);
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"HOME","A");geo.seed_unit(1,"A","DISTRICT");
        geo.seed_assignment(1,1);
        if(foreign) {
            geo.seed_province(2,"FOREIGN","B");geo.seed_unit(2,"B","OTHER");
            geo.seed_assignment(2,2);
        }
        auto& c=geo.seed_core().seed_core();
        c.seed_actor("A",1000);c.seed_actor("EMPLOYER",1000);
        c.seed_government("A","HEAD","PM",5,3);
        c.seed_p1_constitution("A",{P1GovernancePrinciple::monarchy,
            P1PowerDistribution::autocracy,P1LegislatureKind::none,"fixture:heritage"});
        law(c,"A","citizenship","subjecthood");
        law(c,"A","land_reform","homesteading");
        std::string racial_parent="A:citizenship",home_parent="A:land_reform";
        if(std::string_view(scenario.jurisdiction)=="wrong") {
            law(c,"A","colonization","frontier_colonization");
            racial_parent=home_parent="A:colonization";
        } else if(std::string_view(scenario.jurisdiction)=="local") {
            P2BaseLaw local{"LOCAL-C","A","local_citizenship","HOME",
                            "law_subjecthood","fixture:local"};
            local.tier=P2NormTier::local_ordinance;
            c.p2_seed_base(local);
            local.id="LOCAL-L";local.domain="local_land";
            local.base_choice="law_homesteading";
            c.p2_seed_base(local);
            racial_parent="LOCAL-C";home_parent="LOCAL-L";
        }
        if(foreign) {
            c.seed_actor("B",1000);
            c.seed_government("B","HEAD","PM",5,3);
            c.seed_p1_constitution("B",{P1GovernancePrinciple::monarchy,
                P1PowerDistribution::autocracy,P1LegislatureKind::none,"fixture:foreign"});
            law(c,"B","citizenship","subjecthood");
            law(c,"B","land_reform","homesteading");
            racial_parent="B:citizenship";home_parent="B:land_reform";
            c.seed_public_institution("FOREIGN-PUBLIC","B","FOREIGN","paper",100,1);
            c.seed_p1_institution("B","FOREIGN-PUBLIC",P1InstitutionKind::administration,1);
        }
        c.seed_public_institution("PUBLIC","EMPLOYER","HOME","paper",100,1);
        c.seed_p1_institution("A","PUBLIC",P1InstitutionKind::administration,1);
        const auto amend=[&](const char* id,const std::string& parent) {
            c.p2_seed_amendment({id,parent,"fixture:heritage","",
                {{id,foreign?"FOREIGN-PUBLIC":"PUBLIC",
                  P2ClauseKind::public_institution_level,1}}});
        };
        if(scenario.racial)amend("amendment_racialized_subjecthood",racial_parent);
        if(scenario.homestead) {
            // Different typed provision keeps the shared erroneous parent
            // scenario legal in the low-level historical state store.
            c.p2_seed_amendment({"amendment_homestead_acts",home_parent,
                "fixture:heritage","",{{"association",foreign?"B":"A",
                P2ClauseKind::political_association_allowed,1}}});
        }
        c.p2_seed_culture({"PRIMARY",{"H"},{"HF"},{"L"},{"LF"},{"T"},{}});
        c.p2_seed_culture({"SECONDARY",{"H2"},{"HF2"},{"L2"},{"LF2"},{"T2"},{}});
        c.p2_seed_culture({"EXACT",{"H"},{"HF"},{"L"},{"LF"},{"NO"},{}});
        c.p2_seed_culture({"FAMILY",{"OTHER"},{"HF"},{"L"},{"LF"},{"NO"},{}});
        c.p2_seed_culture({"NO_MATCH",{"OTHER"},{"OTHER_F"},{"L"},{"LF"},{"NO"},{}});
        c.p2_seed_culture({"CROSSED",{"H"},{"HF"},{"L2"},{"LF2"},{"NO"},{}});
        c.p2_seed_national_identity({"A",{"PRIMARY","SECONDARY"},{}});
        if(foreign)c.p2_seed_national_identity({"B",{"PRIMARY","SECONDARY"},{}});
        auto& pop=c.seed_population();
        const auto home=pop.household_profile({"HOME","mixed","shared",100});
        const auto econ=pop.economic_profile({"HOME","worker","wage","employee",
                                             pop.default_education()});
        const auto resident=[&](const char* culture) {
            const auto dem=pop.demographic_profile({"HOME",23650,phaseb::Sex::male,
                culture,"UNKNOWN","citizen","free",pop.default_health(),phaseb::Service::civilian});
            return pop.seed_population(dem,econ,home,10);
        };
        const auto primary=resident("PRIMARY"),unknown=resident("UNKNOWN");
        const std::array<phaseb::Id,4> candidates{
            resident("EXACT"),resident("FAMILY"),resident("NO_MATCH"),resident("CROSSED")};
        const std::array<int,4> expect{
            40+(racial?40:0),35+(racial?30:0),
            20-(racial?5:0)-(homestead?5:0),20+(racial?40:0)};
        auto world=daily::World::adopt(std::move(gh),38);
        phasegh::World::Plan payroll;
        for(std::size_t i=0;i<candidates.size();++i) {
            const auto id=candidates[i];
            const auto score=view(world).p2_acceptance("A",id);
            check(score&&score->score==expect[i],
                  "heritage amendment was not applied to the exact eligible POP branch and parent");
            payroll[2].push_back([id](phasegh::Stage& stage) {
                stage.phase_c().hire(id,1,"EMPLOYER",100);
            });
        }
        check(view(world).p2_acceptance("A",primary)->score==100&&
              !view(world).p2_acceptance("A",unknown),
              "heritage amendment was applied to primary or unsupported culture evidence");
        if(foreign)
            check(view(world).p2_acceptance("B",candidates[2])->score==10,
                  "a foreign national amendment must affect only that evaluating sovereign");
        world.advance_day(payroll);
        for(std::size_t i=0;i<candidates.size();++i) {
            const int score=expect[i];
            const int wage=score>=80?100:score>=60?90:score>=40?80:score>=20?70:60;
            const auto job=view(world).people().job(candidates[i]);
            check(job&&job->wage_per_month==wage,
                  "adopted heritage amendment missed the real citizenship wage contract");
        }
    }
}

int main(){
    try{
        ordinary_police_do_not_receive_home_affairs_operating_discounts();
        national_school_and_health_laws_change_legal_administration_cost();
        chiefs_aid_uses_only_effective_welfare_bureaucracy();
        source_ideologies_laws_traits();
        national_base_laws_change_only_incorporated_pop_administration();
        national_bureaucracy_amendments_change_only_funded_generation();
        no_colonial_affairs_adds_only_verified_homeland_acceptance();
        canonical_named_acceptance_changes_only_verified_pop_and_country();
        acceptance_and_five_tier_rights();
        minimum_wage_protects_accepted_new_contract_and_payroll();
        canonical_citizenship_traits_change_real_pop_rights();
        canonical_citizenship_homeland_allows_actual_border_movement();
        national_heritage_amendments_change_real_pop_wages();
        physical_migration_and_law_block();
        daily_migration_checks_receiving_law_without_an_origin_government();
        effective_receiving_law_opens_actual_daily_migration();
        land_reform_restricts_only_peasant_internal_migration();
        abolition_preserves_real_people_and_membership();
        economic_law_restricts_real_company_ownership();
        exact_church_laws_change_real_religious_acceptance();
        religious_migration_amendment_changes_acceptance_not_border_law();
        military_peasant_levy_restricts_real_conscripts();
        institutional_bureaucracy_prices_ongoing_commitments();
        effective_workhouse_amendment_reduces_only_welfare_cost(
            "amendment_religious_workhouses");
        effective_workhouse_amendment_reduces_only_welfare_cost(
            "amendment_workhouse_regulations");
        for(const char* id:{"amendment_religious_workhouses","amendment_workhouse_regulations"})
            for(const int invalid_scope:{1,2})
                effective_workhouse_amendment_reduces_only_welfare_cost(id,invalid_scope);
        effective_health_institution_and_flat_law_change_daily_mortality();
        childrens_rights_changes_only_exact_profession_mortality();
        enacted_education_changes_real_daily_schooling();
        school_staff_capacity_requires_workers_in_the_school_region();
        std::cout<<"VIC2.5 POLICY IDENTITY AND REFERENCE DATA PASS\n";
    }catch(const std::exception& e){
        std::cerr<<"POLITICS IDENTITY FAILURE: "<<e.what()<<"\n";
        return 1;
    }
}
