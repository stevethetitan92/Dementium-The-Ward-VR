#pragma once
#include <array>
#include <cmath>
#include <algorithm>

namespace flashlightProfileRange {
// Trial policy for the observed hallway upload only. Caller must independently
// establish native flashlight resource identity. Never alter the source buffer.
inline bool normalizeObservedFloor(const float* source, std::array<float,257>& output) {
    if(!source) return false;
    for(unsigned i=0;i<257;++i)
        if(!std::isfinite(source[i]) || source[i]<.9499f || source[i]>1.0001f) return false;
    for(unsigned i=0;i<228;++i)
        if(std::abs(source[i]-.95f)>.00001f) return false;
    if(std::abs(source[256]-source[255])>.00001f) return false;
    const auto maximum=*std::max_element(source,source+257);
    if(maximum<.999f) return false;
    std::array<float,257> candidate{};
    for(unsigned i=0;i<257;++i)
        candidate[i]=std::clamp((source[i]-.95f)/.05f,0.f,1.f);
    output=candidate;
    return true;
}
}
