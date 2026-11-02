#pragma once
#include "wristband_model.hpp"
#include "head_lamp.hpp"
#include "arsenal_runtime.hpp"
#include "pill_bottle.hpp"
#include "ammo_limits.hpp"
#include "weapon_group_runtime.hpp"
namespace wristband {
struct CodeGuard{std::uintptr_t rva;std::array<unsigned char,16> bytes;};
inline const CodeGuard guards[]={{0x867e0,{0x48,0x8b,0xc4,0x55,0x48,0x8d,0x68,0xa1,0x48,0x81,0xec,0xe0,0x0,0x0,0x0,0x44}},{0x843e0,{0xb8,0xa,0x0,0x0,0x0,0x2a,0x5,0x55,0x1f,0x31,0x0,0xc3,0xcc,0xcc,0xcc,0xcc}},{0x84f8a,{0x44,0xf,0xb6,0xd,0xae,0x13,0x31,0x0,0xba,0xa,0x0,0x0,0x0,0x44,0x8b,0xc2}},{0x88caf,{0x44,0xf,0xb6,0xd,0x89,0xd6,0x30,0x0,0xba,0xa,0x0,0x0,0x0,0x8b,0xc2,0x41}},{0x90ee0,{0x8b,0x5,0x5a,0x54,0x30,0x0,0xf,0xb6,0x2d,0x93,0x54,0x30,0x0,0xc1,0xe8,0x12}},{0x915d0,{0x81,0xfa,0xe8,0x3,0x0,0x0,0x73,0x1b,0x8b,0xc2,0x48,0x8d,0xd,0x9f,0x4d,0x30}},{0x91600,{0x48,0x83,0xec,0x8,0x44,0x8b,0xca,0x81,0xfa,0xe8,0x3,0x0,0x0,0xf,0x83,0x8e}}};
inline std::uintptr_t image{};inline bool adapterVerified{};inline std::uint64_t lastPoll{};
using PlayerUpdate=void(*)(void*,void*,void*);inline PlayerUpdate originalPlayerUpdate{};
inline bool verify(std::uintptr_t base){
    for(const auto& g:guards)if(std::memcmp(reinterpret_cast<const void*>(base+g.rva),g.bytes.data(),16))return false;
    return *reinterpret_cast<std::uintptr_t*>(base+0x30c0f8+0x20)==base+0x867e0;
}
inline Snapshot decode(unsigned deficit,int weapon,int loaded,int reserve,std::uint64_t now){
    Snapshot s;s.time=now;s.world=true;
    // Native FPE health is 10 minus the saved deficit byte. Maximum 10 is
    // established by both the health predicate and damage/restore clamps.
    if(deficit<=10){s.health=10-static_cast<int>(deficit);s.maximum=10;s.healthValid=true;}
    s.weapon=weapon;s.loaded=loaded;s.reserve=reserve;s.ammoValid=weapon>=0&&weapon<=4;
    s.meds=static_cast<int>(pillBottle::carried.load());s.medsValid=pillBottle::installed;
    return sanitize(s);
}
inline Snapshot capture(void* entity,std::uint64_t now){
    Snapshot missing;missing.time=now;
    if(!adapterVerified||!manualReload::readable(entity,0x320)||
       *reinterpret_cast<void**>(image+0x3a5340)!=entity||
       *reinterpret_cast<std::uintptr_t*>(entity)!=image+0x30c0f8||
       *reinterpret_cast<std::uintptr_t*>(static_cast<unsigned char*>(entity)+0xb8)!=image+0x30bff8||
       !*reinterpret_cast<void**>(image+0x3a5330))return missing;
    int weapon=-1,loaded=0,reserve=0;
    auto head=reinterpret_cast<std::uintptr_t>(entity)+0x308;
    auto link=*reinterpret_cast<std::uintptr_t*>(head);
    if(link!=head&&link>=0x98&&manualReload::readable(reinterpret_cast<void*>(link-0x98),0xd0)){
        auto held=reinterpret_cast<unsigned char*>(link-0x98);auto vt=*reinterpret_cast<std::uintptr_t*>(held);
        const std::uintptr_t tables[]={0x30c500,0x30c640,0x30c900,0x30d2e0,0x30d668};
        const unsigned loadedRva[]={0x396346,0x396344,0x39634a,0x396348,0x39634c};
        const unsigned reserveRva[]={0x396345,0x396343,0x396349,0x396347,0x39634b};
        for(int i=0;i<5;i++)if(vt==image+tables[i]&&(*reinterpret_cast<unsigned*>(held+0x44)&0x18)==0x18){weapon=i;loaded=*reinterpret_cast<unsigned char*>(image+loadedRva[i]);reserve=*reinterpret_cast<unsigned char*>(image+reserveRva[i]);}
    }
    return decode(*reinterpret_cast<unsigned char*>(image+0x396340),weapon,loaded,reserve,now);
}
inline void playerUpdate(void* entity,void* step,void* context){
    ammoLimits::clamp(entity);originalPlayerUpdate(entity,step,context);auto now=GetTickCount64();weaponGroups::observeEquipment(entity,now);headLamp::tick(entity);pillBottle::tick(entity);weaponGroups::tick(entity);
    if(now<lastPoll||now-lastPoll>=50){lastPoll=now;model.publish(capture(entity,now));}
}
inline bool installAdapter(){
    image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));adapterVerified=verify(image);if(!adapterVerified)return false;
    if(MH_CreateHook(reinterpret_cast<void*>(image+0x867e0),reinterpret_cast<void*>(playerUpdate),reinterpret_cast<void**>(&originalPlayerUpdate))!=MH_OK){adapterVerified=false;return false;}return true;
}
}

