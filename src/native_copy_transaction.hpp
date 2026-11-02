#pragma once
#include "native_copy_sequence.hpp"
namespace nativeCopyTransaction {
// Unused/test-only transaction core. Hold a proven Begin AND its Copy until
// End; then emit only the copy on a complete match, or replay the original
// buffered prefix before forwarding an unmatched command. No rollback after
// an already-omitted GPU command is required. Integration must retain owning
// resource references and flush before ANY intervening command or Close.
class Buffer {
    bool active{},copied{};nativeCopySequence::Matcher matcher;
    D3D12_RESOURCE_BARRIER beginBarrier{};
    D3D12_TEXTURE_COPY_LOCATION source{},destination{};D3D12_BOX box{};
public:
    bool pending() const{return active;}
    // Call only AFTER successful native Reset (which discards its recording).
    void discard(){active=copied=false;matcher.reset();beginBarrier={};source={};destination={};box={};}
    void afterReset(bool succeeded){if(succeeded)discard();}
    template<class Barrier,class Copy> void flush(Barrier emitBarrier,Copy emitCopy){
        if(!active)return;
        const auto barrier=beginBarrier;const auto src=source,dst=destination;const auto region=box;const bool hasCopy=copied;
        // Clear BEFORE calling native emitters: callbacks/throws cannot replay
        // the same prefix twice. The integration owns refs through emission.
        discard();emitBarrier(barrier);if(hasCopy)emitCopy(dst,src,region);
    }
    template<class Barrier,class Copy> bool begin(const nativeCopyPreflight::Plan& plan,const void* list,std::uint64_t generation,std::uint64_t now,std::uint64_t armUntil,const D3D12_RESOURCE_BARRIER& barrier,Barrier emitBarrier,Copy emitCopy){
        if(active){flush(emitBarrier,emitCopy);return false;}
        if(now>=armUntil||!plan.valid)return false;
        const bool upload=plan.site==nativeCopyBarrierPolicy::Site::UploadBegin;
        if(barrier.Type!=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION||barrier.Flags!=D3D12_RESOURCE_BARRIER_FLAG_NONE||
           barrier.Transition.pResource!=(upload?plan.destination.pResource:plan.source.pResource)||!barrier.Transition.pResource||
           barrier.Transition.Subresource!=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES||barrier.Transition.StateBefore!=D3D12_RESOURCE_STATE_COMMON||
           barrier.Transition.StateAfter!=(upload?D3D12_RESOURCE_STATE_COPY_DEST:D3D12_RESOURCE_STATE_COPY_SOURCE))return false;
        // Arming controls new transactions. An already admitted transaction
        // finishes after expiry/cancel, rather than changing half its sequence.
        if(!matcher.begin(plan,list,generation,now,(std::numeric_limits<std::uint64_t>::max)()))return false;
        beginBarrier=barrier;active=true;return true;
    }
    template<class Barrier,class Copy> bool copy(const void* list,std::uint64_t generation,std::uint64_t caller,std::uint64_t now,const D3D12_TEXTURE_COPY_LOCATION* dst,UINT x,UINT y,UINT z,const D3D12_TEXTURE_COPY_LOCATION* src,const D3D12_BOX* region,Barrier emitBarrier,Copy emitCopy){
        if(!active)return false;
        if(!matcher.copy(list,generation,caller,now,dst,x,y,z,src,region)){flush(emitBarrier,emitCopy);return false;}
        source=*src;destination=*dst;box=*region;copied=true;return true;
    }
    template<class Barrier,class Copy> bool end(const void* list,std::uint64_t generation,std::uint64_t caller,std::uint64_t now,UINT count,const D3D12_RESOURCE_BARRIER* barrier,Barrier emitBarrier,Copy emitCopy){
        if(!active)return false;
        if(!matcher.end(list,generation,caller,now,count,barrier)){flush(emitBarrier,emitCopy);return false;}
        const auto src=source,dst=destination;const auto region=box;discard();emitCopy(dst,src,region);return true;
    }
};
}
