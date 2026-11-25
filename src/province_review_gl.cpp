#include "vic25/phase_e_gl.hpp"
#include <array>
#include <charconv>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>

namespace review=vic25::phasee::province_review;
using vic25::phasee::gpu::RasterGlobe;

template<class T> static T number(std::string_view input) {
    T value{};
    const auto [end,ec]=std::from_chars(input.data(),input.data()+input.size(),value);
    if(ec!=std::errc{}||end!=input.data()+input.size())
        throw std::invalid_argument("invalid numeric argument");
    return value;
}

static void framebuffer_test() {
    using Pixel=std::array<unsigned char,4>;
    // Repeated RGB across water classes, black land and NoData are intentional.
    const std::array<Pixel,6> source{{
        {0,0,0,0}, {0,0,0,1}, {17,91,201,2},
        {17,91,201,3}, {207,61,13,1}, {23,109,79,1},
    }};
    const std::array<Pixel,6> kinds{{
        {54,57,66,255},{112,163,96,255},{36,89,162,255},
        {76,194,210,255},{112,163,96,255},{112,163,96,255},
    }};
    std::string bytes;
    for(const auto& cell:source)bytes.append(reinterpret_cast<const char*>(cell.data()),cell.size());
    // Use a noncentral rectangle on every face to catch transposes/reflections.
    for(unsigned face=0;face<6;++face) {
        std::istringstream in(bytes,std::ios::binary);
        const auto tile=review::Tile::read(in,{3,2,face,-.52,-.11,.38,.64});
        RasterGlobe renderer;
        renderer.initialize_review(tile);
        for(unsigned y=0;y<2;++y)for(unsigned x=0;x<3;++x) {
            const auto i=y*3+x;
            const double u=-.52+(x+.5)*.30,v=-.11+(y+.5)*.375;
            const auto ids=i==0?kinds[0]:Pixel{source[i][0],source[i][1],source[i][2],255};
            if(renderer.province_review_pixel(u,v)!=kinds[i]||
               renderer.province_review_pixel(u,v,true)!=ids)
                throw std::runtime_error("RGB-kind framebuffer mismatch on face "+std::to_string(face));
        }
    }
    std::cout<<"PROVINCE_REVIEW_RGBI_OPENGL_FRAMEBUFFER PASS\n";
}

int main(int argc,char** argv) {
    try {
        if(argc==2&&std::string_view(argv[1])=="--self-test") {
            framebuffer_test();
            return 0;
        }
        if(argc!=9)
            throw std::invalid_argument("usage: vic25_province_review_gl PATH WIDTH HEIGHT FACE UMIN VMIN UMAX VMAX");
        std::ifstream in(argv[1],std::ios::binary);
        if(!in)throw std::invalid_argument("cannot open RGB-kind review file");
        const auto tile=review::Tile::read(in,{number<unsigned>(argv[2]),number<unsigned>(argv[3]),
            number<unsigned>(argv[4]),number<double>(argv[5]),number<double>(argv[6]),
            number<double>(argv[7]),number<double>(argv[8])});
        RasterGlobe renderer;
        renderer.initialize_review(tile,true);
        renderer.show_review();
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"PROVINCE REVIEW: "<<e.what()<<'\n';
        return 1;
    }
}
