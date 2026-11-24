#pragma once
// Private, single-window review data. Not a production province or route map.
#include "vic25/phase_e_raster.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <istream>
#include <optional>
#include <stdexcept>
#include <vector>

namespace vic25::phasee::province_review {

struct Window {
    unsigned width{},height{},face{};
    double u_min{},v_min{},u_max{},v_max{};
};

enum class Kind : std::uint8_t { missing=0, land=1, ocean=2, lake=3 };

struct Texel {
    std::array<std::uint8_t,3> rgb{};
    Kind kind{};
    // Source color + kind is a review-local label, not a gameplay ID.
    auto operator==(const Texel&) const -> bool = default;
    std::array<std::uint8_t,4> rgba() const {
        return {rgb[0],rgb[1],rgb[2],static_cast<std::uint8_t>(kind==Kind::missing ? 0 : 255)};
    }
};

class Tile {
    Window window_;
    std::vector<std::uint8_t> bytes_;

    explicit Tile(Window window) : window_(window) {}

    static bool valid_interval(double lo,double hi) {
        return std::isfinite(lo)&&std::isfinite(hi)&&lo>=-1&&hi<=1&&lo<hi;
    }

public:
    static constexpr unsigned max_side=1024;

    // Seekable binary stream for one complete file, with no embedded header.
    // Refuse unknown size, trailing data, and truncated reads.
    static Tile read(std::istream& in,Window window) {
        const auto [width,height,face,u0,v0,u1,v1]=window;
        if (!width||!height||width>max_side||height>max_side||face>5||
            !valid_interval(u0,u1)||!valid_interval(v0,v1))
            throw std::invalid_argument("invalid RGB-kind review window");
        const auto size=static_cast<std::size_t>(width)*height*4;
        in.seekg(0,std::ios::end);
        if (!in||in.tellg()!=static_cast<std::streamoff>(size))
            throw std::invalid_argument("invalid RGB-kind review byte count");
        in.seekg(0,std::ios::beg);
        if (!in) throw std::invalid_argument("unreadable RGB-kind review stream");
        Tile result(window);
        result.bytes_.resize(size);
        if (!in.read(reinterpret_cast<char*>(result.bytes_.data()),static_cast<std::streamsize>(size))||
            in.peek()!=std::char_traits<char>::eof()||in.bad())
            throw std::invalid_argument("RGB-kind review data size changed or read failed");
        const auto& bytes=result.bytes_;
        for (std::size_t i=0;i<size;i+=4) {
            if (bytes[i+3]>3||(!bytes[i+3]&&(bytes[i]||bytes[i+1]||bytes[i+2])))
                throw std::invalid_argument("invalid RGB-kind review texel");
        }
        return result;
    }

    const Window& window() const { return window_; }

    Texel at(unsigned column,unsigned row) const {
        if (column>=window_.width||row>=window_.height)
            throw std::out_of_range("outside RGB-kind review window");
        const auto i=4*(static_cast<std::size_t>(row)*window_.width+column);
        return {{bytes_[i],bytes_[i+1],bytes_[i+2]},static_cast<Kind>(bytes_[i+3])};
    }

    // Half-open [min,max) on each axis. No remap to neighboring faces.
    // An engaged missing texel defines a covered review location with NoData;
    // an empty optional means the direction is not in this window at all.
    std::optional<Texel> sample(raster::Vector3 dir) const {
        if (!std::isfinite(dir.x)||!std::isfinite(dir.y)||!std::isfinite(dir.z)||
            (dir.x==0&&dir.y==0&&dir.z==0)) return std::nullopt;
        const auto c=raster::direction_to_cube(dir);
        const auto [w,h,face,u0,v0,u1,v1]=window_;
        if (c.face!=face||c.u<u0||c.u>=u1||c.v<v0||c.v>=v1) return std::nullopt;
        const auto x=std::min(w-1,static_cast<unsigned>((c.u-u0)/(u1-u0)*w));
        const auto y=std::min(h-1,static_cast<unsigned>((c.v-v0)/(v1-v0)*h));
        return at(x,y);
    }
};

} // namespace vic25::phasee::province_review
