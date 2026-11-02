#pragma once
#include "native_copy_context.hpp"
#include "native_copy_admission.hpp"
#include "native_copy_lease.hpp"
namespace nativeCopyPrepare {
struct Prepared {nativeCopyPreflight::Plan plan;nativeCopyLease::Lease owners;};
// Unused integration entry for a VALID native call. Context checks establish
// identities, not pointer validity: caller must guarantee live COM arguments
// just as the original D3D12 call requires. Pins then span queries/admission.
template<class Reader> Prepared capture(std::uintptr_t image,std::uintptr_t caller,ID3D12Device* device,ID3D12GraphicsCommandList* list,UINT count,const D3D12_RESOURCE_BARRIER* barriers,std::uintptr_t frame,Reader reader){
    auto context=nativeCopyContext::capture(image,caller,list,count,barriers,frame,reader);
    if(!context.valid)return {};
    Prepared result;result.owners.list=list;
    const bool upload=context.callerRva==0x1eabf1;
    result.owners.source=upload?context.staging:context.texture;result.owners.destination=upload?context.texture:context.staging;
    result.plan=nativeCopyAdmission::capture(true,device,list,list,context.texture,context.staging,context.callerRva,count,barriers,frame,reader);
    if(!result.plan.valid)return {};return result;
}
}
