#include "vic25/phase_e_gl.hpp"
#include "vic25/phase_e_snapshot.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <numbers>
#include <stdexcept>
using namespace vic25::phasee;
static void review_tile(const char* path) {
    std::ifstream in(path);
    std::string format;
    unsigned width=0,height=0,face=0;
    gpu::ReviewTile tile;
    if(!(in>>format>>width>>height>>face>>tile.u_min>>tile.u_max>>tile.v_min>>tile.v_max)||
       format!="VIC25_ORESUND_CUBE_REVIEW_V1"||width!=gpu::ReviewTile::side||height!=gpu::ReviewTile::side||face!=4)
        throw std::runtime_error("invalid Oresund review tile header");
    tile.ids.resize(width*height);
    for(auto& id:tile.ids)
        if(!(in>>id)||(id>3&&id!=101))throw std::runtime_error("invalid Oresund review ID");
    std::string trailing;
    if(in>>trailing)throw std::runtime_error("trailing Oresund review tile data");
    gpu::RasterGlobe renderer;
    renderer.initialize(0,&tile);
    struct Sample { double lon,lat; unsigned id; };
    for(const auto [lon,lat,id]:std::array<Sample,7>{{
        {12.5188,55.975,1},{12.8273,55.9768,2},{12.7056,55.903,3},
        {12.63,55.91,101},{12.75,55.91,101},{12.65,56.04,101},
        {12.304263293985624,55.859762517148624,0}
    }}) {
        constexpr double rad=std::numbers::pi/180;
        const raster::Vector3 d{std::cos(lat*rad)*std::cos(lon*rad),
            std::cos(lat*rad)*std::sin(lon*rad),std::sin(lat*rad)};
        const auto pixel=renderer.review_pixel(d);
        const auto expected=gpu::RasterGlobe::review_color(id);
        if(!std::equal(pixel.begin(),pixel.begin()+3,expected.begin()))
            throw std::runtime_error("Oresund framebuffer ID mismatch at "+std::to_string(lon)+
                ", "+std::to_string(lat)+" (expected "+std::to_string(id)+
                ", sampled "+std::to_string(pixel[0])+")");
    }
    std::cout<<"ORESUND_REVIEW_OPENGL_FRAMEBUFFER PASS\n";
}

int main(int argc,char** argv) {
    try {
        if(argc==2){review_tile(argv[1]);return 0;}
        if(argc!=1)throw std::runtime_error("expected optional review tile path");
        World simulation(1234);
        simulation.seed_province(1,"R1","A");
        simulation.seed_unit(100,"A","A1");
        simulation.seed_assignment(1,100);
        const auto initial=SnapshotCodec::encode(simulation);
        gpu::RasterGlobe renderer;
        renderer.initialize(30000);
        const auto country=renderer.render(0);
        const auto administration=renderer.render(1);
        const auto military=renderer.render(2);
        if(!(country[0]>country[1]&&administration[1]>administration[0]&&
             military[2]>military[0])) {
            throw std::runtime_error("GPU political-layer color LUT failed");
        }
        // Deliberately destroy and re-create an *actual OpenGL context*.
        // This tests resource re-upload, not OS-level TDR/forced device reset.
        renderer.destroy();
        if(SnapshotCodec::encode(simulation)!=initial)
            throw std::runtime_error("GL context loss mutated world");
        renderer.initialize(30000);
        const auto restored=renderer.render(0);
        if(restored!=country||SnapshotCodec::encode(simulation)!=initial)
            throw std::runtime_error("GL resource rebuild inconsistency");
        std::cout<<"GPU_ACTUAL_GL33_RENDER PASS\n"
                 <<"EV-T45_GPU_LAYERS PASS\n"
                 <<"EV10_GPU_DENSE_INDEX_30000 PASS\n"
                 <<"EV-T49_CONTEXT_RECREATE PASS (not OS TDR)\n";
    }catch(const std::exception& e) {
        std::cerr<<"GPU FAIL: "<<e.what()<<"\n";
        return 1;
    }
    return 0;
}
