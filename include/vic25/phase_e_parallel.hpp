#pragma once
// Deterministic parallel proposal preflight; only immutable state is read by
// workers. The actual cross-domain month staging remains single-writer.
#include "vic25/phase_e.hpp"
#include <algorithm>
#include <exception>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace vic25::phasee::parallel {
struct PreparedBatch {
    std::vector<Operation> ordered;
    std::size_t actual_worker_count{};
};
inline PreparedBatch prepare(const World& world,
                             const std::vector<Operation>& proposed,
                             std::size_t worker_count) {
    require(worker_count>0&&worker_count<=32,"ED0_ERR_REVISION_CONFLICT");
    const auto& state=world.spatial();
    const auto& core=world.core().core();
    if(proposed.empty())return {};
    const std::size_t actual=std::min(worker_count,proposed.size());
    std::vector<std::vector<Operation>> partitions(actual);
    std::vector<std::exception_ptr> errors(actual);
    std::vector<std::thread> threads;
    threads.reserve(actual);
    for(std::size_t w=0;w<actual;++w) {
        threads.emplace_back([&,w]{
            try {
                for(std::size_t idx=w;idx<proposed.size();idx+=actual) {
                    const auto& op=proposed[idx];
                    require(!op.id.empty(),"ED0_ERR_DUPLICATE_EVENT");
                    if(op.kind==OpKind::reassign) {
                        require(op.provinces.size()==1 &&
                                state.provinces.contains(op.provinces.front()) &&
                                state.units.contains(op.unit) &&
                                state.units.at(op.unit).active &&
                                state.units.at(op.unit).country==
                                   core.sovereignty(state.provinces.at(op.provinces.front()).region),
                                "ED0_ERR_ADMIN_ASSIGNMENT");
                    } else if(op.kind==OpKind::rename) {
                        require(state.units.contains(op.unit)&&
                                state.units.at(op.unit).active&&!op.value.empty(),
                                "ED0_ERR_ADMIN_ASSIGNMENT");
                    } else {
                        throw std::invalid_argument("ED0_ERR_INVALID_SCOPE");
                    }
                    partitions[w].push_back(op);
                }
            } catch (...) {errors[w]=std::current_exception();}
        });
    }
    for(auto& thread:threads)thread.join();
    for(const auto& failure:errors)if(failure)std::rethrow_exception(failure);
    PreparedBatch result;
    result.actual_worker_count=actual;
    for(auto& partition:partitions)
        result.ordered.insert(result.ordered.end(),partition.begin(),partition.end());
    std::sort(result.ordered.begin(),result.ordered.end(),
        [](const Operation& a,const Operation& b){
          if(a.id!=b.id)return a.id<b.id;
          return a.provinces<b.provinces;
        });
    for(std::size_t i=1;i<result.ordered.size();++i)
        require(result.ordered[i-1].id!=result.ordered[i].id,
                "ED0_ERR_DUPLICATE_EVENT");
    return result;
}
inline PreparedBatch commit(World& world,const std::vector<Operation>& proposed,
                            std::size_t workers) {
    const auto result=prepare(world,proposed,workers);
    World::Plan plan;
    plan[9].push_back([ordered=result.ordered](SpatialStage& s){
        for(auto op:ordered)s.schedule(std::move(op));
    });
    world.advance(plan);
    return result;
}
} // namespace vic25::phasee::parallel
