#pragma once
#include "reload_gestures.hpp"
#include "manual_reload_native.hpp"
#include "gamepad_bridge.hpp"
#include "vr_feedback.hpp"
#include "weapon_shots.hpp"
#include "reload_audio.hpp"
#include "drop_surface.hpp"
#include "semi_fire_edge.hpp"
#include <MinHook.h>
#include <atomic>
#include <mutex>
namespace manualReload {
inline bool enabled=true;
inline std::atomic<bool> installed{};
inline std::atomic<unsigned> completed{},partDraws{};
inline std::mutex mutex;
inline Hands hands;
inline Gestures gestures;
inline NativePistol native;
inline std::uintptr_t image{};
using MenuState=int(*)();inline MenuState menuState=gamepadBridge::menuState;
inline bool owned=false;
using Update=void(*)(void*,void*,void*);
using Fire=void(*)(void*);
using Reload=bool(*)(void*,bool);
inline Update originalUpdate{};
inline Fire originalFire{};
inline Reload originalReload{};
inline semiFire::Edge pistolFireEdge;
inline std::atomic<unsigned> pistolRecoveryInterrupts{};
inline void publish(Hands next){
    std::lock_guard<std::mutex> lock(mutex);next.serial=hands.serial+1;hands=next;
}
inline bool ownsLocked(){auto now=GetTickCount64();return enabled&&installed&&hands.session&&now>=hands.time&&now-hands.time<500;}
inline bool consumesAction(){std::lock_guard<std::mutex> lock(mutex);return ownsLocked()&&hands.pistol&&hands.gameplay;}
inline bool blocked(){std::lock_guard<std::mutex> lock(mutex);return ownsLocked()&&hands.pistol&&(!hands.gunTracked||gestures.transaction().blocksFire());}
inline Visual visual(){std::lock_guard<std::mutex> lock(mutex);auto v=gestures.visual();v.stage=gestures.transaction().stage();v.valid=v.valid&&ownsLocked()&&hands.pistol&&hands.gameplay;return v;}
inline void updateHook(void* entity,void* step,void* context){
    dropSurface::service();
    auto now=GetTickCount64();
    {
        std::lock_guard<std::mutex> lock(mutex);
        // Visibility belongs to this concrete pistol; inactive inventory weapons
        // also receive updates and must never consume a pending transaction.
        bool visible=(*reinterpret_cast<unsigned*>(static_cast<unsigned char*>(entity)+0x44)&0x18)==0x18;
        owned=ownsLocked()&&visible;
        Hands current=hands;current.pistol=current.pistol&&visible;
        current.gameplay=current.gameplay&&menuState()==0;
        if(owned&&current.pistol&&current.gameplay){
            using Get=int(*)(void*);auto iface=static_cast<unsigned char*>(entity)+0xb8;
            int loaded=reinterpret_cast<Get>(image+0x8b4a0)(iface),reserve=reinterpret_cast<Get>(image+0x8b470)(iface);
            current.magazineAvailable=loaded>=0&&loaded<=15&&reserve>=0&&(loaded>0||reserve>0);
            current.chamberLoaded=loaded>0&&loaded<=15;
        }
        auto before=gestures.transaction().stage();gestures.update(current,now);
        const auto after=gestures.transaction().stage();static bool pullSound=false;
        if(owned&&current.pistol&&current.tracked&&current.gameplay){
            using A=reloadAudio::Action;
            if(after==Stage::EmptyWell&&(before==Stage::Ready||before==Stage::Inserted||before==Stage::SlideHeld))reloadAudio::play(1,A::MagazineDrop);
            if(before==Stage::MagazineHeld&&(after==Stage::Inserted||after==Stage::CommitPending))reloadAudio::play(1,A::MagazineInsert);
            if(after==Stage::SlideHeld&&before!=after)pullSound=false;
            if(after==Stage::SlideHeld&&!pullSound&&gestures.visual().slidePull>.01f){reloadAudio::play(1,A::MechanismPull);pullSound=true;}
            if(before==Stage::SlideHeld&&after!=before&&!current.grip&&pullSound)reloadAudio::play(1,A::MechanismRelease);
        }else pullSound=false;
        if(before!=gestures.transaction().stage()&&current.tracked&&current.gameplay)vrFeedback::queue(0,.25f,20);
        // Full native reload clips contain slide motion and must never play
        // on magazine insertion. Individual action clips are handled separately.
        if(owned&&current.pistol&&current.tracked&&current.gameplay&&now>=current.time&&now-current.time<150&&gestures.transaction().pendingToken()){
            if(native.commit(gestures.transaction(),static_cast<unsigned char*>(entity)+0xb8)){++completed;vrFeedback::queue(0,.4f,25);}
        }
    }
    auto observe=[&]{std::lock_guard<std::mutex> lock(mutex);
        bool valid=owned&&ownsLocked()&&hands.pistol&&hands.gameplay&&hands.gunTracked&&menuState()==0;
        using Get=int(*)(void*);auto iface=static_cast<unsigned char*>(entity)+0xb8;
        int a=valid?reinterpret_cast<Get>(image+0x8b4a0)(iface):0;
        int b=valid?reinterpret_cast<Get>(image+0x8b470)(iface):0;
        weaponShots::observers[1].sample(a,b,GetTickCount64(),valid);};
    bool newPull=false;
    {std::lock_guard<std::mutex> lock(mutex);
        const bool valid=owned&&ownsLocked()&&hands.pistol&&hands.gameplay&&hands.gunTracked&&
            now>=hands.time&&now-hands.time<100&&menuState()==0&&!gestures.transaction().blocksFire();
        newPull=pistolFireEdge.update(gamepadBridge::firePressSerial.load(),valid);
    }
    if(newPull&&*reinterpret_cast<std::uintptr_t*>(entity)==image+0x30c640){
        auto bytes=static_cast<unsigned char*>(entity);
        // Native pistol Fire starts animation2 and refuses another shot while
        // it runs. On a fresh trigger edge, use its own animation-end transition
        // (also called by native Update), never overwrite state/ammunition.
        // Animation3 is reload and is deliberately not interrupted.
        if(*reinterpret_cast<unsigned short*>(bytes+0xd0)==2){
            auto table=*reinterpret_cast<std::uintptr_t**>(entity);
            reinterpret_cast<void(*)(void*,unsigned,void*)>(table[0xe8/sizeof(std::uintptr_t)])(entity,0xffff,nullptr);
            ++pistolRecoveryInterrupts;
        }
    }
    observe();originalUpdate(entity,step,context);observe();
}
inline void fireHook(void* weapon){
    bool suppress;
    {std::lock_guard<std::mutex> lock(mutex);auto now=GetTickCount64();
        suppress=owned&&ownsLocked()&&(!hands.gunTracked||!hands.gameplay||now<hands.time||now-hands.time>=150||gestures.transaction().blocksFire());}
    if(!suppress)originalFire(weapon);
}
inline bool reloadHook(void* weapon,bool automatic){
    {std::lock_guard<std::mutex> lock(mutex);if(owned&&ownsLocked())return false;}
    return originalReload(weapon,automatic);
}
inline bool readable(const void* p,std::size_t n){
    MEMORY_BASIC_INFORMATION m{};if(!p||!VirtualQuery(p,&m,sizeof m)||m.State!=MEM_COMMIT||(m.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
    auto offset=reinterpret_cast<std::uintptr_t>(p)-reinterpret_cast<std::uintptr_t>(m.BaseAddress);
    return offset<=m.RegionSize&&n<=m.RegionSize-offset;
}
// Restore every changed word after the native renderer has copied the palette.
// Never patch mesh files or leave gameplay animation data modified.
class Parts {
    Bone* target{};Bone saved[12]{};std::uint32_t* mask{};std::uint32_t savedMask{};
public:
    explicit Parts(void* entity){
        auto v=visual();if(!v.valid||v.stage==Stage::Ready)return;
        auto bytes=static_cast<unsigned char*>(entity);
        if(!readable(bytes,0x58)||*reinterpret_cast<std::uintptr_t*>(bytes)!=image+0x30c640)return;
        auto animation=*reinterpret_cast<unsigned char**>(bytes+0x50);
        if(!readable(animation,0x34+sizeof saved)||animation[0x10]!=12)return;
        auto bones=reinterpret_cast<Bone*>(animation+0x34);
        // Refuse unknown layouts/invalid animation records rather than writing
        // through a guessed model pointer.
        if(!validBone(bones[5])||!validBone(bones[6])||!validBone(bones[8]))return;
        target=bones;std::memcpy(saved,target,sizeof saved);
        mask=reinterpret_cast<std::uint32_t*>(animation+0x18);savedMask=*mask;
        *mask|=(1u<<6)|(1u<<8);
        if((v.stage==Stage::MagazineHeld||v.stage==Stage::SlideHeld)&&validBone(bones[10])&&validBone(bones[11]))*mask|=(1u<<10)|(1u<<11);
        applyParts(target,v);++partDraws;
    }
    ~Parts(){if(target){std::memcpy(target,saved,sizeof saved);*mask=savedMask;}}
    Parts(const Parts&)=delete;Parts& operator=(const Parts&)=delete;
};
inline bool matchesHooks(std::uintptr_t candidate){
    const unsigned char updateCode[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0xe8,0x52,0xe5,0xff,0xff};
    const unsigned char fireCode[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x0f,0xb7,0x41,0x18};
    const unsigned char reloadCode[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x20};
    const unsigned char paletteCode[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x48,0x89,0x7c,0x24,0x18};
    return candidate&&!(std::memcmp(reinterpret_cast<void*>(candidate+0x8b810),updateCode,sizeof updateCode)||
       std::memcmp(reinterpret_cast<void*>(candidate+0x8b8d0),fireCode,sizeof fireCode)||
       std::memcmp(reinterpret_cast<void*>(candidate+0x8b4f0),reloadCode,sizeof reloadCode)||
       std::memcmp(reinterpret_cast<void*>(candidate+0x7c990),paletteCode,sizeof paletteCode));
}
inline bool install(){
    image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if(!enabled)return true;
    if(!native.verify(image)||!matchesHooks(image))return false;
    const std::uintptr_t rvas[]={0x8b810,0x8b8d0,0x8b4f0};
    void* hooks[]={reinterpret_cast<void*>(updateHook),reinterpret_cast<void*>(fireHook),reinterpret_cast<void*>(reloadHook)};
    void** originals[]={reinterpret_cast<void**>(&originalUpdate),reinterpret_cast<void**>(&originalFire),reinterpret_cast<void**>(&originalReload)};
    for(unsigned i=0;i<3;i++)if(MH_CreateHook(reinterpret_cast<void*>(image+rvas[i]),hooks[i],originals[i])!=MH_OK){
        for(unsigned j=0;j<i;j++)MH_RemoveHook(reinterpret_cast<void*>(image+rvas[j]));return false;
    }
    installed=true;return true;
}
}
