#include "vic25/p0_daily.hpp"
#include "vic25/daily_snapshot.hpp"
#include <iostream>
#include <limits>
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

static void automatic_sovereign_budget_uses_laws_effective_today() {
    using namespace phasec;
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    auto& ledger=geo.seed_core().seed_core();
    for(const auto* country:{"A","B"}) {
        const std::string tag=country,region="R"+tag,clinic=tag+"_CLINIC";
        const int number=tag=="A"?1:2;
        geo.seed_province(number,region,tag);
        geo.seed_unit(number,tag,"ROOT");
        geo.seed_assignment(number,number);
        ledger.seed_government(tag,tag+"_HEAD",tag+"_PM",5,3);
        ledger.seed_p1_constitution(tag,{P1GovernancePrinciple::monarchy,
            P1PowerDistribution::autocracy,P1LegislatureKind::none,
            "fixture:constitution"});
        ledger.seed_actor(tag+"_TREASURY",0);
        ledger.seed_actor(tag+"_BIDDER",100);
        ledger.seed_actor(clinic,0);
        ledger.seed_country_treasury(tag,tag+"_TREASURY");
        ledger.seed_market(region,"medicine",1,1,1);
        ledger.seed_public_institution(clinic,clinic,region,"medicine",16,1);
        ledger.seed_p1_institution(tag,clinic,P1InstitutionKind::healthcare,5);
        ledger.seed_stock(clinic,region,"medicine",10);
        auto& pop=ledger.seed_population();
        const auto hh=pop.household_profile({region,"mixed","shared",5});
        const auto econ=pop.economic_profile({region,"worker","wage","employee",
            pop.default_education()});
        const auto demo=pop.demographic_profile({region,24000-300,
            phaseb::Sex::female,"culture","faith","citizen","free",
            pop.default_health(),phaseb::Service::civilian});
        const auto worker=pop.seed_population(demo,econ,hh,5,number);
        pop.hire(worker,1,clinic,"worker",10,{});
        if(tag=="B") {
            P2BaseLaw prior;
            prior.id="B_HEALTH"; prior.country=tag; prior.domain="healthcare";
            prior.scope="national"; prior.base_choice="fixture-care";
            prior.legal_basis="fixture:prior-health-statute";
            prior.clauses={{"health-right",tag,P2ClauseKind::public_healthcare_right,1}};
            ledger.p2_seed_base(prior);
        }
    }
    gh.validate();
    auto opening=daily::World::adopt(std::move(gh),316);
    const auto today=opening.day_index();
    phasegh::World::Plan introduce;
    introduce[9].push_back([today](phasegh::Stage& stage) {
        auto& c=stage.phase_c();
        P2Bill grant;
        grant.id="GRANT"; grant.country="A"; grant.proposer="A_PM";
        grant.legal_basis="fixture:health-grant"; grant.effective_day=today+1;
        grant.proposed_base.id="A_HEALTH"; grant.proposed_base.country="A";
        grant.proposed_base.domain="healthcare"; grant.proposed_base.scope="national";
        grant.proposed_base.base_choice="fixture-care";
        grant.proposed_base.legal_basis=grant.legal_basis;
        grant.proposed_base.clauses={{"health-right","A",
            P2ClauseKind::public_healthcare_right,1}};
        c.p2_submit_bill(grant,today);
        c.p2_approve_bill("GRANT",{});
        c.p2_promulgate_bill("GRANT","A_HEAD",today);
        P2Bill repeal;
        repeal.id="REPEAL"; repeal.country="B"; repeal.proposer="B_PM";
        repeal.legal_basis="fixture:health-repeal"; repeal.effective_day=today+1;
        repeal.action=P2BillAction::repeal_base; repeal.target_id="B_HEALTH";
        c.p2_submit_bill(repeal,today);
        c.p2_approve_bill("REPEAL",{});
        c.p2_promulgate_bill("REPEAL","B_HEAD",today);
    });
    p0::Policies policies;
    policies.daily_food_units_per_person=0;
    policies.bond_buyer_orders={{"A","A_BIDDER",10,0,0},{"B","B_BIDDER",10,0,0}};
    const auto core=[](const p0::DailyGame& game)->const IntegratedWorld& {
        return game.world().engine().base().core().core();
    };
    p0::DailyGame before(std::move(opening),policies);
    before.advance_day(introduce); // Jan 1: daily clinic quota is zero.
    check(!core(before).public_clinic_entitled("A_CLINIC")&&
          core(before).public_clinic_entitled("B_CLINIC")&&
          core(before).sovereign_bond_principal("A")==0&&
          core(before).sovereign_bond_principal("B")==0,
          "dated bills must not alter opening day or create pre-effective debt");

    const auto pending=daily::SnapshotCodec::encode(before.world());
    p0::DailyGame reopened(daily::SnapshotCodec::decode(pending),policies);
    reopened.advance_day(); // Jan 2: exactly one clinic quota and both P2 laws mature.
    const auto& settled=core(reopened);
    check(settled.public_clinic_entitled("A_CLINIC")&&
          !settled.public_clinic_entitled("B_CLINIC"),
          "test health laws did not take effect at opening on their authorized date");
    const bool grant_paid=settled.cash("A_CLINIC")==1&&
             settled.cash("A_BIDDER")==99&&
             settled.sovereign_bond_principal("A")==1&&
             settled.country_fiscal("A").realized_expenditure==1&&
             settled.cash("A_TREASURY")==0;
    const bool repeal_no_debt=settled.cash("B_CLINIC")==0&&
             settled.cash("B_BIDDER")==100&&
             settled.sovereign_bond_principal("B")==0&&
             settled.country_fiscal("B").realized_expenditure==0&&
             settled.cash("B_TREASURY")==0;
    if(!grant_paid||!repeal_no_debt)
        throw std::runtime_error(std::string("dated clinic budget: ")+
            (grant_paid?"grant correctly paid":"grant failed real same-day payment")+
            "; "+(repeal_no_debt?"repeal had no debt":"repeal issued imaginary principal"));

    // A manually configured freeze is not overwritten by automatic funding
    // even when the newly valid clinic is otherwise eligible for a real bid.
    policies.national_budget_envelopes.push_back({"A",0,0,0,1,1,0,0,365,false,true});
    p0::DailyGame explicit_freeze(daily::SnapshotCodec::decode(pending),policies);
    explicit_freeze.advance_day();
    const auto& fixed=core(explicit_freeze);
    check(fixed.public_clinic_entitled("A_CLINIC")&&fixed.cash("A_CLINIC")==0&&
          fixed.cash("A_BIDDER")==100&&fixed.sovereign_bond_principal("A")==0&&
          fixed.sovereign_bond_principal("B")==0,
          "explicit envelope cannot be replaced by an automatic one");
}

static void automatic_budget_retains_real_coupon_only_financing() {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"R","A");
    geo.seed_unit(1,"A","ROOT");
    geo.seed_assignment(1,1);
    auto& opening=geo.seed_core().seed_core();
    opening.seed_government("A","HEAD","PM",5,3);
    opening.seed_actor("TREASURY",0);
    opening.seed_actor("INITIAL_HOLDER",365);
    opening.seed_actor("STANDING_BIDDER",100);
    opening.seed_actor("TRANSFER_DONOR",500);
    opening.seed_actor("PAID_RECIPIENT",0);
    opening.seed_country_treasury("A","TREASURY");
    auto world=daily::World::adopt(std::move(gh),317);
    const auto today=world.day_index();
    phasegh::World::Plan obligation;
    obligation[1].push_back([today](phasegh::Stage& stage) {
        stage.phase_c().issue_sovereign_bond("ORIGINAL","A","PM",
            "INITIAL_HOLDER",365,1'000'000,today,today+365);
    });
    obligation[5].push_back([](phasegh::Stage& stage) {
        auto& c=stage.phase_c();
        c.pay("TRANSFER_DONOR","TREASURY",500);
        c.spend_country_budget("A","PAID_RECIPIENT",865);
    });
    p0::Policies policies;
    policies.daily_food_units_per_person=0;
    policies.bond_buyer_orders.push_back({"A","STANDING_BIDDER",10,0,0});
    p0::DailyGame game(std::move(world),policies);
    const auto core=[&]()->const phasec::IntegratedWorld& {
        return game.world().engine().base().core().core();
    };
    game.advance_day(obligation);
    game.advance_day(); // The first actual coupon accrues, but cannot be paid.
    check(core().country_fiscal("A").unpaid_coupons==1&&
          core().country_fiscal("A").realized_revenue==500&&
          core().cash("TREASURY")==0&&core().cash("STANDING_BIDDER")==100&&
          core().sovereign_bond_principal("A")==365,
          "unfunded authentic coupon must be visible before automatic financing");
    game.advance_day(); // Existing unpaid coupon is enough to derive an envelope.
    check(core().sovereign_bond_principal("A")==366&&
          core().country_fiscal("A").unpaid_coupons==1&&
          core().cash("STANDING_BIDDER")==99&&
          core().cash("INITIAL_HOLDER")==1&&core().cash("TREASURY")==0,
          "coupon-only automatic envelope must fund actual dated coupon cash");
}

