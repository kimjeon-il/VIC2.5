#include "vic25/p0_daily.hpp"
#include "vic25/daily_snapshot.hpp"
#include <optional>
#include <stdexcept>
#include <string>
using namespace vic25;
static void verify(bool ok,const char* why) {
    if(!ok)throw std::runtime_error(why);
}
static phasegh::World commerce_fixture(bool future=false,int future_start=24001,
        bool fixed_quotes=false,bool producer=false,
        phasec::Money freight=2,phasec::Money cash=1000,
        bool costly_remote_payroll=false) {
    phasegh::World gh(24000);
    auto& e=gh.seed_geography();
    e.seed_province(1,"SOURCE","A");
    e.seed_province(2,"PORT","B");
    e.seed_border(1,2);
    e.seed_unit(11,"A","A-admin");
    e.seed_unit(22,"B","B-admin");
    e.seed_assignment(1,11);
    e.seed_assignment(2,22);
    auto& c=e.seed_core().seed_core();
    for(const auto& actor:{"A","B","SELLER","TRADER","CARRIER"})
        c.seed_actor(actor,std::string(actor)=="TRADER"?cash:0);
    c.seed_government("A","HEAD_A","PM_A",5,3);
    c.seed_government("B","HEAD_B","PM_B",5,3);
    c.seed_market("SOURCE","ore",10,fixed_quotes?10:1,fixed_quotes?10:100,0);
    const phasec::Money port_ore=producer?9:30;
    c.seed_market("PORT","ore",port_ore,fixed_quotes?port_ore:1,
                  fixed_quotes?port_ore:100,producer?0:8);
    c.seed_stock("SELLER","SOURCE","ore",20);
    gh.seed_verified_geographic_link("test-rail-link",1,2,"rail");
    phasegh::TransportEdge edge;
    edge.id="documented-freight";edge.carrier="CARRIER";
    edge.from=1;edge.to=2;edge.mode="rail";
    edge.monthly_capacity=62;edge.freight_per_unit=freight;
    edge.transit_days=2;edge.evidence_id="fixture-rate-and-rail-document";
    edge.valid_from_month=future?future_start:23999;
    edge.valid_through_month=24012;
    gh.seed_transport_edge(edge);
    // This cheaper synthetic/legacy link is *not* historical service proof.
    // The merchant must find the dated sourced rail alternative instead.
    phasegh::TransportEdge fictional;
    fictional.id="legacy-month-only";fictional.carrier="CARRIER";
    fictional.from=1;fictional.to=2;fictional.mode="legacy";
    fictional.monthly_capacity=100;fictional.freight_per_unit=0;
    fictional.transit_days=1;
    gh.seed_transport_edge(fictional);
    if(producer) {
        auto& d=e.seed_core();
        c.seed_market("PORT","fuel",6,1,100,0);
        c.seed_market("PORT","tools",35,1,100,2);
        c.seed_stock("TRADER","PORT","fuel",2);
        c.seed_production_facility("WORKS","TRADER","PORT",31);
        phased::IndustryRecipe recipe;
        recipe.id="MAKE_TOOLS";recipe.site="WORKS";
        recipe.inputs={{"ore",2},{"fuel",2}};
        recipe.outputs={{"tools",2}};
        recipe.workers_per_batch=recipe.capacity_per_batch=1;
        d.seed_recipe(recipe);
        auto& pop=c.seed_population();
        const auto h=pop.household_profile({"PORT","mixed","shared",1});
        const auto economy=pop.economic_profile({"PORT","worker","wage","employee",
                                               pop.default_education()});
        const auto dem=pop.demographic_profile({"PORT",23700,phaseb::Sex::female,
            "culture","faith","citizen","free",pop.default_health(),
            phaseb::Service::civilian});
        const auto worker=pop.seed_population(dem,economy,h,1);
        c.seed_actor("HH:"+std::to_string(h),0);
        pop.hire(worker,1,"TRADER","worker",93,phaseb::JobRequirements{});
        if(costly_remote_payroll) {
            const auto remote_h=pop.household_profile({"SOURCE","mixed","shared",1});
            const auto remote_economy=pop.economic_profile({"SOURCE","worker","wage",
                                                           "employee",pop.default_education()});
            const auto remote_dem=pop.demographic_profile({"SOURCE",23700,phaseb::Sex::female,
                "culture","faith","citizen","free",pop.default_health(),phaseb::Service::civilian});
            const auto remote_worker=pop.seed_population(remote_dem,remote_economy,remote_h,1);
            c.seed_actor("HH:"+std::to_string(remote_h),0);
            pop.hire(remote_worker,1,"TRADER","worker",1240,phaseb::JobRequirements{});
        }
    }
    gh.validate();
    return gh;
}
static p0::Policies merchant_policy() {
    p0::Policies p;
    p.monthly_clothing_units_per_person=0;
    p.merchant_offices.push_back({"TRADER","PORT",8,0,50'000});
    return p;
}
static void merchant_discovers_seller_goods_and_path() {
    auto options=merchant_policy();
    p0::DailyGame game(daily::World::adopt(commerce_fixture(),6),options);
    verify(game.world().engine().commerce().routes.empty(),
           "merchant fixture must have no registered route");
    game.advance_day();
    const auto& c=game.world().engine().base().core().core();
    const auto& trades=game.world().engine().commerce();
    verify(trades.fills.size()==1 &&
           trades.fills.front().physical_path.size()==1 &&
           trades.fills.front().physical_path.front()=="documented-freight" &&
           trades.fills.front().route_id.starts_with("G:DISCOVERY:"),
           "merchant did not discover its own genuine service");
    verify(trades.fills.front().quantity==2 &&
           c.stock("SELLER","SOURCE","ore")==18 &&
           c.cash("TRADER")==976 &&
           c.cash("SELLER")==20 &&
           c.cash("CARRIER")==4,
           "merchant trade did not use physical assets, price or freight");
    const auto saved=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame replay(daily::SnapshotCodec::decode(saved),options);
    for(int i=0;i<2;++i){game.advance_day();replay.advance_day();}
    verify(daily::SnapshotCodec::encode(game.world())==
           daily::SnapshotCodec::encode(replay.world()),
           "merchant shipments did not survive deterministic save/replay");
    verify(game.world().engine().base().core().core().stock(
        "TRADER","PORT","ore")>=2,"real cargo never reached buyer");
}
static void no_historical_service_no_trade() {
    p0::DailyGame game(daily::World::adopt(commerce_fixture(true),5),
                       merchant_policy());
    game.advance_day();
    verify(game.world().engine().commerce().fills.empty() &&
           game.world().engine().base().core().core().stock(
               "SELLER","SOURCE","ore")==20,
           "future railway service traded before its documented start");
}
static void producer_imports_for_final_sales_not_ore_resale(
        bool future,phasec::Money freight,phasec::Money cash,bool should_trade,
        bool costly_remote_payroll=false) {
    p0::Policies options;
    options.daily_food_units_per_person=0;
    options.monthly_clothing_units_per_person=0;
    options.allow_endogenous_investment_credit=false;
    options.allow_collateralized_enterprise_credit=false;
    p0::DailyGame game(daily::World::adopt(
        commerce_fixture(future,24001,false,true,freight,cash,costly_remote_payroll),
        6),options);
    game.advance_day();
    auto core=[&]() -> const phasec::IntegratedWorld& {
        return game.world().engine().base().core().core();
    };
    auto fills=[&]() -> const std::vector<phasegh::TradeFill>& {
        return game.world().engine().commerce().fills;
    };
    verify(fills().size()==(should_trade?1u:0u),
           "production need did not discover only an affordable profitable dated input shipment");
    verify(core().stock("TRADER","PORT","ore")==0 &&
           core().stock("TRADER","PORT","tools")==0,
           "discovery teleported inputs or manufactured before arrival");
    if(!should_trade) {
        verify(core().stock("SELLER","SOURCE","ore")==20 &&
               core().cash("SELLER")==0 && core().cash("CARRIER")==0,
               "rejected shipment changed actual stock or paid a seller");
        return;
    }
    verify(fills().front().quantity==2 &&
           fills().front().physical_path==std::vector<std::string>{"documented-freight"} &&
           fills().front().route_id.starts_with("G:DISCOVERY:") &&
           core().cash("SELLER")==20 && core().cash("CARRIER")==4 &&
           core().stock("SELLER","SOURCE","ore")==18,
           "production discovery bypassed actual G fulfillment or payment");
    const auto snapshot=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame replay(daily::SnapshotCodec::decode(snapshot),options);
    game.advance_day();
    replay.advance_day();
    verify(fills().size()==1 && core().stock("TRADER","PORT","ore")==0 &&
           core().stock("TRADER","PORT","tools")==0,
           "in-flight input was duplicated or used before arrival");
    game.advance_day();
    replay.advance_day();
    verify(daily::SnapshotCodec::encode(game.world())==
           daily::SnapshotCodec::encode(replay.world()),
           "in-transit input roll-forward diverged after loading");
    verify(fills().size()==1 && core().stock("TRADER","PORT","ore")==0 &&
           core().stock("TRADER","PORT","tools")==2,
           "factory did not consume physically arrived ore in ordinary production");
}
static void earned_wage_arrears_take_priority_over_input_imports() {
    p0::Policies quiet;
    quiet.daily_food_units_per_person=0;
    quiet.monthly_clothing_units_per_person=0;
    quiet.allow_endogenous_investment_credit=false;
    quiet.allow_collateralized_enterprise_credit=false;
    quiet.allow_endogenous_input_procurement=false;
    p0::DailyGame earlier(daily::World::adopt(
        commerce_fixture(false,24001,false,true),6),quiet);
    phasegh::World::Plan additional;
    additional[5].push_back([today=earlier.world().day_index()](phasegh::Stage& stage) {
        auto& c=stage.phase_c();
        const auto worker=c.people().jobs().begin()->first;
        const auto h=c.people().slice(worker).key.household;
        c.phase_time_accrue_contract_wage(
            "earned-previous-day","TRADER","HH:"+std::to_string(h),"B","",40,today);
    });
    earlier.advance_day(additional);
    verify(earlier.world().engine().base().core().core()
               .outstanding_worker_wages("TRADER")==40,
           "wage-priority fixture did not accrue a real prior-day claim");
    quiet.allow_endogenous_input_procurement=true;
    p0::DailyGame later(earlier.world(),quiet);
    later.advance_day();
    const auto& c=later.world().engine().base().core().core();
    verify(later.world().engine().commerce().fills.empty() &&
           c.outstanding_worker_wages("TRADER")==0 &&
           c.stock("SELLER","SOURCE","ore")==20,
           "import bypassed an outstanding earned wage claim");
}
static void bank_credit_uses_actual_profit_and_balance_sheet() {
    phasec::IntegratedWorld c(24000);
    c.seed_actor("FIRM",50);
    c.seed_actor("BUYER",950);
    c.seed_region("R","A");
    c.seed_market("R","ore",10,1,100,0);
    c.seed_stock("FIRM","R","ore",20);
    c.seed_annual_bank_interest_ppm(100'000);
    const phasec::IntegratedWorld::BankCreditRule rule{};
    verify(!c.assess_investment_credit("FIRM",rule).eligible,
           "a firm with no actual sales received an unsecured AI loan");
    phasec::IntegratedWorld::Plan first;
    first[7].push_back([](phasec::IntegratedWorld& live) {
        live.trade("FIRM","BUYER","R","ore",5,10);
    });
    c.advance(first);
    const auto offer=c.assess_investment_credit("FIRM",rule);
    verify(offer.eligible&&offer.firm_available==25,
           "bank did not evaluate real recorded sales and risk limits");
    phasec::IntegratedWorld::Plan second;
    second[1].push_back([rule](phasec::IntegratedWorld& live) {
        verify(live.lend_investment_credit("FIRM",50,rule)==25,
               "unsecured borrowing bypassed the bank's actual credit ceiling");
        verify(live.lend_investment_credit("FIRM",50,rule)==0,
               "legacy monthly clearing bypassed its one Stage1 draw allowance");
    });
    second[8].push_back([](phasec::IntegratedWorld& live) {
        live.repay("FIRM",10);
    });
    c.advance(second);
    verify(c.outstanding_loan("FIRM")==15 &&
           c.financial().balance("BANK:loan")==15 &&
           c.cash("FIRM")==115,
           "bank loan and repayment did not reconcile real deposits");
    c.validate();
}

static void bank_investment_credit_has_one_per_firm_daily_draw_allowance() {
    using Core=phasec::IntegratedWorld;
    phasegh::World opening(24000);
    auto& geo=opening.seed_geography();
    geo.seed_unit(1,"A","Credit district");
    geo.seed_province(1,"R","A");geo.seed_assignment(1,1);
    auto& c=geo.seed_core().seed_core();
    c.seed_actor("FIRM",50);c.seed_actor("PEER",50);c.seed_actor("BUYER",900);
    c.seed_market("R","ore",10,1,100,0);
    c.seed_stock("FIRM","R","ore",20);c.seed_stock("PEER","R","ore",20);
    c.seed_annual_bank_interest_ppm(100'000);
    auto world=daily::World::adopt(std::move(opening),23);
    phasegh::World::Plan receipts;
    receipts[7].push_back([](phasegh::Stage& stage) {
        auto& live=stage.phase_c();
        live.trade("FIRM","BUYER","R","ore",5,10);
        live.trade("PEER","BUYER","R","ore",5,10);
    });
    world.advance_day(receipts);
    phasegh::World::Plan fragmented;
    fragmented[1].push_back([](phasegh::Stage& stage) {
        auto& live=stage.phase_c();
        const Core::BankCreditRule rule{};
        verify(live.lend_investment_credit("FIRM",0,rule)==0&&
               live.lend_investment_credit("FIRM",10,rule)==10&&
               live.assess_investment_credit("FIRM",rule).firm_available==15,
               "first actual daily investment draw did not reduce its remaining quote");
        auto stricter=rule;
        stricter.max_daily_draw_to_revenue_ppm=200'000;
        verify(live.lend_investment_credit("FIRM",50,stricter)==0,
               "a stricter rule did not reduce remaining daily headroom");
    });
    fragmented[1].push_back([](phasegh::Stage& stage) {
        auto& live=stage.phase_c();
        const Core::BankCreditRule rule{};
        auto looser=rule;
        looser.max_daily_draw_to_revenue_ppm=1'000'000;
        verify(live.lend_investment_credit("FIRM",50,looser)==15&&
               live.lend_investment_credit("FIRM",50,looser)==0&&
               live.lend_investment_credit("PEER",50,rule)==25,
               "separate requests bypassed the daily limit or shared it across firms");
    });
    const auto before=daily::SnapshotCodec::encode(world);
    auto discarded=fragmented;
    discarded[11].push_back([](phasegh::Stage&) {
        throw std::runtime_error("discard requested after actual bank draws");
    });
    bool rejected=false;
    try {world.advance_day(discarded);}
    catch(const std::runtime_error& e) {
        rejected=std::string_view(e.what())=="discard requested after actual bank draws";
        if(!rejected)throw;
    }
    verify(rejected&&daily::SnapshotCodec::encode(world)==before,
           "aborted day leaked actual credit or its daily allowance");
    world.advance_day(fragmented);
    const auto& first=world.engine().base().core().core();
    verify(first.outstanding_loan("FIRM")==25&&first.outstanding_loan("PEER")==25&&
           first.financial().balance("BANK:loan")==50&&
           first.cash("FIRM")==125&&first.cash("PEER")==125,
           "same-day credit allowance did not reconcile bank and firm balances");
    auto restored=daily::SnapshotCodec::decode(daily::SnapshotCodec::encode(world));
    phasegh::World::Plan next;
    next[1].push_back([](phasegh::Stage& stage) {
        auto& live=stage.phase_c();
        const Core::BankCreditRule rule{};
        verify(live.lend_investment_credit("FIRM",50,rule)==25&&
               live.lend_investment_credit("FIRM",1,rule)==0,
               "new date did not restore allowance or preserve total debt cap");
    });
    world.advance_day(next);restored.advance_day(next);
    const auto& final=world.engine().base().core().core();
    verify(final.outstanding_loan("FIRM")==50&&final.outstanding_loan("PEER")==25&&
           final.financial().balance("BANK:loan")==75&&
           final.financial().balance("BANK:deposit")==-1075&&
           final.cash("FIRM")==150&&final.cash("PEER")==125&&
           daily::SnapshotCodec::encode(world)==daily::SnapshotCodec::encode(restored),
           "next-date loan totals or save/replay lost daily draw isolation");
}

static void factory_freight_wages_and_fifty_day_replay(bool route_available) {
    auto gh=commerce_fixture(!route_available,24012,true);
    auto& d=gh.seed_geography().seed_core();
    auto& c=d.seed_core();
    c.seed_actor("FACTORY",10000);
    // Fixed quotes isolate physical and financial accounting from repricing.
    c.seed_market("PORT","food",25,25,25,0);
    c.seed_production_facility("MILL","FACTORY","PORT",62);
    phased::IndustryRecipe recipe;
    recipe.id="MILL_RECIPE";recipe.site="MILL";
    recipe.inputs={{"ore",1}};recipe.outputs={{"food",3}};
    recipe.workers_per_batch=1;recipe.capacity_per_batch=1;
    d.seed_recipe(recipe);
    auto& pop=c.seed_population();
    const auto h=pop.household_profile({"PORT","mixed","shared",1});
    const auto e=pop.economic_profile({"PORT","worker","wage","employee",
                                       pop.default_education()});
    const auto dem=pop.demographic_profile({"PORT",23700,phaseb::Sex::female,
        "culture","faith","citizen","free",pop.default_health(),phaseb::Service::civilian});
    const auto worker=pop.seed_population(dem,e,h,1);
    const auto wallet="HH:"+std::to_string(h);
    c.seed_actor(wallet,0);
    pop.hire(worker,1,"FACTORY","worker",930,phaseb::JobRequirements{});
    p0::Policies policy;
    policy.monthly_clothing_units_per_person=0;
    policy.allow_endogenous_investment_credit=false;
    policy.allow_collateralized_enterprise_credit=false;
    p0::DailyGame game(daily::World::adopt(std::move(gh),6),policy);
    const auto cash_total=[&](const auto& live) {
        return live.cash("A")+live.cash("B")+live.cash("SELLER")+
               live.cash("TRADER")+live.cash("CARRIER")+live.cash("FACTORY")+
               live.cash(wallet);
    };
    const auto& opening=game.world().engine().base().core().core();
    const auto initial_cash=cash_total(opening);
    std::optional<p0::DailyGame> replay;
    for(int tick=1;tick<=50;++tick) {
        game.advance_day();
        const auto& engine=game.world().engine();
        const auto& core=engine.base().core().core();
        const auto& commerce=engine.commerce();
        Quantity batches=0,ore=0,food=0;
        for(const auto& run:engine.base().core().state().actual_production_runs)
            if(run.recipe==recipe.id)batches+=run.batches;
        for(const auto& [key,qty]:core.physical_inventory()) {
            if(key.good_id=="ore")ore+=qty; // includes physical C transit once
            if(key.good_id=="food")food+=qty;
        }
        const auto food_sales=core.firm_finance("FACTORY").receipts;
        verify(food_sales%25==0 && food+food_sales/25==3*batches,
               "factory food stock or real customer purchases did not reconcile");
        verify(cash_total(core)==initial_cash,"closed scenario created or lost deposits");
        verify(ore+batches==20,"ore stock including transit did not reconcile with production");
        if(tick<=2)
            verify(batches==0 && food_sales==0,"factory consumed ore before dated arrival");
        if(tick==1 && route_available) {
            verify(commerce.routes.empty()&&commerce.fills.size()==1&&
                   commerce.fills.front().route_id.starts_with("G:DISCOVERY:")&&
                   commerce.fills.front().physical_path==std::vector<std::string>{"documented-freight"}&&
                   commerce.fills.front().quantity==2&&commerce.daily_shipments.size()==1&&
                   commerce.daily_shipments.begin()->second.buyer=="FACTORY"&&
                   core.cash("SELLER")==20&&core.cash("CARRIER")==4&&core.cash(wallet)>0,
                   "autonomous factory did not actually dispatch and pay for freight and labor");
            const auto saved=daily::SnapshotCodec::encode(game.world());
            replay.emplace(daily::SnapshotCodec::decode(saved),policy);
            verify(daily::SnapshotCodec::encode(replay->world())==saved,
                   "in-transit freight checkpoint changed when restored");
        } else if(replay) {
            replay->advance_day();
            verify(daily::SnapshotCodec::encode(replay->world())==
                   daily::SnapshotCodec::encode(game.world()),
                   "factory freight/payroll/consumption replay diverged");
        }
        if(tick==3 && route_available)
            verify(batches>0&&food_sales>0,
                   "arrived ore did not turn into goods bought by the paid household");
        if(!route_available)
            verify(commerce.fills.empty()&&batches==0&&food_sales==0&&
                   core.stock("SELLER","SOURCE","ore")==20,
                   "factory produced or moved ore without an active documented service");
    }
}

int main() {
    factory_freight_wages_and_fifty_day_replay(true);
    factory_freight_wages_and_fifty_day_replay(false);
    merchant_discovers_seller_goods_and_path();
    no_historical_service_no_trade();
    producer_imports_for_final_sales_not_ore_resale(false,2,1000,true);
    producer_imports_for_final_sales_not_ore_resale(true,2,1000,false);
    producer_imports_for_final_sales_not_ore_resale(false,18,1000,false);
    producer_imports_for_final_sales_not_ore_resale(false,2,5,false);
    producer_imports_for_final_sales_not_ore_resale(false,2,1000,false,true);
    earned_wage_arrears_take_priority_over_input_imports();
    bank_investment_credit_has_one_per_firm_daily_draw_allowance();
    bank_credit_uses_actual_profit_and_balance_sheet();
}
