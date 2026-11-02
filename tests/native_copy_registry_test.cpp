#define NOMINMAX
#include "../src/native_copy_registry.hpp"
#include "../src/native_copy_runtime.hpp"
#include "../src/native_copy_endpoints.hpp"
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <thread>
#include <array>
#include <vector>
#include <d3d12sdklayers.h>
#include <cstdio>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
static void check(HRESULT value){if(FAILED(value))throw std::runtime_error("registry WARP call failed");}
static void require(bool value){if(!value)throw std::runtime_error("copy registry check failed");}
int main(){try{
    ComPtr<ID3D12Debug> debug;check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();
    ComPtr<IDXGIFactory4> factory;check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));ComPtr<IDXGIAdapter> warp;check(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));ComPtr<ID3D12Device> device;check(D3D12CreateDevice(warp.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));
    std::array<ComPtr<ID3D12CommandAllocator>,4> allocators;std::array<ComPtr<ID3D12GraphicsCommandList>,4> lists;for(unsigned i=0;i<4;++i){auto type=i==3?D3D12_COMMAND_LIST_TYPE_DIRECT:D3D12_COMMAND_LIST_TYPE_COPY;check(device->CreateCommandAllocator(type,IID_PPV_ARGS(&allocators[i])));check(device->CreateCommandList(0,type,allocators[i].Get(),nullptr,IID_PPV_ARGS(&lists[i])));if(i!=1)check(lists[i]->Close());}
    nativeCopyRegistry::Registry<2> registry;unsigned forwards{};auto barrier=[](const auto&){};auto copy=[](const auto&,const auto&,const auto&){};
    registry.intervening(lists[0].Get(),barrier,copy,[&]{++forwards;});require(forwards==1&&!registry.size()&&!registry.find(lists[0].Get()));require(registry.reset(lists[0].Get(),[]{return E_FAIL;})==E_FAIL&&!registry.size());
    check(registry.reset(lists[0].Get(),[&]{return lists[0]->Reset(allocators[0].Get(),nullptr);}));auto state=registry.find(lists[0].Get());require(state&&state->owns(lists[0].Get())&&registry.size()==1);auto epoch=state->epoch();require(registry.reset(lists[0].Get(),[]{return E_FAIL;})==E_FAIL&&state->epoch()==epoch);
    std::exception_ptr failure;std::thread handoff([&]{try{require(registry.find(lists[0].Get())==state);check(registry.close(lists[0].Get(),barrier,copy,[&]{return lists[0]->Close();}));check(registry.reset(lists[0].Get(),[&]{return lists[0]->Reset(allocators[0].Get(),nullptr);}));require(state->epoch()!=epoch);}catch(...){failure=std::current_exception();}});handoff.join();if(failure)std::rethrow_exception(failure);require(registry.find(lists[0].Get())==state);
    require(registry.created(lists[1].Get())&&registry.size()==2&&registry.find(lists[1].Get())->epoch()!=state->epoch());require(!registry.created(lists[2].Get())&&!registry.created(lists[3].Get())&&!registry.created(nullptr));check(registry.reset(lists[2].Get(),[&]{return lists[2]->Reset(allocators[2].Get(),nullptr);}));require(!registry.find(lists[2].Get())&&registry.size()==2);check(registry.reset(lists[3].Get(),[&]{return lists[3]->Reset(allocators[3].Get(),nullptr);}));require(!registry.find(lists[3].Get()));
    // Quiescent teardown inspection must count independent held recordings,
    // not merely expose a global boolean. Logical lifecycle outcomes below are
    // stubs: no commands are emitted or submitted to these WARP lists.
    {
        D3D12_RESOURCE_DESC description{};description.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;description.Width=2048;description.Height=1;description.DepthOrArraySize=description.MipLevels=1;description.SampleDesc.Count=1;description.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_UPLOAD;ComPtr<ID3D12Resource> source,destination;
        check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&description,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&source)));
        check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&description,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&destination)));
        nativeCopyPreflight::Plan plan{};plan.valid=true;plan.site=nativeCopyBarrierPolicy::Site::UploadBegin;plan.copyReturnRva=0x1eac36;plan.endReturnRva=0x1eac72;plan.source.pResource=source.Get();plan.destination.pResource=destination.Get();
        D3D12_RESOURCE_BARRIER begin{};begin.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;begin.Transition={destination.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_DEST};
        require(!registry.pendingRecordings());
        for(unsigned i=0;i<2;++i)require(registry.find(lists[i].Get())->begin(plan,lists[i].Get(),1,100,begin,barrier,copy));
        require(registry.pendingRecordings()==2);
        require(registry.reset(lists[0].Get(),[]{return E_FAIL;})==E_FAIL&&registry.pendingRecordings()==2);
        require(registry.reset(lists[0].Get(),[]{return S_OK;})==S_OK&&registry.pendingRecordings()==1);
        unsigned replays{};require(registry.close(lists[1].Get(),[&](const auto&){++replays;},copy,[]{return E_FAIL;})==E_FAIL);
        require(replays==1&&!registry.pendingRecordings());
    }
    nativeCopyRuntime::Runtime runtime;require(runtime.created(lists[0].Get()));unsigned proofReads{},barrierForwards{},copyForwards{};D3D12_RESOURCE_BARRIER opaque{};opaque.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;opaque.Transition={reinterpret_cast<ID3D12Resource*>(std::uintptr_t(3)),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_DEST};const std::uintptr_t image=0x100000000ULL;
    auto reader=[&](std::uintptr_t,void*,size_t){++proofReads;return false;};auto forwardBarrier=[&](UINT count,const D3D12_RESOURCE_BARRIER* values){require(count==1&&values==&opaque);++barrierForwards;};auto forwardCopy=[&](const D3D12_TEXTURE_COPY_LOCATION* dst,UINT x,UINT y,UINT z,const D3D12_TEXTURE_COPY_LOCATION* src,const D3D12_BOX* box){require(!dst&&!src&&!box&&x==1&&y==2&&z==3);++copyForwards;};
    runtime.barrier(image,image+0x1eabf1,device.Get(),lists[0].Get(),100,1,&opaque,0,reader,forwardBarrier,forwardCopy);runtime.copy(image,image+0x1eac36,lists[0].Get(),100,nullptr,1,2,3,nullptr,nullptr,forwardBarrier,forwardCopy);require(!proofReads&&barrierForwards==1&&copyForwards==1&&!runtime.pending(lists[0].Get()));
    require(runtime.arm(100,10,1));runtime.barrier(image,image+1,device.Get(),lists[0].Get(),101,1,&opaque,0,reader,forwardBarrier,forwardCopy);runtime.barrier(image,image+0x1eabf1,device.Get(),lists[0].Get(),110,1,&opaque,0,reader,forwardBarrier,forwardCopy);require(!proofReads&&barrierForwards==3);runtime.cancel();
    static unsigned endpointCloses{},endpointResets{},endpointBarriers{},endpointCopies{},contextCalls{};static const D3D12_RESOURCE_BARRIER* endpointExpected;endpointExpected=&opaque;
    nativeCopyEndpoints::runtime=&runtime;nativeCopyEndpoints::originalClose=+[](ID3D12GraphicsCommandList* incoming){++endpointCloses;return incoming->Close();};nativeCopyEndpoints::originalReset=+[](ID3D12GraphicsCommandList* incoming,ID3D12CommandAllocator* allocator,ID3D12PipelineState* pipeline){++endpointResets;return incoming->Reset(allocator,pipeline);};nativeCopyEndpoints::originalBarrier=+[](ID3D12GraphicsCommandList*,UINT count,const D3D12_RESOURCE_BARRIER* values){require(count==1&&values==endpointExpected);++endpointBarriers;};nativeCopyEndpoints::originalCopy=+[](ID3D12GraphicsCommandList*,const D3D12_TEXTURE_COPY_LOCATION* dst,UINT x,UINT y,UINT z,const D3D12_TEXTURE_COPY_LOCATION* src,const D3D12_BOX* box){require(!dst&&!src&&!box&&x==1&&y==2&&z==3);++endpointCopies;};
    nativeCopyEndpoints::contextFactory=+[](const nativeCopyEndpoints::Observation& observation){require(observation.caller!=0);if(observation.kind==nativeCopyEndpoints::Kind::Barrier)require(observation.frame==reinterpret_cast<std::uintptr_t>(endpointExpected));++contextCalls;return nativeCopyEndpoints::Context{};};
    check(nativeCopyEndpoints::close(lists[0].Get()));check(nativeCopyEndpoints::reset(lists[0].Get(),allocators[0].Get(),nullptr));nativeCopyEndpoints::barrier(lists[0].Get(),1,&opaque);nativeCopyEndpoints::copy(lists[0].Get(),nullptr,1,2,3,nullptr,nullptr);require(endpointCloses==1&&endpointResets==1&&contextCalls==2);
    nativeCopyEndpoints::runtime=nullptr;nativeCopyEndpoints::barrier(lists[0].Get(),1,&opaque);nativeCopyEndpoints::copy(lists[0].Get(),nullptr,1,2,3,nullptr,nullptr);require(endpointBarriers==2&&endpointCopies==2&&contextCalls==2);nativeCopyEndpoints::contextFactory=nullptr;nativeCopyEndpoints::originalClose=nullptr;nativeCopyEndpoints::originalReset=nullptr;nativeCopyEndpoints::originalBarrier=nullptr;nativeCopyEndpoints::originalCopy=nullptr;
    for(auto& list:lists)check(list->Close());
    ComPtr<ID3D12InfoQueue> messages;check(device.As(&messages));for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T bytes{};check(messages->GetMessage(i,nullptr,&bytes));std::vector<unsigned char> data(bytes);auto message=reinterpret_cast<D3D12_MESSAGE*>(data.data());check(messages->GetMessage(i,message,&bytes));if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(message->pDescription);}
    std::puts("PASS: actual WARP COPY registry establishes epochs only on observed creation/successful Reset; failed/unknown/direct/over-cap calls preserve native results; stable sequential thread handoff; no commands/submissions/game/XR.");return 0;
}catch(const std::exception& error){std::puts(error.what());return 1;}}

