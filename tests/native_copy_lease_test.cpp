#define NOMINMAX
#include <windows.h>
#include "../src/native_copy_lease.hpp"
#include "../src/native_copy_device.hpp"
#include "../src/native_copy_lifecycle.hpp"
#include "../src/native_copy_session.hpp"
#include "../src/native_copy_dispatch.hpp"
#include <dxgi1_4.h>
#include <d3d12sdklayers.h>
#include <atomic>
#include <vector>
#include <cstdio>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
static void check(HRESULT result){if(FAILED(result))throw std::runtime_error("copy lease WARP operation failed");}
static void require(bool value){if(!value)throw std::runtime_error("copy lease lifetime check failed");}
struct Probe: IUnknown {
    std::atomic<ULONG> refs{1};
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** output) override{if(!output)return E_POINTER;*output=nullptr;if(iid!=__uuidof(IUnknown))return E_NOINTERFACE;*output=this;AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release() override{return --refs;}
};
static const GUID lifetimeTag={0x4ba2c958,0xd20b,0x4cb9,{0xa1,0x5c,0x82,0x41,0x09,0xe3,0x6f,0x21}};
int main(){try{
    ComPtr<ID3D12Debug> debug;check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();ComPtr<IDXGIFactory4> factory;check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));ComPtr<IDXGIAdapter> warp;check(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));ComPtr<ID3D12Device> device;check(D3D12CreateDevice(warp.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));
    // D3D12CreateDevice returns the adapter's singleton; a second WARP call is
    // not a foreign device. Use a distinct hardware adapter when available.
    ComPtr<ID3D12Device> foreignDevice;
    for(UINT index=0;!foreignDevice;++index){ComPtr<IDXGIAdapter1> adapter;if(factory->EnumAdapters1(index,&adapter)==DXGI_ERROR_NOT_FOUND)break;DXGI_ADAPTER_DESC1 description{};check(adapter->GetDesc1(&description));if(description.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&foreignDevice));}
    std::printf("Foreign-adapter ownership check: %s\n",foreignDevice?"available":"SKIPPED (no second adapter)");
    unsigned cases{};
    for(unsigned scenario=0;scenario<16;++scenario){
        Probe listProbe,sourceProbe,destinationProbe;
        auto resource=[&](bool texture){D3D12_RESOURCE_DESC description{};description.Dimension=texture?D3D12_RESOURCE_DIMENSION_TEXTURE2D:D3D12_RESOURCE_DIMENSION_BUFFER;description.Width=texture?8:2048;description.Height=texture?8:1;description.DepthOrArraySize=description.MipLevels=1;description.Format=texture?DXGI_FORMAT_R8G8B8A8_UNORM:DXGI_FORMAT_UNKNOWN;description.SampleDesc.Count=1;description.Layout=texture?D3D12_TEXTURE_LAYOUT_UNKNOWN:D3D12_TEXTURE_LAYOUT_ROW_MAJOR;D3D12_HEAP_PROPERTIES heap{};heap.Type=texture?D3D12_HEAP_TYPE_DEFAULT:D3D12_HEAP_TYPE_UPLOAD;ComPtr<ID3D12Resource> result;check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&description,texture?D3D12_RESOURCE_STATE_COMMON:D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&result)));return result;};
        auto source=resource(false),destination=resource(true);ComPtr<ID3D12CommandAllocator> allocator;check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY,IID_PPV_ARGS(&allocator)));ComPtr<ID3D12GraphicsCommandList> list;check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_COPY,allocator.Get(),nullptr,IID_PPV_ARGS(&list)));
        require(nativeCopyDevice::matches(device.Get(),list.Get(),source.Get(),destination.Get()));
        if(foreignDevice)require(!nativeCopyDevice::matches(foreignDevice.Get(),list.Get(),source.Get(),destination.Get()));
        if(scenario==0){
            ComPtr<ID3D12CommandAllocator> directAllocator;ComPtr<ID3D12GraphicsCommandList> directList;
            check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&directAllocator)));check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,directAllocator.Get(),nullptr,IID_PPV_ARGS(&directList)));
            require(!nativeCopyDevice::matches(device.Get(),directList.Get(),source.Get(),destination.Get()));check(directList->Close());
            if(foreignDevice){D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_UPLOAD;auto description=source->GetDesc();ComPtr<ID3D12Resource> foreignResource;check(foreignDevice->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&description,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&foreignResource)));
                require(!nativeCopyDevice::matches(device.Get(),list.Get(),foreignResource.Get(),destination.Get()));
                require(!nativeCopyDevice::matches(device.Get(),list.Get(),source.Get(),foreignResource.Get()));
            }
        }
        require(!nativeCopyDevice::matches(nullptr,list.Get(),source.Get(),destination.Get()));
        require(!nativeCopyDevice::matches(device.Get(),nullptr,source.Get(),destination.Get()));
        require(!nativeCopyDevice::matches(device.Get(),list.Get(),nullptr,destination.Get()));
        require(!nativeCopyDevice::matches(device.Get(),list.Get(),source.Get(),nullptr));
        require(!nativeCopyDevice::matches(device.Get(),list.Get(),source.Get(),source.Get()));
        check(source->SetPrivateDataInterface(lifetimeTag,&sourceProbe));check(destination->SetPrivateDataInterface(lifetimeTag,&destinationProbe));check(list->SetPrivateDataInterface(lifetimeTag,&listProbe));require(sourceProbe.refs==2&&destinationProbe.refs==2&&listProbe.refs==2);
        auto rawList=list.Get();nativeCopyPreflight::Plan plan;plan.valid=true;plan.site=nativeCopyBarrierPolicy::Site::UploadBegin;plan.copyReturnRva=0x1eac36;plan.endReturnRva=0x1eac72;plan.source.pResource=source.Get();plan.source.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;plan.source.PlacedFootprint.Footprint={DXGI_FORMAT_R8G8B8A8_UNORM,8,8,1,256};plan.destination.pResource=destination.Get();plan.destination.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;plan.box={0,0,0,8,8,1};
        D3D12_RESOURCE_BARRIER begin{};begin.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;begin.Transition={destination.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_DEST};auto end=begin;end.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_DEST;end.Transition.StateAfter=D3D12_RESOURCE_STATE_COMMON;unsigned emissions{};
        if(scenario==15){
            nativeCopyEpoch::Source epochs;nativeCopySession::Session session;require(session.created(epochs,rawList));unsigned admissions{},barrierCalls{},copyCalls{};UINT expectedCount=1;const D3D12_RESOURCE_BARRIER* expected=&begin;
            auto originalBarrier=[&](UINT count,const D3D12_RESOURCE_BARRIER* values){require(count==expectedCount&&values==expected);++barrierCalls;};
            auto originalCopy=[&](const D3D12_TEXTURE_COPY_LOCATION* dst,UINT x,UINT y,UINT z,const D3D12_TEXTURE_COPY_LOCATION* src,const D3D12_BOX* region){require(!dst&&!src&&!region&&x==12&&y==13&&z==14);++copyCalls;};
            auto reject=[&]{++admissions;return nativeCopyPreflight::Plan{};};
            nativeCopyDispatch::barrier(session,rawList,0,10,0,1,&begin,reject,originalBarrier,originalCopy);
            nativeCopyDispatch::barrier(session,rawList,0,20,10,1,&begin,reject,originalBarrier,originalCopy);
            nativeCopyDispatch::barrier(session,nullptr,0,10,100,1,&begin,reject,originalBarrier,originalCopy);
            expectedCount=0;nativeCopyDispatch::barrier(session,rawList,0,10,100,0,&begin,reject,originalBarrier,originalCopy);
            expectedCount=1;expected=nullptr;nativeCopyDispatch::barrier(session,rawList,0,10,100,1,nullptr,reject,originalBarrier,originalCopy);require(!admissions);
            expected=&begin;nativeCopyDispatch::barrier(session,rawList,0,10,100,1,&begin,reject,originalBarrier,originalCopy);require(admissions==1&&barrierCalls==6&&!session.pending());
            nativeCopyDispatch::copy(session,rawList,0,10,nullptr,12,13,14,nullptr,nullptr,originalBarrier,originalCopy);nativeCopyDispatch::copy(session,nullptr,0,10,nullptr,12,13,14,nullptr,nullptr,originalBarrier,originalCopy);require(copyCalls==2);
            auto singleBarrier=[&](const D3D12_RESOURCE_BARRIER& value){originalBarrier(1,&value);};auto singleCopy=[&](const auto& dst,const auto& src,const auto& region){originalCopy(&dst,0,0,0,&src,&region);};
            require(session.close(singleBarrier,singleCopy,[&]{nativeCopyDispatch::barrier(session,rawList,0,10,100,1,&begin,reject,originalBarrier,originalCopy);return E_FAIL;})==E_FAIL);require(admissions==1&&barrierCalls==7);
            require(session.close(singleBarrier,singleCopy,[]{return S_OK;})==S_OK);nativeCopyDispatch::barrier(session,rawList,0,10,100,1,&begin,reject,originalBarrier,originalCopy);require(admissions==1&&barrierCalls==8&&!session.pending()); // Closed-list calls forward without proof reads.
            source.Reset();destination.Reset();list.Reset();require(sourceProbe.refs==1&&destinationProbe.refs==1&&listProbe.refs==1);++cases;continue;
        }
        if(scenario==14){
            nativeCopyEpoch::Source epochs;nativeCopySession::Session session;unsigned barriers{},copies{};
            auto emitBarrier=[&](const D3D12_RESOURCE_BARRIER&){++barriers;};auto emitCopy=[&](const auto&,const auto&,const auto&){++copies;};
            require(!session.begin(plan,rawList,10,100,begin,emitBarrier,emitCopy));require(session.created(epochs,rawList));auto old=session.epoch();
            require(session.begin(plan,rawList,10,100,begin,emitBarrier,emitCopy));require(session.copy(plan.copyReturnRva,20,&plan.destination,0,0,0,&plan.source,&plan.box,emitBarrier,emitCopy));
            require(!session.created(epochs,rawList));require(session.reset(epochs,[&]{require(!session.begin(plan,rawList,25,100,begin,emitBarrier,emitCopy));require(!session.created(epochs,rawList));return E_FAIL;})==E_FAIL&&session.pending()&&session.epoch()==old);
            check(session.reset(epochs,[&]{check(rawList->Close());return rawList->Reset(allocator.Get(),nullptr);}));require(!session.pending()&&session.epoch()!=old&&!barriers&&!copies);
            session.intervening(emitBarrier,emitCopy,[&]{require(!session.begin(plan,rawList,26,100,begin,emitBarrier,emitCopy));bool nestedForwarded=false;require(session.close(emitBarrier,emitCopy,[&]{nestedForwarded=true;return E_FAIL;})==E_FAIL&&nestedForwarded);});
            bool closeThrew=false;try{session.close(emitBarrier,emitCopy,[]()->HRESULT{throw std::runtime_error("expected forward exception");});}catch(const std::runtime_error&){closeThrew=true;}require(closeThrew&&!session.pending());
            require(session.begin(plan,rawList,30,100,begin,emitBarrier,emitCopy));require(session.copy(plan.copyReturnRva,40,&plan.destination,0,0,0,&plan.source,&plan.box,emitBarrier,emitCopy));require(session.end(plan.endReturnRva,50,1,&end,emitBarrier,emitCopy));require(copies==1&&!barriers);
            check(session.close(emitBarrier,emitCopy,[&]{require(!session.begin(plan,rawList,55,100,begin,emitBarrier,emitCopy));return rawList->Close();}));require(!session.begin(plan,rawList,60,100,begin,emitBarrier,emitCopy));
            old=session.epoch();check(session.reset(epochs,[&]{return rawList->Reset(allocator.Get(),nullptr);}));require(session.epoch()!=old);
            nativeCopySession::Session recreated;require(recreated.created(epochs,rawList)&&recreated.epoch()!=session.epoch());nativeCopyEpoch::Source exhausted(0);nativeCopySession::Session disabled;require(!disabled.created(exhausted,rawList)&&!disabled.begin(plan,rawList,60,100,begin,emitBarrier,emitCopy));
            source.Reset();destination.Reset();list.Reset();require(sourceProbe.refs==1&&destinationProbe.refs==1&&listProbe.refs==1);++cases;continue;
        }
        {
            nativeCopyLease::Pinned transaction;
            auto alive=[&]{require(sourceProbe.refs==2&&destinationProbe.refs==2&&listProbe.refs==2);require(plan.source.pResource->GetDesc().Width==2048&&plan.destination.pResource->GetDesc().Width==8&&rawList->GetType()==D3D12_COMMAND_LIST_TYPE_COPY);};
            auto emitBarrier=[&](const D3D12_RESOURCE_BARRIER&){alive();++emissions;if(scenario==8)throw std::runtime_error("expected barrier emitter exception");};
            auto emitCopy=[&](const D3D12_TEXTURE_COPY_LOCATION&,const D3D12_TEXTURE_COPY_LOCATION&,const D3D12_BOX&){alive();++emissions;if(scenario==4)throw std::runtime_error("expected emitter exception");if(scenario==5)require(!transaction.begin(plan,rawList,2,40,100,begin,emitBarrier,[](const auto&,const auto&,const auto&){}));};
            if(scenario==6){auto invalid=plan;invalid.valid=false;require(!transaction.begin(invalid,rawList,1,10,100,begin,emitBarrier,emitCopy));source.Reset();destination.Reset();list.Reset();require(sourceProbe.refs==1&&destinationProbe.refs==1&&listProbe.refs==1);++cases;continue;}
            require(transaction.begin(plan,rawList,1,10,100,begin,emitBarrier,emitCopy));source.Reset();destination.Reset();list.Reset();alive();
            if(scenario==10){auto nativeCallLease=transaction.retain();auto wrong=plan.source;wrong.PlacedFootprint.Offset=512;require(!transaction.copy(rawList,1,plan.copyReturnRva,20,&plan.destination,0,0,0,&wrong,&plan.box,emitBarrier,emitCopy));require(emissions==1&&!transaction.pending());alive();nativeCallLease.clear();require(sourceProbe.refs==1&&destinationProbe.refs==1&&listProbe.refs==1);++cases;continue;}
            require(transaction.copy(rawList,1,plan.copyReturnRva,20,&plan.destination,0,0,0,&plan.source,&plan.box,emitBarrier,emitCopy));require(emissions==0);
            if(scenario==7){require(transaction.pending());alive();} // Verify destruction below, before probes leave scope.
            else if(scenario==3){require(nativeCopyLifecycle::reset(transaction,[&]{alive();return E_FAIL;})==E_FAIL);alive();require(transaction.pending());require(nativeCopyLifecycle::reset(transaction,[&]{alive();return S_OK;})==S_OK);require(!transaction.pending()&&sourceProbe.refs==1&&destinationProbe.refs==1&&listProbe.refs==1);} // Reset outcomes simulated; real COM lifetime checked.
            else if(scenario==11){check(nativeCopyLifecycle::close(transaction,emitBarrier,emitCopy,[&]{alive();require(emissions==2&&!transaction.pending());return rawList->Close();}));require(sourceProbe.refs==1&&destinationProbe.refs==1&&listProbe.refs==1);}
            else if(scenario==12){require(nativeCopyLifecycle::close(transaction,emitBarrier,emitCopy,[&]{alive();require(emissions==2&&!transaction.pending());return E_FAIL;})==E_FAIL);require(sourceProbe.refs==1&&destinationProbe.refs==1&&listProbe.refs==1);} // Failure HRESULT forwarded unchanged.
            else if(scenario==13){bool forwarded=false;nativeCopyLifecycle::intervening(transaction,emitBarrier,emitCopy,[&]{alive();require(emissions==2&&!transaction.pending());forwarded=true;});require(forwarded&&sourceProbe.refs==1&&destinationProbe.refs==1&&listProbe.refs==1);}
            else if(scenario==8){bool threw=false;try{transaction.flush(emitBarrier,emitCopy);}catch(const std::runtime_error&){threw=true;}require(threw&&emissions==1&&!transaction.pending());require(sourceProbe.refs==1&&destinationProbe.refs==1&&listProbe.refs==1);transaction.flush(emitBarrier,emitCopy);require(emissions==1);}
            else if(scenario==1||scenario==2||scenario==9){auto nativeCallLease=transaction.retain();if(scenario==1)require(!transaction.end(rawList,1,plan.endReturnRva+1,30,1,&end,emitBarrier,emitCopy));else if(scenario==9)require(!transaction.begin(plan,rawList,1,30,100,begin,emitBarrier,emitCopy));else transaction.flush(emitBarrier,emitCopy);require(!transaction.pending()&&emissions==2);alive();check(rawList->Close());alive();nativeCallLease.clear();require(sourceProbe.refs==1&&destinationProbe.refs==1&&listProbe.refs==1);}
            else {bool threw=false;try{require(transaction.end(rawList,1,plan.endReturnRva,30,1,&end,emitBarrier,emitCopy));}catch(const std::runtime_error&){if(scenario!=4)throw;threw=true;}require(threw==(scenario==4)&&emissions==1&&!transaction.pending());require(sourceProbe.refs==1&&destinationProbe.refs==1&&listProbe.refs==1);}
        }
        require(sourceProbe.refs==1&&destinationProbe.refs==1&&listProbe.refs==1);++cases;
    }
    ComPtr<ID3D12InfoQueue> messages;check(device.As(&messages));for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T bytes{};check(messages->GetMessage(i,nullptr,&bytes));std::vector<unsigned char> data(bytes);auto message=reinterpret_cast<D3D12_MESSAGE*>(data.data());check(messages->GetMessage(i,message,&bytes));if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(message->pDescription);}
    std::printf("PASS: %u actual WARP resource/list lifetime cases retain COM objects through CPU emission, native-call lease forwarding, failed Reset and exceptions; reentrant admission rejected, discarded teardown releases pins; no GPU submission/game/XR.\n",cases);return 0;
}catch(const std::exception& error){std::puts(error.what());return 1;}}
