#pragma once
// E layered snapshot: Phase D bytes + E immutable asset binding / dynamic
// spatial assignments, pending events and historical log. Never treats a
// render tile or duplicate sovereignty value as authoritative.
#include "vic25/phase_e.hpp"
#include "vic25/phase_d_snapshot.hpp"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <type_traits>

namespace vic25::phasee {
namespace persisted {
using Writer=phasec::persistence_detail::BinaryWriter;
using Reader=phasec::persistence_detail::BinaryReader;
template<class A> void op(A& ar,Operation& x) {
    ar(x.id);ar(x.kind);ar(x.provinces);ar(x.unit);ar(x.other);
    ar(x.value);ar(x.other_value);ar(x.effective);
    ar(x.leader);ar(x.minister);ar(x.chamber_seats);ar(x.quorum);
}
template<class A> void log(A& ar,ChangeLog& x) {
    ar(x.event_id);ar(x.kind);ar(x.effective);ar(x.provinces);
}
template<class A> void province(A& ar,Province& x) {
    ar(x.id);ar(x.region);ar(x.area_m2);ar(x.source_hash);
    ar(x.neighbors);ar(x.coastal_seas);
}
template<class A> void unit(A& ar,AdministrativeUnit& x) {
    ar(x.id);ar(x.name);ar(x.country);ar(x.parent);ar(x.active);
}
template<class A> void scope(A& ar,TerritorialScope& x) {
    ar(x.id);ar(x.kind);ar(x.mode);ar(x.admin);ar(x.provinces);
}
template<class A,class X,class Fn>
void checked_map(A& ar,X& map,Fn each) {
    std::uint64_t count=0;
    if constexpr(std::is_same_v<A,Writer>)count=map.size();
    ar(count);
    if(count>500'000)throw std::runtime_error("Phase E invalid container length");
    if constexpr(std::is_same_v<A,Writer>) {
        for(const auto& [k,v]:map) {
            auto key=k; auto obj=v;
            ar(key);each(ar,obj);
        }
    } else {
        for(std::uint64_t i=0;i<count;i++) {
            typename X::key_type key{};
            typename X::mapped_type obj{};
            ar(key);each(ar,obj);
            if(!map.emplace(std::move(key),std::move(obj)).second)
                throw std::runtime_error("Phase E duplicate snapshot key");
        }
    }
}
template<class A>void election(A& ar,ElectoralBoundary& b) {
    ar(b.opened_month);ar(b.boundary_revision);ar(b.provinces_by_district);
    ar(b.allocated);ar(b.named_districts);ar(b.split_without_geometry);
}
template<class A>void state(A& ar,SpatialState& s) {
    ar(s.dataset_hash);
    checked_map(ar,s.provinces,[](A& a,auto& x){province(a,x);});
    ar(s.regions);
    checked_map(ar,s.units,[](A& a,auto& x){unit(a,x);});
    ar(s.membership);
    checked_map(ar,s.scopes,[](A& a,auto& x){scope(a,x);});
    ar(s.claims);ar(s.civil);ar(s.fiscal);
    checked_map(ar,s.elections,[](A& a,auto& b){election(a,b);});
    checked_map(ar,s.pending,[](A& a,auto& xs) {
        std::uint64_t count=0;
        if constexpr(std::is_same_v<A,Writer>)count=xs.size();
        a(count);
        if(count>100'000)throw std::runtime_error("Phase E invalid pending events");
        if constexpr(std::is_same_v<A,Writer>) {
            for(auto& x:xs){auto copy=x;op(a,copy);}
        } else {
            for(std::uint64_t i=0;i<count;i++){Operation item;op(a,item);xs.push_back(std::move(item));}
        }
    });
    ar(s.commands);
    std::uint64_t count=0;
    if constexpr(std::is_same_v<A,Writer>)count=s.journal.size();
    ar(count);
    if(count>1'000'000)throw std::runtime_error("Phase E invalid historical event length");
    if constexpr(std::is_same_v<A,Writer>) {
        for(auto& x:s.journal){auto copy=x;log(ar,copy);}
    } else {
        for(std::uint64_t i=0;i<count;i++){ChangeLog e;log(ar,e);s.journal.push_back(std::move(e));}
    }
    ar(s.revision);
}
} // namespace persisted

struct SnapshotCodec {
    static constexpr const char* magic="VIC25_PHASEE_SNAPSHOT_1";
    static constexpr const char* version="phase-e-cpp/0.3";
    static std::string encode(const World& w) {
        w.validate();
        std::ostringstream out(std::ios::binary|std::ios::out);
        persisted::Writer ar{out};
        ar(std::string(magic));
        ar(std::string(version));
        ar(std::string(contract_version));
        ar(phased::SnapshotCodec::encode(w.core_));
        auto state=w.spatial_;
        persisted::state(ar,state);
        const auto payload=out.str();
        std::ostringstream checkstream(std::ios::binary|std::ios::out);
        persisted::Writer checksum_writer{checkstream};
        checksum_writer(phasec::persistence_detail::checksum(payload));
        return payload+checkstream.str();
    }
    static World decode(const std::string& bytes) {
        if(bytes.size()<48||bytes.size()>512'000'000)
            throw std::runtime_error("Phase E invalid snapshot size");
        std::istringstream in(bytes,std::ios::binary|std::ios::in);
        persisted::Reader ar{in};
        std::string m,v,design,core;
        ar(m);ar(v);ar(design);
        if(m!=magic||v!=version||design!=contract_version)
            throw std::runtime_error("Phase E contract mismatch");
        ar(core);
        World candidate;
        candidate.core_=phased::SnapshotCodec::decode(core);
        persisted::state(ar,candidate.spatial_);
        std::uint64_t digest{};
        ar(digest);
        if(in.peek()!=std::char_traits<char>::eof()||
           digest!=phasec::persistence_detail::checksum(
                        bytes.substr(0,bytes.size()-sizeof(std::uint64_t))))
            throw std::runtime_error("Phase E checksum mismatch");
        candidate.validate();
        return candidate;
    }
    static void save(const World& w,const std::filesystem::path& target) {
        if(std::filesystem::exists(target))
            throw std::runtime_error("Phase E refuses to overwrite checkpoint");
        const auto payload=encode(w);
        const auto temporary=target.string()+".uncommitted";
        {
            std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
            if(!out)throw std::runtime_error("Phase E open checkpoint failed");
            out.write(payload.data(),static_cast<std::streamsize>(payload.size()));
            out.flush();
            if(!out)throw std::runtime_error("Phase E write checkpoint failed");
        }
        std::filesystem::rename(temporary,target);
    }
    static World load(const std::filesystem::path& target) {
        std::ifstream in(target,std::ios::binary);
        if(!in)throw std::runtime_error("Phase E checkpoint missing");
        const std::string bytes{std::istreambuf_iterator<char>{in},
                                std::istreambuf_iterator<char>{}};
        return decode(bytes);
    }
};
} // namespace vic25::phasee
