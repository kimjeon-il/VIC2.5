#include "vic25/daily_snapshot.hpp"
#include "vic25/p0_daily.hpp"
#include <charconv>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
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

daily::World tutorial_world() {
    phasegh::World engine{24000};
    auto& geo=engine.seed_geography();
    geo.set_dataset(origin);
    geo.seed_province(1,region,country);
    geo.seed_unit(1,country,"Tutorial district");
    geo.seed_assignment(1,1);
    auto& industry=geo.seed_core();
    auto& core=industry.seed_core();
    core.seed_actor(country,100);
    core.seed_actor(producer,100);
    core.seed_actor(buyer,50);
    core.seed_actor(workers,0);
    core.seed_government(country,"HEAD","PM",5,3);
    core.seed_market(region,"ore",1,1,10,0);
    core.seed_market(region,"food",10,1,20,0);
    core.seed_stock(producer,region,"ore",62);
    core.seed_stock(producer,region,"food",4);
    core.seed_production_facility("MILL-SITE",producer,region,62);
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
    recipe.id="TUTORIAL-FOOD";recipe.site="MILL-SITE";
    recipe.inputs={{"ore",1}};recipe.outputs={{"food",2}};
    recipe.workers_per_batch=1;recipe.capacity_per_batch=1;
    industry.seed_recipe(recipe);
    return daily::World::adopt(std::move(engine),2025,origin);
}

p0::Policies tutorial_policies() {
    p0::Policies result;
    // The player is the only household buyer in this exercise.
    result.daily_food_units_per_person=0;
    result.monthly_clothing_units_per_person=0;
    result.manufacturers.push_back({"TUTORIAL-FOOD",100,workers,country,1});
    return result;
}

constexpr auto trial="tariff-v1";
constexpr auto trial_route="TRIAL-ORE";
constexpr auto trial_seller="TRIAL-SELLER";
constexpr auto trial_buyer="TRIAL-IMPORTER";
constexpr auto trial_carrier="TRIAL-CARRIER";
constexpr auto trial_good="ore";

std::string trial_origin(std::string_view tag) {
    return "trial:tariff-v1:"+std::string(tag);
}
const char* trial_region(std::string_view tag) {return tag=="A"?"DISTRICT-A":"DISTRICT-B";}
std::string_view other_country(std::string_view tag) {return tag=="A"?"B":"A";}
phasegh::Id province(std::string_view tag) {return tag=="A"?1:2;}

daily::World trial_world(const std::string& selected) {
    using namespace phasec;
    phasegh::World engine{24000};
    auto& geo=engine.seed_geography();
    geo.set_dataset(trial_origin(selected));
    auto& core=geo.seed_core().seed_core();
    for(const std::string tag:{"A","B"}) {
        const auto id=province(tag);
        geo.seed_province(id,trial_region(tag),tag);
        geo.seed_unit(id,tag,trial_region(tag));
        geo.seed_assignment(id,id);
        core.seed_actor(tag,100);
        core.seed_government(tag,"HEAD-"+tag,"PM-"+tag,5,3);
        core.seed_p1_constitution(tag,{P1GovernancePrinciple::monarchy,
            P1PowerDistribution::autocracy,P1LegislatureKind::none,"trial:constitution"});
        core.p2_seed_base({"TRIAL-CUSTOMS-"+tag,tag,"trial_customs","national",
            "synthetic-import-ceiling","trial:customs-statute",P2NormTier::statute,
            {{"ore-tariff-ceiling",tag,P2ClauseKind::maximum_import_tariff_ppm,
                tag=="A"?200'000:400'000}}});
        const auto price=tag==selected?30:10;
        core.seed_market(trial_region(tag),trial_good,price,price,price,0);
    }
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
    return daily::World::adopt(std::move(engine),2025,trial_origin(selected));
}

p0::Policies trial_policies() {
    p0::Policies result;
    result.daily_food_units_per_person=0;
    result.monthly_clothing_units_per_person=0;
    result.auto_national_budgets_from_actual_services=false;
    return result;
}

