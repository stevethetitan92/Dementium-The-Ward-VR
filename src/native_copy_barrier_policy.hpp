#pragma once
#include <d3d12.h>
#include <cstdint>
namespace nativeCopyBarrierPolicy {
enum class Site {None,UploadBegin,UploadEnd,ReadbackBegin,ReadbackEnd};
struct Proof {bool signaturesVerified{},nativeCopyList{},copyQueue{},resourceDescriptionKnown{};};
// Classification only. This function NEVER records/omits/rewrites a barrier.
// A future candidate must additionally prove the complete native copy sequence,
// resource ownership and reset/expiry behavior before acting on this result.
inline Site classify(const Proof& proof,std::uintptr_t returnRva,const D3D12_RESOURCE_DESC& resource,UINT count,const D3D12_RESOURCE_BARRIER* barriers){
    if(!proof.signaturesVerified||!proof.nativeCopyList||!proof.copyQueue||!proof.resourceDescriptionKnown||
       resource.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||resource.Flags!=D3D12_RESOURCE_FLAG_NONE||resource.SampleDesc.Count!=1||
       count!=1||!barriers||barriers->Type!=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION||barriers->Flags!=D3D12_RESOURCE_BARRIER_FLAG_NONE)return Site::None;
    const auto& transition=barriers->Transition;
    if(!transition.pResource||transition.Subresource!=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)return Site::None;
    if(returnRva==0x1eabf1&&transition.StateBefore==D3D12_RESOURCE_STATE_COMMON&&transition.StateAfter==D3D12_RESOURCE_STATE_COPY_DEST)return Site::UploadBegin;
    if(returnRva==0x1eac72&&transition.StateBefore==D3D12_RESOURCE_STATE_COPY_DEST&&transition.StateAfter==D3D12_RESOURCE_STATE_COMMON)return Site::UploadEnd;
    if(returnRva==0x1ea8eb&&transition.StateBefore==D3D12_RESOURCE_STATE_COMMON&&transition.StateAfter==D3D12_RESOURCE_STATE_COPY_SOURCE)return Site::ReadbackBegin;
    if(returnRva==0x1ea95a&&transition.StateBefore==D3D12_RESOURCE_STATE_COPY_SOURCE&&transition.StateAfter==D3D12_RESOURCE_STATE_COMMON)return Site::ReadbackEnd;
    return Site::None;
}
}
