#pragma once
#include "tracking_math.hpp"
#include "sniper_scope.hpp"
#include "gamepad_bridge.hpp"
#include "reload_runtime.hpp"
#include "arsenal_runtime.hpp"
#include "native_hand_clip.hpp"
#include "selected_weapon_draw.hpp"
#include "head_lamp.hpp"
#include "body_holsters.hpp"
#include "native_wrist_pose.hpp"
#include "native_body_anchor.hpp"
#include "native_body_reference.hpp"
#include "native_transform_cache.hpp"
#include "geometry_isolation.hpp"
#include "selected_ballistics.hpp"
#include "native_flash_alignment.hpp"
#include <MinHook.h>
#include <atomic>
namespace nativeMotion {
inline bool enabled=true,headRelativeMovement=true;
inline bool bodyAnchorTrial{};
inline std::atomic<unsigned> anchoredBodies{},rejectedBodyAnchors{};
inline std::atomic<bool> renderInVR{};
inline std::atomic<unsigned> aimedRays{},trackedDraws{};
inline std::atomic<unsigned> hiddenDraws{},unavailablePoseDraws{};
inline std::atomic<unsigned> suppressedMenuDraws{};
struct DrawEvidence {unsigned weapon,flags;float sourceEye[3],requested[12];};
inline thread_local const DrawEvidence* drawEvidence{};
struct EvidenceScope {
    const DrawEvidence* previous=drawEvidence;
    explicit EvidenceScope(const DrawEvidence& value){drawEvidence=&value;}
    ~EvidenceScope(){drawEvidence=previous;}
};
using RayFunction=float*(*)(void*,float*);
using RenderFunction=void(*)(void*,void*,unsigned,void*);
inline RayFunction originalRay{};
inline void(*gripPoseSink)(unsigned,void*){};
inline bool(*replacementDraw)(unsigned){};
inline RenderFunction originalRender[7]{};
inline constexpr std::uintptr_t renderRVA[]={0x8aa80,0x8b5a0,0x8c0d0,0x8f930,0x90660,0x89e20,0x89e20};
inline constexpr std::uintptr_t weaponVtable[]={0x30c500,0x30c640,0x30c900,0x30d2e0,0x30d668,0x30cb70,0x30d1a8};
inline std::uintptr_t base{};
inline void(*flashEvidence)(unsigned,const float*,const float*,const float*){};
inline std::atomic<unsigned> cachedTransforms{},uncachedTransforms{};
class DrawCache {
    unsigned char* flag{};int savedIndex{};float* top{};float saved[12]{};
public:
    DrawCache(void* options,const float* previous,const float* next){
        if(geometryIsolation::mode(GetTickCount64())==geometryIsolation::NoWeaponCache)return;
        if(!manualReload::readable(options,1))return;
        // A clear flag means native 7C990 owns a forthcoming stack push.
        // Leave that push and its caller-owned pop entirely untouched.
        if(!*static_cast<unsigned char*>(options)){++uncachedTransforms;return;}
        auto index=*reinterpret_cast<int*>(base+0x6bf284);
        // These are the model-view stack and dirty flag read by native 7C990.
        if(index<0||index>=32)return;
        auto entry=reinterpret_cast<float*>(base+0x6bf288+index*48);
        if(!manualReload::readable(entry,sizeof saved))return;
        flag=static_cast<unsigned char*>(options);savedIndex=index;top=entry;std::memcpy(saved,top,sizeof saved);
        float changed[12];if(nativeTransformCache::replace(saved,previous,next,changed)){std::memcpy(top,changed,sizeof changed);*reinterpret_cast<unsigned char*>(base+0x6bf280)=1;++cachedTransforms;}
    }
    ~DrawCache(){
        // Pistol muzzle rendering (8B5EB) can pop this entry and clear the
        // shared flag. Never resurrect that flag or overwrite a popped slot.
        if(top&&*flag&&*reinterpret_cast<int*>(base+0x6bf284)==savedIndex){std::memcpy(top,saved,sizeof saved);*reinterpret_cast<unsigned char*>(base+0x6bf280)=1;}
    }
};
inline bool weapon(void* entity){
    auto table=*reinterpret_cast<std::uintptr_t*>(entity);
    for(auto rva:weaponVtable)if(table==base+rva)return true;
    return false;
}
inline bool pose(float* ray,float* matrix,bool display=false,int weaponId=-1){
    motionTracking::Sample sample;
    if(!enabled||!(display?motionTracking::readDisplay(GetTickCount64(),sample):motionTracking::read(GetTickCount64(),sample))||gamepadBridge::menuState()!=0)return false;
    auto player=*reinterpret_cast<unsigned char**>(base+0x3a5340);
    if(!player||*reinterpret_cast<std::uintptr_t*>(player+0xb8)!=base+0x30bff8)return false;
    auto camera=*reinterpret_cast<unsigned char**>(base+0x3a5338);
    if(!camera||!manualReload::readable(camera+0xbc,48))return false;
    float view[12],body[12];std::memcpy(view,camera+0xbc,sizeof view);
    if(!motionTracking::cameraWorldBasis(view,body))return false;
    XrVector3f eye{ray[0],ray[1],ray[2]};
    if(!std::isfinite(eye.x)||!std::isfinite(eye.y)||!std::isfinite(eye.z))return false;
    motionTracking::weaponMatrix(body,eye,sample.rightAim,sample.scale,matrix);
    // OpenXR aim space points down -Z. Use the same axis for the actual shot
    // and the drawn weapon; the small muzzle offset is in metres.
    ray[0]=matrix[3]-.08f*sample.scale*matrix[2];
    ray[1]=matrix[7]-.08f*sample.scale*matrix[6];
    ray[2]=matrix[11]-.08f*sample.scale*matrix[10];
    if(!display&&weaponId>=0&&weaponId<7&&selectedBallistics::ready[weaponId]){
        const auto& p=selectedBallistics::muzzle[weaponId];
        for(unsigned r=0;r<3;++r)ray[r]=matrix[r*4+3]+sample.scale*(matrix[r*4]*p.x+matrix[r*4+1]*p.y+matrix[r*4+2]*p.z);
    }
    ray[3]=-matrix[2];ray[4]=-matrix[6];ray[5]=-matrix[10];return true;
}
inline float* rayHook(void* entity,float* ray){
    auto result=originalRay(entity,ray);float matrix[12];
    auto table=*reinterpret_cast<std::uintptr_t*>(static_cast<unsigned char*>(entity)-0xb8);
    for(unsigned i=0;i<7;++i)if(table==base+weaponVtable[i]){if(pose(ray,matrix,false,int(i)))++aimedRays;break;}
    return result;
}
inline bool anchorAnimatedBody(unsigned i,void* entity,float* matrix){
    if(i>=6)return false;const auto& profile=arsenal::profiles[i];
    auto animation=*reinterpret_cast<unsigned char**>(static_cast<unsigned char*>(entity)+0x50);
    if(!manualReload::readable(animation,0x34+profile.bones*sizeof(manualReload::Bone))||animation[0x10]!=profile.bones||profile.body>=profile.bones)return false;
    const auto& b=reinterpret_cast<const manualReload::Bone*>(animation+0x34)[profile.body];
    if(!manualReload::validBone(b))return false;
    float reference[7],current[7];for(unsigned k=0;k<4;k++)current[k]=b.q[k]/4096.f;
    for(unsigned k=0;k<3;k++)current[k+4]=b.p[k]/4096.f;
    for(unsigned k=0;k<7;k++)reference[k]=nativeBodyAnchor::reference[i][k]/4096.f;
    return nativeBodyAnchor::stabilize(reference,current,matrix);
}
template<unsigned Index> inline void renderHook(void* entity,void* renderer,unsigned flags,void* options){
    float ray[6],matrix[12];
    if(!weapon(entity)){originalRender[Index](entity,renderer,flags,options);return;}
    auto diagnostic=geometryIsolation::mode(GetTickCount64());
    nativeHandClip::Scope clip(diagnostic==geometryIsolation::NoHandClip?0:nativeHandClip::mask(Index,GetTickCount64()));
    if((flags&4)&&(*reinterpret_cast<unsigned char*>(static_cast<unsigned char*>(entity)+0x44)&8)){
        sniperScope::visibleWeapon=Index;sniperScope::weaponTime=GetTickCount64();
        headLamp::emit(entity,renderer);
    }
    if(bodyHolsters::hidden.load()&&(flags&4)){++hiddenDraws;return;}
    if(diagnostic==geometryIsolation::NativeWeaponPose){originalRender[Index](entity,renderer,flags,options);return;}
    if constexpr(Index==4){if(sniperScope::factor()>1.01f)return;}
    if(!weapon(entity)){originalRender[Index](entity,renderer,flags,options);return;}
    originalRay(static_cast<unsigned char*>(entity)+0xb8,ray);
    const float sourceEye[]={ray[0],ray[1],ray[2]};
    if(!pose(ray,matrix,true,int(Index))){
        // Menus suspend the tracked pose, but native held-weapon passes also
        // arrive with flags=1. Falling through on those passes resurrects the
        // desktop viewmodel under the VR menu. VR owns every held-weapon pass.
        if(renderInVR.load()){
            ++unavailablePoseDraws;if(gamepadBridge::menuState()!=0)++suppressedMenuDraws;return;
        }
        originalRender[Index](entity,renderer,flags,options);return;
    }
    float aimMatrix[12];std::memcpy(aimMatrix,matrix,sizeof(matrix));
    if constexpr(Index!=1){int i=arsenal::index(Index);if(i>=0&&arsenal::installed)arsenal::align(i,matrix);}
    if constexpr(Index>=2&&Index<=4){if(gripPoseSink)gripPoseSink(Index,entity);}
    selectedWeaponDraw::Scope replacement(selectedWeaponDraw::ownsBody(
        replacementDraw&&replacementDraw(Index),Index,renderInVR.load()));
    if constexpr(Index==1){
        if(manualReload::consumesAction()){
            // The native pistol muzzle follows model +Z; OpenXR aim is -Z.
            // Rotate the model 180 degrees about up, preserving handedness.
            manualReload::alignPistol(matrix);
        }
    }
    if constexpr(Index!=1){
        const int i=arsenal::index(Index);
        if(bodyAnchorTrial&&i>=0&&arsenal::installed){
            if(anchorAnimatedBody(static_cast<unsigned>(i),entity,matrix))++anchoredBodies;else ++rejectedBodyAnchors;
        }
    }
    auto target=reinterpret_cast<float*>(static_cast<unsigned char*>(entity)+8);
    if constexpr(Index==2||Index==4){
        // Recorded M16/hunting draws compute their effect origin from the Q12
        // attachment at +100h. Translate only this scoped native draw; the
        // selected mesh, saved grip and simulation pose remain unchanged.
        if(selectedWeaponDraw::replacing&&selectedBallistics::ready[Index]&&manualReload::readable(static_cast<unsigned char*>(entity)+0x100,sizeof(void*))){
            auto attachment=*reinterpret_cast<const std::int32_t**>(static_cast<unsigned char*>(entity)+0x100);
            if(manualReload::readable(attachment,0x30)&&manualReload::readable(reinterpret_cast<void*>(base+(Index==2?0x6bda70:0x6bdc18)),sizeof(float))){
                const float length=*reinterpret_cast<const float*>(base+(Index==2?0x6bda70:0x6bdc18));
                if(std::isfinite(length)&&std::fabs(length)<4){float local[3];for(unsigned j=0;j<3;++j)local[j]=(attachment[j]*length+attachment[9+j])/4096.f;
                    const auto& m=selectedBallistics::muzzle[Index];float muzzle[]={m.x,m.y,m.z};nativeFlashAlignment::align(matrix,aimMatrix,local,muzzle);
                    if constexpr(Index==4){if(flashEvidence&&(flags&4)&&*reinterpret_cast<unsigned char*>(static_cast<unsigned char*>(entity)+0xcc))flashEvidence(Index,local,muzzle,matrix);}}
            }
        }
    }
    struct Restore {float* target;float saved[12];~Restore(){std::memcpy(target,saved,sizeof(saved));}} restore{target,{}};
    std::memcpy(restore.saved,target,sizeof(restore.saved));DrawCache cache(options,restore.saved,matrix);std::memcpy(target,matrix,sizeof(matrix));
    DrawEvidence evidence{Index,flags,{sourceEye[0],sourceEye[1],sourceEye[2]}, {}};
    std::memcpy(evidence.requested,matrix,sizeof matrix);EvidenceScope evidenceScope(evidence);
    // Wrap the complete weapon draw, including its muzzle flash, rather than
    // only the base animated mesh draw. Restore before native simulation.
    if constexpr(Index==1){manualReload::Parts parts(entity);
        auto animation=*reinterpret_cast<unsigned char**>(static_cast<unsigned char*>(entity)+0x50);
        motionTracking::Sample sample;
        if(manualReload::readable(animation,0x34+3*sizeof(manualReload::Bone))&&animation[0x10]>=3&&motionTracking::read(GetTickCount64(),sample)){
            auto& b=reinterpret_cast<const manualReload::Bone*>(animation+0x34)[2];
            if(manualReload::validBone(b))nativeWristPose::capture(aimMatrix,matrix,{{b.q[0]/4096.f,b.q[1]/4096.f,b.q[2]/4096.f,b.q[3]/4096.f},{b.p[0]/4096.f,b.p[1]/4096.f,b.p[2]/4096.f}},sample.scale,GetTickCount64());
        }
        originalRender[Index](entity,renderer,flags,options);}
    else {arsenal::Parts parts(static_cast<unsigned>(arsenal::index(Index)),entity);originalRender[Index](entity,renderer,flags,options);}
    if constexpr(Index==4){if(flashEvidence&&(flags&4)&&*reinterpret_cast<unsigned char*>(static_cast<unsigned char*>(entity)+0xcc)){
        auto a=*reinterpret_cast<const std::int32_t**>(static_cast<unsigned char*>(entity)+0x100);
        if(manualReload::readable(a,0x30)){float local[3];const float len=*reinterpret_cast<const float*>(base+0x6bdc18);for(unsigned j=0;j<3;++j)local[j]=(a[j]*len+a[9+j])/4096.f;
            const auto m=selectedBallistics::muzzle[4];float tip[]={m.x,m.y,m.z};flashEvidence(14,local,tip,target);}
    }}
    ++trackedDraws;
}
inline void sharedMeleeRender(void* entity,void* renderer,unsigned flags,void* options){
    if(*reinterpret_cast<std::uintptr_t*>(entity)==base+weaponVtable[6])renderHook<6>(entity,renderer,flags,options);
    else renderHook<5>(entity,renderer,flags,options);
}
inline bool install(){
    base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const unsigned char rayCode[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x40,0x48,0x8b,0xd9,0x48,0x8b,0xfa};
    const unsigned char drawCode[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x48,0x89,0x7c,0x24,0x18};
    if(std::memcmp(reinterpret_cast<void*>(base+0x8a090),rayCode,sizeof(rayCode)))return false;
    for(unsigned i=0;i<7;i++)if((i<5&&std::memcmp(reinterpret_cast<void*>(base+renderRVA[i]),drawCode,sizeof(drawCode)))||*reinterpret_cast<std::uintptr_t*>(base+weaponVtable[i]+0x30)!=base+renderRVA[i])return false;
    const unsigned char sharedCode[]={0xf6,0x41,0x44,0x08,0x0f,0x85};
    if(std::memcmp(reinterpret_cast<void*>(base+renderRVA[5]),sharedCode,sizeof sharedCode))return false;
    auto ray=reinterpret_cast<void*>(base+0x8a090);
    if(MH_CreateHook(ray,reinterpret_cast<void*>(rayHook),reinterpret_cast<void**>(&originalRay))!=MH_OK)return false;
    RenderFunction hooks[]={renderHook<0>,renderHook<1>,renderHook<2>,renderHook<3>,renderHook<4>,sharedMeleeRender};
    for(unsigned i=0;i<6;i++)if(MH_CreateHook(reinterpret_cast<void*>(base+renderRVA[i]),reinterpret_cast<void*>(hooks[i]),reinterpret_cast<void**>(&originalRender[i]))!=MH_OK){
        for(unsigned j=0;j<i;j++)MH_RemoveHook(reinterpret_cast<void*>(base+renderRVA[j]));MH_RemoveHook(ray);return false;
    }
    originalRender[6]=originalRender[5];
    return true;
}
}
