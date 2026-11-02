#pragma once
#include "reload_runtime.hpp"
#include "head_light_constants.hpp"
#include <MinHook.h>
namespace headLamp {
struct Frame {XrPosef head{{0,0,0,1},{}},origin{{0,0,0,1},{}};float scale=1;std::uint64_t time{};bool valid{};};
inline std::mutex mutex;inline Frame frame;inline std::atomic<bool> diagnosticBypass{};inline bool emitted{};inline std::uintptr_t base{};inline bool installed{};
using Emit=void(*)(void*,void*);using Render=void(*)(void*,void*,unsigned,void*);
inline Emit originalEmit{};inline Render originalRender{};
using Toggle=void(*)(void*);inline Toggle originalToggle{};
// Native first-person lamp mesh is never a tracking-loss fallback in VR.
inline std::atomic<bool> vrPresentation{};
inline std::atomic<unsigned> blockedToggles{},forcedOn{};inline std::atomic<bool> testToggleRequested{};
inline std::atomic<unsigned> brightnessRestored{};
inline auto& lightEnabled=headLightConstants::enabled;
inline void controllerToggle(){
    lightEnabled=!lightEnabled.load();
    handheldLight::clickedAt=GetTickCount64();
    handheldLight::soundRequested=true;
    ++handheldLight::switchCount;
}
inline bool mount(const Frame& f,std::uint64_t now,const float* body,XrVector3f eye,float* matrix){
    if(!f.valid||now<f.time||now-f.time>=150||!motionTracking::validPose(f.head)||!motionTracking::validPose(f.origin)||!motionTracking::validBasis(body)||!std::isfinite(f.scale)||f.scale<=0||f.scale>100)return false;
    auto local=motionTracking::relativePose(f.origin,f.head);
    // A small forehead mount; full head orientation, never a controller pose.
    using namespace DirectX;XMFLOAT3 offset;XMStoreFloat3(&offset,XMVector3TransformNormal(XMVectorSet(0,.035f,-.045f,0),motionTracking::rotation(local.orientation)));
    local.position.x+=offset.x;local.position.y+=offset.y;local.position.z+=offset.z;
    motionTracking::weaponMatrix(body,eye,local,f.scale,matrix);return true;
}
inline void publish(Frame next){std::lock_guard<std::mutex> lock(mutex);frame=next;emitted=false;}
inline bool active(){std::lock_guard<std::mutex> lock(mutex);auto now=GetTickCount64();return installed&&!diagnosticBypass.load()&&frame.valid&&now>=frame.time&&now-frame.time<150;}
inline bool forceOn(void* entity){
    if(!entity||!manualReload::readable(entity,0x158)||*reinterpret_cast<std::uintptr_t*>(entity)!=base+0x30cdd0)return false;
    auto flags=reinterpret_cast<unsigned*>(static_cast<unsigned char*>(entity)+0x44);
    // Native 8DE18 toggles bit 5 only after the lamp's normal initialization.
    // Preserve every other state bit; do not invent animation/setup flags.
    // Keep native room-light setup stable. L3 switches only the audited lamp
    // slot in each VR eye's cloned pixel constants, never the room light.
    bool changed=false;
    if(!(*flags&0x20)){*flags|=0x20;++forcedOn;changed=true;}
    // Native 8DDE0 scales the light by the low five bits at +154. The
    // unequipped lamp can freeze at the native flicker value 1 when another
    // item becomes active. VR retains this lamp independently of that item.
    // Restore its normal 31/31 level, preserving all timer/state bits.
    auto brightness=reinterpret_cast<unsigned*>(static_cast<unsigned char*>(entity)+0x154);
    if((*brightness&31)!=31){*brightness=(*brightness&~31u)|31u;++brightnessRestored;changed=true;}
    if(changed){
        // Same material/light invalidation bit used by the native toggle.
        auto dirty=reinterpret_cast<unsigned*>(base+0x6bcae0);if(manualReload::readable(dirty,4))*dirty|=1;
    }
    return true;
}
inline void toggleHook(void* interfacePointer){
    if(active()&&interfacePointer&&forceOn(static_cast<unsigned char*>(interfacePointer)-0xb8)){++blockedToggles;return;}
    originalToggle(interfacePointer);
}
inline void tick(void* player){
    if(!active()||player!=*reinterpret_cast<void**>(base+0x3a5340)||!manualReload::readable(player,0x320))return;
    auto head=reinterpret_cast<std::uintptr_t>(player)+0x308,link=*reinterpret_cast<std::uintptr_t*>(head);
    for(unsigned n=0;n<16&&link!=head&&link>=0x98;n++){
        auto item=reinterpret_cast<unsigned char*>(link-0x98);if(!manualReload::readable(item,0x158))return;
        if(forceOn(item)){
            if(handheldLight::soundRequested.exchange(false)&&originalToggle){originalToggle(item+0xb8);forceOn(item);++handheldLight::soundCount;}
            if(testToggleRequested.exchange(false))toggleHook(item+0xb8);return;}link=*reinterpret_cast<std::uintptr_t*>(item+0x98);
    }
}
inline void emitHook(void* entity,void* renderer){if(!active()&&(!vrPresentation.load()||diagnosticBypass.load()))originalEmit(entity,renderer);}
inline void emit(void*,void*){
    // 8E040 is the lens billboard, not the scene illumination path. Placing
    // that billboard at the eye creates the giant bright patch seen in VR.
}
inline constexpr std::uintptr_t weaponTables[]={0x30c500,0x30c640,0x30c900,0x30d2e0,0x30d668,0x30cb70,0x30d1a8};
inline bool illuminationQuery(void* entity){
    if(!active()||!manualReload::readable(entity,sizeof(std::uintptr_t)))return false;
    auto table=*reinterpret_cast<std::uintptr_t*>(entity);
    for(auto known:weaponTables)if(table==base+known)return true;
    return false;
}
inline bool bindIlluminationQueries(){
    // Native scene setup 3AE9F queries slot D0 on the equipped item before
    // locating the owned flashlight and applying its brightness to the world.
    // These seven slots originally share the verified return-false function.
    for(auto table:weaponTables)if(*reinterpret_cast<std::uintptr_t*>(base+table+0xd0)!=base+0x2edd0)return false;
    const unsigned char falseCode[]={0x32,0xc0,0xc3};if(std::memcmp(reinterpret_cast<void*>(base+0x2edd0),falseCode,3))return false;
    unsigned applied=0;
    for(auto table:weaponTables){
        auto slot=reinterpret_cast<void**>(base+table+0xd0);DWORD old{};
        if(!VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&old))break;
        InterlockedExchangePointer(slot,reinterpret_cast<void*>(illuminationQuery));DWORD ignored{};VirtualProtect(slot,sizeof(void*),old,&ignored);++applied;
    }
    if(applied==7)return true;
    for(unsigned i=0;i<applied;i++){auto slot=reinterpret_cast<void**>(base+weaponTables[i]+0xd0);DWORD old{};if(VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&old)){InterlockedExchangePointer(slot,reinterpret_cast<void*>(base+0x2edd0));DWORD ignored{};VirtualProtect(slot,sizeof(void*),old,&ignored);}}
    return false;
}
inline void renderHook(void* entity,void* renderer,unsigned flags,void* options){
    if(active()){if(flags&4)emit(entity,renderer);return;}
    if(vrPresentation.load()&&!diagnosticBypass.load())return;
    originalRender(entity,renderer,flags,options);
}
inline bool install(){
    base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const unsigned char brightnessCode[]={0x8b,0x81,0x54,0x01,0x00,0x00,0x83,0xe0,0x1f,0x0f,0xaf,0xc2,0x83,0xc0,0x10,0xc1,0xf8,0x05,0xc3};
    if(std::memcmp(reinterpret_cast<void*>(base+0x8dde0),brightnessCode,sizeof brightnessCode)||*reinterpret_cast<std::uintptr_t*>(base+0x30cdd0+0xf0)!=base+0x8dde0)return false;
    const unsigned char toggleCode[]={0x48,0x83,0xec,0x28,0x8b,0x51,0x8c,0x0f,0xba,0xf2,0x0b,0x89,0x51,0x8c,0x0f,0xba};
    if(std::memcmp(reinterpret_cast<void*>(base+0x8de00),toggleCode,16)||*reinterpret_cast<std::uintptr_t*>(base+0x30cee0+0x18)!=base+0x8de00)return false;
    const unsigned char emitCode[]={0x48,0x8b,0xc4,0x48,0x89,0x58,0x10,0x48,0x89,0x78,0x18,0x55,0x48,0x8d,0x68,0xa1};
    const unsigned char renderCode[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57};
    if(std::memcmp(reinterpret_cast<void*>(base+0x8e040),emitCode,16)||std::memcmp(reinterpret_cast<void*>(base+0x8e270),renderCode,16)||*reinterpret_cast<std::uintptr_t*>(base+0x30cdd0+0x30)!=base+0x8e270)return false;
    // 8E11E's native integer-to-float attachment scale must match this adapter.
    if(*reinterpret_cast<float*>(base+0x328b50)!=1.f/4096.f)return false;
    if(MH_CreateHook(reinterpret_cast<void*>(base+0x8e040),reinterpret_cast<void*>(emitHook),reinterpret_cast<void**>(&originalEmit))!=MH_OK)return false;
    if(MH_CreateHook(reinterpret_cast<void*>(base+0x8e270),reinterpret_cast<void*>(renderHook),reinterpret_cast<void**>(&originalRender))!=MH_OK){MH_RemoveHook(reinterpret_cast<void*>(base+0x8e040));return false;}if(MH_CreateHook(reinterpret_cast<void*>(base+0x8de00),reinterpret_cast<void*>(toggleHook),reinterpret_cast<void**>(&originalToggle))!=MH_OK){MH_RemoveHook(reinterpret_cast<void*>(base+0x8e040));MH_RemoveHook(reinterpret_cast<void*>(base+0x8e270));return false;}
    if(!bindIlluminationQueries()){MH_RemoveHook(reinterpret_cast<void*>(base+0x8e040));MH_RemoveHook(reinterpret_cast<void*>(base+0x8e270));MH_RemoveHook(reinterpret_cast<void*>(base+0x8de00));return false;}
    return installed=true;
}
}
