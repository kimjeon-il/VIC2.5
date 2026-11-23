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
p0::PublicConstructionOffer public_offer(const std::string& tag) {
    p0::PublicConstructionOffer offer;
    auto& p=offer.project;
    p.id="PUBLIC-TOOLS-"+tag;p.target_site="PUBLIC-TOOLS-SITE-"+tag;
    p.client=tag;p.contractor=public_builder(tag);p.region=trial_region(tag);
    p.material_good="steel";p.work_required=1;p.material_per_work=2;
    p.workers_per_work=1;p.cash_price_per_work=20;p.target_capacity=31;
    offer.treasury_cash_reserve=40;
    auto& recipe=offer.recipe;
    recipe.id="PUBLIC-TOOLS-METHOD-"+tag;recipe.site=p.target_site;
    recipe.inputs={{"iron",1},{"wood",1}};recipe.outputs={{"tools",1}};
    recipe.workers_per_batch=recipe.capacity_per_batch=1;
    return offer;
}

// An earlier budget checkpoint has neither role and keeps its original
// policies. Never create these actors, markets or stock on a resume.
bool budget_build_fixture(const daily::World& world,const std::string& tag) {
    const auto& c=world.engine().base().core().core();
    const auto builder=public_builder(tag),supplier=public_steel_seller(tag);
    if(!c.has_actor(builder)&&!c.has_actor(supplier))return false;
    if(!c.has_actor(builder)||!c.has_actor(supplier)||
       c.country_treasury(tag)==builder||c.country_treasury(tag)==supplier||
       !c.has_market(trial_region(tag),"steel")||!c.has_market(trial_region(tag),"tools"))
        throw std::invalid_argument("checkpoint has an incomplete public construction trial");
    return true;
}

void seed_budget_build_fixture(phasec::IntegratedWorld& core,const std::string& tag) {
    const std::string region=trial_region(tag);
    const auto builder=public_builder(tag),supplier=public_steel_seller(tag);
    core.seed_actor(builder,1200);core.seed_actor(supplier,0);
    core.seed_market(region,"steel",3,3,3);
    core.seed_market(region,"tools",20,20,20);
    core.seed_market(region,"iron",4,4,4);
    core.seed_market(region,"wood",2,2,2);
    core.seed_stock(supplier,region,"steel",2);
    auto& pop=core.seed_population();
    const auto household=pop.household_profile({region,builder,"shared",1});
    const auto economy=pop.economic_profile(
        {region,"worker","wage","employee",pop.default_education()});
    const auto demographic=pop.demographic_profile(
        {region,23640,phaseb::Sex::female,"trial-culture","trial-faith",
         "citizen","free",pop.default_health(),phaseb::Service::civilian});
    const auto worker=pop.seed_population(demographic,economy,household,1);
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
    if(budget)seed_budget_build_fixture(core,selected);
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
    if(budget&&budget_build_fixture(world,selected))
        result.public_construction_offers.push_back(public_offer(selected));
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
               <<",\"commissioned_capacity\":"<<capacity<<'}';
        }

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
            else if(command=="new"||command=="load") {
                const auto [choice,path]=panel_token(arg);
                const bool budget=choice=="BUDGET-A"||choice=="BUDGET-B";
                const auto tag=budget?choice.substr(7):choice;
                if((tag!="A"&&tag!="B"&&tag!=country)||(command=="new"&&!path.empty()))
                    throw std::invalid_argument("select A, B, TUT, BUDGET-A or BUDGET-B");
                const auto resume=command=="load"?std::optional{checkpoint_path_utf8(path)}:std::nullopt;
                const bool industry=tag==country;
                auto next=industry?open_industry(resume):open_trial(std::string(tag),resume,budget);
                auto policy=industry?tutorial_policies(true):trial_policies(next,std::string(tag),budget);
                auto replacement=std::make_unique<Session>(Session{std::string(tag),
                    p0::DailyGame(std::move(next),std::move(policy))});
                last_status=panel_status(replacement->game,replacement->tag);
                session=std::move(replacement);
            } else if(command=="step"||command=="tariff"||command=="income-tax"||command=="spending-cap"||command=="public-build"||command=="save") {
                if(!session)throw std::invalid_argument("start or load a trial first");
                if(command=="step") {
                    const auto days=command_number(arg,1,366);
                    try {
                        for(int i=0;i<days;++i)session->game.advance_day();
                    } catch(...) {
                        // Keep any earlier successful daily commits visible
                        // even if a later day in this command is rejected.
                        last_status=panel_status(session->game,session->tag);
                        throw;
                    }
                } else if(command=="tariff") {
                    if(session->tag==country)
                        throw std::invalid_argument("the industry trial has no tariff order");
                    set_trial_tariff(session->game,session->tag,command_number(arg,0,100));
                } else if(command=="income-tax") {
                    if(session->tag==country)
                        throw std::invalid_argument("the industry trial has no income-tax order");
                    propose_trial_income_tax(session->game,session->tag,command_number(arg,0,100));
                } else if(command=="spending-cap") {
                    propose_trial_spending_cap(session->game,session->tag,command_number(arg,0,1'000'000));
                } else if(command=="public-build") {
                    if(!arg.empty())throw std::invalid_argument("public-build takes no arguments");
                    order_trial_public_build(session->game,session->tag);
                } else save_new(session->game.world(),checkpoint_path_utf8(arg));
            } else throw std::invalid_argument("unknown or malformed panel command");
            if(session&&(command=="step"||command=="tariff"||command=="income-tax"||command=="spending-cap"||command=="public-build"))
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
