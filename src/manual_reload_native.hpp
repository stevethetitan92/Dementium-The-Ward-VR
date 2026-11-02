#pragma once
#include "manual_reload.hpp"
#include <Windows.h>
#include <cstring>

namespace manualReload {
// Invoke only from the native
// simulation thread, after validating the active pistol and spatial sequence.
class NativePistol {
    std::uintptr_t base{};
    bool verified{};
public:
    bool verify(std::uintptr_t image){
        base=image;verified=false;
        const unsigned char loaded[]={0x0f,0xb6,0x05,0x9d,0xae,0x30,0,0xc3};
        const unsigned char reserve[]={0x0f,0xb6,0x05,0xcc,0xae,0x30,0,0xc3};
        const unsigned char capacity[]={0xb8,0x0f,0,0,0,0xc3};
        const unsigned char commit[]={0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48,0x83,0xec,0x20};
        if(!base||std::memcmp(reinterpret_cast<void*>(base+0x8b4a0),loaded,sizeof loaded)||
           std::memcmp(reinterpret_cast<void*>(base+0x8b470),reserve,sizeof reserve)||
           std::memcmp(reinterpret_cast<void*>(base+0x8b4e0),capacity,sizeof capacity)||
           std::memcmp(reinterpret_cast<void*>(base+0x91db0),commit,sizeof commit))return false;
        const std::uintptr_t offsets[]={0x70,0x78,0x80,0x88,0x90};
        const std::uintptr_t targets[]={0x8b4a0,0x8b4b0,0x8b4e0,0x8b470,0x8b480};
        for(unsigned i=0;i<5;i++)if(*reinterpret_cast<std::uintptr_t*>(base+0x30c748+offsets[i])!=base+targets[i])return false;
        return verified=true;
    }
    bool commit(Pistol& reload,void* weaponInterface){
        auto request=reload.pendingToken();
        if(!verified||!request||!weaponInterface||*reinterpret_cast<std::uintptr_t*>(weaponInterface)!=base+0x30c748)return false;
        using Getter=int(*)(void*);using Apply=bool(*)(void*,bool);
        int loaded=reinterpret_cast<Getter>(base+0x8b4a0)(weaponInterface);
        int reserve=reinterpret_cast<Getter>(base+0x8b470)(weaponInterface);
        int capacity=reinterpret_cast<Getter>(base+0x8b4e0)(weaponInterface);
        // Reuse the game's bounded transfer and UI notification. Never call
        // the scripted reload animation, which transfers ammo at its start.
        return completeReload(reload,loaded,reserve,capacity,[&]{return reinterpret_cast<Apply>(base+0x91db0)(weaponInterface,false);});
    }
};
}
