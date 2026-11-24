#include "vic25/daily_snapshot.hpp"
#include "vic25/p0_daily.hpp"
#include <array>
#include <charconv>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace {
using namespace vic25;
namespace fs=std::filesystem;
constexpr auto tutorial="market-v1";
constexpr auto origin="tutorial:market-v1";
constexpr auto country="TUT";
constexpr auto region="TUTORIAL";
constexpr auto producer="MILL";
constexpr auto buyer="PLAYER";
constexpr auto workers="WORKERS";
constexpr auto industry_scenario="industry-v1";
constexpr auto industry_origin="panel:industry-v1:TUT";
constexpr auto works="PUBLIC-WORKS";
constexpr auto mill_recipe="TUTORIAL-FOOD";
constexpr auto mill_site="MILL-SITE";

daily::World tutorial_world(bool industry_trial=false) {
    const auto checkpoint_origin=industry_trial?industry_origin:origin;
    phasegh::World engine{24000};
    auto& geo=engine.seed_geography();
    geo.set_dataset(checkpoint_origin);
    geo.seed_province(1,region,country);
    geo.seed_unit(1,country,"Tutorial district");
    geo.seed_assignment(1,1);
    auto& industry=geo.seed_core();
    auto& core=industry.seed_core();
    core.seed_actor(country,100);
    core.seed_actor(producer,100);
    core.seed_actor(buyer,50);
    core.seed_actor(workers,0);
    if(industry_trial)core.seed_actor(works,0);
    core.seed_government(country,"HEAD","PM",5,3);
    core.seed_market(region,"ore",1,1,10,0);
    core.seed_market(region,"food",10,1,20,0);
    core.seed_stock(producer,region,"ore",industry_trial?8:62);
    core.seed_stock(producer,region,"food",4);
    core.seed_production_facility(mill_site,producer,region,62);
    auto& pop=core.seed_population();
    const auto household=pop.household_profile({region,"mixed","shared",4});
    const auto economy=pop.economic_profile(
        {region,"worker","wage","employee",pop.default_education()});
    const auto demographic=pop.demographic_profile(
        {region,23640,phaseb::Sex::female,"tutorial-culture","tutorial-faith",
         "citizen","free",pop.default_health(),phaseb::Service::civilian});
    pop.hire(pop.seed_population(demographic,economy,household,4),
             4,producer,"worker",31,{});
    phased::IndustryRecipe recipe;
    recipe.id=mill_recipe;recipe.site=mill_site;
    recipe.inputs={{"ore",1}};recipe.outputs={{"food",2}};
    recipe.workers_per_batch=1;recipe.capacity_per_batch=1;
    industry.seed_recipe(recipe);
    return daily::World::adopt(std::move(engine),2025,checkpoint_origin);
}

p0::Policies tutorial_policies(bool industry_trial=false) {
    p0::Policies result;
    // The player is the only household buyer in this exercise.
    result.daily_food_units_per_person=0;
    result.monthly_clothing_units_per_person=0;
    result.manufacturers.push_back({mill_recipe,100,workers,country,1});
    if(industry_trial)result.public_budget.push_back({country,works,2,98});
    return result;
}

constexpr auto trial="tariff-v1";
constexpr auto budget_scenario="budget-v1";
constexpr auto trial_route="TRIAL-ORE";
constexpr auto trial_seller="TRIAL-SELLER";
constexpr auto trial_buyer="TRIAL-IMPORTER";
constexpr auto trial_carrier="TRIAL-CARRIER";
constexpr auto trial_good="ore";

std::string trial_origin(std::string_view tag,bool budget=false) {
    return std::string(budget?"trial:budget-v1:":"trial:tariff-v1:")+std::string(tag);
}
bool budget_world(const daily::World& world) {
    return world.migration_origin().starts_with("trial:budget-v1:");
}
std::array<std::string,2> budget_providers(const std::string& tag) {
    return {"PUBLIC-SERVICE-"+tag,"PUBLIC-PROGRAM-"+tag};
}
phasec::Money program_receipts(const phasec::IntegratedWorld& core,const std::string& tag) {
    const auto providers=budget_providers(tag);
    return phaseb::add_checked(core.firm_finance(providers[0]).receipts,
                               core.firm_finance(providers[1]).receipts);
}
const char* trial_region(std::string_view tag) {return tag=="A"?"DISTRICT-A":"DISTRICT-B";}
std::string_view other_country(std::string_view tag) {return tag=="A"?"B":"A";}
phasegh::Id province(std::string_view tag) {return tag=="A"?1:2;}

std::string public_builder(const std::string& tag) {return "PUBLIC-BUILDER-"+tag;}
std::string public_steel_seller(const std::string& tag) {return "PUBLIC-STEEL-"+tag;}
constexpr phasec::Money steel_campaign_price=24,steel_campaign_invoice=52;
constexpr phaseb::Id steel_builder_worker=209;
p0::PublicConstructionOffer public_offer(const std::string& tag,
        const std::string& region,bool elected=false,bool manufactured_steel=false) {
    p0::PublicConstructionOffer offer;
    auto& p=offer.project;
    p.id="PUBLIC-TOOLS-"+tag;p.target_site="PUBLIC-TOOLS-SITE-"+tag;
    p.client=tag;p.contractor=public_builder(tag);p.region=region;
    p.material_good="steel";p.work_required=1;p.material_per_work=2;
    p.workers_per_work=1;p.cash_price_per_work=manufactured_steel?steel_campaign_invoice:20;
    p.target_capacity=31;
    offer.treasury_cash_reserve=40;offer.elected_appropriation=elected;
    auto& recipe=offer.recipe;
    recipe.id="PUBLIC-TOOLS-METHOD-"+tag;recipe.site=p.target_site;
    recipe.inputs={{"iron",1},{"wood",1}};recipe.outputs={{"tools",1}};
    recipe.workers_per_batch=recipe.capacity_per_batch=1;
    return offer;
}

// An earlier budget checkpoint has neither role and keeps its original
// policies. Never create these actors, markets or stock on a resume.
bool has_public_build_fixture(const daily::World& world,const std::string& tag,
        const std::string& region) {
    const auto& c=world.engine().base().core().core();
    const auto builder=public_builder(tag),supplier=public_steel_seller(tag);
    if(!c.has_actor(builder)&&!c.has_actor(supplier))return false;
    if(!c.has_actor(builder)||!c.has_actor(supplier)||
       c.country_treasury(tag)==builder||c.country_treasury(tag)==supplier||
       !c.has_market(region,"steel")||!c.has_market(region,"tools"))
        throw std::invalid_argument("checkpoint has an incomplete public construction trial");
    return true;
}

void seed_public_build_fixture(phasec::IntegratedWorld& core,const std::string& tag,
        const std::string& region,bool linked_supply=false,bool manufactured_steel=false) {
    const auto builder=public_builder(tag),supplier=public_steel_seller(tag);
    core.seed_actor(builder,1200);core.seed_actor(supplier,manufactured_steel?200:0);
    const auto steel_price=manufactured_steel?steel_campaign_price:3;
    core.seed_market(region,"steel",steel_price,steel_price,steel_price,
                     manufactured_steel?2:0);
    const auto tools_quote=linked_supply?60:20;
    const auto iron_quote=linked_supply?40:4;
    core.seed_market(region,"tools",tools_quote,tools_quote,tools_quote,linked_supply?3:0);
    core.seed_market(region,"iron",iron_quote,iron_quote,iron_quote,linked_supply?32:0);
    core.seed_market(region,"wood",2,2,2,linked_supply?3:0);
    if(!manufactured_steel)core.seed_stock(supplier,region,"steel",2);
    auto& pop=core.seed_population();
    const auto household=pop.household_profile({region,builder,"shared",1});
    const auto economy=pop.economic_profile(
        {region,"worker","wage","employee",pop.default_education()});
    const auto demographic=pop.demographic_profile(
        {region,23640,phaseb::Sex::female,"trial-culture","trial-faith",
         "citizen","free",pop.default_health(),phaseb::Service::civilian});
    const auto worker=manufactured_steel?
        pop.seed_population(demographic,economy,household,1,steel_builder_worker):
        pop.seed_population(demographic,economy,household,1);
    core.seed_actor("HH:"+std::to_string(household),0);
    pop.hire(worker,1,builder,"worker",31,{});
}

daily::World trial_world(const std::string& selected,bool budget) {
    using namespace phasec;
    phasegh::World engine{24000};
    auto& geo=engine.seed_geography();
    geo.set_dataset(trial_origin(selected,budget));
    auto& core=geo.seed_core().seed_core();
    for(const std::string tag:{"A","B"}) {
        const auto id=province(tag);
        geo.seed_province(id,trial_region(tag),tag);
        geo.seed_unit(id,tag,trial_region(tag));
        geo.seed_assignment(id,id);
        core.seed_actor(tag,budget?200:100);
        core.seed_government(tag,"HEAD-"+tag,"PM-"+tag,5,3);
        core.seed_p1_constitution(tag,{P1GovernancePrinciple::monarchy,
            P1PowerDistribution::autocracy,P1LegislatureKind::none,"trial:constitution"});
        if(budget) {
            for(const auto& provider:budget_providers(tag))core.seed_actor(provider,0);
            core.seed_civilian_spending_cap_authority(tag);
        }
        core.p2_seed_base({"TRIAL-CUSTOMS-"+tag,tag,"trial_customs","national",
            "synthetic-import-ceiling","trial:customs-statute",P2NormTier::statute,
            {{"ore-tariff-ceiling",tag,P2ClauseKind::maximum_import_tariff_ppm,
                tag=="A"?200'000:400'000}}});
        const auto price=tag==selected?30:10;
        core.seed_market(trial_region(tag),trial_good,price,price,price,0);
    }
    if(budget)seed_public_build_fixture(core,selected,trial_region(selected));
    core.seed_actor(trial_seller,0);
    core.seed_actor(trial_buyer,3000);
    core.seed_actor(trial_carrier,0);
    const auto exporter=other_country(selected);
    core.seed_stock(trial_seller,trial_region(exporter),trial_good,120);
    const auto from=province(exporter),to=province(selected);
    engine.seed_verified_geographic_link("trial-rail-link",from,to,"rail");
    phasegh::TransportEdge edge;
    edge.id="trial-rail";edge.carrier=trial_carrier;edge.from=from;edge.to=to;
    edge.mode="rail";edge.monthly_capacity=31;edge.freight_per_unit=1;
    edge.transit_days=1;edge.evidence_id="trial:synthetic-rail";
    edge.valid_from_month=24000;edge.valid_through_month=24011;
    engine.seed_transport_edge(edge);
    engine.seed_route({trial_route,trial_seller,trial_buyer,trial_good,from,to,31,1});
    return daily::World::adopt(std::move(engine),2025,trial_origin(selected,budget));
}

p0::Policies trial_policies(const daily::World& world,const std::string& selected,bool budget) {
    p0::Policies result;
    result.daily_food_units_per_person=0;
    result.monthly_clothing_units_per_person=0;
    result.auto_national_budgets_from_actual_services=budget;
    result.player_controlled_countries.insert(selected);
    if(budget)
        for(const std::string tag:{"A","B"}) {
            const auto providers=budget_providers(tag);
            result.public_budget.push_back({tag,providers[0],2,40});
            result.public_budget.push_back({tag,providers[1],3,40});
        }
    if(budget&&has_public_build_fixture(world,selected,trial_region(selected)))
        result.public_construction_offers.push_back(public_offer(selected,trial_region(selected)));
    return result;
}

void verify_trial(const daily::World& world,const std::string& tag,bool budget) {
    const auto& engine=world.engine();
    const auto& routes=engine.commerce().routes;
    const auto it=routes.find(trial_route);
    if(world.migration_origin()!=trial_origin(tag,budget)||
       engine.base().spatial().dataset_hash!=trial_origin(tag,budget)||
       routes.size()!=1||it==routes.end()||
       it->second.origin!=province(other_country(tag))||
       it->second.destination!=province(tag)||
       it->second.buyer!=trial_buyer||it->second.seller!=trial_seller||
       it->second.good!=trial_good)
        throw std::invalid_argument("checkpoint does not belong to the selected trial country");
    if(budget) {
        const auto& core=engine.base().core().core();
        for(const std::string side:{"A","B"}) {
            if(!core.has_country_treasury(side)||core.country_treasury(side)!=side||
               !core.has_civilian_spending_cap_authority(side))
                throw std::invalid_argument("checkpoint does not contain the spending trial authorities");
            for(const auto& provider:budget_providers(side))
                if(!core.has_actor(provider))
                    throw std::invalid_argument("checkpoint does not contain the spending trial providers");
        }
    }
}

void trial_help(bool budget) {
    std::cout<<"Commands: status | step [days] | tariff <percent> | income-tax <percent> | save <new path> | help | quit\n"
             <<"Set the selected country's import duty on ore (whole percent); a lawful order advances one day.\n"
             <<"A lawful income-tax order advances one day and takes effect at the opening of the next month.\n"
             <<"Resume: vic25_play --trial "<<(budget?budget_scenario:trial)
             <<" --country A|B --resume <save>\n";
    if(budget)
        std::cout<<"spending-cap <amount>: schedule a daily public-program limit starting next month.\n"
                 <<"public-build: submit the offered ordinary public project if the trial has one.\n";
}

std::string month_opening(std::int64_t month) {
    const auto date=daily::from_legacy_month(month);
    std::ostringstream out;
    out<<std::setfill('0')<<std::setw(4)<<date.year<<'-'<<std::setw(2)<<date.month<<"-01";
    return out.str();
}

const p0::PublicConstructionOffer* find_public_offer(const p0::DailyGame& game,
                                                     const std::string& tag) {
    for(const auto& offer:game.policies().public_construction_offers)
        if(offer.project.client==tag)return &offer;
    return nullptr;
}

const char* public_construction_state(const phased::ConstructionProject* p) {
    return !p?"available":p->commissioned?"commissioned":p->ready?"paid":"queued";
}

void order_trial_public_build(p0::DailyGame& game,const std::string& tag) {
    const auto* offer=find_public_offer(game,tag);
    if(!offer)throw std::invalid_argument("this trial has no public construction offer");
    const auto& c=game.world().engine().base().core().core();
    game.order_public_construction(tag,c.government(tag).prime_minister,offer->project.id);
}

