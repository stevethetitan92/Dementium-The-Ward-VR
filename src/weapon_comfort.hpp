#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>
namespace weaponComfort {
// Steam 1.1.10: HandheldEntity's shared hitscan firing routine. The
// weapon's vtable +0x58 supplies the kick strength; only this subtraction
// feeds it into the player's local movement vector. Preserve the existing
// velocity, projectile/hit logic, ammo, sound and weapon animation.
inline constexpr std::uintptr_t signatureRVA=0x92335;
inline constexpr unsigned impulseOffset=0x25;
inline constexpr unsigned char signature[]={
    0x48,0x8b,0x0f,0x48,0x8b,0x51,0x58,0x48,0x8b,0xcf,0xff,0xd2,
    0xf3,0x0f,0x10,0x53,0x08,0x45,0x0f,0x57,0xc9,0x41,0x0f,0x2f,0xd1,
    0x76,0x0a,0x0f,0x28,0xc8,0xf3,0x0f,0x58,0xc8,0x0f,0x28,0xc1,
    0xf3,0x0f,0x5c,0xd0,0xf3,0x0f,0x11,0x53,0x08};
inline constexpr unsigned char replacement[]={0x90,0x90,0x90,0x90};
inline bool matches(const unsigned char* code){return std::memcmp(code,signature,sizeof(signature))==0;}
inline bool install(){
    auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if(!base||dos->e_magic!=IMAGE_DOS_SIGNATURE)return false;
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->OptionalHeader.SizeOfImage<signatureRVA+sizeof(signature))return false;
    auto code=base+signatureRVA;
    if(!matches(code))return false;
    DWORD previous{};
    if(!VirtualProtect(code+impulseOffset,sizeof(replacement),PAGE_EXECUTE_READWRITE,&previous))return false;
    std::memcpy(code+impulseOffset,replacement,sizeof(replacement));
    FlushInstructionCache(GetCurrentProcess(),code+impulseOffset,sizeof(replacement));
    DWORD ignored{};VirtualProtect(code+impulseOffset,sizeof(replacement),previous,&ignored);
    return true;
}
}
