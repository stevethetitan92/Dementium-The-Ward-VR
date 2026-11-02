#define NOMINMAX
#include "../src/native_copy_configuration.hpp"
#include <cstdio>
#include <stdexcept>
static void require(bool value){if(!value)throw std::runtime_error("configuration restoration failed");}
int main(){try{
    nativeCopyConfiguration::Snapshot initial;
    auto entries=nativeCopyForwarders::entries();
    for(size_t i=0;i<entries.size();++i)*entries[i].original=reinterpret_cast<void*>(std::uintptr_t(0x1000+i));
    nativeCopyRuntime::Runtime first,second;
    nativeCopyEndpoints::runtime=&first;
    nativeCopyEndpoints::contextFactory=+[](const nativeCopyEndpoints::Observation&)->nativeCopyEndpoints::Context{return {};};
    nativeCopyForwarders::dispatch=+[](ID3D12GraphicsCommandList*,void(*)(void*),void*){};
    nativeCopyEndpoints::originalClose=+[](ID3D12GraphicsCommandList*)->HRESULT{return S_OK;};
    nativeCopyEndpoints::originalReset=+[](ID3D12GraphicsCommandList*,ID3D12CommandAllocator*,ID3D12PipelineState*)->HRESULT{return S_OK;};
    nativeCopyEndpoints::originalBarrier=+[](ID3D12GraphicsCommandList*,UINT,const D3D12_RESOURCE_BARRIER*){};
    nativeCopyEndpoints::originalCopy=+[](ID3D12GraphicsCommandList*,const D3D12_TEXTURE_COPY_LOCATION*,UINT,UINT,UINT,const D3D12_TEXTURE_COPY_LOCATION*,const D3D12_BOX*){};
    auto close=nativeCopyEndpoints::originalClose;auto reset=nativeCopyEndpoints::originalReset;auto barrier=nativeCopyEndpoints::originalBarrier;auto copy=nativeCopyEndpoints::originalCopy;
    auto factory=nativeCopyEndpoints::contextFactory;auto dispatch=nativeCopyForwarders::dispatch;
    nativeCopyConfiguration::Snapshot saved;
    for(size_t i=0;i<entries.size();++i)*entries[i].original=reinterpret_cast<void*>(std::uintptr_t(0x2000+i));
    nativeCopyEndpoints::runtime=&second;nativeCopyEndpoints::contextFactory=nullptr;nativeCopyForwarders::dispatch=nullptr;
    nativeCopyEndpoints::originalClose=nullptr;nativeCopyEndpoints::originalReset=nullptr;nativeCopyEndpoints::originalBarrier=nullptr;nativeCopyEndpoints::originalCopy=nullptr;
    require(!saved.restore(false,true)&&!saved.restore(true,false)&&!saved.restore(false,false));
    require(nativeCopyEndpoints::runtime==&second&&!nativeCopyEndpoints::contextFactory&&!nativeCopyForwarders::dispatch);
    for(size_t i=0;i<entries.size();++i)require(*entries[i].original==reinterpret_cast<void*>(std::uintptr_t(0x2000+i)));
    struct Batch {unsigned calls{};bool succeeds{};bool rollback(){++calls;return succeeds;}} batch;
    require(!nativeCopyConfiguration::release(batch,saved,false)&&!batch.calls);
    require(!nativeCopyConfiguration::release(batch,saved,true)&&batch.calls==1&&nativeCopyEndpoints::runtime==&second);
    for(size_t i=0;i<entries.size();++i)require(*entries[i].original==reinterpret_cast<void*>(std::uintptr_t(0x2000+i)));
    batch.succeeds=true;require(nativeCopyConfiguration::release(batch,saved,true)&&batch.calls==2&&saved.restore(true,true));
    require(nativeCopyEndpoints::runtime==&first&&nativeCopyEndpoints::contextFactory==factory&&nativeCopyForwarders::dispatch==dispatch);
    require(nativeCopyEndpoints::originalClose==close&&nativeCopyEndpoints::originalReset==reset&&nativeCopyEndpoints::originalBarrier==barrier&&nativeCopyEndpoints::originalCopy==copy);
    for(size_t i=0;i<entries.size();++i)require(*entries[i].original==reinterpret_cast<void*>(std::uintptr_t(0x1000+i)));
    require(initial.restore(true,true));
    for(const auto& entry:entries)require(!*entry.original);
    require(!nativeCopyEndpoints::runtime&&!nativeCopyEndpoints::contextFactory&&!nativeCopyForwarders::dispatch);
    require(!nativeCopyEndpoints::originalClose&&!nativeCopyEndpoints::originalReset&&!nativeCopyEndpoints::originalBarrier&&!nativeCopyEndpoints::originalCopy);
    std::puts("PASS:47 adapter/configuration snapshot restores exact prior values only after explicit hook-removal/quiescence assertions; rejection preserves active configuration; CPU only, no hooks/game/XR.");return 0;
}catch(const std::exception& e){std::puts(e.what());return 1;}}

