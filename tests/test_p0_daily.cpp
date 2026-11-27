#include "vic25/p0_daily.hpp"
#include "vic25/daily_snapshot.hpp"
#include <initializer_list>
#include <iostream>
#include <utility>
#include <iterator>
#include <stdexcept>
#include <string>

using namespace vic25;
static void require_test(bool v,const char* msg){if(!v)throw std::runtime_error(msg);}
static phaseb::Id people(phasegh::World& gh,const std::string& region,
                  phaseb::People n,const std::string& profession,
                  phaseb::Service service=phaseb::Service::civilian) {
    auto& p=gh.seed_geography().seed_core().seed_core().seed_population();
    const auto h=p.household_profile({region,"mixed","shared",n});
    const auto econ=p.economic_profile({region,profession,"wage","employee",
                                       p.default_education()});
    const auto demo=p.demographic_profile({region,gh.month()-300,
                   phaseb::Sex::female,"culture","faith","citizen",
                   "free",p.default_health(),service});
    return p.seed_population(demo,econ,h,n,static_cast<phaseb::Id>(p.slices().size()+1));
}
static const phasec::IntegratedWorld& c(const p0::DailyGame& game) {
    return game.world().engine().base().core().core();
}
static phasegh::World fixture() {
    phasegh::World gh(24000);
    auto& e=gh.seed_geography();
    e.seed_province(1,"R","A");
    e.seed_unit(11,"A","ROOT");
    e.seed_assignment(1,11);
    auto& d=e.seed_core();
    auto& core=d.seed_core();
    core.seed_actor("A",1000);
    core.seed_actor("FIRM",2000);
    core.seed_actor("HH",1000);
    core.seed_actor("POOR",0);
    core.seed_actor("WORKERS",0);
    core.seed_actor("BUILDER",0);
    core.seed_actor("TREASURY",0);
    core.seed_market("R","food",100,1,200,0);
    core.seed_market("R","ore",10,1,30,0);
    core.seed_market("R","steel",10,1,30,0);
    core.seed_production_facility("FARM","FIRM","R",62);
    core.seed_stock("FIRM","R","ore",100);
    core.seed_stock("BUILDER","R","steel",2);
    core.seed_government("A","headA","PM_A",5,3);
    core.seed_interest_group("FOOD_MOVEMENT");
    auto workforce=people(gh,"R",4,"worker");
    auto builders=people(gh,"R",2,"worker");
    auto civilians=people(gh,"R",10,"civilian");
    (void)workforce;(void)builders;(void)civilians;
    phased::IndustryRecipe r;
    r.id="FOOD_RECIPE";r.site="FARM";
    r.inputs={{"ore",1}};r.outputs={{"food",2}};
    r.workers_per_batch=1;r.capacity_per_batch=1;
    d.seed_recipe(r);
    phased::ConstructionProject proj;
    proj.id="NEW_FARM";proj.client="FIRM";proj.contractor="BUILDER";
    proj.region="R";proj.material_good="steel";proj.target_site="FARM2";
    proj.work_required=2;proj.material_per_work=1;proj.workers_per_work=1;
    proj.cash_price_per_work=10;proj.target_capacity=5;
    d.seed_construction(proj);
    gh.validate();
    return gh;
}
static void fitting_purchase_requires_wages_and_consumes_real_stock() {
    const auto scenario=[](phasec::Money wage_per_day) {
        phasegh::World gh(24000);
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"R","A");geo.seed_unit(1,"A","R");
        geo.seed_assignment(1,1);
        auto& core=geo.seed_core().seed_core();
        core.seed_actor("A",0);core.seed_actor("EMPLOYER",200);
        core.seed_actor("WORKSHOP",0);
        core.seed_government("A","HEAD","PM",5,3);
        core.seed_market("R","fittings",90,90,90);
        core.seed_stock("WORKSHOP","R","fittings",1);
        const auto resident=people(gh,"R",1,"worker");
        auto& pop=core.seed_population();
        const auto wallet="HH:"+std::to_string(pop.slice(resident).key.household);
        core.seed_actor(wallet,0);
        pop.hire(resident,1,"EMPLOYER","worker",31*wage_per_day,{});
        p0::Policies policies;policies.daily_food_units_per_person=0;
        policies.buyers.push_back({resident,wallet,"WORKSHOP","R","fittings",1});
        return std::pair{p0::DailyGame(daily::World::adopt(std::move(gh),55),policies),wallet};
    };
    auto [funded,paid_wallet]=scenario(100);
    auto [short_funds,short_wallet]=scenario(98);
    const auto opening=daily::SnapshotCodec::encode(funded.world());
    p0::DailyGame replay(daily::SnapshotCodec::decode(opening),funded.policies());
    funded.advance_day();short_funds.advance_day();replay.advance_day();
    const auto& f=c(funded);
    const auto& u=c(short_funds);
    require_test(f.cash("EMPLOYER")==100&&f.cash("A")==10&&f.cash(paid_wallet)==0&&
                 f.cash("WORKSHOP")==90&&f.firm_finance("WORKSHOP").receipts==90&&
                 f.stock("WORKSHOP","R","fittings")==0&&
                 f.stock(paid_wallet,"R","fittings")==0&&
                 u.cash("EMPLOYER")==102&&u.cash("A")==9&&u.cash(short_wallet)==89&&
                 u.cash("WORKSHOP")==0&&u.firm_finance("WORKSHOP").receipts==0&&
                 u.stock("WORKSHOP","R","fittings")==1&&
                 u.stock(short_wallet,"R","fittings")==0&&
                 daily::SnapshotCodec::encode(funded.world())==
                     daily::SnapshotCodec::encode(replay.world()),
                 "the household must earn enough actual net wages to buy and consume the fitting");
}
static void independent_workshop_must_pay_for_its_real_inputs() {
    const auto scenario=[](phasec::Money own_cash,bool needs_board=false,
                           bool missing_input=false) {
        phasegh::World gh(24000);
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"R","A");geo.seed_unit(1,"A","R");
        geo.seed_assignment(1,1);
        auto& industry=geo.seed_core();
        auto& core=industry.seed_core();
        core.seed_actor("A",0);core.seed_actor("WORKSHOP",own_cash);
        core.seed_government("A","HEAD","PM",5,3);
        core.seed_market("R","tools",60,60,60);
        const phasec::Money board_price=needs_board?20:4;
        core.seed_market("R","boards",board_price,board_price,board_price);
        core.seed_market("R","fittings",90,90,90,1);
        if(needs_board) {
            core.seed_actor("BOARD-VENDOR",0);
            if(!missing_input)core.seed_stock("BOARD-VENDOR","R","boards",1);
            core.seed_stock("WORKSHOP","R","tools",1);
        } else {
            if(!missing_input)core.seed_stock("A","R","tools",1);
            core.seed_stock("WORKSHOP","R","boards",1);
        }
        core.seed_production_facility("SITE","WORKSHOP","R",31);
        const auto worker=people(gh,"R",1,"worker");
        auto& pop=core.seed_population();
        const auto wallet="HH:"+std::to_string(pop.slice(worker).key.household);
        core.seed_actor(wallet,0);
        pop.hire(worker,1,"WORKSHOP","worker",31,{});
        phased::IndustryRecipe r;r.id="FITTINGS";r.site="SITE";
        r.inputs={{"tools",1},{"boards",1}};r.outputs={{"fittings",1}};
        r.workers_per_batch=r.capacity_per_batch=1;
        industry.seed_recipe(r);
        p0::Policies policies;policies.daily_food_units_per_person=0;
        return p0::DailyGame(daily::World::adopt(std::move(gh),55),policies);
    };
    auto funded=scenario(61), short_cash=scenario(60);
    funded.advance_day();short_cash.advance_day();
    const auto& f=c(funded);
    const auto& u=c(short_cash);
    require_test(f.cash("A")==60&&f.country_fiscal("A").realized_revenue==60&&
                 f.cash("WORKSHOP")==0&&f.stock("A","R","tools")==0&&
                 f.stock("WORKSHOP","R","tools")==0&&
                 f.stock("WORKSHOP","R","boards")==0&&
                 f.stock("WORKSHOP","R","fittings")==1&&
                 u.cash("A")==0&&u.country_fiscal("A").realized_revenue==0&&
                 u.cash("WORKSHOP")==59&&u.stock("A","R","tools")==1&&
                 u.stock("WORKSHOP","R","tools")==0&&
                 u.stock("WORKSHOP","R","boards")==1&&
                 u.stock("WORKSHOP","R","fittings")==0,
                 "the workshop must afford the physical tool and ordinary payroll");
    auto stocked=scenario(21,true), cannot_restock=scenario(20,true);
    stocked.advance_day();cannot_restock.advance_day();
    const auto& paid=c(stocked);
    const auto& denied=c(cannot_restock);
    require_test(paid.cash("WORKSHOP")==0&&paid.cash("BOARD-VENDOR")==20&&
                 paid.firm_finance("BOARD-VENDOR").receipts==20&&
                 paid.stock("BOARD-VENDOR","R","boards")==0&&
                 paid.stock("WORKSHOP","R","boards")==0&&
                 paid.stock("WORKSHOP","R","tools")==0&&
                 paid.stock("WORKSHOP","R","fittings")==1&&
                 denied.cash("WORKSHOP")==19&&denied.cash("BOARD-VENDOR")==0&&
                 denied.firm_finance("BOARD-VENDOR").receipts==0&&
                 denied.stock("BOARD-VENDOR","R","boards")==1&&
                 denied.stock("WORKSHOP","R","boards")==0&&
                 denied.stock("WORKSHOP","R","tools")==1&&
                 denied.stock("WORKSHOP","R","fittings")==0,
                 "the replacement board and output require a real affordable restock");
    // Same solvent workshop, local prices and paid labor, but only one
    // physical input already delivered. Cash cannot replace the other one.
    auto tool_only=scenario(21,true,true), board_only=scenario(61,false,true);
    tool_only.advance_day();board_only.advance_day();
    const auto& tool=c(tool_only);
    const auto& board=c(board_only);
    require_test(tool.cash("WORKSHOP")==20&&tool.cash("BOARD-VENDOR")==0&&
                 tool.stock("WORKSHOP","R","tools")==1&&
                 tool.stock("WORKSHOP","R","boards")==0&&
                 tool.stock("WORKSHOP","R","fittings")==0&&
                 board.cash("WORKSHOP")==60&&board.cash("A")==0&&
                 board.stock("WORKSHOP","R","tools")==0&&
                 board.stock("WORKSHOP","R","boards")==1&&
                 board.stock("WORKSHOP","R","fittings")==0,
                 "neither a real tool nor a real board alone can make a fitting");
}
static void public_employment_pays_net_without_self_tax_revenue() {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"R","A");geo.seed_unit(1,"A","R");
    geo.seed_assignment(1,1);
    auto& core=geo.seed_core().seed_core();
    core.seed_actor("GOVERNMENT-WALLET",500);
    core.seed_actor("FIRM",200);
    core.seed_government("A","HEAD","PM",5,3);
    core.seed_country_treasury("A","GOVERNMENT-WALLET");
    const auto public_worker=people(gh,"R",1,"worker");
    auto& pop=core.seed_population();
    const auto public_household="HH:"+std::to_string(pop.slice(public_worker).key.household);
    // Use separate native wallets despite the same region and profession.
    const auto house=pop.household_profile({"R","private","shared",1});
    const auto& private_slice=pop.slice(public_worker);
    const auto private_member=pop.seed_population(private_slice.key.demographic,
                              private_slice.key.economic,house,1,42);
    const auto private_household="HH:"+std::to_string(house);
    pop.hire(public_worker,1,"GOVERNMENT-WALLET","worker",3100,{});
    pop.hire(private_member,1,"FIRM","worker",3100,{});
    core.seed_actor(public_household,0);core.seed_actor(private_household,0);
    p0::Policies p;p.daily_food_units_per_person=0;
    p0::DailyGame game(daily::World::adopt(std::move(gh),7),p);
    const auto start=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame replay(daily::SnapshotCodec::decode(start),p);
    game.advance_day();replay.advance_day();
    const auto& paid=c(game);
    const auto fiscal=paid.country_fiscal("A");
    require_test(paid.cash(public_household)==90&&paid.cash(private_household)==90&&
                 paid.cash("FIRM")==100&&fiscal.cash==420&&
                 fiscal.realized_revenue==10&&fiscal.realized_expenditure==90&&
                 paid.outstanding_worker_wages("GOVERNMENT-WALLET")==0&&
                 daily::SnapshotCodec::encode(game.world())==
                     daily::SnapshotCodec::encode(replay.world()),
                 "public employer must pay a real net wage without taxing itself");
    for(int i=0;i<5;++i)game.advance_day();
    const auto short_pay=c(game).country_fiscal("A");
    require_test(short_pay.cash==7&&short_pay.realized_revenue==20&&
                 short_pay.realized_expenditure==543&&
                 c(game).cash(public_household)==513&&
                 c(game).outstanding_worker_wages("GOVERNMENT-WALLET")==30,
                 "unfunded gross sovereign wages must remain owed after real partial payment");
    const auto arrears=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame arrears_replay(daily::SnapshotCodec::decode(arrears),p);
    game.advance_day();arrears_replay.advance_day();
    require_test(daily::SnapshotCodec::encode(game.world())==
                     daily::SnapshotCodec::encode(arrears_replay.world()),
                 "public same-wallet arrears must survive exact resume");
}
static void automatic_wage_withholding_rereads_office_funding() {
    using namespace phasec;
    auto gh=fixture();
    auto& d=gh.seed_geography().seed_core();
    auto& core=d.seed_core();
    core.seed_actor("OFFICE",0);
    core.p1_set_incorporation("R",true);
    core.seed_country_treasury("A","TREASURY");
    core.seed_p1_constitution("A",{P1GovernancePrinciple::monarchy,
        P1PowerDistribution::autocracy,P1LegislatureKind::none,"test:office"});
    const auto administrators=people(gh,"R",2'500'000,"bureaucrat");
    auto& pop=core.seed_population();
    pop.hire(administrators,2,"OFFICE","bureaucrat",1,{});
    pop.hire(pop.slices().begin()->first,4,"FIRM","worker",1,{});
    core.seed_public_institution("ADMIN","OFFICE","R","paper",5,1);
    core.seed_p1_institution("A","ADMIN",P1InstitutionKind::administration,1);
    core.seed_production_facility("PUBLIC_OFFICE","OFFICE","R",2);
    core.p1_register_office({"ADMIN","PUBLIC_OFFICE",P1OfficeMethod::simple,2,1});
    core.seed_production_facility("FARM2","FIRM","R",31);
    core.seed_market("R","textiles",200,1,300,0);
    phased::IndustryRecipe recipe;
    recipe.id="SECOND_RECIPE";recipe.site="FARM2";recipe.inputs={{"ore",1}};
    recipe.outputs={{"textiles",1}};recipe.workers_per_batch=1;recipe.capacity_per_batch=1;
    d.seed_recipe(recipe);
    const auto opening=core.p1_bureaucracy("A");
    require_test(opening.capacity.complete&&opening.capacity.committed==111&&
                 opening.office_output==0&&opening.tax_waste_ppm==110'000,
                 "fixture needs a staffed but unfunded office and real uncovered tax waste");
    p0::Policies policy;
    policy.auto_national_budgets_from_actual_services=false;
    policy.manufacturers.push_back({"FOOD_RECIPE",2,"WORKERS","TREASURY",100});
    policy.manufacturers.push_back({"SECOND_RECIPE",1,"WORKERS","TREASURY",100});
    p0::DailyGame game(daily::World::adopt(std::move(gh),82),policy);
    game.advance_day();
    const auto& state=c(game);
    require_test(state.stock("FIRM","R","food")==2&&state.stock("FIRM","R","textiles")==1&&
                 state.cash("WORKERS")==182&&state.cash("TREASURY")==18&&
                 state.p1_bureaucracy("A").office_output==20,
                 "tax source reused stale office funding across two real automatic wages");
}

