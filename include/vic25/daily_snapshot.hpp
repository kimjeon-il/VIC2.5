#pragma once
// Daily v2.0 snapshot and explicit migration of authentic Phase B(in-memory),
// C, D, E, G/H codec states. Reproduces the ORIGINAL legacy payload verbatim
// after migration: no fabricated monthly payouts, births, battles or debt.
#include "vic25/daily_time.hpp"
#include "vic25/phase_gh_snapshot.hpp"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace vic25::daily {
namespace detail {
using Writer=phasec::persistence_detail::BinaryWriter;
using Reader=phasec::persistence_detail::BinaryReader;
template<class A>void command(A& a,Command& c,bool modern=true){
    a(c.id);a(c.kind);a(c.from);a(c.to);a(c.region);a(c.good);
    a(c.amount);a(c.unit_price);
    if(modern){a(c.destination);a(c.auxiliary);a(c.route_id);}
}
template<class A>void entry(A& a,Entry& e) {
    a(e.effective_day);a(e.command_id);a(e.kind);a(e.c_commit_id);
}
template<class A,class W>void metadata(A& a,W& world,bool modern) {
    a(world.day_);
    a(world.day_commit_);
    a(world.world_seed_);
    a(world.migration_origin_);
    a(world.migration_checksum_);
    a(world.known_command_ids_);
    std::uint64_t groups=0;
    if constexpr(std::is_same_v<A,Writer>)groups=world.scheduled_.size();
    a(groups);
    if(groups>500'000)
        throw std::runtime_error("TIME_ERR_TOO_MANY_SCHEDULED_DAYS");
    if constexpr(std::is_same_v<A,Writer>) {
        for(const auto& [day,commands]:world.scheduled_) {
            a(day);
            std::uint64_t count=commands.size();a(count);
            if(count>200'000)
                throw std::runtime_error("TIME_ERR_TOO_MANY_COMMANDS");
            for(const auto& c:commands){auto copy=c;command(a,copy,modern);}
        }
    }else {
        for(std::uint64_t i=0;i<groups;++i) {
            DayIndex day{};std::uint64_t count{};a(day);a(count);
            if(count==0||count>200'000)
                throw std::runtime_error("TIME_ERR_INVALID_COMMAND_GROUP");
            std::vector<Command> commands;
            commands.reserve(static_cast<std::size_t>(count));
            for(std::uint64_t j=0;j<count;++j) {
                Command c;command(a,c,modern);commands.push_back(std::move(c));
            }
            if(!world.scheduled_.emplace(day,std::move(commands)).second)
                throw std::runtime_error("TIME_ERR_DUPLICATE_COMMAND_DAY");
        }
    }
    std::uint64_t count=0;
    if constexpr(std::is_same_v<A,Writer>)count=world.journal_.size();
    a(count);
    if(count>5'000'000)throw std::runtime_error("TIME_ERR_JOURNAL_LIMIT");
    if constexpr(std::is_same_v<A,Writer>) {
        for(const auto& e:world.journal_) {auto copy=e;entry(a,copy);}
    }else {
        for(std::uint64_t i=0;i<count;++i) {
            Entry e;entry(a,e);world.journal_.push_back(std::move(e));
        }
    }
    if(modern) {
        std::uint64_t count_cargo=0;
        if constexpr(std::is_same_v<A,Writer>)
            count_cargo=world.cargo_.size();
        a(count_cargo);
        if(count_cargo>1'000'000)
            throw std::runtime_error("TIME_ERR_CARGO_SNAPSHOT_LIMIT");
        if constexpr(std::is_same_v<A,Writer>) {
            for(const auto& [id,c]:world.cargo_) {
                a(id);a(c.id);a(c.sender);a(c.recipient);a(c.origin);
                a(c.destination);a(c.good);a(c.edge_id);a(c.quantity);
                a(c.loss);a(c.dispatched);a(c.arrival);a(c.arrived);
            }
        } else {
            for(std::uint64_t i=0;i<count_cargo;++i) {
                std::string id;
                DailyCargo c;
                a(id);a(c.id);a(c.sender);a(c.recipient);a(c.origin);
                a(c.destination);a(c.good);a(c.edge_id);a(c.quantity);
                a(c.loss);a(c.dispatched);a(c.arrival);a(c.arrived);
                if(!world.cargo_.emplace(id,std::move(c)).second)
                    throw std::runtime_error("TIME_ERR_DUPLICATE_CARGO_SNAPSHOT");
            }
        }
    }
}
} // namespace detail

struct SnapshotCodec {
    static constexpr const char* magic="VIC25_DAILY_SNAPSHOT_1";
    static constexpr const char* version="vic25-daily-save/2.0";
    static constexpr const char* prior_version="vic25-daily-save/1.0";
    static std::string encode(const World& world) {
        world.validate();
        const std::string base=phasegh::SnapshotCodec::encode(world.engine_);
        std::ostringstream os(std::ios::binary|std::ios::out);
        detail::Writer w{os};
        w(std::string(magic));
        w(std::string(version));
        w(std::string(time_contract));
        w(std::string(calendar_contract));
        const auto length=static_cast<std::uint64_t>(base.size());
        w(length);
        os.write(base.data(),static_cast<std::streamsize>(base.size()));
        if(!os)throw std::runtime_error("TIME_ERR_WRITE_LEGACY_PAYLOAD");
        detail::metadata(w,world);
        const auto payload=os.str();
        std::ostringstream tail(std::ios::binary|std::ios::out);
        detail::Writer checksum{tail};
        checksum(phasec::persistence_detail::checksum(payload));
        return payload+tail.str();
    }
    static World decode(const std::string& bytes) {
        if(bytes.size()<64||bytes.size()>850'000'000)
            throw std::runtime_error("TIME_ERR_INVALID_SNAPSHOT_SIZE");
        const auto payload=bytes.substr(0,bytes.size()-8);
        std::uint64_t stored_hash{};
        std::istringstream trailer(bytes.substr(bytes.size()-8),
                                   std::ios::binary|std::ios::in);
        detail::Reader checksum{trailer};
        checksum(stored_hash);
        if(stored_hash!=phasec::persistence_detail::checksum(payload))
            throw std::runtime_error("TIME_ERR_SNAPSHOT_CHECKSUM");
        std::istringstream is(payload,std::ios::binary|std::ios::in);
        detail::Reader r{is};
        std::string m,v,t,c;
        r(m);r(v);r(t);r(c);
        if(m!=magic||(v!=version&&v!=prior_version)||t!=time_contract||c!=calendar_contract)
            throw std::runtime_error("TIME_ERR_SNAPSHOT_CONTRACT_MISMATCH");
        std::uint64_t length{};r(length);
        if(length==0||length>768'000'000)
            throw std::runtime_error("TIME_ERR_INVALID_LEGACY_PAYLOAD_SIZE");
        std::string base(static_cast<std::size_t>(length),'\0');
        is.read(base.data(),static_cast<std::streamsize>(length));
        if(!is)throw std::runtime_error("TIME_ERR_TRUNCATED_LEGACY_PAYLOAD");
        auto original=phasegh::SnapshotCodec::decode(base);
        World result=World::adopt(std::move(original));
        detail::metadata(r,result,v==version);
        if(is.peek()!=std::char_traits<char>::eof())
            throw std::runtime_error("TIME_ERR_TRAILING_DATA");
        result.validate();
        return result;
    }
    static void save(const World& world,const std::filesystem::path& file) {
        if(std::filesystem::exists(file))
            throw std::runtime_error("TIME_ERR_REFUSE_OVERWRITE_EXISTING_SAVE");
        const auto bytes=encode(world);
        const auto tmp=file.string()+".uncommitted";
        {
            std::ofstream out(tmp,std::ios::binary|std::ios::trunc);
            if(!out)throw std::runtime_error("TIME_ERR_CHECKPOINT_OPEN");
            out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));
            out.flush();
            if(!out)throw std::runtime_error("TIME_ERR_CHECKPOINT_WRITE");
        }
        std::filesystem::rename(tmp,file);
    }
    static World load(const std::filesystem::path& file) {
        std::ifstream in(file,std::ios::binary);
        if(!in)throw std::runtime_error("TIME_ERR_CHECKPOINT_NOT_FOUND");
        const std::string bytes{std::istreambuf_iterator<char>{in},
                                std::istreambuf_iterator<char>{}};
        return decode(bytes);
    }
};

struct Migrator {
    static World phase_gh_bytes(const std::string& bytes,std::uint64_t seed=0) {
        auto legacy=phasegh::SnapshotCodec::decode(bytes);
        return World::adopt(std::move(legacy),seed,"phase-gh/codec",
                            phasec::persistence_detail::checksum(bytes));
    }
    static World phase_e_bytes(const std::string& bytes,std::uint64_t seed=0) {
        auto e=phasee::SnapshotCodec::decode(bytes);
        auto gh=phasegh::World::from_legacy_phase_e(std::move(e));
        return World::adopt(std::move(gh),seed,"phase-e/codec",
                            phasec::persistence_detail::checksum(bytes));
    }
    static World phase_d_bytes(const std::string& bytes,std::uint64_t seed=0) {
        auto d=phased::SnapshotCodec::decode(bytes);
        auto e=phasee::World::from_legacy_phase_d(std::move(d));
        auto gh=phasegh::World::from_legacy_phase_e(std::move(e));
        return World::adopt(std::move(gh),seed,"phase-d/codec",
                            phasec::persistence_detail::checksum(bytes));
    }
    static World phase_c_payload(const std::string& bytes,std::uint64_t seed=0) {
        auto c=phasec::SnapshotCodec::decode(bytes);
        auto d=phased::World::from_legacy_phase_c(std::move(c));
        auto e=phasee::World::from_legacy_phase_d(std::move(d));
        auto gh=phasegh::World::from_legacy_phase_e(std::move(e));
        return World::adopt(std::move(gh),seed,"phase-c/raw-payload",
                            phasec::persistence_detail::checksum(bytes));
    }
    static World phase_c_file(const std::filesystem::path& path,
                              std::uint64_t seed=0) {
        std::ifstream file(path,std::ios::binary);
        if(!file)throw std::runtime_error("TIME_ERR_MISSING_PHASE_C_SAVE");
        const std::string original_bytes{std::istreambuf_iterator<char>{file},
                                         std::istreambuf_iterator<char>{}};
        auto c=phasec::SnapshotCodec::load(path);
        auto d=phased::World::from_legacy_phase_c(std::move(c));
        auto e=phasee::World::from_legacy_phase_d(std::move(d));
        auto gh=phasegh::World::from_legacy_phase_e(std::move(e));
        return World::adopt(std::move(gh),seed,"phase-c/checkpoint-file",
                            phasec::persistence_detail::checksum(original_bytes));
    }
    static World phase_b_in_memory(phaseb::World legacy,std::uint64_t seed=0) {
        const auto digest=legacy.fingerprint();
        auto c=phasec::IntegratedWorld::from_legacy_phase_b(std::move(legacy));
        auto d=phased::World::from_legacy_phase_c(std::move(c));
        auto e=phasee::World::from_legacy_phase_d(std::move(d));
        auto gh=phasegh::World::from_legacy_phase_e(std::move(e));
        return World::adopt(std::move(gh),seed,"phase-b/in-memory",digest);
    }
};
} // namespace vic25::daily