void trial_status(const p0::DailyGame& game,const std::string& selected) {
    const auto& world=game.world();
    const auto date=world.date();
    const auto& engine=world.engine();
    const auto& c=engine.base().core().core();
    const auto& tariffs=engine.diplomacy().tariffs;
    const auto current=tariffs.find({selected,trial_good});
    const auto pending=c.pending_country_tax(selected);
    const auto cap=c.p2_rule(selected,selected,
        phasec::P2ClauseKind::maximum_import_tariff_ppm).value_or(1'000'000);
    phasec::Money duties=0;
    for(const auto& receipt:engine.commerce().customs)
        if(receipt.importer==selected&&receipt.good==trial_good)
            duties=phaseb::add_checked(duties,receipt.paid);
    std::cout<<"date="<<std::setfill('0')<<std::setw(4)<<date.year<<'-'
             <<std::setw(2)<<date.month<<'-'<<std::setw(2)<<date.day<<std::setfill(' ')
             <<" country="<<selected<<" treasury="<<c.country_fiscal(selected).cash
             <<" max_tariff_ppm="<<cap<<" tariff_ppm="
             <<(current==tariffs.end()?0:current->second.ppm)
             <<" income_tax_ppm="<<c.income_tax_ppm(selected)
             <<" pending_income_tax_ppm="<<(pending?std::to_string(pending->second):"-")
             <<" pending_income_tax_date="<<(pending?month_opening(pending->first):"-")
             <<" paid_duty="<<duties
             <<" delivered_ore="<<c.stock(trial_buyer,trial_region(selected),trial_good)
             <<" native_commit="<<engine.commit_id();
    if(budget_world(world)) {
        const auto limit=c.civilian_spending_cap(selected);
        const auto change=c.pending_civilian_spending_cap(selected);
        std::cout<<" civilian_spending_cap="<<(limit?std::to_string(*limit):"-")
                 <<" pending_civilian_spending_cap="<<(change?std::to_string(change->second):"-")
                 <<" pending_civilian_spending_date="<<(change?month_opening(change->first):"-")
                 <<" public_program_paid="<<program_receipts(c,selected);
    }
    if(const auto* offer=find_public_offer(game,selected)) {
        const auto& p=offer->project;
        const auto& projects=engine.base().core().state().constructions;
        const auto started=projects.find(p.id);
        std::cout<<" public_build_state="<<public_construction_state(
                       started==projects.end()?nullptr:&started->second)
                 <<" public_build_invoice="<<phaseb::mul_checked(p.work_required,p.cash_price_per_work)
                 <<" public_build_steel="<<phaseb::mul_checked(p.work_required,p.material_per_work)
                 <<" public_build_paid_work="<<(started==projects.end()?0:started->second.paid_work);
    }
    std::cout<<'\n';
}

void help() {
    std::cout<<"Commands: status | step [days] | buy <food units> | save <new path> | help | quit\n"
             <<"A successful buy advances one day; stock and wallet apply at the live daily price.\n";
}

void status(const p0::DailyGame& game) {
    const auto& world=game.world();
    const auto date=world.date();
    const auto& core=world.engine().base().core().core();
    std::cout<<"date="<<std::setfill('0')<<std::setw(4)<<date.year<<'-'
             <<std::setw(2)<<date.month<<'-'<<std::setw(2)<<date.day<<std::setfill(' ')
             <<" population="<<core.people().population()
             <<" treasury="<<core.country_fiscal(country).cash
             <<" food_price="<<core.market(region,"food").price
             <<" producer_food="<<core.stock(producer,region,"food")
             <<" producer_ore="<<core.stock(producer,region,"ore")
             <<" buyer_cash="<<core.cash(buyer)
             <<" buyer_food="<<core.stock(buyer,region,"food")
             <<" native_commit="<<world.engine().commit_id()<<'\n';
}

int command_number(std::string_view value,int minimum,int maximum) {
    int result{};
    const auto end=value.data()+value.size();
    const auto [last,err]=std::from_chars(value.data(),end,result);
    if(err!=std::errc{}||last!=end||result<minimum||result>maximum)
        throw std::invalid_argument(minimum==0?"enter a whole percent from 0 to 100":
            "enter a positive whole number within the command limit");
    return result;
}

std::string_view trim(std::string_view value) {
    const auto first=value.find_first_not_of(" \t\r");
    if(first==std::string_view::npos)return {};
    return value.substr(first,value.find_last_not_of(" \t\r")-first+1);
}

// A hard link publishes the complete checkpoint in one no-replace operation.
// On a filesystem without hard-link support, fail safely instead of replacing
// a path or leaving a partial destination. The native codec owns the bytes.
void save_new(const daily::World& world,const fs::path& destination) {
    if(destination.empty()||fs::exists(fs::symlink_status(destination)))
        throw std::invalid_argument("refusing to overwrite an existing save (or empty path)");
    auto stage=destination;
    stage+=".vic25-saving";
    if(!fs::create_directory(stage))throw std::runtime_error("save staging path already exists");
    const auto pending=stage/"checkpoint";
    const auto cleanup=[&]() {
        std::error_code ignored;
        auto unfinished=pending;
        unfinished+=".uncommitted";
        fs::remove(unfinished,ignored);
        fs::remove(pending,ignored);
        fs::remove(stage,ignored);
    };
    try {
        daily::SnapshotCodec::save(world,pending);
        if(daily::SnapshotCodec::encode(daily::SnapshotCodec::load(pending))!=
           daily::SnapshotCodec::encode(world))
            throw std::runtime_error("save verification failed before publication");
        fs::create_hard_link(pending,destination);
    }catch(...) {
        cleanup();
        throw;
    }
    cleanup();
}

void set_trial_tariff(p0::DailyGame& game,const std::string& tag,int percent) {
    const auto ppm=percent*10'000;
    const auto key="TRIAL-TARIFF:"+std::to_string(game.world().day_index());
    phasegh::World::Plan order;
    order[0].push_back([tag,ppm,key](phasegh::Stage& stage){
        stage.adopt_tariff_daily(key,tag,trial_good,ppm,
            stage.phase_c().government(tag).prime_minister);
    });
    game.advance_day(order);
}

void propose_trial_income_tax(p0::DailyGame& game,const std::string& tag,int percent) {
    const auto ppm=percent*10'000;
    phasegh::World::Plan order;
    order[9].push_back([tag,ppm](phasegh::Stage& stage){
        auto& c=stage.phase_c();
        // This trial is a direct executive government. Never invent a
        // parliamentary vote; the same native authority and limits apply.
        c.propose_tax_law(tag,c.government(tag).prime_minister,{},ppm);
    });
    game.advance_day(order);
}

void propose_trial_spending_cap(p0::DailyGame& game,const std::string& tag,phasec::Money cap) {
    if(!budget_world(game.world()))
        throw std::invalid_argument("this trial has no public spending order");
    phasegh::World::Plan order;
    order[9].push_back([tag,cap](phasegh::Stage& stage){
        auto& c=stage.phase_c();
        c.propose_civilian_spending_cap(tag,c.government(tag).prime_minister,cap);
    });
    game.advance_day(order);
}

daily::World load_panel_checkpoint(const fs::path& resume) {
    if(!fs::is_regular_file(resume)||fs::file_size(resume)>850'000'000)
        throw std::invalid_argument("resume must use a regular file within the native save limit");
    return daily::SnapshotCodec::load(resume);
}

daily::World open_trial(const std::string& tag,const std::optional<fs::path>& resume,bool budget=false) {
    auto world=resume?load_panel_checkpoint(*resume):trial_world(tag,budget);
    verify_trial(world,tag,budget);
    return world;
}

daily::World open_industry(const std::optional<fs::path>& resume) {
    auto world=resume?load_panel_checkpoint(*resume):tutorial_world(true);
    const auto& base=world.engine().base();
    const auto& industry=base.core();
    const auto& core=industry.core();
    if(world.migration_origin()!=industry_origin||base.spatial().dataset_hash!=industry_origin||
       !core.has_country_treasury(country)||!core.has_actor(works)||
       core.country_treasury(country)==works||
       !industry.state().recipes.contains(mill_recipe)||
       industry.state().recipes.at(mill_recipe).site!=mill_site||
       !core.facilities().contains(mill_site)||core.facility(mill_site).owner!=producer||
       core.facility(mill_site).region!=region)
        throw std::invalid_argument("checkpoint does not belong to the industry trial");
    return world;
}

std::string json_string(std::string_view value) {
    std::string result="\"";
    constexpr auto hex="0123456789abcdef";
    for(unsigned char ch:value) {
        if(ch=='"'||ch=='\\') {result+='\\';result+=static_cast<char>(ch);}
        else if(ch<32) {
            result+="\\u00";result+=hex[ch>>4];result+=hex[ch&15];
        } else result+=static_cast<char>(ch);
    }
    return result+'"';
}

// This fixture exercises one actual electoral and statutory chain. It
// intentionally does not change the authority of the trade/budget trials.
constexpr auto law_trial="law-v1";
constexpr auto law_origin="panel:law-v1:A";
constexpr auto education_trial="education-v1";
constexpr auto education_origin="panel:education-v1:A";
constexpr auto education_remote_origin="panel:education-v1:remote:A";
constexpr auto education_school="EDU-SCHOOL";
constexpr auto education_home="EDU-DISTRICT";
constexpr auto education_compare="EDU-COMPARISON";
constexpr auto education_bill="LAW-SCHOOL-BILL";
constexpr auto law_region="LAW-DISTRICT";
constexpr auto campaign_scenario="campaign-v1";
constexpr auto legacy_campaign_origin="panel:campaign-v1";
constexpr auto campaign_procurement_origin="panel:campaign-procurement-v2";
constexpr auto campaign_ore_inventory_origin="panel:campaign-ore-inventory-v3";
constexpr auto supply_recovery_origin="panel:campaign-supply-v1";
constexpr auto steel_chain_origin="panel:campaign-steel-v1";
constexpr auto steel_coal_owner="CAMPAIGN-COAL-OWNER";
constexpr auto steel_coal_site="CAMPAIGN-COAL-MINE";
constexpr auto steel_coal_source="CAMPAIGN-COAL-SEAM";
constexpr auto steel_mill_site="CAMPAIGN-STEEL-MILL";
constexpr auto steel_mill_method="CAMPAIGN-STEEL-METHOD";
constexpr phasec::Money steel_coal_price=4;
constexpr phaseb::Id steel_coal_worker=207,steel_mill_worker=208;
constexpr auto campaign_region_b="CAMPAIGN-PEER-DISTRICT";
constexpr auto campaign_chamber_b="CAMPAIGN-PEER-ASSEMBLY";
constexpr auto campaign_employer_b="CAMPAIGN-PEER-EMPLOYER";
constexpr auto campaign_program_a="CAMPAIGN-A-PROVIDER";
constexpr auto campaign_program_b="CAMPAIGN-B-PROVIDER";
constexpr auto campaign_seller="CAMPAIGN-ORE-SELLER";
constexpr auto campaign_buyer="CAMPAIGN-ORE-BUYER";
constexpr auto campaign_carrier="CAMPAIGN-CARRIER";
constexpr auto campaign_route="CAMPAIGN-ORE-ROUTE";
constexpr auto campaign_edge="CAMPAIGN-RAIL";
constexpr auto recovery_edge="CAMPAIGN-RELIEF-ROAD";
constexpr auto recovery_carrier="CAMPAIGN-ROAD-CARRIER";
constexpr phaseb::Id recovery_artisan=205;
constexpr phaseb::Id campaign_worker_b=201;
constexpr phaseb::Id campaign_factory_worker=202;
constexpr auto campaign_smelter="CAMPAIGN-IRON-SITE";
constexpr auto campaign_iron_method="CAMPAIGN-IRON-METHOD";
constexpr auto campaign_wood_seller="CAMPAIGN-WOOD-SELLER";
constexpr auto campaign_tool_buyer="CAMPAIGN-FITTINGS-WORKSHOP";
constexpr auto campaign_tool_buyer_site="CAMPAIGN-FITTINGS-SITE";
constexpr auto campaign_tool_buyer_recipe="CAMPAIGN-FITTINGS-METHOD";
constexpr auto campaign_fittings="wooden_fittings";
constexpr auto campaign_logger_site="CAMPAIGN-LOGGER-SITE";
constexpr auto campaign_forest="CAMPAIGN-FOREST";
constexpr auto campaign_mill_site="CAMPAIGN-SAWMILL";
constexpr auto campaign_mill_method="CAMPAIGN-SAWING";
constexpr phaseb::Id campaign_logger_worker=203,campaign_mill_worker=204;
constexpr phaseb::Id recovery_smelter_worker=206;
constexpr auto campaign_boards="sawn_boards";
constexpr auto campaign_board_vendor="CAMPAIGN-BOARD-VENDOR";
constexpr phasec::Money campaign_board_price=20;


bool has_campaign_supply(const daily::World& world) {
    const auto& d=world.engine().base().core();
    const auto& c=d.core();
    if(!c.has_actor(campaign_wood_seller)&&
       !c.facilities().contains(campaign_smelter)&&
       !d.state().recipes.contains(campaign_iron_method))return false;
    const auto facility=c.facilities().find(campaign_smelter);
    const auto recipe=d.state().recipes.find(campaign_iron_method);
    if(!c.has_actor(campaign_wood_seller)||facility==c.facilities().end()||
       facility->second.owner!=campaign_buyer||facility->second.region!=law_region||
       recipe==d.state().recipes.end()||recipe->second.site!=campaign_smelter)
        throw std::invalid_argument("checkpoint has an incomplete campaign supply chain");
    return true;
}

bool has_campaign_workshop(const daily::World& world) {
    const auto& domain=world.engine().base().core();
    const auto& core=domain.core();
    const bool buyer=core.has_actor(campaign_tool_buyer);
    const auto site=core.facilities().find(campaign_tool_buyer_site);
    const auto recipe=domain.state().recipes.find(campaign_tool_buyer_recipe);
    if(!buyer&&site==core.facilities().end()&&recipe==domain.state().recipes.end())return false;
    const auto exactly=[](const phased::Input& input,const char* good) {
        return input.good==good&&input.per_unit==1;
    };
    if(!buyer||site==core.facilities().end()||site->second.owner!=campaign_tool_buyer||
       site->second.region!=law_region||recipe==domain.state().recipes.end()||
       !has_campaign_supply(world))
        throw std::invalid_argument("checkpoint has an incomplete campaign workshop");
    const auto& method=recipe->second;
    if(method.site!=campaign_tool_buyer_site||method.inputs.size()!=2||
       !exactly(method.inputs[0],"tools")||!exactly(method.inputs[1],campaign_boards)||
       method.outputs.size()!=1||!exactly(method.outputs.front(),campaign_fittings)||
       !method.byproducts.empty()||
       !core.has_market(law_region,"tools")||!core.has_market(law_region,campaign_boards)||
       !core.has_market(law_region,campaign_fittings))
        throw std::invalid_argument("checkpoint has a mismatched campaign workshop");
    return true;
}

