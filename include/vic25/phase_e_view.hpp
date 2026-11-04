#pragma once
// View only. The province and its political affiliation remain in Phase E/C,
// independent of terrain LOD, palette, GPU mode or label placement.
#include "vic25/phase_e.hpp"
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace vic25::phasee::view {
enum class Layer { sovereign, administration, military };
struct GeoMarker {Id province{};double longitude{},latitude{};};
class Display {
    const World* w_{};
    Layer mode_{Layer::sovereign};
public:
    explicit Display(const World& w):w_(&w){}
    void set_layer(Layer x){mode_=x;}
    [[nodiscard]] Layer layer()const{return mode_;}
    [[nodiscard]] std::string owner(Id id)const{
        switch(mode_){
        case Layer::sovereign:return w_->sovereign(id);
        case Layer::military:return w_->military_control(id);
        case Layer::administration:return "ADMIN:"+std::to_string(w_->leaf(id));
        }
        throw std::logic_error("unsupported map layer");
    }
};
// Locale/scale UI can render fallback dots for provinces smaller than the
// active LOD's raster coverage. The source marker must be georeferenced to
// the real ProvinceId and never substituted for a canonical precise geometry.
class Locator {
    std::map<Id,GeoMarker> markers_;
public:
    void add(Id id,double lon,double lat,const World& w) {
        require(w.spatial().provinces.contains(id)&&std::isfinite(lon)&&
                std::isfinite(lat)&&lon>=-180&&lon<=180&&lat>=-90&&lat<=90,
                "ED0_ERR_INVALID_PROVINCE");
        markers_[id]={id,lon,lat};
    }
    [[nodiscard]] std::vector<GeoMarker> invisible_in(
          const std::set<Id>& coarse_visible)const{
        std::vector<GeoMarker> result;
        for(const auto& [id,marker]:markers_)
            if(!coarse_visible.contains(id))result.push_back(marker);
        return result;
    }
    [[nodiscard]] std::optional<Id> pick_near(
              double lon,double lat,double max_degrees,
              const std::set<Id>& coarse_visible)const {
        require(std::isfinite(lon)&&std::isfinite(lat)&&
                std::isfinite(max_degrees)&&max_degrees>=0,"ED0_ERR_INVALID_PROVINCE");
        std::optional<Id> picked;
        double best=max_degrees*max_degrees;
        for(const auto& m:invisible_in(coarse_visible)){
            const double longitudeDelta=std::remainder(m.longitude-lon,360.0);
            const double latitudeDelta=m.latitude-lat;
            const double dist=longitudeDelta*longitudeDelta+
                              latitudeDelta*latitudeDelta;
            if(dist<=best) {
                if(dist<best||!picked||m.province<*picked) {
                    picked=m.province;best=dist;
                }
            }
        }
        return picked;
    }
};
} // namespace vic25::phasee::view
