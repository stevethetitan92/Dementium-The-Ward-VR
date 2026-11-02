#pragma once
#include "simulation_clock.hpp"
#include <windows.h>
#include <MinHook.h>
#include <intrin.h>
#include <cstdint>
#include <cstring>
namespace nativeSimulationClock {
using Frame=void(*)(std::uintptr_t,bool,bool,bool,bool);
using Input=void(*)(void*,void*);
inline Frame originalFrame{};
inline Input originalInput{};
inline bool(*enabled)(){};
inline std::uintptr_t base{};
inline std::uint32_t* frameCounter{};
inline thread_local bool inFrame{},renderOnly{};
inline double nativeHz{};
inline simulationClock::Clock clock;
inline unsigned long long updates{},extraRenders{};
inline double seconds(){static const double frequency=[](){LARGE_INTEGER v;QueryPerformanceFrequency(&v);return double(v.QuadPart);}();LARGE_INTEGER v;QueryPerformanceCounter(&v);return double(v.QuadPart)/frequency;}
inline void input(void* player,void* sample){
    // Native 853E0 also mutates player input; an extra rendered view must not
    // inject a zero input sample between native updates.
    if(renderOnly&&reinterpret_cast<std::uintptr_t>(_ReturnAddress())==base+0x3a8ee)return;
    originalInput(player,sample);
}
inline void frame(std::uintptr_t first,bool services,bool inputSample,bool simulation,bool render){
    if(inFrame||reinterpret_cast<std::uintptr_t>(_ReturnAddress())!=base+0x3bb85||!enabled||!enabled()||!services||!inputSample||!simulation||!render||nativeHz<30||nativeHz>120){
        clock.reset();originalFrame(first,services,inputSample,simulation,render);return;
    }
    const bool advance=clock.update(seconds(),nativeHz);
    struct Scope {Scope(bool extra){inFrame=true;renderOnly=extra;}~Scope(){renderOnly=false;inFrame=false;}} scope(!advance);
    if(advance){++updates;originalFrame(first,services,inputSample,simulation,render);}
    else {
        ++extraRenders;
        // The exact native entry increments this counter once even when its
        // simulation flag is false. Hold its tick value for the extra render.
        --*frameCounter;
        originalFrame(first,false,false,false,render);
    }
}
template<size_t N> inline bool signature(std::uintptr_t rva,const unsigned char(&expected)[N]){
    unsigned char actual[N]{};SIZE_T count{};
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(base+rva),actual,N,&count)&&count==N&&!memcmp(actual,expected,N);
}
inline bool install(){
    base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const unsigned char entry[]={0x48,0x8b,0xc4,0x48,0x89,0x58,0x08,0x48,0x89,0x70,0x10,0x48,0x89,0x78,0x20};
    const unsigned char caller[]={0xe8,0x9b,0xec,0xff,0xff};
    const unsigned char counter[]={0xff,0x05,0xd1,0x1a,0x3c,0};
    const unsigned char branch[]={0x45,0x84,0xf6,0x0f,0x84,0xe8,0x01,0,0};
    const unsigned char inputEntry[]={0x48,0x89,0x54,0x24,0x10,0x53,0x56,0x41,0x56};
    const unsigned char inputCaller[]={0xe8,0xf2,0xaa,0x04,0};
    if(!signature(0x3a820,entry)||!signature(0x3bb80,caller)||!signature(0x3a8d1,counter)||!signature(0x3a91a,branch)||!signature(0x853e0,inputEntry)||!signature(0x3a8e9,inputCaller))return false;
    frameCounter=reinterpret_cast<std::uint32_t*>(base+0x3fc3a8);
    if(MH_CreateHook(reinterpret_cast<void*>(base+0x3a820),reinterpret_cast<void*>(frame),reinterpret_cast<void**>(&originalFrame))!=MH_OK)return false;
    if(MH_CreateHook(reinterpret_cast<void*>(base+0x853e0),reinterpret_cast<void*>(input),reinterpret_cast<void**>(&originalInput))!=MH_OK){MH_RemoveHook(reinterpret_cast<void*>(base+0x3a820));return false;}
    return true;
}
}
