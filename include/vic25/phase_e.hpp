#pragma once
// VIC2.5 Phase E bounded real integration. Phase C remains the sole sovereignty
// and military-control authority; Phase D's World is the sole staged runtime.
// Geography owns immutable province identity/topology; administration and
// purpose-specific scopes are separate spatial relationships.
// GPU rendering, full historical GIS import and advanced electoral policy are
// explicitly NOT implemented by this header.
#include "vic25/phase_d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace vic25::phasee {
using Id=std::uint64_t;
using Month=std::int64_t;
using People=phaseb::People;
inline void require(bool ok,const char* why) {
    if(!ok)throw std::invalid_argument(why);
}
inline constexpr const char* contract_version="phase-e-common/1.0";

struct Province {
    Id id{};
    std::string region;
    std::uint64_t area_m2{};
    std::string source_hash;
    std::set<Id> neighbors;
    std::set<Id> coastal_seas;
};
struct AdministrativeUnit {
    Id id{};
    std::string name,country;
    Id parent{}; // 0 means root, not an arbitrary Province or State
    bool active{true};
};
enum class ScopeMode:std::uint8_t { explicit_provinces, derived_administrative };
struct TerritorialScope {
    Id id{};
    std::string kind;
    ScopeMode mode{ScopeMode::explicit_provinces};
    Id admin{};
    std::set<Id> provinces;
};
struct Claim {Id province{};std::string claimant;bool active{true};};
enum class OpKind:std::uint8_t {
    transfer, reassign, rename, create_admin, merge_admin,
    reparent_admin, add_claim, remove_claim, set_civil, set_fiscal,
    found_country
};
struct Operation {
    std::string id;
    OpKind kind{};
    std::vector<Id> provinces;
    Id unit{},other{};
    std::string value,other_value;
    Month effective{};
    std::string leader,minister;
    People chamber_seats{},quorum{};
};
struct ChangeLog {
    std::string event_id;
    OpKind kind{};
    Month effective{};
    std::vector<Id> provinces;
};
// This is a frozen GEOGRAPHIC district snapshot, not a second vote ledger.
// Ballot totals and anchor eligibility remain owned by Phase C.
struct PhysicalLink {
    Id from{},to{};
    std::string mode;
};
struct ElectoralBoundary {
    Month opened_month{};
    std::uint64_t boundary_revision{};
    std::map<Id,std::set<Id>> provinces_by_district;
    std::map<std::pair<Id,phaseb::Id>,People> allocated;
    std::map<std::string,Id> named_districts;
    std::set<Id> split_without_geometry; // POP partition only, no invented shapes
};
struct SpatialState {
    std::string dataset_hash{"synthetic-provinces/1"};
    std::map<Id,Province> provinces;
    std::map<std::string,Id> regions;
    std::map<Id,AdministrativeUnit> units;
    std::map<Id,Id> membership;
    std::map<Id,TerritorialScope> scopes;
    std::map<Id,std::set<std::string>> claims;
    std::map<Id,std::string> civil,fiscal;
    std::map<std::string,ElectoralBoundary> elections;
    std::map<std::string,PhysicalLink> geographic_links;
    std::map<Month,std::vector<Operation>> pending;
    std::set<std::string> commands;
    std::vector<ChangeLog> journal;
    std::uint64_t revision{1};
};

