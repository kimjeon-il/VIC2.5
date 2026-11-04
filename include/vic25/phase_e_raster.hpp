#pragma once
// E-G1/E-G5 headless geographic compiler and cube-sphere addressing.
// Canonical map pixels are unsigned Province IDs, never RGB political colors.
// No OpenGL/Vulkan runtime or GPU claims in this file.
#include "vic25/phase_e.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace vic25::phasee::raster {
struct ProvinceMask {
    Id id{};
    std::vector<std::uint32_t> pixels;
};
struct CompiledRaster {
    RasterIdTile base;
    std::map<Id,std::set<Id>> neighbors;
    std::map<Id,std::uint32_t> component_count;
};
inline CompiledRaster compile_masks(std::uint32_t width,std::uint32_t height,
                                    const std::vector<bool>& land,
                                    const std::vector<ProvinceMask>& masks) {
    require(width>0&&height>0&&
            static_cast<std::uint64_t>(width)*height<=std::numeric_limits<std::uint32_t>::max()&&
            land.size()==static_cast<std::size_t>(width)*height,
            "ED0_ERR_DATASET_MISMATCH");
    CompiledRaster out;out.base.width=width;out.base.height=height;
    out.base.ids.assign(land.size(),0);
    for(const auto& mask:masks) {
        require(mask.id!=0&&!mask.pixels.empty(),"ED0_ERR_INVALID_PROVINCE");
        for(auto pixel:mask.pixels) {
            require(pixel<land.size()&&land[pixel],"ED0_ERR_DATASET_MISMATCH");
            require(out.base.ids[pixel]==0,"ED0_ERR_DATASET_MISMATCH");
            out.base.ids[pixel]=mask.id;
        }
    }
    for(std::size_t i=0;i<land.size();++i)
        require((out.base.ids[i]!=0)==land[i],"ED0_ERR_DATASET_MISMATCH");
    auto neighbors_of=[&](std::size_t i) {
        std::array<std::size_t,4> a{std::numeric_limits<std::size_t>::max(),
                                    std::numeric_limits<std::size_t>::max(),
                                    std::numeric_limits<std::size_t>::max(),
                                    std::numeric_limits<std::size_t>::max()};
        const auto x=i%width,y=i/width;
        if(x>0)a[0]=i-1;
        if(x+1<width)a[1]=i+1;
        if(y>0)a[2]=i-width;
        if(y+1<height)a[3]=i+width;
        return a;
    };
    std::vector<bool> visited(land.size());
    for(std::size_t i=0;i<land.size();++i) {
        const Id id=out.base.ids[i];
        if(!id||visited[i])continue;
        ++out.component_count[id];
        visited[i]=true;
        std::vector<std::size_t> q{i};
        for(std::size_t k=0;k<q.size();++k)
            for(auto candidate:neighbors_of(q[k])) {
                if(candidate>=land.size())continue;
                const auto other=out.base.ids[candidate];
                if(other&&other!=id) {
                    out.neighbors[id].insert(other);
                    out.neighbors[other].insert(id);
                }
                if(other==id&&!visited[candidate]) {
                    visited[candidate]=true;
                    q.push_back(candidate);
                }
            }
    }
    // All perimeter contacts, including contacts around visited components,
    // are collected in the traversal above. Diagonal/corner-only is excluded.
    return out;
}
struct Vector3{double x{},y{},z{};};
struct CubeUV{std::uint8_t face{};double u{},v{};};
inline CubeUV direction_to_cube(Vector3 d) {
    require(std::isfinite(d.x)&&std::isfinite(d.y)&&std::isfinite(d.z),
            "ED0_ERR_INVALID_PROVINCE");
    const double x=std::abs(d.x),y=std::abs(d.y),z=std::abs(d.z);
    const double major=std::max({x,y,z});
    require(major>0,"ED0_ERR_INVALID_PROVINCE");
    // Deterministic tie preference X > Y > Z; face index and signs stable.
    if(x>=y&&x>=z) {
        return d.x>=0?CubeUV{0,-d.z/major,d.y/major}:
                      CubeUV{1,d.z/major,d.y/major};
    }
    if(y>=z) {
        return d.y>=0?CubeUV{2,d.x/major,-d.z/major}:
                      CubeUV{3,d.x/major,d.z/major};
    }
    return d.z>=0?CubeUV{4,d.x/major,d.y/major}:
                  CubeUV{5,-d.x/major,d.y/major};
}
inline Vector3 cube_to_direction(CubeUV c) {
    require(c.face<6&&std::abs(c.u)<=1&&std::abs(c.v)<=1,
            "ED0_ERR_INVALID_PROVINCE");
    Vector3 v{};
    switch(c.face){
      case 0:v={1,c.v,-c.u};break;
      case 1:v={-1,c.v,c.u};break;
      case 2:v={c.u,1,-c.v};break;
      case 3:v={c.u,-1,c.v};break;
      case 4:v={c.u,c.v,1};break;
      default:v={-c.u,c.v,-1};break;
    }
    const double scale=1.0/std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);
    return {v.x*scale,v.y*scale,v.z*scale};
}
struct TileAddress {
    std::uint8_t face{};
    std::uint8_t level{};
    std::uint32_t x{},y{};
    auto operator<=>(const TileAddress&)const=default;
};
inline TileAddress address(Vector3 direction,std::uint8_t level) {
    require(level<=24,"ED0_ERR_DATASET_MISMATCH");
    const auto cube=direction_to_cube(direction);
    const auto scale=std::uint32_t{1}<<level;
    const auto x=std::min(scale-1,static_cast<std::uint32_t>((cube.u+1)*.5*scale));
    const auto y=std::min(scale-1,static_cast<std::uint32_t>((cube.v+1)*.5*scale));
    return {cube.face,level,x,y};
}
class TileCache {
    struct Entry {
        RasterIdTile tile;
        std::uint64_t last_use{};
        std::size_t bytes{};
    };
    std::set<TileAddress> pending_;
    std::map<TileAddress,Entry> loaded_;
    std::size_t resident_bytes_{};
    std::size_t max_tiles_{128};
    std::size_t byte_budget_{96u*1024u*1024u};
    std::uint64_t clock_{};
    std::uint64_t evictions_{};
    void evict_one() {
        require(!loaded_.empty(),"EV10_EMPTY_TILE_CACHE");
        auto oldest=loaded_.begin();
        for(auto it=std::next(loaded_.begin());it!=loaded_.end();++it)
            if(it->second.last_use<oldest->second.last_use)
                oldest=it; // TileAddress key ordering breaks exact ties.
        resident_bytes_-=oldest->second.bytes;
        loaded_.erase(oldest);
        ++evictions_;
    }
public:
    explicit TileCache(std::size_t max_tiles=128,
                       std::size_t max_bytes=96u*1024u*1024u)
        :max_tiles_(max_tiles),byte_budget_(max_bytes) {
        require(max_tiles_>0&&byte_budget_>0,"EV10_INVALID_TILE_BUDGET");
    }
    void request(TileAddress a) {
        if(!loaded_.contains(a))pending_.insert(a);
    }
    void arrive(TileAddress a,RasterIdTile tile) {
        require(pending_.contains(a)&&tile.width>0&&tile.height>0&&
                tile.ids.size()==static_cast<std::size_t>(tile.width)*tile.height,
                "ED0_ERR_DATASET_MISMATCH");
        const auto bytes=tile.ids.size()*sizeof(Id);
        require(bytes<=byte_budget_,"EV10_TILE_EXCEEDS_MEMORY_BUDGET");
        while(!loaded_.empty()&&
              (loaded_.size()>=max_tiles_ ||
               resident_bytes_>byte_budget_-bytes))evict_one();
        pending_.erase(a);
        resident_bytes_+=bytes;
        loaded_.emplace(a,Entry{std::move(tile),++clock_,bytes});
    }
    void mark_visible(TileAddress a) {
        if(auto it=loaded_.find(a);it!=loaded_.end())
            it->second.last_use=++clock_;
    }
    [[nodiscard]] std::optional<Id> sample(TileAddress a,std::uint32_t x,std::uint32_t y)const {
        auto it=loaded_.find(a);
        if(it==loaded_.end())return std::nullopt;
        return it->second.tile.sample(x,y);
    }
    [[nodiscard]] bool pending(TileAddress a)const {return pending_.contains(a);}
    [[nodiscard]] std::size_t loaded_count()const{return loaded_.size();}
    [[nodiscard]] std::size_t resident_bytes()const{return resident_bytes_;}
    [[nodiscard]] std::uint64_t eviction_count()const{return evictions_;}
};
class DirtyProvinceSet {
    std::set<Id> changed_;
    std::set<std::pair<Id,Id>> border_edges_;
public:
    void mark(Id id,const SpatialState& world) {
        require(world.provinces.contains(id),"ED0_ERR_INVALID_PROVINCE");
        changed_.insert(id);
        for(Id neighbor:world.provinces.at(id).neighbors)
            border_edges_.insert(std::minmax(id,neighbor));
    }
    [[nodiscard]] const std::set<Id>& changed()const{return changed_;}
    [[nodiscard]] const std::set<std::pair<Id,Id>>& edges()const{return border_edges_;}
};
} // namespace vic25::phasee::raster