static void refinance_only_matured_sovereign_principal_with_existing_cash() {
    using namespace phasec;
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    auto& c=geo.seed_core().seed_core();
    for(const auto* country:{"A","B"}) {
        const std::string tag=country;
        const int id=tag=="A"?1:2;
        geo.seed_province(id,"R"+tag,tag);
        geo.seed_unit(id,tag,"ROOT");
        geo.seed_assignment(id,id);
        c.seed_government(tag,tag+"_HEAD",tag+"_PM",5,3);
        c.seed_actor(tag+"_TREASURY",0);
        c.seed_country_treasury(tag,tag+"_TREASURY");
    }
    c.seed_actor("ORIGINAL_SELLER",100);
    c.seed_actor("CURRENT_HOLDER",100);
    c.seed_actor("FUTURE_HOLDER",30);
    c.seed_actor("FOREIGN_HOLDER",80);
    c.seed_actor("STANDING_BIDDER",1'000);
    c.seed_actor("RECEIPT_DONOR",500);
    c.seed_actor("PAID_RECIPIENT",0);
    auto opening=daily::World::adopt(std::move(gh),318);
    const auto today=opening.day_index();
    const auto open_bonds=[today](Money receipts) {
        phasegh::World::Plan plan;
        plan[1].push_back([today](phasegh::Stage& stage) {
            auto& live=stage.phase_c();
            live.issue_sovereign_bond("MATURITY","A","A_PM","ORIGINAL_SELLER",
                100,0,today,today+1);
            live.issue_sovereign_bond("FUTURE","A","A_PM","FUTURE_HOLDER",
                30,0,today,today+40);
            live.issue_sovereign_bond("FOREIGN","B","B_PM","FOREIGN_HOLDER",
                80,0,today,today+1);
        });
        plan[5].push_back([receipts](phasegh::Stage& stage) {
            auto& live=stage.phase_c();
            live.pay("RECEIPT_DONOR","A_TREASURY",receipts);
            live.spend_country_budget("A","PAID_RECIPIENT",130+receipts);
            live.spend_country_budget("B","PAID_RECIPIENT",80);
        });
        plan[7].push_back([](phasegh::Stage& stage) {
            stage.phase_c().exchange_sovereign_bond("MATURITY",
                "ORIGINAL_SELLER","CURRENT_HOLDER",100,1'000'000);
        });
        return plan;
    };
    p0::Policies bidder;
    bidder.daily_food_units_per_person=0;
    bidder.bond_buyer_orders.push_back({"A","STANDING_BIDDER",1'000,0,0});
    const auto core=[](const p0::DailyGame& game)->const IntegratedWorld& {
        return game.world().engine().base().core().core();
    };
    p0::DailyGame original(opening,bidder);
    original.advance_day(open_bonds(500));
    check(core(original).sovereign_bond_principal("A")==130&&
          core(original).sovereign_bond_principal("B")==80&&
          core(original).shares("bond:MATURITY","CURRENT_HOLDER")==100&&
          core(original).cash("CURRENT_HOLDER")==0&&
          core(original).cash("A_TREASURY")==0&&
          core(original).cash("STANDING_BIDDER")==1'000&&
          core(original).country_fiscal("A").unpaid_coupons==0,
          "newly issued bonds and actual secondary title must exist before maturity");
    const auto saved=daily::SnapshotCodec::encode(original.world());
    const auto verify_paid=[&](const p0::DailyGame& game,std::int64_t date) {
        const auto& live=core(game);
        const auto replacement="P0:AUCTION:A:"+std::to_string(date)+
                               ":STANDING_BIDDER";
        check(live.shares("bond:MATURITY","CURRENT_HOLDER")==0&&
              live.cash("CURRENT_HOLDER")==100&&live.cash("ORIGINAL_SELLER")==100&&
              live.cash("STANDING_BIDDER")==900&&live.cash("A_TREASURY")==0&&
              live.country_fiscal("A").realized_revenue==500&&
              live.shares("bond:"+replacement,"STANDING_BIDDER")==100&&
              live.shares("bond:FUTURE","FUTURE_HOLDER")==30&&
              live.shares("bond:FOREIGN","FOREIGN_HOLDER")==80&&
              live.cash("FUTURE_HOLDER")==0&&live.cash("FOREIGN_HOLDER")==0&&
              live.sovereign_bond_principal("A")==130&&
              live.sovereign_bond_principal("B")==80&&
              live.cash("B_TREASURY")==0&&live.financial().balanced(),
              "real standing bid must refinance exactly mature title now owned by claimant");
    };
    original.advance_day();
    verify_paid(original,today+1);
    original.advance_day();
    verify_paid(original,today+1); // A redeemed original face must not be refinanced again.

    auto no_bid=bidder;
    no_bid.bond_buyer_orders.clear();
    const auto verify_unpaid=[&](const p0::DailyGame& game) {
        const auto& live=core(game);
        check(live.sovereign_bond_principal("A")==130&&
              live.shares("bond:MATURITY","CURRENT_HOLDER")==100&&
              live.cash("CURRENT_HOLDER")==0&&live.cash("A_TREASURY")==0&&
              live.cash("STANDING_BIDDER")==1'000&&live.financial().balanced(),
              "unfunded maturity must stay unpaid, never create cash or bypass approval");
    };
    p0::DailyGame overdue(daily::SnapshotCodec::decode(saved),no_bid);
    overdue.advance_day();
    verify_unpaid(overdue);
    p0::DailyGame delayed(daily::SnapshotCodec::decode(
        daily::SnapshotCodec::encode(overdue.world())),bidder);
    delayed.advance_day();
    verify_paid(delayed,today+2);

    // A genuine partial redemption reduces the outstanding mature face.
    p0::DailyGame partial(daily::SnapshotCodec::decode(saved),no_bid);
    phasegh::World::Plan extra_cash;
    extra_cash[5].push_back([](phasegh::Stage& stage) {
        stage.phase_c().pay("STANDING_BIDDER","A_TREASURY",40);
    });
    partial.advance_day(extra_cash);
    check(core(partial).shares("bond:MATURITY","CURRENT_HOLDER")==60&&
          core(partial).cash("CURRENT_HOLDER")==40&&
          core(partial).sovereign_bond_principal("A")==90,
          "first actual partial redemption must update current title and principal");
    p0::DailyGame remaining(daily::SnapshotCodec::decode(
        daily::SnapshotCodec::encode(partial.world())),bidder);
    remaining.advance_day();
    const auto& after_partial=core(remaining);
    const auto replacement="bond:P0:AUCTION:A:"+std::to_string(today+2)+
                           ":STANDING_BIDDER";
    check(after_partial.shares(replacement,"STANDING_BIDDER")==60&&
          after_partial.shares("bond:MATURITY","CURRENT_HOLDER")==0&&
          after_partial.cash("CURRENT_HOLDER")==100&&
          after_partial.cash("STANDING_BIDDER")==900&&
          after_partial.cash("A_TREASURY")==0&&
          after_partial.sovereign_bond_principal("A")==90,
          "partially redeemed maturity may refinance only its remaining held face");

    auto explicit_freeze=bidder;
    explicit_freeze.national_budget_envelopes.push_back({
        "A",0,0,0,1,1,0,0,365,false,true});
    p0::DailyGame explicit_budget(daily::SnapshotCodec::decode(saved),explicit_freeze);
    explicit_budget.advance_day();
    verify_unpaid(explicit_budget);

    // Receipt ceiling equals actual gross debt. An expected redemption is
    // not prepayment and must not lend the budget more headroom.
    p0::DailyGame capped(std::move(opening),bidder);
    capped.advance_day(open_bonds(130));
    capped.advance_day();
    verify_unpaid(capped);
}

static void earned_wages_open_only_the_missing_live_household_deposit() {
    using namespace phasec;
    phasegh::World opening(24000);
    auto& geo=opening.seed_geography();
    geo.seed_province(1,"R","A");geo.seed_unit(1,"A","ROOT");
    geo.seed_assignment(1,1);
    auto& c=geo.seed_core().seed_core();
    c.seed_government("A","HEAD","PM",5,3);
    c.seed_actor("A",0);c.seed_actor("FUNDED",100);c.seed_actor("POOR",0);
    auto& pop=c.seed_population();
    const auto economy=pop.economic_profile({"R","worker","wage","employee",
                                              pop.default_education()});
    const auto adult=pop.demographic_profile({"R",23700,phaseb::Sex::female,
        "culture","faith","citizen","free",pop.default_health(),phaseb::Service::civilian});
    const auto household=[&](const char* identity,const char* employer) {
        const auto h=pop.household_profile({"R",identity,"shared",1});
        const auto worker=pop.seed_population(adult,economy,h,1);
        if(*employer)pop.hire(worker,1,employer,"worker",310,{});
        return "HH:"+std::to_string(h);
    };
    const auto paid=household("new-paid","FUNDED");
    const auto unpaid=household("new-unpaid","POOR");
    const auto known=household("already-banked","FUNDED");
    const auto idle=household("not-earning","");
    c.seed_actor(known,7);
    const auto edge_h=pop.household_profile({"R","same-slice","shared",2});
    const auto edge="HH:"+std::to_string(edge_h);
    const auto edge_worker=pop.seed_population(adult,economy,edge_h,2);
    pop.hire(edge_worker,1,"FUNDED","worker",20,{});
    p0::Policies policy;
    policy.daily_food_units_per_person=0;
    policy.monthly_clothing_units_per_person=0;
    p0::DailyGame game(daily::World::adopt(std::move(opening),649),policy);
    const auto start=daily::SnapshotCodec::encode(game.world());
    phasegh::World::Plan inspect;
    inspect[1].push_back([&](phasegh::Stage& stage) {
        const auto& live=stage.phase_c();
        check(!live.has_actor(paid)&&!live.has_actor(unpaid)&&!live.has_actor(idle)&&
               !live.has_actor(edge),
               "a Stage1 reservation opened an account before actually earned wages");
        check(live.cash(known)==7&&
               live.financial().balance("BANK:deposit")==-107&&
               live.financial().balance("BANK:reserves")==107,
               "merely opening a wallet changed existing cash or bank balances");
    });
    // January day 1 rounds one worker at wage 20 to zero. Extending the
    // same live contract to two workers creates an actual wage of one.
    inspect[2].push_back([&](phasegh::Stage& stage) {
        stage.phase_c().hire(edge_worker,1,"FUNDED",20);
    });
    auto abort=inspect;
    abort[9].push_back([](phasegh::Stage&) {throw std::runtime_error("discard payroll day");});
    bool rolled_back=false;
    try {game.advance_day(abort);}
    catch(const std::runtime_error& e) {
        rolled_back=std::string_view(e.what())=="discard payroll day";
        if(!rolled_back)throw;
    }
    check(rolled_back&&daily::SnapshotCodec::encode(game.world())==start,
           "discarded payroll day left a deposit account or payment behind");
    game.advance_day(inspect);
    const auto& live=game.world().engine().base().core().core();
    check(live.cash(paid)==9&&live.cash(known)==16&&live.cash(unpaid)==0&&
           live.cash(edge)==1&&live.cash("A")==2&&live.cash("FUNDED")==79&&
           live.outstanding_worker_wages("POOR")==10&&
           !live.has_actor(idle)&&live.financial().balanced()&&
           live.financial().balance("BANK:deposit")==-107,
           "lawful new deposit paid improperly, invented income or lost arrears");
    p0::DailyGame replay(daily::SnapshotCodec::decode(
        daily::SnapshotCodec::encode(game.world())),policy);
    game.advance_day();replay.advance_day();
    check(daily::SnapshotCodec::encode(game.world())==
           daily::SnapshotCodec::encode(replay.world())&&
           game.world().engine().base().core().core().cash(paid)==18&&
           game.world().engine().base().core().core().outstanding_worker_wages("POOR")==20,
           "new household deposit failed next-day checkpoint replay");
}

static void wallet_required_staffing_cannot_activate_unbanked_inactive_adults() {
    using namespace phasec;
    phasegh::World opening(24000);
    auto& geo=opening.seed_geography();
    geo.seed_province(1,"R","A");geo.seed_unit(1,"A","ROOT");
    geo.seed_assignment(1,1);
    auto& c=geo.seed_core().seed_core();
    c.seed_government("A","HEAD","PM",5,3);
    c.seed_p1_constitution("A",{P1GovernancePrinciple::monarchy,
        P1PowerDistribution::autocracy,P1LegislatureKind::none,"fixture:politics"});
    c.seed_actor("A",0);c.seed_actor("FIRM",100);
    c.seed_interest_group("UNION");
    c.p3_seed_group({"A","UNION","","custom_fixture","favored",
        P3GroupType::trade_unions,0,0});
    c.p2_seed_base({"fixture-law","A","custom_fixture","national","favored","fixture:law"});
    auto& pop=c.seed_population();
    const auto idle=pop.economic_profile({"R","adult","inactive","unemployed",
                                           pop.default_education()});
    const auto adult=pop.demographic_profile({"R",23700,phaseb::Sex::female,
        "culture","faith","citizen","free",pop.default_health(),phaseb::Service::civilian});
    const auto representative=pop.household_profile({"R","member","shared",1});
    const auto member=pop.seed_population(adult,idle,representative,1);
    const auto hh=pop.household_profile({"R","inactive-candidates","shared",20});
    const auto candidates=pop.seed_population(adult,idle,hh,20);
    const auto wallet="HH:"+std::to_string(hh);
    auto world=daily::World::adopt(std::move(opening),650);
    phasegh::World::Plan join;
    join[9].push_back([member](phasegh::Stage& stage) {
        stage.phase_c().join_interest_group(member,"UNION",1);
    });
    world.advance_day(join);
    check(world.engine().base().core().core().p3_trait_modifier_ppm(
              "A","state_working_adult_ratio_add")==50'000,
          "fixture did not activate the existing working adult trait");
    const auto checkpoint=daily::SnapshotCodec::encode(world);
    for(const auto guarded:{true,false}) {
        p0::Policies policy;
        policy.daily_food_units_per_person=0;
        policy.monthly_clothing_units_per_person=0;
        policy.staffing.push_back({"FIRM","R",1,310,guarded});
        p0::DailyGame game(daily::SnapshotCodec::decode(checkpoint),policy);
        game.advance_day();
        const auto& live=game.world().engine().base().core().core();
        check(live.has_actor(wallet)==!guarded&&
              live.people().slice(candidates).count==(guarded?20:19)&&
              live.cash("FIRM")== (guarded?100:90),
              "adult trait bypassed the required-wallet policy or blocked explicit payroll");
        if(!guarded)
            check(live.cash(wallet)==9&&live.cash("A")==1,
                  "authorized explicit hire did not receive its actual first wage");
    }
}

static void late_sale_cash_pays_earned_wages_before_bank_service() {
    using namespace phasec;
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"R","A");
    geo.seed_unit(1,"A","ROOT");
    geo.seed_assignment(1,1);
    auto& ledger=geo.seed_core().seed_core();
    ledger.seed_government("A","HEAD","PM",5,3);
    ledger.seed_actor("A",0);
    ledger.seed_actor("FIRM",0);
    ledger.seed_actor("SUPPLIER",0);
    ledger.seed_actor("CUSTOMER",500);
    ledger.seed_market("R","widgets",500,500,500);
    ledger.seed_stock("FIRM","R","widgets",1);
    ledger.seed_production_facility("SITE","FIRM","R",1);
    ledger.seed_annual_bank_interest_ppm(1'000'000);
    auto& pop=ledger.seed_population();
    const auto hh=pop.household_profile({"R","mixed","shared",3});
    const auto wallet="HH:"+std::to_string(hh);
    ledger.seed_actor(wallet,0);
    const auto econ=pop.economic_profile({"R","worker","wage","employee",
        pop.default_education()});
    const auto demo=pop.demographic_profile({"R",24000-300,
        phaseb::Sex::female,"culture","faith","citizen","free",
        pop.default_health(),phaseb::Service::civilian});
    const auto worker=pop.seed_population(demo,econ,hh,3,1);
    pop.hire(worker,1,"FIRM","worker",310,{});
    auto opening=daily::World::adopt(std::move(gh),319);
    phasegh::World::Plan origin;
    origin[1].push_back([](phasegh::Stage& stage) {
        stage.phase_c().lend("FIRM",365);
    });
    origin[5].push_back([](phasegh::Stage& stage) {
        stage.phase_c().pay("FIRM","SUPPLIER",365);
    });
    opening.advance_day(origin);
    const auto saved=daily::SnapshotCodec::encode(opening);
    phasegh::World::Plan late_sale;
    late_sale[7].push_back([](phasegh::Stage& stage) {
        auto& c=stage.phase_c();
        check(c.cash("FIRM")==0&&c.outstanding_worker_wages("FIRM")==10,
              "the real job must earn unpaid wages before the late cash arrives");
        check(c.clear_consumer_market("FIRM","CUSTOMER","R","widgets",1)==1,
              "late revenue must transfer existing goods and cash");
    });
    const auto core=[](const p0::DailyGame& game)->const IntegratedWorld& {
        return game.world().engine().base().core().core();
    };
    std::string failures;
    for(const std::string_view mode:{"manual-principal","endogenous-principal","interest"}) {
        p0::Policies policy;
        policy.daily_food_units_per_person=0;
        policy.allow_endogenous_debt_repayment=mode=="endogenous-principal";
        if(mode=="manual-principal")policy.debt_service.push_back({"FIRM",0,50});
        p0::DailyGame game(daily::SnapshotCodec::decode(saved),policy);
        game.advance_day(late_sale);
        const auto& held=core(game);
        check(held.cash(wallet)==0&&held.cash("CUSTOMER")==0&&
              held.stock("CUSTOMER","R","widgets")==1&&
              held.outstanding_worker_wages("FIRM")==10&&
              held.financial().balanced(),
              "sale, unpaid wage and balanced accounting must remain physical and exact");
        if(held.outstanding_loan("FIRM")!=365||held.cash("FIRM")!=500||
           held.financial().balance("BANK:interest_receivable")!=2) {
            failures+=std::string(mode)+" [principal="+
                std::to_string(held.outstanding_loan("FIRM"))+
                ", cash="+std::to_string(held.cash("FIRM"))+
                ", booked unpaid interest="+std::to_string(
                    held.financial().balance("BANK:interest_receivable"))+"]; ";
            continue;
        }
        bool wages_paid_before_credit=false;
        phasegh::World::Plan observe;
        observe[7].push_back([&](phasegh::Stage& stage) {
            const auto& c=stage.phase_c();
            // Two gross 10 wages use the actual default 10% withholding.
            wages_paid_before_credit=c.cash(wallet)==18&&c.cash("A")==2&&
                c.cash("FIRM")==480&&
                c.outstanding_worker_wages("FIRM")==0&&
                c.outstanding_loan("FIRM")==365&&
                c.financial().balance("BANK:interest_receivable")==2;
        });
        game.advance_day(observe);
        const auto& next=core(game);
        check(wages_paid_before_credit&&next.cash(wallet)==18&&next.cash("A")==2&&
              (mode=="interest"?next.outstanding_loan("FIRM")==365:
                                next.outstanding_loan("FIRM")<365)&&
              next.financial().balance("BANK:interest_receivable")==0&&
              next.cash("FIRM")<480&&next.financial().balanced(),
              "next Stage5 must actually pay earned wages before ordinary loan service resumes");
    }
    if(!failures.empty())
        throw std::runtime_error("late-sale cash bypassed unpaid earned wages: "+failures);
}

static void collateral_cannot_be_reused_after_actual_inventory_shrinks() {
    using namespace phasec;
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"R","A");
    geo.seed_unit(1,"A","ROOT");
    geo.seed_assignment(1,1);
    auto& d=geo.seed_core();
    auto& opening=d.seed_core();
    opening.seed_actor("FIRM",0);
    opening.seed_actor("CLIENT",100);
    opening.seed_market("R","steel",10,10,10);
    opening.seed_market("R","widgets",10,10,10);
    opening.seed_stock("FIRM","R","steel",10);
    auto& pop=opening.seed_population();
    const auto hh=pop.household_profile({"R","mixed","shared",5});
    const auto econ=pop.economic_profile({"R","worker","wage","employee",
        pop.default_education()});
    const auto demo=pop.demographic_profile({"R",24000-300,
        phaseb::Sex::female,"culture","faith","citizen","free",
        pop.default_health(),phaseb::Service::civilian});
    const auto worker=pop.seed_population(demo,econ,hh,5,1);
    pop.hire(worker,1,"FIRM","worker",310,{});
    phased::ConstructionProject project;
    project.id="PLEDGED-STEEL"; project.client="CLIENT"; project.contractor="FIRM";
    project.region="R"; project.material_good="steel"; project.target_site="NEW";
    project.work_required=1; project.material_per_work=1; project.workers_per_work=1;
    project.cash_price_per_work=10; project.target_capacity=1;
    d.seed_construction(project);
    phasegh::World::Plan offers;
    offers[9].push_back([](phasegh::Stage& stage) {
        for(const auto* id:{"LINE-A","LINE-B"}) {
            phasegh::CreditFacility line;
            line.id=id; line.borrower="FIRM"; line.pledge_region="R";
            line.pledge_good="steel"; line.pledged_goods=5; line.limit=40;
            stage.offer_trade_credit(line);
        }
    });
    gh.advance(offers);
    phasegh::World::Plan consumed;
    consumed[7].push_back([](phasegh::Stage& stage) {
        stage.phase_c().consume("FIRM","R","steel",5);
    });
    gh.advance(consumed);
    check(gh.base().core().core().stock("FIRM","R","steel")==5&&
          gh.trade_credit("LINE-A").drawn==0&&gh.trade_credit("LINE-B").drawn==0,
          "actual intervening use may reduce stock while both lines are undrawn");

    p0::Policies policy;
    policy.daily_food_units_per_person=0;
    policy.allow_endogenous_investment_credit=false;
    policy.allow_endogenous_debt_repayment=false;
    p0::DailyGame game(daily::World::adopt(std::move(gh),320),policy);
    Money first=-1, extension=-1;
    const auto core=[](const p0::DailyGame& g)->const IntegratedWorld& {
        return g.world().engine().base().core().core();
    };
    phasegh::World::Plan auto_credit;
    auto_credit[1].push_back([&](phasegh::Stage& stage) {
        first=enterprise_ai::draw_documented_credit(stage,"FIRM",30);
        extension=enterprise_ai::draw_documented_credit(stage,"FIRM",20);
    });
    try {game.advance_day(auto_credit);}
    catch(const std::exception& e) {
        throw std::runtime_error(std::string("autonomous collateral day rolled back: ")+e.what());
    }
    check(first==30&&extension==10&&
          game.world().engine().trade_credit("LINE-A").drawn==40&&
          game.world().engine().trade_credit("LINE-B").drawn==0&&
          core(game).cash("FIRM")==40&&core(game).outstanding_loan("FIRM")==40&&
          core(game).financial().balance("BANK:loan")==40&&
          core(game).stock("FIRM","R","steel")==5&&core(game).financial().balanced(),
          "one actual collateral lot funds only one facility, including its same-line extension");
    const auto committed=daily::SnapshotCodec::encode(game.world());

    phasegh::World::Plan invalid;
    invalid[1].push_back([](phasegh::Stage& stage) {
        stage.draw_trade_credit("LINE-B",10);
    });
    bool rejected=false;
    try {game.advance_day(invalid);}
    catch(const std::exception&) {rejected=true;}
    check(rejected&&daily::SnapshotCodec::encode(game.world())==committed,
          "explicit invalid draw must leave both cash and G utilization unchanged");

    p0::DailyGame independently_caught(daily::SnapshotCodec::decode(committed),policy);
    phasegh::World::Plan caught;
    caught[1].push_back([](phasegh::Stage& stage) {
        bool blocked=false;
        try {stage.draw_trade_credit("LINE-B",10);}
        catch(const std::invalid_argument& error) {
            blocked=std::string_view(error.what())=="G_ERR_MISSING_COLLATERAL";
        }
        check(blocked&&stage.commerce().credit.at("LINE-B").drawn==0&&
              stage.phase_c().outstanding_loan("FIRM")==40&&
              stage.phase_c().cash("FIRM")==40,
              "invalid direct draw must fail before posting any bank cash");
    });
    independently_caught.advance_day(caught);
    check(core(independently_caught).financial().balanced(),
          "caught invalid direct draw cannot leave partial finance state");

    policy.investments.push_back({"PLEDGED-STEEL","widgets",0,1,1});
    p0::DailyGame project_game(daily::SnapshotCodec::decode(committed),policy);
    phasegh::World::Plan observe;
    observe[1].push_back([](phasegh::Stage& stage) {
        auto& c=stage.phase_c();
        check(c.phase_d_material_available("FIRM","R","steel")==0&&
              c.stock("FIRM","R","steel")==5,
              "opening encumbrance must be visible to Stage1 planners");
        bool denied=false;
        try {c.phase_g_release_reserved_goods(
            "FIRM","R","steel","G-COLLATERAL:LINE-A",5);}
        catch(const std::logic_error&) {denied=true;}
        check(denied&&c.phase_d_material_available("FIRM","R","steel")==0&&
              c.outstanding_loan("FIRM")==40,
              "public Stage1 release cannot remove an existing drawn lien");
    });
    observe[3].push_back([](phasegh::Stage& stage) {
        auto& c=stage.phase_c();
        bool denied=false;
        try {c.phase_g_release_reserved_goods(
            "FIRM","R","steel","G-COLLATERAL:LINE-A",5);}
        catch(const std::invalid_argument&) {denied=true;}
        check(denied&&c.phase_d_material_available("FIRM","R","steel")==0&&
              c.stock("FIRM","R","steel")==5&&
              c.outstanding_loan("FIRM")==40&&
              stage.commerce().credit.at("LINE-A").drawn==40,
              "generic trade settlement release cannot free existing bank collateral");
    });
    project_game.advance_day(observe);
    check(project_game.world().engine().base().core().construction("PLEDGED-STEEL").paid_work==0&&
          core(project_game).cash("CLIENT")==100&&
          core(project_game).stock("FIRM","R","steel")==5&&
          project_game.world().engine().trade_credit("LINE-A").drawn==40,
          "pledged steel cannot pay construction or be consumed by it");
}

static void caught_bank_rejection_releases_only_its_new_collateral_ticket() {
    using namespace phasec;
    constexpr Money opening_cash=std::numeric_limits<Money>::max()-5;
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"R","A");
    geo.seed_unit(1,"A","ROOT");
    geo.seed_assignment(1,1);
    auto& c=geo.seed_core().seed_core();
    c.seed_actor("FIRM",opening_cash);
    c.seed_market("R","steel",10,10,10);
    c.seed_stock("FIRM","R","steel",5);
    phasegh::World::Plan offer;
    offer[9].push_back([](phasegh::Stage& stage) {
        phasegh::CreditFacility line;
        line.id="CREDIT"; line.borrower="FIRM"; line.pledge_region="R";
        line.pledge_good="steel"; line.pledged_goods=5; line.limit=10;
        stage.offer_trade_credit(line);
    });
    gh.advance(offer);
    auto game=daily::World::adopt(std::move(gh),321);
    phasegh::World::Plan attempts;
    attempts[1].push_back([](phasegh::Stage& stage) {
        auto& live=stage.phase_c();
        bool denied=false;
        try {stage.draw_trade_credit("CREDIT",10);}
        catch(const std::overflow_error&) {denied=true;}
        check(denied&&stage.commerce().credit.at("CREDIT").drawn==0&&
              live.cash("FIRM")==opening_cash&&live.outstanding_loan("FIRM")==0&&
              live.phase_d_material_available("FIRM","R","steel")==5,
              "caught rejected first draw must return its new collateral ticket");
        stage.draw_trade_credit("CREDIT",3); // Real lower draw fits the C journal.
        check(stage.commerce().credit.at("CREDIT").drawn==3&&
              live.phase_d_material_available("FIRM","R","steel")==0,
              "same-day valid draw must use inventory freed by rejected first draw");
        denied=false;
        try {stage.draw_trade_credit("CREDIT",7);}
        catch(const std::overflow_error&) {denied=true;}
        check(denied&&stage.commerce().credit.at("CREDIT").drawn==3&&
              live.cash("FIRM")==opening_cash+3&&live.outstanding_loan("FIRM")==3&&
              live.phase_d_material_available("FIRM","R","steel")==0,
              "caught rejected extension must retain the earlier genuine lien");
    });
    game.advance_day(attempts);
    const auto& committed=game.engine().base().core().core();
    check(committed.outstanding_loan("FIRM")==3&&
          committed.financial().balance("BANK:loan")==3&&
          committed.stock("FIRM","R","steel")==5&&
          committed.financial().balanced()&&
          game.engine().trade_credit("CREDIT").drawn==3,
          "only legitimate smaller loan and existing inventory commit");
}

