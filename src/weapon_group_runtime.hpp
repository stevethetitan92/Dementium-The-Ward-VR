#pragma once
#include "weapon_groups.hpp"
#include "arsenal_runtime.hpp"
namespace weaponGroups {
inline Requests requests;
inline std::atomic<int> nativeEquipped{-1};
inline std::atomic<bool> nativeEquipmentValid{},lampOwned{};
inline void(*equipmentObserved)(int,std::uint64_t){};
inline void observeEquipment(void* player,std::uint64_t now){
    auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if(player!=*reinterpret_cast<void**>(base+0x3a5340)||!manualReload::readable(player,0x360)||*reinterpret_cast<std::uintptr_t*>(player)!=base+0x30c0f8)return;
    lampOwned=(*reinterpret_cast<unsigned char*>(base+0x396380)&1)!=0;
    auto head=reinterpret_cast<std::uintptr_t>(player)+0x308,link=*reinterpret_cast<std::uintptr_t*>(head);
    if(link==head||link<0x10098||!manualReload::readable(reinterpret_cast<void*>(link-0x98),0xd0))return;
    auto item=reinterpret_cast<unsigned char*>(link-0x98);auto table=*reinterpret_cast<std::uintptr_t*>(item);auto flags=*reinterpret_cast<unsigned*>(item+0x44);
    constexpr std::uintptr_t tables[]={0x30c500,0x30c640,0x30c900,0x30d2e0,0x30d668,0x30cb70,0x30d1a8};
    int current=-2;
    if(table==base+0x30cdd0&&(flags&8))current=-1; // Native flashlight-only loadout.
    for(int i=0;i<7;++i)if(table==base+tables[i]&&(flags&8))current=i;
    if(current< -1)return;
    nativeEquipped=current;nativeEquipmentValid=true;
    if(equipmentObserved)equipmentObserved(current,now);
}
inline void request(int weapon,std::uint64_t now){requests.submit(weapon,-1,now);}
inline void equip(int current,int target,std::uint64_t now){requests.submit(current,target,now);}
inline void equipFromInventory(int current,int target,std::uint64_t now){requests.submit(current,target,now,true);}
inline void tick(void* player){
    auto command=requests.take();int current=command.current,requestedTarget=command.target;if(current< -1||current>6||(current==-1&&requestedTarget<0))return;
    auto now=GetTickCount64(),when=command.time;if(now<when||now-when>150||manualReload::menuState()!=0)return;
    manualReload::Hands h;{std::lock_guard<std::mutex> lock(manualReload::mutex);h=manualReload::hands;}
    if(!command.inventory&&(!h.session||!h.gameplay||!h.tracked||h.weapon!=current||now<h.time||now-h.time>100))return;
    auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if(player!=*reinterpret_cast<void**>(base+0x3a5340)||!manualReload::readable(player,0x360)||*reinterpret_cast<std::uintptr_t*>(player)!=base+0x30c0f8)return;
    const unsigned char code[]={0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x41,0x56,0x41,0x57,0x48};
    if(std::memcmp(reinterpret_cast<void*>(base+0x914c0),code,sizeof code))return;
    unsigned owned=*reinterpret_cast<unsigned char*>(base+0x396380);int target=requestedTarget>=0?requestedTarget:next(current,owned);if(target<0||target>6||target==current||!(owned&(1u<<ids[target])))return;
    // Native deferred holster/equip transition; no intrusive list or ammo edits.
    reinterpret_cast<void(*)(void*,unsigned)>(base+0x914c0)(static_cast<unsigned char*>(player)+0x300,0x10800+ids[target]);
}
}
