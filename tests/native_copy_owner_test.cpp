#define NOMINMAX
#include "../src/native_copy_owner.hpp"
#include <dxgi1_4.h>
#include <map>
#include <cstdio>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
static void require(bool value){if(!value)throw std::runtime_error("copy installation owner check failed");}
struct State {std::map<void*,bool> hooks;int failCreate=-1,failEnable=-1;unsigned creates{},enables{};void* failDisable{};void* failRemove{};};
struct Backend {
    State* state;
    bool create(const nativeCopyInstall::Binding& b){if(int(state->creates++)==state->failCreate||state->hooks.count(b.target))return false;state->hooks[b.target]=false;*b.original=b.target;return true;}
    bool enable(void* target){if(int(state->enables++)==state->failEnable)return false;state->hooks.at(target)=true;return true;}
    bool disable(void* target)noexcept{if(target==state->failDisable)return false;state->hooks[target]=false;return true;}
    bool remove(void* target)noexcept{if(target==state->failRemove)return false;state->hooks.erase(target);return true;}
};
int main(){try{
    ComPtr<IDXGIFactory4> factory;require(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))));ComPtr<IDXGIAdapter> warp;require(SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp))));ComPtr<ID3D12Device> device;require(SUCCEEDED(D3D12CreateDevice(warp.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))));ComPtr<ID3D12CommandAllocator> allocator;require(SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY,IID_PPV_ARGS(&allocator))));ComPtr<ID3D12GraphicsCommandList> list;require(SUCCEEDED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_COPY,allocator.Get(),nullptr,IID_PPV_ARGS(&list))));
    State state;std::array<void*,51> originals{};unsigned builds{};
    auto builder=[&](ID3D12GraphicsCommandList* incoming){require(incoming==list.Get());++builds;auto adapters=nativeCopyForwarders::entries();for(size_t i=0;i<adapters.size();++i)*adapters[i].original=reinterpret_cast<void*>(std::uintptr_t(0x2000+i));std::array<nativeCopyInstall::Binding,51> bindings;for(size_t i=0;i<51;++i)bindings[i]={reinterpret_cast<void*>(std::uintptr_t(0x1000+i)),reinterpret_cast<void*>(std::uintptr_t(0x3000+i)),&originals[i]};return bindings;};
    nativeCopyOwner::Installation<Backend> owner{Backend{&state}};
    require(!owner.arm(100,2));
    ComPtr<ID3D12Device> foreign;
    for(UINT index=0;index<16&&!foreign;++index){ComPtr<IDXGIAdapter1> adapter;if(factory->EnumAdapters1(index,&adapter)==DXGI_ERROR_NOT_FOUND)break;DXGI_ADAPTER_DESC1 description{};require(SUCCEEDED(adapter->GetDesc1(&description)));if(!(description.Flags&DXGI_ADAPTER_FLAG_SOFTWARE))D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&foreign));}
    if(foreign){require(!owner.install(foreign.Get(),list.Get(),0x10000000,builder,true)&&!builds&&state.hooks.empty()&&!nativeCopyEndpoints::runtime);std::puts("PASS: actual distinct-adapter device/list mismatch rejected before builder or backend mutation; no hardware submission.");}else std::puts("SKIP: no distinct hardware device available for owner identity mismatch test.");
    require(!owner.install(device.Get(),list.Get(),0x10000000,builder,false)&&!builds);
    require(owner.install(device.Get(),list.Get(),0x10000000,builder,true)&&owner.installed()&&state.hooks.size()==51&&builds==1);
    require(nativeCopyEndpoints::runtime&&nativeCopyEndpoints::contextFactory&&nativeCopyForwarders::dispatch==nativeCopyEndpoints::intervening);
    require(!owner.arm(0,2)&&!owner.arm(100,0)&&owner.arm(100,2));owner.cancel();
    require(!nativeCopyEndpoints::contextFactory({nativeCopyEndpoints::Kind::Copy,1,list.Get(),0}).read);
    nativeCopyOwner::Installation<Backend> nested{Backend{&state}};require(!nested.install(device.Get(),list.Get(),0x10000000,builder,true)&&builds==1);
    require(!owner.stop(false)&&owner.installed());require(owner.stop(true)&&!owner.installed()&&!owner.pendingCleanup()&&state.hooks.empty());
    for(auto value:originals)require(!value);for(const auto& entry:nativeCopyForwarders::entries())require(!*entry.original);require(!nativeCopyEndpoints::runtime&&!nativeCopyEndpoints::contextFactory&&!nativeCopyForwarders::dispatch);
    require(!owner.install(device.Get(),list.Get(),0x10000000,builder,true)&&builds==1);
    for(unsigned mode=0;mode<4;++mode){State failed;auto unrelated=reinterpret_cast<void*>(std::uintptr_t(0x77));failed.hooks[unrelated]=true;if(mode<2)failed.failCreate=50;else failed.failEnable=50;auto target=reinterpret_cast<void*>(std::uintptr_t(0x1007));if(mode&1)failed.failRemove=target;else failed.failDisable=target;nativeCopyOwner::Installation<Backend> recovering{Backend{&failed}};
        require(!recovering.install(device.Get(),list.Get(),0x10000000,builder,true)&&recovering.pendingCleanup()&&failed.hooks.size()==2&&originals[7]);
        require(nativeCopyEndpoints::runtime&&nativeCopyEndpoints::contextFactory&&nativeCopyForwarders::dispatch);require(!recovering.stop(false)&&!recovering.stop(true)&&recovering.pendingCleanup());failed.failDisable=failed.failRemove=nullptr;
        require(recovering.stop(true)&&!recovering.pendingCleanup()&&failed.hooks.size()==1&&failed.hooks.at(unrelated));for(auto value:originals)require(!value);for(const auto& entry:nativeCopyForwarders::entries())require(!*entry.original);require(!nativeCopyEndpoints::runtime&&!nativeCopyEndpoints::contextFactory&&!nativeCopyForwarders::dispatch);
    }
    State thrown;nativeCopyOwner::Installation<Backend> throwing{Backend{&thrown}};auto throws=[&](ID3D12GraphicsCommandList* incoming)->std::array<nativeCopyInstall::Binding,51>{builder(incoming);throw std::runtime_error("builder failure");};require(!throwing.install(device.Get(),list.Get(),0x10000000,throws,true)&&throwing.lastFailure()&&!throwing.pendingCleanup()&&thrown.hooks.empty());for(const auto& entry:nativeCopyForwarders::entries())require(!*entry.original);require(!nativeCopyEndpoints::runtime&&!nativeCopyEndpoints::contextFactory&&!nativeCopyForwarders::dispatch);
    require(SUCCEEDED(list->Close()));
    std::puts("PASS: unused installation owner binds/restores51 simulated hooks and exact-observation context, rejects nonquiescent/nested/reuse attempts; actual WARP device identity, no actual hooks/GPU submission/game/XR.");return 0;
}catch(const std::exception& e){std::puts(e.what());return 1;}}