inline bool descendant_or_self(const SpatialState& s,Id node,Id ancestor) {
    std::set<Id> seen;
    while(node) {
        require(seen.insert(node).second,"ED0_ERR_ADMIN_CYCLE");
        const auto it=s.units.find(node);
        require(it!=s.units.end()&&it->second.active,"ED0_ERR_ADMIN_ASSIGNMENT");
        if(node==ancestor)return true;
        node=it->second.parent;
    }
    return false;
}
inline std::set<Id> unit_provinces(const SpatialState& s,Id unit) {
    std::set<Id> out;
    for(const auto& [p,leaf]:s.membership)
        if(descendant_or_self(s,leaf,unit))out.insert(p);
    return out;
}
inline void validate_spatial(const SpatialState& s,const phasec::IntegratedWorld& c) {
    std::set<std::string> regions;
    for(const auto& [id,p]:s.provinces) {
        require(id>0&&p.id==id&&!p.region.empty()&&p.area_m2>0,
                "ED0_ERR_INVALID_PROVINCE");
        require(regions.insert(p.region).second&&s.regions.contains(p.region)&&
                s.regions.at(p.region)==id,"ED0_ERR_INVALID_PROVINCE");
        // Throws if Phase C has no unique authoritative region.
        const auto& country=c.sovereignty(p.region);
        require(!country.empty(),"ED0_ERR_JURISDICTION_CONFLICT");
        const auto m=s.membership.find(id);
        require(m!=s.membership.end()&&s.units.contains(m->second),
                "ED0_ERR_ADMIN_ASSIGNMENT");
        const auto& leaf=s.units.at(m->second);
        require(leaf.active&&leaf.country==country,"ED0_ERR_ADMIN_ASSIGNMENT");
        for(Id n:p.neighbors) {
            require(n!=id&&s.provinces.contains(n)&&
                    s.provinces.at(n).neighbors.contains(id),
                    "ED0_ERR_INVALID_PROVINCE");
        }
        for(Id sea:p.coastal_seas)
            require(sea!=0,"ED0_ERR_INVALID_PROVINCE");
    }
    require(s.regions.size()==s.provinces.size()&&s.membership.size()==s.provinces.size(),
            "ED0_ERR_ADMIN_ASSIGNMENT");
    for(const auto& [id,u]:s.units) {
        require(id!=0&&u.id==id&&!u.country.empty(),"ED0_ERR_ADMIN_ASSIGNMENT");
        if(!u.active)continue;
        std::set<Id> path;
        Id parent=id;
        while(parent) {
            require(path.insert(parent).second,"ED0_ERR_ADMIN_CYCLE");
            const auto it=s.units.find(parent);
            require(it!=s.units.end()&&it->second.active,"ED0_ERR_ADMIN_ASSIGNMENT");
            require(it->second.country==u.country,"ED0_ERR_ADMIN_ASSIGNMENT");
            parent=it->second.parent;
        }
    }
    for(const auto& [id,sc]:s.scopes) {
        require(id>0&&sc.id==id&&!sc.kind.empty(),"ED0_ERR_INVALID_SCOPE");
        if(sc.mode==ScopeMode::derived_administrative) {
            require(sc.admin>0&&s.units.contains(sc.admin)&&s.units.at(sc.admin).active,
                    "ED0_ERR_INVALID_SCOPE");
        } else {
            for(Id p:sc.provinces)
                require(s.provinces.contains(p),"ED0_ERR_INVALID_SCOPE");
        }
    }
    for(const auto& [id,link]:s.geographic_links) {
        require(!id.empty()&&link.from!=link.to&&
                s.provinces.contains(link.from)&&s.provinces.contains(link.to)&&
                !link.mode.empty(),"ED0_ERR_INVALID_PROVINCE");
    }
    for(const auto& [contest,boundary]:s.elections) {
        require(!contest.empty()&&!boundary.provinces_by_district.empty(),
                "ED0_ERR_INVALID_SCOPE");
        for(const auto& [district,ids]:boundary.provinces_by_district) {
            require(s.scopes.contains(district)&&!ids.empty(),"ED0_ERR_INVALID_SCOPE");
            for(Id p:ids)require(s.provinces.contains(p),"ED0_ERR_INVALID_SCOPE");
        }
        for(const auto& [key,count]:boundary.allocated) {
            require(boundary.provinces_by_district.contains(key.first)&&count>0,
                    "ED0_ERR_INVALID_SCOPE");
        }
        for(const auto& [anchor,district]:boundary.named_districts) {
            require(!anchor.empty()&&boundary.provinces_by_district.contains(district),
                    "ED0_ERR_INVALID_SCOPE");
        }
    }
    for(const auto& [p,claims]:s.claims) {
        require(s.provinces.contains(p),"ED0_ERR_INVALID_PROVINCE");
        for(const auto& country:claims)require(!country.empty(),"ED0_ERR_INVALID_AUTHORITY");
    }
    for(const auto& table:{&s.civil,&s.fiscal})
        for(const auto& [p,authority]:*table)
            require(s.provinces.contains(p)&&!authority.empty(),
                    "ED0_ERR_INVALID_AUTHORITY");
}
inline std::set<Id> scope_members_of(const SpatialState& s,Id id) {
    require(s.scopes.contains(id),"ED0_ERR_INVALID_SCOPE");
    const auto& sc=s.scopes.at(id);
    return sc.mode==ScopeMode::derived_administrative?
           unit_provinces(s,sc.admin):sc.provinces;
}
inline void apply_operation(SpatialState& s,phasec::IntegratedWorld& c,const Operation& op) {
    require(!op.id.empty(),"ED0_ERR_DUPLICATE_EVENT");
    auto valid_province=[&](Id id) {
        require(s.provinces.contains(id),"ED0_ERR_INVALID_PROVINCE");
    };
    switch(op.kind) {
    case OpKind::found_country: {
        require(op.unit!=0&&!s.units.contains(op.unit)&&
                !op.value.empty()&&!op.other_value.empty() &&
                !op.provinces.empty()&&!op.leader.empty()&&!op.minister.empty()&&
                op.chamber_seats>0&&op.quorum>0&&op.quorum<=op.chamber_seats,
                "ED0_ERR_INVALID_AUTHORITY");
        std::set<Id> unique(op.provinces.begin(),op.provinces.end());
        require(unique.size()==op.provinces.size(),"ED0_ERR_INVALID_PROVINCE");
        for(Id id:unique) {
            valid_province(id);
            require(c.sovereignty(s.provinces.at(id).region)!=op.value,
                    "ED0_ERR_INVALID_AUTHORITY");
        }
        s.units.emplace(op.unit,AdministrativeUnit{
            op.unit,op.other_value,op.value,0,true});
        for(Id id:unique) {
            c.phase_e_transfer_sovereignty(s.provinces.at(id).region,op.value);
            s.membership[id]=op.unit;
        }
        c.phase_e_found_government(op.value,op.leader,op.minister,
                                   op.chamber_seats,op.quorum);
        break;
    }
    case OpKind::transfer: {
        require(s.units.contains(op.unit)&&s.units.at(op.unit).active&&
                s.units.at(op.unit).country==op.value && !op.provinces.empty(),
                "ED0_ERR_ADMIN_ASSIGNMENT");
        std::set<Id> unique(op.provinces.begin(),op.provinces.end());
        require(unique.size()==op.provinces.size(),"ED0_ERR_INVALID_PROVINCE");
        for(Id id:unique)valid_province(id);
        for(Id id:unique) {
            const auto& p=s.provinces.at(id);
            c.phase_e_transfer_sovereignty(p.region,op.value);
            s.membership[id]=op.unit;
        }
        break;
    }
    case OpKind::reassign:
        require(op.provinces.size()==1,"ED0_ERR_ADMIN_ASSIGNMENT");
        valid_province(op.provinces.front());
        require(s.units.contains(op.unit)&&s.units.at(op.unit).active &&
                s.units.at(op.unit).country==
                c.sovereignty(s.provinces.at(op.provinces.front()).region),
                "ED0_ERR_ADMIN_ASSIGNMENT");
        s.membership[op.provinces.front()]=op.unit;
        break;
    case OpKind::rename:
        require(s.units.contains(op.unit)&&s.units.at(op.unit).active &&
                !op.value.empty(),"ED0_ERR_ADMIN_ASSIGNMENT");
        s.units.at(op.unit).name=op.value;
        break;
    case OpKind::create_admin:
        require(op.unit!=0&&!s.units.contains(op.unit)&&!op.value.empty()&&
                !op.other_value.empty(),"ED0_ERR_ADMIN_ASSIGNMENT");
        if(op.other)require(s.units.contains(op.other)&&s.units.at(op.other).active&&
                             s.units.at(op.other).country==op.other_value,
                             "ED0_ERR_ADMIN_ASSIGNMENT");
        s.units.emplace(op.unit,AdministrativeUnit{op.unit,op.value,op.other_value,op.other,true});
        break;
    case OpKind::merge_admin: {
        require(s.units.contains(op.unit)&&s.units.contains(op.other)&&
                op.unit!=op.other&&s.units.at(op.unit).active&&
                s.units.at(op.other).active&&
                s.units.at(op.unit).country==s.units.at(op.other).country,
                "ED0_ERR_ADMIN_ASSIGNMENT");
        for(const auto& [id,u]:s.units)
            if(u.active&&u.parent==op.other)
                require(false,"ED0_ERR_ADMIN_ASSIGNMENT");
        for(auto& [p,leaf]:s.membership)
            if(leaf==op.other)leaf=op.unit;
        s.units.at(op.other).active=false;
        break;
    }
    case OpKind::reparent_admin:
        require(s.units.contains(op.unit)&&s.units.at(op.unit).active &&
                (!op.other||(s.units.contains(op.other)&&s.units.at(op.other).active&&
                             s.units.at(op.unit).country==s.units.at(op.other).country)) &&
                (!op.other||!descendant_or_self(s,op.other,op.unit)),
                "ED0_ERR_ADMIN_CYCLE");
        s.units.at(op.unit).parent=op.other;
        break;
    case OpKind::add_claim:
    case OpKind::remove_claim:
        require(op.provinces.size()==1&&!op.value.empty(),"ED0_ERR_INVALID_AUTHORITY");
        valid_province(op.provinces.front());
        if(op.kind==OpKind::add_claim)s.claims[op.provinces.front()].insert(op.value);
        else s.claims[op.provinces.front()].erase(op.value);
        break;
    case OpKind::set_civil:
    case OpKind::set_fiscal:
        require(op.provinces.size()==1&&!op.value.empty(),"ED0_ERR_INVALID_AUTHORITY");
        valid_province(op.provinces.front());
        (op.kind==OpKind::set_civil?s.civil:s.fiscal)[op.provinces.front()]=op.value;
        break;
    }
    ++s.revision;
    s.journal.push_back({op.id,op.kind,op.effective,op.provinces});
}

