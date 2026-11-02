#pragma once
#include "interaction_gaze_math.hpp"
#include "gamepad_bridge.hpp"
#include "reload_runtime.hpp"
#include <MinHook.h>
namespace interactionGaze {
inline std::mutex mutex;
inline motionTracking::Sample current;
inline void publish(const XrPosef& origin,const XrPosef& head,uint64_t now,float scale,bool valid){
    motionTracking::Sample next;next.time=now;next.scale=scale;next.valid=valid&&motionTracking::validPose(origin)&&motionTracking::validPose(head);
    if(next.valid)next.rightAim=motionTracking::relativePose(origin,head);
    std::lock_guard<std::mutex> lock(mutex);current=next;
}
using Update=void(*)(void*,const float*,const float*,void*);
inline Update original;
inline uintptr_t base;
inline void(*evidence)(const float*,const float*,unsigned,unsigned,const float*,const float*,bool){};
inline void update(void* scene,const float* position,const float* facing,void* state){
    motionTracking::Sample sample;{std::lock_guard<std::mutex> lock(mutex);sample=current;}
    float corrected[3],origin[3];bool changed=false;
    auto player=*reinterpret_cast<unsigned char**>(base+0x3a5340);
    auto camera=*reinterpret_cast<unsigned char**>(base+0x3a5338);
    // Only the player's trigger evaluation. Keep the native volumes,
    // angle thresholds, script callbacks and enter/exit bookkeeping.
    if(player&&state==player+0x238&&camera&&gamepadBridge::menuState()==0&&
       manualReload::readable(camera+0xbc,48))
        changed=direction(reinterpret_cast<float*>(camera+0xbc),sample,GetTickCount64(),corrected)&&interactionGaze::position(reinterpret_cast<float*>(camera+0xbc),sample,GetTickCount64(),position,origin);
    unsigned before=manualReload::readable(state,4)?*static_cast<unsigned*>(state):0;
    original(scene,changed?origin:position,changed?corrected:facing,state);
    if(changed&&evidence){
        auto ui=*reinterpret_cast<unsigned char**>(base+0x3fc468);
        bool prompt=ui&&manualReload::readable(ui+0x1bc0,8)&&*reinterpret_cast<uintptr_t*>(ui+0x1bc0)!=0;
        evidence(facing,corrected,before,*static_cast<unsigned*>(state),position,origin,prompt);
    }
}
inline bool install(){
    base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const unsigned char code[]={0x40,0x53,0x56,0x57,0x41,0x54,0x41,0x57,0x48,0x81,0xec,0x00,0x01,0x00,0x00};
    auto target=reinterpret_cast<void*>(base+0xf8320);
    if(std::memcmp(target,code,sizeof code))return false;
    return MH_CreateHook(target,reinterpret_cast<void*>(update),reinterpret_cast<void**>(&original))==MH_OK;
}
}