static void automatic_program_finance_requires_an_actual_distinct_payee() {
    using namespace phasec;
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    auto& ledger=geo.seed_core().seed_core();
    p0::Policies policy;
    policy.daily_food_units_per_person=0;
    std::uint32_t id=0;
    for(const auto* country:{"A","B","C","D"}) {
        const std::string tag=country;
        ++id;
        geo.seed_province(id,"R"+tag,tag);
        geo.seed_unit(id,tag,"ROOT");
        geo.seed_assignment(id,id);
        ledger.seed_government(tag,tag+"_HEAD",tag+"_PM",5,3);
        ledger.seed_actor(tag+"_TREASURY",0);
        ledger.seed_actor(tag+"_BIDDER",10);
        ledger.seed_country_treasury(tag,tag+"_TREASURY");
        policy.bond_buyer_orders.push_back({tag,tag+"_BIDDER",1,0,0});
    }
    ledger.seed_actor("REAL_PROVIDER",0);
    ledger.seed_actor("FROZEN_PROVIDER",0);
    policy.public_budget.push_back({"A","MISSING_PROVIDER",1,0});
    policy.public_budget.push_back({"B","B_TREASURY",1,0});
    policy.public_budget.push_back({"C","REAL_PROVIDER",1,0});
    policy.public_budget.push_back({"D","FROZEN_PROVIDER",1,0});
    policy.national_budget_envelopes.push_back({"D",0,0,0,1,1,0,0,365,false,true});
    p0::DailyGame game(daily::World::adopt(std::move(gh),322),policy);
    game.advance_day();
    const auto& c=game.world().engine().base().core().core();
    const auto unspent=[&](const std::string& country) {
        const auto fiscal=c.country_fiscal(country);
        return fiscal.bond_principal==0&&fiscal.realized_revenue==0&&
               fiscal.realized_expenditure==0&&fiscal.cash==0&&
               c.cash(country+"_BIDDER")==10;
    };
    const bool missing_no_debt=unspent("A");
    const bool self_no_debt=unspent("B");
    if(!missing_no_debt||!self_no_debt)
        throw std::runtime_error(std::string("program funding: ")+
            (missing_no_debt?"missing payee blocked":"missing payee caused real debt")+
            "; "+(self_no_debt?"self payee blocked":"self payee caused real debt"));
    const auto valid=c.country_fiscal("C");
    check(valid.bond_principal==1&&valid.realized_expenditure==1&&valid.cash==0&&
          c.cash("C_BIDDER")==9&&c.cash("REAL_PROVIDER")==1&&
          c.shares("bond:"+c.sovereign_bonds().begin()->first,"C_BIDDER")==1,
          "eligible program must pay real cash funded by its own standing bidder");
    check(unspent("D")&&c.cash("FROZEN_PROVIDER")==0&&c.financial().balanced(),
          "explicit freeze and balanced legal debt must remain authoritative");
}

