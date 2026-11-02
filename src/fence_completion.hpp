#pragma once
#include <cstdint>
#include <limits>
#include <stdexcept>
namespace fenceCompletion {
// D3D12 reserves UINT64_MAX to report device removal, not completion.
template<class Read, class Arm, class Wait, class Removed>
void wait(std::uint64_t target,Read read,Arm arm,Wait waitEvent,Removed removed) {
    if(target==(std::numeric_limits<std::uint64_t>::max)())
        throw std::runtime_error("stereo invalid fence target");
    auto completed=[&] {
        const auto value=read();
        if(value==(std::numeric_limits<std::uint64_t>::max)()) {
            removed();
            throw std::runtime_error("stereo GPU device removed");
        }
        return value>=target;
    };
    if(completed())return;
    arm(target);
    waitEvent();
    // An event wake alone does not prove that submitted resources are reusable.
    if(!completed())throw std::runtime_error("stereo GPU fence woke before completion");
}
}
