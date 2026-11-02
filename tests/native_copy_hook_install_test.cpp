#define NOMINMAX
#include "../src/native_copy_forwarders.hpp"
#include "../src/native_copy_session.hpp"
#include "../src/native_copy_minhook.hpp"
#include <MinHook.h>
#include <dxgi1_4.h>
#include <d3d12sdklayers.h>
#include <wrl/client.h>
#include <vector>
#include <cstring>
#include <cstdio>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
static nativeCopySession::Session* activeSession;static std::vector<unsigned> events;static unsigned forwarded{},bypassed{};
using Index=nativeCopyForwarders::Adapter<&ID3D12GraphicsCommandList::IASetIndexBuffer>::Function;
static Index nativeIndex{};static unsigned legacyIndexCalls{},ownedIndexCalls{};
using Close=HRESULT(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*);static Close unrelatedClose{};static unsigned unrelatedCloses{};
static HRESULT STDMETHODCALLTYPE closeRecorder(ID3D12GraphicsCommandList* list){++unrelatedCloses;return unrelatedClose(list);}
using Marker=nativeCopyForwarders::Adapter<&ID3D12GraphicsCommandList::SetMarker>::Function;static Marker unrelatedMarker{};static unsigned unrelatedMarkers{};
static void STDMETHODCALLTYPE markerRecorder(ID3D12GraphicsCommandList* list,UINT metadata,const void* data,UINT size){++unrelatedMarkers;unrelatedMarker(list,metadata,data,size);}
static void STDMETHODCALLTYPE legacyIndex(ID3D12GraphicsCommandList* list,const D3D12_INDEX_BUFFER_VIEW* view){if(view||events!=std::vector<unsigned>({1,2}))throw std::runtime_error("legacy chain arguments/order changed");++legacyIndexCalls;const bool owned=activeSession&&activeSession->owns(list);ownedIndexCalls+=owned;
    ComPtr<IUnknown> canonical,deviceIdentity;ComPtr<ID3D12Device> device;
    if(FAILED(list->QueryInterface(IID_PPV_ARGS(&canonical)))||FAILED(list->GetDevice(IID_PPV_ARGS(&device)))||FAILED(device.As(&deviceIdentity)))throw std::runtime_error("runtime identity query failed");
    std::printf("legacy_index_entry list=%p canonical=%p device=%p type=%u owned=%d\n",list,canonical.Get(),deviceIdentity.Get(),static_cast<unsigned>(list->GetType()),owned);nativeIndex(list,view);}
