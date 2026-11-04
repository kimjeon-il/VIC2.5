#include "vic25/phase_e_gl.hpp"
#include "vic25/phase_e_snapshot.hpp"
#include <iostream>
#include <stdexcept>
using namespace vic25::phasee;
int main() {
    try {
        World simulation(1234);
        simulation.seed_province(1,"R1","A");
        simulation.seed_unit(100,"A","A1");
        simulation.seed_assignment(1,100);
        const auto initial=SnapshotCodec::encode(simulation);
        gpu::RasterGlobe renderer;
        renderer.initialize();
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
        renderer.initialize();
        const auto restored=renderer.render(0);
        if(restored!=country||SnapshotCodec::encode(simulation)!=initial)
            throw std::runtime_error("GL resource rebuild inconsistency");
        std::cout<<"GPU_ACTUAL_GL33_RENDER PASS\n"
                 <<"EV-T45_GPU_LAYERS PASS\n"
                 <<"EV-T49_CONTEXT_RECREATE PASS (not OS TDR)\n";
    }catch(const std::exception& e) {
        std::cerr<<"GPU FAIL: "<<e.what()<<"\n";
        return 1;
    }
    return 0;
}