enum class LegislativeTrial { wage, education, education_remote, campaign, supply_recovery, steel_chain };
bool campaign_trial(LegislativeTrial trial) {
    return trial==LegislativeTrial::campaign||trial==LegislativeTrial::supply_recovery||
           trial==LegislativeTrial::steel_chain;
}
bool is_steel_campaign(const daily::World& world) {
    return world.migration_origin()==steel_chain_origin;
}
bool is_supply_recovery(const daily::World& world) {
    return world.migration_origin()==supply_recovery_origin;
}
const char* legislative_origin(LegislativeTrial trial) {
    switch(trial) {
        case LegislativeTrial::wage:return law_origin;
        case LegislativeTrial::education:return education_origin;
        case LegislativeTrial::education_remote:return education_remote_origin;
        case LegislativeTrial::campaign:return campaign_ore_inventory_origin;
        case LegislativeTrial::supply_recovery:return supply_recovery_origin;
        case LegislativeTrial::steel_chain:return steel_chain_origin;
    }
    throw std::logic_error("unknown legislative trial");
}
std::optional<LegislativeTrial> legislative_trial(std::string_view choice) {
    if(choice=="LAW-A")return LegislativeTrial::wage;
    if(choice=="CAMPAIGN-A")return LegislativeTrial::campaign;
    if(choice=="SUPPLY-A")return LegislativeTrial::supply_recovery;
    if(choice=="STEEL-A")return LegislativeTrial::steel_chain;
    if(choice=="EDU-A")return LegislativeTrial::education;
    if(choice=="EDU-REMOTE-A")return LegislativeTrial::education_remote;
    return std::nullopt;
}
constexpr auto law_chamber="LAW-ASSEMBLY";
constexpr auto law_bill="LAW-WAGE-BILL";
constexpr auto law_domain="trial_wage_floor";
constexpr auto law_choice="statutory-contract-minimum";
constexpr auto law_backers="WAGE-ASSOCIATION";
constexpr auto law_employer="LAW-EMPLOYER";
constexpr std::array<phaseb::Id,2> law_worker_partitions{101,102};
constexpr phaseb::Id law_voter_partition=103;
constexpr phasec::Money opening_contract_wage=31, proposed_floor=62;

constexpr auto campaign_retail_household="campaign-wage-fitting-buyer";
constexpr phasec::Money campaign_fitting_price=90;

phaseb::Id law_slice(const phasec::IntegratedWorld& c,phaseb::Id partition,
            std::string_view region=law_region) {
    for(const auto& [id,slice]:c.people().slices())
        if(slice.key.partition==partition&&
           c.people().demographic(slice.key.demographic).region==region)
            return id;
    throw std::invalid_argument("expected legislative trial cohort is missing");
}

std::string trial_worker_wallet(const phasec::IntegratedWorld& c,phaseb::Id worker) {
    return "HH:"+std::to_string(c.people().slice(worker).key.household);
}

// A separate small economy: yesterday's private earnings can finance
// today's expansion without changing the shared campaign or its saves.
constexpr auto expansion_scenario="expansion-v1";
constexpr auto expansion_origin="panel:expansion-v1:A";
constexpr auto expansion_region="MILL-DISTRICT";
constexpr auto expansion_maker="PRIVATE-MILL";
constexpr auto expansion_builder="PRIVATE-BUILDER";
constexpr auto expansion_site="OPENING-MILL";
constexpr auto expansion_method="GRAIN-MILLING";
constexpr auto expansion_template="OPENING-MILL-TEMPLATE";
constexpr phaseb::Id expansion_recruit=301;

bool is_expansion_world(const daily::World& world) {
    return world.migration_origin()==expansion_origin;
}

daily::World expansion_world() {
    phasegh::World engine(24000);
    auto& geo=engine.seed_geography();
    geo.set_dataset(expansion_origin);
    geo.seed_province(1,expansion_region,"A");
    geo.seed_unit(1,"A",expansion_region);geo.seed_assignment(1,1);
    auto& industry=geo.seed_core();
    auto& c=industry.seed_core();
    c.seed_actor("A",0);
    c.seed_actor(expansion_maker,30);
    c.seed_actor(expansion_builder,1000);
    c.seed_government("A","MILL-HEAD","MILL-PM",5,3);
    c.p1_mark_player_country("A",true);
    c.seed_market(expansion_region,"food",25,25,25);
    c.seed_market(expansion_region,"grain",1,1,1);
    c.seed_market(expansion_region,"steel",1,1,1);
    c.seed_production_facility(expansion_site,expansion_maker,expansion_region,31);
    c.seed_stock(expansion_maker,expansion_region,"grain",31);
    c.seed_stock(expansion_builder,expansion_region,"steel",8);
    auto& pop=c.seed_population();
    const auto economic=pop.economic_profile({expansion_region,"worker","wage","employee",
                                              pop.default_education()});
    const auto demographic=pop.demographic_profile({expansion_region,23700,phaseb::Sex::female,
        "trial-culture","trial-faith","citizen","free",pop.default_health(),
        phaseb::Service::civilian});
    auto add_worker=[&](const std::string& owner,const std::string& family,
                        phasec::Money daily_wage,phaseb::Id partition) {
        const auto home=pop.household_profile({expansion_region,family,"shared",1});
        const auto resident=pop.seed_population(demographic,economic,home,1,partition);
        c.seed_actor("HH:"+std::to_string(home),0);
        if(daily_wage)pop.hire(resident,1,owner,"worker",daily_wage*31,{});
    };
    add_worker(expansion_maker,"mill-worker",10,302);
    add_worker(expansion_maker,"idle-recruit",0,expansion_recruit);
    add_worker(expansion_builder,"contractor-worker",60,303);
    add_worker(expansion_builder,"contractor-helper",10,304);
    // Describe the already operating mill. Normal enterprise AI decides
    // when it can pay for the next one; no work or funds are granted later.
    phased::ConstructionProject original;
    original.id=expansion_template;original.client=expansion_maker;
    original.contractor=expansion_builder;original.region=expansion_region;
    original.target_site=expansion_site;original.material_good="steel";
    original.work_required=original.material_per_work=original.workers_per_work=1;
    original.cash_price_per_work=30;original.target_capacity=31;
    original.accepted_work=original.paid_work=original.capitalized_work=1;
    original.capitalized_value=30;original.commissioned=true;
    industry.seed_construction(original);
    phased::IndustryRecipe recipe;
    recipe.id=expansion_method;recipe.site=expansion_site;
    recipe.inputs={{"grain",1}};recipe.outputs={{"food",1}};
    recipe.workers_per_batch=recipe.capacity_per_batch=1;
    industry.seed_recipe(recipe);
    return daily::World::adopt(std::move(engine),54,expansion_origin);
}

daily::World open_expansion(const std::optional<fs::path>& resume) {
    auto world=resume?load_panel_checkpoint(*resume):expansion_world();
    const auto& base=world.engine().base();
    const auto& industry=base.core();
    const auto& c=industry.core();
    if(!is_expansion_world(world)||base.spatial().dataset_hash!=expansion_origin||
       !c.has_country_treasury("A")||!c.has_actor(expansion_maker)||
       !c.has_actor(expansion_builder)||!c.facilities().contains(expansion_site)||
       c.facility(expansion_site).owner!=expansion_maker||
       c.facility(expansion_site).region!=expansion_region||
       !industry.state().constructions.contains(expansion_template)||
       !industry.state().recipes.contains(expansion_method)||
       industry.state().recipes.at(expansion_method).site!=expansion_site||
       !c.has_actor(trial_worker_wallet(c,law_slice(c,expansion_recruit,expansion_region))))
        throw std::invalid_argument("checkpoint does not belong to the private expansion trial");
    return world;
}

p0::Policies expansion_policies() {
    p0::Policies p;
    p.daily_food_units_per_person=3;
    p.monthly_clothing_units_per_person=0;
    p.allow_endogenous_input_procurement=false;
    p.allow_endogenous_investment_credit=false;
    p.allow_collateralized_enterprise_credit=false;
    return p;
}

void expansion_status(std::ostringstream& out,const daily::World& world) {
    const auto& industry=world.engine().base().core();
    const auto& c=industry.core();
    const auto& domain=industry.state();
    phasec::Money paid=0;
    Quantity capacity=0,batches=0;
    for(const auto& [id,project]:domain.constructions) {
        if(id==expansion_template||project.client!=expansion_maker||
           project.region!=expansion_region)continue;
        paid=phaseb::add_checked(paid,project.capitalized_value);
        if(!project.commissioned)continue;
        capacity=phaseb::add_checked(capacity,c.facility(project.target_site).capacity);
        for(const auto& run:domain.actual_production_runs)
            if(domain.recipes.at(run.recipe).site==project.target_site)
                batches=phaseb::add_checked(batches,run.batches);
    }
    const auto worker=law_slice(c,expansion_recruit,expansion_region);
    const auto* job=c.people().job(worker);
    out<<",\"scenario\":"<<json_string(expansion_scenario)
       <<",\"private_expansion\":{\"maker_cash\":"<<c.cash(expansion_maker)
       <<",\"sales_receipts\":"<<c.firm_finance(expansion_maker).receipts
       <<",\"builder_cash\":"<<c.cash(expansion_builder)
       <<",\"builder_steel\":"<<c.stock(expansion_builder,expansion_region,"steel")
       <<",\"paid_construction\":"<<paid
       <<",\"commissioned_capacity\":"<<capacity
       <<",\"new_worker_cash\":"<<c.cash(trial_worker_wallet(c,worker))
       <<",\"new_worker_employed\":"<<(job&&job->employer==expansion_maker?"true":"false")
       <<",\"expansion_batches\":"<<batches
       <<",\"food_stock\":"<<c.stock(expansion_maker,expansion_region,"food")<<"}}";
}

// The two markets deliberately do not share goods, payroll or transport.
// Only household contract pay differs; both use the same native basket.
constexpr auto staples_origin="panel:staples-v1:A";
constexpr auto staples_scenario="staples-v1";
constexpr auto staples_household_class="budget-comparison-household";
struct StaplesCase {
    const char* key;
    const char* region;
    phaseb::Id district, household_partition;
    phasec::Money contract_wage;
};
constexpr std::array<StaplesCase,2> staples_cases{{
    {"necessities_only","ESSENTIALS-DISTRICT",1,401,50*31},
    {"with_discretionary","WIDER-BASKET-DISTRICT",2,402,84*31},
}};
struct StaplesGood {
    const char* key;
    const char* good;
    const char* input;
    const char* archetype;
    phasec::Money price;
};
constexpr std::array<StaplesGood,3> staples_goods{{
    {"groceries","groceries","grain","vic3_115.urban.food_industries",20},
    {"clothes","clothes","fabric","vic3_115.urban.textile_mills",25},
    {"fittings",campaign_fittings,campaign_boards,nullptr,30},
}};

bool is_staples_world(const daily::World& world) {
    return world.migration_origin()==staples_origin;
}
std::string staples_id(const StaplesCase& trial,std::string_view role) {
    return std::string(role)+':'+trial.region;
}
std::string staples_seller(const StaplesCase& trial,const StaplesGood& good) {
    return staples_id(trial,std::string("FIRM:")+good.key);
}

daily::World staples_world() {
    phasegh::World engine(24000);
    auto& geo=engine.seed_geography();
    geo.set_dataset(staples_origin);
    for(const auto& trial:staples_cases) {
        geo.seed_province(trial.district,trial.region,"A");
        geo.seed_unit(trial.district,"A",trial.region);
        geo.seed_assignment(trial.district,trial.district);
    }
    auto& industry=geo.seed_core();
    auto& c=industry.seed_core();
    c.seed_actor("A",0);
    c.seed_government("A","STAPLES-HEAD","STAPLES-PM",5,3);
    c.p1_mark_player_country("A",true);
    industry.seed_standard_industries();
    auto& pop=c.seed_population();
    for(const auto& trial:staples_cases) {
        const auto worker=pop.economic_profile(
            {trial.region,"worker","wage","employee",pop.default_education()});
        const auto consumer=pop.economic_profile(
            {trial.region,"worker","wage",staples_household_class,pop.default_education()});
        const auto demo=pop.demographic_profile({trial.region,23700,phaseb::Sex::female,
            "trial-culture","trial-faith","citizen","free",pop.default_health(),
            phaseb::Service::civilian});
        const auto add_worker=[&](const std::string& role,const std::string& employer,
                                  phaseb::Id profile,phasec::Money pay,
                                  phaseb::Id partition) {
            const auto home=pop.household_profile({trial.region,role,"shared",1});
            const auto resident=pop.seed_population(demo,profile,home,1,partition);
            c.seed_actor("HH:"+std::to_string(home),0);
            pop.hire(resident,1,employer,"worker",pay,{});
        };
        const auto employer=staples_id(trial,"EMPLOYER");
        c.seed_actor(employer,3000);
        add_worker("comparison-household",employer,consumer,trial.contract_wage,
                   trial.household_partition);
        c.seed_market(trial.region,"tools",1,1,1);
        phaseb::Id producer_partition=trial.household_partition*10;
        for(const auto& good:staples_goods) {
            const auto seller=staples_seller(trial,good);
            const auto site=staples_id(trial,std::string("SITE:")+good.key);
            c.seed_actor(seller,200);
            c.seed_market(trial.region,good.good,good.price,good.price,good.price);
            c.seed_market(trial.region,good.input,1,1,1);
            c.seed_stock(seller,trial.region,good.input,8);
            add_worker(good.key,seller,worker,31,++producer_partition);
            phased::IndustryRecipe method;
            method.id=staples_id(trial,good.key);method.site=site;
            method.inputs={{good.input,1}};method.outputs={{good.good,1}};
            method.workers_per_batch=method.capacity_per_batch=1;
            if(good.archetype) {
                geo.seed_building_in_province(site,seller,trial.district,31,good.archetype);
                method.industry_class="MANU";
                industry.seed_building_recipe(method);
            } else {
                c.seed_stock(seller,trial.region,"tools",8);
                c.seed_production_facility(site,seller,trial.region,31);
                method.inputs.push_back({"tools",1});
                industry.seed_recipe(method);
            }
        }
    }
    return daily::World::adopt(std::move(engine),93,staples_origin);
}

