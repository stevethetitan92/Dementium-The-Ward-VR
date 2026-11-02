#pragma once
#include "native_copy_runtime.hpp"
#include <intrin.h>
#include <exception>
namespace nativeCopyEndpoints {
using Close=HRESULT(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*);
using Reset=HRESULT(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,ID3D12CommandAllocator*,ID3D12PipelineState*);
using Barrier=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,UINT,const D3D12_RESOURCE_BARRIER*);
using Copy=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,const D3D12_TEXTURE_COPY_LOCATION*,UINT,UINT,UINT,const D3D12_TEXTURE_COPY_LOCATION*,const D3D12_BOX*);
enum class Kind {Barrier,Copy};
struct Observation {Kind kind;std::uintptr_t caller;ID3D12GraphicsCommandList* list;std::uintptr_t frame;};
struct Context {
    std::uintptr_t image{},caller{},frame{};ID3D12Device* device{};std::uint64_t now{};
    bool(*read)(std::uintptr_t,void*,size_t,void*){};void* readerContext{};
};
// Unused concrete ABI endpoints. Configure original functions/runtime/factory
// while recording is quiescent, before enabling detours. Null runtime forwards
// directly. Context factory must preserve actual native evidence in production;
// synthetic caller/frame substitution is only for isolated fixtures.
inline nativeCopyRuntime::Runtime* runtime{};
inline Context(*contextFactory)(const Observation&){};
inline Close originalClose{};inline Reset originalReset{};inline Barrier originalBarrier{};inline Copy originalCopy{};
// Typed intervening adapters share this synchronous dispatcher. Configure it
// only while quiescent; a held prefix must flush before the original method.
inline void intervening(ID3D12GraphicsCommandList* list,void(*invoke)(void*),void* context){
    if(!invoke)std::terminate();
    if(!runtime){invoke(context);return;}
    if(!originalBarrier||!originalCopy)std::terminate();
    runtime->intervening(list,[&](UINT count,const auto* values){originalBarrier(list,count,values);},
        [&](const auto* dst,UINT x,UINT y,UINT z,const auto* src,const auto* box){originalCopy(list,dst,x,y,z,src,box);},
        [&]{invoke(context);});
}
inline __declspec(noinline) HRESULT STDMETHODCALLTYPE close(ID3D12GraphicsCommandList* list){
    if(!originalClose)std::terminate();if(!runtime)return originalClose(list);
    return runtime->close(list,[&](UINT count,const auto* values){originalBarrier(list,count,values);},[&](const auto* dst,UINT x,UINT y,UINT z,const auto* src,const auto* box){originalCopy(list,dst,x,y,z,src,box);},[&]{return originalClose(list);});
}
inline __declspec(noinline) HRESULT STDMETHODCALLTYPE reset(ID3D12GraphicsCommandList* list,ID3D12CommandAllocator* allocator,ID3D12PipelineState* pipeline){
    if(!originalReset)std::terminate();if(!runtime)return originalReset(list,allocator,pipeline);return runtime->reset(list,[&]{return originalReset(list,allocator,pipeline);});
}
inline __declspec(noinline) void STDMETHODCALLTYPE barrier(ID3D12GraphicsCommandList* list,UINT count,const D3D12_RESOURCE_BARRIER* values){
    if(!originalBarrier)std::terminate();if(!runtime){originalBarrier(list,count,values);return;}
    Observation observation{Kind::Barrier,reinterpret_cast<std::uintptr_t>(_ReturnAddress()),list,reinterpret_cast<std::uintptr_t>(values)};
    auto context=contextFactory?contextFactory(observation):Context{};
    auto reader=[&](std::uintptr_t at,void* out,size_t bytes){return context.read&&context.read(at,out,bytes,context.readerContext);};
    runtime->barrier(context.image,context.caller,context.device,list,context.now,count,values,context.frame,reader,[&](UINT n,const auto* batch){originalBarrier(list,n,batch);},[&](const auto* dst,UINT x,UINT y,UINT z,const auto* src,const auto* box){originalCopy(list,dst,x,y,z,src,box);});
}
inline __declspec(noinline) void STDMETHODCALLTYPE copy(ID3D12GraphicsCommandList* list,const D3D12_TEXTURE_COPY_LOCATION* destination,UINT x,UINT y,UINT z,const D3D12_TEXTURE_COPY_LOCATION* source,const D3D12_BOX* box){
    if(!originalCopy)std::terminate();if(!runtime){originalCopy(list,destination,x,y,z,source,box);return;}
    Observation observation{Kind::Copy,reinterpret_cast<std::uintptr_t>(_ReturnAddress()),list,0};auto context=contextFactory?contextFactory(observation):Context{};
    runtime->copy(context.image,context.caller,list,context.now,destination,x,y,z,source,box,[&](UINT n,const auto* batch){originalBarrier(list,n,batch);},[&](const auto* dst,UINT a,UINT b,UINT c,const auto* src,const auto* region){originalCopy(list,dst,a,b,c,src,region);});
}
}
