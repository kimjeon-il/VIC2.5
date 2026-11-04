#include "vic25/phase_e_compact.hpp"
#include "vic25/phase_e_snapshot.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>
#include <atomic>
#include <exception>
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

        // Actual concurrency test: only simulation worker mutates World/Index.
        // The UI/render thread reads a stable immutable GPU palette snapshot.
        // No frame is allowed to wait for a monthly transaction lock.
        compact::FrameExchange frame_exchange(index);
        const auto original=frame_exchange.acquire();
        std::atomic<bool> completed{false};
        std::exception_ptr background_error;
        const auto concurrent_begin=Clock::now();
        std::thread worker([&] {
            try {
                World::Plan another;
                another[9].push_back([](SpatialStage& s) {
                    s.queue_transfer("EV10-cession-second",
                                     {11,12,13,14,15,16,17,18,19,20},"B",2);
                });
                w.advance(another);
                w.advance();
                constexpr std::array<Id,10> second={11,12,13,14,15,16,17,18,19,20};
                index.apply(w,second);
                frame_exchange.publish(index);
            } catch(...) {background_error=std::current_exception();}
            completed.store(true,std::memory_order_release);
        });
        std::uint64_t ui_reads=0;
        for(int frame=0;frame<20000;++frame) {
            const auto f=frame_exchange.acquire();
            const auto dense=static_cast<std::size_t>((frame*37u)%size);
            ui_reads+=f->country[dense]+f->admin[dense]+f->military[dense];
        }
        const auto concurrent_end=Clock::now();
        worker.join();
        if(background_error)std::rethrow_exception(background_error);
        const auto latest=frame_exchange.acquire();
        if(!completed.load(std::memory_order_acquire) ||
           original->commit_id>=latest->commit_id ||
           original->country[10]==latest->country[10] ||
           original->country[10]==0 || ui_reads==0 ||
           latest->estimated_bytes()>2*1024*1024 ||
           latest->province_ids.size()!=size)
            throw std::runtime_error("async immutable frame read or commit publish corrupted");
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
                 <<" concurrent_ui_reads_ms="<<ms(concurrent_begin,concurrent_end)
                 <<" committed_frame_bytes="<<latest->estimated_bytes()
                 <<" checksum="<<checksum<<"\n";
        std::cout<<"EV10_30K_COMPACT PASS\n";
    }catch(const std::exception& ex){
        std::cerr<<"EV10_30K_FAIL "<<ex.what()<<"\n";
        return 1;
    }
}
