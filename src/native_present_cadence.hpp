#pragma once
#include <windows.h>
#include <MinHook.h>
#include <intrin.h>
#include <cstdint>
#include <cstring>

namespace nativePresentCadence {
// Native desktop refresh conversion ALSO paces the fixed-step game loop.
// Do not enable single presentation without an independent simulation clock.
struct State { std::uint64_t numerator, denominator, remainder; unsigned rendered, presented; };
using Function=void(*)(State*,void*,void*,void*);
inline Function original{};
inline bool(*enabled)(){};
inline std::uintptr_t expectedCaller{};
inline unsigned long long corrected{},observedNumerator{},observedDenominator{};
inline void(*sourceRateObserved)(std::uint64_t,std::uint64_t){};
template<class Invoke> void singlePresentation(State& state,Invoke invoke){
    const auto numerator=state.numerator,remainder=state.remainder;
    struct Restore {State& s;std::uint64_t n,r;~Restore(){s.numerator=n;s.remainder=r;}} restore{state,numerator,remainder};
    state.numerator=state.denominator;state.remainder=0;
    // Keep native callback invocation, cleanup, counters and synchronization.
    invoke();
}
inline void hook(State* state,void* skip,void* first,void* repeat){
    if(state&&sourceRateObserved&&reinterpret_cast<std::uintptr_t>(_ReturnAddress())==expectedCaller){
        // At the signature-checked caller: state=renderer+D0; original source
        // clock is renderer+90/+98. Read its actual rate, not a guessed 60 Hz.
        static const State* observedState{};
        if(observedState!=state){std::uint64_t rate[2]{};SIZE_T count{};
            if(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const unsigned char*>(state)-0x40,rate,sizeof rate,&count)&&count==sizeof rate&&rate[1]){
                sourceRateObserved(rate[0],rate[1]);observedState=state;
            }
        }
    }
    if(reinterpret_cast<std::uintptr_t>(_ReturnAddress())==expectedCaller&&enabled&&enabled()&&state&&state->denominator){
        observedNumerator=state->numerator;observedDenominator=state->denominator;++corrected;
        singlePresentation(*state,[&]{original(state,skip,first,repeat);});
    }else original(state,skip,first,repeat);
}
inline bool install(){
    const auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    constexpr unsigned char signature[]={0x48,0x89,0x5c,0x24,0x08,0x4c,0x89,0x4c,0x24,0x20,0x4c,0x89,0x44,0x24,0x18,0x48,0x89,0x54,0x24,0x10};
    constexpr unsigned char caller[]={0xe8,0x3d,0xe9,0xff,0xff,0xb9,0xa2,0,0,0};
    unsigned char bytes[sizeof signature]{};SIZE_T read{};
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(base+0x1f5fd0),bytes,sizeof bytes,&read)||read!=sizeof bytes||memcmp(bytes,signature,sizeof bytes))return false;
    unsigned char site[sizeof caller]{};
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(base+0x1f768e),site,sizeof site,&read)||read!=sizeof site||memcmp(site,caller,sizeof site))return false;
    expectedCaller=base+0x1f7693;
    return MH_CreateHook(reinterpret_cast<void*>(base+0x1f5fd0),reinterpret_cast<void*>(hook),reinterpret_cast<void**>(&original))==MH_OK;
}
}