static void lawful_old_age_pension_is_funded_per_retiree() {
    using namespace phasec;
    const auto core=[](const p0::DailyGame& x)->const IntegratedWorld& {
        return x.world().engine().base().core().core();
    };
    for(int mode=0;mode<8;++mode) {
        // Dated adequate, shared cap, scarce existing cash, no cash, local-only, saved cap, newly due coupon, both cap sources.
        const Money initial=mode==2?5:(mode==3||mode==6)?0:100;
        phasegh::World gh(24000);
        auto& geo=gh.seed_geography();
        auto& c=geo.seed_core().seed_core();
        for(const std::string tag:{"A","B"}) {
            const auto id=tag=="A"?1:2;
            geo.seed_province(id,"R"+tag,tag);geo.seed_unit(id,tag,"CAPITAL");
            geo.seed_assignment(id,id);
            c.seed_actor(tag,tag=="A"?initial:100);
            c.seed_government(tag,tag+"-HEAD",tag+"-PM",5,3);
            c.seed_p1_constitution(tag,{P1GovernancePrinciple::monarchy,
                P1PowerDistribution::autocracy,P1LegislatureKind::none,
                "fixture:direct executive"});
        }
        const bool saved_cap=mode==5||mode==7;
        if(saved_cap)c.seed_civilian_spending_cap_authority("A");
        c.seed_actor("BIDDER",100);
        c.seed_actor("PROVIDER",0);
        if(mode==6)c.seed_actor("COUPON-HOLDER",365);
        c.p2_seed_base({"OPEN-WELFARE","A","welfare","national",
                        "no_social_security","fixture:opening statute"});
        if(mode==4)
            c.p2_seed_base({"LOCAL-ONLY","A","fixture_local_welfare","RA","old_age_pension",
                "fixture:local lookalike",P2NormTier::local_ordinance});
        auto& p=c.seed_population();
        const auto household=[&](const std::string& region,const std::string& name,
                                 bool backed) {
            const auto h=p.household_profile({region,name,"shared",1});
            if(backed)c.seed_actor("HH:"+std::to_string(h),0);
            return h;
        };
        const auto add=[&](const std::string& region,phaseb::Id home,int birthday,
                           const std::string& labor,People n) {
            const auto d=p.demographic_profile({region,birthday,phaseb::Sex::female,
                "culture","faith","citizen","free",p.default_health(),phaseb::Service::civilian});
            const auto e=p.economic_profile({region,"worker","inactive",labor,p.default_education()});
            p.seed_population(d,e,home,n);
        };
        const auto mixed=household("RA","mixed",true);
        const auto elder=household("RA","elder",true);
        const auto young=household("RA","young",true);
        const auto absent=household("RA","missing wallet",false);
        const auto foreign=household("RB","foreign",true);
        add("RA",mixed,23200,"retired",1);
        add("RA",mixed,23100,"retired",2);
        add("RA",mixed,23600,"inactive",5);
        add("RA",elder,23200,"retired",1);
        add("RA",young,23800,"inactive",2);
        add("RA",absent,23200,"retired",2);
        add("RB",foreign,23200,"retired",3);

        p0::Policies policies;
        policies.daily_food_units_per_person=0;
        policies.old_age_pensions.push_back({"A",2});
        policies.bond_buyer_orders.push_back({"A","BIDDER",90,0,0});
        if(mode==1||saved_cap)policies.public_budget.push_back({"A","PROVIDER",2,0});
        if(mode==6) {
            policies.auto_national_budgets_from_actual_services=false;
            policies.public_budget.push_back({"A","PROVIDER",120,0});
        }
        if(mode==1||mode==7) {
            policies.national_budget_envelopes.push_back({"A",0,mode==7?9:7,0,1,1,0,0,365,false,true});
        }
        const auto opening=daily::World::adopt(std::move(gh),71);
        const auto start=opening.day_index();
        const auto effective=saved_cap?daily::to_day_index({2000,2,1}):start+2;
        p0::DailyGame game(opening,policies),closed(opening,policies);
        const auto balance=[](const IntegratedWorld& world,phaseb::Id h) {
            return world.cash("HH:"+std::to_string(h));
        };
        phasegh::World::Plan enact,remain;
        if(saved_cap) {
            remain[9].push_back([mode](phasegh::Stage& stage) {
                stage.phase_c().propose_civilian_spending_cap("A","A-PM",mode==7?2:7);
            });
            enact=remain;
        }
        if(mode==6) {
            remain[1].push_back([start](phasegh::Stage& stage) {
                stage.phase_c().issue_sovereign_bond("ORIGINAL-COUPON","A","A-PM",
                    "COUPON-HOLDER",365,1'000'000,start,start+90);
            });
            enact=remain;
        }
        if(mode!=4)enact[9].push_back([=](phasegh::Stage& stage) {
            auto& live=stage.phase_c();
            P2Bill bill;
            bill.id="PENSION";bill.country="A";bill.proposer="A-PM";
            bill.legal_basis="fixture:dated old-age statute";
            bill.effective_day=effective;
            bill.proposed_base={"ACT","A","welfare","national","old_age_pension",
                                "fixture:effective old age pension"};
            live.p2_submit_bill(std::move(bill),start);
            live.p2_approve_bill("PENSION",{});
            live.p2_promulgate_bill("PENSION","A-HEAD",start);
        });
        game.advance_day(enact);closed.advance_day(remain);
        while(game.world().day_index()<effective) {game.advance_day();closed.advance_day();}
        check(balance(core(game),mixed)==0&&balance(core(closed),mixed)==0,
              "pending pension took cash before its effective day");
        if(mode==6)
            check(core(game).cash("A")==124&&core(game).cash("COUPON-HOLDER")==1&&
                  core(game).country_fiscal("A").unpaid_coupons==0,
                  "coupon control only has new daily interest due");
        const auto saved=daily::SnapshotCodec::encode(game.world());
        p0::DailyGame replay(daily::SnapshotCodec::decode(saved),policies);
        game.advance_day();closed.advance_day();replay.advance_day();
        const auto& actual=core(game);
        const auto per=mode==0?2:(mode==1||mode==2||mode==5)?1:0;
        if(mode==6) {
            check(actual.cash("A")==3&&actual.cash("PROVIDER")==360&&
                  actual.cash("COUPON-HOLDER")==2&&actual.sovereign_bonds().size()==1&&
                  actual.country_fiscal("A").unpaid_coupons==0&&actual.cash("BIDDER")==100&&
                  balance(actual,mixed)==0&&balance(actual,elder)==0&&
                  actual.cash("A")==core(closed).cash("A")&&
                  daily::SnapshotCodec::encode(game.world())==daily::SnapshotCodec::encode(replay.world()),
                  "pension consumed cash owed to today's newly accrued sovereign coupon");
            continue;
        }
        const auto other=mode==1?6:saved_cap?64:0;
        if(saved_cap)
            check(actual.civilian_spending_cap("A")== (mode==7?2:7),
                  "the shared funded policy needs the saved cap actually in effect");
        if(mode==7)
            check(balance(actual,mixed)==0&&balance(actual,elder)==0,
                  "an authored envelope must not bypass the tighter enacted pension cap");
        check(balance(actual,mixed)==3*per&&balance(actual,elder)==per&&
              balance(actual,young)==0&&balance(actual,foreign)==0&&
              !actual.has_actor("HH:"+std::to_string(absent))&&
              balance(core(closed),mixed)==0&&balance(core(closed),elder)==0&&
              actual.cash("A")==initial-other-4*per&&actual.cash("PROVIDER")==other&&
              core(closed).cash("A")==initial-other&&actual.cash("B")==100&&
              actual.cash("BIDDER")==100&&actual.sovereign_bonds().empty()&&
              actual.people().population()==16&&actual.financial().balanced()&&
              daily::SnapshotCodec::encode(game.world())==daily::SnapshotCodec::encode(replay.world()),
              "effective pension must pay only real eligible household people in equal funded units");
    }
}

