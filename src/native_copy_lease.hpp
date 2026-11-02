#pragma once
#include "native_copy_transaction.hpp"
#include <wrl/client.h>
namespace nativeCopyLease {
struct Lease {
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list;
    Microsoft::WRL::ComPtr<ID3D12Resource> source,destination;
    void swap(Lease& other) noexcept{list.Swap(other.list);source.Swap(other.source);destination.Swap(other.destination);}
    // Detach all member references before any Release can invoke callbacks.
    void clear(){Lease previous;swap(previous);}
};
// Unused runtime-integration building block. Native callers must serialize this
// object's recording. CPU leases are NOT GPU retirement/fence ownership.
class Pinned {
    nativeCopyTransaction::Buffer buffer;Lease pins;bool busy{};
    struct Operation {
        Pinned& owner;Lease hold;
        explicit Operation(Pinned& value):owner(value){owner.busy=true;hold=owner.pins;}
        ~Operation(){if(!owner.buffer.pending())owner.pins.clear();hold.clear();owner.busy=false;}
    };
public:
    Pinned()=default;Pinned(const Pinned&)=delete;Pinned& operator=(const Pinned&)=delete;
    // Destruction discards held CPU work. Integration must first establish a
    // discarded recording/process teardown, or flush through native emitters.
    ~Pinned(){busy=true;buffer.discard();pins.clear();}
    bool pending() const{return buffer.pending();}
    // An actual hook must retain this lease UNTIL AFTER forwarding a triggering
    // unmatched command/Close/Reset, not merely until flush() returns.
    Lease retain() const{return pins;}
    template<class Barrier,class Copy> bool begin(const nativeCopyPreflight::Plan& plan,ID3D12GraphicsCommandList* list,std::uint64_t generation,std::uint64_t now,std::uint64_t until,const D3D12_RESOURCE_BARRIER& barrier,Barrier emitBarrier,Copy emitCopy){
        if(busy)return false;Operation operation(*this);
        if(!buffer.begin(plan,list,generation,now,until,barrier,emitBarrier,emitCopy))return false;
        Lease admitted;admitted.list=list;admitted.source=plan.source.pResource;admitted.destination=plan.destination.pResource;pins.swap(admitted);return true;
    }
    template<class Barrier,class Copy> bool copy(const void* list,std::uint64_t generation,std::uint64_t caller,std::uint64_t now,const D3D12_TEXTURE_COPY_LOCATION* dst,UINT x,UINT y,UINT z,const D3D12_TEXTURE_COPY_LOCATION* src,const D3D12_BOX* box,Barrier emitBarrier,Copy emitCopy){
        if(busy)return false;Operation operation(*this);return buffer.copy(list,generation,caller,now,dst,x,y,z,src,box,emitBarrier,emitCopy);
    }
    template<class Barrier,class Copy> bool end(const void* list,std::uint64_t generation,std::uint64_t caller,std::uint64_t now,UINT count,const D3D12_RESOURCE_BARRIER* barrier,Barrier emitBarrier,Copy emitCopy){
        if(busy)return false;Operation operation(*this);return buffer.end(list,generation,caller,now,count,barrier,emitBarrier,emitCopy);
    }
    template<class Barrier,class Copy> void flush(Barrier emitBarrier,Copy emitCopy){if(busy)return;Operation operation(*this);buffer.flush(emitBarrier,emitCopy);}
    void afterReset(bool succeeded){if(busy){buffer.afterReset(succeeded);return;}Operation operation(*this);buffer.afterReset(succeeded);}
};
}
