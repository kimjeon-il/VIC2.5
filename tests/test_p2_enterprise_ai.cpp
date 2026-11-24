#include "vic25/p0_daily.hpp"
#include "vic25/daily_snapshot.hpp"
#include "vic25/p2_enterprise_ai.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

using namespace vic25;
static void check(bool condition,const char* message) {
    if(!condition)throw std::runtime_error(message);
}
static phaseb::Id worker(phasegh::World& gh,
                         const std::string& region,
                         const std::string& employer,
                         phaseb::People size=1,
                         bool employed=true, phasec::Money monthly_wage=30) {
    auto& c=gh.seed_geography().seed_core().seed_core();
    auto& p=c.seed_population();
    const auto h=p.household_profile({region,"shared","shared",size});
    const auto econ=p.economic_profile({region,"worker","wage","employee",
                                        p.default_education()});
    const auto dem=p.demographic_profile({region,gh.month()-360,
        phaseb::Sex::female,"culture","faith","citizen","free",
        p.default_health(),phaseb::Service::civilian});
    const auto id=p.seed_population(dem,econ,h,size,
                                     static_cast<phaseb::Id>(p.slices().size()+1));
    const auto wallet="HH:"+std::to_string(p.slice(id).key.household);
    if(!c.has_actor(wallet))c.seed_actor(wallet,0);
    if(employed)p.hire(id,size,employer,"worker",monthly_wage,phaseb::JobRequirements{});
    return id;
}
static void quote_requires_real_supply_and_verified_route() {
    phasegh::World gh{24000};
    auto& e=gh.seed_geography();
    e.seed_province(1,"R","STATE");
    e.seed_province(2,"B","STATE");
    e.seed_border(1,2);
    e.seed_unit(11,"STATE","STATE-ROOT");
    e.seed_assignment(1,11);
    e.seed_assignment(2,11);
    auto& d=e.seed_core();
    auto& c=d.seed_core();
    c.seed_actor("STATE",0);
    c.seed_actor("OWNER",80);
    c.seed_actor("SELLER",100);
    c.seed_actor("CARRIER",50);
    c.seed_government("STATE","HEAD","PM",5,3);
    c.seed_market("R","steel",20,1,60,20);
    c.seed_market("R","iron",2,1,10);
    c.seed_market("R","coal",2,1,10);
    c.seed_market("B","copper",3,1,12);
    c.seed_stock("SELLER","R","iron",10);
    c.seed_stock("SELLER","R","coal",10);
    c.seed_stock("SELLER","B","copper",10);
    d.seed_standard_industries();
    d.seed_building_facility("MILL","OWNER","R",10,"vic3_115.urban.steel_mills");
    phased::IndustryRecipe steel;
    steel.id="STEEL";steel.site="MILL";steel.industry_class="MANU";
    steel.inputs={{"iron",1},{"coal",1}};
    steel.outputs={{"steel",2}};steel.workers_per_batch=1;
    d.seed_building_recipe(steel);
    worker(gh,"R","OWNER");
    const auto& real=gh.base().core().core();
    const auto& domain=gh.base().core().state();
    const auto local=enterprise_ai::evaluate(
        real,domain,gh.base().spatial(),gh.commerce(),steel,1,31);
    check(local.valid && local.margin_per_batch>0 &&
          local.freight_per_batch==0,"local observed profits incorrect");
    auto repeated=steel;
    repeated.inputs.push_back({"iron",4});
    const auto combined=enterprise_ai::evaluate(
        real,domain,gh.base().spatial(),gh.commerce(),repeated,1,31);
    check(combined.valid && combined.feasible_supply_batches==2 &&
          combined.inputs_per_batch==12 && combined.freight_per_batch==0,
          "enterprise quote must divide stock by the combined input requirement");
    auto invalid=steel;
    invalid.inputs.push_back({"iron",0});
    check(!enterprise_ai::evaluate(
        real,domain,gh.base().spatial(),gh.commerce(),invalid,1,31).valid,
        "enterprise quote let aggregation hide a nonpositive draft input");
    repeated.inputs.push_back({"iron",6});
    const auto insufficient=enterprise_ai::evaluate(
        real,domain,gh.base().spatial(),gh.commerce(),repeated,1,31);
    check(!insufficient.valid && real.stock("SELLER","R","iron")==10,
          "enterprise quote double-counted stock for a single batch or mutated it");
    steel.inputs.push_back({"copper",1});
    const auto absent=enterprise_ai::evaluate(
        real,domain,gh.base().spatial(),gh.commerce(),steel,1,31);
    check(!absent.valid,"remote goods treated as free instant inventory");
    gh.seed_verified_geographic_link("R2-R1",2,1,"road");
    phasegh::TransportEdge edge;
    edge.id="ROAD";edge.carrier="CARRIER";edge.from=2;edge.to=1;
    edge.monthly_capacity=10;edge.freight_per_unit=2;edge.transit_days=2;
    edge.mode="road";edge.evidence_id="TEST-EVIDENCE";
    edge.valid_from_month=24000;edge.valid_through_month=24100;
    gh.seed_transport_edge(edge);
    const auto routed=enterprise_ai::evaluate(
        real,domain,gh.base().spatial(),gh.commerce(),steel,1,31);
    check(routed.valid && routed.freight_per_batch==2 &&
          routed.margin_per_batch<local.margin_per_batch,
          "actual route tariff/freight failed to affect enterprise margin");
}
static void collateralized_autonomous_new_factory(bool scenario_credit) {
    phasegh::World gh{24000};
    auto& e=gh.seed_geography();
    e.seed_province(1,"R","STATE");
    e.seed_unit(11,"STATE","STATE-ROOT");
    e.seed_assignment(1,11);
    auto& d=e.seed_core();
    auto& c=d.seed_core();
    c.seed_actor("STATE",0);c.seed_actor("OWNER",1);
    c.seed_actor("BUILDER",100);c.seed_actor("SUPPLIER",10);
    c.seed_government("STATE","HEAD","PM",5,3);
    c.seed_market("R","grain",2,1,10);
    c.seed_market("R","groceries",20,1,60,10);
    c.seed_market("R","steel",5,1,20);
    c.seed_market("R","tools",10,1,30);
    c.seed_stock("SUPPLIER","R","grain",15);
    c.seed_stock("OWNER","R","tools",5);
    c.seed_stock("BUILDER","R","steel",1);
    worker(gh,"R","BUILDER");
    worker(gh,"R","OWNER",1,false);
    d.seed_standard_industries();
    phased::ConstructionProject p;
    p.id="FOOD_PLAN";p.client="OWNER";p.contractor="BUILDER";
    p.region="R";p.target_site="NEW_FOOD";p.material_good="steel";
    p.work_required=1;p.material_per_work=1;p.workers_per_work=1;
    p.cash_price_per_work=25;p.target_capacity=10;
    d.seed_building_construction(p,"vic3_115.urban.food_industries");
    phased::IndustryRecipe recipe;
    recipe.id="MAKE_GROCERIES";recipe.site="NEW_FOOD";
    recipe.industry_class="MANU";recipe.inputs={{"grain",1}};
    recipe.outputs={{"groceries",2}};recipe.workers_per_batch=1;
    d.seed_building_recipe(recipe);
    // Credit is issued ONLY after a G Stage9 secured offer, backed by real
    // inventory, and is effective in the next month (not immediately).
    phasegh::World::Plan offer;
    offer[9].push_back([](phasegh::Stage& stage) {
        phasegh::CreditFacility credit;
        credit.id="FOOD_CREDIT";credit.borrower="OWNER";
        credit.pledge_region="R";credit.pledge_good="tools";
        credit.pledged_goods=5;credit.limit=40;
        stage.offer_trade_credit(credit);
    });
    gh.advance(offer);
    check(gh.month()==24001 &&
          enterprise_ai::approved_credit_remaining(
              gh.base().core().core(),gh.commerce(),"OWNER")==40,
          "credit availability was not backed by the live G collateral ledger");
    p0::Policies policy;
    // Isolate the optional facility; ordinary investment lending is tested
    // separately and remains enabled by default.
    policy.allow_endogenous_investment_credit=false;
    if(scenario_credit)policy.allow_collateralized_enterprise_credit=true;
    p0::DailyGame game(daily::World::adopt(std::move(gh),25),policy);
    game.advance_day(); // February 1, first possible build day
    const auto& gh1=game.world().engine();
    const auto& live=gh1.base().core().core();
    const auto& project=gh1.base().core().construction("FOOD_PLAN");
    if(!scenario_credit) {
        check(project.accepted_work==0 && live.outstanding_loan("OWNER")==0 &&
              gh1.commerce().credit.at("FOOD_CREDIT").drawn==0 &&
              live.stock("BUILDER","R","steel")==1 &&
              !live.facilities().contains("NEW_FOOD"),
              "default factory planning must not draw scenario collateral credit");
        return;
    }
    check(project.accepted_work==1 && project.ready &&
          live.outstanding_loan("OWNER")>=24 &&
          live.stock("BUILDER","R","steel")==0 &&
          !live.facilities().contains("NEW_FOOD"),
          "profitable new factory must use actual bank loan, contractor and input stock");
    game.advance_day();
    check(game.world().engine().base().core().construction("FOOD_PLAN").commissioned &&
          game.world().engine().base().core().core()
            .facilities().contains("NEW_FOOD"),
          "unpaid or immediate site creation incorrectly bypassed commissioning");
    game.advance_day(); // factory hires real POP, buys real grain and produces
    const auto& live_factory=game.world().engine().base().core().core();
    check(live_factory.stock("OWNER","R","groceries")>=2 &&
          live_factory.stock("SUPPLIER","R","grain")<=14 &&
          live_factory.outstanding_loan("OWNER")>=25,
          "new enterprise did not finance and consume physical working capital");
}
static void selected_recipe_drives_production_hiring_and_restock(bool explicit_override) {
    phasegh::World gh{24000};
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"R","STATE");
    geo.seed_unit(11,"STATE","STATE-ROOT");
    geo.seed_assignment(1,11);
    auto& d=geo.seed_core();
    auto& c=d.seed_core();
    c.seed_actor("STATE",0);c.seed_actor("OWNER",150);
    c.seed_actor("RANCH",0);
    c.seed_government("STATE","HEAD","PM",5,3);
    c.seed_market("R","groceries",30,1,100,10);
    c.seed_market("R","grain",2,1,10);
    c.seed_market("R","meat",20,1,50);
    c.seed_stock("OWNER","R","grain",10);
    c.seed_stock("RANCH","R","meat",5);
    if(explicit_override)c.seed_stock("OWNER","R","meat",1);
    d.seed_standard_industries();
    d.seed_building_facility("FOOD","OWNER","R",31,
                            "vic3_115.urban.food_industries");
    phased::IndustryRecipe costly;
    costly.id="COSTLY";costly.site="FOOD";costly.industry_class="MANU";
    costly.inputs={{"grain",1},{"meat",1}};
    costly.outputs={{"groceries",2}};costly.workers_per_batch=2;
    phased::IndustryRecipe cheap=costly;
    cheap.id="CHEAP";cheap.inputs={{"grain",1}};cheap.workers_per_batch=1;
    d.seed_building_recipe(costly);
    d.seed_building_recipe(cheap);
    worker(gh,"R","OWNER",1,true,31);
    worker(gh,"R","OWNER",2,false);
    p0::Policies policy;
    if(explicit_override)policy.manufacturers.push_back({"COSTLY",10});
    p0::DailyGame game(daily::World::adopt(std::move(gh),9),policy);
    game.advance_day();
    bool cheap_produced=false,costly_produced=false;
    const auto& runs=game.world().engine().base().core().state().actual_production_runs;
    for(const auto& run:runs) {
        if(run.recipe=="CHEAP")cheap_produced=true;
        if(run.recipe=="COSTLY")costly_produced=true;
    }
    check(cheap_produced!=explicit_override && costly_produced==explicit_override,
          explicit_override ? "explicit recipe was not produced" : "selected recipe was not produced");
    const auto& ledger=game.world().engine().base().core().core();
    check(ledger.stock("RANCH","R","meat")==(explicit_override?4:5) &&
          ledger.cash("RANCH")==(explicit_override?20:0),
          "market purchase did not follow the chosen recipe");
    check(ledger.phase_d_workforce("OWNER")==(explicit_override?2:1),
          "enterprise hired labor for an inactive recipe");
}
static void sales_fund_expansion_on_an_ordinary_day() {
    auto run = [](phasec::Money shopper_wage, bool funded,
                  const std::string& proposer, const std::string& current_owner,
                  int law=0, int rival_capacity=-1, bool manufacturing=true,
                  bool different_good=false, bool coproduct=false,
                  bool different_region=false, bool retrofit=false,
                  int byproduct_case=0,int wage_priority=0) {
        phasegh::World gh{24000};
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"R","STATE");
        geo.seed_unit(11,"STATE","STATE-ROOT");
        geo.seed_assignment(1,11);
        if(different_region) {
            geo.seed_province(2,"OTHER_REGION","STATE");
            geo.seed_assignment(2,11);
        }
        if(law==3) {
            geo.seed_province(3,"FOREIGN_REGION","OTHER");
            geo.seed_unit(13,"OTHER","OTHER-ROOT");
            geo.seed_assignment(3,13);
        }
        auto& d=geo.seed_core();
        auto& c=d.seed_core();
        c.seed_actor("STATE",0);
        c.seed_government("STATE","HEAD","PM",5,3);
        if(law!=0) {
            const std::string authority=law==3?"OTHER":"STATE";
            if(law==3) {
                c.seed_actor(authority,0);
                c.seed_government(authority,"HEAD","PM",5,3);
            }
            c.seed_p1_constitution(authority,{phasec::P1GovernancePrinciple::monarchy,
                phasec::P1PowerDistribution::autocracy,
                phasec::P1LegislatureKind::none,"fixture:constitution"});
            phasec::P2BaseLaw act;
            act.id=authority+":economy";act.country=authority;
            act.domain=law==2||law==4?"custom_economy":"economic_system";
            act.scope=law==2?"R":"national";
            if(law==2)act.tier=phasec::P2NormTier::local_ordinance;
            act.base_choice="law_laissez_faire";act.legal_basis="fixture:economy";
            c.p2_seed_base(act);
        }
        c.seed_actor("OWNER",20);
        c.seed_actor("BUILDER",1000);
        const auto food_price=wage_priority?16:10;
        c.seed_market("R","food",food_price,food_price,food_price,wage_priority>=2?1:0);
        c.seed_market("R","ore",1,1,1);
        c.seed_market("R","steel",1,1,1);
        c.seed_production_facility("FARM","OWNER","R",31);
        c.seed_stock("OWNER","R","ore",31);
        c.seed_stock("BUILDER","R","steel",8);
        auto add_worker = [&](const std::string& employer, phasec::Money wage) {
            auto& pop=c.seed_population();
            const auto h=pop.household_profile({"R",employer,"shared",1});
            const auto e=pop.economic_profile({"R","worker","wage","employee",
                                               pop.default_education()});
            const auto dem=pop.demographic_profile({"R",23700,phaseb::Sex::female,
                "culture","faith","citizen","free",pop.default_health(),
                phaseb::Service::civilian});
            const auto slice=pop.seed_population(dem,e,h,1);
            const auto wallet="HH:"+std::to_string(h);
            c.seed_actor(wallet,0);
            pop.hire(slice,1,employer,"worker",wage,phaseb::JobRequirements{});
            return wallet;
        };
        const auto owner_worker=add_worker("OWNER",3*31);
        const auto shopper=add_worker("BUILDER",shopper_wage*31);
        phased::ConstructionProject p;
        p.id="BUILT_FARM";p.client="OWNER";p.contractor="BUILDER";
        p.region="R";p.target_site="FARM";p.material_good="steel";
        p.work_required=p.material_per_work=p.workers_per_work=1;
        p.cash_price_per_work=wage_priority?5:30;p.target_capacity=31;
        p.accepted_work=p.paid_work=p.capitalized_work=1;
        p.capitalized_value=p.cash_price_per_work;p.commissioned=true;
        d.seed_construction(p);
        if(!current_owner.empty()) {
            // A commissioned project still uses the owner's monthly allowance.
            const auto day=daily::to_day_index(daily::from_legacy_month(gh.month()));
            auto prior=p;
            const auto legacy_key="OLDER_TEMPLATE:"+std::to_string(day);
            prior.id="AUTO:PROJECT:"+(proposer.empty()?legacy_key:
                "DAILY:"+std::to_string(proposer.size())+":"+proposer+":"+legacy_key);
            prior.client=current_owner;
            prior.target_site="ALREADY_EXPANDED";
            c.seed_production_facility(prior.target_site,prior.client,"R",31);
            d.seed_construction(prior);
        }
        const auto initial_projects=current_owner.empty()?1u:2u;
        phased::IndustryRecipe recipe;
        recipe.id="MAKE_FOOD";recipe.site="FARM";
        recipe.inputs={{"ore",1}};recipe.outputs={{"food",1}};
        recipe.workers_per_batch=recipe.capacity_per_batch=1;
        if(rival_capacity>=0 && manufacturing) {
            d.seed_industry_class({"manufacturing","","Fixture manufacturing"});
            recipe.industry_class="manufacturing";
        }
        if(coproduct) {
            c.seed_market("R","tools",10,10,10);
            recipe.outputs.push_back({"tools",1});
        }
        if(byproduct_case!=0) {
            c.seed_market("R","tools",10,10,10);
            if(byproduct_case==1)recipe.byproducts={{"tools",1}};
        }
        d.seed_recipe(recipe);
        std::string pending_id;
        if(wage_priority>=2) {
            auto pending=p;
            const auto date=daily::to_day_index(daily::from_legacy_month(gh.month()));
            pending_id=wage_priority==3?"MANUAL:PRIOR_FARM":
                "AUTO:PROJECT:DAILY:5:OWNER:BUILT_FARM:"+std::to_string(date);
            pending.id=pending_id;
            pending.target_site="AUTO:SITE:PRIOR_FARM";
            pending.accepted_work=pending.paid_work=pending.capitalized_work=0;
            pending.capitalized_value=0;
            pending.commissioned=false;
            d.seed_construction(pending);
            auto method=recipe;
            method.id="AUTO:RECIPE:PRIOR_FARM";
            method.site=pending.target_site;
            d.seed_recipe(method);
        }
        if(rival_capacity>0) {
            c.seed_actor("RIVAL",0);
            c.seed_production_facility("RIVAL_SITE","RIVAL",
                different_region?"OTHER_REGION":"R",rival_capacity);
            recipe.site="RIVAL_SITE";recipe.id="RIVAL_METHOD_1";
            recipe.outputs={{different_good||byproduct_case==2?"tools":"food",1}};
            recipe.byproducts.clear();
            if(byproduct_case==2)recipe.byproducts={{"food",1}};
            recipe.needs_retrofit=retrofit;
            d.seed_recipe(recipe);
            recipe.id="RIVAL_METHOD_2";
            d.seed_recipe(recipe); // alternative procedure, never a second installation
        }
        p0::Policies policy;
        policy.daily_food_units_per_person=3;
        policy.monthly_clothing_units_per_person=0;
        policy.allow_endogenous_input_procurement=false;
        policy.allow_endogenous_investment_credit=false;
        policy.allow_collateralized_enterprise_credit=false;
        if(wage_priority==4)policy.investments.push_back({pending_id,"food",3,1,1});
        auto clock=daily::World::adopt(std::move(gh),19);
        if(wage_priority) {
            phasegh::World::Plan prior_wage;
            prior_wage[5].push_back([owner_worker,day=clock.day_index()](phasegh::Stage& stage) {
                stage.phase_c().phase_time_accrue_contract_wage(
                    "PRE-EXISTING-EARNED","OWNER",owner_worker,"STATE","STATE",100,day);
            });
            clock.advance_day(prior_wage);
        }
        p0::DailyGame game(std::move(clock),policy);
        game.advance_day();
        const auto& day1=game.world().engine().base().core();
        if(wage_priority) {
            const auto earned=day1.core().outstanding_worker_wages("OWNER");
            const auto owner_cash=day1.core().cash("OWNER");
            std::size_t projects=day1.state().constructions.size();
            Quantity paid_work=0;
            for(const auto& [id,project]:day1.state().constructions)
                if(id.starts_with("AUTO:PROJECT:")||id==pending_id)
                    paid_work+=project.paid_work;
            const bool manual=wage_priority>=3;
            if(earned!=(manual?88:83)||owner_cash!=16||paid_work!=(manual?1:0)||
               projects!=(wage_priority>=2?2u:1u))
                throw std::runtime_error(
                    "earned wage priority after real household sale: mode="+
                    std::to_string(wage_priority)+" owed="+std::to_string(earned)+
                    " owner_cash="+std::to_string(owner_cash)+
                    " projects="+std::to_string(projects)+
                    " paid_work="+std::to_string(paid_work));
            return;
        }
        check(day1.state().constructions.size()==initial_projects,
              "unfunded expansion proposed on day one");
        check(day1.core().cash("OWNER")== (funded?27:17),
              "day-one paid wage and real sale did not balance");
        check(game.world().date().day==2,"test did not reach an ordinary day");
        const auto proposal_day=game.world().day_index();
        game.advance_day();
        const auto& day2=game.world().engine().base().core();
        check(day2.core().stock("OWNER","R","ore")==29,
              "the existing farm did not consume physical inputs on both days");
        check(day2.core().cash(shopper)==(funded?10:6),
              "empty household wallet did not receive payroll and pay for consumption");
        check(day2.core().cash("OWNER")== (funded?34:14),
              "actual sales did not reach the enterprise cash balance");
        if(byproduct_case==1)
            check(day2.core().stock("OWNER","R","tools")==2,
                  "fixture failed to produce actual marketed byproduct");
        const auto count=day2.state().constructions.size();
        const bool owner_used_quota=proposer=="OWNER" ||
            (proposer.empty()&&current_owner=="OWNER");
        if(funded&&!owner_used_quota) {
            check(count==initial_projects+1,
                  "cash-backed expansion did not follow real sales (the regional share must not block it)");
            const auto id="AUTO:PROJECT:DAILY:5:OWNER:BUILT_FARM:"+
                std::to_string(proposal_day);
            const auto& proposal=day2.construction(id);
            check(proposal.accepted_work==0 && proposal.capitalized_value==0 &&
                  !day2.core().facilities().contains(proposal.target_site),
                  "a proposal granted unearned WIP or a free factory");
        } else
            check(count==initial_projects,
                  "expansion ignored actual funds or the one-proposal monthly limit");
    };
    bool priority_failed=false;
    for(int priority:{1,2,3,4}) {
        try {
            run(24,true,"","",0,-1,true,false,false,false,false,0,priority);
        } catch(const std::exception& e) {
            std::cerr<<e.what()<<"\n";
            priority_failed=true;
        }
    }
    check(!priority_failed,"automatic expansion and queued work must not spend earned wages");
    run(3,false,"","");
    run(16,true,"","");
    run(16,true,"","OWNER"); // proposal saved by the old monthly scheduler
    run(16,true,"OWNER","BUILDER"); // the due allowance stays with the proposer
    run(16,true,"BUILDER","OWNER"); // buying a factory does not spend an allowance
    run(16,true,"","",0,20); // unchanged baseline; an actual funded project
    run(16,true,"","",1,20); // 31/51; two rival recipes, one physical facility
    run(16,true,"","",1,31); // exactly 50%, not dominant
    run(16,true,"","",1,40); // minority
    run(16,true,"","",1,40,true,true); // other good cannot dilute dominance
    run(16,true,"","",1,20,false); // uncategorized production remains unchanged
    run(16,true,"","",2,20); // local lookalike
    run(16,true,"","",3,20); // foreign law
    run(16,true,"","",4,20); // custom national group lookalike
    run(16,true,"","",0,31,true,false,true); // both outputs allowed without law
    run(16,true,"","",1,31,true,false,true); // dominant in a real secondary output
    run(16,true,"","",1,40,true,false,false,true); // different market region
    run(16,true,"","",1,40,true,false,false,false,true); // unusable recipe
    run(16,true,"","",0,31,true,false,false,false,false,1); // baseline marketed byproduct
    run(16,true,"","",1,31,true,false,false,false,false,1); // incumbent leads marketed byproduct
    run(16,true,"","",1,31,true,false,false,false,false,2); // rival byproduct ties food capacity
}
static void self_funded_expansion_commissions_and_sells_real_output() {
    for(bool solvent_demand:{true,false}) {
        phasegh::World gh(24000);
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"R","STATE");geo.seed_unit(1,"STATE","DISTRICT");
        geo.seed_assignment(1,1);
        auto& industrial=geo.seed_core();
        auto& c=industrial.seed_core();
        c.seed_actor("STATE",0);c.seed_actor("MAKER",20);c.seed_actor("BUILDER",1000);
        c.seed_government("STATE","HEAD","PM",5,3);
        c.seed_market("R","food",10,10,10);c.seed_market("R","ore",1,1,1);
        c.seed_market("R","steel",1,1,1);
        c.seed_production_facility("EXISTING","MAKER","R",31);
        c.seed_stock("MAKER","R","ore",31);c.seed_stock("BUILDER","R","steel",8);
        auto add_worker=[&](const std::string& owner,const std::string& family,phasec::Money wage) {
            auto& pop=c.seed_population();
            const auto h=pop.household_profile({"R",family,"shared",1});
            const auto e=pop.economic_profile({"R","worker","wage","employee",pop.default_education()});
            const auto d=pop.demographic_profile({"R",23700,phaseb::Sex::female,
                "culture","faith","citizen","free",pop.default_health(),phaseb::Service::civilian});
            const auto resident=pop.seed_population(d,e,h,1);
            const auto wallet="HH:"+std::to_string(h);
            c.seed_actor(wallet,0);
            pop.hire(resident,1,owner,"worker",wage*31,{});
            return wallet;
        };
        const auto wage1=add_worker("MAKER","maker-one",3);
        const auto wage2=add_worker("MAKER","maker-two",3);
        const auto buyer=add_worker("BUILDER","buyer",solvent_demand?24:1);
        phased::ConstructionProject original;
        original.id="FARM_TEMPLATE";original.client="MAKER";original.contractor="BUILDER";
        original.region="R";original.target_site="EXISTING";original.material_good="steel";
        original.work_required=original.material_per_work=original.workers_per_work=1;
        original.cash_price_per_work=30;original.target_capacity=31;
        original.accepted_work=original.paid_work=original.capitalized_work=1;
        original.capitalized_value=30;original.commissioned=true;
        industrial.seed_construction(original);
        phased::IndustryRecipe method;
        method.id="FOOD";method.site="EXISTING";method.inputs={{"ore",1}};
        method.outputs={{"food",1}};method.workers_per_batch=method.capacity_per_batch=1;
        industrial.seed_recipe(method);
        p0::Policies policy;
        policy.daily_food_units_per_person=3;policy.monthly_clothing_units_per_person=0;
        policy.allow_endogenous_input_procurement=false;
        policy.allow_endogenous_investment_credit=false;
        policy.allow_collateralized_enterprise_credit=false;
        p0::DailyGame game(daily::World::adopt(std::move(gh),54),policy);
        std::optional<p0::DailyGame> replay;
        std::string new_project,new_site,new_recipe;
        Quantity previous_batches=0,previous_food=0;
        phasec::Money previous_receipts=0;
        bool saw_unpaid_draft=false,saw_paid_work=false,saw_sold_new_output=false;
        for(int tick=1;tick<=12;++tick) {
            game.advance_day();
            if(replay) {
                replay->advance_day();
                check(daily::SnapshotCodec::encode(game.world())==
                      daily::SnapshotCodec::encode(replay->world()),
                      "precommission expansion save replay changed real future transactions");
            }
            const auto& domain=game.world().engine().base().core();
            const auto& live=domain.core();
            const auto maker=live.firm_finance("MAKER");
            const auto total=live.cash("MAKER")+live.cash("BUILDER")+live.cash("STATE")+
                live.cash(wage1)+live.cash(wage2)+live.cash(buyer);
            check(total==1020&&(!solvent_demand||maker.outstanding_wages==0)&&live.outstanding_loan("MAKER")==0,
                  "self-funded production changed closed wallet cash or made a new loan/arrear");
            Quantity batches=0,new_batches=0;
            for(const auto& r:domain.state().actual_production_runs) {
                batches+=r.batches;
                if(!new_recipe.empty()&&r.recipe==new_recipe)new_batches+=r.batches;
            }
            if(new_project.empty())
                for(const auto& [id,project]:domain.state().constructions)
                    if(id.starts_with("AUTO:PROJECT:")) {
                        new_project=id;new_site=project.target_site;
                        for(const auto& [rid,recipe]:domain.state().recipes)
                            if(recipe.site==new_site)new_recipe=rid;
                        check(!new_recipe.empty()&&tick>1&&maker.receipts>0&&
                              project.paid_work==0&&!live.facilities().contains(new_site),
                              "autonomous draft preceded observed revenue or created a free plant");
                        saw_unpaid_draft=true;
                    }
            check(maker.receipts%10==0&&
                  live.stock("MAKER","R","food")+maker.receipts/10==batches&&
                  live.stock("MAKER","R","ore")+batches==31,
                  "real food purchases and physical production did not reconcile");
            if(solvent_demand) {
                const auto paid_capex=new_project.empty()?0:
                    domain.construction(new_project).paid_work*30;
                check(maker.cash==20+maker.receipts-6*tick-paid_capex&&
                      live.cash("BUILDER")==1000-24*tick+paid_capex,
                      "actual payroll, sales and paid work did not balance on the original wallets");
            }
            if(!new_project.empty()) {
                const auto& project=domain.construction(new_project);
                if(project.paid_work>0&&!saw_paid_work) {
                    check(project.paid_work==1&&project.capitalized_value==30&&
                          live.stock("BUILDER","R","steel")==7&&!project.commissioned&&
                          !live.facilities().contains(new_site),
                          "paid work must consume material and capitalize before any new plant exists");
                    saw_paid_work=true;
                    const auto save=daily::SnapshotCodec::encode(game.world());
                    replay.emplace(daily::SnapshotCodec::decode(save),policy);
                    check(daily::SnapshotCodec::encode(replay->world())==save,
                          "precommission project checkpoint changed on reload");
                }
                if(new_batches>0) {
                    check(project.commissioned&&live.facility(new_site).owner=="MAKER"&&
                          saw_paid_work&&previous_food==0&&
                          live.stock("MAKER","R","food")==0&&
                          maker.receipts-previous_receipts==20&&batches-previous_batches==2,
                          "newly commissioned plant did not add actually sold output");
                    saw_sold_new_output=true;
                    break;
                }
            }
            previous_batches=batches;
            previous_food=live.stock("MAKER","R","food");
            previous_receipts=maker.receipts;
        }
        check(saw_unpaid_draft==solvent_demand&&saw_paid_work==solvent_demand&&
              saw_sold_new_output==solvent_demand,
              "only the funded demand case should close the sale-funded physical expansion loop");
    }
}

