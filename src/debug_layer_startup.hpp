#pragma once
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <MinHook.h>
// Experimental, fresh-process-only policy. Bootstrap and native game devices
// must use the same COM wrapper domain. Never toggle this on a running device.
namespace debugLayerStartup {
inline void* enableTarget{};
inline void STDMETHODCALLTYPE skipEnable(ID3D12Debug*) {}
inline bool suppressEnable(ID3D12Debug* debug) {
    if(!debug || enableTarget) return false;
    auto target=(*reinterpret_cast<void***>(debug))[3];
    if(MH_CreateHook(target,reinterpret_cast<void*>(&skipEnable),nullptr)!=MH_OK) return false;
    if(MH_EnableHook(target)!=MH_OK){MH_RemoveHook(target);return false;}
    enableTarget=target;
    return true;
}
}
