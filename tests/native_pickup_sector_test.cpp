#include "../src/native_pickup_sector.hpp"
#include <map>
#include <vector>
#include <cstdio>
#include <stdexcept>
using namespace nativePickupSector;
static void require(bool value){if(!value)throw std::runtime_error("pickup sector snapshot check failed");}
int main(){try{
    std::map<std::uintptr_t,unsigned char> memory;
    auto bytes=[&](std::uintptr_t at,const void* data,size_t count){auto p=static_cast<const unsigned char*>(data);for(size_t i=0;i<count;++i)memory[at+i]=p[i];};
    auto write=[&](std::uintptr_t at,auto value){bytes(at,&value,sizeof value);};
    unsigned reads{};auto reader=[&](std::uintptr_t at,void* output,size_t count){++reads;auto p=static_cast<unsigned char*>(output);for(size_t i=0;i<count;++i){auto found=memory.find(at+i);if(found==memory.end())return false;p[i]=found->second;}return true;};
    constexpr std::uintptr_t image=0x10000000,entity=0x20000000,query=0x30000000,sector=0x40000000,manager=0x50000000,base=0x60000000;
    const unsigned char entityCode[]={0x48,0x8b,0x87,0x90,0,0,0},countCode[]={0x8b,0x83,0x24,0x01,0,0,0x39,0x83,0xb8,0,0,0};bytes(image+0x7c90a,entityCode,sizeof entityCode);bytes(image+0x7c92c,countCode,sizeof countCode);
    require(capture(image,entity,0,reader).status==Status::NoQuery);
    require(capture(image,entity,query,reader).status==Status::Unknown);
    write(query+0xc0,std::uintptr_t(0));reads=0;require(capture(image,entity,query,reader).status==Status::NoCamera&&reads==3);
    write(query+0xc0,std::uintptr_t(0x70000000));write(entity+0x90,std::uintptr_t(0));require(capture(image,entity,query,reader).status==Status::NoEntitySector);
    write(entity+0x90,sector);write(query+0xb8,std::uint32_t(10));write(query+0x124,std::uint32_t(10));require(capture(image,entity,query,reader).status==Status::CountBypass);
    write(query+0x124,std::uint32_t(9));write(query+0xb0,std::uintptr_t(0));require(capture(image,entity,query,reader).status==Status::NoCurrentSector);
    write(query+0xb0,std::uintptr_t(0x71000000));write(query+0xac,std::uint32_t(123));write(sector+0x18,std::uint32_t(0));require(capture(image,entity,query,reader).status==Status::StampUnavailable);
    write(sector+0x18,std::uint32_t(0x80));write(image+0x3fc340,manager);write(manager+8,base);write(base+0x88,std::uint32_t(122));
    auto differs=capture(image,entity,query,reader);require(differs.status==Status::StampDiffers&&differs.record==base+0x80&&differs.count==10&&differs.limit==9&&differs.revision==123&&differs.stamp==122);
    write(base+0x88,std::uint32_t(123));require(capture(image,entity,query,reader).status==Status::StampMatches);
    // Native comparison is unsigned, not a signed <= test.
    write(query+0xb8,std::uint32_t(0xffffffff));write(query+0x124,std::uint32_t(0));require(capture(image,entity,query,reader).status==Status::StampMatches);
    write(manager+8,(std::numeric_limits<std::uintptr_t>::max)()-16);require(capture(image,entity,query,reader).status==Status::StampUnavailable);
    write(manager+8,base);memory.erase(base+0x88);require(capture(image,entity,query,reader).status==Status::StampUnavailable);
    memory[image+0x7c90a]=0x90;require(capture(image,entity,query,reader).status==Status::Unknown);
    std::puts("PASS: read-only pickup sector snapshot handles native bypasses, matching/mismatching revisions, unsigned gate, null/unknown/overflow data and signature changes; no game/headset.");return 0;
}catch(const std::exception& error){std::puts(error.what());return 1;}}