static void sale_funded_expansion_adds_a_real_taxpaying_worker() {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"R","STATE");geo.seed_unit(1,"STATE","DISTRICT");
    geo.seed_assignment(1,1);
    auto& industrial=geo.seed_core();
    auto& c=industrial.seed_core();
    c.seed_actor("STATE",0);c.seed_actor("MAKER",30);c.seed_actor("BUILDER",1000);
    c.seed_government("STATE","HEAD","PM",5,3);
    c.seed_market("R","food",25,25,25);c.seed_market("R","ore",1,1,1);
    c.seed_market("R","steel",1,1,1);
    c.seed_production_facility("EXISTING","MAKER","R",31);
    c.seed_stock("MAKER","R","ore",31);c.seed_stock("BUILDER","R","steel",8);
    auto add_worker=[&](const std::string& owner,const std::string& family,phasec::Money wage) {
        auto& pop=c.seed_population();
        const auto h=pop.household_profile({"R",family,"shared",1});
        const auto e=pop.economic_profile({"R","worker","wage","employee",pop.default_education()});
        const auto d=pop.demographic_profile({"R",23700,phaseb::Sex::female,
            "culture","faith","citizen","free",pop.default_health(),phaseb::Service::civilian});
        const auto resident=pop.seed_population(d,e,h,1);
        const auto wallet="HH:"+std::to_string(h);
        c.seed_actor(wallet,0);
        if(wage>0)pop.hire(resident,1,owner,"worker",wage*31,{});
        return std::pair{resident,wallet};
    };
    const auto worker=add_worker("MAKER","maker",10);
    const auto idle=add_worker("MAKER","previously-idle",0);
    const auto buyer=add_worker("BUILDER","buyer",60);
    const auto helper=add_worker("BUILDER","builder",10);
    phased::ConstructionProject original;
    original.id="FARM_TEMPLATE";original.client="MAKER";original.contractor="BUILDER";
    original.region="R";original.target_site="EXISTING";original.material_good="steel";
    original.work_required=original.material_per_work=original.workers_per_work=1;
    original.cash_price_per_work=30;original.target_capacity=31;
    original.accepted_work=original.paid_work=original.capitalized_work=1;
    original.capitalized_value=30;original.commissioned=true;
    industrial.seed_construction(original);
    phased::IndustryRecipe method;
    method.id="FOOD";method.site="EXISTING";method.inputs={{"ore",1}};
    method.outputs={{"food",1}};method.workers_per_batch=method.capacity_per_batch=1;
    industrial.seed_recipe(method);
    p0::Policies policy;
    policy.daily_food_units_per_person=3;policy.monthly_clothing_units_per_person=0;
    policy.allow_endogenous_input_procurement=false;
    policy.allow_endogenous_investment_credit=false;
    policy.allow_collateralized_enterprise_credit=false;
    p0::DailyGame game(daily::World::adopt(std::move(gh),54),policy);
    auto staff_disabled=policy;
    staff_disabled.allow_endogenous_staffing=false;
    auto no_hire=p0::DailyGame(
        daily::SnapshotCodec::decode(daily::SnapshotCodec::encode(game.world())),
        staff_disabled);
    std::optional<p0::DailyGame> replay;
    std::string new_project,new_site,new_recipe;
    Quantity previous_batches=0,previous_food=0;
    phasec::Money previous_receipts=0;
    bool saw_unpaid_draft=false,saw_paid_work=false,saw_sold_new_output=false;
    int idle_paid_days=0;
    for(int tick=1;tick<=12;++tick) {
        game.advance_day();
        no_hire.advance_day();
        const auto& disabled_domain=no_hire.world().engine().base().core();
        const auto& disabled=disabled_domain.core();
        check(disabled.people().jobs().size()==3 &&
              !disabled.people().job(idle.first) &&
              disabled.cash("STATE")==8*tick,
              "recipe comparison must not bypass the disabled hiring policy");
        if(replay) {
            replay->advance_day();
            check(daily::SnapshotCodec::encode(game.world())==
                  daily::SnapshotCodec::encode(replay->world()),
                  "precommission expansion save replay changed real future transactions");
        }
        const auto& domain=game.world().engine().base().core();
        const auto& live=domain.core();
        const auto maker=live.firm_finance("MAKER");
        const auto& jobs=live.people().jobs();
        const auto new_job=jobs.find(idle.first);
        const bool hired=new_job!=jobs.end();
        if(hired) {
            check(new_job->second.employer=="MAKER" &&
                  !new_project.empty()&&domain.construction(new_project).commissioned,
                  "the formerly idle POP must fill commissioned production demand");
            if(new_job->second.wage_per_month!=310)
                throw std::runtime_error("new hire did not receive the funded regional wage: "+
                    std::to_string(new_job->second.wage_per_month));
            ++idle_paid_days;
        }
        check(jobs.size()==static_cast<std::size_t>(3+(hired?1:0)) &&
              jobs.at(worker.first).employer=="MAKER" &&
              jobs.at(buyer.first).employer=="BUILDER" &&
              jobs.at(helper.first).employer=="BUILDER",
              "new employment must not transfer an existing job");
        check(live.cash(idle.second)==9*idle_paid_days &&
              live.cash("STATE")==8*tick+idle_paid_days,
              "new employee net pay and actual national income tax must accrue");
        const auto total=live.cash("MAKER")+live.cash("BUILDER")+live.cash("STATE")+
            live.cash(worker.second)+live.cash(idle.second)+
            live.cash(buyer.second)+live.cash(helper.second);
        check(total==1030&&maker.outstanding_wages==0&&live.outstanding_loan("MAKER")==0,
              "hire and expansion must conserve total real cash without a new loan or arrear");
        Quantity batches=0,new_batches=0;
        for(const auto& r:domain.state().actual_production_runs) {
            batches+=r.batches;
            if(!new_recipe.empty()&&r.recipe==new_recipe)new_batches+=r.batches;
        }
        if(new_project.empty())
            for(const auto& [id,project]:domain.state().constructions)
                if(id.starts_with("AUTO:PROJECT:")) {
                    new_project=id;new_site=project.target_site;
                    for(const auto& [rid,recipe]:domain.state().recipes)
                        if(recipe.site==new_site)new_recipe=rid;
                    check(!new_recipe.empty()&&maker.receipts>0&&!hired&&
                          project.paid_work==0&&!live.facilities().contains(new_site),
                          "autonomous draft preceded observed revenue or created a free plant");
                    saw_unpaid_draft=true;
                }
        check(maker.receipts%25==0&&
              live.stock("MAKER","R","food")+maker.receipts/25==batches&&
              live.stock("MAKER","R","ore")+batches==31,
              "real food purchases and physical production did not reconcile");
        const auto paid_capex=new_project.empty()?0:
            domain.construction(new_project).paid_work*30;
        check(maker.cash==30+maker.receipts-10*tick-10*idle_paid_days-paid_capex&&
              live.cash("BUILDER")==1000-70*tick+paid_capex,
              "payroll, sales and paid construction must balance on original wallets");
        if(tick==1)
            check(maker.receipts==25 && live.cash(buyer.second)==29 &&
                  !hired && paid_capex==0,
                  "real wage-funded food purchase must precede expansion and recruitment");
        if(!new_project.empty()) {
            const auto& project=domain.construction(new_project);
            if(project.paid_work>0&&!saw_paid_work) {
                check(project.paid_work==1&&project.capitalized_value==30&&
                      live.stock("BUILDER","R","steel")==7&&!project.commissioned&&
                      !live.facilities().contains(new_site),
                      "paid work must consume material and capitalize before any new plant exists");
                saw_paid_work=true;
                const auto save=daily::SnapshotCodec::encode(game.world());
                replay.emplace(daily::SnapshotCodec::decode(save),policy);
                check(daily::SnapshotCodec::encode(replay->world())==save,
                      "precommission project checkpoint changed on reload");
            }
            if(new_batches>0) {
                check(project.commissioned&&live.facility(new_site).owner=="MAKER"&&
                      saw_paid_work&&previous_food==0&&
                      live.stock("MAKER","R","food")==0&&
                      maker.receipts-previous_receipts==50&&batches-previous_batches==2&&hired,
                      "newly commissioned plant did not add actually sold output");
                check(disabled_domain.construction(new_project).commissioned &&
                      disabled.phase_d_workforce("MAKER")==1 &&
                      disabled.firm_finance("MAKER").receipts+25==maker.receipts,
                      "commissioning alone must not create additional labor or sales");
                saw_sold_new_output=true;
                break;
            }
        }
        previous_batches=batches;
        previous_food=live.stock("MAKER","R","food");
        previous_receipts=maker.receipts;
    }
    check(saw_unpaid_draft&&saw_paid_work&&saw_sold_new_output&&idle_paid_days>0,
          "actual earnings must finance expansion, additional output and a new paid job");
}

