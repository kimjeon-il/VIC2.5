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

static void known_birthdays_preserve_pre_transition_snapshot() {
    using namespace vic25;
    constexpr std::int64_t start=24000;
    const auto rule=phaseb::LaborRule{};
    phaseb::World pop(start-rule.maximum_age_months);
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
    pop.hire(retire,1,"RETIRING_FIRM","worker",310,{});
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
    phaseb::DemographyRules rates;
    rates.fertility_ppm=rates.infant_death_ppm=rates.child_death_ppm=
        rates.adult_death_ppm=rates.senior_death_ppm=rates.very_old_death_ppm=
        rates.illness_incidence_ppm=rates.accident_ppm=0;
    rates.primary_access_ppm=rates.secondary_access_ppm=rates.tertiary_access_ppm=
        rates.adult_literacy_access_ppm=rates.training_ppm=0;
    c.enable_demography(rates);
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
    // The saved calendar opens Jan 2, but employment is still valid as of
    // the committed Jan 1 computation. Decoding must not treat it as age 75.
    const auto restored=[&] {
        try { return daily::SnapshotCodec::decode(saved); }
        catch(const std::exception& e) {
            throw std::runtime_error(std::string("pre-birthday snapshot rejected: ")+e.what());
        }
    }();
    check(restored.date()==daily::Date{2000,1,2}&&
          daily::SnapshotCodec::encode(restored)==saved&&
          restored.engine().base().core().core().people().native_calendar_day()==1&&
          restored.engine().base().core().core().people().exact_birth_day(retire)==2&&
          restored.engine().base().core().core().phase_d_workforce("RETIRING_FIRM")==1,
          "restoring before retirement must preserve effective date, identity and contract");
}

int main(){
    try{
        known_birthdays_preserve_pre_transition_snapshot();
        automatic_sovereign_budget_uses_laws_effective_today();
        automatic_budget_retains_real_coupon_only_financing();
        refinance_only_matured_sovereign_principal_with_existing_cash();
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
