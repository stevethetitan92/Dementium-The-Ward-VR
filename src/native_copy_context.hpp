#pragma once
#include "native_copy_code.hpp"
#include "native_copy_preflight.hpp"
namespace nativeCopyContext {
struct Context {bool valid{};std::uintptr_t callerRva{};ID3D12Resource* texture{};ID3D12Resource* staging{};};
// Unused read-only native identity extraction. Raw pointers are identities, NOT
// a COM lifetime proof. Before querying them, integration must establish valid
// native-call ownership and retain resources through buffering/forwarding.
template<class Reader> Context capture(std::uintptr_t image,std::uintptr_t caller,ID3D12GraphicsCommandList* list,UINT count,const D3D12_RESOURCE_BARRIER* barriers,std::uintptr_t frame,Reader reader){
    Context result;const auto maximum=(std::numeric_limits<std::uintptr_t>::max)();
    if(!list||!image||caller<image||count!=1||!barriers||barriers->Type!=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION||
       barriers->Flags!=D3D12_RESOURCE_BARRIER_FLAG_NONE||!barriers->Transition.pResource||barriers->Transition.Subresource!=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES||barriers->Transition.StateBefore!=D3D12_RESOURCE_STATE_COMMON)return result;
    result.callerRva=caller-image;const bool upload=result.callerRva==0x1eabf1;
    if((!upload&&result.callerRva!=0x1ea8eb)||barriers->Transition.StateAfter!=(upload?D3D12_RESOURCE_STATE_COPY_DEST:D3D12_RESOURCE_STATE_COPY_SOURCE)||!nativeCopyCode::verify(image,reader))return result;
    constexpr std::uintptr_t nativeListRva=0x6bb740;ID3D12GraphicsCommandList* observed{};
    if(nativeListRva>maximum-image||sizeof observed>maximum-(image+nativeListRva)||!reader(image+nativeListRva,&observed,sizeof observed)||observed!=list)return result;
    auto read=[&](std::uintptr_t offset,void* output,size_t bytes){return frame&&offset<=maximum-frame&&bytes<=maximum-(frame+offset)&&reader(frame+offset,output,bytes);};
    D3D12_TEXTURE_COPY_LOCATION source{},destination{};
    if(!read(upload?0x78:0xd0,&source,sizeof source)||!read(upload?0x48:0x20,&destination,sizeof destination))return result;
    const auto& texture=upload?destination:source;const auto& staging=upload?source:destination;
    if(texture.Type!=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX||texture.SubresourceIndex||texture.pResource!=barriers->Transition.pResource||staging.Type!=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT||!staging.pResource||staging.pResource==texture.pResource)return result;
    result.texture=texture.pResource;result.staging=staging.pResource;result.valid=true;return result;
}
}
