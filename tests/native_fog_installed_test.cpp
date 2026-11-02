#define NOMINMAX
#include "../src/bridge.cpp"
#include "../src/native_copy_minhook.hpp"
#include "../src/native_copy_configuration.hpp"
#include "../src/native_copy_owner.hpp"
#define DEMENTIUM_DEBUG_ALREADY_ENABLED
#define main nativeFogFixtureMain
#include "native_fog_test.cpp"
#undef main
// Run the actual native PS fog/spotlight oracle through the existing bridge
// bodies and all51 candidate command detours. Owner Runtime stays unarmed.
int main(){try{
    ComPtr<ID3D12Debug> debug;stereo::hr(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();
    ComPtr<IDXGIFactory4> factory;stereo::hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));ComPtr<IDXGIAdapter> warp;stereo::hr(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));ComPtr<ID3D12Device> device;stereo::hr(D3D12CreateDevice(warp.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));
    ComPtr<ID3D12CommandAllocator> allocator;stereo::hr(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));ComPtr<ID3D12GraphicsCommandList> list;stereo::hr(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)));stereo::hr(list->Close());
    if(MH_Initialize()!=MH_OK)throw std::runtime_error("native PS hook initialization failed");
    {
        // Last-slot collision must roll back the owner's preceding50 hooks,
        // preserving an unrelated pre-existing enabled hook and its trampoline.
        auto target=(*reinterpret_cast<void***>(list.Get()))[59];std::array<unsigned char,32> before{},enabled{};std::memcpy(before.data(),target,32);void* unrelatedOriginal{};
        auto detour=+[](ID3D12GraphicsCommandList*,ID3D12CommandSignature*,UINT,ID3D12Resource*,UINT64,ID3D12Resource*,UINT64){};
        if(MH_CreateHook(target,reinterpret_cast<void*>(detour),&unrelatedOriginal)!=MH_OK||MH_EnableHook(target)!=MH_OK)throw std::runtime_error("unrelated collision setup failed");std::memcpy(enabled.data(),target,32);
        nativeCopyOwner::Installation<nativeCopyMinHook::Backend> collision{nativeCopyMinHook::Backend{}};
        if(collision.install(device.Get(),list.Get(),reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)),candidateCommandBindings,true)||collision.installed()||collision.pendingCleanup()||!unrelatedOriginal||std::memcmp(enabled.data(),target,32)||MH_EnableHook(target)!=MH_ERROR_ENABLED)throw std::runtime_error("owner collision rollback changed unrelated hook");
        if(nativeCopyEndpoints::runtime||nativeCopyEndpoints::contextFactory||nativeCopyForwarders::dispatch)throw std::runtime_error("owner collision left configuration active");for(const auto& entry:nativeCopyForwarders::entries())if(*entry.original)throw std::runtime_error("owner collision left adapter configuration");
        for(unsigned slot=9;slot<59;++slot)if(MH_EnableHook((*reinterpret_cast<void***>(list.Get()))[slot])!=MH_ERROR_NOT_CREATED)throw std::runtime_error("owner collision left owned target installed");
        if(MH_DisableHook(target)!=MH_OK||MH_RemoveHook(target)!=MH_OK||std::memcmp(before.data(),target,32))throw std::runtime_error("unrelated collision target restoration failed");
        std::puts("PASS: actual owner last-slot collision preserves unrelated hook and restores configuration after50 owned creations.");
    }
    std::array<nativeCopyInstall::Binding,51> bindings{};std::array<std::array<unsigned char,32>,51> bytes;
    nativeCopyOwner::Installation<nativeCopyMinHook::Backend> owner{nativeCopyMinHook::Backend{}};
    auto builder=[&](ID3D12GraphicsCommandList* incoming){bindings=candidateCommandBindings(incoming);for(size_t i=0;i<51;++i)std::memcpy(bytes[i].data(),bindings[i].target,32);return bindings;};
    if(!owner.install(device.Get(),list.Get(),reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)),builder,true))throw std::runtime_error("native PS command hooks failed");
    if(owner.stop(false)||!owner.installed())throw std::runtime_error("nonquiescent owner stop changed hooks");
    const auto before=draws.load();const int result=nativeFogFixtureMain();const auto observed=draws.load()-before;
    if(!owner.stop(true))std::terminate();bool restored=!owner.installed()&&!owner.pendingCleanup();for(size_t i=0;i<51;++i)restored&=!std::memcmp(bytes[i].data(),bindings[i].target,32);
    restored&=MH_Uninitialize()==MH_OK;
    if(result||!restored||!observed||internal||nativeCopyEndpoints::runtime)throw std::runtime_error("installed native PS oracle/restoration failed");
    std::printf("PASS: actual native PS fog/spotlight oracle through51 installed command hooks; legacy draws=%u; target bytes restored; no game/XR.\n",observed);return 0;
}catch(const std::exception& e){std::puts(e.what());return 1;}}
