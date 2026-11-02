#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <limits>
namespace nativePickupSector {
enum class Status {Unknown,NoQuery,NoCamera,NoEntitySector,CountBypass,NoCurrentSector,StampUnavailable,StampMatches,StampDiffers};
struct Snapshot {
    Status status=Status::Unknown;
    std::uintptr_t camera{},entitySector{},currentSector{},record{};
    std::uint32_t count{},limit{},revision{},relative{},stamp{};
};
inline const char* name(Status value){switch(value){
case Status::NoQuery:return "no_query";case Status::NoCamera:return "no_camera";
case Status::NoEntitySector:return "no_entity_sector";case Status::CountBypass:return "count_bypass";
case Status::NoCurrentSector:return "no_current_sector";case Status::StampUnavailable:return "stamp_unavailable";
case Status::StampMatches:return "stamp_matches";case Status::StampDiffers:return "stamp_differs";default:return "unknown";}}
// Read-only snapshot of the native 7C90A..7C94D sector gate. Capture on the
// owning update thread; this does not replace the engine's visibility decision.
template<class Reader> Snapshot capture(std::uintptr_t image,std::uintptr_t entity,std::uintptr_t query,Reader reader){
    Snapshot result;
    auto read=[&](std::uintptr_t base,std::uintptr_t offset,void* output,std::size_t bytes){
        const auto maximum=(std::numeric_limits<std::uintptr_t>::max)();
        return base&&offset<=maximum-base&&bytes<=maximum-(base+offset)&&reader(base+offset,output,bytes);
    };
    const unsigned char entityCode[]={0x48,0x8b,0x87,0x90,0,0,0};
    const unsigned char countCode[]={0x8b,0x83,0x24,0x01,0,0,0x39,0x83,0xb8,0,0,0};
    unsigned char check[sizeof countCode];
    if(!read(image,0x7c90a,check,sizeof entityCode)||std::memcmp(check,entityCode,sizeof entityCode)||
       !read(image,0x7c92c,check,sizeof countCode)||std::memcmp(check,countCode,sizeof countCode))return result;
    if(!query){result.status=Status::NoQuery;return result;}
    if(!read(query,0xc0,&result.camera,sizeof result.camera))return result;
    if(!result.camera){result.status=Status::NoCamera;return result;}
    if(!read(entity,0x90,&result.entitySector,sizeof result.entitySector))return result;
    if(!result.entitySector){result.status=Status::NoEntitySector;return result;}
    if(!read(query,0xb8,&result.count,sizeof result.count)||!read(query,0x124,&result.limit,sizeof result.limit))return result;
    if(result.count<=result.limit){result.status=Status::CountBypass;return result;}
    if(!read(query,0xb0,&result.currentSector,sizeof result.currentSector))return result;
    if(!result.currentSector){result.status=Status::NoCurrentSector;return result;}
    result.status=Status::StampUnavailable;
    if(!read(query,0xac,&result.revision,sizeof result.revision)||!read(result.entitySector,0x18,&result.relative,sizeof result.relative)||!result.relative)return result;
    std::uintptr_t manager{},base{};
    if(!read(image,0x3fc340,&manager,sizeof manager)||!read(manager,8,&base,sizeof base))return result;
    const auto maximum=(std::numeric_limits<std::uintptr_t>::max)();
    if(!base||result.relative>maximum-base)return result;
    result.record=base+result.relative;
    if(!read(result.record,8,&result.stamp,sizeof result.stamp))return result;
    result.status=result.stamp==result.revision?Status::StampMatches:Status::StampDiffers;
    return result;
}
}
