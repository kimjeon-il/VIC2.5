#include "vic25/p0_daily.hpp"
#include "vic25/daily_snapshot.hpp"
#include <iostream>
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
static void check_paid_investment_order(const char* law,const char* profession,bool share,
                      bool foreign,bool resolved,phasec::Money cash,
                      phaseb::People expected,phaseb::People maximum=4,
                      phaseb::People staff=12,phaseb::People material=20,
                      bool public_funds=false,phasec::Money arrears=0,
                      int false_amendment=0,InvestmentOrderLegalSources sources={}) {
    phasegh::World gh(24000);
    auto& geo=gh.seed_geography();
    geo.seed_province(1,"R",sources.country);geo.seed_unit(11,sources.country,"ROOT");
    geo.seed_assignment(1,11);
    if(foreign) {
        geo.seed_province(2,"F","B");geo.seed_unit(12,"B","FOREIGN");
        geo.seed_assignment(2,12);
    }
    auto& industry=geo.seed_core();
    auto& core=industry.seed_core();
    core.seed_actor(sources.country,0);core.seed_actor("FIRM",cash);
    core.seed_actor("BUILDER",0);core.seed_actor("CORP",10);
    const auto enact=[&](const std::string& country,const char* choice) {
        core.seed_government(country,"HEAD","PM",5,3);
        core.seed_p1_constitution(country,{phasec::P1GovernancePrinciple::monarchy,
            phasec::P1PowerDistribution::autocracy,
            phasec::P1LegislatureKind::none,"fixture:constitution"});
        phasec::P2BaseLaw act;
        act.id=country+":economy";act.country=country;
        act.domain="economic_system";act.scope="national";
        act.base_choice=choice;act.legal_basis="fixture:economy";
        core.p2_seed_base(act);
    };
    enact(sources.country,law);
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
    p0::DailyGame game([&] {
        if(arrears>0) {
            core.seed_actor("EMPLOYEE",0);
            const auto employee=people(gh,"R",1,"worker");
            core.seed_population().hire(employee,1,"FIRM","worker",arrears,
                                        phaseb::JobRequirements{});
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
            " paid="+std::to_string(work.paid_work)+
            " accepted="+std::to_string(work.accepted_work)+
            " capitalized="+std::to_string(work.capitalized_work)+
            " capitalized value="+std::to_string(work.capitalized_value));

    if(arrears>0)require_test(c(game).cash("EMPLOYEE")>0&&
                              c(game).cash("EMPLOYEE")+c(game).cash(sources.country)==arrears&&
                              c(game).outstanding_worker_wages("FIRM")==0,
                              "existing earned household wages were not actually settled");
    require_test(c(game).cash("FIRM")==cash-10*expected-arrears&&
                 c(game).cash(household)==100&&c(game).cash("CORP")==10&&
                 c(game).shares("company:FIRM",share?household:"CORP")==100&&
                 c(game).stock("BUILDER","R","steel")==material-expected,
                 "investment efficiency manufactured funds, equity or inputs");
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


static void native_autonomous_funded_research_and_day_lab_limit() {
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
static p0::DailyGame autonomous_war_fixture(bool combined_arms) {
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
    p0::Policies config;
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

int main(){
    try{
        daily_production_trade_ai_investment_replay();
        household_shareholder_efficiency_changes_paid_daily_construction_orders();
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
        automatic_peace_repatriates_and_pays_actual_debt();
        additional_active_cohorts_do_not_block_combat();
        duplicate_pop_demand_is_rejected();
        multiple_households_do_not_inflate_one_sellers_stock();
        a_strike_is_not_a_permanent_hiring_vacancy();
        native_autonomous_extraction_physical_flow();
        native_autonomous_funded_research_and_day_lab_limit();
        national_elections_are_independent_and_persistent();
        p1_parliamentary_cabinet_next_day_and_save();
        p1_autocracy_uses_real_institution_and_rejects_false_votes();
        native_autonomous_h_crisis_and_organization_funding();
        autonomous_f_two_hop_freight_and_save_replay();
        autonomous_daily_facility_maintenance_with_real_inputs();
        autonomous_h_treaty_risk_and_institutional_vote();
        autonomous_f_parallel_lanes_respect_shared_real_resources();
        autonomous_f_congestion_uses_actual_alternate_edge();
        autonomous_daily_bank_debt_service_and_liquidity_floor();
        native_cg_credit_service_keeps_trade_facility_in_sync();
        std::cout<<"P0 daily grand strategy: bounded integration scenarios PASS\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"P0 DAILY TEST FAILED: "<<e.what()<<"\n";
        return 1;
    }
}