static void country_tariff_ai_respects_each_sovereigns_current_law() {
    const auto run=[](bool dated_free_trade) {
        using namespace phasec;
        auto gh=fixture();
        auto& geo=gh.seed_geography();
        geo.seed_province(88,"FOREIGN","B");
        geo.seed_unit(88,"B","ROOT-B");geo.seed_assignment(88,88);
        auto& core=geo.seed_core().seed_core();
        core.seed_actor("B",200);core.seed_actor("B-SELLER",0);
        core.seed_government("B","HEAD_B","PM_B",5,3);
        core.seed_country_treasury("A","TREASURY");
        for(const auto* tag:{"A","B"})
            core.seed_p1_constitution(tag,{P1GovernancePrinciple::monarchy,
                P1PowerDistribution::autocracy,P1LegislatureKind::none,"test:constitution"});
        core.p2_seed_base({"A-TRADE","A","trade_policy","national",
                           "law_protectionism","test:enacted trade law"});
        core.p2_seed_base({"B-TRADE","B","trade_policy","national",
                           dated_free_trade?"law_protectionism":"law_free_trade",
                           "test:enacted trade law"});
        core.seed_market("FOREIGN","food",100,1,200,0);
        core.seed_stock("FIRM","R","food",2);
        core.seed_stock("B-SELLER","FOREIGN","food",2);
        const auto worker=core.seed_population().slices().begin()->first;
        core.seed_population().hire(worker,4,"FIRM","worker",10,{});
        p0::Policies policy;
        policy.manufacturers.push_back({"FOOD_RECIPE",15,"WORKERS","TREASURY",1});
        policy.countries.push_back({"A","PM_A","R","food","FIRM",120,2,100'000});
        policy.countries.push_back({"B","PM_B","FOREIGN","food","B-SELLER",120,2,100'000});
        p0::DailyGame game(daily::World::adopt(std::move(gh),82),policy);
        const auto today=game.world().day_index();
        const auto tariff=[&](const char* tag) {
            return game.world().engine().diplomacy().tariffs.at({tag,"food"}).ppm;
        };
        const auto advance=[&](const phasegh::World::Plan& commands) {
            const auto opening=c(game).stock("FIRM","R","ore");
            game.advance_day(commands);
            require_test(c(game).stock("FIRM","R","ore")==opening-2,
                         "lawful country failed to commit real daily industry");
            require_test(tariff("A")==100'000&&
                         c(game).stock("B-SELLER","FOREIGN","food")==2,
                         "independent lawful protection or actual foreign stock changed");
        };
        phasegh::World::Plan enact;
        if(dated_free_trade)enact[9].push_back([today](phasegh::Stage& stage) {
            auto& state=stage.phase_c();
            P2Bill bill;
            bill.id="B-FREE-TRADE";bill.country="B";bill.proposer="PM_B";
            bill.legal_basis="test:bill";bill.effective_day=today+1;
            bill.action=P2BillAction::enact_or_replace_base;
            bill.proposed_base={"B-TRADE-FREE","B","trade_policy","national",
                                "law_free_trade","test:effective trade law"};
            state.p2_submit_bill(std::move(bill),today);
            state.p2_approve_bill("B-FREE-TRADE",{});
            state.p2_promulgate_bill("B-FREE-TRADE","HEAD_B",today);
        });
        advance(enact);
        require_test(tariff("B")==(dated_free_trade?100'000:0),
                     "automatic tariff ignored the second country's opening law");
        if(dated_free_trade) {
            advance({}); // Stage0 applies the new P2 ceiling, before tariff AI.
            require_test(c(game).p2_selected_law("B","trade_policy")=="free_trade"&&
                         tariff("B")==0,
                         "automatic tariff reimposed a duty after lawful free trade took effect");
        }
        const auto before=daily::SnapshotCodec::encode(game.world());
        phasegh::World::Plan forbidden;
        forbidden[0].push_back([](phasegh::Stage& stage) {
            stage.adopt_tariff_daily("MANUAL-OVER-CAP","B","food",100'000,"PM_B");
        });
        bool legally_rejected=false;
        try { game.advance_day(forbidden); }
        catch(const std::exception& e) {
            legally_rejected=std::string_view(e.what()).find(
                "P2 current tariff law forbids excessive import duties")!=std::string_view::npos;
        }
        require_test(legally_rejected&&daily::SnapshotCodec::encode(game.world())==before,
                     "manual H setter no longer strictly rejects illegal duties atomically");
    };
    run(false);
    run(true);
}

static void tariff_ai_skips_stale_or_delegated_authority() {
    using namespace phasec;
    enum class Mode { cabinet, delegated_mandate, delegated_food };
    const auto run=[](Mode mode) {
        const bool cabinet=mode==Mode::cabinet;
        const bool food=mode==Mode::delegated_food;
        auto gh=fixture();
        auto& geo=gh.seed_geography();
        geo.seed_province(88,"FOREIGN","B");
        geo.seed_unit(88,"B","ROOT-B");geo.seed_assignment(88,88);
        auto& core=geo.seed_core().seed_core();
        core.seed_actor("B",200);core.seed_actor("B-SELLER",0);
        core.seed_country_treasury("A","TREASURY");
        core.seed_government("B","HEAD_B","PM_B",5,3);
        for(const auto* tag:{"A","B"}) {
            core.seed_p1_constitution(tag,{P1GovernancePrinciple::monarchy,
                P1PowerDistribution::autocracy,P1LegislatureKind::none,"test:constitution"});
            core.p2_seed_base({std::string(tag)+"-TRADE",tag,"trade_policy","national",
                               "law_protectionism","test:enacted trade law"});
        }
        core.seed_market("FOREIGN","food",100,1,200,0);
        core.seed_stock("FIRM","R","food",2);core.seed_stock("FIRM","R","ore",100);
        core.seed_stock("B-SELLER","FOREIGN","food",2);
        const auto worker=core.seed_population().slices().begin()->first;
        core.seed_population().hire(worker,4,"FIRM","worker",10,{});
        phaseb::Id hungry{};
        std::string hungry_wallet;
        if(food) {
            hungry=people(gh,"FOREIGN",2,"worker");
            hungry_wallet="HH:"+std::to_string(core.people().slice(hungry).key.household);
            core.seed_actor(hungry_wallet,0);
        }
        p0::Policies policy;policy.daily_food_units_per_person=0;
        policy.manufacturers.push_back({"FOOD_RECIPE",1000,"WORKERS","TREASURY",1});
        policy.countries.push_back({"A","PM_A","R","food","FIRM",300,2,100'000});
        if(!food)policy.countries.push_back({"B","PM_B","FOREIGN","food","B-SELLER",300,2,100'000});
        p0::DailyGame game(daily::World::adopt(std::move(gh),83),policy);
        const auto tariff=[](const p0::DailyGame& sim,const char* tag) {
            return sim.world().engine().diplomacy().tariffs.at({tag,"food"}).ppm;
        };
        const auto tomorrow=game.world().day_index()+1;
        phasegh::World::Plan first;
        if(food)first[0].push_back([](phasegh::Stage& stage) {
            stage.adopt_tariff_daily("AUTHORED-B-OPENING","B","food",100'000,"PM_B");
        });
        if(cabinet)first[7].push_back([](phasegh::Stage& stage) {
            stage.phase_c().consume("B-SELLER","FOREIGN","food",2);
        });
        first[9].push_back([=](phasegh::Stage& stage) {
            if(cabinet)stage.phase_c().schedule_p1_cabinet_daily(
                "B","HEAD_B","PM_NEXT",{},tomorrow,"test:actual cabinet formation");
            else stage.establish_protectorate("LAWFUL-DELEGATION","A","B","PM_A","PM_B");
        });
        game.advance_day(first);
        require_test(c(game).government("B").prime_minister=="PM_B"&&
                     tariff(game,"A")==100'000&&tariff(game,"B")==100'000,
                     "opening authorized tariffs and incumbent must both exist");
        if(!cabinet) {
            while(game.world().date()<daily::Date{2000,1,31})game.advance_day();
            if(food) {
                // Enable actual household demand after preserving the
                // incumbent tariff up to the delegation's effective date.
                policy.buyers.push_back({hungry,hungry_wallet,"B-SELLER","FOREIGN","food",1});
                game=p0::DailyGame(daily::SnapshotCodec::decode(
                    daily::SnapshotCodec::encode(game.world())),policy);
            }
            phasegh::World::Plan shortage;
            shortage[7].push_back([](phasegh::Stage& stage) {
                stage.phase_c().consume("B-SELLER","FOREIGN","food",2);
            });
            game.advance_day(shortage);
            require_test(!game.world().engine().diplomacy().protectorates.at("LAWFUL-DELEGATION").active&&
                         (!food||c(game).food_grievance("FOREIGN")>0),
                         "fixture must have an imminent, not already active, delegation and an observed shortage");
        }
        const auto pending=daily::SnapshotCodec::encode(game.world());
        p0::DailyGame replay(daily::SnapshotCodec::decode(pending),policy);
        const auto ore=c(game).stock("FIRM","R","ore");
        const auto before_commands=game.world().engine().diplomacy().commands.size();
        const auto next=daily::following_day(game.world().date());
        game.advance_day();replay.advance_day();
        require_test(game.world().date()==next&&c(game).stock("FIRM","R","ore")==ore-2&&
                     tariff(game,"A")==100'000&&tariff(game,"B")==100'000&&
                     game.world().engine().diplomacy().commands.size()==before_commands&&
                     daily::SnapshotCodec::encode(game.world())==daily::SnapshotCodec::encode(replay.world()),
                     "unauthorized automatic tariff cannot abort independent production, change duty, or impersonate");
        const auto live_prime=cabinet?"PM_NEXT":"PM_B";
        require_test(c(game).government("B").prime_minister==live_prime&&
                     (cabinet||game.world().engine().diplomacy().protectorates.at("LAWFUL-DELEGATION").active),
                     "authorized cabinet or delegated foreign policy did not take effect");
        const auto before=daily::SnapshotCodec::encode(game.world());
        phasegh::World::Plan manual;
        manual[0].push_back([](phasegh::Stage& stage) {
            stage.adopt_tariff_daily("MANUAL-CHECK","B","food",0,"PM_B");
        });
        bool strict=false;
        try{game.advance_day(manual);}
        catch(const std::exception& e) {
            strict=std::string_view(e.what()).find(cabinet?"H_ERR_UNAUTHORIZED_GOVERNMENT":
                "H_ERR_DELEGATED_FOREIGN_POLICY")!=std::string_view::npos;
        }
        require_test(strict&&daily::SnapshotCodec::encode(game.world())==before,
                     "manual tariff authority must still reject and roll back the entire day");
        if(cabinet) {
            policy.countries.back().prime_minister="PM_NEXT";
            p0::DailyGame renewed(daily::SnapshotCodec::decode(before),policy);
            renewed.advance_day();
            require_test(tariff(renewed,"B")==0&&
                         renewed.world().engine().diplomacy().commands.size()==before_commands+1,
                         "new prime's separately authored mandate must still update the rate");
        }
    };
    std::string failures;
    for(const auto mode:{Mode::cabinet,Mode::delegated_mandate,Mode::delegated_food}) {
        try{run(mode);}
        catch(const std::exception& e) {
            failures+=std::to_string(static_cast<int>(mode))+": "+e.what()+"; ";
        }
    }
    if(!failures.empty())throw std::runtime_error("tariff authority blocked daily commit: "+failures);
}

static void daily_production_trade_ai_investment_replay() {
    auto w=fixture();
    const auto& pop=w.base().core().core().people().slices();
    auto worker=pop.begin()->first;
    auto builder=std::next(pop.begin())->first;
    auto buyer=std::next(pop.begin(),2)->first;
    auto& core=w.seed_geography().seed_core().seed_core();
    // Employment is a real Phase B contract; the P0 engine does not grant free labor.
    auto& seeded=core.seed_population();
    seeded.hire(worker,4,"FIRM","worker",10,phaseb::JobRequirements{});
    seeded.hire(builder,2,"BUILDER","worker",10,phaseb::JobRequirements{});
    p0::Policies options;
    options.manufacturers.push_back({"FOOD_RECIPE",15,"WORKERS","TREASURY",1});
    options.buyers.push_back({buyer,"HH","FIRM","R","food",1});
    options.investments.push_back({"NEW_FARM","food",100,10,1});
    options.countries.push_back({"A","PM_A","R","food","FIRM",120,0,100000});
    p0::DailyGame game(daily::World::adopt(std::move(w),7),options);
    const auto before=c(game).cash("HH");
    game.advance_day();
    require_test(game.world().date()==daily::Date{2000,1,2},"day did not advance");
    require_test(!game.world().engine().base().core().state().actual_production_runs.empty(),
                 "real daily industry did not run");
    require_test(c(game).stock("FIRM","R","food")==0 &&
                 c(game).stock("HH","R","food")==0,
                 "food must not be duplicated after purchase and real consumption");
    require_test(c(game).cash("HH")<before,"buyer did not pay real money");
    require_test(c(game).cash("WORKERS")>0,"worker wages were not settled");
    require_test(game.world().engine().base().core().construction("NEW_FARM").accepted_work==1,
        "daily private construction missing");
    require_test(game.world().engine().diplomacy().tariffs.at({"A","food"}).ppm==100000,
        "national AI failed to adopt lawful tariff");
    // Existing snapshots are saved by the native day codec; policy config is
    // campaign input and must be reused on load.
    auto saved=daily::SnapshotCodec::encode(game.world());
    auto restored=daily::SnapshotCodec::decode(saved);
    p0::DailyGame replay(std::move(restored),options);
    for(int i=0;i<4;++i){game.advance_day();replay.advance_day();}
    require_test(daily::SnapshotCodec::encode(game.world())==
                 daily::SnapshotCodec::encode(replay.world()),
                 "same snapshot and policies must replay exactly");
    require_test(game.world().engine().base().core().construction("NEW_FARM").commissioned,
                 "daily private investment must commission completed facilities");
}
struct InvestmentOrderLegalSources {
    std::string country="A",land_reform,amendment;
    bool parents_amendment_in_land_reform{};
};
enum class CeilingLaw { national, absent, local, foreign, misleading_national };
static void check_paid_investment_order(const char* law,const char* profession,bool share,
                      bool foreign,bool resolved,phasec::Money cash,
                      phaseb::People expected,phaseb::People maximum=4,
                      phaseb::People staff=12,phaseb::People material=20,
                      bool public_funds=false,phasec::Money arrears=0,
                      int false_amendment=0,InvestmentOrderLegalSources sources={},
                      CeilingLaw law_scope=CeilingLaw::national,
                      phasec::Money expected_current_wage=0) {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"R",sources.country);geo.seed_unit(11,sources.country,"ROOT");
    geo.seed_assignment(1,11);
    if(foreign||law_scope==CeilingLaw::foreign) {
        geo.seed_province(2,"F","B");geo.seed_unit(12,"B","FOREIGN");
        geo.seed_assignment(2,12);
    }
    auto& industry=geo.seed_core();
    auto& core=industry.seed_core();
    core.seed_actor(sources.country,0);core.seed_actor("FIRM",cash);
    core.seed_actor("BUILDER",0);core.seed_actor("CORP",10);
    const auto enact=[&](const std::string& country,const char* choice,
                         bool local=false,bool custom=false) {
        core.seed_government(country,"HEAD","PM",5,3);
        core.seed_p1_constitution(country,{phasec::P1GovernancePrinciple::monarchy,
            phasec::P1PowerDistribution::autocracy,
            phasec::P1LegislatureKind::none,"fixture:constitution"});
        phasec::P2BaseLaw act;
        act.id=country+":economy";act.country=country;
        act.domain="economic_system";act.scope="national";
        if(!choice)return;
        if(local||custom)act.domain="custom_economy";
        if(local) {
            act.scope="R";
            act.tier=phasec::P2NormTier::local_ordinance;
        }
        act.base_choice=choice;act.legal_basis="fixture:economy";
        core.p2_seed_base(act);
    };
    enact(sources.country,
          law_scope==CeilingLaw::absent||law_scope==CeilingLaw::foreign?nullptr:law,
          law_scope==CeilingLaw::local,law_scope==CeilingLaw::misleading_national);
    if(law_scope==CeilingLaw::foreign) {
        core.seed_actor("B",0);
        enact("B",law);
    }
    if(!sources.land_reform.empty()) {
        phasec::P2BaseLaw land;
        land.id=sources.country+":land";land.country=sources.country;
        land.domain="land_reform";land.scope="national";
        land.base_choice=sources.land_reform;land.legal_basis="fixture:land";
        core.p2_seed_base(land);
    }
    if(!sources.amendment.empty()) {
        const auto parent=sources.country+
            (sources.parents_amendment_in_land_reform?":land":":economy");
        core.p2_seed_amendment({sources.amendment,parent,"fixture:enacted","",
            {{"actual-law-clause",sources.country,
              phasec::P2ClauseKind::political_association_allowed,1}}});
    }
    if(public_funds)core.seed_country_treasury(sources.country,"FIRM");
    if(false_amendment==1) {
        phasec::P2BaseLaw local;
        local.id="LOCAL";local.country=sources.country;local.domain="synthetic";
        local.scope="R";local.tier=phasec::P2NormTier::local_ordinance;
        local.base_choice="not_a_catalogue_law";local.legal_basis="fixture:local";
        core.p2_seed_base(local);
        core.p2_seed_amendment({"amendment_farmers_cooperatives","LOCAL",
          "fixture:lookalike","",{{"legal-clause",sources.country,
           phasec::P2ClauseKind::political_association_allowed,1}}});
    } else if(false_amendment==2)
        core.p2_seed_amendment({"amendment_economic_regeneration_1",sources.country+":economy",
          "fixture:invalid-country-tag","",{{"legal-clause",sources.country,
           phasec::P2ClauseKind::political_association_allowed,1}}});
    if(foreign) {
        core.seed_actor("B",0);
        enact("B",law);
    }
    const auto builders=people(gh,"R",staff,"worker");
    core.seed_population().hire(builders,staff,"BUILDER","worker",1,
                                phaseb::JobRequirements{});
    const auto investor=people(gh,foreign?"F":"R",7,profession);
    const auto household="HH:"+std::to_string(
        resolved?core.people().slice(investor).key.household:999999);
    core.seed_actor(household,100);
    core.seed_shares("company:FIRM",share?household:"CORP",100);
    core.seed_market("R","food",100,1,200,0);
    core.seed_stock("BUILDER","R","steel",material);
    phased::ConstructionProject project;
    project.id="SITE";project.client="FIRM";project.contractor="BUILDER";
    project.region="R";project.material_good="steel";project.target_site="NEW";
    project.work_required=20;project.material_per_work=1;
    project.workers_per_work=1;project.cash_price_per_work=10;
    project.target_capacity=12;
    industry.seed_construction(project);
    p0::Policies opts;
    opts.allow_endogenous_investment_credit=false;
    opts.allow_collateralized_enterprise_credit=false;
    opts.investments.push_back({"SITE","food",10,100,maximum});
    std::string current_wallet;
    p0::DailyGame game([&] {
        if(arrears>0) {
            // This posted historical liability survives independently of any
            // still-current job. Only the paired case adds a new day's salary.
            core.seed_actor("EMPLOYEE",0);
            if(expected_current_wage>0) {
                const auto employee=people(gh,"R",1,"worker");
                current_wallet="HH:"+std::to_string(core.people().slice(employee).key.household);
                core.seed_population().hire(employee,1,"FIRM","worker",arrears,
                                            phaseb::JobRequirements{});
            }
        }
        auto clock=daily::World::adopt(std::move(gh),7);
        if(arrears>0) {
            const auto earned=clock.day_index();
            phasegh::World::Plan preexisting;
            preexisting[5].push_back([=](phasegh::Stage& stage) {
                stage.phase_c().phase_time_accrue_contract_wage(
                    "already-earned","FIRM","EMPLOYEE",sources.country,sources.country,arrears,earned);
            });
            clock.advance_day(preexisting);
        }
        return clock;
    }(),opts);
    game.advance_day();
    if(maximum==1) {
        const bool traditional=std::string_view(law)=="law_traditionalism";
        const auto expected_first=traditional?1:2;
        const auto& first=game.world().engine().base().core().construction("SITE");
        require_test(first.paid_work==expected_first&&
                     c(game).cash("FIRM")==cash-10*expected_first,
                     "first fractional calendar day granted or lost a whole order");
        const auto saved=daily::SnapshotCodec::encode(game.world());
        p0::DailyGame replay(daily::SnapshotCodec::decode(saved),opts);
        game.advance_day();replay.advance_day();
        require_test(daily::SnapshotCodec::encode(game.world())==
                     daily::SnapshotCodec::encode(replay.world()),
                     "fractional construction quota did not replay from the saved day");
    }
    const auto& work=game.world().engine().base().core().construction("SITE");
    if(work.accepted_work!=expected||work.paid_work!=expected||
       work.capitalized_work!=expected||work.capitalized_value!=10*expected)
        throw std::runtime_error(
            std::string("share-backed paid construction order: ")+law+
            " "+profession+" "+sources.country+
            " "+sources.land_reform+" "+sources.amendment+" expected="+std::to_string(expected)+
            " scope="+std::to_string(static_cast<int>(law_scope))+
            " paid="+std::to_string(work.paid_work)+
            " accepted="+std::to_string(work.accepted_work)+
            " capitalized="+std::to_string(work.capitalized_work)+
            " capitalized value="+std::to_string(work.capitalized_value));

    if(arrears>0)require_test(c(game).cash("EMPLOYEE")>0&&
                              c(game).cash("EMPLOYEE")+c(game).cash(sources.country)==arrears&&
                              c(game).outstanding_worker_wages("FIRM")==0,
                              "existing earned household wages were not actually settled");
    if(expected_current_wage>0)
        require_test(c(game).has_actor(current_wallet)&&
                     c(game).cash(current_wallet)==expected_current_wage,
                     "new daily wages did not enter the actual household deposit");
    require_test(c(game).cash("FIRM")==cash-10*expected-arrears-expected_current_wage&&
                 c(game).cash("BUILDER")==10*expected&&
                 c(game).cash(household)==100&&c(game).cash("CORP")==10&&
                 c(game).shares("company:FIRM",share?household:"CORP")==100&&
                 c(game).stock("BUILDER","R","steel")==material-expected,
                 "investment efficiency manufactured funds, equity or inputs");
}
// D-D0/D-C4 use real project orders, not an allocation of a fixed national queue.
static void economic_laws_adjust_only_paid_private_order_ceiling() {
    const auto run=[](const char* law,phaseb::People expected,bool public_funds=false,
                      CeilingLaw scope=CeilingLaw::national,phasec::Money cash=300,
                      phaseb::People staff=20,phaseb::People steel=20) {
        check_paid_investment_order(law,"worker",false,false,true,cash,expected,
                                   10,staff,steel,public_funds,0,0,{},scope);
    };
    run("law_laissez_faire",10,false,CeilingLaw::absent);
    run("law_laissez_faire",10,false,CeilingLaw::local);
    run("law_laissez_faire",10,false,CeilingLaw::foreign);
    run("law_laissez_faire",10,false,CeilingLaw::misleading_national);
    for(const auto& [law,paid]:std::initializer_list<std::pair<const char*,int>>{
          {"law_traditionalism",12},{"law_interventionism",15},
          {"law_agrarianism",15},{"law_industry_banned",15},
          {"law_extraction_economy",15},{"law_laissez_faire",17},
          {"law_cooperative_ownership",13},{"law_command_economy",11}}) {
        run(law,paid);
        run(law,10,true);
    }
    // The policy reserves 10 cash before the project can pay 10 per unit.
    run("law_laissez_faire",0,false,CeilingLaw::national,19);
    run("law_laissez_faire",1,false,CeilingLaw::national,29);
    run("law_laissez_faire",3,false,CeilingLaw::national,300,3);
    run("law_laissez_faire",2,false,CeilingLaw::national,300,20,2);
}

static void household_shareholder_efficiency_changes_paid_daily_construction_orders() {
    // Existing private construction allocation first scales 4 to 5 (under
    // Traditionalism) or 6 (under Agrarianism). Isolate the shareholder rows
    // against those same laws without eligible ownership.
    check_paid_investment_order("law_traditionalism","farmer",true,false,true,200,3);
    check_paid_investment_order("law_traditionalism","farmer",false,false,true,200,5);
    check_paid_investment_order("law_agrarianism","farmer",true,false,true,200,9);
    check_paid_investment_order("law_agrarianism","farmer",false,false,true,200,6);
    check_paid_investment_order("law_agrarianism","worker",true,false,true,200,6);
    check_paid_investment_order("law_agrarianism","farmer",true,true,true,200,6);
    check_paid_investment_order("law_agrarianism","farmer",true,false,false,200,6);
    check_paid_investment_order("law_agrarianism","farmer",true,false,true,30,2);
    check_paid_investment_order("law_agrarianism","farmer",true,false,true,9,0);
    check_paid_investment_order("law_agrarianism","farmer",true,false,true,200,3,4,3,20);
    check_paid_investment_order("law_agrarianism","farmer",true,false,true,200,2,4,12,2);
    check_paid_investment_order("law_agrarianism","farmer",true,false,true,200,4,4,12,20,true);
    check_paid_investment_order("law_agrarianism","farmer",true,false,true,120,6,4,12,20,false,60);
    check_paid_investment_order("law_agrarianism","farmer",true,false,true,122,6,4,12,20,
                                false,60,0,{},CeilingLaw::national,2);
    check_paid_investment_order("law_traditionalism","farmer",true,false,true,200,1,1);
    check_paid_investment_order("law_agrarianism","farmer",true,false,true,200,3,1);
    check_paid_investment_order("law_agrarianism","farmer",true,false,true,300,11,5,12,20,false,0,1);
    check_paid_investment_order("law_agrarianism","aristocrat",true,false,true,200,9,4,12,20,false,0,2);
}

// Each case compares two actual paid orders from the same authorized
// nation, law and construction model, removing only eligible share title for
// a base source or only the one valid amendment/land law for a stack.
static void canonical_profession_sources_change_real_construction_orders() {
    const auto run=[](const char* economy,const char* profession,bool shares,
                      phaseb::People paid,InvestmentOrderLegalSources legal={}) {
        check_paid_investment_order(economy,profession,shares,false,true,
            500,paid,8,24,30,false,0,0,std::move(legal));
    };
    const auto base=[&](const char* law,const char* profession,
                        phaseb::People ordinary,phaseb::People affected) {
        run(law,profession,false,ordinary);
        run(law,profession,true,affected);
    };
    for(const char* job:{"shopkeeper","bureaucrat","capitalist","clergyman","aristocrat"})
        base("law_traditionalism",job,10,5);
    for(const char* job:{"aristocrats","clergymen"})
        base("law_agrarianism",job,12,18);
    base("law_agrarianism","capitalists",12,9);
    for(const char* job:{"shopkeepers","bureaucrats"})
        base("law_extraction_economy",job,12,6);
    base("law_extraction_economy","capitalists",12,9);
    for(const char* job:{"farmers","clergymen","aristocrats"})
        base("law_extraction_economy",job,12,3);
    for(const char* job:{"shopkeepers","capitalists"})
        base("law_laissez_faire",job,14,18);
    for(const char* job:{"shopkeepers","farmers"})
        base("law_cooperative_ownership",job,10,13);
    // An already authorized and privately paid explicit project, not an
    // autonomous expansion proposal or an enabled global investment pool.
    base("law_command_economy","bureaucrats",8,10);
    base("law_agrarianism","farmer_unknown",12,12);

    const auto amendment=[&](const char* source,const char* country,
                             const char* profession,phaseb::People paid,
                             const char* land="") {
        run("law_interventionism",profession,true,12,{country,land,"",false});
        run("law_interventionism",profession,true,paid,
            {country,land,source,*land!='\0'});
    };
    for(const char* job:{"capitalist","aristocrat","shopkeeper"}) {
        amendment("amendment_economic_regeneration_1","SPA",job,15);
        amendment("amendment_economic_regeneration_2","POR",job,14);
    }
    amendment("amendment_farmers_cooperatives","USA","farmer",11,"law_homesteading");
    amendment("amendment_zaibatsu_oligopoly_1","JAP","capitalist",15);
    amendment("amendment_zaibatsu_oligopoly_2","JAP","capitalist",15);
    run("law_interventionism","farmer",true,12,
        {"USA","law_peasant_proprietorship","amendment_farmers_cooperatives",true});
    run("law_interventionism","aristocrat",true,12,{"MEX","","",false});
    run("law_interventionism","aristocrat",true,10,{"MEX","law_latifundias","",false});
    run("law_interventionism","aristocrat",true,15,
        {"MEX","law_expanded_latifundias","",false});
    // Independent land/economic sources stack additively (+0.5 −0.2)
    // before the real calendar-rounded order and cash settlement.
    run("law_agrarianism","aristocrat",true,16,{"MEX","law_latifundias","",false});
}

static void aristocratic_shareholders_inject_only_lawful_monthly_capital() {
    enum class Law { national, other, fake_local, fake_national, absent };
    const auto run=[](Law law,bool shares,bool aristocrat,bool funded,
                      bool holder_abroad,bool mixed,phasec::Money expected) {
        using namespace phasec;
        phasegh::World gh(24000);
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"R","A");geo.seed_province(2,"X","B");
        geo.seed_unit(11,"A","A-admin");geo.seed_unit(22,"B","B-admin");
        geo.seed_assignment(1,11);geo.seed_assignment(2,22);
        auto& d=geo.seed_core();auto& core=d.seed_core();
        for(const auto* nat:{"A","B"}) {
            core.seed_actor(nat,0);
            core.seed_government(nat,"HEAD","PM",5,3);
            core.seed_p1_constitution(nat,{P1GovernancePrinciple::monarchy,
                P1PowerDistribution::autocracy,P1LegislatureKind::none,"fixture:constitution"});
        }
        core.seed_actor("FIRM",100);core.seed_actor("BUILDER",0);
        core.seed_production_facility("SITE","FIRM","R",10);
        phased::ConstructionProject project;
        project.id="PROJECT";project.client="FIRM";project.contractor="BUILDER";
        project.region="R";project.material_good="steel";project.target_site="SITE";
        project.work_required=project.material_per_work=project.workers_per_work=1;
        project.target_capacity=10;project.cash_price_per_work=10;
        project.accepted_work=project.paid_work=project.capitalized_work=1;
        project.capitalized_value=10;project.commissioned=true;project.ready_month=23999;
        d.seed_construction(project);
        const std::string region=holder_abroad?"X":"R";
        auto& pop=core.seed_population();
        const auto h=pop.household_profile({region,"family","shared",10});
        const auto demo=pop.demographic_profile({region,23700,phaseb::Sex::male,
            "culture","faith","citizen","free",pop.default_health(),phaseb::Service::civilian});
        const auto add=[&](const char* profession) {
            const auto econ=pop.economic_profile({region,profession,"self_employed","self_employed",
                                                pop.default_education()});
            pop.seed_population(demo,econ,h,10);
        };
        add(aristocrat?"aristocrat":"worker");
        if(mixed)add("worker");
        const std::string wallet="HH:"+std::to_string(h);
        const Money opening=funded?100:0;
        core.seed_actor(wallet,opening);
        core.seed_shares("company:FIRM",shares?wallet:"A",100);
        if(law!=Law::absent) {
            const bool fake=law==Law::fake_local||law==Law::fake_national;
            P2BaseLaw enacted{"ACT","A",fake?"custom_economy":"economic_system",
                law==Law::fake_local?"R":"national",
                law==Law::other?"law_agrarianism":"law_industry_banned","fixture:law"};
            if(law==Law::fake_local)enacted.tier=P2NormTier::local_ordinance;
            core.p2_seed_base(enacted);
        }
        p0::DailyGame game(daily::World::adopt(std::move(gh),15),{});
        const auto opening_capital=c(game).financial().balance("FIRM:capital");
        const auto verify=[&] {
            const auto& live=c(game);
            const std::string diagnostic="aristocratic capital requires a real entitled holder and exact sovereign enacted law (case "+
                std::to_string(static_cast<int>(law))+")";
            require_test(live.cash(wallet)==opening-expected &&
                         live.cash("FIRM")==100+expected &&
                         live.cash("A")==0 && live.cash("B")==0 &&
                         live.shares("company:FIRM",shares?wallet:"A")==100,
                         diagnostic.c_str());
            require_test(live.financial().balance("FIRM:capital")==opening_capital-expected &&
                         live.financial().balance("FIRM:revenue")==0,
                         "actual contribution belongs to the company's capital, not operating revenue");
        };
        game.advance_day();
        verify();
        game.advance_day();
        verify(); // Monthly decision, not another ordinary-day transfer.
    };
    run(Law::national,true,true,true,false,false,25);
    run(Law::national,true,true,true,false,true,12);
    run(Law::national,false,true,true,false,false,0);
    run(Law::national,true,false,true,false,false,0);
    run(Law::national,true,true,false,false,false,0);
    run(Law::national,true,true,true,true,false,0);
    run(Law::other,true,true,true,false,false,0);
    run(Law::absent,true,true,true,false,false,0);
    run(Law::fake_local,true,true,true,false,false,0);
    run(Law::fake_national,true,true,true,false,false,0);
}

static void daily_market_shortage_and_political_response() {
    auto w=fixture();
    const auto& slices=w.base().core().core().people().slices();
    auto buyer=std::next(slices.begin(),2)->first;
    p0::Policies opt;
    opt.buyers.push_back({buyer,"HH","FIRM","R","food",1});
    opt.political_reactions.push_back({buyer,"R","FOOD_MOVEMENT",2,1});
    p0::DailyGame g(daily::World::adopt(std::move(w),4),opt);
    g.advance_day();
    require_test(c(g).food_grievance("R")>0,"daily shortage did not enter political evidence");
    g.advance_day();
    require_test(c(g).group_members(buyer,"FOOD_MOVEMENT")>0,
        "POP did not join a grievance movement after observed scarcity");
    require_test(c(g).protest_participants(buyer)>0,
        "real POP did not participate in bounded protest");
}
static void daily_battle_consumes_actual_ammo_and_pop() {
    phasegh::World gh(24000);
    auto& e=gh.seed_geography();
    e.seed_province(1,"R","A");
    e.seed_unit(11,"A","ROOT");e.seed_assignment(1,11);
    auto& core=e.seed_core().seed_core();
    for(auto who:{"A","ATT","DEF"})core.seed_actor(who,100);
    core.seed_unit("ATT");core.seed_unit("DEF");
    core.seed_stock("ATT","R","ammunition",20);
    core.seed_stock("DEF","R","ammunition",20);
    auto at=people(gh,"R",20,"soldier",phaseb::Service::active);
    auto de=people(gh,"R",20,"soldier",phaseb::Service::active);
    core.seed_active("ATT",at);
    core.seed_active("DEF",de);
    const auto population=core.people().population();
    p0::Policies rules;rules.engagements.push_back({"ATT","DEF","R",at,de,1500000});
    p0::DailyGame g(daily::World::adopt(std::move(gh),2),rules);
    g.advance_day();
    require_test(c(g).stock("ATT","R","ammunition")<20 &&
                 c(g).stock("DEF","R","ammunition")<20,
                 "battle did not use real ammunition");
    require_test(c(g).people().population()<population,
                 "daily battle did not inflict real casualties");
    const auto before=c(g).people().population();
    g.advance_day(); // no ammo left: no phantom repeated casualty
    require_test(c(g).people().population()==before,"ammunition-free casualties");
}

static void daily_insolvent_hunger_is_not_ignored() {
    auto w=fixture();
    const auto& slices=w.base().core().core().people().slices();
    const auto buyer=std::next(slices.begin(),2)->first;
    p0::Policies opt;
    opt.buyers.push_back({buyer,"POOR","FIRM","R","food",1});
    opt.political_reactions.push_back({buyer,"R","FOOD_MOVEMENT",1,1});
    p0::DailyGame g(daily::World::adopt(std::move(w),12),opt);
    const auto price=c(g).market("R","food").price;
    g.advance_day();
    require_test(c(g).cash("POOR")==0 && c(g).food_grievance("R")>0,
        "insolvent POP must still register essential unmet food demand");
    require_test(c(g).market("R","food").price==price,
        "an insolvent buyer must not cause false solvent-demand price inflation");
    require_test(c(g).group_members(buyer,"FOOD_MOVEMENT")>0,
        "observable same-day hunger should support political response");
}
static void daily_price_not_replayed_at_month_close() {
    auto w=fixture();
    const auto& slices=w.base().core().core().people().slices();
    const auto buyer=std::next(slices.begin(),2)->first;
    p0::Policies options;
    options.buyers.push_back({buyer,"HH","FIRM","R","food",1});
    p0::DailyGame g(daily::World::adopt(std::move(w),17),options);
    for(int i=0;i<30;++i)g.advance_day(); // Jan 1 through Jan 30
    require_test(g.world().date()==daily::Date{2000,1,31},
        "invalid daily advance before month boundary");
    const auto price30=c(g).market("R","food").price;
    const auto report=g.advance_day(); // Jan 31, then MonthClose
    require_test(report.monthly_close &&
                 g.world().date()==daily::Date{2000,2,1},
                 "Gregorian month boundary or MonthClose failed");
    require_test(c(g).market("R","food").price==price30+1,
                 "MonthClose applied a duplicate P0 price movement");
    g.advance_day();
    require_test(g.world().date()==daily::Date{2000,2,2},
                 "native day did not resume after monthly accounting close");
}


static void low_unit_price_still_changes_across_months() {
    phasegh::World gh(24000);
    auto& e=gh.seed_geography();
    e.seed_province(1,"R","A");
    e.seed_unit(11,"A","ROOT");
    e.seed_assignment(1,11);
    auto& c=e.seed_core().seed_core();
    c.seed_actor("A",100);
    c.seed_actor("SELLER",0);
    c.seed_actor("HH",1000);
    c.seed_market("R","food",1,1,10);
    c.seed_government("A","head","PM",5,3);
    auto& p=c.seed_population();
    const auto h=p.household_profile({"R","mixed","shared",5});
    const auto econ=p.economic_profile({"R","worker","wage","employee",p.default_education()});
    const auto demo=p.demographic_profile({"R",24000-300,phaseb::Sex::female,
                    "culture","faith","citizen","free",
                    p.default_health(),phaseb::Service::civilian});
    const auto slice=p.seed_population(demo,econ,h,5,1);
    p0::Policies policy;
    policy.buyers.push_back({slice,"HH","SELLER","R","food",1});
    p0::DailyGame game(daily::World::adopt(std::move(gh),3),policy);
    for(int i=0;i<102;++i)game.advance_day();
    require_test(game.world().engine().base().core().core().market("R","food").price>=2,
         "low nominal prices must accrue pressure across month boundaries");
}


static void duplicate_pop_demand_is_rejected() {
    auto w=fixture();
    const auto& slices=w.base().core().core().people().slices();
    const auto buyer=std::next(slices.begin(),2)->first;
    p0::Policies config;
    config.buyers.push_back({buyer,"HH","FIRM","R","food",1});
    config.buyers.push_back({buyer,"HH","FIRM","R","food",1});
    bool rejected=false;
    try{p0::DailyGame game(daily::World::adopt(std::move(w),8),config);}
    catch(const std::invalid_argument&){rejected=true;}
    require_test(rejected,"same POP and commodity must not be counted twice");
}
static void additional_active_cohorts_do_not_block_combat() {
    phasegh::World gh(24000);
    auto& e=gh.seed_geography();
    e.seed_province(1,"R","A");e.seed_unit(11,"A","ROOT");e.seed_assignment(1,11);
    auto& c=e.seed_core().seed_core();
    for(auto who:{"A","ATT","DEF"})c.seed_actor(who,100);
    c.seed_unit("ATT");c.seed_unit("DEF");
    c.seed_stock("ATT","R","ammunition",12);
    c.seed_stock("DEF","R","ammunition",12);
    const auto leadA=people(gh,"R",20,"soldier",phaseb::Service::active);
    const auto reserveA=people(gh,"R",20,"soldier",phaseb::Service::active);
    const auto leadB=people(gh,"R",20,"soldier",phaseb::Service::active);
    const auto reserveB=people(gh,"R",20,"soldier",phaseb::Service::active);
    c.seed_active("ATT",leadA);c.seed_active("ATT",reserveA);
    c.seed_active("DEF",leadB);c.seed_active("DEF",reserveB);
    p0::Policies opts;
    opts.engagements.push_back({"ATT","DEF","R",leadA,leadB,1'000'000});
    // Capture baselines before moving the source world into the day engine.
    // A reference into the moved-from GH world is not a valid oracle.
    const auto population_before=c.people().population();
    const auto events_before=c.people().events().size();
    p0::DailyGame g(daily::World::adopt(std::move(gh),17),opts);
    g.advance_day();
    // Twelve rounds at this calibration wound people but need not kill any.
    require_test(::c(g).people().population()==population_before &&
                 ::c(g).people().events().size()>events_before,
       "extra unit cohorts must not disable actual injury transitions");
    require_test(::c(g).stock("ATT","R","ammunition")<12 &&
                 ::c(g).stock("DEF","R","ammunition")<12,
       "multi-cohort engagement must consume actual ammunition");
}


static void multiple_households_do_not_inflate_one_sellers_stock() {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"R","A");
    geo.seed_unit(11,"A","ROOT");geo.seed_assignment(1,11);
    auto& c=geo.seed_core().seed_core();
    c.seed_actor("A",100);
    c.seed_actor("SELLER",0);
    c.seed_actor("HH1",1000);
    c.seed_actor("HH2",1000);
    c.seed_stock("SELLER","R","food",10);
    c.seed_market("R","food",100,1,300,7);
    const auto h1=people(gh,"R",2,"civilian");
    const auto h2=people(gh,"R",2,"civilian");
    p0::Policies opts;
    opts.buyers.push_back({h1,"HH1","SELLER","R","food",1});
    opts.buyers.push_back({h2,"HH2","SELLER","R","food",1});
    p0::DailyGame game(daily::World::adopt(std::move(gh),5),opts);
    game.advance_day();
    require_test(::c(game).stock("SELLER","R","food")==6,
        "two households must buy four real goods");
    require_test(::c(game).market("R","food").price==100,
        "multiple purchasers must not double count one sellers offered stock");
}
static void a_strike_is_not_a_permanent_hiring_vacancy() {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"R","A");
    geo.seed_unit(11,"A","ROOT");geo.seed_assignment(1,11);
    auto& c=geo.seed_core().seed_core();
    c.seed_actor("A",100);
    c.seed_actor("FIRM",100);
    c.seed_interest_group("MOVEMENT");
    const auto workforce=people(gh,"R",4,"worker");
    c.seed_population().hire(workforce,2,"FIRM","worker",10,
                             phaseb::JobRequirements{});
    phasegh::World::Plan plan;
    plan[9].push_back([workforce](phasegh::Stage& step) {
        auto& c=step.phase_c();
        c.join_interest_group(workforce,"MOVEMENT",2);
        c.protest(workforce,"MOVEMENT",2,2);
    });
    gh.advance(plan);
    require_test(gh.month()==24001,"strike fixture must reach following month");
    p0::Policies opts;
    opts.staffing.push_back({"FIRM","R",2,10});
    p0::DailyGame game(daily::World::adopt(std::move(gh),6),opts);
    game.advance_day();
    require_test(::c(game).people().employed(workforce)==2,
       "temporary strike must not create new permanent employment contracts");
    require_test(::c(game).currently_striking(workforce)==2,
       "actual strike should remain active for the configured period");
}


static void native_autonomous_extraction_physical_flow() {
    auto w=fixture();
    auto& d=w.seed_geography().seed_core();
    auto& core=d.seed_core();
    core.seed_production_facility("MINE","FIRM","R",31);
    core.seed_market("R","coal",20,1,40,6);
    phased::ResourceSite site;
    site.id="COAL-MINE";
    site.site="MINE";site.owner="FIRM";site.region="R";
    site.good="coal";site.monthly_capacity=31;
    site.workers_per_unit=1;site.site_service_per_unit=1;
    d.seed_resource_site(site);
    const auto worker=core.seed_population().slices().begin()->first;
    core.seed_population().hire(worker,4,"FIRM","worker",10,phaseb::JobRequirements{});
    p0::Policies rules;
    rules.extractors.push_back({"COAL-MINE",5,1,"WORKERS","TREASURY"});
    p0::DailyGame game(daily::World::adopt(std::move(w),19),rules);
    game.advance_day();
    require_test(::c(game).stock("FIRM","R","coal")==1 &&
                 ::c(game).cash("WORKERS")==1,
                 "resource extractor did not produce one day of physical output and wages");
    const auto initial=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame replay(daily::SnapshotCodec::decode(initial),rules);
    for(int i=0;i<30;i++) {
        game.advance_day();
        replay.advance_day();
    }
    require_test(daily::SnapshotCodec::encode(game.world())==
                 daily::SnapshotCodec::encode(replay.world()),
                 "resource site native day replay did not preserve actual flow");
    const auto& r=game.world().engine().base().core().resource_site("COAL-MINE");
    require_test(r.cumulative_output==5 &&
                 ::c(game).stock("FIRM","R","coal")==5 &&
                 ::c(game).cash("WORKERS")==5,
                 "resource extractor must stop at actual inventory target");
    game.advance_day(); // February 1, no hidden underground stock regenerated.
    require_test(game.world().engine().base().core().resource_site("COAL-MINE")
                          .cumulative_output==5,
                 "date rollover must not invent or duplicate in-situ output");
}


static void domestic_quarry_hires_and_supplies_paid_glassworks() {
    const auto run=[](bool viable,bool older_debt=false,bool expensive_contract=false) {
        phasegh::World w(24000);
        auto& geography=w.seed_geography();
        geography.seed_province(1,"R","A");
        geography.seed_unit(1,"A","LOCAL");
        geography.seed_assignment(1,1);
        auto& d=geography.seed_core();
        auto& core=d.seed_core();
        core.seed_actor("A",100);
        core.seed_government("A","head","prime",5,3);
        for(const auto* owner:{"QUARRY","GLASS"})core.seed_actor(owner,1000);
        core.seed_market("R","sand",viable?10:1,1,viable?30:1,2);
        core.seed_market("R","coal",10,1,30,0);
        core.seed_market("R","glass",100,1,300,3);
        core.seed_stock("GLASS","R","coal",5);
        d.seed_standard_industries();
        d.seed_building_facility("GLASSWORKS","GLASS","R",31,
                                "vic3_115.urban.glassworks");
        phased::IndustryRecipe glass;
        glass.id="GLASS-RECIPE";glass.site="GLASSWORKS";
        glass.inputs={{"coal",1},{"sand",1}};glass.outputs={{"glass",1}};
        glass.workers_per_batch=1;glass.capacity_per_batch=1;
        glass.industry_class="MANU";
        d.seed_building_recipe(glass);
        const auto quarry_worker=people(w,"R",4,"worker");
        const auto glass_worker=people(w,"R",1,"glass-worker");
        core.seed_population().hire(glass_worker,1,"GLASS","glass-worker",31,{});
        const auto wallet=[&](auto id) {
            return "HH:"+std::to_string(core.people().slice(id).key.household);
        };
        const auto quarry_wallet=wallet(quarry_worker);
        const auto glass_wallet=wallet(glass_worker);
        core.seed_actor(quarry_wallet,0);
        if(glass_wallet!=quarry_wallet)core.seed_actor(glass_wallet,0);
        if(expensive_contract) {
            core.seed_actor("BENCHMARK",100);
            const auto benchmark=people(w,"R",2,"benchmark-worker");
            core.seed_population().hire(benchmark,1,"BENCHMARK","benchmark-worker",31,{});
            core.seed_actor(wallet(benchmark),0);
            const auto expensive=people(w,"R",3,"certified-quarry-worker");
            core.seed_population().hire(expensive,1,"QUARRY","certified-quarry-worker",620,{});
            core.seed_actor(wallet(expensive),0);
        }
        const int site_capacity=expensive_contract?62:31;
        core.seed_production_facility("SAND-SITE","QUARRY","R",site_capacity);
        phased::ResourceSite sand;
        sand.id="SAND-RESOURCE";sand.site="SAND-SITE";sand.owner="QUARRY";
        sand.region="R";sand.good="sand";sand.monthly_capacity=site_capacity;
        sand.workers_per_unit=1;sand.site_service_per_unit=1;
        d.seed_resource_site(sand);

        p0::Policies policies;
        policies.allow_endogenous_staffing=true;
        policies.daily_food_units_per_person=0;
        policies.auto_national_budgets_from_actual_services=false;
        if(older_debt)core.seed_actor("FORMER-QUARRY-WORKER",0);
        auto clock=daily::World::adopt(std::move(w),19);
        if(older_debt) {
            phasegh::World::Plan earned;
            earned[5].push_back([today=clock.day_index()](phasegh::Stage& stage) {
                stage.phase_c().phase_time_accrue_contract_wage(
                    "quarry-previous-earned","QUARRY","FORMER-QUARRY-WORKER",
                    "A","A",1000,today);
            });
            clock.advance_day(earned);
        }
        p0::DailyGame game(std::move(clock),policies);
        require_test(::c(game).phase_d_workforce("QUARRY")==static_cast<int>(expensive_contract)&&
                     !::c(game).stock("QUARRY","R","sand")&&
                     ::c(game).has_actor(quarry_wallet),
                     "quarry fixture must not preseed sand or quarry labor");
        require_test(!older_debt||
                     (::c(game).cash("QUARRY")==1000&&
                      ::c(game).outstanding_worker_wages("QUARRY")==1000),
                     "prior earned quarry wages must coexist with positive opening cash");
        game.advance_day();
        const auto& resource=game.world().engine().base().core().resource_site("SAND-RESOURCE");
        if(expensive_contract) {
            require_test(::c(game).phase_d_workforce("QUARRY")==1&&
                         !resource.cumulative_output&&
                         !::c(game).stock("GLASS","R","glass")&&
                         ::c(game).cash("QUARRY")==980&&
                         !::c(game).outstanding_worker_wages("QUARRY"),
                         "quarry forecast must include its highest existing daily contract wage");
            return;
        }
        if(older_debt) {
            require_test(!::c(game).phase_d_workforce("QUARRY")&&
                         !resource.cumulative_output&&
                         ::c(game).cash("FORMER-QUARRY-WORKER")==900&&
                         ::c(game).cash("QUARRY")==0&&
                         !::c(game).outstanding_worker_wages("QUARRY"),
                         "opening cash owed for old quarry wages cannot fund new recruiting");
            return;
        }
        if(::c(game).phase_d_workforce("QUARRY")!=static_cast<int>(viable)||
           resource.cumulative_output!=static_cast<int>(viable))
            throw std::runtime_error(std::string("quarry employment/output (expected ")+
                std::to_string(viable)+") was "+
                std::to_string(::c(game).phase_d_workforce("QUARRY"))+"/"+
                std::to_string(resource.cumulative_output));
        if(viable)
            require_test(::c(game).stock("GLASS","R","sand")==1&&
                         ::c(game).cash("QUARRY")>1000&&
                         ::c(game).cash("GLASS")<1000&&
                         ::c(game).cash(quarry_wallet)==1&&
                         !::c(game).stock("GLASS","R","glass"),
                         "quarry sand must be bought for actual cash before factory use");
        const auto checkpoint=daily::SnapshotCodec::encode(game.world());
        p0::DailyGame replay(daily::SnapshotCodec::decode(checkpoint),policies);
        game.advance_day();
        replay.advance_day();
        require_test(::c(game).stock("GLASS","R","glass")==static_cast<int>(viable)&&
                     ::c(game).stock("GLASS","R","coal")==5-static_cast<int>(viable)&&
                     daily::SnapshotCodec::encode(game.world())==
                         daily::SnapshotCodec::encode(replay.world()),
                     "paid domestic inputs must make only real glass and reproduce on replay");
    };
    run(true);run(false);run(true,true);run(true,false,true);
}

enum class ResearchLawScope { actual, local, custom, foreign, none, other, dated };
static void paid_laboratory_research_speed(
        ResearchLawScope scope,const char* discipline,int first,int days,bool trait=false,
        bool education=false,bool stack_economic=false,bool low_daily_limit=false,
        int paid_after_twenty_days=-1) {
    using Scope=ResearchLawScope;
    using namespace phasec;
    auto gh=fixture();
    auto& geo=gh.seed_geography();
    geo.seed_province(88,"FOREIGN","B");geo.seed_unit(88,"B","Root B");geo.seed_assignment(88,88);
    auto& d=geo.seed_core();auto& core=d.seed_core();
    core.seed_actor("B",100);core.seed_government("B","HB","PB",5,3);
    for(const auto* nat:{"A","B"})
        core.seed_p1_constitution(nat,{P1GovernancePrinciple::monarchy,
            P1PowerDistribution::autocracy,P1LegislatureKind::none,"test:constitution"});
    if(scope!=Scope::none) {
        const bool fake=scope==Scope::local||scope==Scope::custom;
        P2BaseLaw law{"LAB-LAW",scope==Scope::foreign?"B":"A",
            fake?"legacy_research_evidence":(education?"education_system":"economic_system"),
            scope==Scope::local?"R":"national",
            education?(scope==Scope::other||scope==Scope::dated?
                         "law_private_schools":"law_terakoya"):"law_industry_banned",
            "test:source"};
        if(scope==Scope::local)law.tier=P2NormTier::local_ordinance;
        core.p2_seed_base(law);
    }
    if(stack_economic)
        core.p2_seed_base({"INDUSTRY","A","economic_system","national",
                           "law_industry_banned","test:industry"});
    const auto scientist=people(gh,"R",40,"scientist");
    core.seed_population().hire(scientist,40,"FIRM","scientist",1,{});
    const auto scientist_wallet="HH:"+std::to_string(
        core.people().slice(scientist).key.household);
    core.seed_production_facility("RESEARCH-LAB","FIRM","R",1240);
    phased::TechnologyDefinition tech;tech.id="SOURCED";d.seed_technology(tech);
    phased::ResearchProject p;p.id="RESEARCH";p.holder="FIRM";p.sponsor="A";
    p.lab_site="RESEARCH-LAB";p.region="R";p.tech_id=tech.id;
    const bool explicit_micro_order=paid_after_twenty_days>=0;
    const int required=low_daily_limit?10:explicit_micro_order?18:40;
    const std::string research_material=low_daily_limit?"lab-paper":"ore";
    p.material_good=research_material;
    if(low_daily_limit) {
        core.seed_actor("PAPER-MERCHANT",0);
        core.seed_stock("PAPER-MERCHANT","R",research_material,required);
    }
    p.required_evidence=required;p.material_per_work=1;p.workers_per_work=1;p.cash_cost_per_work=5;
    d.seed_research(p);
    if(trait) {
        core.seed_interest_group("INDUSTRIALISTS");
        core.p3_seed_group({"A","INDUSTRIALISTS","","","",P3GroupType::industrialists,0,0});
        core.p3_set_ideologies("A","INDUSTRIALISTS",{"luddite"});
    }
    auto world=daily::World::adopt(std::move(gh),56);
    phasegh::World::Plan establish;
    if(trait)establish[9].push_back([scientist](phasegh::Stage& stage) {
        stage.phase_c().join_interest_group(scientist,"INDUSTRIALISTS",40);
    });
    const auto submitted=world.day_index();
    if(scope==Scope::dated)establish[9].push_back([submitted](phasegh::Stage& stage) {
        auto& core=stage.phase_c();
        P2Bill bill;
        bill.id="TERAKOYA-BILL";bill.country="A";bill.proposer="PM_A";
        bill.legal_basis="test:legislation";bill.effective_day=submitted+2;
        bill.action=P2BillAction::enact_or_replace_base;
        bill.proposed_base={"TERAKOYA","A","education_system","national",
                            "law_terakoya","test:effective education law"};
        core.p2_submit_bill(std::move(bill),submitted);
        core.p2_approve_bill("TERAKOYA-BILL",{});
        core.p2_promulgate_bill("TERAKOYA-BILL","headA",submitted);
    });
    world.advance_day(establish);
    if(trait)require_test(world.engine().base().core().core().p3_trait_modifier_ppm(
                           "A","country_production_tech_research_speed_mult")==100'000,
                         "fixture lacks actual active Industrialist trait");
    p0::Policies policies;
    // Capture exact lab receipts and lab inputs, not unrelated farm output.
    policies.manufacturers.push_back({"FOOD_RECIPE",0});
    if(!low_daily_limit)
        policies.researches.push_back({"RESEARCH",explicit_micro_order?1:20,discipline});
    // No policy override in the low case: use P0's own one-unit daily order.
    p0::DailyGame game(std::move(world),policies);
    const auto progress=[&]() -> const phased::ResearchProject& {
        return game.world().engine().base().core().state().researches.at("RESEARCH");
    };
    if(low_daily_limit) {
        const auto starting_cash=c(game).cash("A");
        for(int i=0;i<3;++i)game.advance_day();
        phasegh::World::Plan deliver;
        deliver[3].push_back([&](phasegh::Stage& stage) {
            stage.phase_c().trade("PAPER-MERCHANT","FIRM","R",research_material,required,1);
        });
        game.advance_day(deliver); // Stage 1 could not reserve before delivery.
        require_test(progress().paid_work==0&&c(game).cash("A")==starting_cash&&
                     c(game).stock("FIRM","R",research_material)==required&&
                     c(game).cash("PAPER-MERCHANT")==required,
                     "missing lab material caused unbacked research or purchase");
    }
    const auto cash=c(game).cash("A"),firm=c(game).cash("FIRM"),
        inputs=c(game).stock("FIRM","R",research_material);
    const auto opening_payroll=c(game).has_actor(scientist_wallet)?
        c(game).cash(scientist_wallet):Money{0};
    Money new_payroll=0;
    for(int step=1;step<=days;++step) {
        const auto date=game.world().date();
        const auto days_this_month=daily::days_in_month(date.year,date.month);
        new_payroll+=40*date.day/days_this_month-40*(date.day-1)/days_this_month;
        game.advance_day();
        if(step==1&&!low_daily_limit&&!explicit_micro_order) {
            const std::string why="wrong actual funded research speed (scope "+
                std::to_string(static_cast<int>(scope))+", "+discipline+
                ", education="+std::to_string(education)+")";
            require_test(progress().verified_work==first&&progress().paid_work==first,why.c_str());
        }
        if(scope==Scope::dated&&step<=2) {
            require_test(c(game).p2_selected_law("A","education_system")==
                         (step==1?"private_schools":"terakoya"),
                         "Terakoya changed a lab outside its lawful effective date");
            require_test(progress().paid_work==(step==1?20:38),
                         "effective Terakoya did not reduce today's paid work, or changed prior work");
        }
        if(low_daily_limit&&step==10)
            require_test(progress().paid_work==9&&progress().verified_work==9,
                         "fractional Terakoya research rate stalled or rounded away");
        if(explicit_micro_order&&step==20)
            require_test(progress().paid_work==paid_after_twenty_days&&
                         progress().verified_work==paid_after_twenty_days,
                         "fractional production research rate stalled or a second penalty accelerated it");
        require_test(c(game).cash("A")==cash-5*progress().paid_work&&
                     c(game).cash("FIRM")==firm+5*progress().paid_work-new_payroll&&
                     c(game).cash(scientist_wallet)==opening_payroll+new_payroll&&
                     c(game).stock("FIRM","R",research_material)==inputs-progress().paid_work,
                     "daily paid evidence did not match actual lab money and material");
        if(!low_daily_limit&&!explicit_micro_order)
            require_test(progress().reviewed==(step==days),"research review ran on the wrong game date");
    }
    phaseb::People evidence=0;
    for(const auto& e:game.world().engine().base().core().state().research_evidence)
        if(e.project=="RESEARCH")evidence+=e.units;
    require_test(c(game).cash("A")==cash-5*required&&c(game).cash("FIRM")==firm+5*required-new_payroll&&
                 c(game).cash(scientist_wallet)==opening_payroll+new_payroll&&
                 c(game).stock("FIRM","R",research_material)==inputs-required&&evidence==required&&
                 progress().paid_work==required&&progress().verified_work==required&&
                 progress().required_evidence==required&&progress().reviewed,
                 "effective speed invented paid research, funds, or inputs");
    while(game.world().date()<daily::Date{2000,2,1})game.advance_day();
    require_test(game.world().engine().base().core().knowledge("FIRM","SOURCED")!=
                 phased::KnowledgeLevel::usable,"research bypassed monthly knowledge date");
    game.advance_day();
    require_test(game.world().engine().base().core().knowledge("FIRM","SOURCED")==
                 phased::KnowledgeLevel::usable&&c(game).cash("A")==cash-5*required&&
                 c(game).stock("FIRM","R",research_material)==inputs-required,
                 "funded research failed dated promotion or spent twice");
}
static void production_research_speed_requires_actual_economic_law() {
    using Scope=ResearchLawScope;
    paid_laboratory_research_speed(Scope::actual,"production",15,3);
    paid_laboratory_research_speed(Scope::actual,"production",17,3,true);
    paid_laboratory_research_speed(Scope::foreign,"production",20,2);
    paid_laboratory_research_speed(Scope::none,"production",20,2);
    paid_laboratory_research_speed(Scope::actual,"military",20,2);
    paid_laboratory_research_speed(Scope::actual,"society",20,2);
    paid_laboratory_research_speed(Scope::local,"production",20,2);
    paid_laboratory_research_speed(Scope::custom,"production",20,2);
}

static void enacted_terakoya_slows_actual_paid_original_research() {
    using Scope=ResearchLawScope;
    const auto run=[](Scope scope,const char* discipline,int first,int days,
                      bool stack_economic=false,bool low_daily_limit=false) {
        paid_laboratory_research_speed(
            scope,discipline,first,days,false,true,stack_economic,low_daily_limit);
    };
    run(Scope::actual,"",18,3);
    run(Scope::actual,"military",18,3);
    run(Scope::actual,"society",18,3);
    run(Scope::actual,"production",13,4,true);
    // Twenty dates schedule 15 whole units at 75% and 13 at 65%.
    // Adding the education penalty must not restart a stalled 1-unit lab.
    paid_laboratory_research_speed(Scope::actual,"production",0,30,
                                   false,false,false,false,15);
    paid_laboratory_research_speed(Scope::actual,"production",0,30,
                                   false,true,true,false,13);
    run(Scope::actual,"",0,12,false,true);
    run(Scope::foreign,"",20,2);
    run(Scope::local,"",20,2);
    run(Scope::custom,"",20,2);
    run(Scope::other,"",20,2);
    run(Scope::dated,"",20,3);
}

static void native_autonomous_funded_research_and_day_lab_limit(bool competing_factory=false) {
    auto gh=fixture();
    auto& d=gh.seed_geography().seed_core();
    auto& c=d.seed_core();
    c.seed_production_facility("RESEARCH-LAB","FIRM","R",31);
    phased::TechnologyDefinition tech;
    tech.id="TECH-DAILY";
    d.seed_technology(tech);
    phased::ResearchProject project;
    project.id="PROJECT-RESEARCH";
    project.holder="FIRM";
    project.sponsor="A";
    project.lab_site="RESEARCH-LAB";
    project.region="R";
    project.material_good="ore";
    project.tech_id="TECH-DAILY";
    project.material_per_work=1;
    project.required_evidence=2;
    project.workers_per_work=1;
    project.cash_cost_per_work=5;
    d.seed_research(project);
    const auto worker=c.seed_population().slices().begin()->first;
    c.seed_population().hire(worker,4,"FIRM","worker",10,
                             phaseb::JobRequirements{});
    p0::Policies rules;
    if(!competing_factory)rules.manufacturers.push_back({"FOOD_RECIPE",0});
    rules.researches.push_back({"PROJECT-RESEARCH",1});
    p0::DailyGame game(daily::World::adopt(std::move(gh),23),rules);
    const auto sponsor_cash=::c(game).cash("A");
    const auto materials=::c(game).stock("FIRM","R","ore");
    game.advance_day();
    const auto& progress=game.world().engine().base().core().state().researches.at(
                          "PROJECT-RESEARCH");
    require_test(progress.verified_work==1&&progress.paid_work==1&&
                 !progress.reviewed&&::c(game).cash("A")==sponsor_cash-5&&
                 ::c(game).stock("FIRM","R","ore")==materials-1,
                 "research Day1 did not use actual paid labor, materials and lab");
    auto bytes=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame replay(daily::SnapshotCodec::decode(bytes),rules);
    game.advance_day();replay.advance_day();
    require_test(daily::SnapshotCodec::encode(game.world())==
                 daily::SnapshotCodec::encode(replay.world()),
                 "research stage replay must be deterministic");
    if(competing_factory) {
        const auto& state=game.world().engine().base().core().state();
        const auto& actual=state.researches.at("PROJECT-RESEARCH");
        require_test(!actual.reviewed&&actual.verified_work==1&&actual.paid_work==1&&
                     actual.planned_work==0&&::c(game).cash("A")==sponsor_cash-5&&
                     ::c(game).stock("FIRM","R","ore")==materials-5,
                     "competing real output did not use four existing workers or cancel only unused lab funding");
        return;
    }
    require_test(game.world().engine().base().core().state().researches.at(
                     "PROJECT-RESEARCH").reviewed &&
                 ::c(game).cash("A")==sponsor_cash-10 &&
                 ::c(game).stock("FIRM","R","ore")==materials-2 &&
                 game.world().engine().base().core().knowledge(
                     "FIRM","TECH-DAILY")!=phased::KnowledgeLevel::usable,
                 "real paid research must be reviewed on Day2, not instantly usable");
    for(int day=3;day<=31;++day)game.advance_day();
    game.advance_day(); // February 1: next-month researched technology is usable.
    require_test(game.world().engine().base().core().knowledge(
                     "FIRM","TECH-DAILY")==phased::KnowledgeLevel::usable &&
                 ::c(game).cash("A")==sponsor_cash-10 &&
                 ::c(game).stock("FIRM","R","ore")==materials-2,
                 "next-month knowledge must not duplicate financing or material");
}


static phasegh::World real_h_crisis_and_organization_fixture() {
    phasegh::World w(23998); // November 1999, legacy seeding only
    auto& e=w.seed_geography();
    e.seed_province(1,"RA","A");e.seed_province(2,"RB","B");
    e.seed_province(3,"RC","C");e.seed_border(1,2);e.seed_border(2,3);
    e.seed_unit(11,"A","UA");e.seed_unit(22,"B","UB");
    e.seed_unit(33,"C","UC");
    e.seed_assignment(1,11);e.seed_assignment(2,22);e.seed_assignment(3,33);
    auto& c=e.seed_core().seed_core();
    for(const char* name:{"A","B","C"})c.seed_actor(name,100);
    c.seed_actor("ORG:LEAGUE",0);
    c.seed_government("A","HEAD_A","PM_A",5,3);
    c.seed_government("B","HEAD_B","PM_B",5,3);
    c.seed_government("C","HEAD_C","PM_C",5,3);
    phasegh::World::Plan claimed;
    claimed[9].push_back([](phasegh::Stage& h){
        h.geography().queue_claim("CLAIM-A-RB",2,"A",true);
    });
    w.advance(claimed); // December 1999, true claim in E
    phasegh::World::Plan organized;
    organized[9].push_back([](phasegh::Stage& h){
        h.open_crisis("CRISIS-RB",2,"A","B","PM_A");
        h.create_organization("LEAGUE",{"A","B"},10,
                              {{"A","PM_A"},{"B","PM_B"}});
    });
    w.advance(organized); // January 2000, legal pending activation
    require_test(w.month()==24000,"crisis fixture not aligned to 2000");
    return w;
}

static void native_autonomous_h_crisis_and_organization_funding() {
    auto gh=real_h_crisis_and_organization_fixture();
    auto day=daily::World::adopt(std::move(gh),77);
    phasegh::World::Plan mediated;
    mediated[9].push_back([](phasegh::Stage& s) {
        s.mediate_crisis("CRISIS-RB","C","PM_C");
        phasegh::CrisisProposal offer;
        offer.id="REAL-OFFER";offer.proposer="C";
        offer.payer="B";offer.recipient="A";offer.compensation=20;
        offer.freeze_claim=true;
        s.offer_crisis_compromise("CRISIS-RB",offer,"PM_C");
    });
    day.advance_day(mediated); // Jan 1: real H legal state and proposal
    require_test(day.engine().diplomacy().crises.at("CRISIS-RB").status==
                 phasegh::CrisisStatus::mediation &&
                 day.engine().diplomacy().organizations.at("LEAGUE").active,
                 "H real legal effectiveness or neutral mediation failed");

    p0::Policies mandates;
    mandates.crisis_mandates.push_back({"A","PM_A",0,true,false});
    mandates.crisis_mandates.push_back({"B","PM_B",30,true,false});
    mandates.organization_dues.push_back({"A","PM_A","LEAGUE"});
    mandates.organization_dues.push_back({"B","PM_B","LEAGUE"});
    p0::DailyGame game(std::move(day),mandates);
    game.advance_day(); // Jan 2: actual country policy decisions and payments
    const auto& org=game.world().engine().diplomacy().organizations.at("LEAGUE");
    const auto& crisis=game.world().engine().diplomacy().crises.at("CRISIS-RB");
    require_test(crisis.status==phasegh::CrisisStatus::settled &&
                 crisis.proposals.at("REAL-OFFER").approvers.size()==2 &&
                 c(game).cash("A")==90 && c(game).cash("B")==90 &&
                 c(game).cash("ORG:LEAGUE")==20 &&
                 org.paid.at({24000,"A"})==10 && org.paid.at({24000,"B"})==10,
                 "H autonomous diplomacy must use actual signed offers/dues/treasury");
    const auto snapshot=daily::SnapshotCodec::encode(game.world());
    auto replay=p0::DailyGame(daily::SnapshotCodec::decode(snapshot),mandates);
    game.advance_day();replay.advance_day();
    require_test(daily::SnapshotCodec::encode(game.world())==
                 daily::SnapshotCodec::encode(replay.world()) &&
                 c(game).cash("ORG:LEAGUE")==20,
                 "H autonomous obligations replayed or paid duplicate monthly dues");

    auto gh_unwilling=real_h_crisis_and_organization_fixture();
    auto pending=daily::World::adopt(std::move(gh_unwilling),77);
    pending.advance_day(mediated);
    p0::Policies no_coercion=mandates;
    no_coercion.crisis_mandates[1].maximum_cash_settlement=5;
    p0::DailyGame limited(std::move(pending),no_coercion);
    limited.advance_day();
    require_test(limited.world().engine().diplomacy().crises.at("CRISIS-RB").status==
                 phasegh::CrisisStatus::mediation &&
                 limited.world().engine().diplomacy().crises.at("CRISIS-RB").
                      proposals.at("REAL-OFFER").approvers.size()==1,
                 "unaffordable crisis concession must not be accepted automatically");

    auto gh_unauthorized=real_h_crisis_and_organization_fixture();
    auto unauthorized=daily::World::adopt(std::move(gh_unauthorized));
    unauthorized.advance_day(mediated);
    p0::Policies wrong=mandates;
    wrong.organization_dues[0].prime_minister="INVALID-PRIME";
    p0::DailyGame rejected(std::move(unauthorized),wrong);
    const auto before=daily::SnapshotCodec::encode(rejected.world());
    bool failed=false;
    try{rejected.advance_day();}catch(const std::exception&){failed=true;}
    require_test(failed && daily::SnapshotCodec::encode(rejected.world())==before,
                 "H unlawful government policy must rollback every daily domain");
}


static void explicit_and_automatic_dues_keep_live_payment_priority() {
    for(const phasec::Money opening:{16,6}) {
        phasegh::World gh(23999);
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"RA","A");geo.seed_unit(1,"A","DISTRICT_A");
        geo.seed_province(2,"RB","B");geo.seed_unit(2,"B","DISTRICT_B");
        geo.seed_assignment(1,1);geo.seed_assignment(2,2);
        auto& core=geo.seed_core().seed_core();
        core.seed_actor("A",opening);
        if(opening==16)core.seed_actor("B",0);
        core.seed_actor("ORG:Z",0);core.seed_actor("ORG:A",0);core.seed_actor("RESERVE",20);
        core.seed_government("A","HEAD_A","PM_A",5,3);
        core.seed_government("B","HEAD_B","PM_B",5,3);
        phasegh::World::Plan charters;
        charters[9].push_back([](phasegh::Stage& stage) {
            stage.create_organization("Z",{"A","B"},10,{{"A","PM_A"},{"B","PM_B"}});
            stage.create_organization("A",{"A","B"},10,{{"A","PM_A"},{"B","PM_B"}});
        });
        gh.advance(charters);
        p0::Policies policy;
        policy.auto_national_budgets_from_actual_services=false;
        policy.organization_dues.push_back({"A","PM_A","Z"});
        p0::DailyGame game(daily::World::adopt(std::move(gh),37),policy);
        const auto observe=[](const p0::DailyGame& live,const char* id) {
            const auto& org=live.world().engine().diplomacy().organizations.at(id);
            const auto it=org.paid.find({24000,"A"});
            return it==org.paid.end()?0:it->second;
        };
        game.advance_day();
        const phasec::Money paid_z=opening==16?10:0;
        require_test(observe(game,"A")==6&&observe(game,"Z")==paid_z&&
                     c(game).cash("ORG:A")==6&&c(game).cash("ORG:Z")==paid_z&&
                     c(game).cash("A")==0,
                     "manual full/skipped dues priority or sorted automatic partial payments changed");
        const auto snapshot=daily::SnapshotCodec::encode(game.world());
        p0::DailyGame replay(daily::SnapshotCodec::decode(snapshot),policy);
        phasegh::World::Plan top_up;
        top_up[3].push_back([opening](phasegh::Stage& stage) {
            stage.phase_c().pay("RESERVE","A",20-opening);
        });
        game.advance_day(top_up);replay.advance_day(top_up);
        const auto fully_paid=daily::SnapshotCodec::encode(game.world());
        require_test(fully_paid==daily::SnapshotCodec::encode(replay.world())&&
                     observe(game,"Z")==10&&observe(game,"A")==10&&
                     c(game).cash("ORG:A")==10&&c(game).cash("ORG:Z")==10&&c(game).cash("A")==0,
                     "top-up failed to settle only the actual remainder or broke snapshot replay");
        game.advance_day();
        require_test(observe(game,"Z")==10&&observe(game,"A")==10&&
                     c(game).cash("ORG:A")==10&&c(game).cash("ORG:Z")==10,
                     "fully paid obligations must not charge a live ledger again");
        policy.organization_dues.front().prime_minister="WRONG";
        p0::DailyGame invalid(daily::SnapshotCodec::decode(fully_paid),policy);
        bool rejected=false;
        try {invalid.advance_day();} catch(const std::invalid_argument& e) {
            rejected=std::string_view(e.what())=="P0_DUES_UNAUTHORIZED_GOVERNMENT";
        }
        require_test(rejected&&daily::SnapshotCodec::encode(invalid.world())==fully_paid,
                     "already-paid explicit mandate bypassed prime-minister authority or rollback");
        if(opening==6) {
            policy.organization_dues={{"B","PM_B","Z"}};
            p0::DailyGame missing(daily::SnapshotCodec::decode(fully_paid),policy);
            bool strict=false;
            try {missing.advance_day();} catch(const std::invalid_argument& e) {
                strict=std::string_view(e.what())=="country treasury needs explicit binding";
            }
            require_test(strict&&daily::SnapshotCodec::encode(missing.world())==fully_paid,
                         "automatic dues must skip a missing wallet while an explicit mandate fails atomically");
        }
    }
}


static auto paid_freight_fixture(bool expires,bool replacement=false,bool bounded=false) {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"SOURCE","A");
    geo.seed_province(2,"PORT","B");
    geo.seed_unit(1,"A","A-ADMIN");geo.seed_assignment(1,1);
    geo.seed_unit(2,"B","B-ADMIN");geo.seed_assignment(2,2);
    geo.seed_border(1,2);
    auto& d=geo.seed_core();
    auto& c=d.seed_core();
    for(const auto* actor:{"A","B","MINE","RAIL"})c.seed_actor(actor,0);
    c.seed_actor("FACTORY",4000);
    if(replacement)c.seed_actor("NEW-RAIL",0);
    c.seed_government("A","HEAD-A","PM-A",5,3);
    c.seed_government("B","HEAD-B","PM-B",5,3);
    c.seed_market("SOURCE","ore",10,10,10);
    c.seed_market("PORT","ore",30,30,30);
    c.seed_market("PORT","fuel",4,4,4);
    c.seed_market("PORT","tools",40,1,100);
    c.seed_stock("MINE","SOURCE","ore",200);
    c.seed_stock("FACTORY","PORT","fuel",200);
    c.seed_production_facility("WORKS","FACTORY","PORT",31);
    phased::IndustryRecipe recipe;
    recipe.id="TOOLS";recipe.site="WORKS";
    recipe.inputs={{"ore",2},{"fuel",2}};recipe.outputs={{"tools",2}};
    recipe.workers_per_batch=recipe.capacity_per_batch=1;
    d.seed_recipe(recipe);
    const auto worker=people(gh,"PORT",1,"worker");
    const auto customer=people(gh,"PORT",2,"consumer");
    auto& pop=c.seed_population();
    pop.hire(worker,1,"FACTORY","worker",31,{});
    const auto wallet=[&](auto id) {
        return "HH:"+std::to_string(c.people().slice(id).key.household);
    };
    const auto buyer=wallet(customer);
    c.seed_actor(wallet(worker),0);
    c.seed_actor(buyer,10000);

    gh.seed_verified_geographic_link("rail-source",1,2,"rail");
    phasegh::TransportEdge rail;
    rail.id="sole-service";rail.carrier="RAIL";rail.from=1;rail.to=2;
    rail.monthly_capacity=62;rail.freight_per_unit=1;rail.transit_days=2;
    rail.mode="rail";rail.evidence_id="fixture:dated-rail-service";
    rail.valid_from_month=24000;rail.valid_through_month=expires?24000:24002;
    gh.seed_transport_edge(rail);
    if(replacement) {
        rail.id="replacement-service";rail.carrier="NEW-RAIL";
        rail.freight_per_unit=3;rail.evidence_id="fixture:replacement-dated-rail-service";
        rail.valid_from_month=rail.valid_through_month=24002;
        gh.seed_transport_edge(rail);
    }
    gh.seed_route({"ORE","MINE","FACTORY","ore",1,2,62,1});

    p0::Policies rules;
    rules.auto_national_budgets_from_actual_services=false;
    rules.daily_food_units_per_person=rules.monthly_clothing_units_per_person=0;
    // No fictional substitute input. Both choices use the same real
    // recurring order and buyer, isolated from independent discovery.
    rules.allow_endogenous_input_procurement=false;
    rules.buyers.push_back({customer,buyer,"FACTORY","PORT","tools",1});
    if(bounded)rules.standing_route_stock_targets.emplace("ORE",2);
    return std::pair{p0::DailyGame(daily::World::adopt(std::move(gh),85),rules),buyer};
}

static Quantity tools_batches(const p0::DailyGame& game) {
    Quantity n=0;
    for(const auto& run:game.world().engine().base().core().state().actual_production_runs)
        if(run.recipe=="TOOLS")n+=run.batches;
    return n;
}

static const phasegh::TradeState& freight(const p0::DailyGame& game) {
    return game.world().engine().commerce();
}

static void loss_of_paid_freight_constrains_output_and_solvent_customers() {
    auto [open,buyer]=paid_freight_fixture(false);
    auto [cut,cut_buyer]=paid_freight_fixture(true);
    auto [recover,recover_buyer]=paid_freight_fixture(true,true);
    for(int i=0;i<31;++i){open.advance_day();cut.advance_day();recover.advance_day();}
    const auto old_batches=tools_batches(cut);
    const auto old_fills=freight(cut).fills.size();
    require_test(cut.world().date()==daily::Date{2000,2,1} && cut_buyer==buyer &&
                 old_batches>20 && old_batches==tools_batches(open) &&
                 old_fills==freight(open).fills.size() &&
                 !freight(cut).daily_shipments.empty() &&
                 !freight(open).daily_shipments.empty() &&
                 c(cut).cash(buyer)==c(open).cash(buyer) &&
                 c(cut).cash(buyer)<10000 && c(cut).cash("MINE")>0 &&
                 c(cut).cash("RAIL")>0,
                 "both operating lines must have paid cargo, actual tools and household fulfillment");
    require_test(recover_buyer==buyer && tools_batches(recover)==old_batches &&
                 freight(recover).fills.size()==old_fills &&
                 !freight(recover).daily_shipments.empty() &&
                 c(recover).cash(buyer)==c(cut).cash(buyer) &&
                 c(recover).cash("NEW-RAIL")==0,
                 "dormant March service must not change paid January freight");
    auto replay=p0::DailyGame(daily::SnapshotCodec::decode(
        daily::SnapshotCodec::encode(recover.world())),recover.policies());
    const auto step_recovery=[&](const phasegh::World::Plan& audit={}) {
        recover.advance_day(audit);replay.advance_day(audit);
        require_test(c(recover).financial().balanced() &&
                     daily::SnapshotCodec::encode(recover.world())==
                     daily::SnapshotCodec::encode(replay.world()),
                     "freight expiry and replacement must replay on the original ledger");
    };
    const auto closing_customer_cash=c(cut).cash(buyer);
    const auto old_in_flight=freight(cut).daily_shipments.size();
    cut.advance_day();open.advance_day();step_recovery();
    require_test(freight(cut).daily_shipments.size()<old_in_flight &&
                 freight(cut).fills.size()==old_fills &&
                 tools_batches(cut)>old_batches &&
                 c(cut).cash(buyer)<closing_customer_cash,
                 "expiry must honor in-flight cargo and sale without dispatching new goods");
    for(int i=0;i<7;++i){open.advance_day();cut.advance_day();step_recovery();}
    require_test(freight(cut).daily_shipments.empty() &&
                 freight(cut).fills.size()==old_fills &&
                 freight(open).fills.size()>old_fills &&
                 tools_batches(open)>tools_batches(cut) &&
                 c(open).cash(buyer)<c(cut).cash(buyer) &&
                 c(cut).previous_daily_solvent_unmet(
                     "PORT","tools",cut.world().day_index())==2 &&
                 c(open).previous_daily_solvent_unmet(
                     "PORT","tools",open.world().day_index())==0 &&
                 c(cut).market("PORT","tools").price>c(open).market("PORT","tools").price,
                 "loss of the sole real link must cut production and paid sales, and raise the solvent shortage price");

    while(recover.world().date()<daily::Date{2000,3,1}) {
        cut.advance_day();step_recovery();
        require_test(tools_batches(recover)==tools_batches(cut) &&
                     freight(recover).fills.size()==old_fills &&
                     freight(recover).daily_shipments.empty() &&
                     c(recover).cash(buyer)==c(cut).cash(buyer) &&
                     c(recover).cash("NEW-RAIL")==0,
                     "February customers cannot buy output from a replacement freight service that is still closed");
    }
    const auto drought_batches=tools_batches(recover);
    const auto drought_cash=c(recover).cash(buyer);
    const auto drought_receipts=c(recover).firm_finance("FACTORY").receipts;
    const auto old_carrier_cash=c(recover).cash("RAIL");
    const auto seller_cash=c(recover).cash("MINE");
    const auto seller_ore=c(recover).stock("MINE","SOURCE","ore");
    const auto departure_day=recover.world().day_index();
    step_recovery();cut.advance_day();
    require_test(freight(recover).fills.size()==old_fills+1 &&
                 freight(recover).daily_shipments.size()==1,
                 "opening the verified replacement must create exactly one real paid departure");
    const auto& fill=freight(recover).fills.back();
    const auto& pending=freight(recover).daily_shipments.at(fill.execution_id);
    const auto fill_id=fill.execution_id;
    const auto arrival_day=pending.arrival_day;
    const auto transit_holder="TRANSIT:DAY:"+pending.current_leg_id;
    require_test(fill.physical_path==std::vector<std::string>{"replacement-service"} &&
                 fill.shipped && fill.quantity==2 && fill.goods_value==20 && fill.freight_paid==6 &&
                 arrival_day==departure_day+2 &&
                 c(recover).cash("MINE")==seller_cash+20 &&
                 c(recover).cash("RAIL")==old_carrier_cash &&
                 c(recover).cash("NEW-RAIL")==6 &&
                 c(recover).stock("MINE","SOURCE","ore")==seller_ore-2 &&
                 c(recover).stock(transit_holder,"in_transit","ore")==2 &&
                 c(recover).stock("FACTORY","PORT","ore")==0 &&
                 tools_batches(recover)==drought_batches &&
                 c(recover).cash(buyer)==drought_cash,
                 "paid replacement cargo must exist in transit, never at the factory before arrival");
    // Reload after the replacement actually departed, not only before expiry.
    replay=p0::DailyGame(daily::SnapshotCodec::decode(
        daily::SnapshotCodec::encode(recover.world())),recover.policies());
    while(recover.world().day_index()<arrival_day) {
        step_recovery();cut.advance_day();
        require_test(c(recover).stock(transit_holder,"in_transit","ore")==2 &&
                     c(recover).stock("FACTORY","PORT","ore")==0 &&
                     tools_batches(recover)==drought_batches &&
                     c(recover).cash(buyer)==drought_cash,
                     "in-transit replacement material must not relieve the real factory or customer shortage");
    }
    const auto paid_price=c(recover).market("PORT","tools").price;
    phasegh::World::Plan audit;
    audit[1].push_back([transit_holder](phasegh::Stage& stage){
        const auto& core=stage.phase_c();
        require_test(core.stock(transit_holder,"in_transit","ore")==0 &&
                     core.stock("FACTORY","PORT","ore")==2,
                     "the replacement must deliver its physical ore before using it in manufacturing");
    });
    audit[8].push_back([](phasegh::Stage& stage){
        require_test(stage.phase_c().market("PORT","tools").delivered==2,
                     "both previously unsatisfied customers must receive real finished tools");
    });
    step_recovery(audit);cut.advance_day();
    require_test(!freight(recover).daily_shipments.contains(fill_id) &&
                 tools_batches(recover)==drought_batches+1 &&
                 c(recover).stock("FACTORY","PORT","ore")==0 &&
                 c(recover).firm_finance("FACTORY").receipts==drought_receipts+2*paid_price &&
                 c(recover).cash(buyer)==drought_cash-2*paid_price &&
                 c(recover).previous_daily_solvent_unmet("PORT","tools",recover.world().day_index())==0 &&
                 tools_batches(cut)==drought_batches && c(cut).cash(buyer)==drought_cash &&
                 c(cut).previous_daily_solvent_unmet("PORT","tools",cut.world().day_index())==2,
                 "only genuine arrival may restore actual production, consumer deliveries and factory receipts");
}

static void bounded_paid_freight_recovers_only_after_physical_arrival() {
    auto [recover,buyer]=paid_freight_fixture(true,true,true);
    auto [uncapped,uncapped_buyer]=paid_freight_fixture(true,true);
    const auto inbound=[](const p0::DailyGame& g) {
        Quantity ore=0;
        for(const auto& [id,cargo]:freight(g).daily_shipments) {
            (void)id;
            if(cargo.buyer=="FACTORY"&&cargo.destination=="PORT"&&cargo.good=="ore")
                ore+=c(g).stock("TRANSIT:DAY:"+cargo.current_leg_id,"in_transit","ore");
        }
        return ore;
    };
    const auto check_position=[&](const p0::DailyGame& g) {
        require_test(c(g).stock("FACTORY","PORT","ore")+inbound(g)<=2 &&
                     c(g).financial().balanced(),
                     "the bounded factory cannot exceed its target with physical or paid inbound ore");
    };
    recover.advance_day();uncapped.advance_day();
    require_test(inbound(recover)==2 && c(recover).cash("MINE")==20 &&
                 c(recover).cash("RAIL")==2 && tools_batches(recover)==0,
                 "day one must settle real supplier and freight payments without producing tools");
    recover.advance_day();uncapped.advance_day();
    require_test(buyer==uncapped_buyer && inbound(recover)==2 && inbound(uncapped)==4 &&
                 c(recover).stock("FACTORY","PORT","ore")==0 &&
                 c(recover).cash("MINE")==20 && c(recover).cash("RAIL")==2 &&
                 c(uncapped).cash("MINE")==40 && c(uncapped).cash("RAIL")==4,
                 "already-paid ore in transit must suppress the second order only under the cap");
    check_position(recover);

    while(recover.world().date()<daily::Date{2000,2,1}) {
        recover.advance_day();
        check_position(recover);
    }
    const auto mine_cash=c(recover).cash("MINE");
    const auto rail_cash=c(recover).cash("RAIL");
    const auto fills=freight(recover).fills.size();
    // January commitments may finish in February; later drought must be real.
    recover.advance_day();recover.advance_day();
    const auto stalled=tools_batches(recover);
    const auto household_cash=c(recover).cash(buyer);
    const auto verify_closed=[&] {
        require_test(stalled>0 && tools_batches(recover)==stalled && inbound(recover)==0 &&
                     c(recover).stock("FACTORY","PORT","ore")==0 &&
                     freight(recover).fills.size()==fills && c(recover).cash("MINE")==mine_cash &&
                     c(recover).cash("RAIL")==rail_cash && c(recover).cash("NEW-RAIL")==0 &&
                     c(recover).cash(buyer)==household_cash,
                     "no new payment, ore, or tools sale is possible during the rail outage");
    };
    verify_closed();
    while(recover.world().date()<daily::Date{2000,3,1}) {
        recover.advance_day();
        verify_closed();
    }

    recover.advance_day();
    const auto& fill=freight(recover).fills.back();
    require_test(freight(recover).fills.size()==fills+1 &&
                 fill.physical_path==std::vector<std::string>{"replacement-service"} &&
                 fill.goods_value==20 && fill.freight_paid==6 &&
                 c(recover).cash("MINE")==mine_cash+20 && c(recover).cash("RAIL")==rail_cash &&
                 c(recover).cash("NEW-RAIL")==6 && inbound(recover)==2 &&
                 c(recover).stock("FACTORY","PORT","ore")==0 && tools_batches(recover)==stalled,
                 "restored rail must book exactly one useful capped shipment without early output");
    const auto due=freight(recover).daily_shipments.at(fill.execution_id).arrival_day;
    auto replay=p0::DailyGame(daily::SnapshotCodec::decode(
        daily::SnapshotCodec::encode(recover.world())),recover.policies());
    const auto step=[&](const phasegh::World::Plan& audit={}) {
        recover.advance_day(audit);replay.advance_day(audit);
        check_position(recover);
        require_test(daily::SnapshotCodec::encode(recover.world())==
                     daily::SnapshotCodec::encode(replay.world()) &&
                     freight(recover).fills.size()==fills+1 &&
                     c(recover).cash("MINE")==mine_cash+20 && c(recover).cash("NEW-RAIL")==6,
                     "restored paid freight must replay exactly without duplicate payments");
    };
    while(recover.world().day_index()<due) {
        step();
        require_test(inbound(recover)==2 && tools_batches(recover)==stalled &&
                     c(recover).stock("FACTORY","PORT","ore")==0 &&
                     c(recover).cash(buyer)==household_cash,
                     "paid replacement cargo cannot produce or sell tools before arrival");
    }
    const auto price=c(recover).market("PORT","tools").price;
    phasegh::World::Plan audit;
    audit[1].push_back([](phasegh::Stage& stage) {
        require_test(stage.phase_c().stock("FACTORY","PORT","ore")==2,
                     "physical ore must reach the capped factory before production");
    });
    step(audit);
    require_test(inbound(recover)==0 && c(recover).stock("FACTORY","PORT","ore")==0 &&
                 tools_batches(recover)==stalled+1 && c(recover).cash(buyer)==household_cash-2*price,
                 "only the physically received ore can restore production and paid customer purchases");
}

static void commissioned_public_site_enables_paid_civilian_freight() {
    phasegh::World opening(24000);
    auto& geo=opening.seed_geography();
    geo.seed_province(1,"SOURCE","A");geo.seed_unit(1,"A","DISTRICT_A");
    geo.seed_province(2,"PORT","B");geo.seed_unit(2,"B","DISTRICT_B");
    geo.seed_border(1,2);geo.seed_assignment(1,1);geo.seed_assignment(2,2);
    auto& ledger=geo.seed_core().seed_core();
    for(const auto* actor:{"A","B","BUILDER","SELLER","TRADER","CARRIER"})
        ledger.seed_actor(actor,actor==std::string_view("A")?200:
                           actor==std::string_view("TRADER")?1000:0);
    ledger.seed_government("A","HEAD","PM",5,3);
    ledger.seed_p1_constitution("A",{phasec::P1GovernancePrinciple::monarchy,
        phasec::P1PowerDistribution::autocracy,phasec::P1LegislatureKind::none,"fixture"});
    ledger.seed_government("B","HEAD_B","PM_B",5,3);
    ledger.seed_market("SOURCE","ore",4,4,4);
    ledger.seed_market("PORT","ore",10,10,10,1);
    ledger.seed_market("SOURCE","steel",2,2,2);
    ledger.seed_stock("SELLER","SOURCE","ore",10);
    ledger.seed_stock("BUILDER","SOURCE","steel",1);
    const auto worker=people(opening,"SOURCE",1,"worker");
    ledger.seed_population().hire(worker,1,"BUILDER","worker",0,{});
    opening.seed_verified_geographic_link("fixture-link",1,2,"rail");
    phasegh::TransportEdge edge;
    edge.id="FREIGHT";edge.carrier="CARRIER";edge.mode="rail";
    edge.from=1;edge.to=2;edge.monthly_capacity=7;edge.freight_per_unit=2;
    edge.transit_days=2;edge.evidence_id="fixture-rail";
    edge.valid_from_month=23999;edge.valid_through_month=24012;
    edge.capacity_site="TERMINAL";
    opening.seed_transport_edge(edge);
    p0::Policies policy;
    policy.daily_food_units_per_person=0;
    policy.merchant_offices.push_back({"TRADER","PORT",1,0,0});
    phased::ConstructionProject p;
    p.id="TERMINAL-WORKS";p.client="A";p.contractor="BUILDER";p.region="SOURCE";
    p.material_good="steel";p.target_site="TERMINAL";p.work_required=1;
    p.material_per_work=1;p.workers_per_work=1;p.cash_price_per_work=20;p.target_capacity=24;
    policy.public_construction_offers.push_back({p,{},40,false,"infrastructure","FREIGHT"});
    policy.public_construction_priorities["A"]={"infrastructure"};
    policy.public_construction_daily_caps["A"]=20;
    const auto fills=[](const p0::DailyGame& game)->const auto& {return game.world().engine().commerce().fills;};
    p0::DailyGame work(daily::World::adopt(std::move(opening),81),policy);
    const auto initial=daily::SnapshotCodec::encode(work.world());
    auto paused=policy;
    paused.public_construction_daily_caps["A"]=0;
    p0::DailyGame control(daily::SnapshotCodec::decode(initial),paused);
    auto mismatch=policy;
    mismatch.public_construction_offers.front().freight_edge="not-a-route";
    p0::DailyGame invalid(daily::SnapshotCodec::decode(initial),mismatch);
    invalid.advance_day();
    require_test(invalid.world().engine().base().core().state().constructions.empty(),
                 "an unmatched freight offer must not start or block day advance");
    const auto before_rejection=daily::SnapshotCodec::encode(invalid.world());
    bool rejected=false;
    try {invalid.order_public_construction("A","PM",p.id);}
    catch(const std::invalid_argument&) {rejected=true;}
    require_test(rejected&&daily::SnapshotCodec::encode(invalid.world())==before_rejection,
                 "manual public order cannot bypass the same freight binding");
    work.advance_day();
    work.advance_day();control.advance_day();control.advance_day();
    require_test(fills(work).empty()&&fills(control).empty()&&!c(work).facilities().contains("TERMINAL")&&
                 c(work).cash("A")==180,"unfinished terminal cannot increase capacity before commissioning");
    p0::DailyGame replay(daily::SnapshotCodec::decode(daily::SnapshotCodec::encode(work.world())),policy);
    work.advance_day();replay.advance_day();control.advance_day();
    require_test(c(work).facilities().contains("TERMINAL")&&
                 work.world().engine().base().core().state().recipes.empty()&&
                 fills(work).size()==1&&fills(control).empty()&&
                 fills(work).front().quantity==1&&c(work).cash("SELLER")==4&&c(work).cash("CARRIER")==2&&
                 c(work).stock("TRADER","PORT","ore")==0,
                 "commissioning must enable an earlier real freight booking, payment and transit");
    work.advance_day();replay.advance_day();work.advance_day();replay.advance_day();
    require_test(c(work).stock("TRADER","PORT","ore")==1&&fills(work).size()==1&&
                 daily::SnapshotCodec::encode(work.world())==daily::SnapshotCodec::encode(replay.world()),
                 "site-supported freight must physically arrive once, with deterministic save replay");
}

static void producer_import_counts_real_on_site_and_inbound_stock_once() {
    p0::Policies options;
    options.daily_food_units_per_person=0;
    options.monthly_clothing_units_per_person=0;
    options.allow_endogenous_investment_credit=false;
    options.allow_collateralized_enterprise_credit=false;
    auto first_order=options;
    first_order.allow_endogenous_input_procurement=false;
    first_order.merchant_offices.push_back({"TRADER","PORT",1,0,0});
    phasegh::World opening_world(24000);
    auto& geo=opening_world.seed_geography();
    geo.seed_province(1,"SOURCE","A");geo.seed_unit(1,"A","DISTRICT_A");
    geo.seed_province(2,"PORT","B");geo.seed_unit(2,"B","DISTRICT_B");
    geo.seed_border(1,2);geo.seed_assignment(1,1);geo.seed_assignment(2,2);
    auto& industry=geo.seed_core();
    auto& c=industry.seed_core();
    for(const auto* actor:{"A","B","SELLER","TRADER","CARRIER"})
        c.seed_actor(actor,std::string_view(actor)=="TRADER"?1000:0);
    c.seed_government("A","HEAD_A","PM_A",5,3);
    c.seed_government("B","HEAD_B","PM_B",5,3);
    c.seed_market("SOURCE","ore",4,4,4,0);
    // Day one can fill one unit of genuine merchant demand. The
    // production order must cover its entire uncovered batch on day two.
    c.seed_market("PORT","ore",9,9,9,2);
    c.seed_market("PORT","fuel",6,1,100,0);
    c.seed_market("PORT","tools",35,1,100,2);
    c.seed_stock("SELLER","SOURCE","ore",20);
    c.seed_stock("TRADER","PORT","fuel",2);
    c.seed_stock("TRADER","PORT","ore",1);
    opening_world.seed_verified_geographic_link("verified-link",1,2,"rail");
    phasegh::TransportEdge edge;
    edge.id="documented-freight";edge.carrier="CARRIER";edge.mode="rail";
    edge.from=1;edge.to=2;edge.monthly_capacity=62;edge.freight_per_unit=2;
    edge.transit_days=2;edge.evidence_id="real-rail-rate";
    edge.valid_from_month=23999;edge.valid_through_month=24012;
    opening_world.seed_transport_edge(edge);
    c.seed_production_facility("WORKS","TRADER","PORT",31);
    phased::IndustryRecipe recipe;
    recipe.id="MAKE_TOOLS";recipe.site="WORKS";recipe.inputs={{"ore",4},{"fuel",2}};
    recipe.outputs={{"tools",2}};recipe.workers_per_batch=recipe.capacity_per_batch=1;
    industry.seed_recipe(recipe);
    const auto worker=people(opening_world,"PORT",1,"worker");
    c.seed_actor("HH:"+std::to_string(c.people().slice(worker).key.household),0);
    c.seed_population().hire(worker,1,"TRADER","worker",93,{});
    p0::DailyGame initial(daily::World::adopt(std::move(opening_world),6),first_order);
    initial.advance_day();
    const auto& opening=initial.world().engine().commerce();
    require_test(opening.fills.size()==1&&opening.fills[0].quantity==1&&
           opening.daily_shipments.size()==1,
           "fixture must start the second day with precisely one physical unit inbound");
    const auto first_saved=daily::SnapshotCodec::encode(initial.world());
    p0::DailyGame game(daily::SnapshotCodec::decode(first_saved),options);
    p0::DailyGame replay(daily::SnapshotCodec::decode(first_saved),options);
    const auto core=[&]() -> const phasec::IntegratedWorld& {
        return game.world().engine().base().core().core();
    };
    const auto& commerce=game.world().engine().commerce();
    game.advance_day();replay.advance_day();
    require_test(commerce.fills.size()==2&&commerce.fills.back().quantity==2&&
           commerce.daily_shipments.size()==2&&
           core().stock("TRADER","PORT","ore")==1&&core().stock("TRADER","PORT","tools")==0,
           "four required minus one live stock and one inbound must dispatch exactly two");
    game.advance_day();replay.advance_day();
    require_test(core().stock("TRADER","PORT","ore")==2&&core().stock("TRADER","PORT","tools")==0&&
           commerce.fills.size()==2,
           "an incomplete shipment set was consumed or substituted by a duplicate order");
    game.advance_day();replay.advance_day();
    require_test(core().stock("TRADER","PORT","ore")==0&&core().stock("TRADER","PORT","tools")==2&&
           commerce.fills.size()==2&&core().stock("SELLER","SOURCE","ore")==17&&
           daily::SnapshotCodec::encode(game.world())==daily::SnapshotCodec::encode(replay.world()),
           "only delivered input must feed the real batch, with exact snapshot replay");
}

static void expired_trade_leg_uses_alternate_that_can_carry_full_lot() {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    for(const auto& [id,region,country]:std::vector<std::tuple<int,std::string,std::string>>{
            {1,"SOURCE","A"},{2,"HUB","A"},{3,"PORT","B"}}) {
        geo.seed_province(id,region,country);
        geo.seed_unit(id,country,"DISTRICT-"+region);
        geo.seed_assignment(id,id);
    }
    geo.seed_border(1,2);geo.seed_border(2,3);
    auto& seed=geo.seed_core().seed_core();
    for(const auto* actor:{"A","B","SELLER","TRADER","FIRST","EXPIRED","SMALL","FULL"})
        seed.seed_actor(actor,std::string_view(actor)=="TRADER"?100:0);
    seed.seed_government("A","HEAD_A","PM_A",5,3);
    seed.seed_government("B","HEAD_B","PM_B",5,3);
    seed.seed_market("SOURCE","ore",10,10,10,0);
    seed.seed_market("PORT","ore",30,30,30,2);
    seed.seed_stock("SELLER","SOURCE","ore",2);
    gh.seed_verified_geographic_link("source-hub",1,2,"rail");
    gh.seed_verified_geographic_link("hub-port",2,3,"rail");
    const auto edge=[&](const char* id,const char* carrier,int from,int to,
                        int capacity,int rate,int start,int end) {
        gh.seed_transport_edge({id,carrier,static_cast<phaseb::Id>(from),
            static_cast<phaseb::Id>(to),capacity,rate,0,true,1,"rail",
            "fixture:dated-service",start,end});
    };
    edge("FIRST","FIRST",1,2,62,1,24000,24001);
    edge("EXPIRED","EXPIRED",2,3,62,1,24000,24000);
    edge("SMALL","SMALL",2,3,29,2,24001,24001);
    edge("FULL","FULL",2,3,58,3,24001,24001);
    auto world=daily::World::adopt(std::move(gh),57);
    for(int i=0;i<30;++i)world.advance_day(); // Jan 31: the last operating day.
    p0::Policies policies;
    policies.merchant_offices.push_back({"TRADER","PORT",2,0,50'000});
    p0::DailyGame game(std::move(world),policies);
    game.advance_day();
    const auto& trade=game.world().engine().commerce();
    require_test(trade.fills.size()==1&&trade.daily_shipments.size()==1&&
                 trade.fills.front().quantity==2&&
                 trade.fills.front().physical_path==
                     std::vector<std::string>{"FIRST","EXPIRED"}&&
                 c(game).cash("SELLER")==20&&c(game).cash("FIRST")==2&&
                 c(game).cash("TRADER")==78&&c(game).stock("TRADER","PORT","ore")==0,
                 "fixture must dispatch and pay for both units on the January booked route");
    const auto saved=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame replay(daily::SnapshotCodec::decode(saved),policies);
    game.advance_day();replay.advance_day(); // Feb 1: expired next leg, full alternate fits exactly.
    require_test(trade.daily_shipments.size()==1&&!trade.daily_shipments.begin()->second.at_hub&&
                 trade.daily_shipments.begin()->second.path_edges==
                     std::vector<std::string>{"FIRST","FULL"}&&
                 c(game).cash("FULL")==6&&c(game).cash("SMALL")==0&&
                 c(game).cash("EXPIRED")==0&&c(game).cash("TRADER")==72&&
                 c(game).stock("TRADER","PORT","ore")==0,
                 "expired booked leg must bypass the cheaper too-small alternate and dispatch the full lot");
    game.advance_day();replay.advance_day(); // Feb 2: now and only now buyer can use the units.
    require_test(trade.daily_shipments.empty()&&trade.fills.size()==1&&
                 c(game).stock("SELLER","SOURCE","ore")==0&&
                 c(game).stock("TRADER","PORT","ore")==2&&
                 c(game).cash("SELLER")+c(game).cash("FIRST")+c(game).cash("FULL")+
                     c(game).cash("TRADER")==100&&c(game).financial().balanced()&&
                 daily::SnapshotCodec::encode(game.world())==
                     daily::SnapshotCodec::encode(replay.world()),
                 "alternate cargo must arrive once without a duplicate purchase or freight debit");
}

static void autonomous_f_two_hop_freight_and_save_replay() {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"A-HUB","A");
    geo.seed_province(2,"B-HUB","B");
    geo.seed_province(3,"C-HUB","C");
    geo.seed_border(1,2);geo.seed_border(2,3);
    geo.seed_unit(11,"A","UA");geo.seed_unit(22,"B","UB");
    geo.seed_unit(33,"C","UC");
    geo.seed_assignment(1,11);geo.seed_assignment(2,22);
    geo.seed_assignment(3,33);
    auto& initial_core=geo.seed_core().seed_core();
    initial_core.seed_actor("SELLER",100);initial_core.seed_actor("BUYER",100);
    initial_core.seed_actor("CARRIER",100);
    initial_core.seed_stock("SELLER","A-HUB","iron",3);
    gh.seed_transport_edge({"HOP-A-B","CARRIER",1,2,310,1,1,true});
    gh.seed_transport_edge({"HOP-B-C","CARRIER",2,3,310,1,1,true});
    phasef::FreightLane lane;
    lane.id="IRON";lane.seller="SELLER";lane.buyer="BUYER";
    lane.good="iron";lane.origin_region="A-HUB";
    lane.destination_region="C-HUB";
    lane.target_inventory=2;lane.maximum_daily_units=2;
    lane.edge_transit_days={{"HOP-A-B",1},{"HOP-B-C",1}};
    p0::Policies options;
    options.freight_lanes.push_back(lane);
    p0::DailyGame game(daily::World::adopt(std::move(gh),55),options);
    game.advance_day(); // Jan 1: autonomous A->B dispatch, actual freight
    require_test(game.world().cargo().size()==1,
                 "autonomous F cargo must be tracked under exact dispatch ID");
    const auto first=game.world().cargo().begin()->second;
    require_test(first.origin=="A-HUB"&&first.destination=="B-HUB" &&
                 c(game).stock("SELLER","A-HUB","iron")==1&&
                 c(game).cash("SELLER")==98 &&
                 c(game).cash("CARRIER")==102,
                 "first F hop must move real stock and pay actual freight");
    game.advance_day(); // Jan 2: arrival at intermediate hub
    require_test(c(game).stock("BUYER","B-HUB","iron")==2 &&
                 game.world().cargo().begin()->second.arrived,
                 "F did not deliver to actual intermediate owner");
    const auto checkpoint=daily::SnapshotCodec::encode(game.world());
    auto replay=p0::DailyGame(daily::SnapshotCodec::decode(checkpoint),options);
    game.advance_day();replay.advance_day(); // Jan 3: autonomous B->C
    require_test(game.world().cargo().size()==2 &&
                 c(game).stock("BUYER","B-HUB","iron")==0 &&
                 c(game).cash("BUYER")==98 &&
                 c(game).cash("CARRIER")==104,
                 "second real carrier must be charged exactly once");
    game.advance_day();replay.advance_day(); // Jan 4: final delivery
    require_test(daily::SnapshotCodec::encode(game.world())==
                 daily::SnapshotCodec::encode(replay.world())&&
                 c(game).stock("BUYER","C-HUB","iron")==2,
                 "multi-hop F forwarding/replay failed or stock duplicated");
    game.advance_day(); // Jan 5: target met, no extra movement
    require_test(game.world().cargo().size()==2 &&
                 c(game).stock("SELLER","A-HUB","iron")==1,
                 "fulfilled autonomous F lane should stop shipping");

    phasegh::World missing(24000);
    auto& eg=missing.seed_geography();
    eg.seed_province(1,"R1","A");eg.seed_province(2,"R2","B");
    eg.seed_border(1,2);
    eg.seed_unit(11,"A","UA");eg.seed_unit(22,"B","UB");
    eg.seed_assignment(1,11);eg.seed_assignment(2,22);
    auto& cc=eg.seed_core().seed_core();
    cc.seed_actor("S",100);cc.seed_actor("B",100);cc.seed_actor("CARRIER",100);
    cc.seed_stock("S","R1","iron",3);
    missing.seed_transport_edge({"NO-DURATION","CARRIER",1,2,310,1,1,true});
    phasef::FreightLane bad_lane;
    bad_lane.id="BAD";bad_lane.seller="S";bad_lane.buyer="B";
    bad_lane.good="iron";bad_lane.origin_region="R1";
    bad_lane.destination_region="R2";bad_lane.target_inventory=1;
    bad_lane.maximum_daily_units=1;bad_lane.edge_transit_days={{"UNRELATED",1}};
    p0::Policies policy;policy.freight_lanes.push_back(bad_lane);
    p0::DailyGame invalid(daily::World::adopt(std::move(missing)),policy);
    const auto before=daily::SnapshotCodec::encode(invalid.world());
    bool refused=false;
    try{invalid.advance_day();}catch(const std::exception&){refused=true;}
    if(refused) {
        require_test(daily::SnapshotCodec::encode(invalid.world())==before,
                     "rejected freight command must preserve the entire day");
    } else {
        // A missing verified route is also allowed to yield no transport
        // command. That is not a fabricated shipment or a financial action.
        const auto& result=::c(invalid);
        require_test(invalid.world().cargo().empty() &&
                     result.stock("S","R1","iron")==3 &&
                     result.cash("S")==100 &&
                     result.cash("B")==100 &&
                     result.cash("CARRIER")==100,
                     "missing grounded transit days must not fabricate route or post money");
    }
}


static void autonomous_daily_facility_maintenance_with_real_inputs() {
    auto w=fixture();
    auto& seeded=w.seed_geography().seed_core().seed_core();
    seeded.seed_stock("FIRM","R","steel",3);
    auto& persons=seeded.seed_population();
    const auto labor_slice=persons.slices().begin()->first;
    persons.hire(labor_slice,4,"FIRM","technician",10,
                 phaseb::JobRequirements{});
    auto world=daily::World::adopt(std::move(w),99);
    phasegh::World::Plan damage;
    damage[10].push_back([](phasegh::Stage& stage) {
        stage.phase_c().war_damage_facility("FARM",3);
    });
    world.advance_day(damage); // first day causes real physical damage
    require_test(world.engine().base().core().core().facility("FARM").damaged==3,
                 "unfunded maintenance fixture failed to create damage");
    p0::Policies policy;
    policy.maintenance.push_back({"FARM","steel",2,1,1,1,
                                  "WORKERS","TREASURY"});
    p0::DailyGame game(std::move(world),policy);
    const auto before=daily::SnapshotCodec::encode(game.world());
    phasegh::World::Plan fail;
    fail[9].push_back([](phasegh::Stage&){
        throw std::runtime_error("injected later stage failure");
    });
    bool aborted=false;
    try{game.advance_day(fail);}catch(const std::runtime_error&){aborted=true;}
    require_test(aborted &&
                 daily::SnapshotCodec::encode(game.world())==before,
                 "late failure must roll back materials, wages, repaired capacity and day");

    game.advance_day(); // day 2: two real restoration units
    require_test(c(game).facility("FARM").damaged==1 &&
                 c(game).stock("FIRM","R","steel")==1 &&
                 c(game).cash("FIRM")==1998 &&
                 c(game).cash("WORKERS")==2,
                 "D day-native maintenance must restore capacity and use real C ledger");
    const auto checkpoint=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame replay(daily::SnapshotCodec::decode(checkpoint),policy);
    game.advance_day();replay.advance_day(); // day 3, remaining one unit
    require_test(c(game).facility("FARM").damaged==0 &&
                 c(game).stock("FIRM","R","steel")==0 &&
                 c(game).cash("FIRM")==1997 &&
                 c(game).cash("WORKERS")==3 &&
                 daily::SnapshotCodec::encode(game.world())==
                 daily::SnapshotCodec::encode(replay.world()),
                 "D maintenance replay/remaining capacity accounting diverged");
    game.advance_day();replay.advance_day();
    require_test(c(game).facility("FARM").damaged==0 &&
                 c(game).cash("FIRM")==1997 &&
                 daily::SnapshotCodec::encode(game.world())==
                 daily::SnapshotCodec::encode(replay.world()),
                 "D no-damage day must never book phantom labor, stock or wages");

    p0::Policies duplicate=policy;
    duplicate.maintenance.push_back(policy.maintenance.front());
    bool refused=false;
    try {p0::DailyGame illegal(daily::SnapshotCodec::decode(checkpoint),duplicate);}
    catch(const std::invalid_argument&){refused=true;}
    require_test(refused,"duplicate owner-site maintenance policy must be rejected");
}


static daily::World seed_lawful_autonomous_treaty_and_council() {
    auto gh=real_h_crisis_and_organization_fixture();
    auto world=daily::World::adopt(std::move(gh),912);
    phasegh::World::Plan setup;
    setup[9].push_back([](phasegh::Stage& stage){
        phasegh::TreatyClause clause;
        clause.id="FISCAL-AID";clause.kind=phasegh::ClauseKind::financial_aid;
        clause.obligor="B";clause.beneficiary="A";clause.amount=20;
        stage.propose_treaty_package("PACT-A-B",{"A","B"},
                                    {clause},"A","PM_A");
        phasegh::OrganizationRule rule;
        rule.quorum=2;rule.yes_required=1;
        rule.veto_members.insert("B");
        stage.configure_organization("LEAGUE",rule,"PM_A");
    });
    world.advance_day(setup); // January 1: lawful C/H proposal, no AI fiat
    require_test(world.engine().diplomacy().organizations.at("LEAGUE").active,
                 "real H organization must be active before votes");
    return world;
}

static void autonomous_h_treaty_risk_and_institutional_vote() {
    p0::Policies policy;
    policy.treaty_ratifications.push_back({"B","PM_B",20,false,false,false});
    policy.council_decisions.push_back(
        {"LEAGUE","COUNCIL-APPROVE","research:cooperation",
         "A","PM_A",{{"A","PM_A",50},{"B","PM_B",50}}});
    policy.council_decisions.push_back(
        {"LEAGUE","COUNCIL-VETO","trade:embargo",
         "A","PM_A",{{"A","PM_A",50},{"B","PM_B",200}}});
    auto world=seed_lawful_autonomous_treaty_and_council();
    p0::DailyGame game(std::move(world),policy);
    const auto beforeA=c(game).cash("A"),beforeB=c(game).cash("B");
    const auto beforeOrganization=c(game).cash("ORG:LEAGUE");
    game.advance_day(); // January 2: lawful country AI decisions
    const auto& h=game.world().engine().diplomacy();
    require_test(h.treaties.at("PACT-A-B").ratifications.contains("B"),
                 "H country AI did not ratify authorized treaty");
    require_test(!h.treaties.at("PACT-A-B").active,
                 "H ratification applied treaty before effective month");
    require_test(h.organizations.at("LEAGUE").decisions.at("COUNCIL-APPROVE").adopted,
                 "H organization with a lawful quorum and yes votes should adopt");
    require_test(!h.organizations.at("LEAGUE").decisions.at("COUNCIL-VETO").adopted,
                 "H organization veto member rejected but vote was adopted");
    // Ordinary active organization membership is a pre-existing, legally
    // enforceable monthly obligation: the native P0 code pays real dues even
    // without an explicit manual mandate. Voting and treaty ratification
    // create no cash, while funded membership debits must be fully balanced.
    const auto& council=h.organizations.at("LEAGUE");
    const auto dues=council.dues_per_member;
    require_test(dues>0 &&
                 council.paid.at({24000,"A"})==dues &&
                 council.paid.at({24000,"B"})==dues &&
                 c(game).cash("A")==beforeA-dues &&
                 c(game).cash("B")==beforeB-dues &&
                 c(game).cash("ORG:LEAGUE")==beforeOrganization+2*dues &&
                 c(game).cash("A")+c(game).cash("B")+
                 c(game).cash("ORG:LEAGUE")==
                 beforeA+beforeB+beforeOrganization,
                 "H autonomous dues must transfer actual cash once, no money creation");

    const auto checkpoint=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame replay(daily::SnapshotCodec::decode(checkpoint),policy);
    game.advance_day();replay.advance_day();
    require_test(daily::SnapshotCodec::encode(game.world())==
                 daily::SnapshotCodec::encode(replay.world()) &&
                 game.world().engine().diplomacy().
                    organizations.at("LEAGUE").decisions.size()==2,
                 "H dated AI treaties/council decisions must be idempotent on replay");
    while(game.world().date()!=daily::Date{2000,2,1})
        game.advance_day(); // no backdated H legal effects until due month
    require_test(!game.world().engine().diplomacy().
                   treaties.at("PACT-A-B").active,
                 "H treaty must not retroactively activate during January");
    game.advance_day();
    require_test(game.world().engine().diplomacy().
                    treaties.at("PACT-A-B").active,
                 "approved H treaty must activate at actual February 1 opening");

    p0::Policies unwilling=policy;
    unwilling.treaty_ratifications[0].maximum_new_fiscal_obligation=5;
    p0::DailyGame limited(seed_lawful_autonomous_treaty_and_council(),unwilling);
    limited.advance_day();
    require_test(!limited.world().engine().diplomacy().
                   treaties.at("PACT-A-B").ratifications.contains("B"),
                 "country AI must not approve liabilities above declared fiscal mandate");

    p0::Policies impersonator=policy;
    impersonator.council_decisions[0].member_rules[1].prime_minister="INVALID";
    p0::DailyGame illegal(seed_lawful_autonomous_treaty_and_council(),impersonator);
    const auto prior=daily::SnapshotCodec::encode(illegal.world());
    bool denied=false;
    try{illegal.advance_day();}catch(const std::exception&){denied=true;}
    require_test(denied && prior==daily::SnapshotCodec::encode(illegal.world()),
                 "H forged member ballot must roll back earlier same-day treaty and all ledgers");

    p0::Policies duplicate=policy;
    duplicate.council_decisions.push_back(duplicate.council_decisions.front());
    bool rejected=false;
    try{p0::DailyGame invalid(seed_lawful_autonomous_treaty_and_council(),
                              duplicate);}
    catch(const std::invalid_argument&){rejected=true;}
    require_test(rejected,"duplicate H institutional decision must fail policy validation");
}


static void autonomous_f_parallel_lanes_respect_shared_real_resources() {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"RA","A");geo.seed_province(2,"RB","B");
    geo.seed_border(1,2);
    geo.seed_unit(11,"A","UA");geo.seed_unit(22,"B","UB");
    geo.seed_assignment(1,11);geo.seed_assignment(2,22);
    auto& core=geo.seed_core().seed_core();
    core.seed_actor("SELLER",100);core.seed_actor("B1",100);
    core.seed_actor("B2",100);core.seed_actor("CARRIER",100);
    core.seed_stock("SELLER","RA","iron",3);
    gh.seed_transport_edge({"LINK","CARRIER",1,2,310,1,1,true});
    phasef::FreightLane l1;
    l1.id="ONE";l1.seller="SELLER";l1.buyer="B1";l1.good="iron";
    l1.origin_region="RA";l1.destination_region="RB";
    l1.target_inventory=2;l1.maximum_daily_units=2;
    l1.edge_transit_days={{"LINK",1}};
    auto l2=l1;l2.id="TWO";l2.buyer="B2";
    p0::Policies rules;rules.freight_lanes={l1,l2};
    p0::DailyGame game(daily::World::adopt(std::move(gh),311),rules);
    game.advance_day();
    require_test(game.world().cargo().size()==2 &&
                 c(game).stock("SELLER","RA","iron")==0 &&
                 c(game).cash("SELLER")==97 &&
                 c(game).cash("CARRIER")==103,
                 "F overlapping cargo lanes overcommitted actual stock or cash");
    Quantity shipped1=0,shipped2=0;
    for(const auto& [id,cargo]:game.world().cargo()) {
        if(id.starts_with("F:ONE:"))shipped1=cargo.quantity;
        if(id.starts_with("F:TWO:"))shipped2=cargo.quantity;
    }
    require_test(shipped1==2&&shipped2==1,
                 "F deterministic policy order must allocate scarce common stock");
    auto checkpoint=daily::SnapshotCodec::encode(game.world());
    auto replay=p0::DailyGame(daily::SnapshotCodec::decode(checkpoint),rules);
    game.advance_day();replay.advance_day();
    require_test(daily::SnapshotCodec::encode(game.world())==
                 daily::SnapshotCodec::encode(replay.world())&&
                 c(game).stock("B1","RB","iron")==2 &&
                 c(game).stock("B2","RB","iron")==1,
                 "parallel F cargo receipts must conserve all goods across replay");
}

static void autonomous_f_congestion_uses_actual_alternate_edge() {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"RA","A");geo.seed_province(2,"RB","B");
    geo.seed_province(3,"RC","C");
    geo.seed_border(1,2);geo.seed_border(1,3);geo.seed_border(3,2);
    geo.seed_unit(11,"A","UA");geo.seed_unit(22,"B","UB");
    geo.seed_unit(33,"C","UC");
    geo.seed_assignment(1,11);geo.seed_assignment(2,22);
    geo.seed_assignment(3,33);
    auto& core=geo.seed_core().seed_core();
    core.seed_actor("S",100);core.seed_actor("B1",100);
    core.seed_actor("B2",100);core.seed_actor("CARRIER",100);
    core.seed_stock("S","RA","iron",3);
    // The direct link can move ONE unit on Jan 1; alternate path is
    // more expensive but physically available using two existing edges.
    gh.seed_transport_edge({"DIRECT","CARRIER",1,2,31,1,1,true});
    gh.seed_transport_edge({"ALT-1","CARRIER",1,3,310,2,1,true});
    gh.seed_transport_edge({"ALT-2","CARRIER",3,2,310,2,1,true});
    phasef::FreightLane a;
    a.id="FIRST";a.seller="S";a.buyer="B1";a.good="iron";
    a.origin_region="RA";a.destination_region="RB";
    a.target_inventory=1;a.maximum_daily_units=1;
    a.edge_transit_days={{"DIRECT",1},{"ALT-1",1},{"ALT-2",1}};
    auto b=a;b.id="SECOND";b.buyer="B2";
    p0::Policies rules;rules.freight_lanes={a,b};
    p0::DailyGame game(daily::World::adopt(std::move(gh)),rules);
    game.advance_day();
    const auto& G=game.world().engine().commerce();
    require_test(G.edge_usage.at({24000,"DIRECT"})==1 &&
                 G.edge_usage.at({24000,"ALT-1"})==1 &&
                 G.edge_usage.find({24000,"ALT-2"})==G.edge_usage.end() &&
                 c(game).stock("S","RA","iron")==1,
                 "F capacity-constrained route search must use free alternate first hop");
    bool direct=false,alternate=false;
    for(const auto& [id,cargo]:game.world().cargo()) {
        if(id.starts_with("F:FIRST:"))
            direct=cargo.destination=="RB"&&cargo.edge_id=="DIRECT";
        if(id.starts_with("F:SECOND:"))
            alternate=cargo.destination=="RC"&&cargo.edge_id=="ALT-1";
    }
    require_test(direct&&alternate,
                 "F congested alternatives must not teleport cargo or ignore edge availability");
}


static void autonomous_daily_bank_debt_service_and_liquidity_floor() {
    auto gh=fixture();
    auto world=daily::World::adopt(std::move(gh),2701);
    phasegh::World::Plan origination;
    origination[1].push_back([](phasegh::Stage& stage){
        stage.phase_c().lend("FIRM",30);
    });
    world.advance_day(origination); // day 1 real loan booked
    require_test(world.engine().base().core().core().
                    outstanding_loan("FIRM")==30 &&
                 world.engine().base().core().core().cash("FIRM")==2030,
                 "C day loan must create equal real bank liability/cash");
    p0::Policies policy;
    policy.debt_service.push_back({"FIRM",2015,10});
    p0::DailyGame game(std::move(world),policy);
    game.advance_day(); // day 2, principal 10
    require_test(c(game).outstanding_loan("FIRM")==20 &&
                 c(game).cash("FIRM")==2020,
                 "C autonomous debt service did not retire actual principal");
    const auto checkpoint=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame replay(daily::SnapshotCodec::decode(checkpoint),policy);
    game.advance_day();replay.advance_day(); // day 3, principal 5, cash reserve
    require_test(c(game).outstanding_loan("FIRM")==15 &&
                 c(game).cash("FIRM")==2015 &&
                 daily::SnapshotCodec::encode(game.world())==
                    daily::SnapshotCodec::encode(replay.world()),
                 "C day debt repayment must preserve reserve buffer and replay");
    game.advance_day();replay.advance_day();
    require_test(c(game).outstanding_loan("FIRM")==15 &&
                 c(game).cash("FIRM")==2015 &&
                 daily::SnapshotCodec::encode(game.world())==
                    daily::SnapshotCodec::encode(replay.world()),
                 "C daily credit decision must not invent cash to pay debt");

    p0::Policies duplicate=policy;
    duplicate.debt_service.push_back(policy.debt_service.front());
    bool invalid=false;
    try {p0::DailyGame bad(daily::SnapshotCodec::decode(checkpoint),duplicate);}
    catch(const std::invalid_argument&){invalid=true;}
    require_test(invalid,"duplicate autonomous borrower policy must be rejected");
}


static void native_cg_credit_service_keeps_trade_facility_in_sync() {
    auto gh=fixture();
    gh.seed_geography().seed_core().seed_core().seed_stock("FIRM","R","steel",10);
    phasegh::World::Plan offer;
    offer[9].push_back([](phasegh::Stage& h){
        phasegh::CreditFacility credit;
        credit.id="TRADE-CREDIT";
        credit.borrower="FIRM";credit.pledge_region="R";
        credit.pledge_good="steel";credit.pledged_goods=5;
        credit.limit=30;
        h.offer_trade_credit(credit);
    });
    gh.advance(offer); // offer lawful from next month, actual stock pledged
    phasegh::World::Plan draw;
    draw[1].push_back([](phasegh::Stage& h){
        h.draw_trade_credit("TRADE-CREDIT",30);
    });
    gh.advance(draw); // actual C bank creates 30, matching G utilization
    require_test(gh.commerce().credit.at("TRADE-CREDIT").drawn==30 &&
                 gh.base().core().core().outstanding_loan("FIRM")==30,
                 "G trade line must reflect original C loan and actual collateral");
    p0::Policies policy;
    policy.debt_service.push_back({"FIRM",2015,10});
    p0::DailyGame game(daily::World::adopt(std::move(gh)),policy);
    game.advance_day();
    require_test(c(game).outstanding_loan("FIRM")==20 &&
                 game.world().engine().commerce().credit.at("TRADE-CREDIT").drawn==20 &&
                 c(game).cash("FIRM")==2020,
                 "C automatic repayment must update actual G line utilization");
    const auto saved=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame replay(daily::SnapshotCodec::decode(saved),policy);
    game.advance_day();replay.advance_day();
    require_test(c(game).outstanding_loan("FIRM")==15 &&
                 game.world().engine().commerce().credit.at("TRADE-CREDIT").drawn==15 &&
                 daily::SnapshotCodec::encode(game.world())==
                      daily::SnapshotCodec::encode(replay.world()),
                 "G/C post-repayment facility and liability must survive checkpoint replay");
}


static void insolvent_bank_keeps_pop_deposits_and_stops_new_investment_credit() {
    auto gh=fixture();
    auto& d=gh.seed_geography().seed_core();
    auto& bank=d.seed_core();
    bank.seed_annual_bank_interest_ppm(1'000);
    bank.seed_actor("FAILED",0);
    bank.seed_actor("CREDIT_PROJECT_FIRM",0);
    bank.seed_actor("OLD_DEBT",30);
    bank.seed_stock("CREDIT_PROJECT_FIRM","R","ore",1);
    bank.seed_stock("FIRM","R","food",20);
    bank.seed_stock("BUILDER","R","steel",2);
    auto& pop=bank.seed_population();
    const auto builder=std::next(pop.slices().begin())->first;
    const auto household=std::next(pop.slices().begin(),2)->first;
    pop.hire(builder,2,"BUILDER","worker",10,{});
    auto project=d.construction("NEW_FARM");
    project.id="CREDIT_PROJECT";project.client="CREDIT_PROJECT_FIRM";
    project.target_site="CREDIT_FARM";project.work_required=1;
    project.cash_price_per_work=120;
    d.seed_construction(project);

    p0::Policies options;
    options.daily_food_units_per_person=0;
    options.monthly_clothing_units_per_person=0;
    p0::DailyGame opening(daily::World::adopt(std::move(gh),7),options);
    phasegh::World::Plan start;
    start[1].push_back([](phasegh::Stage& h) {
        h.phase_c().lend("FAILED",200);
        h.phase_c().lend("OLD_DEBT",20);
    });
    start[7].push_back([](phasegh::Stage& h) {
        auto& world=h.phase_c();
        world.pay("FAILED","HH",200); // no assets remain at the unsecured obligor
        world.trade("CREDIT_PROJECT_FIRM","A","R","ore",1,100);
    });
    const auto pre_loss=daily::SnapshotCodec::encode(opening.world());
    auto healthy=p0::DailyGame(daily::SnapshotCodec::decode(pre_loss),options);
    healthy.advance_day(start);
    start[8].push_back([](phasegh::Stage& h) {
        h.phase_c().forgive_defaulted_loan("FAILED",200);
    });
    opening.advance_day(start);
    require_test(c(opening).bank_insolvent()&&c(opening).bank_losses()==200&&
                 c(opening).outstanding_loan("FAILED")==0&&
                 c(opening).cash("HH")==1200&&c(healthy).cash("HH")==1200,
                 "recognizing a loan loss must not erase the POP deposit");
    const auto failed=daily::SnapshotCodec::encode(opening.world());
    options.investments={{"NEW_FARM","food",0,1,1},{"CREDIT_PROJECT","food",0,1,1}};
    options.buyers.push_back({household,"HH","FIRM","R","food",1});
    options.debt_service.push_back({"OLD_DEBT",10,5});
    p0::DailyGame solvent(daily::SnapshotCodec::decode(
        daily::SnapshotCodec::encode(healthy.world())),options);
    p0::DailyGame insolvent(daily::SnapshotCodec::decode(failed),options);
    p0::DailyGame replay(daily::SnapshotCodec::decode(failed),options);
    solvent.advance_day();insolvent.advance_day();replay.advance_day();
    const auto work=[](const p0::DailyGame& g,const char* id) {
        return g.world().engine().base().core().construction(id).paid_work;
    };
    require_test(c(solvent).outstanding_loan("CREDIT_PROJECT_FIRM")==20&&
                 work(solvent,"CREDIT_PROJECT")==1&&
                 c(insolvent).outstanding_loan("CREDIT_PROJECT_FIRM")==0&&
                 work(insolvent,"CREDIT_PROJECT")==0,
                 "a real eligible investment must lose new credit after bank failure");
    require_test(work(insolvent,"NEW_FARM")==1&&
                 c(insolvent).outstanding_loan("OLD_DEBT")==15&&
                 c(insolvent).cash("HH")<1200&&c(insolvent).cash("HH")>0&&
                 c(insolvent).stock("FIRM","R","food")<20,
                 "cash investment, old debt service and POP purchases must still settle");
    require_test(c(insolvent).bank_insolvent()&&
                 daily::SnapshotCodec::encode(insolvent.world())==
                 daily::SnapshotCodec::encode(replay.world()),
                 "bank failure and subsequent economic flows must replay exactly");
}

static void national_elections_are_independent_and_persistent() {
    auto build=[] {
        phasegh::World gh(24000);
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"RA","A");
        geo.seed_province(2,"RB","B");
        geo.seed_border(1,2);
        geo.seed_unit(11,"A","A-ROOT");geo.seed_unit(22,"B","B-ROOT");
        geo.seed_assignment(1,11);geo.seed_assignment(2,22);
        auto& c=geo.seed_core().seed_core();
        c.seed_actor("A",100);c.seed_actor("B",100);
        c.seed_government("A","HEAD_A","PM_A",5,3);
        c.seed_government("B","HEAD_B","PM_B",5,3);
        auto& pop=c.seed_population();
        for(const auto& [region,n]:std::array<std::pair<const char*,phaseb::People>,2>{{
              {"RA",20},{"RB",16}}}) {
            auto h=pop.household_profile({region,"mixed","shared",n});
            auto econ=pop.economic_profile({region,"worker","wage","employee",
                                            pop.default_education()});
            auto dem=pop.demographic_profile(
                {region,24000-360,phaseb::Sex::female,"culture","faith",
                 "citizen","free",pop.default_health(),phaseb::Service::civilian});
            pop.seed_population(dem,econ,h,n,region==std::string("RA")?1:2);
        }
        gh.validate();
        return gh;
    };
    p0::Policies policy;
    // This fixture isolates national voter/seat jurisdiction; no simulated
    // food market was seeded, so disable unrelated endogenous hunger shocks.
    policy.daily_food_units_per_person=0;
    policy.elections.push_back({"A","A_INC","A_REF",2000,1,1,1'000'000});
    policy.elections.push_back({"B","B_INC","B_REF",2000,1,1,1'000'000});

    auto opening=build();
    const auto old_c=phasec::SnapshotCodec::encode(
                         opening.base().core().core());
    require_test(phasec::SnapshotCodec::encode(
                   phasec::SnapshotCodec::decode(old_c))==old_c,
                 "legacy C electionless snapshot must remain byte-identical");

    p0::DailyGame a(daily::World::adopt(std::move(opening),43),policy);
    phasegh::World::Plan cabinet;
    cabinet[9].push_back([](phasegh::Stage& gh){
        gh.phase_c().appoint_cabinet("A","HEAD_A","NEXT_PM_A",{"A_INC"});
    });
    a.advance_day(cabinet);
    const auto& c=::c(a);
    const auto& ae=c.country_election("A");
    const auto& be=c.country_election("B");
    require_test(c.country_elections().size()==2&&
                 ae.eligible==20&&be.eligible==16&&
                 ae.registered==20&&be.registered==16&&
                 ae.valid==20&&be.valid==16&&
                 ae.total_seats==5&&be.total_seats==5&&
                 ae.seats.at("A_INC")==3&&be.seats.at("B_INC")==3&&
                 c.country_election_month("A")==24000&&
                 c.country_election_month("B")==24000,
                 "country voter scope, elections or national seat ledger incorrect");
    const auto saved=daily::SnapshotCodec::encode(a.world());
    auto restored=daily::SnapshotCodec::decode(saved);
    require_test(daily::SnapshotCodec::encode(restored)==saved &&
                 restored.engine().base().core().core().country_elections().size()==2,
                 "country elections must survive C/D/E/GH/day nested snapshots");

    p0::DailyGame replay(std::move(restored),policy);
    while(a.world().date()<daily::Date{2000,2,2}) {
        a.advance_day();replay.advance_day();
    }
    require_test(daily::SnapshotCodec::encode(a.world())==
                 daily::SnapshotCodec::encode(replay.world())&&
                 ::c(a).country_election("B").votes.at("B_INC")==8&&
                 ::c(a).government("A").prime_minister=="NEXT_PM_A"&&
                 ::c(a).government("B").prime_minister=="PM_B",
                 "month-effective national cabinet authority or replay diverged");

    // No other country's incumbent seats may authorize B's government.
    auto rejected=daily::World::adopt(build(),89);
    p0::DailyGame unlawful(std::move(rejected),policy);
    phasegh::World::Plan wrong;
    wrong[9].push_back([](phasegh::Stage& gh) {
        gh.phase_c().appoint_cabinet("B","HEAD_B","UNLAWFUL",{"A_INC"});
    });
    const auto before=daily::SnapshotCodec::encode(unlawful.world());
    bool failed=false;
    try{unlawful.advance_day(wrong);}catch(const std::exception&){failed=true;}
    require_test(failed && daily::SnapshotCodec::encode(unlawful.world())==before,
                 "foreign electoral majority must not seat a government/advance day");

    p0::Policies duplicate=policy;
    duplicate.elections.push_back({"A","ALT_INC","ALT_REF",2000,1,15,1'000'000});
    bool invalid=false;
    try{p0::DailyGame bad(daily::World::adopt(build()),duplicate);}
    catch(const std::invalid_argument&){invalid=true;}
    require_test(invalid,"duplicate national election during one calendar month");

    auto capped=daily::World::adopt(build());
    phasegh::World::Plan over;
    over[9].push_back([](phasegh::Stage& gh){
        gh.phase_c().hold_country_election("B",17,0,{{"B_INC",17}});
    });
    const auto original=daily::SnapshotCodec::encode(capped);
    bool overcount=false;
    try{capped.advance_day(over);}catch(const std::exception&){overcount=true;}
    require_test(overcount&&original==daily::SnapshotCodec::encode(capped),
                 "national election must reject registering citizens of another state");
}


static void p1_parliamentary_cabinet_next_day_and_save() {
    auto opening=fixture();
    auto& c=opening.seed_geography().seed_core().seed_core();
    c.seed_interest_group("LANDOWNERS");
    c.seed_p1_constitution("A",{
        phasec::P1GovernancePrinciple::parliamentary_republic,
        phasec::P1PowerDistribution::universal_vote,
        phasec::P1LegislatureKind::unicameral,"test:p1-parliament"});
    c.seed_p1_coalition("A",{{"LANDOWNERS"},{"INC"},false});
    const auto voter=c.people().slices().begin()->first;
    require_test(c.p1_authority("A").assembly_confidence_required,
                 "P1 parliamentary cabinet must require confidence");
    const auto original_population=c.people().population();
    p0::Policies policy;
    // The confidence-vote fixture has no seeded food supplier for every
    // household. Keep endogenous hunger shocks out of this legal quorum test.
    policy.daily_food_units_per_person=0;
    policy.elections.push_back({"A","INC","REF",2000,1,1,1'000'000});
    policy.government_formations.push_back({
        "A","headA","PM_NEXT","test:p1-vote",
        {"LANDOWNERS"},{"INC"},2000,1,1,false});
    p0::DailyGame game(daily::World::adopt(std::move(opening),308),policy);
    phasegh::World::Plan join;
    join[9].push_back([voter](phasegh::Stage& gh) {
        gh.phase_c().join_interest_group(voter,"LANDOWNERS",1);
    });
    game.advance_day(join);
    const auto& p=::c(game);
    require_test(p.government("A").prime_minister=="PM_A" &&
                 p.has_due_p1_government(game.world().day_index()),
                 "P1 government changed before its next-day effective date");
    const auto treasury_before_read=p.cash("A");
    const auto legitimacy=p.p1_legitimacy("A");
    require_test(legitimacy.observed_groups&&legitimacy.observed_election&&
                 legitimacy.score>=0&&legitimacy.score<=100 &&
                 p.people().population()==original_population &&
                 p.cash("A")==treasury_before_read,
                 "P1 legitimacy created people or money or lost P3/P4 evidence");
    const auto encoded=daily::SnapshotCodec::encode(game.world());
    auto saved=daily::SnapshotCodec::decode(encoded);
    require_test(daily::SnapshotCodec::encode(saved)==encoded,
                 "P1 pending cabinet lost checkpoint round-trip");
    p0::DailyGame replay(std::move(saved),policy);
    game.advance_day();
    replay.advance_day();
    require_test(::c(game).government("A").prime_minister=="PM_NEXT" &&
                 ::c(game).p1_coalition("A").ruling_parties==
                     std::vector<std::string>{"INC"} &&
                 ::c(game).people().population()==original_population &&
                 daily::SnapshotCodec::encode(game.world())==
                     daily::SnapshotCodec::encode(replay.world()),
                 "P1 next-day office change did not replay atomically");
}

static void autonomous_tax_requires_current_parliamentary_authority(bool direct_executive) {
    using namespace phasec;
    phasegh::World gh{24000};
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"RA","A");geo.seed_province(2,"RB","B");
    geo.seed_unit(11,"A","A-ROOT");geo.seed_unit(22,"B","B-ROOT");
    geo.seed_assignment(1,11);geo.seed_assignment(2,22);
    auto& core=geo.seed_core().seed_core();
    core.seed_actor("VENDOR",0);
    const auto election=daily::to_day_index({2000,1,2});
    const auto fiscal_review=daily::to_day_index({2000,2,1});
    const auto recast=daily::to_day_index({2000,2,3});
    for(const std::string country:{"A","B"}) {
        core.seed_actor(country,0);
        core.seed_actor(country+"-EMPLOYER",100);
        core.seed_actor(country+"-WORKER",0);
        core.seed_government(country,country+"-HEAD",country+"-PM",5,3);
        core.seed_p1_constitution(country,{P1GovernancePrinciple::presidential_republic,
            P1PowerDistribution::universal_vote,P1LegislatureKind::unicameral,"test:constitution"});
        core.p4_register_party({country+"-PARTY",country,country+"-HEAD",100'000,true,true});
        core.p4_seed_contest({country+"-HOUSE",country,"national","test:house",
            P4Office::legislature,P4BallotMethod::list_dhondt,P4TermMode::restart_after_poll,
            48,1'000'000,5,election,election,-1,true,true,false});
        const auto worker=people(gh,"R"+country,20,"worker");
        core.seed_population().hire(worker,20,country+"-EMPLOYER","worker",0,{});
    }
    p0::Policies policies;
    policies.autonomous_tax_step_ppm=10'000;
    policies.autonomous_tax_ceiling_ppm=200'000;
    policies.daily_food_units_per_person=0;
    policies.allow_endogenous_debt_repayment=false;
    p0::DailyGame game(daily::World::adopt(std::move(gh),92),policies);
    game.advance_day();
    game.advance_day(); // both actual elections
    require_test(c(game).country_election("A").seats.at("A-PARTY")==5&&
                 c(game).country_election("B").seats.at("B-PARTY")==5,
                 "fixture did not elect independent national assemblies");
    const auto january_third=game.world().day_index();
    phasegh::World::Plan grounds;
    grounds[1].push_back([](phasegh::Stage& stage) {
        stage.phase_c().borrow_country_budget_from_bank("A",100);
        stage.phase_c().borrow_country_budget_from_bank("B",100);
    });
    grounds[5].push_back([](phasegh::Stage& stage) {
        stage.phase_c().spend_country_budget("A","VENDOR",100);
        stage.phase_c().spend_country_budget("B","VENDOR",100);
    });
    grounds[9].push_back([=](phasegh::Stage& stage) {
        auto& auth=stage.phase_c();
        if(!direct_executive) {
            auth.p4_dissolve("B-HOUSE","B-HEAD","test:lawful dissolution",january_third,recast);
            return;
        }
        P2Bill bill;
        bill.id="B-AUTOCRACY-BILL";bill.country="B";bill.proposer="B-PM";
        bill.legal_basis="test:legislative approval";
        bill.effective_day=fiscal_review;
        bill.proposed_base={"B-AUTOCRACY","B","distribution_of_power","national",
                            "law_autocracy","test:effective constitutional law"};
        auth.p2_submit_bill(std::move(bill),january_third);
        auth.p2_approve_bill("B-AUTOCRACY-BILL",{"B-PARTY"});
        auth.p2_promulgate_bill("B-AUTOCRACY-BILL","B-HEAD",january_third);
    });
    game.advance_day(grounds);
    while(game.world().date()<daily::Date{2000,2,1})game.advance_day();
    require_test(c(game).p4_parliament_dissolved("B")==!direct_executive&&
                 !c(game).p4_parliament_dissolved("A")&&!c(game).p1_authority("B").direct_executive&&
                 c(game).country_election("B").seats.at("B-PARTY")==5&&
                 c(game).country_fiscal("A").bank_debt==100&&c(game).cash("A")==0&&
                 c(game).country_fiscal("B").bank_debt==100&&c(game).cash("B")==0&&
                 c(game).cash("VENDOR")==200,
                 "fixture must retain historical B seats and settled, separate bank debt");
    game.advance_day(); // February 1 fiscal AI
    require_test(game.world().date()==daily::Date{2000,2,2}&&
                 c(game).has_pending_country_tax("A")&&!c(game).has_pending_country_tax("B")&&
                 c(game).cash("A")==0&&c(game).cash("B")==0&&
                 c(game).p1_authority("B").direct_executive==direct_executive&&
                 c(game).country_election("B").seats.at("B-PARTY")==5,
                 "B without current parliamentary tax authority blocked A, manufactured revenue or scheduled a tax on old seats");
    const auto before=daily::SnapshotCodec::encode(game.world());
    phasegh::World::Plan rejected;
    rejected[9].push_back([](phasegh::Stage& stage) {
        stage.phase_c().propose_tax_law("B","B-PM",{"B-PARTY"},110'000);
    });
    bool rejected_seats=false;
    try{game.advance_day(rejected);}
    catch(const std::exception& e) {
        rejected_seats=std::string_view(e.what()).find(
            direct_executive?"P1 direct executive may not invent parliamentary votes":
                             "P4 dissolved parliament cannot enact a new tax law")!=std::string_view::npos;
    }
    require_test(rejected_seats&&daily::SnapshotCodec::encode(game.world())==before,
                 "manual tax enactment must still reject unusable parliamentary votes atomically");
    while(game.world().date()<daily::Date{2000,3,1})game.advance_day();
    if(!direct_executive)require_test(!c(game).p4_parliament_dissolved("B")&&
                 c(game).p4_contest("B-HOUSE").last_poll_day==recast&&
                 c(game).country_election("B").seats.at("B-PARTY")==5,
                 "early election did not restore real B parliamentary authority");
    phasegh::World::Plan payroll;
    payroll[5].push_back([](phasegh::Stage& stage) {
        stage.phase_c().wages_under_law("A-EMPLOYER","A-WORKER","A",100,"A");
        stage.phase_c().wages_under_law("B-EMPLOYER","B-WORKER","B",100,"B");
    });
    game.advance_day(payroll);
    require_test(c(game).income_tax_ppm("A")==110'000&&c(game).income_tax_ppm("B")==100'000&&
                 c(game).cash("A")==11&&c(game).cash("A-WORKER")==89&&
                 c(game).cash("B")==10&&c(game).cash("B-WORKER")==90&&
                 c(game).has_pending_country_tax("B")==!direct_executive,
                 "March actual withholding or later elected B policy is wrong");
}

static void public_construction_purchases_uses_and_pays_real_inputs() {
    const auto trial=[](int steel,phasec::Money builder_cash,phasec::Money treasury,bool elected=false,
                        phased::Quantity work_required=1){
        phasegh::World gh{24000};
        auto& geo=gh.seed_geography();
        geo.seed_province(1,"RA","A");geo.seed_unit(1,"A","RA");geo.seed_assignment(1,1);
        auto& d=geo.seed_core();
        auto& c=d.seed_core();
        c.seed_actor("A",treasury);c.seed_actor("BUILDER",builder_cash);
        c.seed_actor("SELLER",0);c.seed_actor("PROVIDER",0);
        c.seed_government("A","HEAD","PM",5,3);
        if(elected) {
            using namespace phasec;
            c.seed_p1_constitution("A",{P1GovernancePrinciple::presidential_republic,
                P1PowerDistribution::universal_vote,P1LegislatureKind::unicameral,"fixture"});
            c.p4_register_party({"PEOPLES","A","HEAD",100'000,true,true});
            const auto date=daily::to_day_index({2000,1,2});
            c.p4_seed_contest({"HOUSE","A","national","fixture:house",
                P4Office::legislature,P4BallotMethod::list_dhondt,P4TermMode::restart_after_poll,
                48,1'000'000,5,date,date,-1,true,true,false});
        } else c.seed_p1_constitution("A",{phasec::P1GovernancePrinciple::monarchy,
            phasec::P1PowerDistribution::autocracy,phasec::P1LegislatureKind::none,"fixture"});
        c.seed_market("RA","steel",3,3,3);
        c.seed_market("RA","tools",5,5,5);
        c.seed_market("RA","wood",1,1,1);
        if(steel>0)c.seed_stock("SELLER","RA","steel",steel);
        const auto worker=people(gh,"RA",1,"worker");
        c.seed_population().hire(worker,1,"BUILDER","worker",0,{});
        phased::ConstructionProject p;
        p.id="PUBLIC-TOOLS-A";p.client="A";p.contractor="BUILDER";p.region="RA";
        p.material_good="steel";p.target_site="SITE";p.work_required=work_required;
        p.material_per_work=2;p.workers_per_work=1;p.cash_price_per_work=20;p.target_capacity=31;
        phased::IndustryRecipe r;
        r.id="METHOD";r.site="SITE";r.inputs={{"wood",1}};r.outputs={{"tools",1}};
        r.workers_per_batch=r.capacity_per_batch=1;
        p0::Policies policy;
        policy.daily_food_units_per_person=0;
        policy.public_budget.push_back({"A","PROVIDER",15,0});
        policy.public_construction_offers.push_back({p,r,40,elected});
        return p0::DailyGame(daily::World::adopt(std::move(gh),81),policy);
    };
    const auto reject=[](p0::DailyGame& game,const std::string& who){
        const auto before=daily::SnapshotCodec::encode(game.world());
        bool denied=false;
        try{game.order_public_construction("A",who,"PUBLIC-TOOLS-A");}catch(const std::exception&){denied=true;}
        require_test(denied&&daily::SnapshotCodec::encode(game.world())==before,
                     "invalid public work must restore the opening world");
    };
    auto elected=trial(2,6,200,true);
    const auto rollback=[](const auto& game,const auto& action) {
        const auto bytes=daily::SnapshotCodec::encode(game.world());
        bool denied=false;
        try{action();}catch(const std::exception&){denied=true;}
        require_test(denied&&daily::SnapshotCodec::encode(game.world())==bytes,
                     "invalid elected appropriation or order mutated the world");
    };
    const std::string id="PUBLIC-TOOLS-A";
    const auto approve=[&](const std::string& pm,const std::vector<std::string>& parties) {
        elected.approve_public_construction_budget("A",pm,id,parties);
    };
    rollback(elected,[&]{approve("PM",{"PEOPLES"});});
    elected.advance_day();elected.advance_day();
    require_test(c(elected).country_election("A").seats.at("PEOPLES")==5,
                 "public construction must use actual elected seats");
    reject(elected,"PM");
    rollback(elected,[&]{
        phasegh::World::Plan forged;
        forged[9].push_back([](phasegh::Stage& gh) {
            gh.phase_c().command_once("P0:ELECTED-PUBLIC-BUILD:approved:PUBLIC-TOOLS-A",
                [](auto&){});
        });
        elected.advance_day(forged);
    });
    rollback(elected,[&]{approve("HEAD",{"PEOPLES"});});
    rollback(elected,[&]{approve("PM",{});});
    rollback(elected,[&]{approve("PM",{"PEOPLES","PEOPLES"});});
    rollback(elected,[&]{approve("PM",{"OTHER"});});
    approve("PM",{"PEOPLES"});
    require_test(elected.has_public_construction_budget("A",id)&&
                 !elected.world().engine().base().core().state().constructions.contains(id)&&
                 c(elected).cash("BUILDER")==6,
                 "budget approval must commit authority, not spend or order work");
    rollback(elected,[&]{approve("PM",{"PEOPLES"});});
    const auto approved=daily::SnapshotCodec::encode(elected.world());
    auto changed=elected.policies();
    ++changed.public_construction_offers.front().project.cash_price_per_work;
    p0::DailyGame changed_offer(daily::SnapshotCodec::decode(approved),changed);
    require_test(!changed_offer.has_public_construction_budget("A",id),
                 "restored changed price inherited an unrelated appropriation");
    reject(changed_offer,"PM");
    rollback(changed_offer,[&]{
        changed_offer.approve_public_construction_budget("A","PM",id,{"PEOPLES"});
    });
    p0::DailyGame elected_replay(daily::SnapshotCodec::decode(approved),elected.policies());
    require_test(elected_replay.has_public_construction_budget("A",id),
                 "appropriation not recognized after restore");
    elected.order_public_construction("A","PM",id);
    elected_replay.order_public_construction("A","PM",id);
    reject(elected,"PM");
    const auto ordered=daily::SnapshotCodec::encode(elected.world());
    changed=elected.policies();
    changed.public_construction_offers.front().elected_appropriation=false;
    p0::DailyGame downgrade(daily::SnapshotCodec::decode(ordered),changed);
    downgrade.advance_day();
    require_test(c(downgrade).cash("SELLER")==0&&
                 c(downgrade).stock("SELLER","RA","steel")==2,
                 "restored elected project bypassed its appropriation");
    for(int day=0;day<3;++day){elected.advance_day();elected_replay.advance_day();}
    require_test(c(elected).cash("BUILDER")==20&&c(elected).cash("SELLER")==6&&
                 c(elected).facilities().contains("SITE")&&
                 daily::SnapshotCodec::encode(elected.world())==
                     daily::SnapshotCodec::encode(elected_replay.world()),
                 "approved ordered work did not buy, pay, commission and replay");
    auto game=trial(2,6,75);
    const auto domain=[](const p0::DailyGame& g)->const phased::World& {
        return g.world().engine().base().core();
    };
    game.order_public_construction("A","PM","PUBLIC-TOOLS-A");
    require_test(domain(game).state().constructions.contains("PUBLIC-TOOLS-A")&&
                 !c(game).facilities().contains("SITE")&&c(game).stock("BUILDER","RA","steel")==0&&
                 c(game).cash("A")==60&&c(game).cash("PROVIDER")==15,
                 "lawful Stage9 did not create only an ordinary persisted funded public queue");
    reject(game,"PM");
    auto wrong=trial(2,6,75);reject(wrong,"HEAD");
    auto poor=trial(2,5,75);reject(poor,"PM");
    auto missing=trial(1,6,75);reject(missing,"PM");
    auto unfunded=trial(2,6,74);reject(unfunded,"PM");
    const auto saved=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame replay(daily::SnapshotCodec::decode(saved),game.policies());
    game.advance_day();replay.advance_day();
    require_test(c(game).stock("BUILDER","RA","steel")==2&&
                 c(game).stock("SELLER","RA","steel")==0&&c(game).cash("SELLER")==6&&
                 c(game).cash("A")==60&&c(game).cash("PROVIDER")==15&&
                 domain(game).construction("PUBLIC-TOOLS-A").paid_work==0,
                 "real Stage7 local purchase or funded invoice reservation was lost");
    const auto funded_checkpoint=daily::SnapshotCodec::encode(game.world());
    auto untrusted=game.policies();
    untrusted.public_construction_offers.clear();
    untrusted.public_budget.clear();
    untrusted.investments.push_back({"PUBLIC-TOOLS-A","tools",0,1,1});
    p0::DailyGame stopped(daily::SnapshotCodec::decode(funded_checkpoint),untrusted);
    stopped.advance_day();
    require_test(domain(stopped).construction("PUBLIC-TOOLS-A").paid_work==0&&
                 c(stopped).cash("A")==60&&c(stopped).stock("BUILDER","RA","steel")==2,
                 "restoring without the sanctioned offer must not reclassify public work as private");
    game.advance_day();replay.advance_day();
    require_test(c(game).stock("BUILDER","RA","steel")==0&&c(game).cash("BUILDER")==20&&
                 c(game).cash("A")==40&&c(game).cash("PROVIDER")==15&&
                 domain(game).construction("PUBLIC-TOOLS-A").paid_work==1&&
                 domain(game).construction("PUBLIC-TOOLS-A").capitalized_value==20&&
                 !c(game).facilities().contains("SITE"),
                 "one paid physical work must capitalize before commissioning and preserve treasury reserve");
    game.advance_day();replay.advance_day();
    require_test(c(game).facilities().contains("SITE")&&c(game).facility("SITE").owner=="A"&&
                 domain(game).construction("PUBLIC-TOOLS-A").commissioned&&c(game).financial().balanced()&&
                 daily::SnapshotCodec::encode(game.world())==daily::SnapshotCodec::encode(replay.world()),
                 "committed construction failed native cash conservation, commissioning, or save replay");

    // Isolate construction from the unrelated civilian service envelope.
    auto funded=trial(4,12,200,false,2);
    auto capped=funded.policies();
    capped.public_budget.clear();
    capped.public_construction_daily_caps["A"]=19;
    p0::DailyGame limited(daily::SnapshotCodec::decode(
        daily::SnapshotCodec::encode(funded.world())),capped);
    limited.order_public_construction("A","PM","PUBLIC-TOOLS-A");
    const auto queued=daily::SnapshotCodec::encode(limited.world());
    limited.advance_day();limited.advance_day();
    require_test(limited.world().day_index()==daily::SnapshotCodec::decode(queued).day_index()+2&&
                 c(limited).cash("A")==200&&c(limited).cash("SELLER")==0&&
                 domain(limited).construction("PUBLIC-TOOLS-A").paid_work==0,
                 "an invoice above today's cap must not stop the date, debit cash or procure inputs");
    capped.public_construction_daily_caps["A"]=20;
    p0::DailyGame running(daily::SnapshotCodec::decode(
        daily::SnapshotCodec::encode(limited.world())),capped);
    running.advance_day();running.advance_day();
    require_test(c(running).cash("A")==180&&
                 domain(running).construction("PUBLIC-TOOLS-A").paid_work==1&&
                 !c(running).facilities().contains("SITE"),
                 "matching cap should cover the first real invoice, not the full project");
    const auto mid=daily::SnapshotCodec::encode(running.world());
    auto zero=capped;
    zero.public_construction_daily_caps["A"]=0;
    p0::DailyGame paused(daily::SnapshotCodec::decode(mid),zero);
    paused.advance_day();
    require_test(c(paused).cash("A")==180&&
                 domain(paused).construction("PUBLIC-TOOLS-A").paid_work==1,
                 "zero cap must pause an active public order without losing paid progress");
    p0::DailyGame capped_replay(daily::SnapshotCodec::decode(mid),capped);
    running.advance_day();capped_replay.advance_day();
    require_test(c(running).cash("A")==160&&
                 domain(running).construction("PUBLIC-TOOLS-A").paid_work==2,
                 "daily cap must reset on each new day");
    running.advance_day();capped_replay.advance_day();
    require_test(c(running).facilities().contains("SITE")&&c(running).financial().balanced()&&
                 daily::SnapshotCodec::encode(running.world())==
                     daily::SnapshotCodec::encode(capped_replay.world()),
                 "capped construction must conserve cash, commission, and replay from mid-project");

    auto auto_policy=funded.policies();
    auto_policy.public_budget.clear();
    auto_policy.public_construction_daily_caps["A"]=20;
    auto_policy.public_construction_offers.front().priority_category="infrastructure";
    auto second=auto_policy.public_construction_offers.front();
    second.project.id="PUBLIC-SECOND";
    second.project.target_site="SITE2";
    second.recipe.site="SITE2";
    second.recipe.id="METHOD2";
    second.priority_category="services";
    auto_policy.public_construction_offers.push_back(second);
    const auto initial=daily::SnapshotCodec::encode(funded.world());
    const auto choose=[&](const auto& policy) {
        p0::DailyGame result(daily::SnapshotCodec::decode(initial),policy);
        result.advance_day();
        return result;
    };
    auto_policy.public_construction_priorities["A"]={"services","infrastructure"};
    auto services=choose(auto_policy);
    require_test(domain(services).state().constructions.contains("PUBLIC-SECOND")&&
                 !domain(services).state().constructions.contains("PUBLIC-TOOLS-A")&&
                 c(services).cash("A")==200,
                 "public automation must choose the preferred eligible category, not spend on ordering");
    auto replay_services=choose(auto_policy);
    services.advance_day();replay_services.advance_day();
    require_test(daily::SnapshotCodec::encode(services.world())==
                     daily::SnapshotCodec::encode(replay_services.world())&&
                 !domain(services).state().constructions.contains("PUBLIC-TOOLS-A"),
                 "a running order must occupy the single treasury slot deterministically");
    reject(services,"PM"); // Competing manual order cannot bypass the shared slot.
    auto_policy.public_construction_priorities["A"]={"infrastructure","services"};
    auto infrastructure=choose(auto_policy);
    require_test(domain(infrastructure).state().constructions.contains("PUBLIC-TOOLS-A")&&
                 !domain(infrastructure).state().constructions.contains("PUBLIC-SECOND"),
                 "reversing broad priorities must choose a different viable project");
    auto_policy.public_construction_offers.front().project.material_good="unavailable-material";
    auto fallback=choose(auto_policy);
    require_test(domain(fallback).state().constructions.contains("PUBLIC-SECOND")&&
                 fallback.world().day_index()==funded.world().day_index()+1,
                 "an ineligible preferred project must not stop the simulation or block a viable fallback");
    auto_policy.public_construction_daily_caps["A"]=0;
    auto no_order=choose(auto_policy);
    require_test(domain(no_order).state().constructions.empty()&&c(no_order).cash("A")==200,
                 "zero budget must not automatically commit a project");

}

static void selected_players_own_tax_without_disabling_rival_ai() {
    using namespace phasec;
    phasegh::World gh{24000};
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"RA","A");geo.seed_province(2,"RB","B");
    geo.seed_unit(11,"A","A-ROOT");geo.seed_unit(22,"B","B-ROOT");
    geo.seed_assignment(1,11);geo.seed_assignment(2,22);
    auto& core=geo.seed_core().seed_core();
    core.seed_actor("VENDOR",0);
    const auto election=daily::to_day_index({2000,1,2});
    for(const std::string country:{"A","B"}) {
        core.seed_actor(country,0);
        core.seed_actor(country+"-EMPLOYER",100);
        core.seed_actor(country+"-WORKER",0);
        core.seed_government(country,country+"-HEAD",country+"-PM",5,3);
        core.seed_p1_constitution(country,{P1GovernancePrinciple::presidential_republic,
            P1PowerDistribution::universal_vote,P1LegislatureKind::unicameral,"test:constitution"});
        core.p4_register_party({country+"-PARTY",country,country+"-HEAD",100'000,true,true});
        core.p4_seed_contest({country+"-HOUSE",country,"national","test:house",
            P4Office::legislature,P4BallotMethod::list_dhondt,P4TermMode::restart_after_poll,
            48,1'000'000,5,election,election,-1,true,true,false});
        core.p2_seed_base({country+"-TAX-BOUND",country,"tax_fixture","national",
            "statutory-limits","test:tax statute",P2NormTier::statute,
            {{"income-tax-cap",country,P2ClauseKind::income_tax_max_ppm,140'000}}});
        (void)people(gh,"R"+country,20,"worker");
    }
    p0::Policies policies;
    policies.autonomous_tax_step_ppm=10'000;
    policies.autonomous_tax_ceiling_ppm=200'000;
    policies.daily_food_units_per_person=0;
    policies.allow_endogenous_debt_repayment=false;
    p0::DailyGame opening(daily::World::adopt(std::move(gh),92),policies);
    opening.advance_day();
    opening.advance_day();
    require_test(c(opening).country_election("A").seats.at("A-PARTY")==5&&
                 c(opening).country_election("B").seats.at("B-PARTY")==5,
                 "tax priorities require independent real parliamentary majorities");
    phasegh::World::Plan grounds;
    grounds[1].push_back([](phasegh::Stage& stage) {
        stage.phase_c().borrow_country_budget_from_bank("A",100);
        stage.phase_c().borrow_country_budget_from_bank("B",100);
    });
    grounds[5].push_back([](phasegh::Stage& stage) {
        stage.phase_c().spend_country_budget("A","VENDOR",100);
        stage.phase_c().spend_country_budget("B","VENDOR",100);
    });
    opening.advance_day(grounds);
    while(opening.world().date()<daily::Date{2000,2,1})opening.advance_day();
    require_test(c(opening).country_fiscal("A").bank_debt==100&&c(opening).cash("A")==0&&
                 c(opening).country_fiscal("B").bank_debt==100&&c(opening).cash("B")==0&&
                 c(opening).cash("VENDOR")==200&&!c(opening).has_pending_country_tax("A")&&
                 !c(opening).has_pending_country_tax("B"),
                 "both governments must have the same actual taxable-policy trigger");

    // First controlled day: no player mark has yet been committed to Phase C.
    auto selected=policies;
    selected.player_controlled_countries.insert("A");
    p0::DailyGame game(opening.world(),selected);
    const auto before=daily::SnapshotCodec::encode(game.world());
    require_test(!c(game).p1_is_player_country("A")&&!c(game).p1_is_player_country("B"),
                 "first player command must precede any committed control mark");
    const auto order=[](int rate,const std::string& proposer) {
        phasegh::World::Plan commands;
        commands[9].push_back([=](phasegh::Stage& stage) {
            stage.phase_c().propose_tax_law("A",proposer,{"A-PARTY"},rate);
        });
        return commands;
    };
    const auto check_rejected=[&](int rate,const std::string& proposer,
                                  std::string_view expected) {
        bool invalid=false;
        try {game.advance_day(order(rate,proposer));}
        catch(const std::exception& e) {
            invalid=std::string_view(e.what()).find(expected)!=std::string_view::npos;
        }
        require_test(invalid&&daily::SnapshotCodec::encode(game.world())==before&&
                     !c(game).has_pending_country_tax("B"),
                     "an unlawful player tax changed the date, world, or rival pending rate");
    };
    check_rejected(150'000,"A-PM","outside binding legal rate limits");
    check_rejected(130'000,"A-HEAD","unauthorized or duplicate national income-tax bill");
    game.advance_day(order(130'000,"A-PM"));
    const auto march=c(game).month()+1;
    require_test(c(game).pending_country_tax("A")==std::pair{march,130'000}&&
                 c(game).pending_country_tax("B")==std::pair{march,110'000}&&
                 c(game).income_tax_ppm("A")==100'000&&c(game).income_tax_ppm("B")==100'000,
                 "lawful player and rival proposals must both wait for the new month");

    // The created Phase C player mark, not the outer Policies object,
    // must retain the selection after loading a committed checkpoint.
    const auto saved=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame replay(daily::SnapshotCodec::decode(saved),policies);
    while(game.world().date()<daily::Date{2000,3,1}) {
        game.advance_day(); replay.advance_day();
    }
    require_test(c(replay).p1_is_player_country("A")&&!c(replay).p1_is_player_country("B")&&
                 c(replay).month()==march&&c(replay).income_tax_ppm("A")==100'000&&
                 c(replay).pending_country_tax("A")==std::pair{march,130'000}&&
                 c(replay).pending_country_tax("B")==std::pair{march,110'000},
                 "unprocessed new-month morning must show the actual pending rates");
    phasegh::World::Plan payroll;
    payroll[5].push_back([](phasegh::Stage& stage) {
        stage.phase_c().wages_under_law("A-EMPLOYER","A-WORKER","A",100,"A");
        stage.phase_c().wages_under_law("B-EMPLOYER","B-WORKER","B",100,"B");
    });
    game.advance_day(payroll); replay.advance_day(payroll);
    require_test(c(game).income_tax_ppm("A")==130'000&&c(game).income_tax_ppm("B")==110'000&&
                 c(game).cash("A")==13&&c(game).cash("A-WORKER")==87&&
                 c(game).cash("B")==11&&c(game).cash("B-WORKER")==89&&
                 c(game).cash("VENDOR")==200&&c(game).outstanding_loan("A")==100&&
                 c(game).outstanding_loan("B")==100&&!c(game).pending_country_tax("A")&&
                 c(game).pending_country_tax("B")==std::pair{march+1,120'000}&&
                 daily::SnapshotCodec::encode(game.world())==
                   daily::SnapshotCodec::encode(replay.world()),
                 "the saved player/rival selection must preserve real rates, receipts, and replay");
}

static void p1_autocracy_uses_real_institution_and_rejects_false_votes() {
    auto opening=fixture();
    auto& c=opening.seed_geography().seed_core().seed_core();
    c.seed_p1_constitution("A",{
        phasec::P1GovernancePrinciple::monarchy,
        phasec::P1PowerDistribution::autocracy,
        phasec::P1LegislatureKind::none,"test:p1-monarchy"});
    const auto worker=c.people().slices().begin()->first;
    c.seed_population().hire(worker,1,"FIRM","worker",5,{});
    c.seed_public_institution("P1_ADMIN","FIRM","R","food",30,4);
    c.seed_p1_institution("A","P1_ADMIN",
                          phasec::P1InstitutionKind::administration,5);
    const auto administration=c.p1_institution_capacity("P1_ADMIN");
    require_test(administration.employed_staff==1&&
                 administration.legal_slots==30&&
                 administration.staffed_slots==4&&
                 administration.usable_slots==4 &&
                 c.p1_authority("A").head_can_appoint &&
                 c.government("A").chamber_seats==0 &&
                 c.government("A").quorum==0,
                 "P1 institution used synthetic staff or executive authority");
    p0::Policies policy;
    policy.government_formations.push_back({
        "A","headA","PM_APPOINTED","test:p1-royal-appointment",
        {},{},2000,1,1,false});
    p0::DailyGame game(daily::World::adopt(std::move(opening),309),policy);
    game.advance_day();
    game.advance_day();
    require_test(::c(game).government("A").prime_minister=="PM_APPOINTED",
                 "P1 head-appointed government failed without fictitious election");
    phasegh::World::Plan decree;
    decree[9].push_back([](phasegh::Stage& gh) {
        gh.phase_c().propose_tax_law("A","PM_APPOINTED",{},200'000);
    });
    game.advance_day(decree);
    require_test(::c(game).has_pending_country_tax("A"),
                 "P1 direct executive created fake parliament or rejected legal tax");

    auto false_election=fixture();
    auto& state=false_election.seed_geography().seed_core().seed_core();
    state.seed_p1_constitution("A",{
        phasec::P1GovernancePrinciple::monarchy,
        phasec::P1PowerDistribution::autocracy,
        phasec::P1LegislatureKind::none,"test:p1-no-parliament"});
    p0::Policies wrong;
    wrong.elections.push_back({"A","INC","REF",2000,1,1,1'000'000});
    p0::DailyGame rejected(daily::World::adopt(std::move(false_election),310),wrong);
    const auto before=daily::SnapshotCodec::encode(rejected.world());
    bool denied=false;
    try{rejected.advance_day();}catch(const std::exception&){denied=true;}
    require_test(denied&&before==daily::SnapshotCodec::encode(rejected.world()),
                 "P1 no-legislature regime fabricated electoral authority");
}

static void peacetime_military_actual_resources_reserves_and_replay() {
    phasegh::World gh(24000);
    auto& e=gh.seed_geography();
    e.seed_province(1,"R","A");
    e.seed_unit(11,"A","A-ROOT");
    e.seed_assignment(1,11);
    auto& core=e.seed_core().seed_core();
    core.seed_actor("A",1000);
    core.seed_actor("SELLER",0);
    core.seed_actor("INSTRUCTOR",0);
    core.seed_government("A","HEAD_A","PM_A",5,3);
    core.seed_unit("LINE");
    core.seed_army_deployment("LINE","A","R");
    core.seed_market("R","musket",3,1,10);
    core.seed_market("R","ammunition",2,1,10);
    core.seed_market("R","spare_parts",2,1,10);
    core.seed_stock("SELLER","R","musket",8);
    core.seed_stock("SELLER","R","ammunition",40);
    core.seed_stock("SELLER","R","spare_parts",5);
    const auto serving=people(gh,"R",4,"soldier",phaseb::Service::active);
    (void)people(gh,"R",2,"reserve",phaseb::Service::reserve);
    (void)people(gh,"R",4,"worker",phaseb::Service::civilian);
    core.seed_active("LINE",serving);
    phasec::PeacetimeMilitaryProgram program;
    program.unit="LINE";
    program.country="A";
    program.equipment_good="musket";
    program.service_provider="INSTRUCTOR";
    program.doctrine=phasec::MilitaryDoctrine::defensive;
    program.standing_target=5;
    program.regional_reserve_target=2;
    program.daily_enlist_limit=2;
    program.equipment_target=6;
    program.ammunition_target=20;
    program.spare_parts_target=2;
    program.daily_procurement_limit=40;
    program.daily_drill_limit=4;
    program.daily_repair_limit=1;
    program.daily_budget=200;
    program.drill_fee=1;
    program.repair_fee=2;
    core.set_peacetime_military_program(program);
    const auto opening_population=core.people().population();
    p0::DailyGame game(daily::World::adopt(std::move(gh),71));
    game.advance_day();
    const auto& state=c(game);
    require_test(state.active_strength("LINE")==5&&
                 state.military_regional_reserves("A","R")==2&&
                 state.people().population()==opening_population,
                 "military AI duplicated or failed to transfer real POP");
    require_test(state.assigned_equipment("LINE","R","musket")==6&&
                 state.stock("LINE","R","musket")==6&&
                 state.stock("LINE","R","ammunition")==16,
                 "peacetime programme did not physically buy, issue and drill");
    require_test(state.cash("A")<1000&&state.cash("SELLER")>0&&
                 state.cash("INSTRUCTOR")>0&&
                 state.peacetime_military_state("LINE").proficiency_ppm==20'000,
                 "military real treasury payment or training evidence missing");
    require_test(state.military_readiness("LINE","R").equipped_supported==5,
                 "military readiness ignores real period-appropriate arms");
    auto snapshot=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame restored(daily::SnapshotCodec::decode(snapshot));
    for(int i=0;i<3;++i) {
        game.advance_day();
        restored.advance_day();
    }
    require_test(daily::SnapshotCodec::encode(game.world())==
                 daily::SnapshotCodec::encode(restored.world()),
                 "peacetime doctrine/stock/training state lost across save");
}

static void peacetime_military_unfunded_and_nonland_are_bounded() {
    phasegh::World gh(24000);
    auto& e=gh.seed_geography();
    e.seed_province(1,"R","A");
    e.seed_unit(11,"A","A-ROOT");
    e.seed_assignment(1,11);
    auto& core=e.seed_core().seed_core();
    core.seed_actor("A",0);
    core.seed_actor("SELLER",0);
    core.seed_actor("INSTRUCTOR",0);
    core.seed_government("A","HEAD_A","PM_A",5,3);
    core.seed_unit("NAVAL");
    core.seed_army_deployment("NAVAL","A","R");
    core.seed_market("R","warship",5,1,10);
    core.seed_stock("SELLER","R","warship",2);
    const auto crew=people(gh,"R",2,"sailor",phaseb::Service::active);
    core.seed_active("NAVAL",crew);
    phasec::PeacetimeMilitaryProgram program;
    program.unit="NAVAL";
    program.country="A";
    program.branch=phasec::MilitaryServiceBranch::navy;
    program.equipment_good="warship";
    program.service_provider="INSTRUCTOR";
    program.standing_target=2;
    program.equipment_target=2;
    program.daily_procurement_limit=2;
    program.daily_drill_limit=1;
    program.drill_fee=1;
    program.daily_budget=100;
    core.set_peacetime_military_program(program);
    p0::DailyGame game(daily::World::adopt(std::move(gh),72));
    game.advance_day();
    require_test(c(game).stock("SELLER","R","warship")==2&&
                 c(game).stock("NAVAL","R","warship")==0&&
                 c(game).peacetime_military_state("NAVAL").proficiency_ppm==0&&
                 c(game).cash("A")==0,
                 "unfunded military policy invented money, warships or drills");
    require_test(c(game).military_service_branch("NAVAL")==
                     phasec::MilitaryServiceBranch::navy&&
                 !c(game).military_readiness("NAVAL","R").combat_capable(),
                 "naval formation incorrectly entered the land battle engine");
}

// P2/P3 integration: claims, war, fronts, fleet and aviation all operate
// through the same actual C/E/H day engine; no separate synthetic war ledger.
enum class CivilianSeaTrade { none, preset, discovered, factory };
enum class SupplySea { none, blocked, open };
static p0::DailyGame autonomous_war_fixture(
        bool combined_arms,CivilianSeaTrade commerce=CivilianSeaTrade::none,
        SupplySea supply=SupplySea::none) {
    phasegh::World gh(24000); // Gregorian 2000-01-01
    auto& e=gh.seed_geography();
    e.seed_province(1,"RA","A");
    e.seed_province(2,"RB","B");
    if(combined_arms)e.seed_province(3,"RC","B");
    e.seed_border(1,2);
    e.seed_unit(11,"A","A-ROOT");
    e.seed_unit(22,"B","B-ROOT");
    e.seed_assignment(1,11);
    e.seed_assignment(2,22);
    if(combined_arms) {
        e.seed_assignment(3,22);
        e.seed_coast(1,7);
        e.seed_coast(2,7);
        e.seed_coast(3,7);
        e.seed_geographic_link("SEA-DIRECT",2,1,"sea");
        e.seed_geographic_link("SEA-LANDING",1,3,"sea");
    }
    if(supply!=SupplySea::none) {
        e.seed_province(4,"DEPOT-B","B");e.seed_assignment(4,22);
        e.seed_province(5,"REAR-B","B");e.seed_assignment(5,22);
        e.seed_coast(4,7);e.seed_coast(5,7);
        e.seed_geographic_link("REAR-SEA",4,5,"sea");
    }
    auto& core=e.seed_core().seed_core();
    core.seed_actor("A",1000);
    core.seed_actor("B",1000);
    core.seed_actor("CARRIER",0);
    core.seed_government("A","HEAD_A","PM_A",5,3);
    core.seed_government("B","HEAD_B","PM_B",5,3);
    core.seed_unit("LAND_A");
    core.seed_unit("LAND_B");
    core.seed_army_deployment("LAND_A","A","RA");
    core.seed_army_deployment("LAND_B","B","RB");
    core.seed_stock("LAND_A","RA","ammunition",12);
    core.seed_stock("LAND_B","RB","ammunition",12);
    core.seed_active("LAND_A",
        people(gh,"RA",12,"soldier",phaseb::Service::active));
    core.seed_active("LAND_B",
        people(gh,"RB",12,"soldier",phaseb::Service::active));
    if(supply!=SupplySea::none) {
        core.seed_unit("REAR_UNIT_B");
        core.seed_army_deployment("REAR_UNIT_B","B","REAR-B");
        core.seed_active("REAR_UNIT_B",people(gh,"REAR-B",3,"soldier",phaseb::Service::active));
        core.seed_stock("B","DEPOT-B","ammunition",12);
        core.seed_stock("A","RA","fuel",1);
        phasegh::TransportEdge freight;
        freight.id="REAR-SEA";freight.carrier="CARRIER";freight.from=4;freight.to=5;
        freight.mode="sea";freight.monthly_capacity=100;freight.freight_per_unit=2;
        freight.transit_days=1;freight.evidence_id="fixture:abstract-rear-sea";
        freight.valid_from_month=24000;freight.valid_through_month=24010;
        gh.seed_transport_edge(freight);
    }
    if(combined_arms) {
        for(const auto& [unit,country,region]:std::vector<
             std::tuple<std::string,std::string,std::string>>{
             {"NAVY_A","A","RA"},{"NAVY_B","B","RB"},
             {"AIR_A","A","RA"},{"AIR_B","B","RB"}}) {
            core.seed_unit(unit);
            core.seed_army_deployment(unit,country,region);
            const bool navy=unit.starts_with("NAVY");
            const phaseb::People crew=navy?4:2;
            core.seed_active(unit,people(gh,region,crew,
                navy?"sailor":"pilot",phaseb::Service::active));
            phasec::PeacetimeMilitaryProgram policy;
            policy.unit=unit;
            policy.country=country;
            policy.branch=navy?
                phasec::MilitaryServiceBranch::navy:
                phasec::MilitaryServiceBranch::air;
            policy.equipment_good=navy?"warship":"aircraft";
            policy.standing_target=crew;
            core.set_peacetime_military_program(policy);
            core.seed_stock(unit,region,"ammunition",navy?
                (country=="A"?8:2):6);
            if(supply!=SupplySea::open||unit!="NAVY_A")
                core.seed_stock(unit,region,"fuel",navy?5:6);
            if(navy)
                core.p1_register_ship(phasec::P1NavalHull{
                    "HULL_"+country,unit,"test-warship","fixture",
                    100,20,20,crew,true});
            else core.seed_stock(unit,region,"aircraft",2);
        }
        core.seed_stock("NAVY_A","RA","landing_berth",12);
        phasegh::TransportEdge sea;
        sea.id="SEA_FROM_B";sea.carrier="CARRIER";
        sea.from=2;sea.to=1;
        sea.monthly_capacity=100;
        if(commerce==CivilianSeaTrade::factory)sea.freight_per_unit=2;
        sea.transit_days=1;
        sea.mode="sea";
        sea.evidence_id="fixture-observed-direct-sea";
        sea.valid_from_month=24000;
        sea.valid_through_month=24010;
        gh.seed_transport_edge(sea);
        phasegh::TransportEdge lift=sea;
        lift.id="SEA_LANDING";
        lift.from=1;lift.to=3;
        lift.evidence_id="fixture-verified-amphibious-lift";
        gh.seed_transport_edge(lift);
    }
    if(commerce!=CivilianSeaTrade::none) {
        core.seed_actor("CIVILIAN-SUPPLIER",0);
        core.seed_actor("CIVILIAN-IMPORTER",10'000);
        core.seed_market("RB","ore",10,10,10);
        core.seed_market("RA","ore",30,30,30,
                         commerce==CivilianSeaTrade::discovered?350:0);
        core.seed_stock("CIVILIAN-SUPPLIER","RB","ore",400);
        if(commerce==CivilianSeaTrade::preset||commerce==CivilianSeaTrade::factory)
            gh.seed_route({"CIVILIAN-ORE-ROUTE","CIVILIAN-SUPPLIER",
                "CIVILIAN-IMPORTER","ore",2,1,100,1});
    }
    p0::Policies config;
    if(commerce==CivilianSeaTrade::factory) {
        auto& industry=e.seed_core();
        constexpr auto importer="CIVILIAN-IMPORTER";
        core.seed_market("RA","food",18,18,18);
        core.seed_stock(importer,"RA","ore",3);
        core.seed_production_facility("CIVILIAN-MILL",importer,"RA",124);
        phased::IndustryRecipe milling;
        milling.id="CIVILIAN-MILLING";milling.site="CIVILIAN-MILL";
        milling.inputs={{"ore",1}};milling.outputs={{"food",3}};
        milling.workers_per_batch=milling.capacity_per_batch=1;
        industry.seed_recipe(milling);
        const auto worker=people(gh,"RA",4,"worker");
        auto& pop=core.seed_population();
        const auto wages="HH:"+std::to_string(pop.slice(worker).key.household);
        core.seed_actor(wages,10'000);
        pop.hire(worker,4,importer,"worker",0,{});
        // Existing work-bound payroll, not cancellation of ordinary salaries.
        config.manufacturers.push_back({milling.id,8,wages,"A",20});
        config.daily_food_units_per_person=0;
        config.buyers.push_back({worker,wages,importer,"RA","food",3});
        config.monthly_clothing_units_per_person=0;
        config.allow_endogenous_investment_credit=false;
    }
    if(commerce==CivilianSeaTrade::discovered)
        config.merchant_offices.push_back({"CIVILIAN-IMPORTER","RA",4,0,50'000});
    config.allow_autonomous_army_resupply=false;
    config.allow_autonomous_peacetime_military_ai=false;
    config.allow_autonomous_war_peace=false;
    p0::DailyGame game(daily::World::adopt(std::move(gh),309),config);
    phasegh::World::Plan claim;
    claim[9].push_back([](phasegh::Stage& s) {
        s.geography().queue_claim("P2:CLAIM",2,"A",true);
    });
    if(combined_arms) {
        claim[9].push_back([](phasegh::Stage& s){
            s.geography().queue_claim("P3:LANDING-CLAIM",3,"A",true);
        });
        claim[7].push_back([](phasegh::Stage& s) {
            s.phase_c().assign_equipment("AIR_A","RA","aircraft",2);
            s.phase_c().assign_equipment("AIR_B","RB","aircraft",2);
            s.phase_c().assign_equipment("NAVY_A","RA","landing_berth",12);
        });
    }
    game.advance_day(claim);
    while(game.world().date()<daily::Date{2000,2,1})
        game.advance_day();
    phasegh::World::Plan crisis;
    crisis[9].push_back([](phasegh::Stage& s) {
        s.open_crisis("P2:CRISIS",2,"A","B","PM_A");
    });
    game.advance_day(crisis);
    while(game.world().date()<daily::Date{2000,3,1})
        game.advance_day();
    game.advance_day(); // legal crisis starts a war on its effective day
    require_test(game.world().engine().diplomacy().wars.contains(
                     "WAR:P2:CRISIS"),
                 "armed adjacent crisis missed its lawful first effective day");
    return game;
}
static void automatic_frontline_uses_adjacent_real_troops() {
    auto game=autonomous_war_fixture(false);
    require_test(game.world().engine().diplomacy().wars.contains(
                    "WAR:P2:CRISIS"),
                 "adjacent real armies did not initiate legal war");
    require_test(c(game).army_deployments().at("LAND_A").region=="RB",
                 "autonomous attacker failed to cross a real enemy front");
    require_test(c(game).army_deployments().at("LAND_B").region=="RB",
                 "defender should hold its real garrison, not swap positions");
    require_test(c(game).stock("LAND_A","RB","ammunition")<12 &&
                 c(game).stock("LAND_B","RB","ammunition")<12,
                 "automatic multi-army front never consumed real ammunition");
    require_test(c(game).sovereignty("RB")=="B",
                 "combat improperly created immediate sovereign annexation");
}
static void automatic_peace_repatriates_and_pays_actual_debt() {
    auto completed=autonomous_war_fixture(false);
    p0::Policies opts;
    opts.allow_autonomous_army_resupply=false;
    opts.allow_autonomous_peacetime_military_ai=false;
    opts.allow_autonomous_naval_air_ai=false;
    opts.allow_autonomous_war_peace=true;
    p0::DailyGame game(
        daily::SnapshotCodec::decode(
            daily::SnapshotCodec::encode(completed.world())),opts);
    for(int i=0;i<210&&
        game.world().engine().diplomacy().wars.at(
            "WAR:P2:CRISIS").ended_day<0;++i)
        game.advance_day();
    const auto& w=game.world().engine().diplomacy().wars.at(
        "WAR:P2:CRISIS");
    require_test(w.ended_day>=0&&!w.active,
                 "actual inactivity did not negotiate an effective peace");
    require_test(c(game).army_deployments().at("LAND_A").region=="RA" &&
                 c(game).sovereignty("RB")=="B",
                 "peace did not repatriate troops or forged annexation");
    const phasec::Money before=c(game).cash("B");
    phasegh::World::Plan reparations;
    reparations[9].push_back([](phasegh::Stage& gh) {
        gh.link_reparation_treaty("P3:REAL-DEBT","B","A",100,"PM_B");
    });
    game.advance_day(reparations);
    for(int i=0;i<45;++i)game.advance_day();
    require_test(c(game).reparations_outstanding("P3:REAL-DEBT")<100 &&
                 c(game).cash("B")<before &&
                 c(game).cash("A")>1000,
                 "recognized reparations did not settle from actual treasury");
}
static void automatic_naval_air_and_real_blockade() {
    auto game=autonomous_war_fixture(true);
    require_test(c(game).army_deployments().at("LAND_A").region=="RC"&&
                 c(game).sovereignty("RC")=="B"&&
                 c(game).stock("LAND_A","RC","ammunition")>0,
                 "verified sea lane and real landing berths did not carry an army");
    const auto hull_a=c(game).commissioned_naval_hulls().at("HULL_A").current_hp;
    const auto air_ammo=c(game).stock("AIR_A","RA","ammunition");
    // Both services fight immediately after the lawful Stage9 declaration.
    require_test(hull_a<20&&
                 c(game).commissioned_naval_hulls().at("HULL_B").current_hp<20,
                 "opposed real fleets did not exchange physical hull damage");
    require_test(air_ammo<6&&c(game).stock("AIR_B","RB","fuel")<6,
                 "real aircraft failed to consume fuel and ordnance");
    game.advance_day();
    // The defender expended its documented ammunition on declaration day.
    // It must not reengage using fictional gunfire next day.
    require_test(c(game).commissioned_naval_hulls().at("HULL_A").current_hp==
                    hull_a,
                 "depleted fleet inflicted unsupported subsequent ship damage");
    bool blockaded=false;
    phasegh::World::Plan next;
    next[1].push_back([&blockaded](phasegh::Stage& s) {
        blockaded=s.naval_blockaded_edge(
            s.commerce().transport_edges.at("SEA_FROM_B"));
    });
    game.advance_day(next);
    require_test(blockaded,
                 "naval superiority did not restrict a documented sea route");
}

static void real_naval_blockade_stops_new_commerce_and_armistice_restores_it(
        CivilianSeaTrade kind) {
    auto game=autonomous_war_fixture(true,kind);
    p0::Policies quiet=game.policies();
    quiet.allow_autonomous_naval_air_ai=false;
    game=p0::DailyGame(game.world(),quiet);
    const auto flows=[](const p0::DailyGame& g) {
        return g.world().engine().commerce().fills.size();
    };
    const auto paid=[](const p0::DailyGame& g) {
        return c(g).cash("CIVILIAN-SUPPLIER");
    };
    const auto source_stock=[](const p0::DailyGame& g) {
        return c(g).stock("CIVILIAN-SUPPLIER","RB","ore");
    };
    const auto delivered=[](const p0::DailyGame& g) {
        return c(g).stock("CIVILIAN-IMPORTER","RA","ore");
    };
    constexpr auto war="WAR:P2:CRISIS";
    const auto& trade=game.world().engine().commerce();
    const auto opening_flows=flows(game);
    require_test(opening_flows>10&&
                 (kind==CivilianSeaTrade::preset?
                  trade.fills.back().route_id=="CIVILIAN-ORE-ROUTE":
                  (trade.routes.empty()&&
                   trade.fills.back().route_id.starts_with("G:DISCOVERY:")&&
                   trade.fills.back().physical_path==std::vector<std::string>{"SEA_FROM_B"})),
                 "fixture must show the requested actual commercial path");
    const auto opening_pay=paid(game),opening_cash=c(game).cash("CIVILIAN-IMPORTER");
    const auto opening_stock=source_stock(game),opening_delivery=delivered(game);
    require_test(opening_flows>10&&opening_pay>0&&
                 opening_stock<400&&opening_delivery>0&&
                 !game.world().engine().commerce().daily_shipments.empty()&&
                 game.world().engine().diplomacy().wars.at(war).active,
                 "fixture must establish actual peacetime trade and paid ore already underway");
    bool blockaded=false;
    phasegh::World::Plan probe;
    probe[1].push_back([&](phasegh::Stage& stage) {
        blockaded=stage.naval_blockaded_edge(
            stage.commerce().transport_edges.at("SEA_FROM_B"));
    });
    game.advance_day(probe);
    require_test(blockaded&&delivered(game)>opening_delivery,
                 "real fleet blockade must not confiscate previously departed civilian ore");
    if(flows(game)!=opening_flows||paid(game)!=opening_pay||
       c(game).cash("CIVILIAN-IMPORTER")!=opening_cash||
       source_stock(game)!=opening_stock)
        throw std::runtime_error("blockaded ordinary route made a new commercial departure: fills "+
            std::to_string(opening_flows)+"->"+std::to_string(flows(game))+
            ", supplier paid "+std::to_string(opening_pay)+"->"+std::to_string(paid(game))+
            ", source inventory "+std::to_string(opening_stock)+"->"+std::to_string(source_stock(game)));

    auto still_at_war=game;
    phasegh::World::Plan consent;
    consent[9].push_back([&](phasegh::Stage& stage) {
        phasegh::TreatyClause term;
        term.id="civilian-armistice";term.kind=phasegh::ClauseKind::ceasefire;
        term.obligor="B";term.beneficiary="A";term.war_id=war;
        stage.propose_treaty_package("CIVILIAN-TRUCE",{"A","B"},{term},"A","PM_A");
        stage.ratify_package_member("CIVILIAN-TRUCE","B","PM_B");
    });
    game.advance_day(consent);still_at_war.advance_day();
    while(game.world().date()<daily::Date{2000,4,1}) {
        game.advance_day();still_at_war.advance_day();
    }
    require_test(flows(game)==opening_flows&&flows(still_at_war)==opening_flows&&
                 !game.world().engine().diplomacy().wars.at(war).armistice,
                 "ratification must not remove a military blockade before its effective month");
    game.advance_day();still_at_war.advance_day();
    require_test(game.world().engine().diplomacy().wars.at(war).armistice&&
                 still_at_war.world().engine().diplomacy().wars.at(war).active&&
                 flows(game)>opening_flows&&paid(game)>opening_pay&&
                 source_stock(game)<opening_stock&&flows(still_at_war)==opening_flows,
                 "only the effective actual armistice should restore new paid commercial departures");
    const auto control_delivery=delivered(still_at_war);
    game.advance_day();still_at_war.advance_day();
    require_test(delivered(game)>control_delivery&&delivered(still_at_war)==control_delivery,
                 "restored trade must physically deliver rather than only create a quote");
}


static void effective_armistice_restores_actual_factory_supply_and_paid_work() {
    constexpr auto importer="CIVILIAN-IMPORTER",supplier="CIVILIAN-SUPPLIER";
    constexpr auto war="WAR:P2:CRISIS";
    auto warm=autonomous_war_fixture(true,CivilianSeaTrade::factory);
    auto policy=warm.policies();
    policy.allow_autonomous_naval_air_ai=false;
    p0::DailyGame game(warm.world(),policy);
    const auto payroll=policy.manufacturers.front().employee_wallet;
    struct View {
        Quantity ore{},world_ore{},batches{},food{};
        phasec::Money vendor{},carrier{},worker{},sales{},treasury{},cash_total{};
        std::size_t fills{},in_flight{};
    };
    const auto observe=[&](const p0::DailyGame& play) {
        const auto& engine=play.world().engine();
        const auto& live=c(play);
        View v;
        v.ore=live.stock(importer,"RA","ore");
        v.food=live.stock(importer,"RA","food");
        for(const auto& [key,quantity]:live.physical_inventory())
            if(key.good_id=="ore")v.world_ore+=quantity;
        for(const auto& work:engine.base().core().state().actual_production_runs)
            if(work.recipe=="CIVILIAN-MILLING")v.batches+=work.batches;
        v.vendor=live.cash(supplier);v.carrier=live.cash("CARRIER");
        v.worker=live.cash(payroll);v.sales=live.firm_finance(importer).receipts;
        v.treasury=live.cash("A");
        for(const auto& actor:std::vector<std::string>{"A","B","CARRIER",supplier,importer,payroll})
            v.cash_total+=live.cash(actor);
        v.fills=engine.commerce().fills.size();v.in_flight=engine.commerce().daily_shipments.size();
        require_test(v.world_ore+v.batches==403&&v.cash_total==22'000&&
                     v.sales%18==0&&v.food+v.sales/18==3*v.batches&&live.financial().balanced(),
                     "physical ore, production sales, or original scenario cash was fabricated");
        return v;
    };
    const auto opening=observe(game);
    if(!(opening.fills>10&&opening.in_flight>0&&opening.batches>0&&
         opening.vendor>0&&opening.carrier>0&&opening.worker>0&&opening.sales>0))
        throw std::runtime_error("factory fixture lacks actual baseline ledger: fills "+
            std::to_string(opening.fills)+", underway "+std::to_string(opening.in_flight)+
            ", batches "+std::to_string(opening.batches)+", seller "+std::to_string(opening.vendor)+
            ", carrier "+std::to_string(opening.carrier)+", wages wallet "+
            std::to_string(opening.worker)+", sales "+std::to_string(opening.sales));
    bool blocked=false;
    phasegh::World::Plan check;
    check[1].push_back([&](phasegh::Stage& stage) {
        blocked=stage.naval_blockaded_edge(stage.commerce().transport_edges.at("SEA_FROM_B"));
    });
    game.advance_day(check);
    const auto still_delivering=observe(game);
    require_test(blocked&&still_delivering.fills==opening.fills&&
                 still_delivering.in_flight<opening.in_flight&&
                 still_delivering.ore+still_delivering.batches>opening.ore+opening.batches,
                 "real blockage dropped previous civilian cargo or permitted a new departure");

    const auto data=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame closed(daily::SnapshotCodec::decode(data),policy);
    phasegh::World::Plan consent;
    consent[9].push_back([=](phasegh::Stage& stage) {
        phasegh::TreatyClause term;
        term.id="factory-ceasefire";term.kind=phasegh::ClauseKind::ceasefire;
        term.obligor="B";term.beneficiary="A";term.war_id=war;
        stage.propose_treaty_package("FACTORY-TRUCE",{"A","B"},{term},"A","PM_A");
        stage.ratify_package_member("FACTORY-TRUCE","B","PM_B");
    });
    game.advance_day(consent);closed.advance_day();
    while(game.world().date()<daily::Date{2000,4,1}) {
        game.advance_day();closed.advance_day();
    }
    const auto shortage=observe(game),matched=observe(closed);
    require_test(shortage.ore==0&&shortage.in_flight==0&&shortage.batches>=opening.batches&&
                 shortage.batches==matched.batches&&shortage.fills==opening.fills&&
                 matched.fills==opening.fills&&shortage.vendor==opening.vendor&&
                 shortage.carrier==opening.carrier&&shortage.worker==matched.worker&&
                 game.world().engine().diplomacy().wars.at(war).active,
                 "physical shortage or both control branches changed before lawful ceasefire");

    game.advance_day();closed.advance_day();
    const auto travel=observe(game),closed_now=observe(closed);
    require_test(game.world().engine().diplomacy().wars.at(war).armistice&&
                 closed.world().engine().diplomacy().wars.at(war).active&&
                 travel.fills>shortage.fills&&travel.in_flight>0&&travel.ore==0&&
                 travel.batches==shortage.batches&&travel.worker==shortage.worker&&
                 travel.sales==shortage.sales&&closed_now.fills==matched.fills&&
                 closed_now.in_flight==0,
                 "only effective ceasefire may dispatch, and no production may predate actual arrival");
    phasec::Money freight=0,goods=0;
    Quantity sent=0;
    const auto& new_fills=game.world().engine().commerce().fills;
    for(std::size_t i=shortage.fills;i<new_fills.size();++i) {
        const auto& f=new_fills[i];
        require_test(f.shipped&&f.route_id=="CIVILIAN-ORE-ROUTE"&&
                     f.physical_path==std::vector<std::string>{"SEA_FROM_B"},
                     "restored ore did not dispatch over the canonical native route");
        freight+=f.freight_paid;goods+=f.goods_value;sent+=f.quantity;
    }
    require_test(sent>0&&freight==2*sent&&goods==10*sent&&
                 travel.vendor-shortage.vendor==goods&&travel.carrier-shortage.carrier==freight,
                 "restored route did not pay actual seller and carrier in cash");

    const auto underway=daily::SnapshotCodec::encode(game.world());
    p0::DailyGame replay(daily::SnapshotCodec::decode(underway),policy);
    game.advance_day();closed.advance_day();replay.advance_day();
    const auto resumed=observe(game),unrecovered=observe(closed);
    const auto produced=resumed.batches-travel.batches;
    const auto sold=(resumed.sales-travel.sales)/18;
    require_test(produced>0&&resumed.ore+produced==sent&&sold>0&&
                 resumed.worker-travel.worker==18*produced-18*sold&&
                 resumed.treasury-travel.treasury==2*produced&&
                 unrecovered.ore==0&&unrecovered.batches==matched.batches&&
                 unrecovered.sales==matched.sales&&unrecovered.worker==matched.worker&&
                 daily::SnapshotCodec::encode(game.world())==daily::SnapshotCodec::encode(replay.world()),
                 "arrived ore did not recover real work, paid withholding and food sales only after armistice");
}


static void blocked_automatic_army_rear_cargo_does_not_abort_a_day() {
    constexpr auto war="WAR:P2:CRISIS";
    for(const bool closed:{false,true}) {
        const auto warm=autonomous_war_fixture(true,CivilianSeaTrade::none,
            closed?SupplySea::blocked:SupplySea::open);
        auto policy=warm.policies();
        policy.allow_autonomous_naval_air_ai=false;
        // Independently observe the canonical blockade without any automatic
        // supply booking, then branch again from the identical saved opening.
        auto probe=warm;
        bool blocked=false;
        phasegh::World::Plan inspect;
        inspect[1].push_back([&](phasegh::Stage& stage) {
            blocked=stage.naval_blockaded_edge(stage.commerce().transport_edges.at("REAR-SEA"));
        });
        probe.advance_day(inspect);
        require_test(blocked==closed&&warm.world().engine().diplomacy().wars.at(war).active&&
                     c(warm).military_control("DEPOT-B")=="B"&&
                     c(warm).stock("B","DEPOT-B","ammunition")==12&&
                     c(warm).stock("REAR_UNIT_B","REAR-B","ammunition")==0&&
                     c(warm).military_readiness("REAR_UNIT_B","REAR-B").assigned_people==3,
                     closed?"blocked case lacks real game inputs":"open case lacks real game inputs");
        const auto opening=daily::SnapshotCodec::encode(warm.world());
        policy.allow_autonomous_army_resupply=true;
        if(closed) {
            // Even a lookalike internal ID submitted as an explicit command
            // retains the strict native rejection; it is not an AI booking.
            auto direct=daily::SnapshotCodec::decode(opening);
            daily::Command order;
            order.id="F:ARMY:MANUAL";order.kind=daily::Kind::cargo_dispatch;
            order.from="B";order.to="REAR_UNIT_B";order.region="DEPOT-B";
            order.destination="REAR-B";order.good="ammunition";order.route_id="REAR-SEA";
            order.amount=3;order.unit_price=1;
            direct.enqueue(direct.date(),std::move(order));
            const auto before=daily::SnapshotCodec::encode(direct);
            bool rejected=false;
            try {direct.advance_day();}
            catch(const std::invalid_argument& e) {
                rejected=std::string_view(e.what())=="H_ERR_REAL_NAVAL_BLOCKADE";
            }
            require_test(rejected&&daily::SnapshotCodec::encode(direct)==before,
                         "explicit blocked command must remain strictly transactional");
        } else {
            p0::DailyGame changed(daily::SnapshotCodec::decode(opening),policy);
            phasegh::World::Plan become_blocked;
            bool booked=false,released=false;
            become_blocked[3].push_back([&](phasegh::Stage& stage) {
                auto& live=stage.phase_c();
                const auto& usage=stage.commerce().edge_usage;
                booked=live.phase_d_cash_available("B")==live.cash("B")-6&&
                    live.phase_d_material_available("B","DEPOT-B","ammunition")==9&&
                    usage.contains({live.month(),"REAR-SEA"})&&
                    usage.at({live.month(),"REAR-SEA"})==3;
                live.phase_g_transfer_goods("A","NAVY_A","RA","fuel",1);
            });
            become_blocked[7].push_back([&](phasegh::Stage& stage) {
                auto& live=stage.phase_c();
                const auto& commerce=stage.commerce();
                released=stage.naval_blockaded_edge(commerce.transport_edges.at("REAR-SEA"))&&
                    live.phase_d_cash_available("B")==live.cash("B")&&
                    live.phase_d_material_available("B","DEPOT-B","ammunition")==12&&
                    !commerce.edge_usage.contains({live.month(),"REAR-SEA"});
            });
            const auto cancelled=changed.advance_day(become_blocked);
            require_test(booked&&released&&cancelled.actions==0&&
                         changed.world().journal().size()==warm.world().journal().size()&&
                         changed.world().day_index()==warm.world().day_index()+1&&
                         c(changed).cash("CARRIER")==0&&changed.world().cargo().empty()&&
                         c(changed).stock("B","DEPOT-B","ammunition")==12&&
                         c(changed).stock("REAR_UNIT_B","REAR-B","ammunition")==0,
                         "newly blocked automatic booking must release exact cash, stock, and edge holds");
        }
        p0::DailyGame game(daily::SnapshotCodec::decode(opening),policy);
        try {game.advance_day();}
        catch(const std::invalid_argument& e) {
            require_test(closed&&std::string_view(e.what())=="H_ERR_REAL_NAVAL_BLOCKADE"&&
                daily::SnapshotCodec::encode(game.world())==opening,
                "unexpected auto-cargo failure or failure failed to roll back");
            throw std::runtime_error("native RED: automatic rear cargo on blockaded edge rolled back the full day (H_ERR_REAL_NAVAL_BLOCKADE)");
        }
        require_test(game.world().day_index()==warm.world().day_index()+1&&
                     game.world().engine().diplomacy().wars.at(war).active,
                     "automatic supply must preserve the real wartime day");
        if(closed) {
            require_test(c(game).stock("B","DEPOT-B","ammunition")==12&&
                         c(game).stock("REAR_UNIT_B","REAR-B","ammunition")==0&&
                         c(game).cash("CARRIER")==0&&game.world().cargo().empty(),
                         "blocked game logistics must remain an actual unshipped shortage");
        } else {
            require_test(c(game).stock("B","DEPOT-B","ammunition")==9&&
                         c(game).cash("CARRIER")==6&&game.world().cargo().size()==1&&
                         c(game).stock("REAR_UNIT_B","REAR-B","ammunition")==0,
                         "accessible rear supply must book and pay a real cargo before arrival");
            game.advance_day();
            require_test(c(game).stock("REAR_UNIT_B","REAR-B","ammunition")==3&&
                         c(game).stock("B","DEPOT-B","ammunition")==9&&
                         c(game).cash("CARRIER")==6,
                         "supplied game unit received no physical cargo on its dated arrival");
        }
    }
}


int main(){
    try{
        fitting_purchase_requires_wages_and_consumes_real_stock();
        independent_workshop_must_pay_for_its_real_inputs();
        public_employment_pays_net_without_self_tax_revenue();
        automatic_wage_withholding_rereads_office_funding();
        country_tariff_ai_respects_each_sovereigns_current_law();
        tariff_ai_skips_stale_or_delegated_authority();
        daily_production_trade_ai_investment_replay();
        economic_laws_adjust_only_paid_private_order_ceiling();
        household_shareholder_efficiency_changes_paid_daily_construction_orders();
        aristocratic_shareholders_inject_only_lawful_monthly_capital();
        canonical_profession_sources_change_real_construction_orders();
        daily_market_shortage_and_political_response();
        daily_insolvent_hunger_is_not_ignored();
        daily_price_not_replayed_at_month_close();
        low_unit_price_still_changes_across_months();
        daily_battle_consumes_actual_ammo_and_pop();
        peacetime_military_actual_resources_reserves_and_replay();
        peacetime_military_unfunded_and_nonland_are_bounded();
        automatic_frontline_uses_adjacent_real_troops();
        automatic_naval_air_and_real_blockade();
        real_naval_blockade_stops_new_commerce_and_armistice_restores_it(
            CivilianSeaTrade::discovered);
        real_naval_blockade_stops_new_commerce_and_armistice_restores_it(
            CivilianSeaTrade::preset);
        effective_armistice_restores_actual_factory_supply_and_paid_work();
        blocked_automatic_army_rear_cargo_does_not_abort_a_day();
        automatic_peace_repatriates_and_pays_actual_debt();
        additional_active_cohorts_do_not_block_combat();
        duplicate_pop_demand_is_rejected();
        multiple_households_do_not_inflate_one_sellers_stock();
        a_strike_is_not_a_permanent_hiring_vacancy();
        native_autonomous_extraction_physical_flow();
        domestic_quarry_hires_and_supplies_paid_glassworks();
        native_autonomous_funded_research_and_day_lab_limit(true);
        native_autonomous_funded_research_and_day_lab_limit();
        production_research_speed_requires_actual_economic_law();
        enacted_terakoya_slows_actual_paid_original_research();
        national_elections_are_independent_and_persistent();
        p1_parliamentary_cabinet_next_day_and_save();
        autonomous_tax_requires_current_parliamentary_authority(false);
        autonomous_tax_requires_current_parliamentary_authority(true);
        selected_players_own_tax_without_disabling_rival_ai();
        public_construction_purchases_uses_and_pays_real_inputs();
        p1_autocracy_uses_real_institution_and_rejects_false_votes();
        native_autonomous_h_crisis_and_organization_funding();
        explicit_and_automatic_dues_keep_live_payment_priority();
        loss_of_paid_freight_constrains_output_and_solvent_customers();
        bounded_paid_freight_recovers_only_after_physical_arrival();
        commissioned_public_site_enables_paid_civilian_freight();
        producer_import_counts_real_on_site_and_inbound_stock_once();
        expired_trade_leg_uses_alternate_that_can_carry_full_lot();
        autonomous_f_two_hop_freight_and_save_replay();
        autonomous_daily_facility_maintenance_with_real_inputs();
        autonomous_h_treaty_risk_and_institutional_vote();
        autonomous_f_parallel_lanes_respect_shared_real_resources();
        autonomous_f_congestion_uses_actual_alternate_edge();
        autonomous_daily_bank_debt_service_and_liquidity_floor();
        native_cg_credit_service_keeps_trade_facility_in_sync();
        insolvent_bank_keeps_pop_deposits_and_stops_new_investment_credit();
        std::cout<<"P0 daily grand strategy: bounded integration scenarios PASS\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"P0 DAILY TEST FAILED: "<<e.what()<<"\n";
        return 1;
    }
}
