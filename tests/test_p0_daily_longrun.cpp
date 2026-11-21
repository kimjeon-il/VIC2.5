#include "vic25/p0_daily.hpp"
#include "vic25/daily_snapshot.hpp"
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

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

static void sovereign_interest_multipliers_charge_only_lawful_daily_principal() {
    using namespace phasec;
    enum class Scope { national,custom,local };
    struct Scenario {
        const char* actor;
        const char* domain;
        const char* choice;
        const char* amendment;
        Money first,second;
        Scope scope{Scope::national};
        bool illiquid{},trait{},dated{};
        Money principal{365'000};
        int annual{100'000};
    };
    const Scenario cases[]={
        {"A","economic_system","law_laissez_faire","",75,150},
        {"SPA","economic_system","law_interventionism","amendment_economic_regeneration_1",80,160},
        {"POR","economic_system","law_agrarianism","amendment_economic_regeneration_2",90,180},
        {"JAP","economic_system","law_interventionism","amendment_zaibatsu_oligopoly_1",80,160},
        {"JAP","economic_system","law_industry_banned","amendment_zaibatsu_oligopoly_2",80,160},
        {"POR","economic_system","law_laissez_faire","amendment_economic_regeneration_1",55,110},
        {"SPA","economic_system","law_interventionism","amendment_economic_regeneration_1",
            80,160,Scope::national,true},
        {"A","economic_system","law_laissez_faire","",65,130,Scope::national,false,true},
        {"SPA","economic_system","law_interventionism","amendment_economic_regeneration_1",
            100,200,Scope::national,false,false,true},
        // While payouts are integral, a genuine fractional balance carries.
        {"A","economic_system","law_laissez_faire","",374,749,
            Scope::national,false,false,false,1'825'000,99'900},
        {"A","economic_system","law_laissez_faire","",0,0,
            Scope::national,false,false,false,365'000,0},
        {"A","custom_finance","law_laissez_faire","",100,200,Scope::custom},
        {"A","custom_finance","law_laissez_faire","",100,200,Scope::local},
        {"SPA","custom_finance","law_interventionism","amendment_economic_regeneration_1",100,200,Scope::custom},
        {"JAP","custom_finance","law_interventionism","amendment_zaibatsu_oligopoly_1",100,200,Scope::local},
        {"JAP","distribution_of_power","law_oligarchy","amendment_zaibatsu_oligopoly_2",100,200},
        {"A","economic_system","law_interventionism","amendment_economic_regeneration_1",100,200},
        {"POR","economic_system","law_interventionism","amendment_zaibatsu_oligopoly_2",100,200},
    };
    for(const auto& tc:cases) {
        const std::string country=tc.actor;
        try {
            phasegh::World gh{24000};
            auto& geo=gh.seed_geography();
            geo.seed_province(1,"R",country);
            geo.seed_province(2,"S","FOREIGN");
            geo.seed_province(3,"U","UNBOUND");
            geo.seed_unit(1,country,"HOME");
            geo.seed_unit(2,"FOREIGN","FOREIGN:ROOT");
            geo.seed_unit(3,"UNBOUND","UNBOUND:ROOT");
            geo.seed_assignment(1,1);geo.seed_assignment(2,2);geo.seed_assignment(3,3);
            auto& c=geo.seed_core().seed_core();
            for(const auto& code:{country,std::string("FOREIGN"),std::string("UNBOUND")}) {
                // The matching actor name is itself a supported sovereign
                // wallet convention. The unbound country must lack both.
                if(code!="UNBOUND")c.seed_actor(code,0);
                c.seed_government(code,"HEAD","PM",5,3);
                c.seed_p1_constitution(code,{P1GovernancePrinciple::monarchy,
                    P1PowerDistribution::autocracy,P1LegislatureKind::none,"fixture:constitution"});
            }
            for(const auto* actor:{"TREASURY","FOREIGN:TREASURY","FIRM","UNBOUND:DEBTOR","SINK"})
                c.seed_actor(actor,0);
            c.seed_country_treasury(country,"TREASURY");
            c.seed_country_treasury("FOREIGN","FOREIGN:TREASURY");
            // Its own legitimate law cannot attach to an unbound actor.
            c.p2_seed_base({"UNBOUND::LAW","UNBOUND","economic_system","national",
                "law_laissez_faire","fixture:unbound"});
            check(!c.has_country_treasury("UNBOUND"),
                  "the unbound sovereign accidentally has a treasury wallet");
            c.seed_annual_bank_interest_ppm(tc.annual);
            P2BaseLaw law{"BASE",country,tc.domain,
                tc.scope==Scope::local?"R":"national",tc.choice,"fixture:law"};
            if(tc.scope==Scope::local)law.tier=P2NormTier::local_ordinance;
            c.p2_seed_base(law);
            const auto make_amendment=[&]() -> P2Amendment {
                return {tc.amendment,"BASE","fixture:amendment","",{{"association",country,
                    P2ClauseKind::political_association_allowed,1}}};
            };
            if(*tc.amendment&&!tc.dated)c.p2_seed_amendment(make_amendment());
            phaseb::Id member{};
            if(tc.trait) {
                c.p2_seed_base({"BANKING",country,"credit_access","national",
                    "protected","fixture:law"});
                c.seed_interest_group("BANKERS");
                c.p3_seed_group({country,"BANKERS","","credit_access","protected",
                    P3GroupType::petite_bourgeoisie,500'000,0});
                c.p3_set_ideologies(country,"BANKERS",{"militarist"});
                auto& pop=c.seed_population();
                const auto hh=pop.household_profile({"R","mixed","shared",100});
                const auto e=pop.economic_profile({"R","worker","wage","employee",
                    pop.default_education()});
                const auto d=pop.demographic_profile({"R",23640,phaseb::Sex::female,
                    "culture","faith","citizen","free",pop.default_health(),phaseb::Service::civilian});
                member=pop.seed_population(d,e,hh,100);
            }
            auto world=daily::World::adopt(std::move(gh),37);
            if(tc.trait) {
                phasegh::World::Plan signup;
                signup[9].push_back([member](phasegh::Stage& stage) {
                    stage.phase_c().join_interest_group(member,"BANKERS",100);
                });
                world.advance_day(signup);
            }
            const auto start=world.day_index();
            phasegh::World::Plan loans;
            loans[1].push_back([principal=tc.principal](phasegh::Stage& stage) {
                for(const auto* debtor:{"TREASURY","FOREIGN:TREASURY","FIRM","UNBOUND:DEBTOR"})
                    stage.phase_c().lend(debtor,principal);
            });
            if(tc.illiquid)
                loans[5].push_back([principal=tc.principal](phasegh::Stage& stage) {
                    stage.phase_c().pay("TREASURY","SINK",principal);
                });
            if(tc.dated)
                loans[9].push_back([&](phasegh::Stage& stage) {
                    auto& live=stage.phase_c();
                    P2Bill bill;
                    bill.id="ATTACH";bill.country=country;bill.proposer="PM";
                    bill.legal_basis="fixture:dated";bill.effective_day=start+2;
                    bill.action=P2BillAction::attach_amendment;
                    bill.proposed_amendment=make_amendment();
                    live.p2_submit_bill(std::move(bill),start);
                    live.p2_approve_bill("ATTACH",{});
                    live.p2_promulgate_bill("ATTACH","HEAD",start);
                });
            world.advance_day(loans);
            auto replay=daily::SnapshotCodec::decode(daily::SnapshotCodec::encode(world));
            const auto verify=[&](int elapsed,Money expected) {
                const auto& ledger=world.engine().base().core().core();
                for(const auto* debtor:{"TREASURY","FOREIGN:TREASURY","FIRM","UNBOUND:DEBTOR"})
                    check(ledger.outstanding_loan(debtor)==tc.principal,
                          "daily interest modified real principal");
                const Money neutral=tc.principal*tc.annual*elapsed/365'000'000;
                for(const auto* debtor:{"FOREIGN:TREASURY","FIRM","UNBOUND:DEBTOR"})
                    check(ledger.cash(debtor)==tc.principal-neutral,
                          "law leaked to a foreign sovereign, private firm or unbound actor");
                check(ledger.cash("TREASURY")==(tc.illiquid?0:tc.principal-expected),
                      "only an eligible national rate should change the actual treasury payment");
                const auto& finance=ledger.financial();
                const Money claim=finance.has_account("BANK:interest_receivable")?
                                  finance.balance("BANK:interest_receivable"):0;
                check(claim==(tc.illiquid?expected:0),
                      "bank accrual did not preserve the actual unpaid balance");
                check(ledger.cash("SINK")==(tc.illiquid?tc.principal:0),
                      "interest created cash instead of settling an existing balance");
            };
            verify(1,tc.first);
            world.advance_day();replay.advance_day();
            verify(2,tc.second);
            check(daily::SnapshotCodec::encode(world)==daily::SnapshotCodec::encode(replay),
                  "effective rates or fractional carry changed after save and replay");
            if(tc.dated) {
                world.advance_day();replay.advance_day();
                verify(3,280);
                check(daily::SnapshotCodec::encode(world)==daily::SnapshotCodec::encode(replay),
                      "effective amendment did not replay on its dated interest accrual");
            }
        } catch(const std::exception& e) {
            throw std::runtime_error(country+" / "+tc.choice+" / "+tc.amendment+": "+e.what());
        }
    }
}

static void public_health_budget_and_treatment_require_eligible_clinics(
        std::string_view extra_case={}) {
    using namespace phasec;
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    auto& core=geo.seed_core().seed_core();
    for(const auto* country:{"A","B"}) {
        const std::string tag=country,region="R"+tag,treasury="TREASURY_"+tag;
        const int id=tag=="A"?1:2;
        geo.seed_province(id,region,tag);
        geo.seed_unit(id,tag,"ROOT");
        geo.seed_assignment(id,id);
        core.seed_government(tag,"HEAD_"+tag,"PM_"+tag,5,3);
        core.seed_p1_constitution(tag,{P1GovernancePrinciple::monarchy,
            P1PowerDistribution::autocracy,P1LegislatureKind::none,
            "fixture:constitution"});
        core.seed_actor(treasury,tag=="A"?20:1'000);
        core.seed_country_treasury(tag,treasury);
        core.seed_market(region,"medicine",10,1,100);
        P2BaseLaw law;
        law.id="HEALTH_"+tag; law.country=tag; law.domain="healthcare";
        law.scope="national"; law.base_choice="fixture-care";
        law.legal_basis="fixture:health-statute";
        law.clauses={{"health-right",tag,P2ClauseKind::public_healthcare_right,
                      tag=="A"?1:0}};
        core.p2_seed_base(law);
    }
    struct Clinic {
        const char* name;
        const char* country;
        std::optional<P1InstitutionKind> kind;
        phaseb::Id pop{};
        std::uint8_t level{5};
        const char* operator_id{};
    };
    std::vector<Clinic> sites{
        {"01_POLICE","A",P1InstitutionKind::policing},
        {"02_TYPED","A",P1InstitutionKind::healthcare},
        {"03_LEGACY","A",std::nullopt},
        {"04_INELIGIBLE","B",P1InstitutionKind::healthcare}};
    if(!extra_case.empty())
        sites.push_back({"00_DECOY","A",P1InstitutionKind::healthcare,0,
            static_cast<std::uint8_t>(extra_case=="seeded-closed"?0:5),
            extra_case=="self-recipient"?"TREASURY_A":nullptr});
    auto& pop=core.seed_population();
    core.seed_actor("MANUAL_WAGES",0);
    core.seed_actor("BOND_BUYER",100);
    for(auto& site:sites) {
        const std::string name=site.name,tag=site.country,region="R"+tag;
        const std::string op=site.operator_id?site.operator_id:name;
        if(!site.operator_id)
            core.seed_actor(op,!extra_case.empty()&&name=="02_TYPED"?0:100);
        core.seed_public_institution(name,op,region,"medicine",31,3);
        if(site.kind)core.seed_p1_institution(tag,name,*site.kind,site.level);
        core.seed_stock(op,region,"medicine",10);
        if(extra_case=="law-closed"&&name=="00_DECOY") {
            P2BaseLaw closure;
            closure.id="CLINIC-CLOSURE"; closure.country=tag;
            closure.domain="facility_investment"; closure.scope="national";
            closure.base_choice="zero-investment";
            closure.legal_basis="fixture:clinic-operating-level";
            closure.clauses={{"closed",name,P2ClauseKind::public_institution_level,0}};
            core.p2_seed_base(closure);
        }
        const auto hh=pop.household_profile({region,"mixed","shared",5});
        const auto econ=pop.economic_profile({region,"worker","wage","employee",
            pop.default_education()});
        const auto demo=pop.demographic_profile({region,24000-300,
            phaseb::Sex::female,"culture","faith","citizen","free",
            pop.default_health(),phaseb::Service::civilian});
        site.pop=pop.seed_population(demo,econ,hh,5,
            static_cast<phaseb::Id>(pop.slices().size()+1));
        pop.hire(site.pop,2,op,"worker",10,{});
    }
    gh.validate();
    auto opening=daily::World::adopt(std::move(gh),312);
    p0::Policies options;
    options.daily_food_units_per_person=0;
    const auto ledger=[](const p0::DailyGame& g)->const IntegratedWorld& {
        return g.world().engine().base().core().core();
    };
    check(!ledger(p0::DailyGame(opening,options)).has_public_healthcare_right(),
          "fixture must not depend on the legacy global healthcare toggle");
    for(const bool explicit_budget:{false,true}) {
        auto policy=options;
        if(explicit_budget) {
            for(const auto* country:{"A","B"})
                policy.national_budget_envelopes.push_back({
                    country,0,50,0,1,1,0,0,365,false,true});
        } else {
            // Actual cash covers just the two eligible clinics. Forecasting
            // even one unit of the police decoy would solicit a real bond.
            policy.bond_buyer_orders.push_back({"A","BOND_BUYER",10,0,0});
        }
        p0::DailyGame game(opening,policy);
        game.advance_day();
        const auto& paid=ledger(game);
        if(!extra_case.empty()) {
            check(paid.cash("02_TYPED")==10&&paid.cash("03_LEGACY")==110&&
                  paid.cash("TREASURY_A")==0&&
                  paid.country_fiscal("A").realized_expenditure==20&&
                  paid.country_fiscal("A").bond_principal==0&&
                  paid.cash("BOND_BUYER")==100&&
                  (extra_case=="self-recipient"||paid.cash("00_DECOY")==100),
                  explicit_budget?
                    "automatic service payout must ignore non-payable clinic":
                    "automatic financing must not forecast/purchase clinic debt for a non-payable clinic");
        }
        else check(paid.cash("02_TYPED")==110&&paid.cash("03_LEGACY")==110&&
              paid.cash("TREASURY_A")==0&&
              paid.country_fiscal("A").realized_expenditure==20&&
              paid.country_fiscal("A").bond_principal==0&&
              paid.cash("BOND_BUYER")==100,
              "eligible sovereign must pay real typed and legacy clinics only");
        check(paid.cash("01_POLICE")==100&&paid.cash("04_INELIGIBLE")==100&&
              paid.cash("TREASURY_B")==1'000&&
              paid.country_fiscal("B").realized_expenditure==0,
              "police and statutorily ineligible sovereign cannot get clinic funds");
    }
    if(!extra_case.empty())return;
    const auto direct_plan=[&](std::initializer_list<std::size_t> indexes) {
        phasegh::World::Plan plan;
        for(const auto index:indexes) {
            const auto site=sites.at(index);
            plan[5].push_back([site](auto& gh) {
                gh.phase_c().wages(site.name,"MANUAL_WAGES",
                    std::string("TREASURY_")+site.country,1,0);
            });
            plan[7].push_back([site](auto& gh) {
                gh.phase_c().provide_public_healthcare(site.name,site.pop,1);
            });
        }
        return plan;
    };
    p0::DailyGame legal(opening,options);
    legal.advance_day(direct_plan({1,2}));
    check(ledger(legal).public_services_rendered("02_TYPED")==1&&
          ledger(legal).public_services_rendered("03_LEGACY")==1&&
          ledger(legal).stock("02_TYPED","RA","medicine")==9&&
          ledger(legal).stock("03_LEGACY","RA","medicine")==9,
          "typed and legacy clinics must actually deliver funded care");
    for(const std::size_t index:{0,3}) {
        p0::DailyGame illegal(opening,options);
        const auto before=daily::SnapshotCodec::encode(illegal.world());
        bool rejected=false;
        try {illegal.advance_day(direct_plan({index}));}
        catch(const std::exception& error) {
            rejected=std::string(error.what()).find("entitled clinical institution")
                !=std::string::npos;
        }
        check(rejected&&daily::SnapshotCodec::encode(illegal.world())==before,
              "nonclinic and ineligible care must be refused without ledger mutation");
    }
}

int main(){
    try{
        public_health_budget_and_treatment_require_eligible_clinics();
        std::string failures;
        for(const auto* extra:{"seeded-closed","law-closed","self-recipient"})
            try {
                public_health_budget_and_treatment_require_eligible_clinics(extra);
            } catch(const std::exception& error) {
                failures+=std::string(extra)+": "+error.what()+"; ";
            }
        if(!failures.empty())throw std::runtime_error(failures);
        one_year_continuity();
        sovereign_bank_interest_law_uses_real_principal();
        sovereign_interest_multipliers_charge_only_lawful_daily_principal();
        std::cout<<"P0 daily 366-day save-replay and conservations: PASS\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"P0 366-day regression FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
