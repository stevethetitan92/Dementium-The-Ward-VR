#define NOMINMAX
#include "../src/native_copy_forwarders.hpp"
#include "../src/native_copy_session.hpp"
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <tuple>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <vector>
static unsigned forwarded{},dispatched{};
static ID3D12GraphicsCommandList* expectedList=reinterpret_cast<ID3D12GraphicsCommandList*>(std::uintptr_t(0x77));
static auto identity(){return expectedList;}
static void(*nativeCheck)();static nativeCopySession::Session* activeSession;static std::vector<unsigned> prefix;
template<class T> T value(size_t index){
    if constexpr(std::is_pointer_v<T>)return reinterpret_cast<T>(std::uintptr_t(0x1000+index*16));
    else if constexpr(std::is_arithmetic_v<T>||std::is_enum_v<T>)return static_cast<T>(index+11);
    else {static_assert(std::is_same_v<T,D3D12_CPU_DESCRIPTOR_HANDLE>||std::is_same_v<T,D3D12_GPU_DESCRIPTOR_HANDLE>);return T{index+901};}
}
template<class T> bool same(T a,T b){if constexpr(std::is_same_v<T,D3D12_CPU_DESCRIPTOR_HANDLE>||std::is_same_v<T,D3D12_GPU_DESCRIPTOR_HANDLE>)return a.ptr==b.ptr;else return a==b;}
template<auto Method,class Signature=decltype(Method)>struct Verify;
template<auto Method,class R,class C,class...A>struct Verify<Method,R(C::*)(A...)>{
    template<size_t...I>static void compare(const std::tuple<A...>& args,std::index_sequence<I...>){assert((same(std::get<I>(args),value<A>(I))&&...));}
    static void STDMETHODCALLTYPE native(ID3D12GraphicsCommandList* list,A...args){assert(list==identity());compare(std::tuple<A...>(args...),std::index_sequence_for<A...>{});if(nativeCheck)nativeCheck();++forwarded;}
    template<size_t...I>static void call(std::index_sequence<I...>){nativeCopyForwarders::Adapter<Method>::original=&native;nativeCopyForwarders::Adapter<Method>::hook(identity(),value<A>(I)...);}
    static void run(){call(std::index_sequence_for<A...>{});}
};
int main(){
    auto entries=nativeCopyForwarders::entries();assert(entries.size()==47);unsigned previous{};for(const auto& entry:entries){assert(entry.slot>previous&&entry.slot!=9&&entry.slot!=10&&entry.slot!=16&&entry.slot!=26&&entry.hook&&entry.original);previous=entry.slot;}
    for(unsigned pass=0;pass<2;++pass){nativeCopyForwarders::dispatch=pass?+[](ID3D12GraphicsCommandList* list,void(*invoke)(void*),void* context){assert(list==identity()&&invoke&&context);++dispatched;invoke(context);}:nullptr;
#define NATIVE_COPY_VERIFY(slot,name) Verify<&ID3D12GraphicsCommandList::name>::run();
        NATIVE_COPY_INTERVENING_METHODS(NATIVE_COPY_VERIFY)
#undef NATIVE_COPY_VERIFY
    }
    assert(forwarded==94&&dispatched==47);nativeCopyForwarders::dispatch=nullptr;
    using Microsoft::WRL::ComPtr;ComPtr<IDXGIFactory4> factory;assert(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))));ComPtr<IDXGIAdapter> warp;assert(SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp))));ComPtr<ID3D12Device> device;assert(SUCCEEDED(D3D12CreateDevice(warp.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))));
    auto resource=[&](bool image){D3D12_RESOURCE_DESC desc{};desc.Dimension=image?D3D12_RESOURCE_DIMENSION_TEXTURE2D:D3D12_RESOURCE_DIMENSION_BUFFER;desc.Width=image?8:2048;desc.Height=image?8:1;desc.DepthOrArraySize=desc.MipLevels=1;desc.Format=image?DXGI_FORMAT_R8G8B8A8_UNORM:DXGI_FORMAT_UNKNOWN;desc.SampleDesc.Count=1;desc.Layout=image?D3D12_TEXTURE_LAYOUT_UNKNOWN:D3D12_TEXTURE_LAYOUT_ROW_MAJOR;D3D12_HEAP_PROPERTIES heap{};heap.Type=image?D3D12_HEAP_TYPE_DEFAULT:D3D12_HEAP_TYPE_UPLOAD;ComPtr<ID3D12Resource> result;assert(SUCCEEDED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,image?D3D12_RESOURCE_STATE_COMMON:D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&result))));return result;};
    auto source=resource(false),destination=resource(true);ComPtr<ID3D12CommandAllocator> allocator;assert(SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY,IID_PPV_ARGS(&allocator))));ComPtr<ID3D12GraphicsCommandList> list;assert(SUCCEEDED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_COPY,allocator.Get(),nullptr,IID_PPV_ARGS(&list))));expectedList=list.Get();
    nativeCopyPreflight::Plan plan;plan.valid=true;plan.site=nativeCopyBarrierPolicy::Site::UploadBegin;plan.copyReturnRva=0x1eac36;plan.endReturnRva=0x1eac72;plan.source.pResource=source.Get();plan.source.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;plan.source.PlacedFootprint.Footprint={DXGI_FORMAT_R8G8B8A8_UNORM,8,8,1,256};plan.destination.pResource=destination.Get();plan.destination.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;plan.box={0,0,0,8,8,1};D3D12_RESOURCE_BARRIER begin{};begin.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;begin.Transition={destination.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_DEST};
    nativeCopyEpoch::Source epochs;nativeCopySession::Session session;assert(session.created(epochs,list.Get()));activeSession=&session;
    auto emitBarrier=[](const D3D12_RESOURCE_BARRIER&){assert(!activeSession->pending());prefix.push_back(1);};auto emitCopy=[](const auto&,const auto&,const auto&){assert(!activeSession->pending());prefix.push_back(2);};
    nativeCheck=+[]{assert(!activeSession->pending()&&prefix==std::vector<unsigned>({1,2}));prefix.push_back(3);};
    nativeCopyForwarders::dispatch=+[](ID3D12GraphicsCommandList* incoming,void(*invoke)(void*),void* context){assert(incoming==identity());++dispatched;activeSession->intervening([](const D3D12_RESOURCE_BARRIER&){prefix.push_back(1);},[](const auto&,const auto&,const auto&){prefix.push_back(2);},[&]{invoke(context);});};
#define NATIVE_COPY_ORDER(slot,name) prefix.clear();assert(session.begin(plan,list.Get(),10,100,begin,emitBarrier,emitCopy));assert(session.copy(plan.copyReturnRva,20,&plan.destination,0,0,0,&plan.source,&plan.box,emitBarrier,emitCopy));Verify<&ID3D12GraphicsCommandList::name>::run();assert(prefix==std::vector<unsigned>({1,2,3})&&!session.pending());
    NATIVE_COPY_INTERVENING_METHODS(NATIVE_COPY_ORDER)
#undef NATIVE_COPY_ORDER
    assert(forwarded==141&&dispatched==94);nativeCopyForwarders::dispatch=nullptr;nativeCheck=nullptr;activeSession=nullptr;assert(SUCCEEDED(list->Close()));
    std::puts("PASS: all47 SDK-typed adapters also flush held Begin/Copy before the original method through an actual WARP-backed session; original arguments retained; metadata-only emitters, no GPU recording/submission.");
    std::puts("PASS: all47 SDK-typed intervening adapters preserve every scalar/pointer/descriptor argument and list identity, direct and synchronous-dispatch paths; 94 original calls, no COM queries/hook installation/GPU/game/XR.");return 0;
}
