#pragma once
// Phase E compiled presentation index: not a second writable geographic authority.
// Province IDs and borders come from Phase E, sovereignty from Phase C.
// Renderer reads dense immutable-ish arrays; only explicit changed IDs are patched.
// No per-frame traversal of the country/POP/administration registries.
#include "vic25/phase_e.hpp"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <map>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace vic25::phasee::compact {
class ProvinceIndex {
    std::vector<Id> province_ids_;                // 64-bit canonical IDs
    std::vector<std::uint32_t> starts_;          // CSR offsets
    std::vector<std::uint32_t> neighbors_;       // dense indices
    std::vector<std::uint32_t> country_;         // renderer palette indices
    std::vector<std::uint32_t> admin_;           // stable AdminUnitId check below
    std::vector<std::uint32_t> military_;        // renderer palette indices
    std::map<std::string,std::uint32_t> country_names_;
    std::map<std::string,std::uint32_t> military_names_;
    std::string dataset_;
    std::set<std::pair<Id,Id>> dirty_edges_;
    std::vector<std::uint32_t> last_changed_;
    std::uint64_t committed_revision_{};
    static std::uint32_t intern(std::map<std::string,std::uint32_t>& names,
                                const std::string& name){
        if(const auto it=names.find(name);it!=names.end())return it->second;
        require(names.size()<0xfffffffeu,"EV10_GPU_PALETTE_EXHAUSTED");
        const auto id=static_cast<std::uint32_t>(names.size()+1);
        names.emplace(name,id);
        return id;
    }
    [[nodiscard]] std::uint32_t index_of(Id id) const{
        const auto it=std::lower_bound(province_ids_.begin(),province_ids_.end(),id);
        require(it!=province_ids_.end()&&*it==id,"ED0_ERR_INVALID_PROVINCE");
        return static_cast<std::uint32_t>(it-province_ids_.begin());
    }
    void patch_one(const World& w,Id id){
        const auto index=index_of(id);
        const auto country=intern(country_names_,w.sovereign(id));
        const auto army=intern(military_names_,w.military_control(id));
        const auto admin=w.leaf(id);
        require(admin<=0xffffffffu,"EV10_ADMIN_GPU_INDEX_LIMIT");
        if(country_[index]==country&&military_[index]==army&&
           admin_[index]==admin)return;
        country_[index]=country;
        military_[index]=army;
        admin_[index]=static_cast<std::uint32_t>(admin);
        last_changed_.push_back(index);
        for(std::uint32_t j=starts_[index];j<starts_[index+1];++j)
            dirty_edges_.insert(std::minmax(id,province_ids_[neighbors_[j]]));
    }
public:
    explicit ProvinceIndex(const World& w) {rebuild(w);}
    void rebuild(const World& w) {
        province_ids_.clear();starts_.clear();neighbors_.clear();
        country_.clear();admin_.clear();military_.clear();
        country_names_.clear();military_names_.clear();
        last_changed_.clear();dirty_edges_.clear();
        dataset_=w.spatial().dataset_hash;
        require(w.spatial().provinces.size()<=0xfffffffeu,
                "EV10_GPU_INDEX_LIMIT");
        for(const auto& [id,definition]:w.spatial().provinces) {
            (void)definition; province_ids_.push_back(id);
        }
        starts_.reserve(province_ids_.size()+1);
        country_.reserve(province_ids_.size());
        admin_.reserve(province_ids_.size());
        military_.reserve(province_ids_.size());
        for(Id id:province_ids_){
            starts_.push_back(static_cast<std::uint32_t>(neighbors_.size()));
            const auto& source=w.province(id);
            for(Id n:source.neighbors)
                neighbors_.push_back(index_of(n));
            country_.push_back(intern(country_names_,w.sovereign(id)));
            military_.push_back(intern(military_names_,w.military_control(id)));
            require(w.leaf(id)<=0xffffffffu,"EV10_ADMIN_GPU_INDEX_LIMIT");
            admin_.push_back(static_cast<std::uint32_t>(w.leaf(id)));
        }
        starts_.push_back(static_cast<std::uint32_t>(neighbors_.size()));
        committed_revision_=w.commit_id();
    }
    void apply(const World& committed,std::span<const Id> changed) {
        require(dataset_==committed.spatial().dataset_hash &&
                province_ids_.size()==committed.spatial().provinces.size(),
                "EV10_DATASET_CHANGE_REQUIRES_REBUILD");
        last_changed_.clear();dirty_edges_.clear();
        std::vector<Id> canonical(changed.begin(),changed.end());
        std::sort(canonical.begin(),canonical.end());
        canonical.erase(std::unique(canonical.begin(),canonical.end()),canonical.end());
        for(Id id:canonical)patch_one(committed,id);
        committed_revision_=committed.commit_id();
    }
    [[nodiscard]] std::uint32_t palette(Id id)const {
        return country_.at(index_of(id));
    }
    [[nodiscard]] std::uint32_t administration(Id id)const {
        return admin_.at(index_of(id));
    }
    [[nodiscard]] std::uint32_t military_palette(Id id)const {
        return military_.at(index_of(id));
    }
    [[nodiscard]] std::size_t province_count()const{return province_ids_.size();}
    [[nodiscard]] std::size_t edge_directed_count()const{return neighbors_.size();}
    [[nodiscard]] std::size_t dirty_border_count()const{return dirty_edges_.size();}
    [[nodiscard]] const std::vector<std::uint32_t>& changed_gpu_indices()const {
        return last_changed_;
    }
    [[nodiscard]] std::size_t estimated_bytes()const {
        return province_ids_.capacity()*sizeof(Id)+
               (starts_.capacity()+neighbors_.capacity()+country_.capacity()+
                admin_.capacity()+military_.capacity()+last_changed_.capacity())*
               sizeof(std::uint32_t);
    }
    [[nodiscard]] std::uint64_t source_commit_id()const{return committed_revision_;}
    [[nodiscard]] const std::string& dataset()const{return dataset_;}
    [[nodiscard]] const std::vector<Id>& ids()const{return province_ids_;}
    [[nodiscard]] const std::vector<std::uint32_t>& country_gpu()const{return country_;}
    [[nodiscard]] const std::vector<std::uint32_t>& admin_gpu()const{return admin_;}
    [[nodiscard]] const std::vector<std::uint32_t>& military_gpu()const{return military_;}
};

