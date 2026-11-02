#pragma once
#include "native_copy_registry.hpp"
#include "native_copy_control.hpp"
#include "native_copy_prepare.hpp"
#include "native_copy_dispatch.hpp"
namespace nativeCopyRuntime {
// Unused composed candidate. Caller supplies live native arguments, fail-closed
// Reader and original functions bound to the incoming list. No installation or
// implicit arming. Flush/discard all recordings before destruction.
class Runtime {
    nativeCopyRegistry::Registry<> registry;nativeCopyControl::Control control;
public:
    bool arm(std::uint64_t now,std::uint64_t duration,unsigned budget){return control.arm(now,duration,budget);}
    void cancel(){control.cancel();}
    bool created(ID3D12GraphicsCommandList* list){return registry.created(list);}
    // Identity was observed at COPY creation/successful Reset; not a live COM
    // validity, open-state or admission proof by itself.
    bool hasRecordingIdentity(ID3D12GraphicsCommandList* list){return registry.find(list)!=nullptr;}
    bool pending(ID3D12GraphicsCommandList* list){auto state=registry.find(list);return state&&state->pending();}
    // Quiescent shutdown inspection only; zero is not GPU fence retirement.
    size_t pendingRecordings(){return registry.pendingRecordings();}
    template<class Forward> HRESULT reset(ID3D12GraphicsCommandList* list,Forward forward){return registry.reset(list,forward);}
    template<class Barrier,class Copy,class Forward> HRESULT close(ID3D12GraphicsCommandList* list,Barrier originalBarrier,Copy originalCopy,Forward forward){
        return registry.close(list,[&](const auto& value){originalBarrier(1,&value);},[&](const auto& dst,const auto& src,const auto& box){originalCopy(&dst,0,0,0,&src,&box);},forward);
    }
    template<class Barrier,class Copy,class Forward> void intervening(ID3D12GraphicsCommandList* list,Barrier originalBarrier,Copy originalCopy,Forward forward){
        registry.intervening(list,[&](const auto& value){originalBarrier(1,&value);},[&](const auto& dst,const auto& src,const auto& box){originalCopy(&dst,0,0,0,&src,&box);},forward);
    }
    template<class Reader,class Barrier,class Copy> void barrier(std::uintptr_t image,std::uintptr_t caller,ID3D12Device* device,ID3D12GraphicsCommandList* list,std::uint64_t now,UINT count,const D3D12_RESOURCE_BARRIER* barriers,std::uintptr_t frame,Reader reader,Barrier originalBarrier,Copy originalCopy){
        auto state=registry.find(list);if(!state){originalBarrier(count,barriers);return;}
        const auto rva=image&&caller>=image?caller-image:0;std::uint64_t deadline{};
        if(!state->pending()&&state->canBegin(list)&&(rva==0x1eabf1||rva==0x1ea8eb)&&count==1&&barriers)deadline=control.reserve(now);
        nativeCopyPrepare::Prepared prepared;
        auto admit=[&]{prepared=nativeCopyPrepare::capture(image,caller,device,list,count,barriers,frame,reader);return prepared.plan;};
        nativeCopyDispatch::barrier(*state,list,rva,now,deadline,count,barriers,admit,originalBarrier,originalCopy);
    }
    template<class Barrier,class Copy> void copy(std::uintptr_t image,std::uintptr_t caller,ID3D12GraphicsCommandList* list,std::uint64_t now,const D3D12_TEXTURE_COPY_LOCATION* destination,UINT x,UINT y,UINT z,const D3D12_TEXTURE_COPY_LOCATION* source,const D3D12_BOX* box,Barrier originalBarrier,Copy originalCopy){
        auto state=registry.find(list);if(!state){originalCopy(destination,x,y,z,source,box);return;}
        const auto rva=image&&caller>=image?caller-image:0;
        nativeCopyDispatch::copy(*state,list,rva,now,destination,x,y,z,source,box,originalBarrier,originalCopy);
    }
};
}