static void check(HRESULT result){if(FAILED(result))throw std::runtime_error("hook fixture D3D operation failed");}
static void require(bool value){if(!value)throw std::runtime_error("hook installation/session check failed");}
struct Hooks {bool initialized{};~Hooks(){nativeCopyForwarders::dispatch=nullptr;if(initialized){MH_DisableHook(MH_ALL_HOOKS);MH_Uninitialize();}}};
int main(){try{
    ComPtr<ID3D12Debug> debug;check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();ComPtr<IDXGIFactory4> factory;check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));ComPtr<IDXGIAdapter> warp;check(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));ComPtr<ID3D12Device> device;check(D3D12CreateDevice(warp.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));
    std::array<ComPtr<ID3D12CommandAllocator>,2> allocators;std::array<ComPtr<ID3D12GraphicsCommandList>,2> lists;
    for(unsigned i=0;i<2;++i){auto type=i?D3D12_COMMAND_LIST_TYPE_COPY:D3D12_COMMAND_LIST_TYPE_DIRECT;check(device->CreateCommandAllocator(type,IID_PPV_ARGS(&allocators[i])));check(device->CreateCommandList(0,type,allocators[i].Get(),nullptr,IID_PPV_ARGS(&lists[i])));}
    auto resource=[&](bool texture){D3D12_RESOURCE_DESC desc{};desc.Dimension=texture?D3D12_RESOURCE_DIMENSION_TEXTURE2D:D3D12_RESOURCE_DIMENSION_BUFFER;desc.Width=texture?8:2048;desc.Height=texture?8:1;desc.DepthOrArraySize=desc.MipLevels=1;desc.Format=texture?DXGI_FORMAT_R8G8B8A8_UNORM:DXGI_FORMAT_UNKNOWN;desc.SampleDesc.Count=1;desc.Layout=texture?D3D12_TEXTURE_LAYOUT_UNKNOWN:D3D12_TEXTURE_LAYOUT_ROW_MAJOR;D3D12_HEAP_PROPERTIES heap{};heap.Type=texture?D3D12_HEAP_TYPE_DEFAULT:D3D12_HEAP_TYPE_UPLOAD;ComPtr<ID3D12Resource> result;check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,texture?D3D12_RESOURCE_STATE_COMMON:D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&result)));return result;};auto source=resource(false),destination=resource(true);
    auto entries=nativeCopyForwarders::entries();auto table=*reinterpret_cast<void***>(lists[0].Get());std::array<std::array<unsigned char,32>,47> originalBytes;for(size_t i=0;i<entries.size();++i)std::memcpy(originalBytes[i].data(),table[entries[i].slot],32);
    Hooks hooks;require(MH_Initialize()==MH_OK);hooks.initialized=true;
    std::array<unsigned char,32> closeBytes;std::memcpy(closeBytes.data(),table[9],32);require(MH_CreateHook(table[9],reinterpret_cast<void*>(closeRecorder),reinterpret_cast<void**>(&unrelatedClose))==MH_OK);require(MH_EnableHook(table[9])==MH_OK);
    std::array<nativeCopyInstall::Binding,47> bindings;for(size_t i=0;i<entries.size();++i){const auto& entry=entries[i];auto receiver=entry.original;if(entry.slot==43)receiver=nativeCopyForwarders::chain<&ID3D12GraphicsCommandList::IASetIndexBuffer>(&legacyIndex,nativeIndex);bindings[i]={table[entry.slot],entry.hook,receiver};}
    nativeCopyInstall::Batch<nativeCopyMinHook::Backend> batch(nativeCopyMinHook::Backend{});
    require(MH_CreateHook(table[56],reinterpret_cast<void*>(markerRecorder),reinterpret_cast<void**>(&unrelatedMarker))==MH_OK);require(MH_EnableHook(table[56])==MH_OK);require(!batch.install(bindings)&&!batch.pendingCleanup());for(size_t i=0;i<entries.size();++i)if(entries[i].slot!=56)require(!std::memcmp(originalBytes[i].data(),table[entries[i].slot],32));lists[0]->SetMarker(0,nullptr,0);require(unrelatedMarkers>=1);require(MH_DisableHook(table[56])==MH_OK);require(MH_RemoveHook(table[56])==MH_OK);std::printf("transactional_collision_rollback preserved_marker_calls=%u\n",unrelatedMarkers);
    require(batch.install(bindings)&&batch.installed());std::puts("transactional_batch_installed targets=47 unrelated_close_enabled=1");
    nativeCopyEpoch::Source epochs;nativeCopySession::Session session;require(session.created(epochs,lists[1].Get()));activeSession=&session;
    nativeCopyPreflight::Plan plan;plan.valid=true;plan.site=nativeCopyBarrierPolicy::Site::UploadBegin;plan.copyReturnRva=0x1eac36;plan.endReturnRva=0x1eac72;plan.source.pResource=source.Get();plan.source.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;plan.source.PlacedFootprint.Footprint={DXGI_FORMAT_R8G8B8A8_UNORM,8,8,1,256};plan.destination.pResource=destination.Get();plan.destination.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;plan.box={0,0,0,8,8,1};D3D12_RESOURCE_BARRIER begin{};begin.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;begin.Transition={destination.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_DEST};
    auto barrier=[](const D3D12_RESOURCE_BARRIER&){events.push_back(1);};auto copy=[](const auto&,const auto&,const auto&){events.push_back(2);};require(session.begin(plan,lists[1].Get(),10,100,begin,barrier,copy));require(session.copy(plan.copyReturnRva,20,&plan.destination,0,0,0,&plan.source,&plan.box,barrier,copy));
    nativeCopyForwarders::dispatch=+[](ID3D12GraphicsCommandList* list,void(*invoke)(void*),void* context){if(!activeSession->owns(list)){++bypassed;invoke(context);return;}activeSession->intervening([](const D3D12_RESOURCE_BARRIER&){events.push_back(1);},[](const auto&,const auto&,const auto&){events.push_back(2);},[&]{require(events==std::vector<unsigned>({1,2})&&!activeSession->pending());invoke(context);events.push_back(3);++forwarded;});};
    lists[1]->SetMarker(0,nullptr,0);std::printf("copy_marker forwarded=%u bypassed=%u pending=%d events=%zu\n",forwarded,bypassed,session.pending(),events.size());require(forwarded==1&&!session.pending()&&events==std::vector<unsigned>({1,2,3}));lists[0]->SetMarker(0,nullptr,0);std::printf("direct_marker bypassed=%u\n",bypassed);require(bypassed>=1);
    require(session.created(epochs,lists[0].Get()));events.clear();require(session.begin(plan,lists[0].Get(),10,100,begin,barrier,copy));require(session.copy(plan.copyReturnRva,20,&plan.destination,0,0,0,&plan.source,&plan.box,barrier,copy));lists[0]->IASetIndexBuffer(nullptr);std::printf("legacy_index calls=%u owned_calls=%u forwarded=%u pending=%d events=%zu\n",legacyIndexCalls,ownedIndexCalls,forwarded,session.pending(),events.size());require(ownedIndexCalls==1&&legacyIndexCalls>=ownedIndexCalls&&forwarded==2&&!session.pending()&&events==std::vector<unsigned>({1,2,3}));
    nativeCopyForwarders::dispatch=nullptr;require(batch.rollback()&&!batch.pendingCleanup());for(size_t i=0;i<entries.size();++i)require(!std::memcmp(originalBytes[i].data(),table[entries[i].slot],32));for(auto& list:lists)check(list->Close());require(unrelatedCloses>=2);std::printf("transactional_batch_removed unrelated_close_calls=%u\n",unrelatedCloses);require(MH_DisableHook(table[9])==MH_OK);require(MH_RemoveHook(table[9])==MH_OK);require(!std::memcmp(closeBytes.data(),table[9],32));require(MH_Uninitialize()==MH_OK);hooks.initialized=false;activeSession=nullptr;
    ComPtr<ID3D12InfoQueue> messages;check(device.As(&messages));for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T bytes{};check(messages->GetMessage(i,nullptr,&bytes));std::vector<unsigned char> data(bytes);auto message=reinterpret_cast<D3D12_MESSAGE*>(data.data());check(messages->GetMessage(i,message,&bytes));if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(message->pDescription);}
    std::puts("PASS: all47 adapters install on isolated WARP debug-wrapper targets; actual COPY SetMarker flushes metadata prefix before native trampoline, unrelated DIRECT bypasses; 32-byte entries restored; no queue submission/game attachment/XR. Marker and DIRECT index-unbind calls exercised natively.");return 0;
}catch(const std::exception& error){std::puts(error.what());return 1;}}
