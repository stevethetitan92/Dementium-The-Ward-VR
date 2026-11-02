#pragma once
#include "native_copy_barrier_policy.hpp"
#include <limits>
namespace nativeCopyPreflight {
// rowBytes must come from the device's GetCopyableFootprints result for the
// exact texture/subresource, not guessed format bytes-per-pixel arithmetic.
struct BufferProof {bool known{};ID3D12Resource* identity{};D3D12_RESOURCE_DESC description{};D3D12_HEAP_TYPE heap{};bool rowBytesKnown{};UINT64 rowBytes{};};
struct Plan {
    bool valid{};nativeCopyBarrierPolicy::Site site=nativeCopyBarrierPolicy::Site::None;
    D3D12_TEXTURE_COPY_LOCATION source{},destination{};D3D12_BOX box{};
    std::uintptr_t copyReturnRva{},endReturnRva{};
};
// Read-only preflight of already-built native stack locations. It is deliberately
// limited to full, single-mip, non-array 2D copies. No command is changed here.
// The caller must pin BOTH native prologues/layout instructions, not just a
// return address, before supplying signaturesVerified.
template<class Reader> Plan capture(const nativeCopyBarrierPolicy::Proof& proof,std::uintptr_t returnRva,const D3D12_RESOURCE_DESC& texture,UINT count,const D3D12_RESOURCE_BARRIER* barriers,std::uintptr_t nativeBarrierAddress,const BufferProof& buffer,Reader reader){
    Plan result;result.site=nativeCopyBarrierPolicy::classify(proof,returnRva,texture,count,barriers);
    const bool upload=result.site==nativeCopyBarrierPolicy::Site::UploadBegin;
    if(!upload&&result.site!=nativeCopyBarrierPolicy::Site::ReadbackBegin)return result;
    if(texture.MipLevels!=1||texture.DepthOrArraySize!=1||!texture.Width||!texture.Height||texture.Format==DXGI_FORMAT_UNKNOWN||
       !buffer.known||!buffer.rowBytesKnown||!buffer.rowBytes||!buffer.identity||buffer.description.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||
       buffer.heap!=(upload?D3D12_HEAP_TYPE_UPLOAD:D3D12_HEAP_TYPE_READBACK))return result;
    auto read=[&](std::uintptr_t offset,void* output,size_t bytes){const auto maximum=(std::numeric_limits<std::uintptr_t>::max)();return nativeBarrierAddress&&offset<=maximum-nativeBarrierAddress&&bytes<=maximum-(nativeBarrierAddress+offset)&&reader(nativeBarrierAddress+offset,output,bytes);};
    // Both audited prologues establish RBP=RSP+100h at the barrier call.
    if(!read(upload?0x78:0xd0,&result.source,sizeof result.source)||
       !read(upload?0x48:0x20,&result.destination,sizeof result.destination)||
       !read(upload?0x20:0x58,&result.box,sizeof result.box))return result;
    const auto& image=upload?result.destination:result.source;const auto& linear=upload?result.source:result.destination;
    if(image.Type!=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX||image.SubresourceIndex!=0||image.pResource!=barriers->Transition.pResource||
       linear.Type!=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT||linear.pResource!=buffer.identity||linear.pResource==image.pResource)return result;
    const auto& placed=linear.PlacedFootprint;const auto& footprint=placed.Footprint;
    if(footprint.Format!=texture.Format||footprint.Width!=texture.Width||footprint.Height!=texture.Height||footprint.Depth!=1||
       !footprint.RowPitch||buffer.rowBytes>footprint.RowPitch||footprint.RowPitch%D3D12_TEXTURE_DATA_PITCH_ALIGNMENT||placed.Offset%D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT||
       placed.Offset>buffer.description.Width||footprint.Height>(buffer.description.Width-placed.Offset)/footprint.RowPitch)return result;
    if(result.box.left||result.box.top||result.box.front||result.box.right!=texture.Width||result.box.bottom!=texture.Height||result.box.back!=1)return result;
    result.copyReturnRva=upload?0x1eac36:0x1ea920;result.endReturnRva=upload?0x1eac72:0x1ea95a;result.valid=true;return result;
}
}
