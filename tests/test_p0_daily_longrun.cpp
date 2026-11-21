#include "vic25/p0_daily.hpp"
#include "vic25/daily_snapshot.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

using namespace vic25;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}

static std::pair<daily::World,p0::Policies> scenario() {
    phasegh::World gh(24000); // 2000-01-01, a Gregorian leap year
    auto& e=gh.seed_geography();
    e.seed_province(1,"R","A");
    e.seed_unit(11,"A","ROOT");
    e.seed_assignment(1,11);
    auto& c=e.seed_core().seed_core();
    c.seed_actor("A",100);
    c.seed_actor("SELLER",1000);
    c.seed_actor("HOUSE",0);
    c.seed_market("R","food",100,1,300);
    c.seed_government("A","head","prime",5,3);
    c.seed_interest_group("FOOD_MOVEMENT");
    auto& pop=c.seed_population();
    const auto h=pop.household_profile({"R","mixed","shared",20});
    const auto econ=pop.economic_profile({"R","worker","wage","employee",
                                         pop.default_education()});
    const auto demo=pop.demographic_profile({"R",24000-300,
                     phaseb::Sex::female,"culture","faith","citizen",
                     "free",pop.default_health(),phaseb::Service::civilian});
    const auto id=pop.seed_population(demo,econ,h,20,1);
    p0::Policies policies;
    policies.buyers.push_back({id,"HOUSE","SELLER","R","food",1});
    policies.political_reactions.push_back({id,"R","FOOD_MOVEMENT",2,1});
    gh.validate();
    return {daily::World::adopt(std::move(gh),29),std::move(policies)};
}
static void one_year_continuity() {
    auto [world,policy]=scenario();
    p0::DailyGame first(std::move(world),policy);
    for(int i=0;i<100;++i)first.advance_day();
    const auto checkpoint=daily::SnapshotCodec::encode(first.world());
    auto restored=daily::SnapshotCodec::decode(checkpoint);
    p0::DailyGame replay(std::move(restored),policy);
    check(daily::SnapshotCodec::encode(replay.world())==checkpoint,
          "P0 daily save did not round-trip at day 100");
    for(int i=100;i<366;++i) {
        first.advance_day();
        replay.advance_day();
        if(i==182 || i==365)
            check(daily::SnapshotCodec::encode(first.world())==
                  daily::SnapshotCodec::encode(replay.world()),
                  "P0 multi-month snapshot replay diverged");
    }
    check(first.world().date()==daily::Date{2001,1,1},
          "leap year 2000 did not end after 366 daily ticks");
    const auto& core=first.world().engine().base().core().core();
    check(core.people().population()==20,"year-long POP conservation failed");
    check(core.cash("HOUSE")==0,"insolvent household created money");
    check(core.food_grievance("R")>0,"year-long unmet demand not registered");
    check(first.world().day_commit_id()==366,"daily commit counter wrong");
}
static void sovereign_bank_interest_law_uses_real_principal() {
    const auto run=[](const std::string& group,const std::string& parent,
                      const std::string& amendment,int base_ppm,
                      phasec::Money interest,bool illiquid=false,
                      bool local_parent=false,bool stack_with_laissez=false) {
        phasegh::World gh{24000};
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"R","A");
        geo.seed_unit(1,"A","ROOT");
        geo.seed_assignment(1,1);
        auto& c=geo.seed_core().seed_core();
        c.seed_actor("A",0);
        c.seed_actor("TREASURY",0);
        c.seed_actor("FIRM",0);
        c.seed_actor("SINK",0);
        c.seed_government("A","HEAD","PM",5,3);
        c.seed_p1_constitution("A",{phasec::P1GovernancePrinciple::monarchy,
            phasec::P1PowerDistribution::autocracy,phasec::P1LegislatureKind::none,
            "fixture:constitution"});
        c.seed_country_treasury("A","TREASURY");
        c.seed_annual_bank_interest_ppm(base_ppm);
        if(!parent.empty()) {
            phasec::P2BaseLaw law;
            law.id="BASE";law.country="A";law.domain=group;
            law.scope=local_parent?"R":"national";
            if(local_parent)law.tier=phasec::P2NormTier::local_ordinance;
            law.base_choice=parent;law.legal_basis="fixture:law";
            c.p2_seed_base(law);
        }
        if(stack_with_laissez) {
            phasec::P2BaseLaw economy;
            economy.id="ECONOMY";economy.country="A";economy.domain="economic_system";
            economy.scope="national";economy.base_choice="law_laissez_faire";
            economy.legal_basis="fixture:economy";
            c.p2_seed_base(economy);
        }
        if(!amendment.empty())
            c.p2_seed_amendment({amendment,"BASE","fixture:amendment","",
                {{"actual-law-clause","A",
                  phasec::P2ClauseKind::political_association_allowed,1}}});
        auto world=daily::World::adopt(std::move(gh),19);
        constexpr phasec::Money principal=365'000;
        const phasec::Money private_interest=base_ppm/1000;
        phasegh::World::Plan loans;
        loans[1].push_back([principal](phasegh::Stage& stage) {
            stage.phase_c().lend("TREASURY",principal);
            stage.phase_c().lend("FIRM",principal);
        });
        if(illiquid)loans[5].push_back([principal](phasegh::Stage& stage) {
            stage.phase_c().pay("TREASURY","SINK",principal);
        });
        world.advance_day(loans);
        const auto saved=daily::SnapshotCodec::encode(world);
        auto restored=daily::SnapshotCodec::decode(saved);
        for(int elapsed=1;elapsed<=2;++elapsed) {
            const auto& ledger=world.engine().base().core().core();
            check(ledger.outstanding_loan("TREASURY")==principal &&
                  ledger.outstanding_loan("FIRM")==principal,
                  "interest changed actual loan principal");
            check(ledger.cash("TREASURY")==
                      (illiquid?0:principal-elapsed*interest),
                  "law did not affect exactly the sovereign's elapsed daily interest");
            check(ledger.cash("FIRM")==principal-elapsed*private_interest,
                  "sovereign interest law leaked into a private loan");
            const auto& finance=ledger.financial();
            const auto receivable=finance.has_account("BANK:interest_receivable")?
                                  finance.balance("BANK:interest_receivable"):0;
            check(receivable==(illiquid?elapsed*interest:0),
                  "interest claims were not conserved with actual available cash");
            if(elapsed==1) {
                world.advance_day();
                restored.advance_day();
                check(daily::SnapshotCodec::encode(world)==
                      daily::SnapshotCodec::encode(restored),
                      "active interest outcome did not survive saved elapsed day");
            }
        }
    };
    run("","", "",100'000,100);
    run("church_and_state","law_state_religion","amendment_draconian_blasphemy_laws",
        100'000,120);
    run("land_reform","law_commercialized_agriculture","amendment_open_legal_questions",
        100'000,90,false,false,true); // (10% + 2 percentage points) × 75%
    run("governance_principles","law_corporate_state","amendment_industrial_socialization",
        100'000,130);
    run("governance_principles","law_corporate_state","amendment_shop_councils",
        100'000,110);
    run("governance_principles","law_corporate_state","amendment_foreign_investment_seizures",
        100'000,150);
    run("church_and_state","law_state_religion","amendment_draconian_blasphemy_laws",
        100'000,120,true);
    run("church_and_state","law_state_religion","amendment_draconian_blasphemy_laws",
        0,0);
    run("regional_finance","local_rate","amendment_draconian_blasphemy_laws",
        100'000,100,false,true);
    run("migration","law_no_migration_controls","amendment_draconian_blasphemy_laws",
        100'000,100);
}

int main(){
    try{
        one_year_continuity();
        sovereign_bank_interest_law_uses_real_principal();
        std::cout<<"P0 daily 366-day save-replay and conservations: PASS\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"P0 366-day regression FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