static void dated_civilian_cap_limits_combined_real_cash_payments() {
    const auto core=[](const p0::DailyGame& x)->const phasec::IntegratedWorld& {
        return x.world().engine().base().core().core();
    };
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    auto& c=geo.seed_core().seed_core();
    p0::Policies policy;
    policy.daily_food_units_per_person=0;
    unsigned id=0;
    for(const std::string tag:{"A","B","C"}) {
        ++id;
        geo.seed_province(id,"R"+tag,tag);
        geo.seed_unit(id,tag,"R"+tag);
        geo.seed_assignment(id,id);
        c.seed_actor(tag,1000);
        c.seed_government(tag,tag+"-HEAD",tag+"-PM",5,3);
        c.seed_p1_constitution(tag,{phasec::P1GovernancePrinciple::monarchy,
            phasec::P1PowerDistribution::autocracy,phasec::P1LegislatureKind::none,
            "fixture:direct executive"});
        c.p1_mark_player_country(tag,true);
        if(tag!="B")c.seed_civilian_spending_cap_authority(tag);
        for(const std::string suffix:{"-X","-Y"}) {
            c.seed_actor(tag+suffix,0);
            policy.public_budget.push_back({tag,tag+suffix,2,0});
        }
    }
    policy.national_budget_envelopes.push_back({"C",1,3,0,1,1,0,0,365,false,true});
    p0::DailyGame game(daily::World::adopt(std::move(gh),205),policy);
    const auto command=[](phasec::Money cap,bool first) {
        phasegh::World::Plan plan;
        plan[9].push_back([=](phasegh::Stage& stage) {
            stage.phase_c().propose_civilian_spending_cap("A","A-PM",cap);
            if(first)stage.phase_c().propose_civilian_spending_cap("C","C-PM",0);
        });
        return plan;
    };
    const auto reject=[&](const std::string& country,const std::string& pm,
                          phasec::Money cap,const char* reason) {
        const auto before=daily::SnapshotCodec::encode(game.world());
        phasegh::World::Plan request;
        request[9].push_back([=](phasegh::Stage& stage) {
            stage.phase_c().propose_civilian_spending_cap(country,pm,cap);
        });
        bool caught=false;
        try { game.advance_day(request); }
        catch(const std::exception& e) {
            caught=std::string_view(e.what()).find(reason)!=std::string_view::npos;
        }
        check(caught&&daily::SnapshotCodec::encode(game.world())==before,
              "invalid cap unexpectedly moved any treasury or calendar");
    };
    reject("A","A-PM",-1,"invalid civilian spending cap");
    reject("A","B-PM",3,"unauthorized civilian spending cap");
    reject("B","B-PM",3,"unauthorized civilian spending cap");
    game.advance_day(command(3,true));
    reject("A","A-PM",0,"duplicate pending civilian spending cap");
    check(core(game).cash("A")==996&&core(game).cash("B")==996&&
          core(game).cash("C")==997,"next-month cap changed today's actual cash");
    while(game.world().date()<daily::Date{2000,2,1})game.advance_day();
    const auto saved=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame replay(daily::SnapshotCodec::decode(saved),policy);
    check(core(replay).has_civilian_spending_cap_authority("A")&&
          !core(replay).has_civilian_spending_cap_authority("B")&&
          !core(replay).civilian_spending_cap("A")&&
          core(replay).pending_civilian_spending_cap("A")==
              std::pair{core(replay).month(),phasec::Money{3}},
          "unprocessed first of month must retain the authoritative pending cap");
    game.advance_day(command(0,false));replay.advance_day(command(0,false));
    const auto& c1=core(game);
    check(c1.civilian_spending_cap("A")==3&&c1.civilian_spending_cap("C")==0&&
          c1.pending_civilian_spending_cap("A")==
              std::pair{c1.month()+1,phasec::Money{0}}&&c1.cash("A")==873&&c1.cash("A-X")==64&&c1.cash("A-Y")==63&&
          c1.cash("B")==872&&c1.cash("B-X")==64&&c1.cash("B-Y")==64&&
          c1.cash("C")==904&&c1.cash("C-X")==64&&c1.cash("C-Y")==32,
          "civilian cap failed to bind two real payees to one sovereign envelope");
    while(game.world().date()<daily::Date{2000,3,1}) {
        game.advance_day();replay.advance_day();
    }
    const auto last=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame march(daily::SnapshotCodec::decode(last),policy);
    const auto before_a=core(game).country_fiscal("A");
    const auto before_b=core(game).cash("B");
    const auto before_c=core(game).cash("C");
    game.advance_day();replay.advance_day();march.advance_day();
    const auto& after=core(game);
    check(after.cash("A")==before_a.cash&&
          after.country_fiscal("A").realized_expenditure==before_a.realized_expenditure&&
          after.cash("B")==before_b-4&&after.cash("C")==before_c-3&&
          after.sovereign_bonds().empty()&&after.financial().balanced()&&
          daily::SnapshotCodec::encode(game.world())==daily::SnapshotCodec::encode(replay.world())&&
          daily::SnapshotCodec::encode(game.world())==daily::SnapshotCodec::encode(march.world()),
          "zero cap escaped daily envelope, touched rival cash, or failed save replay");
}