void verify_trial(const daily::World& world,const std::string& tag) {
    const auto& engine=world.engine();
    const auto& routes=engine.commerce().routes;
    const auto it=routes.find(trial_route);
    if(world.migration_origin()!=trial_origin(tag)||
       engine.base().spatial().dataset_hash!=trial_origin(tag)||
       routes.size()!=1||it==routes.end()||
       it->second.origin!=province(other_country(tag))||
       it->second.destination!=province(tag)||
       it->second.buyer!=trial_buyer||it->second.seller!=trial_seller||
       it->second.good!=trial_good)
        throw std::invalid_argument("checkpoint does not belong to the selected trial country");
}

void trial_help() {
    std::cout<<"Commands: status | step [days] | tariff <percent> | save <new path> | help | quit\n"
             <<"Set the selected country's import duty on ore (whole percent); a lawful order advances one day.\n"
             <<"Resume: vic25_play --trial tariff-v1 --country A|B --resume <save>\n";
}

void trial_status(const p0::DailyGame& game,const std::string& selected) {
    const auto& world=game.world();
    const auto date=world.date();
    const auto& engine=world.engine();
    const auto& c=engine.base().core().core();
    const auto& tariffs=engine.diplomacy().tariffs;
    const auto current=tariffs.find({selected,trial_good});
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
             <<" paid_duty="<<duties
             <<" delivered_ore="<<c.stock(trial_buyer,trial_region(selected),trial_good)
             <<" native_commit="<<engine.commit_id()<<'\n';
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
    const auto show_help=[&] {if(selected)trial_help();else help();};
    const auto show_status=[&] {if(selected)trial_status(game,*selected);else status(game);};
    if(selected)
        std::cout<<"VIC2.5 tariff-v1: synthetic two-country trial (not a historical campaign)\n";
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
                const auto ppm=command_number(arg,0,100)*10'000;
                const auto key="TRIAL-TARIFF:"+std::to_string(game.world().day_index());
                phasegh::World::Plan order;
                order[0].push_back([tag=*selected,ppm,key](phasegh::Stage& stage){
                    stage.adopt_tariff_daily(key,tag,trial_good,ppm,
                        stage.phase_c().government(tag).prime_minister);
                });
                game.advance_day(order);
                std::cout<<"Tariff set within the current legal ceiling. Duty is collected only on actual trade.\n";
                show_status();
            } else if(command=="save"&&!arg.empty()) {
                save_new(game.world(),fs::path(arg));
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
        std::optional<fs::path> resume;
        bool explicit_tutorial=false,explicit_trial=false;
        std::optional<std::string> selected;
        for(int i=1;i<argc;++i) {
            const std::string_view flag(argv[i]);
            if(flag=="--help"&&argc==2) {
                std::cout<<"Usage: vic25_play [--tutorial market-v1 [--resume <save>]]\n"
                           "       vic25_play --trial tariff-v1 --country A|B [--resume <save>]\n";
                help();
                return 0;
            }
            if(flag=="--tutorial"&&!explicit_tutorial&&i+1<argc) {
                explicit_tutorial=true;
                if(std::string_view(argv[++i])!=tutorial)
                    throw std::invalid_argument("unknown tutorial; choose market-v1");
            } else if(flag=="--trial"&&!explicit_trial&&i+1<argc) {
                explicit_trial=true;
                if(std::string_view(argv[++i])!=trial)
                    throw std::invalid_argument("unknown trial; choose tariff-v1");
            } else if(flag=="--country"&&!selected&&i+1<argc) {
                selected=argv[++i];
                if(*selected!="A"&&*selected!="B")
                    throw std::invalid_argument("choose trial country A or B");
            } else if(flag=="--resume"&&!resume&&i+1<argc) {
                resume=argv[++i];
            } else throw std::invalid_argument("invalid launch argument; use --help");
        }
        if(explicit_trial!=selected.has_value()||(explicit_trial&&explicit_tutorial))
            throw std::invalid_argument("choose --trial tariff-v1 --country A|B separately from --tutorial");
        if(resume&&!explicit_tutorial&&!explicit_trial)
            throw std::invalid_argument("resume requires --tutorial market-v1");
        if(resume&&(!fs::is_regular_file(*resume)||fs::file_size(*resume)>850'000'000))
            throw std::invalid_argument("resume must use a regular file within the native save limit");
        if(selected) {
            auto world=resume?daily::SnapshotCodec::load(*resume):trial_world(*selected);
            verify_trial(world,*selected);
            run(p0::DailyGame(std::move(world),trial_policies()),selected);
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
