#pragma once
#include "native_copy_device.hpp"
#include "native_copy_preflight.hpp"
#include "native_copy_code.hpp"
namespace nativeCopyAdmission {
// Unused read-only integration prerequisite. Caller owns live COM references,
// verifies native_copy_code's complete helper pins, and supplies the observed
// native COPY-list identity. This does not record commands or establish states.
template<class Reader> nativeCopyPreflight::Plan capture(bool codeVerified,ID3D12Device* device,ID3D12GraphicsCommandList* nativeList,ID3D12GraphicsCommandList* list,ID3D12Resource* texture,ID3D12Resource* staging,std::uintptr_t caller,UINT count,const D3D12_RESOURCE_BARRIER* barriers,std::uintptr_t frame,Reader reader){
    if(!codeVerified||!nativeList||list!=nativeList||!texture||!staging||
       !nativeCopyDevice::matches(device,list,texture,staging))return {};
    const auto description=texture->GetDesc();
    nativeCopyBarrierPolicy::Proof proof{true,true,true,true};
    const auto site=nativeCopyBarrierPolicy::classify(proof,caller,description,count,barriers);
    if((site!=nativeCopyBarrierPolicy::Site::UploadBegin&&site!=nativeCopyBarrierPolicy::Site::ReadbackBegin)||barriers->Transition.pResource!=texture)return {};
    // Reject unsupported descriptions before asking the device for footprints.
    if(description.MipLevels!=1||description.DepthOrArraySize!=1||!description.Width||!description.Height||description.Format==DXGI_FORMAT_UNKNOWN)return {};
    D3D12_HEAP_PROPERTIES heap{};D3D12_HEAP_FLAGS flags{};
    if(FAILED(staging->GetHeapProperties(&heap,&flags)))return {};
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout{};UINT rows{};UINT64 rowBytes{},total{};
    device->GetCopyableFootprints(&description,0,1,0,&layout,&rows,&rowBytes,&total);
    if(!rows||!rowBytes||!total||total==(std::numeric_limits<UINT64>::max)())return {};
    nativeCopyPreflight::BufferProof buffer{true,staging,staging->GetDesc(),heap.Type,true,rowBytes};
    return nativeCopyPreflight::capture(proof,caller,description,count,barriers,frame,buffer,reader);
}
// Integration entry joins actual full-code verification to the ownership/frame
// checks; Reader must fail closed on unreadable native image/frame memory.
template<class Reader> nativeCopyPreflight::Plan capturePinned(std::uintptr_t image,ID3D12Device* device,ID3D12GraphicsCommandList* nativeList,ID3D12GraphicsCommandList* list,ID3D12Resource* texture,ID3D12Resource* staging,std::uintptr_t caller,UINT count,const D3D12_RESOURCE_BARRIER* barriers,std::uintptr_t frame,Reader reader){
    if(!nativeCopyCode::verify(image,reader))return {};
    return capture(true,device,nativeList,list,texture,staging,caller,count,barriers,frame,reader);
}
}