class SpatialStage {
    phased::Stage& d_;
    SpatialState& s_;
    int stage_{};
public:
    SpatialStage(phased::Stage& d,SpatialState& s,int phase):d_(d),s_(s),stage_(phase){}
    [[nodiscard]] phased::Stage& phase_d(){return d_;}
    [[nodiscard]] phasec::IntegratedWorld& phase_c(){return d_.phase_c();}
    [[nodiscard]] const SpatialState& spatial() const{return s_;}
    void schedule(Operation op) {
        require(stage_==9,"ED0_ERR_TIME_CONFLICT");
        require(!op.id.empty()&&s_.commands.insert(op.id).second,
                "ED0_ERR_DUPLICATE_EVENT");
        op.effective=phase_c().month()+1;
        for(const auto& [p,queued]:s_.pending)
            if(p==op.effective)
                for(const auto& old:queued)
                    if(old.kind==op.kind &&
                       (old.unit==op.unit&&op.unit!=0 &&
                        (op.kind==OpKind::rename||op.kind==OpKind::create_admin||
                         op.kind==OpKind::merge_admin||op.kind==OpKind::reparent_admin) ||
                        (!old.provinces.empty()&&!op.provinces.empty()&&
                         std::any_of(old.provinces.begin(),old.provinces.end(),
                            [&](Id id){return std::find(op.provinces.begin(),op.provinces.end(),id)!=op.provinces.end();}))))
                        require(false,"ED0_ERR_REVISION_CONFLICT");
        s_.pending[op.effective].push_back(std::move(op));
    }
    // Atomic multi-province secession with an actual Phase C government.
    void queue_independence(std::string event,std::vector<Id> ids,
                            std::string country,Id new_admin,
                            std::string admin_name,std::string head,
                            std::string prime,People seats,People quorum) {
        std::sort(ids.begin(),ids.end());
        Operation op{std::move(event),OpKind::found_country,std::move(ids),
                     new_admin,0,std::move(country),std::move(admin_name)};
        op.leader=std::move(head);op.minister=std::move(prime);
        op.chamber_seats=seats;op.quorum=quorum;
        schedule(std::move(op));
    }
    void queue_transfer(std::string event,std::vector<Id> ids,
                        std::string country,Id new_leaf) {
        std::sort(ids.begin(),ids.end());
        schedule({std::move(event),OpKind::transfer,std::move(ids),new_leaf,0,
                  std::move(country),""});
    }
    void queue_reassign(std::string event,Id province,Id leaf){
        schedule({std::move(event),OpKind::reassign,{province},leaf});
    }
    void queue_rename(std::string event,Id unit,std::string name) {
        schedule({std::move(event),OpKind::rename,{},unit,0,std::move(name)});
    }
    void queue_create_admin(std::string event,Id id,std::string name,
                            std::string country,Id parent=0) {
        schedule({std::move(event),OpKind::create_admin,{},id,parent,
                  std::move(name),std::move(country)});
    }
    void queue_merge(std::string event,Id target,Id absorbed) {
        schedule({std::move(event),OpKind::merge_admin,{},target,absorbed});
    }
    void queue_reparent(std::string event,Id unit,Id parent) {
        schedule({std::move(event),OpKind::reparent_admin,{},unit,parent});
    }
    void queue_claim(std::string event,Id p,std::string claimant,bool enable) {
        schedule({std::move(event),enable?OpKind::add_claim:OpKind::remove_claim,
                  {p},0,0,std::move(claimant)});
    }
    void queue_civil(std::string event,Id p,std::string authority) {
        schedule({std::move(event),OpKind::set_civil,{p},0,0,std::move(authority)});
    }
    void queue_fiscal(std::string event,Id p,std::string authority) {
        schedule({std::move(event),OpKind::set_fiscal,{p},0,0,std::move(authority)});
    }
    void open_election(std::string contest,std::vector<Id> districts) {
        require(stage_==9&&!contest.empty()&&!districts.empty()&&
                !s_.elections.contains(contest),"ED0_ERR_INVALID_SCOPE");
        ElectoralBoundary record;
        record.opened_month=phase_c().month();
        record.boundary_revision=s_.revision;
        std::set<Id> all;
        std::set<Id> shared;
        for(Id district:districts) {
            require(!record.provinces_by_district.contains(district),
                    "ED0_ERR_INVALID_SCOPE");
            const auto ids=scope_members_of(s_,district);
            require(!ids.empty(),"ED0_ERR_INVALID_SCOPE");
            for(Id id:ids)if(!all.insert(id).second)shared.insert(id);
            record.provinces_by_district.emplace(district,ids);
        }
        record.split_without_geometry=std::move(shared);
        s_.elections.emplace(std::move(contest),std::move(record));
    }
    // A cohort can be apportioned across districts only by explicit integer
    // counts: no arbitrary geographic micro-polygons are ever manufactured.
    void allocate_electors(const std::string& contest,Id district,
                           phaseb::Id slice,People count) {
        require(stage_==9&&s_.elections.contains(contest)&&count>0,
                "ED0_ERR_INVALID_SCOPE");
        auto& b=s_.elections.at(contest);
        require(b.provinces_by_district.contains(district) &&
                !b.allocated.contains({district,slice}),
                "ED0_ERR_INVALID_SCOPE");
        const auto& p=phase_c().people();
        require(p.has_slice(slice),"ED0_ERR_INVALID_SCOPE");
        const auto& region=p.demographic(p.slice(slice).key.demographic).region;
        require(s_.regions.contains(region) &&
                b.provinces_by_district.at(district).contains(s_.regions.at(region)),
                "ED0_ERR_INVALID_SCOPE");
        People sum=count;
        for(const auto& [key,n]:b.allocated)
            if(key.second==slice)sum=phaseb::add_checked(sum,n);
        require(sum<=phase_c().phase_e_eligible_slice(slice),
                "ED0_ERR_INVALID_SCOPE");
        b.allocated[{district,slice}]=count;
    }
    void register_elector(const std::string& contest,Id district,
                          const std::string& anchor) {
        require(stage_==9&&s_.elections.contains(contest)&&!anchor.empty(),
                "ED0_ERR_INVALID_SCOPE");
        auto& b=s_.elections.at(contest);
        require(!b.named_districts.contains(anchor),"ED0_ERR_INVALID_SCOPE");
        const auto slice=phase_c().phase_e_anchor_slice(anchor);
        const auto it=b.allocated.find({district,slice});
        require(it!=b.allocated.end(),"ED0_ERR_INVALID_SCOPE");
        People booked=0;
        for(const auto& [name,d]:b.named_districts)
            if(d==district&&phase_c().phase_e_anchor_slice(name)==slice)++booked;
        require(booked<it->second,"ED0_ERR_INVALID_SCOPE");
        b.named_districts.emplace(anchor,district);
    }
    void cast_elector_ballot(const std::string& contest,Id district,
                            const std::string& anchor,const std::string& party) {
        require(stage_==9&&s_.elections.contains(contest),"ED0_ERR_INVALID_SCOPE");
        const auto& b=s_.elections.at(contest);
        require(b.named_districts.contains(anchor)&&b.named_districts.at(anchor)==district,
                "ED0_ERR_INVALID_SCOPE");
        phase_c().cast_named_ballot(contest,std::to_string(district),anchor,party);
    }
    void occupy(Id province,const std::string& army) {
        require(stage_==10&&s_.provinces.contains(province),"ED0_ERR_INVALID_PROVINCE");
        phase_c().occupy(s_.provinces.at(province).region,army);
    }
};

