#define NOMINMAX
#ifdef DEMENTIUM_ALL_COMMAND_HOOKS
#include "../src/bridge.cpp"
#endif
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <vector>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <array>
#include "../src/native_copy_transaction.hpp"
#include "../src/native_copy_admission.hpp"
#include "../src/native_copy_dispatch.hpp"
#include "../src/native_copy_context.hpp"
#include "../src/native_copy_prepare.hpp"
#include "../src/native_copy_runtime.hpp"
#include "../src/native_copy_endpoints.hpp"
#include "../src/native_copy_configuration.hpp"
#ifdef DEMENTIUM_INSTALLED_COPY_ENDPOINTS
#include "../src/native_copy_minhook.hpp"
#endif
using Microsoft::WRL::ComPtr;
static void* endpointBarrierPayload;static void* endpointCopyPayload;static ID3D12GraphicsCommandList* endpointList;static nativeCopyEndpoints::Context endpointContext;static std::uintptr_t endpointObservedFrame;static unsigned endpointObservations;
struct EndpointFixture {~EndpointFixture(){nativeCopyEndpoints::runtime=nullptr;nativeCopyEndpoints::contextFactory=nullptr;nativeCopyEndpoints::originalBarrier=nullptr;nativeCopyEndpoints::originalCopy=nullptr;nativeCopyEndpoints::originalClose=nullptr;nativeCopyEndpoints::originalReset=nullptr;endpointBarrierPayload=endpointCopyPayload=nullptr;endpointList=nullptr;}};
static void check(HRESULT result){if(FAILED(result))throw std::runtime_error("copy layout operation failed");}
static void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
#ifdef DEMENTIUM_INSTALLED_COPY_ENDPOINTS
static nativeCopyEndpoints::Barrier endpointNativeBarrier;static nativeCopyEndpoints::Copy endpointNativeCopy;static nativeCopyEndpoints::Close endpointNativeClose;static nativeCopyEndpoints::Reset endpointNativeReset;static unsigned endpointNestedObservations;
#ifndef DEMENTIUM_ALL_COMMAND_HOOKS
struct InstalledCopyEndpoints {
    nativeCopyInstall::Batch<nativeCopyMinHook::Backend,4> batch{nativeCopyMinHook::Backend{}};bool initialized{};std::array<void*,4> targets{};std::array<std::array<unsigned char,32>,4> bytes;
    void install(ID3D12GraphicsCommandList* list){require(MH_Initialize()==MH_OK,"endpoint MinHook initialize failed");initialized=true;auto table=*reinterpret_cast<void***>(list);targets={table[26],table[16],table[9],table[10]};for(size_t i=0;i<targets.size();++i)std::memcpy(bytes[i].data(),targets[i],32);std::array<nativeCopyInstall::Binding,4> bindings{{{targets[0],reinterpret_cast<void*>(nativeCopyEndpoints::barrier),reinterpret_cast<void**>(&endpointNativeBarrier)},{targets[1],reinterpret_cast<void*>(nativeCopyEndpoints::copy),reinterpret_cast<void**>(&endpointNativeCopy)},{targets[2],reinterpret_cast<void*>(nativeCopyEndpoints::close),reinterpret_cast<void**>(&endpointNativeClose)},{targets[3],reinterpret_cast<void*>(nativeCopyEndpoints::reset),reinterpret_cast<void**>(&endpointNativeReset)}}};require(batch.install(bindings),"copy endpoint installation failed");}
    bool stop(){if(!initialized)return true;if(!batch.rollback())return false;bool restored=true;for(size_t i=0;i<targets.size();++i)restored&=!std::memcmp(bytes[i].data(),targets[i],32);restored&=MH_Uninitialize()==MH_OK;initialized=false;return restored;}
    ~InstalledCopyEndpoints(){if(!stop())std::terminate();}
};
#else
struct InstalledCopyEndpoints {
    nativeCopyInstall::Batch<nativeCopyMinHook::Backend,51> batch{nativeCopyMinHook::Backend{}};bool initialized{};nativeCopyConfiguration::Snapshot configuration;std::array<void*,51> targets{};std::array<std::array<unsigned char,32>,51> bytes;
    nativeCopyEndpoints::Barrier previousBarrier=nativeCopyEndpoints::originalBarrier;nativeCopyEndpoints::Copy previousCopy=nativeCopyEndpoints::originalCopy;
    void install(ID3D12GraphicsCommandList* list){require(MH_Initialize()==MH_OK,"full copy initialize failed");initialized=true;auto bindings=candidateCommandBindings(list);for(size_t i=0;i<51;++i){targets[i]=bindings[i].target;std::memcpy(bytes[i].data(),targets[i],32);}nativeCopyEndpoints::originalBarrier=previousBarrier;nativeCopyEndpoints::originalCopy=previousCopy;require(batch.install(bindings),"full copy table install failed");nativeCopyForwarders::dispatch=nativeCopyEndpoints::intervening;}
    bool stop(){if(!initialized)return true;if(!nativeCopyConfiguration::release(batch,configuration,true))return false;bool restored=true;for(size_t i=0;i<51;++i)restored&=!std::memcmp(bytes[i].data(),targets[i],32);restored&=MH_Uninitialize()==MH_OK;initialized=false;return restored;}
    ~InstalledCopyEndpoints(){if(!stop())std::terminate();}
};
#endif
#endif
// Suspect-layout recordings are never submitted; absence of a recording-time
// message does not establish execution validity. Some debug
// wrappers also raise this structured validation exception synchronously.
static bool recordNegative(ID3D12GraphicsCommandList* list,const D3D12_TEXTURE_COPY_LOCATION* destination,const D3D12_TEXTURE_COPY_LOCATION* source,const D3D12_RESOURCE_BARRIER* barrier){
    __try{if(barrier)list->ResourceBarrier(1,barrier);list->CopyTextureRegion(destination,0,0,0,source,nullptr);list->Close();return false;}
    __except(GetExceptionCode()==0x87d?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}
}
int main(){try{
#ifdef DEMENTIUM_SCALAR_PROFILE_COPY
    constexpr unsigned imageWidth=257,imageHeight=1,rowPitch=1280;constexpr auto imageFormat=DXGI_FORMAT_R32_FLOAT;
#else
    constexpr unsigned imageWidth=8,imageHeight=8,rowPitch=256;constexpr auto imageFormat=DXGI_FORMAT_R8G8B8A8_UNORM;
#endif
    constexpr unsigned bufferBytes=imageHeight*rowPitch;
    ComPtr<ID3D12Debug> debug;check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();
    ComPtr<IDXGIFactory4> factory;check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));ComPtr<IDXGIAdapter> warp;check(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));ComPtr<ID3D12Device> device;check(D3D12CreateDevice(warp.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));ComPtr<ID3D12InfoQueue> messages;check(device.As(&messages));
    auto resource=[&](bool texture,D3D12_HEAP_TYPE heapType,D3D12_RESOURCE_STATES state){
        D3D12_HEAP_PROPERTIES heap{};heap.Type=heapType;D3D12_RESOURCE_DESC description{};description.Dimension=texture?D3D12_RESOURCE_DIMENSION_TEXTURE2D:D3D12_RESOURCE_DIMENSION_BUFFER;description.Width=texture?imageWidth:bufferBytes;description.Height=texture?imageHeight:1;description.DepthOrArraySize=description.MipLevels=1;description.SampleDesc.Count=1;description.Format=texture?imageFormat:DXGI_FORMAT_UNKNOWN;description.Layout=texture?D3D12_TEXTURE_LAYOUT_UNKNOWN:D3D12_TEXTURE_LAYOUT_ROW_MAJOR;ComPtr<ID3D12Resource> result;check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&description,state,nullptr,IID_PPV_ARGS(&result)));return result;
    };
    auto upload=resource(false,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ),readback=resource(false,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
    void* mapped{};D3D12_RANGE none{};check(upload->Map(0,&none,&mapped));std::memset(mapped,0,bufferBytes);for(unsigned y=0;y<imageHeight;++y)for(unsigned x=0;x<imageWidth;++x){auto pixel=static_cast<unsigned char*>(mapped)+y*rowPitch+x*4;pixel[0]=static_cast<unsigned char>(x*29);pixel[1]=static_cast<unsigned char>(y*31);pixel[2]=79;pixel[3]=255;}upload->Unmap(0,nullptr);
    D3D12_TEXTURE_COPY_LOCATION fromUpload{},toReadback{};fromUpload.pResource=upload.Get();fromUpload.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;fromUpload.PlacedFootprint.Footprint={imageFormat,imageWidth,imageHeight,1,rowPitch};toReadback=fromUpload;toReadback.pResource=readback.Get();
    for(unsigned variant=0;variant<4;++variant){const bool sourceCase=(variant&1)!=0,explicitBarrier=variant>=2;
        auto texture=resource(true,D3D12_HEAP_TYPE_DEFAULT,explicitBarrier?D3D12_RESOURCE_STATE_COMMON:(sourceCase?D3D12_RESOURCE_STATE_COPY_SOURCE:D3D12_RESOURCE_STATE_COPY_DEST));ComPtr<ID3D12CommandAllocator> allocator;check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY,IID_PPV_ARGS(&allocator)));ComPtr<ID3D12GraphicsCommandList> list;check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_COPY,allocator.Get(),nullptr,IID_PPV_ARGS(&list)));
        D3D12_TEXTURE_COPY_LOCATION location{};location.pResource=texture.Get();location.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition={texture.Get(),0,D3D12_RESOURCE_STATE_COMMON,sourceCase?D3D12_RESOURCE_STATE_COPY_SOURCE:D3D12_RESOURCE_STATE_COPY_DEST};messages->ClearStoredMessages();const bool exception=recordNegative(list.Get(),sourceCase?&toReadback:&location,sourceCase?&location:&fromUpload,explicitBarrier?&barrier:nullptr);
        unsigned matched{};for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T bytes{};check(messages->GetMessage(i,nullptr,&bytes));std::vector<unsigned char> data(bytes);auto message=reinterpret_cast<D3D12_MESSAGE*>(data.data());check(messages->GetMessage(i,message,&bytes));if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR){std::printf("copy_layout_observed_error id=%u %s\n",static_cast<unsigned>(message->ID),message->pDescription);require(static_cast<unsigned>(message->ID)==1334,"unexpected negative-copy validation error");require(std::strstr(message->pDescription,sourceCase?"source resource":"destination resource")!=nullptr,"negative copy reported wrong side");++matched;}}
        if(explicitBarrier)require(matched>0,"native explicit-COPY-barrier path did not reproduce1334");std::printf("copy_layout_recording_observation side=%s explicit_copy_barrier=%d id1334_matches=%u validation_exception=%d never_submitted=1\n",sourceCase?"source":"destination",explicitBarrier,matched,exception);
        // Recording/Close validation is characterized only; never Execute this case.
    }
    messages->ClearStoredMessages();
    D3D12_COMMAND_QUEUE_DESC queueDescription{};queueDescription.Type=D3D12_COMMAND_LIST_TYPE_COPY;ComPtr<ID3D12CommandQueue> copyQueue;check(device->CreateCommandQueue(&queueDescription,IID_PPV_ARGS(&copyQueue)));queueDescription.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;ComPtr<ID3D12CommandQueue> directQueue;check(device->CreateCommandQueue(&queueDescription,IID_PPV_ARGS(&directQueue)));ComPtr<ID3D12Fence> copyFence,directFence;check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&copyFence)));check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&directFence)));UINT64 copyValue{};
    auto waitCopy=[&]{check(copyQueue->Signal(copyFence.Get(),++copyValue));HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);require(event!=nullptr,"copy event unavailable");auto result=copyFence->SetEventOnCompletion(copyValue,event);if(FAILED(result)){CloseHandle(event);check(result);}auto waited=WaitForSingleObject(event,10000);CloseHandle(event);require(waited==WAIT_OBJECT_0&&copyFence->GetCompletedValue()==copyValue,"copy completion failed");};
    auto recordFullCopy=[&](ID3D12GraphicsCommandList* list,ID3D12CommandAllocator* recordAllocator,ID3D12Resource* texture,bool uploading,UINT64 generation,bool mismatchEnd=false,bool missingEndContext=false,bool interruptWithMarker=false){
        auto textureDesc=texture->GetDesc();D3D12_TEXTURE_COPY_LOCATION image{};image.pResource=texture;image.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;auto source=uploading?fromUpload:image,destination=uploading?image:toReadback;const auto& linear=uploading?source:destination;
        D3D12_HEAP_PROPERTIES heap{};D3D12_HEAP_FLAGS flags{};check(linear.pResource->GetHeapProperties(&heap,&flags));D3D12_PLACED_SUBRESOURCE_FOOTPRINT expected{};UINT rows{};UINT64 rowBytes{},total{};device->GetCopyableFootprints(&textureDesc,0,1,linear.PlacedFootprint.Offset,&expected,&rows,&rowBytes,&total);require(expected.Footprint.RowPitch==linear.PlacedFootprint.Footprint.RowPitch&&rows==imageHeight&&rowBytes!=0,"device footprint facts differ");
        nativeCopyPreflight::BufferProof buffer;buffer.known=true;buffer.identity=linear.pResource;buffer.description=linear.pResource->GetDesc();buffer.heap=heap.Type;buffer.rowBytesKnown=true;buffer.rowBytes=rowBytes;
        D3D12_BOX box{0,0,0,imageWidth,imageHeight,1};std::array<unsigned char,512> frame{};std::memcpy(frame.data()+(uploading?0x48:0x20),&destination,sizeof destination);std::memcpy(frame.data()+(uploading?0x78:0xd0),&source,sizeof source);std::memcpy(frame.data()+(uploading?0x20:0x58),&box,sizeof box);auto address=reinterpret_cast<std::uintptr_t>(frame.data());auto reader=[&](std::uintptr_t at,void* output,size_t bytes){if(at<address||at-address>frame.size()||bytes>frame.size()-(at-address))return false;std::memcpy(output,frame.data()+at-address,bytes);return true;};
        D3D12_RESOURCE_BARRIER begin{};begin.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;begin.Transition={texture,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COMMON,uploading?D3D12_RESOURCE_STATE_COPY_DEST:D3D12_RESOURCE_STATE_COPY_SOURCE};
        const std::uintptr_t syntheticImage=0x100000000ULL;bool corruptCode=false,wrongNativeList=false;
        auto pinnedReader=[&](std::uintptr_t at,void* output,size_t bytes){if(at==syntheticImage+0x6bb740&&bytes==sizeof list){auto value=wrongNativeList?nullptr:list;std::memcpy(output,&value,bytes);return true;}auto copyCode=[&](std::uintptr_t rva,const auto& code){auto start=syntheticImage+rva;if(at<start||at-start>code.size()||bytes>code.size()-(at-start))return false;std::memcpy(output,code.data()+(at-start),bytes);if(corruptCode&&at==syntheticImage+nativeCopyCode::readbackRva&&bytes)static_cast<unsigned char*>(output)[0]^=1;return true;};return copyCode(nativeCopyCode::readbackRva,nativeCopyCode::readback)||copyCode(nativeCopyCode::uploadRva,nativeCopyCode::upload)||reader(at,output,bytes);};
        auto context=nativeCopyContext::capture(syntheticImage,syntheticImage+(uploading?0x1eabf1:0x1ea8eb),list,1,&begin,address,pinnedReader);require(context.valid&&context.texture==texture&&context.staging==linear.pResource,"native identity extraction rejected");
        wrongNativeList=true;require(!nativeCopyContext::capture(syntheticImage,syntheticImage+context.callerRva,list,1,&begin,address,pinnedReader).valid,"different native list admitted");wrongNativeList=false;
        require(!nativeCopyContext::capture(syntheticImage,syntheticImage+context.callerRva,list,1,&begin,0,pinnedReader).valid,"missing native frame admitted");
        auto integrated=nativeCopyAdmission::capturePinned(syntheticImage,device.Get(),list,list,context.texture,context.staging,context.callerRva,1,&begin,address,pinnedReader);
        require(integrated.valid,"integrated actual-device admission rejected");
        auto prepared=nativeCopyPrepare::capture(syntheticImage,syntheticImage+context.callerRva,device.Get(),list,1,&begin,address,pinnedReader);require(prepared.plan.valid&&prepared.owners.list.Get()==list&&prepared.owners.source.Get()==source.pResource&&prepared.owners.destination.Get()==destination.pResource,"owned native preparation rejected");integrated=prepared.plan;
        corruptCode=true;require(!nativeCopyAdmission::capturePinned(syntheticImage,device.Get(),list,list,texture,linear.pResource,uploading?0x1eabf1:0x1ea8eb,1,&begin,address,pinnedReader).valid,"changed native helper admitted");corruptCode=false;
        require(!nativeCopyAdmission::capture(false,device.Get(),list,list,texture,linear.pResource,uploading?0x1eabf1:0x1ea8eb,1,&begin,address,reader).valid,"unverified code admitted");
        require(!nativeCopyAdmission::capture(true,device.Get(),nullptr,list,texture,linear.pResource,uploading?0x1eabf1:0x1ea8eb,1,&begin,address,reader).valid,"unknown native list admitted");
        require(!nativeCopyAdmission::capture(true,device.Get(),list,list,texture,linear.pResource,0,1,&begin,address,reader).valid,"unknown caller admitted");
        // Native frame/caller identity is simulated; heap/footprint/resource
        // facts and the executed copy below are actual WARP D3D12 objects.
        (void)generation;(void)recordAllocator;nativeCopyRuntime::Runtime runtime;require(runtime.created(list)&&runtime.arm(5,10,128),"runtime creation/arming failed");unsigned barriersEmitted{},copiesEmitted{};
        auto originalBarrier=[&](UINT count,const D3D12_RESOURCE_BARRIER* values){barriersEmitted+=count;
#ifdef DEMENTIUM_ALL_COMMAND_HOOKS
            resourceBarriersHook(list,count,values);
#elif defined(DEMENTIUM_INSTALLED_COPY_ENDPOINTS)
            endpointNativeBarrier(list,count,values);
#else
            list->ResourceBarrier(count,values);
#endif
        };
        auto originalCopy=[&](const D3D12_TEXTURE_COPY_LOCATION* dst,UINT x,UINT y,UINT z,const D3D12_TEXTURE_COPY_LOCATION* src,const D3D12_BOX* region){require(!runtime.pending(list),"runtime pending during native emission");++copiesEmitted;
#ifdef DEMENTIUM_ALL_COMMAND_HOOKS
            textureCopyHook(list,dst,x,y,z,src,region);
#elif defined(DEMENTIUM_INSTALLED_COPY_ENDPOINTS)
            endpointNativeCopy(list,dst,x,y,z,src,region);
#else
            list->CopyTextureRegion(dst,x,y,z,src,region);
#endif
        };
        EndpointFixture endpointFixture;endpointBarrierPayload=&originalBarrier;endpointCopyPayload=&originalCopy;endpointList=list;endpointObservations=0;nativeCopyEndpoints::runtime=&runtime;
        nativeCopyEndpoints::originalBarrier=+[](ID3D12GraphicsCommandList* incoming,UINT n,const D3D12_RESOURCE_BARRIER* values){
#ifdef DEMENTIUM_INSTALLED_COPY_ENDPOINTS
            if(incoming!=endpointList){
#ifdef DEMENTIUM_ALL_COMMAND_HOOKS
                resourceBarriersHook(incoming,n,values);
#else
                endpointNativeBarrier(incoming,n,values);
#endif
                return;}
#endif
            require(incoming==endpointList,"endpoint barrier list changed");(*static_cast<decltype(originalBarrier)*>(endpointBarrierPayload))(n,values);};
        nativeCopyEndpoints::originalCopy=+[](ID3D12GraphicsCommandList* incoming,const D3D12_TEXTURE_COPY_LOCATION* dst,UINT x,UINT y,UINT z,const D3D12_TEXTURE_COPY_LOCATION* src,const D3D12_BOX* region){
#ifdef DEMENTIUM_INSTALLED_COPY_ENDPOINTS
            if(incoming!=endpointList){
#ifdef DEMENTIUM_ALL_COMMAND_HOOKS
                textureCopyHook(incoming,dst,x,y,z,src,region);
#else
                endpointNativeCopy(incoming,dst,x,y,z,src,region);
#endif
                return;}
#endif
            require(incoming==endpointList,"endpoint copy list changed");(*static_cast<decltype(originalCopy)*>(endpointCopyPayload))(dst,x,y,z,src,region);};
        endpointContext={syntheticImage,syntheticImage+context.callerRva,address,device.Get(),10,+[](std::uintptr_t at,void* output,size_t bytes,void* payload){return (*static_cast<decltype(pinnedReader)*>(payload))(at,output,bytes);},&pinnedReader};nativeCopyEndpoints::contextFactory=+[](const nativeCopyEndpoints::Observation& observation){
#ifdef DEMENTIUM_INSTALLED_COPY_ENDPOINTS
            if(observation.list!=endpointList){++endpointNestedObservations;return nativeCopyEndpoints::Context{};}
#endif
            require(observation.caller&&observation.list==endpointList&&observation.frame==endpointObservedFrame,"endpoint observation changed");++endpointObservations;return endpointContext;};
#ifdef DEMENTIUM_INSTALLED_COPY_ENDPOINTS
#ifndef DEMENTIUM_ALL_COMMAND_HOOKS
        nativeCopyEndpoints::originalClose=+[](ID3D12GraphicsCommandList* incoming){return endpointNativeClose(incoming);};nativeCopyEndpoints::originalReset=+[](ID3D12GraphicsCommandList* incoming,ID3D12CommandAllocator* allocator,ID3D12PipelineState* pipeline){return endpointNativeReset(incoming,allocator,pipeline);};
#endif
        InstalledCopyEndpoints installed;endpointNestedObservations=0;installed.install(list);check(list->Close());check(list->Reset(recordAllocator,nullptr));
        auto callBarrier=[&](const D3D12_RESOURCE_BARRIER* value){list->ResourceBarrier(1,value);};auto callCopy=[&]{list->CopyTextureRegion(&destination,0,0,0,&source,&box);};
#else
        auto callBarrier=[&](const D3D12_RESOURCE_BARRIER* value){nativeCopyEndpoints::barrier(list,1,value);};auto callCopy=[&]{nativeCopyEndpoints::copy(list,&destination,0,0,0,&source,&box);};
#endif
#ifndef DEMENTIUM_INSTALLED_COPY_ENDPOINTS
        nativeCopyConfiguration::Snapshot configuration;
#endif
        endpointObservedFrame=reinterpret_cast<std::uintptr_t>(&begin);callBarrier(&begin);require(runtime.pending(list)&&runtime.pendingRecordings()==1&&!barriersEmitted&&!copiesEmitted,"endpoint Begin failed");
#ifdef DEMENTIUM_ALL_COMMAND_HOOKS
        require(!installed.stop()&&installed.batch.installed()&&runtime.pendingRecordings()==1,"pending shutdown removed active copy hooks");
#endif
#ifndef DEMENTIUM_INSTALLED_COPY_ENDPOINTS
        require(!configuration.restore(true,true)&&nativeCopyEndpoints::runtime==&runtime&&runtime.pendingRecordings()==1,"configuration restored while native prefix held");
#endif
        runtime.cancel();endpointContext.caller=syntheticImage+integrated.copyReturnRva;endpointContext.now=20;endpointObservedFrame=0;callCopy();require(runtime.pending(list)&&!barriersEmitted&&!copiesEmitted,"copy emitted before End validation");
        if(interruptWithMarker){
#ifdef DEMENTIUM_ALL_COMMAND_HOOKS
            list->SetMarker(0,nullptr,0);
#else
            unsigned invoked{};nativeCopyEndpoints::intervening(list,+[](void* payload){++*static_cast<unsigned*>(payload);},&invoked);require(invoked==1,"intervening callback count changed");
#endif
            require(!runtime.pending(list)&&barriersEmitted==1&&copiesEmitted==1,"intervening method failed to flush original prefix");
        }
        // Test-only buffered correction: no command is emitted until a complete
        // matching End, even though admission expired during the transaction.
        auto end=begin;end.Transition.StateBefore=begin.Transition.StateAfter;end.Transition.StateAfter=D3D12_RESOURCE_STATE_COMMON;endpointContext.caller=syntheticImage+integrated.endReturnRva+(mismatchEnd?1:0);endpointContext.now=30;if(missingEndContext)endpointContext={};endpointObservedFrame=reinterpret_cast<std::uintptr_t>(&end);callBarrier(&end);const bool handled=!barriersEmitted;
        require(!runtime.pending(list)&&!runtime.pendingRecordings()&&endpointObservations==3,"endpoint runtime completion failed");
#ifndef DEMENTIUM_INSTALLED_COPY_ENDPOINTS
        require(configuration.restore(true,true),"quiescent completed configuration rejected");
#endif
        if(mismatchEnd||interruptWithMarker)require(barriersEmitted==2&&copiesEmitted==1,"unmatched dispatch did not replay and forward original barriers");else require(handled&&copiesEmitted==1,"buffered dispatch failed");
#ifdef DEMENTIUM_INSTALLED_COPY_ENDPOINTS
#ifdef DEMENTIUM_ALL_COMMAND_HOOKS
        auto tracked=stereo::lists.find(list);require(tracked&&tracked->textureCopies.count==1&&tracked->barriers.count==barriersEmitted,"actual legacy copy/barrier tracking was not preserved");std::puts("installed_copy_table slots=51 legacy_copy_records=1 legacy_barrier_records_match=1");
#endif
        require(installed.stop(),"copy endpoint restoration failed");std::printf("installed_copy_endpoints nested_observations=%u targets_restored=1\n",endpointNestedObservations);
#endif
        std::printf("copy_preflight_gpu role=%s device_row_bytes=%llu complete_match=%d simulated_native_context=1\n",uploading?"upload":"readback",rowBytes,handled);
    };
    for(bool uploading:{true,false})for(unsigned failureKind:{0u,1u,2u}){
        const bool missingContext=failureKind==1,interrupt=failureKind==2;
        auto texture=resource(true,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COMMON);ComPtr<ID3D12CommandAllocator> allocator;check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY,IID_PPV_ARGS(&allocator)));ComPtr<ID3D12GraphicsCommandList> list;check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_COPY,allocator.Get(),nullptr,IID_PPV_ARGS(&list)));messages->ClearStoredMessages();recordFullCopy(list.Get(),allocator.Get(),texture.Get(),uploading,1,!interrupt,missingContext,interrupt);check(list->Close());unsigned matched{};
        for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T bytes{};check(messages->GetMessage(i,nullptr,&bytes));std::vector<unsigned char> data(bytes);auto message=reinterpret_cast<D3D12_MESSAGE*>(data.data());check(messages->GetMessage(i,message,&bytes));if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR){require(static_cast<unsigned>(message->ID)==1334,"unexpected replay validation error");++matched;}}
        require(matched==1,"original1334 not preserved by unmatched replay");std::printf("copy_transaction_unmatched role=%s missing_context=%d interrupted=%d original_id1334_preserved=1 never_submitted=1\n",uploading?"upload":"readback",missingContext,interrupt);
        // The restored native-error recordings are deliberately NEVER submitted.
    }
    messages->ClearStoredMessages();
    for(unsigned released=0;released<2;++released){
        auto texture=resource(true,D3D12_HEAP_TYPE_DEFAULT,released?D3D12_RESOURCE_STATE_COPY_DEST:D3D12_RESOURCE_STATE_COMMON);
        ComPtr<ID3D12CommandAllocator> directAllocator;ComPtr<ID3D12GraphicsCommandList> directList;
        if(released){
            check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&directAllocator)));check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,directAllocator.Get(),nullptr,IID_PPV_ARGS(&directList)));D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition={texture.Get(),0,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_COMMON};directList->ResourceBarrier(1,&barrier);check(directList->Close());ID3D12CommandList* submitted[]={directList.Get()};directQueue->ExecuteCommandLists(1,submitted);check(directQueue->Signal(directFence.Get(),1));check(copyQueue->Wait(directFence.Get(),1));
        }
        ComPtr<ID3D12CommandAllocator> allocator;check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY,IID_PPV_ARGS(&allocator)));ComPtr<ID3D12GraphicsCommandList> list;check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_COPY,allocator.Get(),nullptr,IID_PPV_ARGS(&list)));D3D12_TEXTURE_COPY_LOCATION location{};location.pResource=texture.Get();location.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        recordFullCopy(list.Get(),allocator.Get(),texture.Get(),true,1+released*2);check(list->Close());ID3D12CommandList* submitted[]={list.Get()};copyQueue->ExecuteCommandLists(1,submitted);waitCopy();
        check(allocator->Reset());check(list->Reset(allocator.Get(),nullptr));recordFullCopy(list.Get(),allocator.Get(),texture.Get(),false,2+released*2);check(list->Close());copyQueue->ExecuteCommandLists(1,submitted);waitCopy();
        check(readback->Map(0,nullptr,&mapped));auto pixels=static_cast<const unsigned char*>(mapped);for(unsigned y=0;y<imageHeight;++y)for(unsigned x=0;x<imageWidth;++x){auto pixel=pixels+y*rowPitch+x*4;require(pixel[0]==static_cast<unsigned char>(x*29)&&pixel[1]==static_cast<unsigned char>(y*31)&&pixel[2]==79&&pixel[3]==255,"copy roundtrip texels differ");}readback->Unmap(0,&none);
        std::printf("copy_layout_positive initial=%s roundtrip_pixels=%u format=%u no_copy_barriers=1\n",released?"COPY_DEST_released_on_DIRECT_then_queue_wait":"COMMON",imageWidth*imageHeight,unsigned(imageFormat));
    }
    for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T bytes{};check(messages->GetMessage(i,nullptr,&bytes));std::vector<unsigned char> data(bytes);auto message=reinterpret_cast<D3D12_MESSAGE*>(data.data());check(messages->GetMessage(i,message,&bytes));if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(message->pDescription);}
    std::puts("PASS: COPY recording/Close validation characterized without submission; COMMON promotion/decay and synchronized DIRECT release preserve texels on WARP; no game/XR or invalid GPU submission.");return 0;
}catch(const std::exception& error){std::puts(error.what());return 1;}}












