#pragma once
#include <windows.h>
#include <MinHook.h>
#include <array>
#include <cstdint>
#include <cstring>

namespace nativeSwarmBounds {
static_assert(sizeof(std::uintptr_t)==8,"Native swarm guard requires x64");
inline void* thunk{};
inline constexpr std::uintptr_t guardRva=0x71def;
// EnemySwarm movement uses 30 records at +240, stride30. Record30 aliases
// voices at +7E0. Native JAE only skips setting a pending bit; a stale set bit
// still executes the out-of-bounds movement write. Skip that write at capacity.
inline constexpr unsigned char expected[]={
    0x41,0x83,0xf9,0x1e,0x73,0x04,0x83,0x4b,0x44,0x20,
    0x8b,0x53,0x44,0xf6,0xc2,0x20,0x0f,0x84,0xee,0,0,0};
inline bool install(std::uintptr_t base=0){
    if(thunk)return true;
    if(!base)base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    auto target=reinterpret_cast<void*>(base+guardRva);
    unsigned char actual[sizeof expected]{};SIZE_T copied{};
    if(!ReadProcessMemory(GetCurrentProcess(),target,actual,sizeof actual,&copied)||copied!=sizeof actual||memcmp(actual,expected,sizeof actual))return false;
    // Both detour destinations belong to this exact native function layout.
    constexpr unsigned char tail[]={0x48,0x8b,0xcb,0x48,0x83,0xc4,0x50,0x5b,0xe9,0x80,0x3a,0,0};
    unsigned char tailActual[sizeof tail]{};
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(base+0x71ef3),tailActual,sizeof tailActual,&copied)||copied!=sizeof tailActual||memcmp(tailActual,tail,sizeof tail))return false;
    // No C++ call in a mid-function detour: retain every register and RSP.
    // cmp r9d,30; jae full; or [rbx+44],20; jmp native continuation;
    // full: jmp native post-movement continuation. RIP-relative absolute jumps
    // do not consume a scratch register. At capacity clear only the extra-entry bit: native renderer71D80
    // would otherwise temporarily increase count30 to31 and read voice memory.
    std::array<unsigned char,42> code={0x41,0x83,0xf9,0x1e,0x73,0x12,
        0x83,0x4b,0x44,0x20,0xff,0x25,0,0,0,0};
    auto normal=base+0x71df9,full=base+0x71ef3;
    memcpy(code.data()+16,&normal,8);
    code[24]=0x83;code[25]=0x63;code[26]=0x44;code[27]=0xdf; // and [rbx+44],~20
    code[28]=0xff;code[29]=0x25;memcpy(code.data()+34,&full,8);
    void* memory=VirtualAlloc(nullptr,code.size(),MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(!memory)return false;
    memcpy(memory,code.data(),code.size());DWORD old{};
    if(!VirtualProtect(memory,code.size(),PAGE_EXECUTE_READ,&old)){VirtualFree(memory,0,MEM_RELEASE);return false;}
    FlushInstructionCache(GetCurrentProcess(),memory,code.size());
    void* unusedGateway{};
    if(MH_CreateHook(target,memory,&unusedGateway)!=MH_OK){VirtualFree(memory,0,MEM_RELEASE);return false;}
    thunk=memory;return true;
}
}
