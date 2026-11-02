#pragma once
#include "native_copy_epoch.hpp"
#include "native_copy_lifecycle.hpp"
namespace nativeCopySession {
// Unused per-list dispatch state. Admission Plan still requires independent
// native-code/list/device/frame proof. Same-list calls must be serialized.
class Session {
    nativeCopyLease::Pinned transaction;
    nativeCopyEpoch::Recording recording;
    ID3D12GraphicsCommandList* owner{};bool open{},forwarding{};
    struct ForwardScope {bool& active;explicit ForwardScope(bool& value):active(value){active=true;}~ForwardScope(){active=false;}};
public:
    bool pending()const{return transaction.pending();}
    bool owns(ID3D12GraphicsCommandList* list)const{return owner&&owner==list;}
    bool canBegin(ID3D12GraphicsCommandList* list)const{return !forwarding&&open&&owns(list)&&recording.value();}
    std::uint64_t epoch()const{return recording.value();}
    bool created(nativeCopyEpoch::Source& source,ID3D12GraphicsCommandList* list){
        if(forwarding||transaction.pending())return false;owner=list;recording.created(source);open=list&&recording.value();return open;
    }
    template<class Barrier,class Copy> bool begin(const nativeCopyPreflight::Plan& plan,ID3D12GraphicsCommandList* list,std::uint64_t now,std::uint64_t until,const D3D12_RESOURCE_BARRIER& barrier,Barrier emitBarrier,Copy emitCopy){
        if(!canBegin(list))return false;
        return transaction.begin(plan,list,recording.value(),now,until,barrier,emitBarrier,emitCopy);
    }
    template<class Barrier,class Copy> bool copy(std::uint64_t caller,std::uint64_t now,const D3D12_TEXTURE_COPY_LOCATION* destination,UINT x,UINT y,UINT z,const D3D12_TEXTURE_COPY_LOCATION* source,const D3D12_BOX* box,Barrier emitBarrier,Copy emitCopy){
        if(forwarding)return false;return transaction.copy(owner,recording.value(),caller,now,destination,x,y,z,source,box,emitBarrier,emitCopy);
    }
    template<class Barrier,class Copy> bool end(std::uint64_t caller,std::uint64_t now,UINT count,const D3D12_RESOURCE_BARRIER* barrier,Barrier emitBarrier,Copy emitCopy){
        if(forwarding)return false;return transaction.end(owner,recording.value(),caller,now,count,barrier,emitBarrier,emitCopy);
    }
    template<class Barrier,class Copy,class Forward> HRESULT close(Barrier emitBarrier,Copy emitCopy,Forward forward){
        if(forwarding)return forward();ForwardScope scope(forwarding);
        const auto result=nativeCopyLifecycle::close(transaction,emitBarrier,emitCopy,forward);if(SUCCEEDED(result))open=false;return result;
    }
    template<class Forward> HRESULT reset(nativeCopyEpoch::Source& epochs,Forward forward){
        if(forwarding)return forward();ForwardScope scope(forwarding);
        const auto result=nativeCopyLifecycle::reset(transaction,forward);if(SUCCEEDED(result)){recording.reset(epochs,true);open=owner&&recording.value();}return result;
    }
    template<class Barrier,class Copy,class Forward> void intervening(Barrier emitBarrier,Copy emitCopy,Forward forward){if(forwarding){forward();return;}ForwardScope scope(forwarding);nativeCopyLifecycle::intervening(transaction,emitBarrier,emitCopy,forward);}
    nativeCopyLease::Lease retain()const{return transaction.retain();}
};
}
