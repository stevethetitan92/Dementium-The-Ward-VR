#include "../src/weapon_comfort.hpp"
#include <fstream>
#include <vector>
#include <cstdio>
#include <cmath>
#include <stdexcept>
void check(bool value){if(!value)throw std::runtime_error("weapon comfort validation failed");}
int main(){try{
    std::ifstream file("C:\\Program Files (x86)\\Steam\\steamapps\\common\\Dementium - The Ward\\Dementium.exe",std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{});check(bytes.size()>4096);
    auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());
    auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(bytes.data()+dos->e_lfanew);
    auto sections=IMAGE_FIRST_SECTION(nt);const unsigned char* native=nullptr;
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;i++){
        auto& s=sections[i];auto rva=weaponComfort::signatureRVA;
        if(rva>=s.VirtualAddress&&rva+sizeof(weaponComfort::signature)<=s.VirtualAddress+s.SizeOfRawData)
            native=bytes.data()+s.PointerToRawData+rva-s.VirtualAddress;
    }
    check(native&&weaponComfort::matches(native));
    // Execute the exact native subtraction in isolation, then its replacement.
    // Both forward and backward motion must survive a shot unchanged.
    unsigned char snippet[]={0x0f,0x28,0xd0,0x0f,0x28,0xc1,0xf3,0x0f,0x5c,0xd0,0x0f,0x28,0xc2,0xc3};
    auto code=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));check(code!=nullptr);
    std::memcpy(code,snippet,sizeof(snippet));
    using Operation=float(*)(float,float);auto operation=reinterpret_cast<Operation>(code);
    check(std::abs(operation(0.f,.03f)+.03f)<1.e-6f);
    check(std::memcmp(code+6,native+weaponComfort::impulseOffset,4)==0);
    std::memcpy(code+6,weaponComfort::replacement,4);FlushInstructionCache(GetCurrentProcess(),code,sizeof(snippet));
    for(float velocity:{-.1f,0.f,.1f})for(float kick:{.009f,.03f,.06f,.1f})check(operation(velocity,kick)==velocity);
    VirtualFree(code,0,MEM_RELEASE);
    puts("PASS: installed game firing signature; original impulse reproduced; replacement preserves movement for all tested kick strengths.");return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}
