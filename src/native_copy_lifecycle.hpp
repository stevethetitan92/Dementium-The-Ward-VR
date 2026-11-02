#pragma once
#include "native_copy_lease.hpp"
namespace nativeCopyLifecycle {
// Unused hook-dispatch helpers. The caller serializes recording and supplies
// original (unhooked) emitters/forwarders. HRESULTs and exceptions propagate.
template<class Barrier,class Copy,class Forward> HRESULT close(nativeCopyLease::Pinned& transaction,Barrier emitBarrier,Copy emitCopy,Forward forward){
    auto lifetime=transaction.retain();
    transaction.flush(emitBarrier,emitCopy);
    return forward(); // Keep the lease until native Close has returned.
}
template<class Forward> HRESULT reset(nativeCopyLease::Pinned& transaction,Forward forward){
    auto lifetime=transaction.retain();
    const HRESULT result=forward();
    transaction.afterReset(SUCCEEDED(result));
    return result; // Failed Reset preserves the held recording.
}
template<class Barrier,class Copy,class Forward> void intervening(nativeCopyLease::Pinned& transaction,Barrier emitBarrier,Copy emitCopy,Forward forward){
    auto lifetime=transaction.retain();
    transaction.flush(emitBarrier,emitCopy);
    forward(); // Preserve prefix ordering and ownership through the native call.
}
}