daily::World open_staples(const std::optional<fs::path>& resume) {
    auto world=resume?load_panel_checkpoint(*resume):staples_world();
    const auto& base=world.engine().base();
    const auto& industry=base.core();
    const auto& c=industry.core();
    if(!is_staples_world(world)||base.spatial().dataset_hash!=staples_origin||
       !c.has_country_treasury("A"))
        throw std::invalid_argument("checkpoint does not belong to the staple household trial");
    for(const auto& trial:staples_cases) {
        const auto household=law_slice(c,trial.household_partition,trial.region);
        const auto& pop=c.people().slice(household);
        if(pop.count!=1||c.people().economic(pop.key.economic).labor_class!=staples_household_class||
           !c.has_actor(trial_worker_wallet(c,household)))
            throw std::invalid_argument("checkpoint lacks the matched household");
        for(const auto& good:staples_goods) {
            const auto seller=staples_seller(trial,good);
            const auto method=industry.state().recipes.find(staples_id(trial,good.key));
            if(!c.has_actor(seller)||method==industry.state().recipes.end()||
               !c.facilities().contains(method->second.site)||
               c.facility(method->second.site).owner!=seller||
               c.facility(method->second.site).region!=trial.region||
               !c.has_market(trial.region,good.good))
                throw std::invalid_argument("checkpoint lacks a matched local supplier");
            const auto& quote=c.market(trial.region,good.good);
            if(quote.price!=good.price||quote.floor!=good.price||quote.ceiling!=good.price)
                throw std::invalid_argument("checkpoint lacks the fixed matched market quote");
        }
    }
    return world;
}

p0::Policies staples_policies() {
    p0::Policies p;
    p.daily_food_units_per_person=p.monthly_clothing_units_per_person=0;
    p.allow_endogenous_staffing=false;
    p.allow_endogenous_factory_expansion=false;
    p.allow_endogenous_input_procurement=false;
    p.allow_endogenous_investment_credit=false;
    p.allow_collateralized_enterprise_credit=false;
    p.household_basket={
        {"groceries",staples_household_class,31,0},
        {"clothes",staples_household_class,31,1},
        {campaign_fittings,staples_household_class,31,2}
    };
    return p;
}

void staples_status(std::ostringstream& out,const daily::World& world) {
    const auto& industry=world.engine().base().core();
    const auto& c=industry.core();
    std::map<std::string,Quantity> batches;
    for(const auto& run:industry.state().actual_production_runs)
        batches[run.recipe]=phaseb::add_checked(batches[run.recipe],run.batches);
    out<<R"(,"scenario":)"<<json_string(staples_scenario)
       <<R"(,"household_staples":{)";
    bool first=true;
    for(const auto& trial:staples_cases) {
        if(!first)out<<',';
        first=false;
        const auto household=law_slice(c,trial.household_partition,trial.region);
        const auto wallet=trial_worker_wallet(c,household);
        // The only household cash inflow is paid wages, and its only
        // non-tax cash outflows are the three isolated seller receipts.
        // Reconcile settled net earnings, not gross wage claims in C revenue.
        const auto cash=c.cash(wallet);
        auto net_wages=cash;
        std::array<phasec::Money,staples_goods.size()> receipts{};
        for(std::size_t i=0;i<staples_goods.size();++i) {
            receipts[i]=c.firm_finance(staples_seller(trial,staples_goods[i])).receipts;
            net_wages=phaseb::add_checked(net_wages,receipts[i]);
        }
        out<<json_string(trial.key)<<R"(:{"wage_received":)"<<net_wages
           <<R"(,"cash":)"<<cash;
        for(std::size_t i=0;i<staples_goods.size();++i) {
            const auto& good=staples_goods[i];
            const auto seller=staples_seller(trial,good);
            const auto field=[&](std::string_view suffix,auto value) {
                out<<','<<json_string(std::string(good.key)+std::string(suffix))<<':'<<value;
            };
            field("_bought",receipts[i]/good.price);
            field("_receipts",receipts[i]);
            field("_stock",c.stock(seller,trial.region,good.good));
            field("_made",batches[staples_id(trial,good.key)]);
            const auto input_key=good.archetype?good.input:"boards";
            out<<','<<json_string(std::string(input_key)+"_stock")<<':'
               <<c.stock(seller,trial.region,good.input);
            if(!good.archetype)
                out<<R"(,"tools_stock":)"<<c.stock(seller,trial.region,"tools");
        }
        out<<'}';
    }
    out<<"}}";
}

std::optional<p0::Buyer> campaign_household_order(const daily::World& world) {
    const auto& c=world.engine().base().core().core();
    const auto resident=law_slice(c,law_worker_partitions.front());
    const auto& slice=c.people().slice(resident);
    const auto& house=c.people().household(slice.key.household);
    if(house.composition!=campaign_retail_household)return std::nullopt;
    const auto wallet=trial_worker_wallet(c,resident);
    if(!has_campaign_workshop(world)||house.region!=law_region||!c.has_actor(wallet))
        throw std::invalid_argument("checkpoint lacks the authored fitting buyer");
    const auto& quote=c.market(law_region,campaign_fittings);
    if(quote.price!=campaign_fitting_price||quote.floor!=campaign_fitting_price||
       quote.ceiling!=campaign_fitting_price)
        throw std::invalid_argument("checkpoint lacks the fixed campaign retail quote");
    return p0::Buyer{resident,wallet,campaign_tool_buyer,law_region,campaign_fittings,1};
}

bool has_campaign_board_vendor(const daily::World& world) {
    const auto& c=world.engine().base().core().core();
    if(!c.has_actor(campaign_board_vendor))return false;
    if(!campaign_household_order(world))
        throw std::invalid_argument("checkpoint has a board vendor without its campaign workshop");
    const auto& quote=c.market(law_region,campaign_boards);
    if(quote.price!=campaign_board_price||quote.floor!=campaign_board_price||
       quote.ceiling!=campaign_board_price)
        throw std::invalid_argument("checkpoint lacks the fixed campaign board quote");
    return true;
}

bool has_campaign_timber(const daily::World& world) {
    const auto& d=world.engine().base().core();
    const auto& c=d.core();
    const auto source=d.state().resource_sites.find(campaign_forest);
    const auto method=d.state().recipes.find(campaign_mill_method);
    const auto logger=c.facilities().find(campaign_logger_site);
    const auto mill=c.facilities().find(campaign_mill_site);
    if(source==d.state().resource_sites.end()&&method==d.state().recipes.end()&&
       logger==c.facilities().end()&&mill==c.facilities().end())return false;
    if(source==d.state().resource_sites.end()||method==d.state().recipes.end()||
       logger==c.facilities().end()||mill==c.facilities().end()||
       !has_campaign_board_vendor(world)||!c.has_market(law_region,"wood"))
        throw std::invalid_argument("checkpoint has an incomplete native timber chain");
    const auto& forest=source->second;
    const auto& saw=method->second;
    const auto& quote=c.market(law_region,"wood");
    if(forest.site!=campaign_logger_site||forest.owner!=campaign_wood_seller||
       forest.region!=law_region||forest.good!="wood"||!forest.renewable||
       logger->second.owner!=campaign_wood_seller||logger->second.region!=law_region||
       mill->second.owner!=campaign_board_vendor||mill->second.region!=law_region||
       saw.site!=campaign_mill_site||saw.inputs.size()!=1||saw.outputs.size()!=1||
       saw.inputs.front().good!="wood"||saw.inputs.front().per_unit!=1||
       saw.outputs.front().good!=campaign_boards||saw.outputs.front().per_unit!=1||
       !saw.byproducts.empty()||quote.price!=2||quote.floor!=2||quote.ceiling!=2)
        throw std::invalid_argument("checkpoint has a mismatched native timber chain");
    return true;
}

p0::Policies law_policies(const daily::World& world,LegislativeTrial trial) {
    p0::Policies p;
    p.daily_food_units_per_person=0;
    p.monthly_clothing_units_per_person=0;
    p.allow_endogenous_staffing=false;
    p.player_controlled_countries.insert("A");
    if(campaign_trial(trial)) {
        p.public_budget.push_back({"A",campaign_program_a,2,0});
        p.public_budget.push_back({"B",campaign_program_b,100,0});
        p.allow_endogenous_debt_repayment=false;
        p.autonomous_tax_step_ppm=10'000;
        p.allow_endogenous_staffing=has_campaign_supply(world);
        const bool buffered_ore=world.migration_origin()==campaign_ore_inventory_origin;
        p.gate_local_input_procurement_by_production=
            buffered_ore||world.migration_origin()==campaign_procurement_origin;
        if(buffered_ore)p.standing_route_stock_targets.emplace(campaign_route,2);
        if(p.allow_endogenous_staffing)
            p.staffing.push_back({campaign_buyer,law_region,1,31,true});
        if(has_campaign_workshop(world))
            p.staffing.push_back({campaign_tool_buyer,law_region,1,31,true});
        if(const auto order=campaign_household_order(world))
            p.buyers.push_back(*order);
        if(is_supply_recovery(world)) {
            const auto& c=world.engine().base().core().core();
            const auto artisan=law_slice(c,recovery_artisan);
            p.buyers.push_back({artisan,trial_worker_wallet(c,artisan),
                                campaign_buyer,law_region,"iron",1});
        }
        if(has_campaign_timber(world)) {
            p.staffing.push_back({campaign_wood_seller,law_region,1,31,true});
            p.staffing.push_back({campaign_board_vendor,law_region,1,31,true});
        } else (void)has_campaign_board_vendor(world);
        if(has_public_build_fixture(world,"A",law_region))
            p.public_construction_offers.push_back(
                public_offer("A",law_region,true,is_steel_campaign(world)));
    }
    return p;
}