static void inactive_producer_leaves_scarce_input_for_real_work(bool stocked_opening) {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"R","STATE");geo.seed_unit(1,"STATE","DISTRICT");
    geo.seed_assignment(1,1);
    auto& industry=geo.seed_core();
    auto& c=industry.seed_core();
    c.seed_actor("STATE",0);c.seed_actor("STOCKED",stocked_opening?100:0);
    c.seed_actor("ACTIVE",100);
    c.seed_actor("SUPPLIER",50);c.seed_actor("BUYER-EMPLOYER",200);
    c.seed_government("STATE","HEAD","PM",5,3);
    c.seed_market("R","ore",4,4,4,1);
    c.seed_market("R","food",20,20,20);
    c.seed_market("R","tools",30,30,30,1);
    if(stocked_opening)c.seed_stock("STOCKED","R","food",1);
    else {
        // Stage 1 cannot reserve the opening contract wage. A separate
        // sale during Stage 3 leaves enough cash to buy ore, but that does
        // not re-enable today's Stage 4 production.
        c.seed_market("R","fabric",6,6,6);
        c.seed_stock("STOCKED","R","fabric",1);
    }
    c.seed_stock("SUPPLIER","R","ore",2);
    const auto add_worker=[&](const std::string& firm,phasec::Money wage) {
        auto& pop=c.seed_population();
        const auto h=pop.household_profile({"R",firm,"shared",1});
        const auto e=pop.economic_profile({"R","worker","wage","employee",pop.default_education()});
        const auto d=pop.demographic_profile({"R",23700,phaseb::Sex::female,
            "culture","faith","citizen","free",pop.default_health(),phaseb::Service::civilian});
        const auto resident=pop.seed_population(d,e,h,1,
            static_cast<phaseb::Id>(pop.slices().size()+1));
        const auto wallet="HH:"+std::to_string(h);
        c.seed_actor(wallet,0);
        pop.hire(resident,1,firm,"worker",wage*31,{});
        return std::pair{resident,wallet};
    };
    const auto stocked=add_worker("STOCKED",1),active=add_worker("ACTIVE",1),
               supplier=add_worker("SUPPLIER",1),customer=add_worker("BUYER-EMPLOYER",25);
    check(stocked.second!=active.second&&active.second!=supplier.second,
          "the procurement control must use independent payroll accounts");
    for(const auto* site:{"STOCKED","ACTIVE","SUPPLIER"})
        c.seed_production_facility(site,site,"R",31);
    const auto seed_recipe=[&](const char* id,const char* site,const char* good,Quantity raw) {
        phased::IndustryRecipe recipe;
        recipe.id=id;recipe.site=site;recipe.inputs={{"ore",raw}};
        if(!stocked_opening&&recipe.site=="ACTIVE")recipe.inputs.push_back({"fabric",1});
        recipe.outputs={{good,1}};recipe.workers_per_batch=recipe.capacity_per_batch=1;
        industry.seed_recipe(recipe);
    };
    // The active line first buys both units in Stage 3. Legacy then lets
    // the idle line buy one back before either recipe can run in Stage 4.
    seed_recipe("1-ACTIVE","ACTIVE","tools",2);
    seed_recipe("2-STOCKED","STOCKED","food",1);
    phased::ResourceSite source;
    source.id="RAW";source.site=source.owner="SUPPLIER";
    source.region="R";source.good="ore";source.monthly_capacity=31;
    industry.seed_resource_site(source);

    p0::Policies old_policy;
    old_policy.daily_food_units_per_person=old_policy.monthly_clothing_units_per_person=0;
    old_policy.allow_endogenous_input_procurement=false;
    old_policy.allow_endogenous_staffing=false;
    old_policy.allow_endogenous_factory_expansion=false;
    old_policy.buyers.push_back({customer.first,customer.second,"STOCKED","R","food",1});
    auto new_policy=old_policy;
    new_policy.gate_local_input_procurement_by_production=true;
    auto opening=daily::World::adopt(std::move(gh),68);
    p0::DailyGame live(opening,new_policy),legacy(std::move(opening),old_policy);
    const auto core=[](const p0::DailyGame& game)->const phasec::IntegratedWorld& {
        return game.world().engine().base().core().core();
    };
    const auto observe=[](const p0::DailyGame& game) {
        const auto& state=game.world().engine().base().core().state();
        std::array<Quantity,2> runs{};
        for(const auto& run:state.actual_production_runs) {
            if(run.recipe=="2-STOCKED")runs[0]+=run.batches;
            if(run.recipe=="1-ACTIVE")runs[1]+=run.batches;
        }
        return runs;
    };
    const auto require_conservation=[&](const p0::DailyGame& game) {
        const auto& c=core(game);
        const auto made=observe(game);
        const auto& state=game.world().engine().base().core().state();
        const auto cash=c.cash("STATE")+c.cash("STOCKED")+c.cash("ACTIVE")+
            c.cash("SUPPLIER")+c.cash("BUYER-EMPLOYER")+c.cash(stocked.second)+
            c.cash(active.second)+c.cash(supplier.second)+c.cash(customer.second);
        const auto raw=c.stock("STOCKED","R","ore")+c.stock("ACTIVE","R","ore")+
                       c.stock("SUPPLIER","R","ore");
        check(cash==(stocked_opening?450:350)&&c.financial().balanced()&&
              raw+made[0]+2*made[1]==2+state.resource_sites.at(source.id).cumulative_output,
              "procurement must preserve the real wages, cash and material ledger");
    };
    live.advance_day();
    legacy.advance_day();
    check(core(legacy).stock("STOCKED","R","ore")==1&&
          core(legacy).stock("ACTIVE","R","ore")==1&&observe(legacy)[1]==0,
          "legacy control must reproduce the unnecessary purchase and blocked rival");
    check(observe(live)==std::array<Quantity,2>{0,1}&&
          core(live).stock("STOCKED","R","ore")==0&&core(live).stock("ACTIVE","R","tools")==1,
          "an idle manufacturer must leave the scarce raw material for real production");
    require_conservation(live);
    require_conservation(legacy);
    if(!stocked_opening) {
        check(core(live).firm_finance("STOCKED").receipts==6&&
              core(live).cash("STOCKED")>=4&&core(legacy).stock("ACTIVE","R","fabric")==1&&
              core(live).stock("ACTIVE","R","fabric")==0,
              "a documented wage shortfall, not insufficient shopping cash, must protect the active line");
        return;
    }
    check(core(live).firm_finance("STOCKED").receipts==20&&
          core(live).firm_finance("SUPPLIER").receipts==8&&core(live).cash(customer.second)==3&&
          core(live).stock("STOCKED","R","food")==0,
          "real paid finished sale must empty the opening stock only after Stage 3");

    const auto settled=daily::SnapshotCodec::encode(live.world());
    p0::DailyGame replay(daily::SnapshotCodec::decode(settled),new_policy);
    live.advance_day();replay.advance_day();
    check(observe(live)==std::array<Quantity,2>{1,1}&&
          core(live).firm_finance("STOCKED").receipts==40&&
          core(live).firm_finance("SUPPLIER").receipts==12&&core(live).cash(customer.second)==6,
          "after the first real output sale the originally stocked line must buy and make again");
    require_conservation(live);
    check(daily::SnapshotCodec::encode(live.world())==daily::SnapshotCodec::encode(replay.world()),
          "the post-sale procurement replay must be byte exact");
}

