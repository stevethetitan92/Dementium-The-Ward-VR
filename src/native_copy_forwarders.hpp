#pragma once
#include <d3d12.h>
#include <array>
#include <exception>
#include <type_traits>
#include "native_copy_methods.hpp"
namespace nativeCopyForwarders {
// Unused typed hook adapters. Callback must invoke synchronously exactly once,
// preserving session leases through the original call. Null callback passes
// directly through. No installation, state changes or default activation here.
using Dispatch=void(*)(ID3D12GraphicsCommandList*,void(*)(void*),void*);
inline Dispatch dispatch{};
template<auto Method,class Signature=decltype(Method)> struct Adapter;
template<auto Method,class R,class C,class... A> struct Adapter<Method,R(C::*)(A...)> {
    static_assert(std::is_void_v<R>,"Close/Reset require their explicit lifecycle paths");
    using Function=R(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,A...);
    inline static Function original{};
    static void STDMETHODCALLTYPE hook(ID3D12GraphicsCommandList* list,A... arguments){
        if(!original)std::terminate();
        auto forward=[&]{original(list,arguments...);};
        if(dispatch)dispatch(list,[](void* context){(*static_cast<decltype(forward)*>(context))();},&forward);else forward();
    }
};
struct Entry {unsigned slot;const char* name;void* hook;void** original;};
// Configure BEFORE enabling hooks. Existing hook body stays in the chain;
// MinHook writes its native trampoline into that body's original-function slot.
template<auto Method> void** chain(typename Adapter<Method>::Function existing,typename Adapter<Method>::Function& nativeOriginal){
    Adapter<Method>::original=existing;return reinterpret_cast<void**>(&nativeOriginal);
}
inline auto entries(){
    return std::array<Entry,47>{{
#define NATIVE_COPY_ENTRY(slot,name) {slot,#name,reinterpret_cast<void*>(&Adapter<&ID3D12GraphicsCommandList::name>::hook),reinterpret_cast<void**>(&Adapter<&ID3D12GraphicsCommandList::name>::original)},
        NATIVE_COPY_INTERVENING_METHODS(NATIVE_COPY_ENTRY)
#undef NATIVE_COPY_ENTRY
    }};
}
}