class World {
    friend struct SnapshotCodec;
public:
    using Task=std::function<void(SpatialStage&)>;
    using Plan=std::array<std::vector<Task>,12>;
private:
    phased::World core_;
    SpatialState spatial_;
    static void due(SpatialState& next,phasec::IntegratedWorld& core) {
        const auto it=next.pending.find(core.month());
        if(it==next.pending.end())return;
        std::vector<Operation> ops=it->second;
        std::sort(ops.begin(),ops.end(),[](const Operation& a,const Operation& b){
            // Create administrative structures first, then sovereignty/assignment
            const auto priority=[](OpKind k){
                if(k==OpKind::create_admin)return 0;
                if(k==OpKind::found_country)return 1;
                if(k==OpKind::reparent_admin)return 1;
                if(k==OpKind::transfer)return 2;
                return 3;
            };
            if(priority(a.kind)!=priority(b.kind))
                return priority(a.kind)<priority(b.kind);
            return a.id<b.id;
        });
        for(const auto& op:ops)apply_operation(next,core,op);
        next.pending.erase(it);
    }
public:
    explicit World(Month start=24000):core_(start){}
    static World from_legacy_phase_d(phased::World legacy) {
        World result(legacy.month());
        result.core_=std::move(legacy);
        result.validate();
        return result;
    }
    phasec::AdvanceReport advance_daily(const phasec::IntegratedWorld::Plan& commands) {
        World staged=*this;
        const auto report=staged.core_.advance_daily(commands);
        staged.validate();
        *this=std::move(staged);
        return report;
    }
    phased::World& seed_core() {
        require(core_.commit_id()==0,"ED0_ERR_TIME_CONFLICT");
        return core_;
    }
    void set_dataset(std::string hash) {
        require(core_.commit_id()==0&&!hash.empty(),"ED0_ERR_DATASET_MISMATCH");
        spatial_.dataset_hash=std::move(hash);
    }
    void seed_province(Id id,std::string region,std::string country,
                       std::uint64_t area_m2=1000) {
        require(core_.commit_id()==0&&id>0&&!region.empty()&&!country.empty()&&
                area_m2>0&&!spatial_.provinces.contains(id)&&
                !spatial_.regions.contains(region),"ED0_ERR_INVALID_PROVINCE");
        core_.seed_core().seed_region(region,country);
        spatial_.provinces.emplace(id,Province{id,region,area_m2,
                                                  spatial_.dataset_hash,{},{}});
        spatial_.regions.emplace(std::move(region),id);
    }
    void seed_border(Id a,Id b) {
        require(core_.commit_id()==0&&a!=b&&spatial_.provinces.contains(a)&&
                spatial_.provinces.contains(b),"ED0_ERR_INVALID_PROVINCE");
        require(!spatial_.provinces.at(a).neighbors.contains(b),
                "ED0_ERR_INVALID_PROVINCE");
        spatial_.provinces.at(a).neighbors.insert(b);
        spatial_.provinces.at(b).neighbors.insert(a);
    }
    void seed_coast(Id p,Id sea) {
        require(core_.commit_id()==0&&sea>0&&spatial_.provinces.contains(p),
                "ED0_ERR_INVALID_PROVINCE");
        spatial_.provinces.at(p).coastal_seas.insert(sea);
    }
    void seed_unit(Id id,std::string country,std::string name,Id parent=0) {
        require(core_.commit_id()==0&&id>0&&!spatial_.units.contains(id)&&
                !country.empty()&&!name.empty()&&
                (!parent||(spatial_.units.contains(parent)&&
                           spatial_.units.at(parent).country==country)),
                "ED0_ERR_ADMIN_ASSIGNMENT");
        spatial_.units.emplace(id,AdministrativeUnit{id,std::move(name),
                                                      std::move(country),parent,true});
    }
    void seed_assignment(Id province,Id leaf) {
        require(core_.commit_id()==0&&spatial_.provinces.contains(province)&&
                spatial_.units.contains(leaf)&&
                spatial_.units.at(leaf).country==
                    core_.core().sovereignty(spatial_.provinces.at(province).region) &&
                !spatial_.membership.contains(province),"ED0_ERR_ADMIN_ASSIGNMENT");
        spatial_.membership.emplace(province,leaf);
    }
    // A geographic connection is not a shipment or transport-capacity ledger.
    // Phase F will own actual transport capacity, tolls and routing.
    void seed_geographic_link(std::string link_id,Id from,Id to,std::string mode) {
        require(core_.commit_id()==0&&!link_id.empty()&&
                !spatial_.geographic_links.contains(link_id)&&
                spatial_.provinces.contains(from)&&spatial_.provinces.contains(to)&&
                from!=to&&!mode.empty(),"ED0_ERR_INVALID_PROVINCE");
        spatial_.geographic_links.emplace(std::move(link_id),
                                            PhysicalLink{from,to,std::move(mode)});
    }
    [[nodiscard]] const PhysicalLink& geographic_link(const std::string& id)const {
        return spatial_.geographic_links.at(id);
    }
    void seed_scope(TerritorialScope scope) {
        require(core_.commit_id()==0&&scope.id>0&&
                !spatial_.scopes.contains(scope.id),"ED0_ERR_INVALID_SCOPE");
        if(scope.mode==ScopeMode::derived_administrative)
            require(spatial_.units.contains(scope.admin),"ED0_ERR_INVALID_SCOPE");
        else for(Id p:scope.provinces)
            require(spatial_.provinces.contains(p),"ED0_ERR_INVALID_SCOPE");
        spatial_.scopes.emplace(scope.id,std::move(scope));
    }
    // Apply the legally scheduled spatial operation on the FIRST calendar
    // day of its effective month; MonthClose never applies it twice.
    phasec::AdvanceReport advance_daily(const Plan& tasks,
                                         bool first_calendar_day=false,
                                         bool final_calendar_day=false) {
        SpatialState staged=spatial_;
        phased::World::Plan bridge;
        for(int i=0;i<12;++i)
            bridge[static_cast<std::size_t>(i)].push_back(
                [&,i](phased::Stage& inner) {
                    if(i==0&&first_calendar_day)due(staged,inner.phase_c());
                    SpatialStage s{inner,staged,i};
                    for(const auto& callback:tasks[static_cast<std::size_t>(i)])
                        callback(s);
                    if(i==11)validate_spatial(staged,inner.phase_c());
                });
        static_assert(std::is_nothrow_move_assignable_v<SpatialState>);
        const auto report=core_.advance_daily(bridge,first_calendar_day,
                                               final_calendar_day);
        spatial_=std::move(staged);
        return report;
    }
    [[nodiscard]] bool has_ready_daily_commission()const {
        return core_.has_ready_daily_commission();
    }
    [[nodiscard]] const SpatialState& spatial() const{return spatial_;}
    [[nodiscard]] const phased::World& core() const{return core_;}
    [[nodiscard]] Month month()const{return core_.month();}
    [[nodiscard]] std::uint64_t commit_id()const{return core_.commit_id();}
    [[nodiscard]] const Province& province(Id id)const{return spatial_.provinces.at(id);}
    [[nodiscard]] const std::string& sovereign(Id id)const {
        return core_.core().sovereignty(province(id).region);
    }
    [[nodiscard]] const std::string& military_control(Id id)const {
        return core_.core().military_control(province(id).region);
    }
    [[nodiscard]] Id leaf(Id p)const{return spatial_.membership.at(p);}
    [[nodiscard]] std::set<Id> provinces_in_unit(Id unit)const {
        require(spatial_.units.contains(unit)&&spatial_.units.at(unit).active,
                "ED0_ERR_ADMIN_ASSIGNMENT");
        return unit_provinces(spatial_,unit);
    }
    [[nodiscard]] const ElectoralBoundary& election(const std::string& contest)const {
        return spatial_.elections.at(contest);
    }
    // Exactly reproducible contracted-wage numerator and denominator.
    // This does NOT claim to be realized wage payments from the financial ledger.
    [[nodiscard]] std::pair<std::int64_t,People> contractual_wage_mean(
                  const std::set<Id>& ids)const {
        std::set<std::string> regions;
        for(Id id:ids)regions.insert(province(id).region);
        std::int64_t wage_sum=0;
        People worker_sum=0;
        const auto& pop=core_.core().people();
        for(const auto& [id,job]:pop.jobs()) {
            if(!pop.has_slice(id))continue;
            const auto& region=pop.demographic(pop.slice(id).key.demographic).region;
            if(!regions.contains(region))continue;
            wage_sum=phaseb::add_checked(wage_sum,
                       phaseb::mul_checked(job.wage_per_month,job.count));
            worker_sum=phaseb::add_checked(worker_sum,job.count);
        }
        return {wage_sum,worker_sum};
    }
    [[nodiscard]] std::set<Id> scope_members(Id scope)const {
        const auto& sc=spatial_.scopes.at(scope);
        return sc.mode==ScopeMode::derived_administrative?
               provinces_in_unit(sc.admin):sc.provinces;
    }
    [[nodiscard]] std::set<Id> sovereign_provinces(const std::string& country)const {
        std::set<Id> ids;
        for(const auto& [id,p]:spatial_.provinces)
            if(core_.core().sovereignty(p.region)==country)ids.insert(id);
        return ids;
    }
    [[nodiscard]] People population(const std::set<Id>& ids)const {
        std::set<std::string> regions;
        for(Id id:ids)regions.insert(province(id).region);
        People total=0;
        const auto& p=core_.core().people();
        for(const auto& [id,slice]:p.slices()) {
            (void)id;
            if(regions.contains(p.demographic(slice.key.demographic).region))
                total=phaseb::add_checked(total,slice.count);
        }
        return total;
    }
    [[nodiscard]] People scope_population(Id id)const {
        return population(scope_members(id));
    }
    [[nodiscard]] std::set<std::pair<Id,Id>> state_borders()const {
        std::set<std::pair<Id,Id>> out;
        for(const auto& [id,p]:spatial_.provinces)
            for(Id neighbor:p.neighbors)
                if(id<neighbor&&sovereign(id)!=sovereign(neighbor))
                    out.insert({id,neighbor});
        return out;
    }
    void validate()const {
        core_.validate();
        validate_spatial(spatial_,core_.core());
    }
    phasec::AdvanceReport advance(const Plan& plan={}) {
        SpatialState staged=spatial_;
        phased::World::Plan bridge;
        for(int i=0;i<12;++i) {
            bridge[static_cast<std::size_t>(i)].push_back(
                [&,i](phased::Stage& live) {
                    if(i==0)due(staged,live.phase_c());
                    SpatialStage stage{live,staged,i};
                    for(const auto& task:plan[static_cast<std::size_t>(i)])task(stage);
                    if(i==11)validate_spatial(staged,live.phase_c());
                });
        }
        static_assert(std::is_nothrow_move_assignable_v<SpatialState>);
        const auto report=core_.advance(bridge);
        spatial_=std::move(staged);
        return report;
    }
};

