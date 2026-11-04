#include "vic25/phase_e_compact.hpp"
#include "vic25/phase_e_snapshot.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace vic25::phasee;
using Clock=std::chrono::steady_clock;
static double ms(Clock::time_point a,Clock::time_point b) {
    return std::chrono::duration<double,std::milli>(b-a).count();
}
int main(int argc,char** argv){
    try {
        if(argc!=2)throw std::invalid_argument("usage: phase_e_30k <10000|20000|30000>");
        const Id size=std::stoull(argv[1]);
        if(size!=10000&&size!=20000&&size!=30000)
            throw std::invalid_argument("unsupported benchmark size");
        const auto started=Clock::now();
        World w(12000);
        w.set_dataset("synthetic-grid-EV10-30k/1");
        w.seed_unit(1,"A","A-state");
        w.seed_unit(2,"B","B-state");
        for(Id i=1;i<=size;++i){
            const bool country_b=(i>size-100);
            w.seed_province(i,"PROV:"+std::to_string(i),
                             country_b?"B":"A",1000000);
            w.seed_assignment(i,country_b?2:1);
        }
        // 200 columns: each grid cell has at most four land neighbors.
        constexpr Id width=200;
        for(Id i=1;i<=size;++i) {
            if(i%width!=0 && i<size)w.seed_border(i,i+1);
            if(i+width<=size)w.seed_border(i,i+width);
        }
        w.validate();
        const auto seeded=Clock::now();
        compact::ProvinceIndex index(w);
        const auto indexed=Clock::now();
        if(index.province_count()!=size ||
           index.estimated_bytes()>8*1024*1024 ||
           index.edge_directed_count()>=size*5)
            throw std::runtime_error("dense index size/adjacency budget exceeded");
        const auto initialOwner=index.palette(1);
        const auto snapshot=w.core().core().people().slices().size();
        World::Plan plan;
        plan[9].push_back([](SpatialStage& s) {
            s.queue_transfer("EV10-cession", {1,2,3,4,5,6,7,8,9,10}, "B",2);
        });
        const auto tickstart=Clock::now();
        w.advance(plan);
        w.advance();
        const auto tickend=Clock::now();
        constexpr std::array<Id,10> changed={1,2,3,4,5,6,7,8,9,10};
        index.apply(w,changed);
        const auto patched=Clock::now();
        if(index.palette(1)==initialOwner || index.changed_gpu_indices().size()!=10 ||
           index.dirty_border_count()>80 ||
           w.sovereign_provinces("A").size()!=size-110 ||
           w.core().core().people().slices().size()!=snapshot)
            throw std::runtime_error("incremental political update incorrect");
        // Rendering reads compact arrays only: no World/POP/C++ registry queries.
        // This measures CPU lookup overhead only, not display FPS.
        volatile std::uint64_t checksum=0;
        const auto framestart=Clock::now();
        constexpr int frames=600;
        for(int frame=0;frame<frames;++frame) {
            std::uint64_t sum=0;
            for(int v=0;v<96;++v) {
                const Id province=1+(static_cast<Id>(frame)*37+
                                     static_cast<Id>(v)*277)%size;
                sum+=index.palette(province);
                sum+=index.administration(province);
            }
            checksum=checksum+sum;
        }
        const auto frameend=Clock::now();
        std::cout<<std::fixed<<std::setprecision(3)
                 <<"EV10_30K_BENCH provinces="<<size
                 <<" seed_ms="<<ms(started,seeded)
                 <<" compact_build_ms="<<ms(seeded,indexed)
                 <<" two_months_ms="<<ms(tickstart,tickend)
                 <<" patch_10_ms="<<ms(tickend,patched)
                 <<" cpu_frame_pseudo_ms="<<ms(framestart,frameend)/frames
                 <<" dense_capacity_bytes="<<index.estimated_bytes()
                 <<" topology_directed_edges="<<index.edge_directed_count()
                 <<" patched_gpu_ids="<<index.changed_gpu_indices().size()
                 <<" checksum="<<checksum<<"\n";
        std::cout<<"EV10_30K_COMPACT PASS\n";
    }catch(const std::exception& ex){
        std::cerr<<"EV10_30K_FAIL "<<ex.what()<<"\n";
        return 1;
    }
}