static void standing_ore_replenishment_uses_physical_destination_and_transit() {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"ORIGIN","B");geo.seed_unit(1,"B","SOURCE");
    geo.seed_assignment(1,1);
    geo.seed_province(2,"PORT","A");geo.seed_unit(2,"A","DESTINATION");
    geo.seed_assignment(2,2);
    auto& industrial=geo.seed_core();
    auto& c=industrial.seed_core();
    for(const auto* actor:{"A","B","MINE-1","MINE-2","MINE-3","RAIL"})
        c.seed_actor(actor,0);
    c.seed_actor("MILL",500);c.seed_actor("OTHER",500);
    c.seed_government("A","HEAD-A","PM-A",5,3);
    c.seed_government("B","HEAD-B","PM-B",5,3);
    c.seed_market("ORIGIN","ore",10,10,10);
    c.seed_market("PORT","ore",30,30,30);
    c.seed_market("PORT","tools",80,80,80);
    for(const auto* seller:{"MINE-1","MINE-2","MINE-3"})
        c.seed_stock(seller,"ORIGIN","ore",20);
    c.seed_stock("MILL","PORT","ore",2);
    c.seed_production_facility("WORKS","MILL","PORT",31);
    phased::IndustryRecipe mill;
    mill.id="TOOLS";mill.site="WORKS";mill.inputs={{"ore",1}};
    mill.outputs={{"tools",1}};mill.workers_per_batch=mill.capacity_per_batch=1;
    industrial.seed_recipe(mill);
    const auto operator_id=worker(gh,"PORT","MILL",1,true,31);
    const auto wallet="HH:"+std::to_string(c.people().slice(operator_id).key.household);
    gh.seed_verified_geographic_link("RAIL-LINK",1,2,"rail");
    phasegh::TransportEdge edge;
    edge.id=edge.carrier="RAIL";edge.from=1;edge.to=2;edge.mode="rail";
    edge.monthly_capacity=186;edge.freight_per_unit=2;edge.transit_days=3;
    edge.evidence_id="fixture:native-three-day-rail";
    edge.valid_from_month=24000;edge.valid_through_month=24002;
    gh.seed_transport_edge(edge);
    gh.seed_route({"A-PRIMARY","MINE-1","MILL","ore",1,2,62,1});
    gh.seed_route({"B-ALTERNATE","MINE-2","MILL","ore",1,2,62,1});
    gh.seed_route({"C-UNRELATED","MINE-3","OTHER","ore",1,2,62,1});
    p0::Policies legacy;
    legacy.daily_food_units_per_person=legacy.monthly_clothing_units_per_person=0;
    legacy.allow_endogenous_input_procurement=false;
    legacy.allow_endogenous_staffing=false;
    legacy.manufacturers.push_back({"TOOLS",4});
    auto policy=legacy;
    policy.standing_route_stock_targets={{"A-PRIMARY",2},{"B-ALTERNATE",2}};
    auto opening=daily::World::adopt(std::move(gh),93);
    p0::DailyGame reserved(opening,policy),game(opening,policy),
                  unbounded(std::move(opening),legacy);
    struct Observation {
        Quantity mill_ore{}, mill_cargo{}, other_ore{}, other_cargo{}, output{};
        Quantity primary{}, alternate{}, unrelated{};
        phasec::Money mill{}, worker{}, rail{}, seller{};
    };
    auto observe=[&](const p0::DailyGame& view) {
        const auto& engine=view.world().engine();
        const auto& core=engine.base().core().core();
        Observation v;
        v.mill_ore=core.stock("MILL","PORT","ore");
        v.other_ore=core.stock("OTHER","PORT","ore");
        v.output=core.stock("MILL","PORT","tools");
        v.mill=core.cash("MILL");v.worker=core.cash(wallet);
        v.rail=core.cash("RAIL");v.seller=core.cash("MINE-1");
        for(const auto& [id,cargo]:engine.commerce().daily_shipments) {
            (void)id;
            if(cargo.buyer=="MILL")v.mill_cargo+=cargo.quantity;
            if(cargo.buyer=="OTHER")v.other_cargo+=cargo.quantity;
        }
        for(const auto& fill:engine.commerce().fills) {
            if(fill.route_id=="A-PRIMARY")v.primary+=fill.quantity;
            if(fill.route_id=="B-ALTERNATE")v.alternate+=fill.quantity;
            if(fill.route_id=="C-UNRELATED")v.unrelated+=fill.quantity;
        }
        Quantity total_raw=v.mill_ore+v.mill_cargo+v.other_ore+v.other_cargo;
        phasec::Money total_cash=v.mill+v.worker+v.rail+core.cash("OTHER");
        for(const auto* seller:{"MINE-1","MINE-2","MINE-3"}) {
            total_raw+=core.stock(seller,"ORIGIN","ore");
            total_cash+=core.cash(seller);
        }
        check(total_raw+v.output==62&&total_cash==1000&&core.financial().balanced(),
              "replenishment changed the canonical ore or money ledger");
        return v;
    };
    phasegh::World::Plan reserved_stock;
    reserved_stock[1].push_back([](phasegh::Stage& stage) {
        stage.phase_c().phase_d_reserve_material("MILL","PORT","ore","contracted",1);
    });
    reserved_stock[7].push_back([](phasegh::Stage& stage) {
        stage.phase_c().phase_g_release_reserved_goods("MILL","PORT","ore","contracted",1);
    });
    reserved.advance_day(reserved_stock);
    const auto backed=observe(reserved);
    check(backed.primary==1&&backed.alternate==0&&backed.mill_ore==1&&
          backed.mill_cargo==1&&backed.output==1,
          "only free local ore counts: a separately reserved unit cannot supply the target");
    game.advance_day();unbounded.advance_day();
    const auto first=observe(game),ordinary=observe(unbounded);
    check(ordinary.primary==2&&ordinary.alternate==2&&ordinary.unrelated==2,
          "the unrestricted control must still purchase every funded rail slot");
    check(first.primary==0&&first.alternate==0&&first.unrelated==2&&
          first.mill_ore==1&&first.output==1,
          "actual stock at target must block standing purchases without disabling real production");
    game.advance_day();
    const auto dispatched=observe(game);
    check(dispatched.primary==1&&dispatched.alternate==0&&dispatched.unrelated==4&&
          dispatched.mill_ore==0&&dispatched.mill_cargo==1&&dispatched.output==2&&
          dispatched.mill==486&&dispatched.seller==10&&dispatched.rail-first.rail==6,
          "use should reopen one real shipment, counting the earlier same-key reservation");
    const auto checkpoint=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame replay(daily::SnapshotCodec::decode(checkpoint),policy);
    auto advance=[&] {
        game.advance_day();replay.advance_day();
        check(daily::SnapshotCodec::encode(game.world())==
              daily::SnapshotCodec::encode(replay.world()),
              "the standing route target and pending real rail cargo did not replay");
        return observe(game);
    };
    const auto second_dispatch=advance();
    check(second_dispatch.primary==2&&second_dispatch.alternate==0&&
          second_dispatch.mill_ore==0&&second_dispatch.mill_cargo==2&&second_dispatch.output==2,
          "a funded native cargo must count toward target without appearing as usable stock");
    const auto full_in_transit=advance();
    check(full_in_transit.primary==2&&full_in_transit.alternate==0&&
          full_in_transit.mill_ore==0&&full_in_transit.mill_cargo==2&&full_in_transit.output==2,
          "two physically travelling parcels must suppress purchases despite zero local stock");
    const auto arrived=advance();
    check(arrived.primary==2&&arrived.alternate==0&&arrived.unrelated==10&&
          arrived.mill_cargo==1&&arrived.mill_ore==0&&arrived.output==3&&arrived.worker==5,
          "paid rail cargo must actually arrive on its third day before becoming output");
    const auto renewed=advance();
    check(renewed.primary==3&&renewed.alternate==0&&renewed.unrelated==12&&
          renewed.mill_cargo==1&&renewed.mill_ore==0&&renewed.output==4,
          "actual material consumption after arrival must reopen bounded replenishment");
}

int main() {
    try {
        standing_ore_replenishment_uses_physical_destination_and_transit();
        inactive_producer_leaves_scarce_input_for_real_work(true);
        inactive_producer_leaves_scarce_input_for_real_work(false);
        quote_requires_real_supply_and_verified_route();
        collateralized_autonomous_new_factory(false);
        collateralized_autonomous_new_factory(true);
        selected_recipe_drives_production_hiring_and_restock(false);
        selected_recipe_drives_production_hiring_and_restock(true);
        sales_fund_expansion_on_an_ordinary_day();
        self_funded_expansion_commissions_and_sells_real_output();
        sale_funded_expansion_adds_a_real_taxpaying_worker();
        std::cout<<"P2 enterprise market/logistics/secured construction cases PASS\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"P2 enterprise FAIL: "<<e.what()<<"\n";return 1;
    }
}