// A simulation worker can advance an entire C/D/E StagingWorld while the
// renderer continues reading its PREVIOUS committed immutable frame. Replacing
// the pointer is atomic; no per-frame POP/administration or map locks are held.
struct FrameSnapshot {
    std::uint64_t commit_id{};
    std::string dataset;
    std::vector<Id> province_ids;
    std::vector<std::uint32_t> country,admin,military;
    [[nodiscard]] std::size_t estimated_bytes()const {
        return province_ids.size()*sizeof(Id)+
               (country.size()+admin.size()+military.size())*sizeof(std::uint32_t);
    }
};
class FrameExchange {
    std::atomic<std::shared_ptr<const FrameSnapshot>> current_;
public:
    explicit FrameExchange(const ProvinceIndex& index) {publish(index);}
    void publish(const ProvinceIndex& index) {
        auto next=std::make_shared<FrameSnapshot>();
        next->commit_id=index.source_commit_id();
        next->dataset=index.dataset();
        next->province_ids=index.ids();
        next->country=index.country_gpu();
        next->admin=index.admin_gpu();
        next->military=index.military_gpu();
        require(next->country.size()==next->province_ids.size()&&
                next->admin.size()==next->province_ids.size()&&
                next->military.size()==next->province_ids.size(),
                "EV10_FRAME_SNAPSHOT_INTEGRITY");
        current_.store(std::move(next),std::memory_order_release);
    }
    [[nodiscard]] std::shared_ptr<const FrameSnapshot> acquire()const {
        return current_.load(std::memory_order_acquire);
    }
};
} // namespace vic25::phasee::compact
