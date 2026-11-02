#pragma once
#include "native_copy_endpoints.hpp"
#include "native_copy_forwarders.hpp"
namespace nativeCopyConfiguration {
// Unused configuration snapshot for candidate installation. Batch separately
// owns native trampoline receivers. Capture BEFORE building the bridge table.
// No automatic destructor restoration: adapters must remain configured while
// any detour survives. Caller must remove hooks and establish quiescence first.
class Snapshot {
    std::array<void*,47> adapters{};
    nativeCopyForwarders::Dispatch dispatch=nativeCopyForwarders::dispatch;
    nativeCopyRuntime::Runtime* runtime=nativeCopyEndpoints::runtime;
    decltype(nativeCopyEndpoints::contextFactory) factory=nativeCopyEndpoints::contextFactory;
    nativeCopyEndpoints::Close close=nativeCopyEndpoints::originalClose;
    nativeCopyEndpoints::Reset reset=nativeCopyEndpoints::originalReset;
    nativeCopyEndpoints::Barrier barrier=nativeCopyEndpoints::originalBarrier;
    nativeCopyEndpoints::Copy copy=nativeCopyEndpoints::originalCopy;
    bool restored{};
public:
    Snapshot(){auto entries=nativeCopyForwarders::entries();for(size_t i=0;i<entries.size();++i)adapters[i]=*entries[i].original;}
    Snapshot(const Snapshot&)=delete;Snapshot& operator=(const Snapshot&)=delete;
    // These flags are caller assertions, NOT synchronization or hook discovery.
    // Quiescence is required even to inspect pendingRecordings safely.
    bool restore(bool hooksRemoved,bool recordingQuiescent){
        if(restored)return true;
        if(!hooksRemoved||!recordingQuiescent)return false;
        if(nativeCopyEndpoints::runtime&&nativeCopyEndpoints::runtime->pendingRecordings())return false;
        auto entries=nativeCopyForwarders::entries();for(size_t i=0;i<entries.size();++i)*entries[i].original=adapters[i];
        nativeCopyForwarders::dispatch=dispatch;nativeCopyEndpoints::runtime=runtime;nativeCopyEndpoints::contextFactory=factory;
        nativeCopyEndpoints::originalClose=close;nativeCopyEndpoints::originalReset=reset;nativeCopyEndpoints::originalBarrier=barrier;nativeCopyEndpoints::originalCopy=copy;restored=true;return true;
    }
};
// Unused shutdown coordinator. Caller stops ALL recording/callback activity
// before entering and keeps it stopped through success. False preserves live
// configuration; pending work must finish/replay before retry. Backend cleanup
// failure also leaves the snapshot intact for a later retry. MinHook lifetime
// and GPU fence retirement remain external responsibilities.
template<class Batch> bool release(Batch& batch,Snapshot& snapshot,bool recordingQuiescent){
    if(!recordingQuiescent)return false;
    if(auto current=nativeCopyEndpoints::runtime){current->cancel();if(current->pendingRecordings())return false;}
    if(!batch.rollback())return false;
    return snapshot.restore(true,true);
}
}
