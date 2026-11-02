#pragma once
#include "tracking_math.hpp"
namespace interactionGaze {
inline bool position(const float* view,const motionTracking::Sample& sample,uint64_t now,const float* native,float* out){
    if(!sample.valid||now<sample.time||now-sample.time>=150||!std::isfinite(sample.scale)||sample.scale<=0)return false;
    float body[12];if(!motionTracking::cameraWorldBasis(view,body))return false;
    auto p=motionTracking::worldPoint(body,{native[0],native[1],native[2]},sample.rightAim.position,sample.scale);
    out[0]=p.x;out[1]=p.y;out[2]=p.z;return true;
}
inline bool direction(const float* view,const motionTracking::Sample& sample,uint64_t now,float* out){
    if(!sample.valid||now<sample.time||now-sample.time>=150)return false;
    float body[12];if(!motionTracking::cameraWorldBasis(view,body))return false;
    auto p=motionTracking::pointAhead(sample.rightAim,1);
    auto h=sample.rightAim.position;
    auto world=motionTracking::worldPoint(body,{0,0,0},{p.x-h.x,p.y-h.y,p.z-h.z},1);
    out[0]=world.x;out[1]=world.y;out[2]=world.z;return true;
}
}