static void known_birthdays_change_actual_daily_staffing_and_payroll(int mode=0) {
    using namespace vic25;
    constexpr std::int64_t start=24000;
    const auto rule=phaseb::LaborRule{};
    const auto person_job_cutoff=rule.maximum_age_months-(mode==2?12:0);
    phaseb::World pop(start-person_job_cutoff);
    const auto cohort=[&](const std::string& region,bool known) {
        const auto hh=pop.household_profile({region,"mixed","shared",1});
        const auto econ=pop.economic_profile({region,"not_assigned","inactive",
            "ineligible",pop.default_education()});
        const phaseb::Demographic demo{region,pop.month(),phaseb::Sex::male,
            "culture","faith","citizen","free",pop.default_health(),
            phaseb::Service::civilian};
        if(known) {
            pop.set_native_calendar_day(2);
            pop.birth(hh,demo,econ,phaseb::Sex::male,1);
        } else pop.seed_population(pop.demographic_profile(demo),econ,hh,1);
        return hh;
    };
    const auto retire_household=cohort("R2",true);
    while(pop.month()<start-rule.minimum_age_months)pop.finish_month();
    const auto entry_household=cohort("R1",true);
    const auto legacy_household=cohort("R1",false);
    check(entry_household==legacy_household,
          "fixture should exercise different birth precision in one household");
    while(pop.month()<start)pop.finish_month();
    pop.set_native_calendar_day(1);
    auto retire=pop.slices().begin()->first;
    auto economics=pop.economic(pop.slice(retire).key.economic);
    economics.profession="worker";economics.livelihood="wage";
    economics.labor_class="employee";
    retire=pop.reclassify_economic(retire,1,economics,0);
    phaseb::JobRequirements requirements;
    requirements.maximum_age_months=person_job_cutoff;
    pop.hire(retire,1,"RETIRING_FIRM","worker",310,requirements);
    auto gh=phasegh::World::from_legacy_phase_e(
        phasee::World::from_legacy_phase_d(
            phased::World::from_legacy_phase_c(
                phasec::IntegratedWorld::from_legacy_phase_b(std::move(pop)))));
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"R1","A");geo.seed_unit(1,"A","ROOT1");geo.seed_assignment(1,1);
    geo.seed_province(2,"R2","A");geo.seed_unit(2,"A","ROOT2");geo.seed_assignment(2,2);
    auto& c=geo.seed_core().seed_core();
    const auto entry_wallet="HH:"+std::to_string(entry_household);
    const auto retire_wallet="HH:"+std::to_string(retire_household);
    for(const auto& wallet:{entry_wallet,retire_wallet,std::string("A")})
        c.seed_actor(wallet,0);
    c.seed_actor("HIRING_FIRM",100);c.seed_actor("RETIRING_FIRM",100);
    phaseb::Id entering{};
    for(const auto& [id,row]:c.people().slices())
        if(row.key.household==entry_household&&c.people().exact_birth_day(id)==2)
            entering=id;
    check(entering!=0,"exact entrant must have a real stable slice");
    c.seed_person_anchor("KNOWN-RETIREMENT",retire);
    c.seed_person_anchor("KNOWN-ENTRY",entering);
    phaseb::DemographyRules rates;
    rates.fertility_ppm=rates.infant_death_ppm=rates.child_death_ppm=
        rates.adult_death_ppm=rates.senior_death_ppm=rates.very_old_death_ppm=
        rates.illness_incidence_ppm=rates.accident_ppm=0;
    rates.primary_access_ppm=rates.secondary_access_ppm=rates.tertiary_access_ppm=
        rates.adult_literacy_access_ppm=rates.training_ppm=0;
    if(mode==0)c.enable_demography(rates);
    p0::Policies policies;
    policies.daily_food_units_per_person=0;
    policies.staffing.push_back({"HIRING_FIRM","R1",1,310,true});
    p0::DailyGame game(daily::World::adopt(std::move(gh),322),policies);
    const auto core=[](const p0::DailyGame& sim)->const phasec::IntegratedWorld& {
        return sim.world().engine().base().core().core();
    };
    game.advance_day(); // Jan 1: both known birthdays are tomorrow.
    const auto& first=core(game);
    check(first.cash(entry_wallet)==0&&first.cash(retire_wallet)==9&&
          first.cash("A")==1&&
          first.phase_d_workforce("HIRING_FIRM")==0&&
          first.phase_d_workforce("RETIRING_FIRM")==1&&first.people().population()==3,
          "pre-birthday controls must preserve actual retirement and labor payroll");
    const auto saved=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame restored(daily::SnapshotCodec::decode(saved),policies);
    check(restored.world().date()==daily::Date{2000,1,2}&&
          daily::SnapshotCodec::encode(restored.world())==saved&&
          core(restored).people().native_calendar_day()==1&&
          core(restored).people().exact_birth_day(retire)==2&&
          core(restored).phase_d_workforce("RETIRING_FIRM")==1,
          "restoring before retirement must preserve effective date, identity and contract");
    // The bare native clock also has to run a genuinely due birthday
    // without a policy, a hazard tick, or a fabricated job command.
    if(mode!=0) {
        auto bare=daily::SnapshotCodec::decode(saved);
        const auto before_commit=bare.engine().commit_id();
        const auto day=bare.advance_day();
        const auto& world=bare.engine().base().core().core();
        check(day.authoritative_commit==before_commit+1&&world.people().employed(retire)==0&&
              world.people().economic(world.people().slice(entering).key.economic)
                  .profession=="unskilled_worker",
              "bare no-hazard clock must wake for a real exact-birthday labor cutoff");
    }
    try {game.advance_day();restored.advance_day();}
    catch(const std::exception& e) {
        throw std::runtime_error("known birthday rejected in mode "+std::to_string(mode)+": "+e.what());
    }
    const auto& after=core(game);
    unsigned entrant=0,legacy=0,retired=0;
    const auto& people=after.people();
    for(const auto& [id,slice]:people.slices()) {
        const auto& econ=people.economic(slice.key.economic);
        const auto& dem=people.demographic(slice.key.demographic);
        if(dem.region=="R1"&&people.exact_birth_day(id)==2&&
           econ.profession=="unskilled_worker"&&people.employed(id)==1)++entrant;
        if(dem.region=="R1"&&people.exact_birth_day(id)==0&&
           econ.profession=="not_assigned"&&people.employed(id)==0)++legacy;
        if(dem.region=="R2"&&people.exact_birth_day(id)==2&&
           econ.profession=="worker"&&econ.labor_class==(mode==2?"employee":"retired")&&
           people.employed(id)==0)
            ++retired;
    }
    check(entrant==1&&legacy==1&&retired==1&&
          after.cash(entry_wallet)==9&&after.cash(retire_wallet)==9&&after.cash("A")==2&&
          after.cash("HIRING_FIRM")==90&&after.cash("RETIRING_FIRM")==90&&
          after.phase_e_anchor_slice("KNOWN-ENTRY")==entering&&
          after.phase_e_anchor_slice("KNOWN-RETIREMENT")==retire&&
          after.people().population()==3&&after.financial().balanced(),
          "on the known birthday, genuine entry must be staffed/paid and retirement must end before payroll");
    check(daily::SnapshotCodec::encode(game.world())==
          daily::SnapshotCodec::encode(restored.world()),
          "known birthday employment, retirement and pay must survive exact replay");
    const auto life_course=[&] {
        std::size_t n=0;
        for(const auto& event:core(game).people().events())
            if(event.kind==phaseb::EventKind::occupation&&
               event.provenance=="economic life-course transition"&&
               event.from==event.to&&(event.from==entering||event.from==retire))
                ++n;
        return n;
    };
    const auto expected_events=mode==2?1u:2u;
    check(life_course()==expected_events,
          "contract-only cutoff is not retirement; each real life-course event occurs once");
    game.advance_day();
    check(life_course()==expected_events&&core(game).cash(entry_wallet)==18&&
          core(game).cash(retire_wallet)==9,
          "continued staffing must not replay a birthday transition or retired wages");
}

