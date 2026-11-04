// VIC2.5 version-locked offline monthly -> daily snapshot migration.
// No native MonthClose is executed, no historical day activity invented,
// and the legacy source path is NEVER overwritten.
#include "vic25/daily_snapshot.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

int main(int argc,char** argv) {
    try {
        if(argc!=5)
            throw std::invalid_argument(
                "usage: vic25_daily_migrate <phase-c|phase-d|phase-e|phase-gh>"
                " <legacy-input> <new-daily-output> <world-seed>");
        const std::string source_kind=argv[1];
        const std::filesystem::path source=argv[2],destination=argv[3];
        const std::uint64_t seed=std::stoull(argv[4]);
        if(!std::filesystem::is_regular_file(source))
            throw std::runtime_error("legacy save source is missing/not regular");
        if(std::filesystem::exists(destination))
            throw std::runtime_error("refusing to overwrite destination");
        const auto size=std::filesystem::file_size(source);
        if(size<20||size>768'000'000)
            throw std::runtime_error("legacy save size outside accepted range");

        vic25::daily::World daily=[&]{
            if(source_kind=="phase-c")
                return vic25::daily::Migrator::phase_c_file(source,seed);
            std::ifstream in(source,std::ios::binary);
            if(!in)throw std::runtime_error("cannot read legacy save");
            const std::string bytes{std::istreambuf_iterator<char>{in},
                                    std::istreambuf_iterator<char>{}};
            if(source_kind=="phase-d")
                return vic25::daily::Migrator::phase_d_bytes(bytes,seed);
            if(source_kind=="phase-e")
                return vic25::daily::Migrator::phase_e_bytes(bytes,seed);
            if(source_kind=="phase-gh")
                return vic25::daily::Migrator::phase_gh_bytes(bytes,seed);
            throw std::invalid_argument("unsupported source kind: "
                "Phase B standalone binary, I/J runtime saves and raw unknown "
                "layouts require a dedicated versioned converter");
        }();
        vic25::daily::SnapshotCodec::save(daily,destination);
        const auto loaded=vic25::daily::SnapshotCodec::load(destination);
        if(vic25::daily::SnapshotCodec::encode(loaded)!=
           vic25::daily::SnapshotCodec::encode(daily))
            throw std::runtime_error("new daily save failed post-write verification");
        const auto date=daily.date();
        std::cout<<"DAILY_MIGRATION_OK"
                 <<" source="<<source_kind
                 <<" date="<<date.year<<"-"<<date.month<<"-"<<date.day
                 <<" seed="<<seed
                 <<" legacy_checksum="<<daily.migration_checksum()
                 <<" day_commit="<<daily.day_commit_id()<<"\n";
    }catch(const std::exception& error) {
        std::cerr<<"DAILY_MIGRATION_FAILED: "<<error.what()<<"\n";
        return 1;
    }
    return 0;
}
