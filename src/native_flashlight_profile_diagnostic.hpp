#pragma once
#include <array>
#include <cstdint>
#include <limits>
#include <cmath>
namespace nativeFlashlightProfileDiagnostic {
struct Snapshot {
    std::uintptr_t textureObject{};
    std::uintptr_t resourceIdentity{};std::uint32_t nativeBindingId{};
    bool resourceKnown{},bindingIdKnown{};
    std::array<float,256> samples{};
    float minimum{},maximum{};
    bool finite{};
    int resolvedSlot{-1};bool slotKnown{},samplerKnown{};
    std::array<std::uint32_t,13> samplerWords{}; // Raw52-byte native sampler record.
};
// Read-only index-zero CPU profile. Not an atomic snapshot, GPU readback,
// resource lifetime proof, or evidence that a particular draw sampled it.
template<class Read> bool capture(std::uintptr_t image,Snapshot& output,Read read){
    output={};
    if(!image||image>(std::numeric_limits<std::uintptr_t>::max)()-0x3e8f60)return false;
    const unsigned char method[]={0xf3,0x0f,0x5c,0xe2}; // high-low at441B6
    const unsigned char field[]={0x41,0x89,0x51,0x20}; // DXGI format store
    std::array<unsigned char,4> bytes{};
    if(!read(image+0x441b6,bytes.data(),bytes.size())||bytes!=std::array<unsigned char,4>{method[0],method[1],method[2],method[3]})return false;
    if(!read(image+0x1e9fa4,bytes.data(),bytes.size())||bytes!=std::array<unsigned char,4>{field[0],field[1],field[2],field[3]})return false;
    std::uintptr_t vptr{},target{};
    if(!read(image+0x3a56e0,&vptr,sizeof vptr)||vptr!=image+0x305e80||
       !read(image+0x305ef8,&target,sizeof target)||target!=image+0x441a0)return false;
    Snapshot result;
    if(!read(image+0x3a5a88,&result.textureObject,sizeof result.textureObject)||
       !read(image+0x3a5ae8,result.samples.data(),sizeof result.samples))return false;
    result.finite=true;result.minimum=result.maximum=result.samples[0];
    for(float value:result.samples){if(!std::isfinite(value)){result.finite=false;break;}if(value<result.minimum)result.minimum=value;if(value>result.maximum)result.maximum=value;}
    // Non-finite samples are evidence too; never report their range as valid.
    if(!result.finite)result.minimum=result.maximum=0;
    result.slotKnown=read(image+0x3a5a6c,&result.resolvedSlot,sizeof result.resolvedSlot);
    if(!result.slotKnown)result.resolvedSlot=-1;
    if(result.slotKnown&&result.resolvedSlot>=0&&result.resolvedSlot<20)
        result.samplerKnown=read(image+0x3e8b50+std::size_t(result.resolvedSlot)*52,result.samplerWords.data(),sizeof result.samplerWords);
    if(!result.samplerKnown)result.samplerWords={};
    // Native1E9AC0 reads wrapper+8 as the D3D resource and+30 as its binding ID.
    // Retain pointer VALUE only; no COM call or resource dereference/lifetime claim.
    if(result.textureObject&&result.textureObject<=(std::numeric_limits<std::uintptr_t>::max)()-0x34){
        result.resourceKnown=read(result.textureObject+8,&result.resourceIdentity,sizeof result.resourceIdentity);
        result.bindingIdKnown=read(result.textureObject+0x30,&result.nativeBindingId,sizeof result.nativeBindingId);
        if(!result.resourceKnown)result.resourceIdentity=0;
        if(!result.bindingIdKnown)result.nativeBindingId=0;
    }
    output=result;return true;
}
}