static void known_future_month_birthday_does_not_end_opening_job_early(bool missing_day=false) {
    using namespace vic25;
    const std::int64_t start=missing_day?1999*12+1:24000;
    phaseb::World pop(start+(missing_day?0:1)-phaseb::LaborRule{}.maximum_age_months);
    const auto hh=pop.household_profile({"R","mixed","shared",1});
    const auto econ=pop.economic_profile({"R","worker","wage","employee",
        pop.default_education()});
    const phaseb::Demographic demo{"R",pop.month(),phaseb::Sex::male,
        "culture","faith","citizen","free",pop.default_health(),
        phaseb::Service::civilian};
    pop.set_native_calendar_day(missing_day?29:2);
    pop.birth(hh,demo,econ,phaseb::Sex::male,1);
    while(pop.month()<start)pop.finish_month();
    pop.set_native_calendar_day(1);
    const auto worker=pop.slices().begin()->first;
    pop.hire(worker,1,"FIRM","worker",310,{});
    auto gh=phasegh::World::from_legacy_phase_e(
        phasee::World::from_legacy_phase_d(
            phased::World::from_legacy_phase_c(
                phasec::IntegratedWorld::from_legacy_phase_b(std::move(pop)))));
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"R","A");geo.seed_unit(1,"A","ROOT");geo.seed_assignment(1,1);
    auto& c=geo.seed_core().seed_core();
    const auto wallet="HH:"+std::to_string(hh);
    c.seed_actor(wallet,0);c.seed_actor("A",0);c.seed_actor("FIRM",1'000);
    phaseb::DemographyRules rates;
    rates.fertility_ppm=rates.infant_death_ppm=rates.child_death_ppm=
        rates.adult_death_ppm=rates.senior_death_ppm=rates.very_old_death_ppm=
        rates.illness_incidence_ppm=rates.accident_ppm=0;
    rates.primary_access_ppm=rates.secondary_access_ppm=rates.tertiary_access_ppm=
        rates.adult_literacy_access_ppm=rates.training_ppm=0;
    c.enable_demography(rates);
    p0::Policies policies;
    policies.daily_food_units_per_person=0;
    p0::DailyGame game(daily::World::adopt(std::move(gh),323),policies);
    const auto core=[](const p0::DailyGame& sim)->const phasec::IntegratedWorld& {
        return sim.world().engine().base().core().core();
    };
    const int final_day=missing_day?28:31;
    for(int day=1;day<=final_day;++day)game.advance_day();
    if(missing_day) {
        check(game.world().date()==daily::Date{1999,3,1}&&
              core(game).cash(wallet)==279&&core(game).cash("A")==31&&
              core(game).cash("FIRM")==690&&core(game).people().employed(worker)==1&&
              core(game).people().native_calendar_day()==28&&
              core(game).people().age_months_slice(worker)==899,
              "a genuine Feb 29 worker must still work through the last actual February day");
        const auto saved=daily::SnapshotCodec::encode(game.world());
        p0::DailyGame restored(daily::SnapshotCodec::decode(saved),policies);
        game.advance_day();restored.advance_day();
        const auto& after=core(game);
        const auto& p=after.people();
        check(after.cash(wallet)==279&&after.cash("A")==31&&p.employed(worker)==0&&
              p.exact_birth_day(worker)==29&&p.age_months_slice(worker)==900&&
              p.economic(p.slice(worker).key.economic).labor_class=="retired"&&
              daily::SnapshotCodec::encode(game.world())==
              daily::SnapshotCodec::encode(restored.world()),
              "March 1 age crossing must retire before pay without inventing a Feb 28 birthday");
        game.advance_day();
        std::size_t transitions=0;
        for(const auto& e:core(game).people().events())
            if(e.kind==phaseb::EventKind::occupation&&e.from==worker&&e.to==worker)
                ++transitions;
        check(transitions==1&&core(game).cash(wallet)==279&&core(game).cash("A")==31,
              "a missing calendar date cannot duplicate retirement");
        return;
    }
    check(game.world().date()==daily::Date{2000,2,1}&&
          core(game).cash(wallet)==279&&core(game).people().employed(worker)==1,
          "January closing must not use January 31 as an age cutoff in February");
    const auto saved=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame restored(daily::SnapshotCodec::decode(saved),policies);
    check(core(restored).people().native_calendar_day()==31&&
          core(restored).people().age_months_slice(worker)==899&&
          daily::SnapshotCodec::encode(restored.world())==saved,
          "restoring at February opening must preserve the committed January civil date");
    game.advance_day();restored.advance_day();
    check(core(game).cash(wallet)==288&&core(game).people().employed(worker)==1,
          "actual February 1 work must be paid before the February 2 birthday");
    game.advance_day();restored.advance_day();
    check(core(game).cash(wallet)==288&&core(game).people().employed(worker)==0&&
          core(game).people().population()==1&&core(game).financial().balanced()&&
          daily::SnapshotCodec::encode(restored.world())==
          daily::SnapshotCodec::encode(game.world()),
          "the real February 2 retirement stops only the due and future wages");
}

