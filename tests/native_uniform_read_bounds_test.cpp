#include "../src/native_uniform_diagnostic.hpp"
#include <map>
#include <vector>
#include <cstring>
#include <cstdio>
#include <stdexcept>
static void require(bool value){if(!value)throw std::runtime_error("uniform diagnostic read-boundary check failed");}
int main(){try{
    constexpr std::uintptr_t image=0x10000000,ring=0x20000000;std::map<std::uintptr_t,std::vector<unsigned char>> memory;
    auto write=[&](std::uintptr_t address,const auto& value){auto& bytes=memory[address];bytes.resize(sizeof value);std::memcpy(bytes.data(),&value,sizeof value);};
    const unsigned char allocation[]={0x48,0x8b,0x5f,0x10,0x48,0x81,0xc3,0xff,0,0,0,0x48,0x81,0xe3,0,0xff,0xff,0xff},capacity[]={0x48,0xc7,0x41,0x08,0,0,0,0x10};
    write(image+0x1e9428,allocation);write(image+0x1e77fe,capacity);write(image+0x6bb798,ring);std::array<std::uint64_t,6> values{0,0x10000000,0,0,0,0};write(ring,values);
    unsigned calls{},wrapped{};auto reader=[&](std::uintptr_t address,void* output,size_t count){++calls;if(address==0xf||address==0x17)++wrapped;auto found=memory.find(address);if(found==memory.end()||count>found->second.size())return false;std::memcpy(output,found->second.data(),count);return true;};
    nativeUniformDiagnostic::Snapshot snapshot;require(nativeUniformDiagnostic::capture(image,snapshot,reader)&&snapshot.ring==ring);
    const auto invalid=UINTPTR_MAX-8;write(image+0x6bb798,invalid);write(invalid,values);
    if(nativeUniformDiagnostic::capture(image,snapshot,reader)){std::puts("FAIL: diagnostic accepted ring read crossing address-space end");return 1;}write(image+0x6bb798,ring);
    write(image+0xc1acb0,invalid);write(std::uintptr_t(0xf),std::uintptr_t(0x30000000));write(std::uintptr_t(0x17),std::uintptr_t(0x40000000));write(std::uintptr_t(0x30000018),0);write(std::uintptr_t(0x40000018),0);
    require(nativeUniformDiagnostic::capture(image,snapshot,reader)&&!snapshot.vertexKnown&&!snapshot.pixelKnown&&!wrapped);
    auto partial=[&](std::uintptr_t address,void* output,size_t count){
        if(address==image+0x6bb7d8||address==image+0x6945a8||address==image+0xc1a8d8||address==image+0xc1acb0){std::memset(output,0x11,count);return false;}return reader(address,output,count);
    };
    require(nativeUniformDiagnostic::capture(image,snapshot,partial));
    if(snapshot.commandList||snapshot.frame||snapshot.dirty||snapshot.shader){std::puts("FAIL: failed partial optional reads survived as native evidence");return 1;}
    const auto before=calls;require(!nativeUniformDiagnostic::capture(UINTPTR_MAX-0x100,snapshot,reader)&&calls==before);require(!nativeUniformDiagnostic::capture(0,snapshot,reader)&&calls==before);
    std::puts("PASS: uniform diagnostic rejects crossing ring/image ranges and wrapped shader fields before reader calls; valid ring survives optional unknown shader fields; fake memory only, no game/GPU/XR.");return 0;
}catch(const std::exception& e){std::puts(e.what());return 1;}}