daily::World law_world(LegislativeTrial trial) {
    using namespace phasec;
    phasegh::World engine(24000);
    auto& geo=engine.seed_geography();
    const bool campaign=campaign_trial(trial);
    const bool recovery=trial==LegislativeTrial::supply_recovery;
    const bool steel_chain=trial==LegislativeTrial::steel_chain;
    const bool education=trial==LegislativeTrial::education||
                         trial==LegislativeTrial::education_remote;
    geo.set_dataset(legislative_origin(trial));
    geo.seed_province(1,law_region,"A");geo.seed_unit(1,"A",law_region);
    geo.seed_assignment(1,1);
    if(education) {
        geo.seed_province(2,education_home,"A");geo.seed_assignment(2,1);
        geo.seed_province(3,education_compare,"A");geo.seed_assignment(3,1);
    }
    if(campaign) {
        geo.seed_province(2,campaign_region_b,"B");geo.seed_unit(2,"B",campaign_region_b);
        geo.seed_assignment(2,2);
        // A verified road also needs physical adjacency of these two
        // already-authored abstract districts.
        if(recovery)geo.seed_border(1,2);
    }
    auto& industry=geo.seed_core();
    auto& c=industry.seed_core();
    c.seed_actor("A",campaign?100:0);c.seed_actor(law_employer,campaign?80'000:1'000);
    c.seed_government("A","LAW-HEAD","LAW-PM",5,3);
    c.seed_p1_constitution("A",{P1GovernancePrinciple::parliamentary_republic,
        P1PowerDistribution::universal_vote,P1LegislatureKind::unicameral,
        "trial:elected-legislature"});
    c.p1_mark_player_country("A",true);
    c.p4_register_party({"WORKERS","A","",700'000,true,true});
    c.p4_register_party({"OTHER","A","",300'000,true,true});
    const auto election=daily::to_day_index({2000,1,2});
    c.p4_seed_contest({law_chamber,"A","national","trial:actual-ballot",
        P4Office::legislature,P4BallotMethod::list_dhondt,P4TermMode::restart_after_poll,
        48,1'000'000,5,election,election,-1,true,true,false});
    c.seed_interest_group(law_backers);
    c.p3_seed_group({"A",law_backers,"","","",P3GroupType::custom,0,0});
    c.p4_set_official_affiliation("A",law_backers,"WORKERS",daily::to_day_index({2000,1,1}));
    c.seed_p1_coalition("A",{{law_backers},{"WORKERS"},false});
    c.p3_seed_movement({education?"SCHOOL-REFORM":"WAGE-REFORM","A",
        education?"education_system":law_domain,education?"public_schools":law_choice,
        education?law_region:"",
        P3MovementGoal::legal_reform,{law_backers},{},900'000,100'000,true});

    auto& pop=c.seed_population();
    const auto e=pop.economic_profile({law_region,"worker","wage","employee",
                                        pop.default_education()});
    const auto demo=pop.demographic_profile({law_region,23700,phaseb::Sex::female,
        "trial-culture","trial-faith","citizen","free",pop.default_health(),
        phaseb::Service::civilian});
    if(!education)for(const auto id:law_worker_partitions) {
        const auto composition=campaign&&id==law_worker_partitions.front()?
            std::string(campaign_retail_household):"worker-"+std::to_string(id);
        const auto h=pop.household_profile({law_region,composition,"shared",1});
        // The campaign has no wage-trial manual hiring command. Leave its
        // comparison cohort inactive so the operating factory recruits from
        // its one funded, eligible vacancy cohort instead.
        const auto profile=campaign&&id!=law_worker_partitions.front()?
            pop.economic_profile({law_region,"not_assigned","inactive","ineligible",
                                  pop.default_education()}):e;
        const auto worker=pop.seed_population(demo,profile,h,1,id);
        if(campaign&&id==law_worker_partitions.front())
            pop.hire(worker,1,law_employer,"worker",3100,{});
        c.seed_actor("HH:"+std::to_string(h),0);
    }
    const auto political_household=pop.household_profile({law_region,"electorate","shared",1000});
    pop.seed_population(demo,e,political_household,1000,law_voter_partition);
    if(campaign) {
        c.seed_actor("B",0);c.seed_actor(campaign_employer_b,80'000);
        c.seed_actor(campaign_program_a,0);c.seed_actor(campaign_program_b,0);
        c.seed_government("B","PEER-HEAD","PEER-PM",5,3);
        c.seed_p1_constitution("B",{P1GovernancePrinciple::parliamentary_republic,
            P1PowerDistribution::universal_vote,P1LegislatureKind::unicameral,
            "campaign:peer-parliament"});
        c.p4_register_party({"B-MAJORITY","B","",800'000,true,true});
        c.p4_register_party({"B-OTHER","B","",200'000,true,true});
        c.p4_seed_contest({campaign_chamber_b,"B","national","campaign:peer-election",
            P4Office::legislature,P4BallotMethod::list_dhondt,P4TermMode::restart_after_poll,
            48,1'000'000,5,election,election,-1,true,true,false});
        const auto h=pop.household_profile({campaign_region_b,"peer-employees","shared",1});
        const auto emp=pop.economic_profile({campaign_region_b,"worker","wage","employee",
                                             pop.default_education()});
        const auto person=pop.demographic_profile({campaign_region_b,23700,phaseb::Sex::female,
            "trial-culture","trial-faith","citizen","free",pop.default_health(),phaseb::Service::civilian});
        const auto worker=pop.seed_population(person,emp,h,1,campaign_worker_b);
        pop.hire(worker,1,campaign_employer_b,"worker",3100,{});
        c.seed_actor("HH:"+std::to_string(h),0);
        const auto voters=pop.household_profile({campaign_region_b,"peer-electorate","shared",1000});
        pop.seed_population(person,emp,voters,1000);

        c.seed_actor(campaign_seller,0);c.seed_actor(campaign_carrier,0);
        c.seed_actor(campaign_buyer,40'000);
        c.seed_market(law_region,"ore",30,30,30);
        c.seed_market(campaign_region_b,"ore",10,10,10);
        c.seed_stock(campaign_seller,campaign_region_b,"ore",4000);
        engine.seed_verified_geographic_link(campaign_edge,2,1,"rail");
        phasegh::TransportEdge rail;
        rail.id=campaign_edge;rail.carrier=campaign_carrier;rail.from=2;rail.to=1;
        rail.mode="rail";rail.monthly_capacity=62;rail.freight_per_unit=2;rail.transit_days=1;
        rail.evidence_id="campaign:synthetic-shared-rail";rail.valid_from_month=24000;
        rail.valid_through_month=recovery?24000:24119;
        if(recovery)rail.monthly_capacity=31;
        engine.seed_transport_edge(rail);
        engine.seed_route({campaign_route,campaign_seller,campaign_buyer,"ore",2,1,62,1});
        if(recovery) {
            // Both endpoints are already present in the shared trial.
            // Only the alternate route has a new verified synthetic
            // connection. Nothing stocks the A smelter during the gap.
            c.seed_actor(recovery_carrier,0);
            engine.seed_verified_geographic_link(recovery_edge,2,1,"road");
            auto road=rail;
            road.id=recovery_edge;road.carrier=recovery_carrier;road.mode="road";
            road.freight_per_unit=5;road.transit_days=3;
            road.evidence_id="campaign:supply-recovery-road";
            road.valid_from_month=24002;road.valid_through_month=24119;
            engine.seed_transport_edge(road);
            const auto home=pop.household_profile({law_region,"independent-blacksmith","shared",1});
            const auto artisan=pop.seed_population(demo,e,home,1,recovery_artisan);
            c.seed_actor("HH:"+std::to_string(home),0);
            // Independent metal demand has cash from ordinary payroll even
            // through the shock, rather than being made to return on cue.
            pop.hire(artisan,1,law_employer,"worker",1550,{});
        }
        seed_public_build_fixture(c,"A",law_region,true,steel_chain);
        if(steel_chain) {
            // A catalog-bound coal mine and steel mill operate in the
            // existing A district. There is no finished steel at opening.
            industry.seed_standard_industries();
            c.seed_actor(steel_coal_owner,100);
            c.seed_market(law_region,"coal",steel_coal_price,steel_coal_price,
                          steel_coal_price,1);
            const auto title=std::string("mining_right:")+law_region+":coal";
            c.seed_shares(title,steel_coal_owner,1);
            geo.seed_building_in_province(steel_coal_site,steel_coal_owner,1,31,
                                          "vic3_115.rural.coal_mines",title);
            phased::ResourceSite coal;
            coal.id=steel_coal_source;coal.site=steel_coal_site;
            coal.owner=steel_coal_owner;coal.region=law_region;coal.good="coal";
            coal.monthly_capacity=31;
            industry.seed_building_resource_site(coal);
            const auto mill_owner=public_steel_seller("A");
            geo.seed_building_in_province(steel_mill_site,mill_owner,1,31,
                                          "vic3_115.urban.steel_mills");
            phased::IndustryRecipe steel;
            steel.id=steel_mill_method;steel.site=steel_mill_site;steel.industry_class="MANU";
            steel.inputs={{"iron",1},{"coal",1}};steel.outputs={{"steel",2}};
            steel.workers_per_batch=steel.capacity_per_batch=1;
            industry.seed_building_recipe(steel);
            for(const auto& [label,employer,partition]:
                std::array<std::tuple<std::string,std::string,phaseb::Id>,2>{{
                    {"coal-miner",steel_coal_owner,steel_coal_worker},
                    {"steelworker",mill_owner,steel_mill_worker},
                }}) {
                const auto home=pop.household_profile({law_region,label,"shared",1});
                const auto resident=pop.seed_population(demo,e,home,1,partition);
                c.seed_actor("HH:"+std::to_string(home),0);
                pop.hire(resident,1,employer,"worker",31,{});
            }
        }
        // Funded employment and forest biomass, but no pre-harvested wood.
        c.seed_actor(campaign_wood_seller,32);
        c.seed_production_facility(campaign_logger_site,campaign_wood_seller,law_region,31);
        phased::ResourceSite forest;
        forest.id=campaign_forest;forest.site=campaign_logger_site;
        forest.owner=campaign_wood_seller;forest.region=law_region;forest.good="wood";
        forest.monthly_capacity=31;forest.renewable=true;
        forest.standing_biomass=forest.carrying_capacity=12;forest.renewal_per_month=3;
        industry.seed_resource_site(forest);
        const auto logger_home=pop.household_profile({law_region,"logger-worker","shared",1});
        const auto logger_worker=pop.seed_population(demo,e,logger_home,1,campaign_logger_worker);
        c.seed_actor("HH:"+std::to_string(logger_home),0);
        pop.hire(logger_worker,1,campaign_wood_seller,"worker",31,{});
        c.seed_production_facility(campaign_smelter,campaign_buyer,law_region,31);
        phased::IndustryRecipe iron;
        iron.id=campaign_iron_method;iron.site=campaign_smelter;
        iron.inputs={{"ore",1}};iron.outputs={{"iron",1}};
        iron.workers_per_batch=iron.capacity_per_batch=1;
        industry.seed_recipe(iron);
        const auto smelter_home=pop.household_profile({law_region,"smelter-worker","shared",1});
        const auto smelter_worker=recovery?
            pop.seed_population(demo,e,smelter_home,1,recovery_smelter_worker):
            pop.seed_population(demo,e,smelter_home,1);
        c.seed_actor("HH:"+std::to_string(smelter_home),0);
        pop.hire(smelter_worker,1,campaign_buyer,"worker",31,{});
        const auto factory_home=pop.household_profile({law_region,"factory-workforce","shared",1});
        pop.seed_population(demo,e,factory_home,1,campaign_factory_worker);
        c.seed_actor("HH:"+std::to_string(factory_home),0);

        // One independent shop, one local board, and enough own cash for
        // one tool plus its operating wages. It never starts with any tools.
        c.seed_actor(campaign_tool_buyer,85);
        c.seed_market(law_region,campaign_boards,campaign_board_price,
                      campaign_board_price,campaign_board_price,2);
        c.seed_actor(campaign_board_vendor,24);
        c.seed_production_facility(campaign_mill_site,campaign_board_vendor,law_region,31);
        phased::IndustryRecipe saw;
        saw.id=campaign_mill_method;saw.site=campaign_mill_site;
        saw.inputs={{"wood",1}};saw.outputs={{campaign_boards,1}};
        saw.workers_per_batch=saw.capacity_per_batch=1;
        industry.seed_recipe(saw);
        const auto mill_home=pop.household_profile({law_region,"sawmill-worker","shared",1});
        const auto mill_worker=pop.seed_population(demo,e,mill_home,1,campaign_mill_worker);
        c.seed_actor("HH:"+std::to_string(mill_home),0);
        pop.hire(mill_worker,1,campaign_board_vendor,"worker",31,{});
        c.seed_stock(campaign_board_vendor,law_region,campaign_boards,1);
        c.seed_market(law_region,campaign_fittings,campaign_fitting_price,
                      campaign_fitting_price,campaign_fitting_price,1);
        c.seed_stock(campaign_tool_buyer,law_region,campaign_boards,1);
        c.seed_production_facility(campaign_tool_buyer_site,campaign_tool_buyer,law_region,31);
        phased::IndustryRecipe fittings;
        fittings.id=campaign_tool_buyer_recipe;fittings.site=campaign_tool_buyer_site;
        fittings.inputs={{"tools",1},{campaign_boards,1}};
        fittings.outputs={{campaign_fittings,1}};
        fittings.workers_per_batch=fittings.capacity_per_batch=1;
        industry.seed_recipe(fittings);
        const auto shop_home=pop.household_profile({law_region,"workshop-worker","shared",1});
        const auto shop_worker=pop.seed_population(demo,e,shop_home,1);
        c.seed_actor("HH:"+std::to_string(shop_home),0);
        pop.hire(shop_worker,1,campaign_tool_buyer,"worker",31,{});
    }
    if(education) {
        c.p1_set_incorporation(education_home,true);
        c.p1_set_incorporation(education_compare,false);
        c.p2_seed_base({"EDU-OPENING-LAW","A","education_system","national",
            "no_schools","trial:opening-school-statute"});
        c.seed_public_institution(education_school,law_employer,education_home,"paper",50,5);
        c.seed_p1_institution("A",education_school,P1InstitutionKind::education,2);
        for(const auto* region:{education_home,education_compare}) {
            const auto child_home=pop.household_profile({region,"children","shared",10'000});
            const auto youth=pop.economic_profile({region,"not_assigned","inactive",
                                                   "ineligible",pop.default_education()});
            // Avoid assuming the bill's future start month: each region has
            // comparable primary-age cohorts with one for each birth month.
            for(int birth=23904;birth<23916;++birth) {
                const auto d=pop.demographic_profile({region,birth,phaseb::Sex::female,
                    "trial-culture","trial-faith","citizen","free",pop.default_health(),
                    phaseb::Service::civilian});
                pop.seed_population(d,youth,child_home,10'000);
            }
        }
        const auto* workplace=trial==LegislativeTrial::education_remote?
            education_compare:education_home;
        const auto staff_home=pop.household_profile({workplace,"school-staff","shared",2});
        const auto staff_profile=pop.economic_profile({workplace,"worker","wage",
                                                      "employee",pop.default_education()});
        const auto mature=pop.demographic_profile({workplace,23700,phaseb::Sex::female,
            "trial-culture","trial-faith","citizen","free",pop.default_health(),
            phaseb::Service::civilian});
        const auto staff=pop.seed_population(mature,staff_profile,staff_home,2);
        pop.hire(staff,2,law_employer,"worker",31,{});
        c.seed_actor("HH:"+std::to_string(staff_home),0);
        phaseb::DemographyRules rates;
        rates.fertility_ppm=rates.infant_death_ppm=rates.child_death_ppm=0;
        rates.adult_death_ppm=rates.senior_death_ppm=rates.very_old_death_ppm=0;
        rates.illness_incidence_ppm=rates.accident_ppm=rates.training_ppm=0;
        rates.secondary_access_ppm=rates.tertiary_access_ppm=rates.adult_literacy_access_ppm=0;
        rates.primary_access_ppm=310'000;
        c.enable_demography(rates);
    }
    return daily::World::adopt(std::move(engine),31,legislative_origin(trial));
}

bool is_education_world(const daily::World& w) {
    return w.migration_origin()==education_origin||w.migration_origin()==education_remote_origin;
}
bool is_campaign_world(const daily::World& w) {
    return w.migration_origin()==legacy_campaign_origin||
           w.migration_origin()==campaign_procurement_origin||
           w.migration_origin()==campaign_ore_inventory_origin||
           is_supply_recovery(w)||is_steel_campaign(w);
}
bool is_law_world(const daily::World& w) {
    return w.migration_origin()==law_origin||is_education_world(w)||is_campaign_world(w);
}


daily::World open_law(const std::optional<fs::path>& resume,LegislativeTrial trial) {
    auto w=resume?load_panel_checkpoint(*resume):law_world(trial);
    const auto& c=w.engine().base().core().core();
    const auto valid_worker=[&](auto partition) {
        const auto id=law_slice(c,partition);
        return c.people().slice(id).count==1&&c.has_actor(trial_worker_wallet(c,id));
    };
    // Preserve the rule set materialized by an existing ordinary campaign
    // checkpoint. Do not migrate its origin or reprovision its supply chain.
    const auto& origin=w.migration_origin();
    const bool origin_matches=origin==legislative_origin(trial)||
        (trial==LegislativeTrial::campaign&&
         (origin==legacy_campaign_origin||origin==campaign_procurement_origin));
    if(!origin_matches||w.engine().base().spatial().dataset_hash!=origin||
       !c.has_country_treasury("A")||!c.has_actor(law_employer)||
       c.p1_constitution("A").legislature!=phasec::P1LegislatureKind::unicameral||
       !c.p4_contests().contains(law_chamber)||c.p4_contest(law_chamber).country!="A"||
       c.people().slice(law_slice(c,law_voter_partition)).count!=1000||
       (!is_education_world(w)&&
        !std::all_of(law_worker_partitions.begin(),law_worker_partitions.end(),valid_worker))||
       (is_education_world(w)&&!c.public_institutions().contains(education_school)))
        throw std::invalid_argument("checkpoint does not belong to the elected law trial");
    if(is_campaign_world(w)) {
        const auto& routes=w.engine().commerce().routes;
        const auto route=routes.find(campaign_route);
        if(!c.has_country_treasury("B")||!c.p1_is_player_country("A")||
           c.p1_is_player_country("B")||!c.p4_contests().contains(campaign_chamber_b)||
           c.p4_contest(campaign_chamber_b).country!="B"||route==routes.end()||
           route->second.origin!=2||route->second.destination!=1||
           route->second.seller!=campaign_seller||route->second.buyer!=campaign_buyer||
           route->second.good!="ore"||!c.has_actor(campaign_carrier)||
           !c.has_actor(campaign_program_a)||!c.has_actor(campaign_program_b)||
           !c.has_actor(trial_worker_wallet(c,law_slice(c,campaign_worker_b,campaign_region_b))))
            throw std::invalid_argument("checkpoint lacks the shared campaign institutions");
        if(is_supply_recovery(w)) {
            const auto& edges=w.engine().commerce().transport_edges;
            const auto road=edges.find(recovery_edge),rail=edges.find(campaign_edge);
            if(road==edges.end()||rail==edges.end()||
               road->second.from!=2||road->second.to!=1||
               road->second.mode!="road"||road->second.carrier!=recovery_carrier||
               road->second.transit_days!=3||!c.has_actor(recovery_carrier)||
               !valid_worker(recovery_artisan)||!valid_worker(recovery_smelter_worker))
                throw std::invalid_argument("checkpoint lacks the recovery route or artisan");
        }
        if(is_steel_campaign(w)) {
            const auto& d=w.engine().base().core();
            const auto& stocks=d.state().resource_sites;
            const auto source=stocks.find(steel_coal_source);
            const auto& recipes=d.state().recipes;
            const auto method=recipes.find(steel_mill_method);
            if(source==stocks.end()||source->second.owner!=steel_coal_owner||
               source->second.good!="coal"||source->second.site!=steel_coal_site||
               source->second.region!=law_region||
               method==recipes.end()||method->second.site!=steel_mill_site||
               !has_public_build_fixture(w,"A",law_region)||
               !c.has_actor(steel_coal_owner)||!c.facilities().contains(steel_mill_site)||
               !valid_worker(steel_coal_worker)||!valid_worker(steel_mill_worker)||
               !valid_worker(steel_builder_worker))
                throw std::invalid_argument("checkpoint lacks its coal mine or steel mill");
        }
    }
    return w;
}

void supply_recovery_status(std::ostringstream& out,const daily::World& world) {
    const auto& engine=world.engine();
    const auto& industry=engine.base().core();
    const auto& c=industry.core();
    const auto& trade=engine.commerce();
    const auto date=world.date();
    const auto month=static_cast<phasegh::Month>(date.year)*12+date.month-1;
    const auto operating=[month](const phasegh::TransportEdge& e) {
        return e.enabled&&e.valid_from_month<=month&&month<=e.valid_through_month;
    };
    const auto& rail=trade.transport_edges.at(campaign_edge);
    const auto& road=trade.transport_edges.at(recovery_edge);
    Quantity road_ore=0,iron=0;
    for(const auto& [id,cargo]:trade.daily_shipments)
        if(cargo.buyer==campaign_buyer&&cargo.good=="ore"&&
           cargo.destination==law_region&&!cargo.path_edges.empty()&&
           cargo.path_edges.front()==recovery_edge)
            road_ore=phaseb::add_checked(road_ore,cargo.quantity);
    for(const auto& run:industry.state().actual_production_runs)
        if(run.recipe==campaign_iron_method)
            iron=phaseb::add_checked(iron,run.batches);
    const auto cash=[&](phaseb::Id partition) {
        return c.cash(trial_worker_wallet(c,law_slice(c,partition)));
    };
    out<<R"(,"supply_recovery":{"route":)"
       <<json_string(operating(rail)?"rail":operating(road)?"road":"closed")
       <<R"(,"road_in_transit_ore":)"<<road_ore
       <<R"(,"road_freight_paid":)"<<c.cash(recovery_carrier)
       <<R"(,"smelter_receipts":)"<<c.firm_finance(campaign_buyer).receipts
       <<R"(,"iron_batches":)"<<iron
       <<R"(,"ore_stock":)"<<c.stock(campaign_buyer,law_region,"ore")
       <<R"(,"artisan_cash":)"<<cash(recovery_artisan)
       <<R"(,"smelter_worker_cash":)"<<cash(recovery_smelter_worker)<<'}';
}

void steel_chain_status(std::ostringstream& out,const daily::World& world) {
    const auto& industry=world.engine().base().core();
    const auto& d=industry.state();
    const auto& c=industry.core();
    const auto mill=public_steel_seller("A");
    const auto& recipe=d.recipes.at(steel_mill_method);
    Quantity batches=0;
    for(const auto& run:d.actual_production_runs)
        if(run.recipe==recipe.id)batches=phaseb::add_checked(batches,run.batches);
    const auto work=[&](std::string_view good,const std::vector<phased::Input>& goods) {
        Quantity used=0;
        for(const auto& row:goods)
            if(row.good==good)used=phaseb::add_checked(used,row.per_unit);
        return phaseb::mul_checked(batches,used);
    };
    const auto worker_cash=[&](phaseb::Id partition) {
        return c.cash(trial_worker_wallet(c,law_slice(c,partition)));
    };
    out<<R"(,"steel_chain":{"coal_mined":)"<<d.resource_sites.at(steel_coal_source).cumulative_output
       <<R"(,"coal_stock":)"<<c.stock(steel_coal_owner,law_region,"coal")
       <<R"(,"coal_receipts":)"<<c.firm_finance(steel_coal_owner).receipts
       <<R"(,"coal_miner_cash":)"<<worker_cash(steel_coal_worker)
       <<R"(,"mill_coal_stock":)"<<c.stock(mill,law_region,"coal")
       <<R"(,"mill_iron_stock":)"<<c.stock(mill,law_region,"iron")
       <<R"(,"mill_coal_used":)"<<work("coal",recipe.inputs)
       <<R"(,"mill_iron_used":)"<<work("iron",recipe.inputs)
       <<R"(,"mill_worker_cash":)"<<worker_cash(steel_mill_worker)
       <<R"(,"builder_worker_cash":)"<<worker_cash(steel_builder_worker)
       <<R"(,"steel_batches":)"<<batches
       <<R"(,"steel_produced":)"<<work("steel",recipe.outputs)
       <<R"(,"steel_stock":)"<<c.stock(mill,law_region,"steel")
       <<R"(,"steel_receipts":)"<<c.firm_finance(mill).receipts<<'}';
}

void campaign_supply_status(std::ostringstream& out,const daily::World& world) {
    if(!has_campaign_supply(world))return;
    const auto& domain=world.engine().base().core();
    const auto& c=domain.core();
    const auto& factory=public_offer("A",law_region,true);
    const auto& factory_id=factory.project.target_site;
    Quantity iron_batches=0,tools_batches=0,tool_buyer_batches=0,mill_batches=0;
    for(const auto& run:domain.state().actual_production_runs) {
        if(run.recipe==campaign_iron_method)
            iron_batches=phaseb::add_checked(iron_batches,run.batches);
        else if(run.recipe==factory.recipe.id)
            tools_batches=phaseb::add_checked(tools_batches,run.batches);
        else if(run.recipe==campaign_tool_buyer_recipe)
            tool_buyer_batches=phaseb::add_checked(tool_buyer_batches,run.batches);
        else if(run.recipe==campaign_mill_method)
            mill_batches=phaseb::add_checked(mill_batches,run.batches);
    }
    const auto known_material=[&](const std::string& good) {
        if(c.stock("A",law_region,good)>0)return true;
        return std::any_of(c.physical_inventory().begin(),c.physical_inventory().end(),
            [&](const auto& item) {
                const auto& key=item.first;
                return key.region_id==law_region&&key.good_id==good&&
                       item.second>0&&c.has_actor(key.holder_id);
            });
    };
    const auto reason=!c.facilities().contains(factory_id)?"construction":
                      c.phase_d_workforce("A")<=0?"staff":
                      !known_material("iron")?"iron":!known_material("wood")?"wood":"none";
    const auto worker=law_slice(c,campaign_factory_worker);
    out<<R"(,"supply_chain":{"ore_stock":)"<<c.stock(campaign_buyer,law_region,"ore")
       <<R"(,"iron_stock":)"<<c.stock(campaign_buyer,law_region,"iron")
       <<R"(,"factory_iron_stock":)"<<c.stock("A",law_region,"iron")
       <<R"(,"factory_wood_stock":)"<<c.stock("A",law_region,"wood")
       <<R"(,"tools_stock":)"<<c.stock("A",law_region,"tools")
       <<R"(,"wood_seller_stock":)"<<c.stock(campaign_wood_seller,law_region,"wood")
       <<R"(,"wood_seller_cash":)"<<c.cash(campaign_wood_seller)
       <<R"(,"smelter_cash":)"<<c.cash(campaign_buyer)
       <<R"(,"smelter_receipts":)"<<c.firm_finance(campaign_buyer).receipts
       <<R"(,"factory_worker_cash":)"<<c.cash(trial_worker_wallet(c,worker))
       <<R"(,"iron_batches":)"<<iron_batches<<R"(,"tools_batches":)"<<tools_batches
       <<R"(,"tools_waiting_for":)"<<json_string(reason);
    if(has_campaign_workshop(world)) {
        const auto shop_tools=c.stock(campaign_tool_buyer,law_region,"tools");
        // The authored shop starts with no tools and can only buy this
        // maker's output. Its sole recipe consumes one tool per actual run.
        out<<R"(,"tools_sold":)"<<phaseb::add_checked(shop_tools,tool_buyer_batches)
           <<R"(,"tool_buyer_cash":)"<<c.cash(campaign_tool_buyer)
           <<R"(,"tool_buyer_tools_stock":)"<<shop_tools
           <<R"(,"tool_buyer_output_stock":)"<<c.stock(campaign_tool_buyer,law_region,campaign_fittings)
           <<R"(,"tool_buyer_batches":)"<<tool_buyer_batches;
        if(campaign_household_order(world)) {
            // This shop makes only fittings and the authored household is
            // their sole buyer. Actual receipts at the immutable unit quote
            // give the count even after Stage7 consumed the delivered stock.
            const auto receipts=c.firm_finance(campaign_tool_buyer).receipts;
            out<<R"(,"household_fittings_bought":)"<<receipts/campaign_fitting_price
               <<R"(,"fittings_sales_receipts":)"<<receipts;
        }
        if(has_campaign_board_vendor(world)) {
            const auto board_stock=c.stock(campaign_tool_buyer,law_region,campaign_boards);
            // The workshop starts with one board, buys its only replacements
            // from this mill, and consumes one per fitting. Raw timber resale
            // is not a board receipt; report only the reconciled board units.
            const auto board_receipts=has_campaign_timber(world)?
                phaseb::mul_checked(phaseb::add_checked(board_stock,tool_buyer_batches)-1,
                                    campaign_board_price):
                c.firm_finance(campaign_board_vendor).receipts;
            out<<R"(,"board_vendor_stock":)"<<c.stock(campaign_board_vendor,law_region,campaign_boards)
               <<R"(,"board_vendor_receipts":)"<<board_receipts
               <<R"(,"workshop_board_stock":)"<<board_stock;
        }
    }
    if(has_campaign_timber(world)) {
        // No opening wood and the named logger is the only wood source.
        // Each mill/tool batch used one unit; the rest is current paid
        // stock, net of any resale rather than a gross market history.
        const auto paid_wages=[&](phaseb::Id partition) {
            return c.cash(trial_worker_wallet(c,law_slice(c,partition)));
        };
        out<<R"(,"logs_harvested":)"<<domain.state().resource_sites.at(campaign_forest).cumulative_output
           <<R"(,"logger_receipts":)"<<c.firm_finance(campaign_wood_seller).receipts
           <<R"(,"logger_worker_cash":)"<<paid_wages(campaign_logger_worker)
           <<R"(,"mill_worker_cash":)"<<paid_wages(campaign_mill_worker)
           <<R"(,"mill_boards_produced":)"<<mill_batches
           <<R"(,"mill_wood_bought":)"<<phaseb::add_checked(mill_batches,c.stock(campaign_board_vendor,law_region,"wood"))
           <<R"(,"toolworks_wood_bought":)"<<phaseb::add_checked(tools_batches,c.stock("A",law_region,"wood"));
    }
    out<<'}';
}

std::string day_label(std::int64_t day) {
    const auto date=daily::from_day_index(day);
    std::ostringstream out;
    out<<std::setfill('0')<<std::setw(4)<<date.year<<'-'<<std::setw(2)<<date.month
       <<'-'<<std::setw(2)<<date.day;
    return out.str();
}

void law_status(std::ostringstream& out,const phasec::IntegratedWorld& c,bool education,
                const char* scenario=nullptr) {
    using phasec::P2BillStatus;
    const auto* bill_id=education?education_bill:law_bill;
    const auto bill=c.p2_bills().find(bill_id);
    const auto process=c.p2_enactments().find(bill_id);
    const auto result=c.country_elections().find("A");
    const auto phase=[&] {
        if(bill==c.p2_bills().end())return "available";
        switch(bill->second.status) {
            case P2BillStatus::submitted:return "submitted";
            case P2BillStatus::approved:return "approved";
            case P2BillStatus::promulgated:return "promulgated";
            case P2BillStatus::effective:return "effective";
            case P2BillStatus::rejected:return "rejected";
        }
        return "rejected";
    }();
    const bool ready=bill!=c.p2_bills().end()&&
        bill->second.status==P2BillStatus::submitted&&c.p2_enactment_ready(bill_id)&&
        !c.p4_parliament_dissolved("A")&&result!=c.country_elections().end();
    const auto next=process==c.p2_enactments().end()?-1:process->second.next_review_day;
    const auto effective=bill==c.p2_bills().end()?-1:bill->second.effective_day;
    out<<",\"scenario\":"<<json_string(scenario?scenario:education?education_trial:law_trial);
    if(!education)
        out<<",\"minimum_monthly_contract_wage\":"<<c.p2_rule("A","A",
            phasec::P2ClauseKind::minimum_monthly_contract_wage).value_or(0)
           <<",\"employer_cash\":"<<c.cash(law_employer);
    out       <<",\"law_bill\":{\"phase\":"<<json_string(phase)
       <<",\"vote_ready\":"<<(ready?"true":"false")
       <<",\"next_review\":"<<(next<0?"null":json_string(day_label(next)))
       <<",\"effective_date\":"<<(effective<0?"null":json_string(day_label(effective)))
       <<",\"threshold\":"<<c.government("A").quorum<<",\"seats\":[";
    bool first=true;
    if(result!=c.country_elections().end())
        for(const auto& [party,seats]:result->second.seats) {
            if(!first)out<<',';
            first=false;
            out<<"{\"party\":"<<json_string(party)<<",\"seats\":"<<seats<<'}';
        }
    out<<"]}";
    if(education) {
        People primary=0,comparison=0;
        phasec::Money staff_cash=0;
        std::set<std::string> staff_wallets;
        for(const auto& [id,row]:c.people().slices()) {
            const auto& pop=c.people();
            const auto& d=pop.demographic(row.key.demographic);
            if(d.birth_month>=23904&&d.birth_month<23916&&
               pop.education(pop.economic(row.key.economic).education).school>=phaseb::School::primary) {
                if(d.region==education_home)primary+=row.count;
                else if(d.region==education_compare)comparison+=row.count;
            }
            if(const auto* job=pop.job(id);job&&job->employer==law_employer)
                staff_wallets.insert(trial_worker_wallet(c,id));
        }
        for(const auto& wallet:staff_wallets)staff_cash+=c.cash(wallet);
        const auto capacity=c.p1_institution_capacity(education_school);
        out<<",\"education\":{\"usable_slots\":"<<capacity.usable_slots
           <<",\"local_staff\":"<<capacity.employed_staff
           <<",\"school_primary\":"<<primary<<",\"comparison_primary\":"<<comparison
           <<",\"operator_cash\":"<<c.cash(law_employer)<<",\"staff_cash\":"<<staff_cash<<"}}";
        return;
    }
    out<<",\"workers\":[";
    first=true;
    for(const auto partition:law_worker_partitions) {
        const auto worker=law_slice(c,partition);
        if(!first)out<<',';
        first=false;
        const auto* job=c.people().job(worker);
        out<<"{\"contract_wage\":"<<(job?std::to_string(job->wage_per_month):"null")
           <<",\"cash\":"<<c.cash(trial_worker_wallet(c,worker))<<'}';
    }
    out<<"]}";
}

void advance_panel_day(p0::DailyGame& game,phasegh::World::Plan plan={}) {
    // Opening association is authored scenario setup using actual POP
    // membership. A player bill never creates or changes its own supporters.
    const bool opening=game.world().date()==daily::Date{2000,1,1};
    if(opening&&is_campaign_world(game.world())) {
        // Opening scenario financing, not a future AI policy decision. The
        // ordinary B provider spends the real deposit under the same budget.
        plan[1].insert(plan[1].begin(),[](phasegh::Stage& stage) {
            stage.phase_c().borrow_country_budget_from_bank("B",100);
        });
    }
    if(opening&&is_law_world(game.world()))
        plan[9].insert(plan[9].begin(),[](phasegh::Stage& stage) {
            auto& c=stage.phase_c();
            const auto cohort=law_slice(c,law_voter_partition);
            c.join_interest_group(cohort,law_backers,c.people().slice(cohort).count);
        });
    game.advance_day(plan);
}

void law_action(p0::DailyGame& game,std::string_view command,std::string_view arg) {
    if(!is_law_world(game.world()))
        throw std::invalid_argument("this command requires the elected law trial");
    phasegh::World::Plan plan;
    const auto today=game.world().day_index();
    const bool education=is_education_world(game.world());
    if(command=="law-hire") {
        if(education||is_campaign_world(game.world()))
            throw std::invalid_argument("law-hire is a wage trial fixture only");
        if(!arg.empty())throw std::invalid_argument("law-hire takes no arguments");
        // Protocol regression fixture only, not a strategy control.
        const auto& opening=game.world().engine().base().core().core();
        const auto found=std::find_if(law_worker_partitions.begin(),law_worker_partitions.end(),
            [&](auto partition){return opening.people().job(law_slice(opening,partition))==nullptr;});
        if(found==law_worker_partitions.end())
            throw std::invalid_argument("all fixture workers have jobs");
        const auto worker=law_slice(opening,*found);
        const auto date=game.world().date();
        const int days=daily::days_in_month(date.year,date.month);
        plan[2].push_back([worker,date,days](phasegh::Stage& stage) {
            auto& c=stage.phase_c();
            const auto floor=c.p2_rule("A","A",
                phasec::P2ClauseKind::minimum_monthly_contract_wage).value_or(0);
            const auto wage=std::max(opening_contract_wage,floor);
            const auto due=daily_budget::increment(wage,date.day,days);
            if(due<=0||c.phase_d_cash_available(law_employer)<due)
                throw std::invalid_argument("fixture employer cannot fund the new contract");
            c.hire(worker,1,law_employer,wage);
        });
        // P0 Stage5 discovers today's new contracts and settles them once.
    } else {
        if((command=="bill-approve")==arg.empty()||arg.find_first_of(" \t")!=arg.npos)
            throw std::invalid_argument("bill approval requires one elected party");
        plan[9].push_back([command=std::string(command),arg=std::string(arg),today,education](phasegh::Stage& stage) {
            using namespace phasec;
            auto& c=stage.phase_c();
            if(!c.country_elections().contains("A")||c.p4_parliament_dissolved("A"))
                throw std::invalid_argument("the elected legislature is not in office");
            const auto* bill_id=education?education_bill:law_bill;
            if(command=="bill-introduce") {
                P2Bill proposal;
                proposal.id=bill_id;proposal.country="A";
                proposal.proposer=c.government("A").prime_minister;
                proposal.legal_basis="trial:ordinary-parliamentary-bill";
                if(education)
                    proposal.proposed_base={"EDU-PUBLIC-STATUTE","A","education_system",
                        "national","public_schools","trial:public-school-statute"};
                else proposal.proposed_base={"LAW-WAGE-STATUTE","A",law_domain,"national",law_choice,
                    "trial:minimum-monthly-contract-wage",P2NormTier::statute,
                    {{"contract-minimum","A",P2ClauseKind::minimum_monthly_contract_wage,proposed_floor}}};
                c.p2_submit_bill(std::move(proposal),today);
            } else if(command=="bill-approve") {
                c.p2_approve_bill(bill_id,{arg});
            } else {
                c.p2_promulgate_bill(bill_id,c.government("A").head_of_state,today);
            }
        });
    }
    advance_panel_day(game,std::move(plan));
}

void public_build_status(std::ostringstream& out,const p0::DailyGame& game,
        const std::string& tag,bool vote_allowed) {
    const auto& engine=game.world().engine();
    const auto& core=engine.base().core().core();
    if(const auto* offer=find_public_offer(game,tag)) {
        const auto& p=offer->project;
        const auto& projects=engine.base().core().state().constructions;
        const auto it=projects.find(p.id);
        const auto* started=it==projects.end()?nullptr:&it->second;
        const auto capacity=started&&started->commissioned
            ?core.facility(p.target_site).capacity:0;
        const auto steel=phaseb::mul_checked(p.work_required,p.material_per_work);
        out<<",\"public_construction\":{\"state\":"
           <<json_string(public_construction_state(started))
           <<",\"invoice\":"<<phaseb::mul_checked(p.work_required,p.cash_price_per_work)
           <<",\"treasury_reserve\":"<<offer->treasury_cash_reserve
           <<",\"steel_required\":"<<steel
           <<",\"steel_purchase_cost\":"<<phaseb::mul_checked(
                steel,core.market(p.region,p.material_good).price)
           <<",\"contractor_steel\":"<<core.stock(p.contractor,p.region,p.material_good)
           <<",\"supplier_receipts\":"<<core.firm_finance(public_steel_seller(tag)).receipts
           <<",\"contractor_receipts\":"<<core.firm_finance(p.contractor).receipts
           <<",\"paid_work\":"<<(started?started->paid_work:0)
           <<",\"invoice_paid\":"<<phaseb::mul_checked(started?started->paid_work:0,
                    p.cash_price_per_work)
           <<",\"commissioned_capacity\":"<<capacity;
        if(offer->elected_appropriation) {
            const bool approved=game.has_public_construction_budget(tag,p.id);
            out<<",\"budget_approval\":"<<json_string(approved?"approved":"available")
               <<",\"budget_vote_allowed\":"<<(!approved&&vote_allowed?"true":"false");
        }
        out<<'}';
    }
}

std::string panel_status(const p0::DailyGame& game,const std::string& tag) {
    const auto& world=game.world();
    const auto& engine=world.engine();
    const auto& core=engine.base().core().core();
    const auto fiscal=core.country_fiscal(tag);
    const auto date=world.date();
    std::ostringstream out;
    out<<"{\"country\":"<<json_string(tag)<<",\"date\":\""
       <<std::setfill('0')<<std::setw(4)<<date.year<<'-'<<std::setw(2)<<date.month
       <<'-'<<std::setw(2)<<date.day<<"\""
       <<",\"commit\":"<<engine.commit_id()
       <<",\"treasury\":"<<fiscal.cash
       <<",\"revenue\":"<<fiscal.realized_revenue
       <<",\"expenditure\":"<<fiscal.realized_expenditure
       <<",\"debt\":"<<phaseb::add_checked(fiscal.bank_debt,
            phaseb::add_checked(fiscal.bond_principal,fiscal.unpaid_coupons));
    if(is_staples_world(world)) {
        staples_status(out,world);
        return out.str();
    }
    if(is_expansion_world(world)) {
        expansion_status(out,world);
        return out.str();
    }
    if(is_campaign_world(world)) {
        const std::string other(other_country(tag));
        const auto sibling=core.country_fiscal(other);
        const auto pending=core.pending_country_tax(tag);
        const auto sibling_pending=core.pending_country_tax(other);
        const auto ready=[&] {
            if(tag!="A"||core.p4_parliament_dissolved("A"))return false;
            const auto actual=core.country_elections().find("A");
            if(actual==core.country_elections().end())return false;
            const auto quorum=core.government("A").quorum;
            return std::any_of(actual->second.seats.begin(),actual->second.seats.end(),
                [&](const auto& party){return party.second>=quorum;});
        }();
        Quantity transit=0;
        for(const auto& [id,cargo]:engine.commerce().daily_shipments)
            if(cargo.buyer==campaign_buyer&&cargo.destination==law_region&&cargo.good=="ore")
                transit=phaseb::add_checked(transit,cargo.quantity);
        const auto household_cash=[&](const std::string& nation) {
            const auto id=nation=="A"?law_slice(core,law_worker_partitions.front()):
                                      law_slice(core,campaign_worker_b,campaign_region_b);
            return core.cash(trial_worker_wallet(core,id));
        };
        out<<",\"player_country\":\"A\",\"can_act\":"<<(tag=="A"?"true":"false")
           <<",\"campaign_tax_allowed\":"<<(ready&&!pending?"true":"false")
           <<",\"income_tax_ppm\":"<<fiscal.income_tax_ppm
           <<",\"pending_income_tax_ppm\":"<<(pending?std::to_string(pending->second):"null")
           <<",\"pending_income_tax_date\":"<<(pending?json_string(month_opening(pending->first)):"null")
           <<",\"household_cash\":"<<household_cash(tag)
           <<",\"peer_country\":"<<json_string(other)
           <<",\"peer_is_player\":"<<(core.p1_is_player_country(other)?"true":"false")
           <<",\"peer_treasury\":"<<sibling.cash<<",\"peer_revenue\":"<<sibling.realized_revenue
           <<",\"peer_debt\":"<<phaseb::add_checked(sibling.bank_debt,
               phaseb::add_checked(sibling.bond_principal,sibling.unpaid_coupons))
           <<",\"peer_household_cash\":"<<household_cash(other)
           <<",\"peer_income_tax_ppm\":"<<sibling.income_tax_ppm
           <<",\"peer_pending_income_tax_ppm\":"
           <<(sibling_pending?std::to_string(sibling_pending->second):"null")
           <<",\"peer_pending_income_tax_date\":"
           <<(sibling_pending?json_string(month_opening(sibling_pending->first)):"null")
           <<",\"public_program_paid\":"<<core.cash(tag=="A"?campaign_program_a:campaign_program_b)
           <<",\"peer_public_program_paid\":"<<core.cash(other=="A"?campaign_program_a:campaign_program_b)
           <<",\"delivered_ore\":"<<core.stock(campaign_buyer,law_region,"ore")
           <<",\"in_transit_ore\":"<<transit
           <<",\"supplier_cash\":"<<core.cash(campaign_seller)
           <<",\"carrier_cash\":"<<core.cash(campaign_carrier);
        if(is_supply_recovery(world))supply_recovery_status(out,world);
        if(is_steel_campaign(world))steel_chain_status(out,world);
        if(tag=="A") {
            campaign_supply_status(out,world);
            public_build_status(out,game,tag,ready);
            law_status(out,core,false,campaign_scenario);
        }
        else out<<",\"scenario\":"<<json_string(campaign_scenario)<<'}';
        return out.str();
    }
    if(is_law_world(world)) {
        law_status(out,core,is_education_world(world));
        return out.str();
    }
    if(tag==country) {
        const auto& industry=engine.base().core().state();
        Quantity per_batch=0,batches=0;
        for(const auto& input:industry.recipes.at(mill_recipe).inputs)
            if(input.good=="ore")per_batch=phaseb::add_checked(per_batch,input.per_unit);
        for(const auto& run:industry.actual_production_runs)
            if(run.recipe==mill_recipe)batches=phaseb::add_checked(batches,run.batches);
        if(per_batch<=0)throw std::invalid_argument("industry trial requires an ore input");
        const auto ore=core.stock(producer,region,"ore");
        out<<",\"scenario\":"<<json_string(industry_scenario)
           <<",\"ore_stock\":"<<ore<<",\"ore_per_batch\":"<<per_batch
           <<",\"food_stock\":"<<core.stock(producer,region,"food")
           <<",\"production_batches\":"<<batches
           <<",\"ore_insufficient\":"<<(ore<per_batch?"true":"false")
           <<",\"public_works_cash\":"<<core.cash(works)<<'}';
        return out.str();
    }
    if(budget_world(world)) {
        const auto cap=core.civilian_spending_cap(tag);
        const auto change=core.pending_civilian_spending_cap(tag);
        out<<",\"scenario\":"<<json_string(budget_scenario)
           <<",\"civilian_spending_cap\":"<<(cap?std::to_string(*cap):"null")
           <<",\"pending_civilian_spending_cap\":"<<(change?std::to_string(change->second):"null")
           <<",\"pending_civilian_spending_date\":"<<(change?json_string(month_opening(change->first)):"null")
           <<",\"public_program_paid\":"<<program_receipts(core,tag)
           <<",\"peer_public_program_paid\":"<<program_receipts(core,std::string(other_country(tag)));
        public_build_status(out,game,tag,false);

    }
    const auto& commerce=engine.commerce();
    const auto& tariffs=engine.diplomacy().tariffs;
    const auto region=trial_region(tag);
    const auto cap=core.p2_rule(tag,tag,
        phasec::P2ClauseKind::maximum_import_tariff_ppm).value_or(1'000'000);
    const auto rate=tariffs.find({tag,trial_good});
    const auto pending=core.pending_country_tax(tag);
    const auto law=core.p2_laws().find("TRIAL-CUSTOMS-"+tag);
    const auto law_label=law!=core.p2_laws().end()&&core.p2_is_active_base(law->first)
        ?law->first:"";
    phasec::Money duty=0;
    for(const auto& receipt:commerce.customs)
        if(receipt.importer==tag&&receipt.good==trial_good)
            duty=phaseb::add_checked(duty,receipt.paid);
    Quantity transit=0;
    for(const auto& [id,cargo]:commerce.daily_shipments)
        if(cargo.buyer==trial_buyer&&cargo.destination==region&&cargo.good==trial_good)
            transit=phaseb::add_checked(transit,cargo.quantity);
    out<<",\"tariff_ppm\":"<<(rate==tariffs.end()?0:rate->second.ppm)
       <<",\"max_tariff_ppm\":"<<cap
       <<",\"income_tax_ppm\":"<<fiscal.income_tax_ppm
       <<",\"pending_income_tax_ppm\":"<<(pending?std::to_string(pending->second):"null")
       <<",\"pending_income_tax_date\":"<<(pending?json_string(month_opening(pending->first)):"null")
       <<",\"law\":"<<json_string(law_label)
       <<",\"ore_price\":"<<core.market(region,trial_good).price
       <<",\"paid_duty\":"<<duty
       <<",\"delivered_ore\":"<<core.stock(trial_buyer,region,trial_good)
       <<",\"in_transit_ore\":"<<transit<<",\"wars\":[";
    bool first=true;
    for(const auto& [id,war]:engine.diplomacy().wars) {
        if((!war.active&&!war.armistice)||(war.attacker!=tag&&war.defender!=tag))continue;
        if(!first)out<<',';
        first=false;
        out<<"{\"opponent\":"<<json_string(war.attacker==tag?war.defender:war.attacker)
           <<",\"state\":"<<json_string(war.armistice?"armistice":"active")<<'}';
    }
    out<<"]}";
    return out.str();
}

std::pair<std::string_view,std::string_view> panel_token(std::string_view line) {
    const auto split=line.find_first_of(" \t");
    return {line.substr(0,split),split==line.npos?std::string_view{}:trim(line.substr(split+1))};
}

// Both Play stdin protocols carry checkpoint paths as UTF-8.
fs::path checkpoint_path_utf8(std::string_view value) {
    if(value.empty()||value.find('\0')!=value.npos)
        throw std::invalid_argument("enter a nonempty checkpoint path");
    return fs::path(std::u8string(value.begin(),value.end()));
}

void run_panel() {
    struct Session {
        std::string tag;
        p0::DailyGame game;
    };
    std::unique_ptr<Session> session;
    std::string last_status="null";
    constexpr std::size_t max_line=32768;
    std::string input;
    for(;;) {
        input.clear();
        bool too_long=false;
        int ch;
        while((ch=std::cin.get())!=EOF&&ch!='\n') {
            if(input.size()<max_line)input+=static_cast<char>(ch);
            else too_long=true;
        }
        if(ch==EOF&&input.empty())break;
        bool quit=false;
        try {
            if(too_long)throw std::invalid_argument("panel command exceeds line limit");
            const auto [command,arg]=panel_token(trim(input));
            if(command=="quit"&&arg.empty())quit=true;
            else if(command=="status"&&arg.empty()) {}
            else if(command=="inspect"&&(arg=="A"||arg=="B")) {
                if(!session||!is_campaign_world(session->game.world()))
                    throw std::invalid_argument("inspection is available only in the shared campaign");
                last_status=panel_status(session->game,std::string(arg));
                session->tag=arg;
            } else if(command=="new"||command=="load") {
                const auto [choice,path]=panel_token(arg);
                const bool budget=choice=="BUDGET-A"||choice=="BUDGET-B";
                const auto law=legislative_trial(choice);
                const bool expansion=choice=="EXPANSION-A";
                const bool staples=choice=="STAPLES-A";
                const auto tag=law||expansion||staples?std::string_view("A"):budget?choice.substr(7):choice;
                if((tag!="A"&&tag!="B"&&tag!=country)||(command=="new"&&!path.empty()))
                    throw std::invalid_argument("select A, B, TUT, BUDGET-A, BUDGET-B, LAW-A, EDU-A, CAMPAIGN-A, SUPPLY-A, EXPANSION-A, STAPLES-A or STEEL-A");
                const auto resume=command=="load"?std::optional{checkpoint_path_utf8(path)}:std::nullopt;
                const bool industry=tag==country;
                auto next=staples?open_staples(resume):expansion?open_expansion(resume):law?open_law(resume,*law):industry?open_industry(resume):
                    open_trial(std::string(tag),resume,budget);
                auto policy=staples?staples_policies():expansion?expansion_policies():law?law_policies(next,*law):industry?tutorial_policies(true):
                    trial_policies(next,std::string(tag),budget);
                auto replacement=std::make_unique<Session>(Session{std::string(tag),
                    p0::DailyGame(std::move(next),std::move(policy))});
                last_status=panel_status(replacement->game,replacement->tag);
                session=std::move(replacement);
            } else if(command=="bill-introduce"||command=="bill-approve"||
                      command=="bill-promulgate"||command=="law-hire") {
                if(!session)throw std::invalid_argument("start or load a trial first");
                if(is_campaign_world(session->game.world())&&session->tag!="A")
                    throw std::invalid_argument("the inspected peer is read-only");
                law_action(session->game,command,arg);
                last_status=panel_status(session->game,session->tag);
            } else if(command=="step"||command=="tariff"||command=="income-tax"||command=="spending-cap"||command=="public-build"||command=="public-build-budget"||command=="save") {
                if(!session)throw std::invalid_argument("start or load a trial first");
                if(is_staples_world(session->game.world())&&command!="step"&&command!="save")
                    throw std::invalid_argument("the staple household trial runs autonomously; use step or save");
                if(is_expansion_world(session->game.world())&&command!="step"&&command!="save")
                    throw std::invalid_argument("private expansion runs autonomously; use step or save");
                if(command=="step") {
                    const auto days=command_number(arg,1,366);
                    try {
                        for(int i=0;i<days;++i)advance_panel_day(session->game);
                    } catch(...) {
                        // Keep any earlier successful daily commits visible
                        // even if a later day in this command is rejected.
                        last_status=panel_status(session->game,session->tag);
                        throw;
                    }
                } else if(command=="tariff") {
                    if(is_law_world(session->game.world()))
                        throw std::invalid_argument("the elected law trial has no direct tariff order");
                    if(session->tag==country)
                        throw std::invalid_argument("the industry trial has no tariff order");
                    set_trial_tariff(session->game,session->tag,command_number(arg,0,100));
                } else if(command=="income-tax"&&is_campaign_world(session->game.world())) {
                    if(session->tag!="A")
                        throw std::invalid_argument("the inspected peer is read-only");
                    const auto [rate,party]=panel_token(arg);
                    if(party.empty()||party.find_first_of(" \t")!=party.npos)
                        throw std::invalid_argument("campaign income-tax requires one elected party");
                    const auto ppm=command_number(rate,0,100)*10'000;
                    phasegh::World::Plan order;
                    order[9].push_back([ppm,party=std::string(party)](phasegh::Stage& stage) {
                        auto& c=stage.phase_c();
                        c.propose_tax_law("A",c.government("A").prime_minister,{party},ppm);
                    });
                    advance_panel_day(session->game,std::move(order));
                } else if(command=="income-tax") {
                    if(is_law_world(session->game.world()))
                        throw std::invalid_argument("the elected law trial has no direct income-tax order");
                    if(session->tag==country)
                        throw std::invalid_argument("the industry trial has no income-tax order");
                    propose_trial_income_tax(session->game,session->tag,command_number(arg,0,100));
                } else if(command=="spending-cap") {
                    propose_trial_spending_cap(session->game,session->tag,command_number(arg,0,1'000'000));
                } else if(command=="public-build-budget") {
                    if(!is_campaign_world(session->game.world())||session->tag!="A")
                        throw std::invalid_argument("construction budgets are only for the player in the shared campaign");
                    if(arg.empty()||arg.find_first_of(" \t")!=arg.npos)
                        throw std::invalid_argument("construction budget requires one elected party");
                    const auto* offer=find_public_offer(session->game,session->tag);
                    if(!offer)throw std::invalid_argument("this checkpoint has no public construction offer");
                    const auto& c=session->game.world().engine().base().core().core();
                    session->game.approve_public_construction_budget(session->tag,
                        c.government(session->tag).prime_minister,offer->project.id,{std::string(arg)});
                } else if(command=="public-build") {
                    if(is_campaign_world(session->game.world())&&session->tag!="A")
                        throw std::invalid_argument("the inspected peer is read-only");
                    if(!arg.empty())throw std::invalid_argument("public-build takes no arguments");
                    order_trial_public_build(session->game,session->tag);
                } else save_new(session->game.world(),checkpoint_path_utf8(arg));
            } else throw std::invalid_argument("unknown or malformed panel command");
            if(session&&(command=="step"||command=="tariff"||command=="income-tax"||command=="spending-cap"||command=="public-build"||command=="public-build-budget"))
                last_status=panel_status(session->game,session->tag);
            const auto reply="{\"ok\":true,\"status\":"+last_status+'}';
            std::cout<<reply<<std::endl;
        } catch(const fs::filesystem_error&) {
            // Avoid writing locale-dependent native path/error bytes into UTF-8 JSON.
            const auto reply="{\"ok\":false,\"error\":\"checkpoint filesystem operation failed\",\"status\":"
                +last_status+'}';
            std::cout<<reply<<std::endl;
        } catch(const std::exception& e) {
            const auto reply="{\"ok\":false,\"error\":"+json_string(e.what())
                +",\"status\":"+last_status+'}';
            std::cout<<reply<<std::endl;
        }
        if(quit||ch==EOF)break;
    }
}

void buy(p0::DailyGame& game,int quantity) {
    phasegh::World::Plan order;
    order[7].push_back([quantity](phasegh::Stage& stage) {
        const auto filled=stage.phase_c().clear_household_market(
            buyer,region,"food",quantity,{producer});
        if(filled!=quantity)
            throw std::runtime_error("order could not be filled from current cash and stock");
    });
    game.advance_day(order);
}

void run(p0::DailyGame game,const std::optional<std::string>& selected={}) {
    const auto show_help=[&] {if(selected)trial_help(budget_world(game.world()));else help();};
    const auto show_status=[&] {if(selected)trial_status(game,*selected);else status(game);};
    if(selected)
        std::cout<<"VIC2.5 "<<(budget_world(game.world())?budget_scenario:trial)
                 <<": synthetic two-country trial (not a historical campaign)\n";
    else std::cout<<"VIC2.5 market-v1: synthetic market tutorial (not a historical campaign)\n";
    show_help();
    show_status();
    for(std::string input;std::getline(std::cin,input);) {
        const auto line=trim(input);
        if(line.empty())continue;
        const auto separator=line.find_first_of(" \t");
        const auto command=line.substr(0,separator);
        const auto arg=separator==std::string_view::npos?std::string_view{}:
                       trim(line.substr(separator+1));
        try {
            if(command=="quit"&&arg.empty())break;
            if(command=="help"&&arg.empty())show_help();
            else if(command=="status"&&arg.empty())show_status();
            else if(command=="step") {
                const int days=arg.empty()?1:command_number(arg,1,366);
                for(int i=0;i<days;++i)game.advance_day();
                show_status();
            } else if(!selected&&command=="buy"&&!arg.empty()) {
                buy(game,command_number(arg,1,1'000'000));
                show_status();
            } else if(selected&&command=="tariff"&&!arg.empty()) {
                set_trial_tariff(game,*selected,command_number(arg,0,100));
                std::cout<<"Tariff set within the current legal ceiling. Duty is collected only on actual trade.\n";
                show_status();
            } else if(selected&&command=="income-tax"&&!arg.empty()) {
                propose_trial_income_tax(game,*selected,command_number(arg,0,100));
                std::cout<<"Income-tax change scheduled for the opening of the next month.\n";
                show_status();
            } else if(selected&&command=="spending-cap"&&!arg.empty()) {
                propose_trial_spending_cap(game,*selected,command_number(arg,0,1'000'000));
                show_status();
            } else if(selected&&command=="public-build"&&arg.empty()) {
                order_trial_public_build(game,*selected);
                show_status();
            } else if(command=="save"&&!arg.empty()) {
                save_new(game.world(),checkpoint_path_utf8(arg));
                std::cout<<"saved\n";
            } else throw std::invalid_argument("unknown or malformed command; type help");
        }catch(const std::exception& e) {
            std::cerr<<"Error: "<<e.what()<<'\n';
        }
    }
}
} // namespace

int main(int argc,char** argv) {
    try {
        if(argc==2&&std::string_view(argv[1])=="--panel-stdio") {run_panel(); return 0;}
        std::optional<fs::path> resume;
        bool explicit_tutorial=false,explicit_trial=false,budget=false;
        std::optional<std::string> selected;
        for(int i=1;i<argc;++i) {
            const std::string_view flag(argv[i]);
            if(flag=="--help"&&argc==2) {
                std::cout<<"Usage: vic25_play [--tutorial market-v1 [--resume <save>]]\n"
                           "       vic25_play --trial tariff-v1|budget-v1 --country A|B [--resume <save>]\n";
                help();
                return 0;
            }
            if(flag=="--tutorial"&&!explicit_tutorial&&i+1<argc) {
                explicit_tutorial=true;
                if(std::string_view(argv[++i])!=tutorial)
                    throw std::invalid_argument("unknown tutorial; choose market-v1");
            } else if(flag=="--trial"&&!explicit_trial&&i+1<argc) {
                explicit_trial=true;
                const std::string_view name(argv[++i]);
                budget=name==budget_scenario;
                if(name!=trial&&!budget)
                    throw std::invalid_argument("unknown trial; choose tariff-v1 or budget-v1");
            } else if(flag=="--country"&&!selected&&i+1<argc) {
                selected=argv[++i];
                if(*selected!="A"&&*selected!="B")
                    throw std::invalid_argument("choose trial country A or B");
            } else if(flag=="--resume"&&!resume&&i+1<argc) {
                resume=argv[++i];
            } else throw std::invalid_argument("invalid launch argument; use --help");
        }
        if(explicit_trial!=selected.has_value()||(explicit_trial&&explicit_tutorial))
            throw std::invalid_argument("choose --trial tariff-v1 or budget-v1 with --country A|B separately from --tutorial");
        if(resume&&!explicit_tutorial&&!explicit_trial)
            throw std::invalid_argument("resume requires --tutorial market-v1");
        if(resume&&(!fs::is_regular_file(*resume)||fs::file_size(*resume)>850'000'000))
            throw std::invalid_argument("resume must use a regular file within the native save limit");
        if(selected) {
            auto world=open_trial(*selected,resume,budget);
            auto policies=trial_policies(world,*selected,budget);
            run(p0::DailyGame(std::move(world),std::move(policies)),selected);
            return 0;
        }
        auto world=resume?daily::SnapshotCodec::load(*resume):tutorial_world();
        if(world.migration_origin()!=origin)
            throw std::invalid_argument("checkpoint does not belong to market-v1");
        run(p0::DailyGame(std::move(world),tutorial_policies()));
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"Launch failed: "<<e.what()<<'\n';
        return 1;
    }
}
