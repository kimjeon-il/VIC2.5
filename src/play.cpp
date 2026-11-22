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

int positive(std::string_view value,int maximum) {
    int result{};
    const auto end=value.data()+value.size();
    const auto [last,err]=std::from_chars(value.data(),end,result);
    if(err!=std::errc{}||last!=end||result<1||result>maximum)
        throw std::invalid_argument("enter a positive whole number within the command limit");
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

void run(p0::DailyGame game) {
    std::cout<<"VIC2.5 market-v1: synthetic market tutorial (not a historical campaign)\n";
    help();
    status(game);
    for(std::string input;std::getline(std::cin,input);) {
        const auto line=trim(input);
        if(line.empty())continue;
        const auto separator=line.find_first_of(" \t");
        const auto command=line.substr(0,separator);
        const auto arg=separator==std::string_view::npos?std::string_view{}:
                       trim(line.substr(separator+1));
        try {
            if(command=="quit"&&arg.empty())break;
            if(command=="help"&&arg.empty())help();
            else if(command=="status"&&arg.empty())status(game);
            else if(command=="step") {
                const int days=arg.empty()?1:positive(arg,366);
                for(int i=0;i<days;++i)game.advance_day();
                status(game);
            } else if(command=="buy"&&!arg.empty()) {
                buy(game,positive(arg,1'000'000));
                status(game);
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
        bool explicit_tutorial=false;
        for(int i=1;i<argc;++i) {
            const std::string_view flag(argv[i]);
            if(flag=="--help"&&argc==2) {
                std::cout<<"Usage: vic25_play [--tutorial market-v1 [--resume <save>]]\n";
                help();
                return 0;
            }
            if(flag=="--tutorial"&&!explicit_tutorial&&i+1<argc) {
                explicit_tutorial=true;
                if(std::string_view(argv[++i])!=tutorial)
                    throw std::invalid_argument("unknown tutorial; choose market-v1");
            } else if(flag=="--resume"&&!resume&&i+1<argc) {
                resume=argv[++i];
            } else throw std::invalid_argument("invalid launch argument; use --help");
        }
        if(resume&&!explicit_tutorial)
            throw std::invalid_argument("resume requires --tutorial market-v1");
        if(resume&&(!fs::is_regular_file(*resume)||fs::file_size(*resume)>850'000'000))
            throw std::invalid_argument("resume must use a regular file within the native save limit");
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