static void elected_cap_requires_actual_parliament_and_limits_funded_outlays() {
    using namespace phasec;
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    auto& c=geo.seed_core().seed_core();
    const auto election=daily::to_day_index({2000,1,2});
    p0::Policies policy;
    policy.daily_food_units_per_person=policy.monthly_clothing_units_per_person=0;
    unsigned province=0;
    for(const std::string tag:{"A","B","C"}) {
        ++province;
        const auto region="R"+tag;
        geo.seed_province(province,region,tag);
        geo.seed_unit(province,tag,region);
        geo.seed_assignment(province,province);
        c.seed_actor(tag,500);
        c.seed_government(tag,tag+"-HEAD",tag+"-PM",5,3);
        c.seed_p1_constitution(tag,tag=="C"?
            P1Constitution{P1GovernancePrinciple::monarchy,P1PowerDistribution::autocracy,
                           P1LegislatureKind::none,"fixture:direct executive"}:
            P1Constitution{P1GovernancePrinciple::parliamentary_republic,P1PowerDistribution::universal_vote,
                           P1LegislatureKind::unicameral,"fixture:elected legislature"});
        if(tag=="C") {
            c.seed_civilian_spending_cap_authority(tag);
            continue;
        }
        c.p4_register_party({tag+"-MAJ",tag,tag+"-HEAD",700'000,true,true});
        c.p4_register_party({tag+"-MIN",tag,tag+"-HEAD",300'000,true,true});
        c.p4_seed_contest({tag+"-HOUSE",tag,"national","fixture:national vote",
            P4Office::legislature,P4BallotMethod::list_dhondt,P4TermMode::restart_after_poll,
            48,1'000'000,5,election,election,-1,true,true,false});
        auto& pop=c.seed_population();
        const auto d=pop.demographic_profile({region,23600,phaseb::Sex::female,
            "citizen-culture","citizen-faith","citizen","free",pop.default_health(),
            phaseb::Service::civilian});
        const auto e=pop.economic_profile({region,"worker","wage","employee",pop.default_education()});
        const auto h=pop.household_profile({region,"voters","shared",20});
        pop.seed_population(d,e,h,20);
        c.seed_actor(tag+"-PROGRAM",0);
        policy.public_budget.push_back({tag,tag+"-PROGRAM",2,0});
    }
    c.seed_elected_civilian_spending_cap_authority("A");
    p0::DailyGame game(daily::World::adopt(std::move(gh),205),policy);
    const auto state=[](const p0::DailyGame& g)->const IntegratedWorld& {
        return g.world().engine().base().core().core();
    };
    const auto propose=[](const std::string& country,const std::string& pm,
                          const std::vector<std::string>& votes,Money cap,bool direct=false) {
        phasegh::World::Plan plan;
        plan[9].push_back([=](phasegh::Stage& stage) {
            if(direct)stage.phase_c().propose_civilian_spending_cap(country,pm,cap);
            else stage.phase_c().propose_elected_civilian_spending_cap(country,pm,votes,cap);
        });
        return plan;
    };
    const auto reject=[](p0::DailyGame& game,const phasegh::World::Plan& action,const char* reason="") {
        const auto bytes=daily::SnapshotCodec::encode(game.world());
        bool denied=false;
        try {game.advance_day(action);}
        catch(const std::exception& e) {
            denied=std::string_view(e.what()).find(reason)!=std::string_view::npos;
        }
        check(denied&&daily::SnapshotCodec::encode(game.world())==bytes,
              "a rejected elected spending bill changed authoritative calendar or payout bytes");
    };
    const auto cap=[&](Money amount){return propose("A","A-PM",{"A-MAJ"},amount);};
    check(state(game).has_elected_civilian_spending_cap_authority("A")&&
          !state(game).has_elected_civilian_spending_cap_authority("B")&&
          !state(game).has_elected_civilian_spending_cap_authority("C")&&
          !state(game).has_civilian_spending_cap_authority("A")&&
          state(game).has_civilian_spending_cap_authority("C"),
          "scenario grant and live constitutional mode must not open the wrong authority");
    reject(game,cap(1),"P1 constitutional law needs the country's own election");
    game.advance_day();game.advance_day();
    check(state(game).country_election("A").seats.at("A-MAJ")>=3&&
          state(game).country_election("B").seats.at("B-MAJ")>=3,
          "both countries must use independently elected seats");
    reject(game,propose("A","A-PM",{},1,true));                 // no executive shortcut
    reject(game,propose("B","B-PM",{"B-MAJ"},1));               // no scenario grant
    reject(game,propose("A","B-PM",{"A-MAJ"},1));               // foreign head of government
    reject(game,propose("A","A-PM",{},1));                     // no vote
    reject(game,propose("A","A-PM",{"A-MIN"},1));               // no quorum
    reject(game,propose("A","A-PM",{"B-MAJ"},1));               // no borrowed foreign seats
    reject(game,propose("A","A-PM",{"A-MAJ","A-MAJ"},1));       // no duplicate vote
    reject(game,cap(-1));
    auto dissolved=p0::DailyGame(game.world(),policy);
    phasegh::World::Plan dissolution;
    const auto jan3=game.world().day_index();
    dissolution[9].push_back([jan3](phasegh::Stage& stage) {
        stage.phase_c().p4_dissolve("A-HOUSE","A-HEAD","fixture:authorized recollection",
                                    jan3,daily::to_day_index({2000,1,10}));
    });
    dissolved.advance_day(dissolution);
    reject(dissolved,cap(1),"P4 dissolved parliament");
    game.advance_day(cap(1));
    reject(game,propose("C","C-PM",{"A-MAJ"},1));               // executive grant is not elected
    reject(game,cap(0),"duplicate pending civilian spending cap");
    check(!state(game).civilian_spending_cap("A")&&
          state(game).pending_civilian_spending_cap("A")==std::pair{std::int64_t{24001},Money{1}}&&
          state(game).cash("A-PROGRAM")==6,
          "lawful vote must schedule next month rather than cut current spending");
    while(game.world().date()<daily::Date{2000,2,1})game.advance_day();
    const auto saved=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame replay(daily::SnapshotCodec::decode(saved),policy);
    check(state(replay).has_elected_civilian_spending_cap_authority("A")&&
          !state(replay).has_civilian_spending_cap_authority("A")&&
          !state(replay).civilian_spending_cap("A")&&state(replay).cash("A-PROGRAM")==62,
          "the new month has not been processed at checkpoint time");
    game.advance_day(cap(0));replay.advance_day(cap(0));
    check(state(game).civilian_spending_cap("A")==1&&state(game).cash("A-PROGRAM")==63&&
          state(game).cash("B-PROGRAM")==64&&state(game).cash("A")==437&&
          state(game).pending_civilian_spending_cap("A")==std::pair{std::int64_t{24002},Money{0}},
          "actual Stage 5 payment needs the effective elected limit, not the new pending vote");
    while(game.world().date()<daily::Date{2000,3,1}) {
        game.advance_day();replay.advance_day();
    }
    const auto spent=state(game).cash("A-PROGRAM");
    game.advance_day();replay.advance_day();
    check(state(game).civilian_spending_cap("A")==0&&!state(game).pending_civilian_spending_cap("A")&&
          state(game).cash("A-PROGRAM")==spent&&state(game).cash("B-PROGRAM")==122&&
          state(game).financial().balanced()&&daily::SnapshotCodec::encode(game.world())==
             daily::SnapshotCodec::encode(replay.world()),
          "parliamentary zero limit must stop real public payment and replay exactly");
}

int main(){
    try{
        elected_cap_requires_actual_parliament_and_limits_funded_outlays();
        known_future_month_birthday_does_not_end_opening_job_early();
        known_future_month_birthday_does_not_end_opening_job_early(true);
        dated_civilian_cap_limits_combined_real_cash_payments();
        lawful_old_age_pension_is_funded_per_retiree();
        known_birthdays_change_actual_daily_staffing_and_payroll();
        known_birthdays_change_actual_daily_staffing_and_payroll(1);
        known_birthdays_change_actual_daily_staffing_and_payroll(2);
        automatic_sovereign_budget_uses_laws_effective_today();
        automatic_budget_retains_real_coupon_only_financing();
        refinance_only_matured_sovereign_principal_with_existing_cash();
        wallet_required_staffing_cannot_activate_unbanked_inactive_adults();
        earned_wages_open_only_the_missing_live_household_deposit();
        late_sale_cash_pays_earned_wages_before_bank_service();
        collateral_cannot_be_reused_after_actual_inventory_shrinks();
        caught_bank_rejection_releases_only_its_new_collateral_ticket();
        automatic_program_finance_requires_an_actual_distinct_payee();
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
