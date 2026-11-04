#pragma once
// Phase D snapshot codec for the bounded Phase C-backed vertical slice.
// Format is version-locked and validates *both* the Phase C ledger and
// construction/industry/research/knowledge sidecar; not a generic migration.
#include "vic25/phase_d.hpp"
#include "vic25/phase_c_snapshot.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace vic25::phased {
namespace detail {
template<class A,class X> void input(A& a,X& x) {a(x.good);a(x.per_unit);}
template<class A,class X> void construction(A& a,X& p) {
#define F(v) a(p.v)
    F(id);F(client);F(contractor);F(region);F(material_good);F(target_site);
    F(work_required);F(material_per_work);F(target_capacity);
    F(workers_per_work);F(cash_price_per_work);F(accepted_work);F(paid_work);
    F(capitalized_work);F(planned_work);F(capitalized_value);
    F(ready);F(commissioned);F(ready_month);
#undef F
}
template<class A,class X,class Fn> void vec(A& a,X& container,Fn each) {
    if constexpr(std::is_const_v<std::remove_reference_t<X>>) {
        a(static_cast<std::uint64_t>(container.size()));
        for(const auto& x:container) each(a,x);
    } else {
        std::uint64_t n{};a(n);
        if(n>2'000'000)throw std::runtime_error("Phase D oversized vector");
        container.clear();
        for(std::uint64_t i=0;i<n;++i) {
            typename X::value_type value{};
            each(a,value);
            container.push_back(std::move(value));
        }
    }
}
template<class A,class X,class Fn> void map(A& a,X& container,Fn each) {
    if constexpr(std::is_const_v<std::remove_reference_t<X>>) {
        a(static_cast<std::uint64_t>(container.size()));
        for(const auto& [key,value]:container) {a(key);each(a,value);}
    } else {
        std::uint64_t n{};a(n);
        if(n>2'000'000)throw std::runtime_error("Phase D oversized map");
        container.clear();
        for(std::uint64_t i=0;i<n;++i) {
            typename X::key_type key{};
            typename X::mapped_type value{};
            a(key);
            each(a,value);
            if(!container.emplace(std::move(key),std::move(value)).second)
                throw std::runtime_error("Phase D duplicate serialized key");
        }
    }
}
template<class A,class X> void industry(A& a,X& x) {
    a(x.id);a(x.site);a(x.tech_id);
    vec(a,x.inputs,[](auto& ar,auto& v){input(ar,v);});
    vec(a,x.outputs,[](auto& ar,auto& v){input(ar,v);});
    a(x.workers_per_batch);a(x.capacity_per_batch);a(x.needs_retrofit);
}
template<class A,class X> void research(A& a,X& x) {
#define F(v) a(x.v)
    F(id);F(holder);F(sponsor);F(lab_site);F(region);F(material_good);F(tech_id);
    F(material_per_work);F(required_evidence);F(verified_work);F(planned_work);
    F(workers_per_work);F(cash_cost_per_work);F(paid_work);F(reviewed);
#undef F
}
template<class A,class X> void promotion(A& a,X& x) {
    a(x.holder);a(x.tech);a(x.next);
}
template<class A,class X> void adoption(A& a,X& x) {
    a(x.site);a(x.owner);a(x.tech);
}
template<class A,class X> void evidence(A& a,X& x) {
    a(x.project);a(x.month);a(x.units);
}
template<class A,class X> void state(A& a,X& d) {
    map(a,d.constructions,[](auto& ar,auto& v){construction(ar,v);});
    map(a,d.recipes,[](auto& ar,auto& v){industry(ar,v);});
    map(a,d.researches,[](auto& ar,auto& v){research(ar,v);});
    a(d.knowledge);a(d.active_site_tech);
    map(a,d.promotions,[](auto& ar,auto& vec_value) {
        vec(ar,vec_value,[](auto& a1,auto& e){promotion(a1,e);});
    });
    map(a,d.pending_adoptions,[](auto& ar,auto& vec_value) {
        vec(ar,vec_value,[](auto& a1,auto& e){adoption(a1,e);});
    });
    vec(a,d.research_evidence,[](auto& ar,auto& e){evidence(ar,e);});
    a(d.journal);a(d.labor_used);a(d.site_capacity_used);
    a(d.funds_reserved);a(d.event_sequence);a(d.opening_month);
}
} // namespace detail

struct SnapshotCodec {
    static constexpr const char* magic="VIC25_PHASED_SNAPSHOT_1";
    static constexpr const char* contract_version="phase-d-cpp/0.1";
    static std::string encode(const World& w) {
        w.validate();
        const auto core_payload=phasec::SnapshotCodec::encode(w.core_);
        std::ostringstream payload(std::ios::binary|std::ios::out);
        phasec::persistence_detail::BinaryWriter ar{payload};
        ar(std::string(magic));
        ar(std::string(contract_version));
        ar(std::string("phase-d-common/1.0"));
        ar(std::string("checked-int64/1"));
        ar(core_payload);
        detail::state(ar,w.domain_);
        const std::string bytes=payload.str();
        std::ostringstream trailer(std::ios::binary|std::ios::out);
        phasec::persistence_detail::BinaryWriter check{trailer};
        check(phasec::persistence_detail::checksum(bytes));
        return bytes+trailer.str();
    }
    static World decode(const std::string& bytes) {
        if(bytes.size()<48||bytes.size()>512'000'000)
            throw std::runtime_error("Phase D unsupported snapshot length");
        std::istringstream stream(bytes,std::ios::binary|std::ios::in);
        phasec::persistence_detail::BinaryReader rd{stream};
        std::string file_magic,ver,design_ver,numeric_ver,core_payload;
        rd(file_magic);rd(ver);rd(design_ver);rd(numeric_ver);
        if(file_magic!=magic || ver!=contract_version ||
           design_ver!="phase-d-common/1.0" || numeric_ver!="checked-int64/1")
            throw std::runtime_error("Phase D snapshot schema or contract mismatch");
        rd(core_payload);
        World candidate;
        candidate.core_=phasec::SnapshotCodec::decode(core_payload);
        detail::state(rd,candidate.domain_);
        std::uint64_t expected{};
        rd(expected);
        if(stream.peek()!=std::char_traits<char>::eof() ||
           expected!=phasec::persistence_detail::checksum(
                bytes.substr(0,bytes.size()-sizeof(std::uint64_t))))
            throw std::runtime_error("Phase D snapshot checksum mismatch");
        candidate.validate();
        return candidate;
    }
    static void save(const World& w,const std::filesystem::path& target) {
        if(std::filesystem::exists(target))
            throw std::runtime_error("refusing to overwrite Phase D checkpoint");
        const auto bytes=encode(w);
        const auto tmp=target.string()+".uncommitted";
        {
            std::ofstream out(tmp,std::ios::binary|std::ios::trunc);
            if(!out)throw std::runtime_error("Phase D checkpoint temp open failed");
            out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));
            out.flush();
            if(!out)throw std::runtime_error("Phase D checkpoint write failed");
        }
        std::filesystem::rename(tmp,target);
    }
    static World load(const std::filesystem::path& target) {
        std::ifstream in(target,std::ios::binary);
        if(!in)throw std::runtime_error("Phase D checkpoint missing");
        const std::string bytes{std::istreambuf_iterator<char>{in},
                                std::istreambuf_iterator<char>{}};
        return decode(bytes);
    }
};
} // namespace vic25::phased