// Portable CPU integer-ID raster/picking foundation. E-G5 GPU cube-sphere
// streaming, border AA and actual performance remain separate unimplemented
// renderer obligations. This deliberately cannot claim 3D GPU completion.
struct RasterIdTile {
    std::uint32_t width{},height{};
    std::vector<Id> ids;
    Id sample(std::uint32_t x,std::uint32_t y)const {
        require(width>0&&height>0&&ids.size()==static_cast<std::size_t>(width)*height,
                "ED0_ERR_DATASET_MISMATCH");
        require(x<width&&y<height,"ED0_ERR_INVALID_PROVINCE");
        return ids[static_cast<std::size_t>(y)*width+x];
    }
    Id sample_lonlat(double lon,double lat)const {
        require(std::isfinite(lon)&&std::isfinite(lat)&&lon>=-180.0&&
                lon<=180.0&&lat>=-90.0&&lat<=90.0,
                "ED0_ERR_INVALID_PROVINCE");
        const double x=(lon+180.0)/360.0;
        const double y=(90.0-lat)/180.0;
        return sample(std::min(width-1,static_cast<std::uint32_t>(x*width)),
                      std::min(height-1,static_cast<std::uint32_t>(y*height)));
    }
};
struct Ray {
    double ox{},oy{},oz{},dx{},dy{},dz{};
};
struct LonLat {double lon{},lat{};};
inline LonLat sphere_pick(const Ray& r) {
    const double a=r.dx*r.dx+r.dy*r.dy+r.dz*r.dz;
    require(a>0&&std::isfinite(a),"ED0_ERR_INVALID_PROVINCE");
    const double b=2*(r.ox*r.dx+r.oy*r.dy+r.oz*r.dz);
    const double c=r.ox*r.ox+r.oy*r.oy+r.oz*r.oz-1.0;
    const double d=b*b-4*a*c;
    require(d>=0&&std::isfinite(d),"ED0_ERR_INVALID_PROVINCE");
    const double root=std::sqrt(d);
    const double t1=(-b-root)/(2*a),t2=(-b+root)/(2*a);
    const double t=t1>=0?t1:t2;
    require(t>=0,"ED0_ERR_INVALID_PROVINCE");
    const double x=r.ox+t*r.dx,y=r.oy+t*r.dy,z=r.oz+t*r.dz;
    constexpr double to_degrees=180.0/3.14159265358979323846;
    return {std::atan2(y,x)*to_degrees,
            std::atan2(z,std::sqrt(x*x+y*y))*to_degrees};
}
} // namespace vic25::phasee
