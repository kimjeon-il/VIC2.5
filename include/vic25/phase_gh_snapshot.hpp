#pragma once
// Version-locked atomic snapshot of the complete existing Phase E/C/D world,
// plus Phase G and Phase H original states. No parallel financial/inventory
// authority: restored Money and stock still live solely in Phase C.
#include "vic25/phase_gh.hpp"
#include "vic25/phase_e_snapshot.hpp"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <type_traits>
#include <utility>

namespace vic25::phasegh {
namespace persisted {
using Writer=phasec::persistence_detail::BinaryWriter;
using Reader=phasec::persistence_detail::BinaryReader;

template<class A,class T,class Fn>
void checked_vector(A& a,std::vector<T>& xs,Fn fn) {
    std::uint64_t count=0;
    if constexpr(std::is_same_v<A,Writer>)count=xs.size();
    a(count);
    if(count>5'000'000)throw std::runtime_error("G/H oversized event vector");
    if constexpr(std::is_same_v<A,Writer>) {
        for(auto& x:xs){auto copy=x;fn(a,copy);}
    } else {
        xs.clear();xs.reserve(static_cast<std::size_t>(count));
        for(std::uint64_t i=0;i<count;++i) {
            T value{};fn(a,value);xs.push_back(std::move(value));
        }
    }
}
template<class A,class K,class V,class Fn>
void checked_map(A& a,std::map<K,V>& map,Fn fn) {
    std::uint64_t count=0;
    if constexpr(std::is_same_v<A,Writer>)count=map.size();
    a(count);
    if(count>5'000'000)throw std::runtime_error("G/H oversized state map");
    if constexpr(std::is_same_v<A,Writer>) {
        for(const auto& [key,val]:map) {
            auto k=key,v=val;
            a(k);fn(a,v);
        }
    } else {
        map.clear();
        for(std::uint64_t i=0;i<count;++i) {
            K key{};V value{};a(key);fn(a,value);
            if(!map.emplace(std::move(key),std::move(value)).second)
                throw std::runtime_error("G/H duplicate snapshot key");
        }
    }
}
template<class A>void route(A& a,Route& x) {
    a(x.id);a(x.seller);a(x.buyer);a(x.good);
    a(x.origin);a(x.destination);a(x.maximum_quantity);
    a(x.travel_months);a(x.arrival_loss);
}
template<class A>void edge(A& a,TransportEdge& x) {
    a(x.id);a(x.carrier);a(x.from);a(x.to);
    a(x.monthly_capacity);a(x.freight_per_unit);
    a(x.transit_months);a(x.enabled);
}
template<class A>void fill(A& a,TradeFill& x) {
    a(x.execution_id);a(x.route_id);a(x.seller_country);a(x.buyer_country);
    a(x.month);a(x.quantity);a(x.goods_value);a(x.duty_paid);
    a(x.shipped);a(x.freight_paid);a(x.contract_id);a(x.currency);
    a(x.physical_path);
}
template<class A>void planned(A& a,PlannedTrade& x) {
    a(x.execution_id);a(x.route_id);a(x.seller);a(x.buyer);
    a(x.good);a(x.origin_region);a(x.destination_region);
    a(x.seller_country);a(x.buyer_country);a(x.quantity);
    a(x.travel_months);a(x.unit_price);a(x.total_cost);
    a(x.duty);a(x.freight);a(x.contract_id);
    a(x.settlement_currency);a(x.path_edges);
    a(x.confirmed_arrival_loss);
}
template<class A>void customs(A& a,CustomsRecord& x) {
    a(x.execution_id);a(x.importer);a(x.treasury);a(x.good);
    a(x.policy_source);a(x.month);a(x.taxable_value);
    a(x.assessed);a(x.paid);a(x.cleared_quantity);
}
template<class A>void contract(A& a,TradeContract& x) {
    a(x.id);a(x.seller);a(x.buyer);a(x.good);a(x.currency);
    a(x.origin);a(x.destination);a(x.contracted);a(x.fulfilled);
    a(x.monthly_limit);a(x.unit_price);a(x.effective);a(x.expires);
    a(x.partial);a(x.active);
}
template<class A>void credit(A& a,CreditFacility& x) {
    a(x.id);a(x.borrower);a(x.pledge_region);a(x.pledge_good);
    a(x.pledged_goods);a(x.limit);a(x.drawn);
    a(x.effective);a(x.active);
}
template<class A>void insurance(A& a,CargoInsurance& x) {
    a(x.id);a(x.shipment_id);a(x.insurer);a(x.beneficiary);
    a(x.premium);a(x.maximum_claim);a(x.claim_due);a(x.claim_paid);
    a(x.insured_quantity);a(x.premium_paid);a(x.claim_registered);
}
template<class A>void g_state(A& a,TradeState& x) {
    checked_map(a,x.routes,route<A>);
    checked_map(a,x.transport_edges,edge<A>);
    a(x.transport_adjacency);a(x.edge_usage);
    checked_map(a,x.contracts,contract<A>);
    checked_map(a,x.credit,credit<A>);
    checked_map(a,x.insurance,insurance<A>);
    checked_vector(a,x.customs,customs<A>);
    a(x.fx_held);
    checked_vector(a,x.pending,planned<A>);
    checked_vector(a,x.fills,fill<A>);
    a(x.executed);a(x.revision);
}
template<class A>void tariff(A& a,Tariff& x) {
    a(x.importer_country);a(x.good);a(x.ppm);
}
template<class A>void pending_tariff(A& a,PendingTariff& x) {
    a(x.effective);a(x.importer);a(x.good);a(x.ppm);
}
template<class A>void clause(A& a,TreatyClause& x) {
    a(x.id);a(x.obligor);a(x.beneficiary);a(x.good);
    a(x.kind);a(x.province);a(x.receiving_admin);
    a(x.amount);a(x.fulfilled);a(x.effective);a(x.expires);a(x.queued);
}
template<class A>void treaty(A& a,Treaty& x) {
    a(x.id);a(x.country_a);a(x.country_b);a(x.good);
    a(x.agreed_month);a(x.effective_month);
    a(x.ratified_a);a(x.ratified_b);a(x.active);
    a(x.parties);a(x.ratifications);
    checked_vector(a,x.clauses,clause<A>);
    a(x.pending_termination);a(x.termination_approvals);
}
template<class A>void governance(A& a,OrganizationRule& x) {
    // size_t is platform-dependent: store portable u64.
    std::uint64_t quorum=0,required=0;
    if constexpr(std::is_same_v<A,Writer>) {
        quorum=x.quorum;required=x.yes_required;
    }
    a(quorum);a(required);a(x.veto_members);
    if constexpr(std::is_same_v<A,Reader>) {
        if(quorum>100000||required>100000)
            throw std::runtime_error("H invalid organization governance");
        x.quorum=static_cast<std::size_t>(quorum);
        x.yes_required=static_cast<std::size_t>(required);
    }
}
template<class A>void vote(A& a,OrganizationVote& x) {
    a(x.decision_id);a(x.organization_id);a(x.subject);
    a(x.ballots);a(x.adopted);a(x.effective);
}
template<class A>void organization(A& a,Organization& x) {
    a(x.id);a(x.members);a(x.effective_month);a(x.dues_per_member);
    a(x.active);a(x.paid);governance(a,x.governance);
    checked_map(a,x.decisions,vote<A>);
    a(x.pending_admissions);a(x.pending_exits);
}
template<class A>void protectorate(A& a,Protectorate& x) {
    a(x.id);a(x.principal);a(x.dependent);
    a(x.effective_month);a(x.active);a(x.exclusive_foreign_policy);
}
template<class A>void position(A& a,CrisisPosition& x) {
    a(x.country);a(x.stance);a(x.conditional);a(x.month);
}
template<class A>void proposal(A& a,CrisisProposal& x) {
    a(x.id);a(x.proposer);a(x.compensation);
    a(x.payer);a(x.recipient);a(x.freeze_claim);
    a(x.approvers);a(x.month);
}
template<class A>void crisis(A& a,Crisis& x) {
    a(x.id);a(x.claimant);a(x.respondent);a(x.mediator);
    a(x.province);a(x.effective_month);a(x.status);
    checked_map(a,x.positions,position<A>);
    checked_map(a,x.proposals,proposal<A>);
    a(x.issues);
}
template<class A>void recognition(A& a,Recognition& x) {
    a(x.recognizer);a(x.recognized);a(x.effective_month);
}
template<class A>void culture(A& a,CulturalExchange& x) {
    a(x.event_id);a(x.sponsor);a(x.beneficiary);
    a(x.funded);a(x.month);
}
template<class A>void aid(A& a,MilitaryAssistance& x) {
    a(x.event_id);a(x.provider);a(x.receiver);a(x.good);
    a(x.shipment_id);a(x.quantity);a(x.market_value);a(x.month);
}
template<class A>void h_state(A& a,DiplomacyState& x) {
    checked_map(a,x.tariffs,tariff<A>);
    checked_vector(a,x.scheduled_tariffs,pending_tariff<A>);
    checked_map(a,x.treaties,treaty<A>);
    checked_map(a,x.organizations,organization<A>);
    checked_map(a,x.protectorates,protectorate<A>);
    checked_map(a,x.crises,crisis<A>);
    checked_vector(a,x.recognitions,recognition<A>);
    a(x.reparation_links);a(x.commands);
    checked_vector(a,x.funded_exchanges,culture<A>);
    checked_vector(a,x.military_assistance,aid<A>);
    a(x.quota_used);a(x.revision);
}
} // namespace persisted

struct SnapshotCodec {
    static constexpr const char* magic="VIC25_PHASE_GH_SNAPSHOT_1";
    static constexpr const char* version="phase-gh-cpp/0.2";
    static std::string encode(const World& w) {
        w.validate();
        std::ostringstream out(std::ios::binary|std::ios::out);
        persisted::Writer a{out};
        a(std::string(magic));
        a(std::string(version));
        a(std::string(g_version));
        a(std::string(h_version));
        const std::string existing=phasee::SnapshotCodec::encode(w.core_);
        a(existing);
        auto g=w.trade_,h=w.diplomacy_;
        persisted::g_state(a,g);persisted::h_state(a,h);
        const std::string payload=out.str();
        std::ostringstream end(std::ios::binary|std::ios::out);
        persisted::Writer check{end};
        check(phasec::persistence_detail::checksum(payload));
        return payload+end.str();
    }
    static World decode(const std::string& bytes) {
        if(bytes.size()<96||bytes.size()>768'000'000)
            throw std::runtime_error("G/H invalid snapshot size");
        std::istringstream in(bytes,std::ios::binary|std::ios::in);
        persisted::Reader a{in};
        std::string m,v,g,h,base;
        a(m);a(v);a(g);a(h);
        if(m!=magic||v!=version||g!=g_version||h!=h_version)
            throw std::runtime_error("G/H incompatible snapshot contract");
        a(base);
        World next;
        next.core_=phasee::SnapshotCodec::decode(base);
        persisted::g_state(a,next.trade_);
        persisted::h_state(a,next.diplomacy_);
        std::uint64_t crc{};a(crc);
        if(in.peek()!=std::char_traits<char>::eof()||
           crc!=phasec::persistence_detail::checksum(
                    bytes.substr(0,bytes.size()-sizeof(std::uint64_t))))
            throw std::runtime_error("G/H snapshot checksum mismatch");
        next.validate();
        return next;
    }
    static void save(const World& w,const std::filesystem::path& target) {
        if(std::filesystem::exists(target))
            throw std::runtime_error("G/H checkpoint already exists");
        const std::string bytes=encode(w);
        const auto tmp=target.string()+".uncommitted";
        {
            std::ofstream file(tmp,std::ios::binary|std::ios::trunc);
            if(!file)throw std::runtime_error("G/H checkpoint open failed");
            file.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));
            file.flush();
            if(!file)throw std::runtime_error("G/H checkpoint write failed");
        }
        std::filesystem::rename(tmp,target);
    }
    static World load(const std::filesystem::path& path) {
        std::ifstream file(path,std::ios::binary);
        if(!file)throw std::runtime_error("G/H checkpoint missing");
        const std::string bytes{std::istreambuf_iterator<char>{file},
                                std::istreambuf_iterator<char>{}};
        return decode(bytes);
    }
};
} // namespace vic25::phasegh
