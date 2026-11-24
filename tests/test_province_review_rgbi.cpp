#include "vic25/province_review_rgbi.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace vic25::phasee;
namespace review = province_review;

static void check(bool yes) {
    if (!yes) throw std::runtime_error("RGB-kind diagnostic check failed");
}
template<class F> static void rejects(F action) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("accepted malformed RGB-kind review tile");
}

int main() {
    // Asymmetric 3x2 native array. rgba differs from rgb-kind in its fourth byte.
    const std::array<unsigned char,24> bytes{
        0,0,0,0, 0,0,0,1, 15,31,47,2,
        15,31,47,3, 2,5,8,1, 29,13,7,1,
    };
    const std::string payload(bytes.begin(),bytes.end());
    const auto read = [&](review::Window area,const std::string& data) {
        std::istringstream in(data,std::ios::binary);
        return review::Tile::read(in,area);
    };
    // Geographic orientation anchors, independent from cube_to_direction.
    // u,v at every non-square cell center; x/y/z sign/order differs by face.
    for (unsigned face=0;face<6;++face) {
        const review::Window area{3,2,face,-.6,.10,.4,.50};
        const auto tile=read(area,payload);
        for (unsigned y=0;y<2;++y) for (unsigned x=0;x<3;++x) {
            const double u=-.6+(x+.5)/3;
            const double v=.10+(y+.5)*.20;
            const std::array<raster::Vector3,6> directions{{
                {1,v,-u},{-1,v,u},{u,1,-v},{u,-1,v},{u,v,1},{-u,v,-1},
            }};
            const auto texel=tile.at(x,y);
            check(texel==tile.sample(directions[face]));
            const auto i=4u*(y*3+x);
            check(texel.rgba()==std::array<std::uint8_t,4>{bytes[i],bytes[i+1],bytes[i+2],
                                                         static_cast<std::uint8_t>(bytes[i+3] ? 255 : 0)});
            check(static_cast<unsigned>(texel.kind)==bytes[i+3]);
        }
        check(tile.at(2,0)!=tile.at(0,1)); // same RGB, ocean != lake
        check(tile.at(1,0)!=tile.at(0,0)); // black land != NoData
        check(!tile.sample({0,0,0}));
        check(!tile.sample(raster::cube_to_direction({static_cast<std::uint8_t>(face),.65,.3})));
        check(!tile.sample(raster::cube_to_direction({static_cast<std::uint8_t>((face+1)%6),0,0})));
    }
    const review::Window area{3,2,4,-.6,.10,.4,.50};
    rejects([&]{ read({0,2,4,-.6,.1,.4,.5},payload); });
    rejects([&]{ read({3,1025,4,-.6,.1,.4,.5},payload); });
    rejects([&]{ read({3,2,6,-.6,.1,.4,.5},payload); });
    rejects([&]{ read({3,2,4,-1.1,.1,.4,.5},payload); });
    rejects([&]{ read({3,2,4,+.4,.1,-.6,.5},payload); });
    rejects([&]{ read({3,2,4,-.6,.1,.4,std::numeric_limits<double>::quiet_NaN()},payload); });
    rejects([&]{ read(area,payload.substr(1)); });
    rejects([&]{ read(area,payload+"x"); });
    auto bad=payload; bad[0]=1;
    rejects([&]{ read(area,bad); });
    bad=payload; bad[7]=4;
    rejects([&]{ read(area,bad); });
    std::cout<<"PROVINCE_REVIEW_RGBI PASS\n";
}
