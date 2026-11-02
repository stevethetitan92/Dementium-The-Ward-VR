#pragma once
#include <array>
#include <cstdint>
#include <limits>
#include <cstring>
namespace nativeUniformDiagnostic {
struct Snapshot {
    std::uintptr_t ring{},resource{},cpu{},commandList{},shader{};
    std::uint64_t capacity{},cursor{},gpu{},dirty{};
    int frame{},vertexBytes{},pixelBytes{};bool vertexKnown{},pixelKnown{};
    bool atEnd(std::uint64_t address)const {
        return capacity&&capacity<=(std::numeric_limits<std::uint64_t>::max)()-gpu&&address==gpu+capacity;
    }
    bool contains(std::uint64_t address)const{return capacity&&address>=gpu&&address-gpu<capacity;}
};
// Read-only, failure-path capture for the precisely identified executable.
// No native functions are invoked, no shader/ring/pointer is repaired here.
template<class Read> bool capture(std::uintptr_t image,Snapshot& output,Read reader){
    output={};
    auto read=[&](std::uintptr_t base,std::uintptr_t offset,void* target,size_t bytes){
        const auto maximum=(std::numeric_limits<std::uintptr_t>::max)();
        if(base&&offset<=maximum-base&&bytes<=maximum-(base+offset)&&reader(base+offset,target,bytes))return true;
        // A reader may have copied a prefix before reporting failure. Never
        // retain that prefix as optional native evidence or candidate input.
        std::memset(target,0,bytes);return false;
    };
    const unsigned char allocation[]={0x48,0x8b,0x5f,0x10,0x48,0x81,0xc3,0xff,0,0,0,0x48,0x81,0xe3,0,0xff,0xff,0xff};
    const unsigned char capacityCode[]={0x48,0xc7,0x41,0x08,0,0,0,0x10};
    std::array<unsigned char,sizeof allocation> found{};std::array<unsigned char,sizeof capacityCode> constructor{};
    if(!read(image,0x1e9428,found.data(),found.size())||!read(image,0x1e77fe,constructor.data(),constructor.size()))return false;
    for(unsigned i=0;i<found.size();++i)if(found[i]!=allocation[i])return false;
    for(unsigned i=0;i<constructor.size();++i)if(constructor[i]!=capacityCode[i])return false;
    Snapshot result;
    if(!read(image,0x6bb798,&result.ring,sizeof result.ring)||!result.ring)return false;
    std::array<std::uint64_t,6> ring{};if(!read(result.ring,0,ring.data(),sizeof ring))return false;
    result.capacity=ring[1];result.cursor=ring[2];result.resource=ring[3];result.cpu=ring[4];result.gpu=ring[5];
    read(image,0x6bb7d8,&result.commandList,sizeof result.commandList);
    read(image,0x6945a8,&result.frame,sizeof result.frame);
    read(image,0xc1a8d8,&result.dirty,sizeof result.dirty);
    if(read(image,0xc1acb0,&result.shader,sizeof result.shader)&&result.shader){
        std::uintptr_t vertex{},pixel{};
        if(read(result.shader,0x18,&vertex,sizeof vertex)&&vertex)result.vertexKnown=read(vertex,0x18,&result.vertexBytes,sizeof result.vertexBytes);
        if(read(result.shader,0x20,&pixel,sizeof pixel)&&pixel)result.pixelKnown=read(pixel,0x18,&result.pixelBytes,sizeof result.pixelBytes);
    }
    output=result;return true;
}
}
