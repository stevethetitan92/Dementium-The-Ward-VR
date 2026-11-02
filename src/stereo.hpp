#include "ppm_capture.hpp"
#include "async_ppm_capture.hpp"
#ifdef DEMENTIUM_PROFILE_DRAWS
#include "gpu_batch_profile.hpp"
#include "native_gpu_profile.hpp"
#endif
#include "stereo_presentation.hpp"
#include "native_ui_text_policy.hpp"
#include "particle_fog_policy.hpp"
#include "particle_fog_depth.hpp"
#pragma once
#include "mod_preferences.hpp"
#include "xr_session.hpp"
#include "swapchain_queue.hpp"
#include "device_ownership.hpp"
#include "head_light_constants.hpp"
#include "sniper_scope.hpp"
#include "native_scope_hud.hpp"
#include "optic_scope.hpp"
#include "native_hand_clip.hpp"
#include "selected_weapon_draw.hpp"
#include "head_reference.hpp"
#include "native_fog.hpp"
#include "geometry_isolation.hpp"
#include "upload_capture.hpp"
#include "resource_capture_lifetime.hpp"
#include "fence_completion.hpp"
#include "descriptor_bindings.hpp"
#include "command_list_states.hpp"
#include "copy_provenance.hpp"
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <shared_mutex>
#include <map>
#include <atomic>
#include <functional>
#include <array>
#include <cmath>
#include <unordered_set>
#include <unordered_map>
namespace stereo {
using ExtraSceneDraw=void(*)(ID3D12Device*,ID3D12GraphicsCommandList*,unsigned,D3D12_CPU_DESCRIPTOR_HANDLE,D3D12_CPU_DESCRIPTOR_HANDLE,const XrView&,unsigned,unsigned);
inline bool(*extraUIDraw)(ID3D12Device*,ID3D12GraphicsCommandList*,ID3D12Resource*,unsigned,unsigned){};
inline bool(*extraUIVisible)(){};
inline bool(*gameplayActive)(){};
inline void(*presentationObserved)(bool,bool,bool){};
inline bool(*extraUISuppressFade)(){};
inline void(*extraUICapture)(ID3D12Device*,ID3D12GraphicsCommandList*,ID3D12Resource*){};
inline bool(*nativeUiGameplay)(){};
inline void(*nativeUiReloadSuppressed)(){};
inline bool(*hideNativeStatus)(const float*,unsigned,unsigned,unsigned){};
inline ExtraSceneDraw extraSceneDraw{};inline void(*prepareSceneExtras)(){};
inline void(*warmSceneResources)(ID3D12Device*,ID3D12GraphicsCommandList*,unsigned){};
using Microsoft::WRL::ComPtr;
using namespace DirectX;
struct SpriteElement {UINT slot{},offset{},step{};DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;};
struct SpriteLayout {SpriteElement position,rows[4],color,colorRows[4],colorBias;bool valid=false;SpriteElement uv;};
struct PipelineInfo {unsigned constants=0; DXGI_FORMAT color{},depth{}; unsigned samples=0;bool sprite=false;ComPtr<ID3D12PipelineState> uiPipeline;bool usesTexture=true;UINT textureMask=1;SpriteLayout layout;bool opaqueSprite=false;ComPtr<ID3D12PipelineState> handPipeline;bool headLightLayout=false;ComPtr<ID3D12PipelineState> fogPipeline,handFogPipeline;std::array<bool,2> unusedStageCBV{};ComPtr<ID3D12PipelineState> promptPipeline;bool particleFogLayout=false;ComPtr<ID3D12PipelineState> particleFogPipeline;unsigned sampleQuality{};ComPtr<ID3D12PipelineState> stereoBase,stereoHand,stereoFog,stereoHandFog,stereoParticle;};
struct ListState : descriptorBindings::State {
#ifdef DEMENTIUM_PROFILE_DRAWS
    unsigned diagnosticWorldDraws{};
#endif
    ID3D12RootSignature* rootSignature{};
    D3D12_GPU_VIRTUAL_ADDRESS pixelCBV{};
    ID3D12PipelineState* pipeline{}; D3D12_GPU_VIRTUAL_ADDRESS cbv{};
    bool deviceChecked{},gameDeviceOwned{};
    UINT rtvCount{}; std::array<D3D12_CPU_DESCRIPTOR_HANDLE,8> rtvs{};
    D3D12_CPU_DESCRIPTOR_HANDLE dsv{}; bool hasDepth=false;
    UINT viewportCount{}; std::array<D3D12_VIEWPORT,16> viewports{};
    UINT scissorCount{}; std::array<D3D12_RECT,16> scissors{};
    struct Heap {UINT64 gpu{};SIZE_T cpu{};UINT count{},stride{};};
    std::vector<Heap> heaps;
    std::array<D3D12_VERTEX_BUFFER_VIEW,32> vertices{};
    D3D12_INDEX_BUFFER_VIEW indices{};
    copyProvenance::History textureCopies;
    copyProvenance::BarrierHistory barriers;
};
// Diagnostic flags are protected by mapMutex, like the capture itself. They
// describe capture provenance/consumption, not GPU lifetime or eviction safety.
struct Upload : UploadCapture {ComPtr<ID3D12Resource> resource;ComPtr<IUnknown> deviceIdentity;UINT64 lifetimeToken{};bool lifetimeTracked{};bool observedMap{},observedCopy{},observedRead{};};
static std::shared_mutex gate;
static std::mutex mapMutex;
static std::unordered_map<ID3D12PipelineState*,PipelineInfo> pipelineInfo;
static std::unordered_map<ID3D12RootSignature*,std::array<bool,2>> stageOnlyCBV;
static std::map<UINT64,Upload> uploads;
static std::atomic<bool> captureRegistryAlive{true};
static UINT64 captureLifetimeSerial{}; // protected by mapMutex
struct CaptureRegistryShutdown {~CaptureRegistryShutdown(){captureRegistryAlive=false;}};
static CaptureRegistryShutdown captureRegistryShutdown;
// Only shadow-backed, unmapped captures can relinquish their resource pin.
// Destruction removes their metadata. No TTL, GPU fence guess, or dangling CPU
// mapping is used. Captures already consumed by replay retain their existing pin.
static void trackCaptureLifetime(UINT64 address,Upload& upload,ID3D12Resource* resource){
    if(upload.lifetimeTracked)return;
    ComPtr<ID3D12Device> owner;
    if(FAILED(resource->GetDevice(IID_PPV_ARGS(&owner)))||FAILED(owner.As(&upload.deviceIdentity)))return;
    const auto token=upload.lifetimeToken=++captureLifetimeSerial;
    upload.lifetimeTracked=resourceCaptureLifetime::attach(resource,[address,token]{
        if(!captureRegistryAlive.load())return;
        std::lock_guard<std::mutex> guard(mapMutex);
        auto it=uploads.find(address);
        if(it!=uploads.end()&&it->second.lifetimeToken==token&&!it->second.resource)uploads.erase(it);
    });
}
static std::unordered_map<SIZE_T,ID3D12Resource*> renderTargetResources;
static std::unordered_set<ID3D12Resource*> backbuffers;
// Descriptor metadata follows CPU-to-GPU heap copies. Render-target samples
// are desktop scene composites; ordinary texture assets are native UI/effects.
static std::unordered_map<SIZE_T,ID3D12Resource*> sampledResources;
struct SampledViewInfo {D3D12_RESOURCE_DESC resource{};D3D12_SHADER_RESOURCE_VIEW_DESC view{};bool resourceKnown{},viewKnown{};};
// Descriptor-time value snapshots let diagnostics inspect layouts without
// dereferencing a non-owning resource identity after its lifetime ends.
static std::unordered_map<SIZE_T,SampledViewInfo> sampledViews;
static std::unordered_set<ID3D12Resource*> sceneTextures;
static std::unordered_set<ID3D12Resource*> capturedUITextures;
static std::unordered_set<ID3D12Resource*> vignetteTextures;
static std::unordered_map<ID3D12Resource*,std::array<float,4>> solidTexels;
static float pendingFadeAlpha{},cachedFadeAlpha{};
static stereoPresentation::HandoffFade presentationFade;
static const GUID backbufferTag={0x51b7a9d1,0x3e29,0x4764,{0x8c,0x16,0xbb,0x42,0x86,0x4c,0xa1,0x79}};
static UINT desktopWidth=1280,desktopHeight=720;
static commandListStates::Registry<ID3D12GraphicsCommandList*,ListState> lists;
static ComPtr<ID3D12CommandQueue> gameQueue;
static ComPtr<ID3D12CommandQueue> xrQueue;
static ComPtr<ID3D12Device> gameDevice;
static XrSessionBridge* xr{};
static ComPtr<ID3D12Resource> eyeColor[2],depth[2],resolved[2],constants;
static ComPtr<ID3D12Resource> uiColor,uiDepth,uiCache;
static bool haveUIFrame=false;
static constexpr unsigned sceneSamples=2;
static ComPtr<ID3D12DescriptorHeap> rtHeap,dsHeap;
static D3D12_CPU_DESCRIPTOR_HANDLE rtvs[2],dsvs[2];
static D3D12_CPU_DESCRIPTOR_HANDLE uiRTV,uiDSV;
static ComPtr<ID3D12CommandAllocator> allocator;
static ComPtr<ID3D12GraphicsCommandList> commands;
static ComPtr<ID3D12Fence> fence;
static HANDLE fenceEvent{};
static unsigned char* constantsCPU{};
static std::atomic<UINT64> cursor{};
static constexpr UINT64 capacity=16*1024*1024;
static UINT64 fenceValue{};
static double gpuWaitMilliseconds{},xrWaitMilliseconds{};
static double lastXrBeginMilliseconds{},lastWorldCompletionMilliseconds{};
#ifdef DEMENTIUM_PROFILE_DRAWS
static double nativeQueueWaitMilliseconds{},ownedQueueWaitMilliseconds{},maxQueueWaitMilliseconds{};
static unsigned nativeQueueWaitCalls{},ownedQueueWaitCalls{};
static double worldCompletionWaitMilliseconds{};
static nativeGpuProfile::Timer<> nativeGpuTimer;
static bool nativeGpuProfiling(){static const bool enabled=[](){wchar_t v[8]{};return GetEnvironmentVariableW(L"DEMENTIUM_PROFILE_NATIVE_GPU",v,8)==1&&v[0]==L'1';}();return enabled;}
#endif
static double preciseMilliseconds(){LARGE_INTEGER time,frequency;QueryPerformanceCounter(&time);QueryPerformanceFrequency(&frequency);return 1000.*time.QuadPart/frequency.QuadPart;}
static std::atomic<unsigned> eyeDraws{};
#ifdef DEMENTIUM_PROFILE_DRAWS
static unsigned freshWorldTicks{},cachedWorldTicks{},nonGameplayTicks{},cachedWithExecutedWorld{};
// Fixed-size diagnostic sequence: distinguish repeated presentation from
// missing stereo interception without allocating or logging per frame.
static char worldCadence[301]{};
static unsigned worldCadenceCount{};
static void* worldCallerStacks[2][24]{};
static unsigned short worldCallerDepth[2]{};
static std::atomic<unsigned> executedWorldDraws{},unknownExecutedLists{};
#endif
static std::atomic<unsigned> particleDraws{},particleFogDraws{};
static std::atomic<unsigned> uiDraws{};
static std::atomic<bool> worldSeen{},compositeSkipped{},captureRequested{},recenterRequested{};
static void(*captureObserved)(unsigned,bool,unsigned,std::uint64_t){};
static std::atomic<unsigned> compositePasses{};
static unsigned lastUIDraws{};
static XrView lastWorldViews[2]={{XR_TYPE_VIEW},{XR_TYPE_VIEW}};
static bool haveWorldFrame=false;
static ULONGLONG lastWorldTicks{},lastUITicks{};
static bool submittedWorld=false,submittedUI=false;
static std::atomic<bool> uiOpaqueDraws{};
static bool cachedUIOpaque=false;
static std::atomic<unsigned> skips[8]{};
static std::atomic<unsigned> directDraws{},bundleDraws{};
static bool active=false,failed=false;
static const char* stage="idle";
static unsigned frames{};
static XrPosef origin{};
static bool originSet=false;
static headReference::Startup originStartup;
static headReference::Postures postures;
static std::atomic<unsigned> postureCalibration{};
static unsigned referenceRebases{};
static float unitsPerMeter=1.0f;
static float frameScopeFactor=1.f;
static bool drawScopeReticle();
static std::function<void(const ListState&,UINT,const float*,const float*)> uiTrace;
static std::function<void(const ListState&,const PipelineInfo&,UINT)> uiFailureTrace;
static XrPosef recenterOrigin(const XrView& left,const XrView& right) {
    return headReference::center(left,right);
}
static void hr(HRESULT r) {if(FAILED(r)) throw std::runtime_error("stereo D3D12 failure "+std::to_string(r));}
static D3D12_RESOURCE_DESC textureDesc(UINT width,UINT height,DXGI_FORMAT format,D3D12_RESOURCE_FLAGS flags) {
    D3D12_RESOURCE_DESC d{}; d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=width;d.Height=height;
    d.DepthOrArraySize=1;d.MipLevels=1;d.Format=format;d.SampleDesc.Count=1;d.Flags=flags;return d;
}
static void syncGPU(ID3D12CommandQueue* queue=nullptr) {
    double started=preciseMilliseconds();
    if(!queue)queue=xrQueue.Get();
    if(fenceValue>=UINT64_MAX-1)throw std::runtime_error("stereo fence values exhausted");
    stage="queue signal";hr(queue->Signal(fence.Get(),++fenceValue));
    stage="queue completion";
    fenceCompletion::wait(fenceValue,[&]{return fence->GetCompletedValue();},
        [&](UINT64 value){hr(fence->SetEventOnCompletion(value,fenceEvent));},
        [&]{const DWORD result=WaitForSingleObject(fenceEvent,10000);
            if(result==WAIT_TIMEOUT)throw std::runtime_error("stereo GPU timeout");
            if(result!=WAIT_OBJECT_0)throw std::runtime_error("stereo GPU fence wait failed "+std::to_string(GetLastError()));},
        [&]{throw std::runtime_error("stereo GPU device removed, reason "+std::to_string(gameDevice->GetDeviceRemovedReason()));});
    const double elapsed=preciseMilliseconds()-started;
    gpuWaitMilliseconds+=elapsed;
#ifdef DEMENTIUM_PROFILE_DRAWS
    if(queue==gameQueue.Get()){nativeQueueWaitMilliseconds+=elapsed;++nativeQueueWaitCalls;}
    else {ownedQueueWaitMilliseconds+=elapsed;++ownedQueueWaitCalls;}
    if(elapsed>maxQueueWaitMilliseconds)maxQueueWaitMilliseconds=elapsed;
#endif
}
static void initialize() {
    initDiagnostics::Observation observation;initDiagnostics::mark("stereo initialization");
    stage="device fence test";
    // Verify the game-provided COM device before binding it to the runtime.
    ComPtr<ID3D12Fence> checkFence;hr(gameDevice->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&checkFence)));
    D3D12_COMMAND_QUEUE_DESC qd{};qd.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    stage="create XR queue";hr(gameDevice->CreateCommandQueue(&qd,IID_PPV_ARGS(&xrQueue)));
    xr=new XrSessionBridge;
    wchar_t diagnostic[8]{};
    bool desktopTest=GetEnvironmentVariableW(L"DEMENTIUM_DESKTOP_STEREO_TEST",diagnostic,8)&&!wcscmp(diagnostic,L"1");
    stage=desktopTest?"initialize desktop stereo diagnostic":"initialize OpenXR";
    if(desktopTest)xr->initializeDesktop(gameDevice.Get());
    else {wchar_t loader[32768]{};
        if(!GetEnvironmentVariableW(L"DEMENTIUM_OPENXR_LOADER",loader,32768)){
            HMODULE bridge{};
            if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&initialize),&bridge)||!GetModuleFileNameW(bridge,loader,32768))throw std::runtime_error("Cannot resolve bundled OpenXR loader path");
            auto separator=wcsrchr(loader,L'\\');if(!separator)throw std::runtime_error("Bridge directory unavailable");
            wcscpy_s(separator+1,32768-(separator+1-loader),L"openxr_loader.dll");
        }
        xr->initialize(loader,gameDevice.Get(),xrQueue.Get(),.7f);
    }
    xr->createOverlay(desktopWidth,desktopHeight);
    hr(gameDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
    hr(gameDevice->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&commands)));hr(commands->Close());
    hr(gameDevice->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));
    fenceEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!fenceEvent) throw std::runtime_error("stereo fence event failed");
    D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.NumDescriptors=4;hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    hr(gameDevice->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtHeap)));hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    hr(gameDevice->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&dsHeap)));
    D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;
    for(int i=0;i<2;i++) {
        auto d=textureDesc(xr->eyes[i].width,xr->eyes[i].height,DXGI_FORMAT_R8G8B8A8_UNORM,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
        d.SampleDesc.Count=sceneSamples;
        D3D12_CLEAR_VALUE clear{};clear.Format=d.Format;clear.Color[3]=1;
        hr(gameDevice->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_RENDER_TARGET,&clear,IID_PPV_ARGS(&eyeColor[i])));
        d.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        clear={};clear.Format=d.Format;clear.DepthStencil.Depth=1;
        hr(gameDevice->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_DEPTH_WRITE,&clear,IID_PPV_ARGS(&depth[i])));
        rtvs[i]=rtHeap->GetCPUDescriptorHandleForHeapStart();rtvs[i].ptr+=i*gameDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        dsvs[i]=dsHeap->GetCPUDescriptorHandleForHeapStart();dsvs[i].ptr+=i*gameDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        gameDevice->CreateRenderTargetView(eyeColor[i].Get(),nullptr,rtvs[i]);gameDevice->CreateDepthStencilView(depth[i].Get(),nullptr,dsvs[i]);
        d=textureDesc(xr->eyes[i].width,xr->eyes[i].height,DXGI_FORMAT_R8G8B8A8_UNORM,D3D12_RESOURCE_FLAG_NONE);
        hr(gameDevice->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_COPY_SOURCE,nullptr,IID_PPV_ARGS(&resolved[i])));
    }
    auto ud=textureDesc(desktopWidth,desktopHeight,DXGI_FORMAT_R8G8B8A8_UNORM,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    D3D12_CLEAR_VALUE uc{};uc.Format=ud.Format;
    hr(gameDevice->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&ud,D3D12_RESOURCE_STATE_RENDER_TARGET,&uc,IID_PPV_ARGS(&uiColor)));
    ud.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;ud.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;uc={};uc.Format=ud.Format;uc.DepthStencil.Depth=1;
    hr(gameDevice->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&ud,D3D12_RESOURCE_STATE_DEPTH_WRITE,&uc,IID_PPV_ARGS(&uiDepth)));
    uiRTV=rtHeap->GetCPUDescriptorHandleForHeapStart();uiRTV.ptr+=2*gameDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    uiDSV=dsHeap->GetCPUDescriptorHandleForHeapStart();uiDSV.ptr+=2*gameDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    gameDevice->CreateRenderTargetView(uiColor.Get(),nullptr,uiRTV);gameDevice->CreateDepthStencilView(uiDepth.Get(),nullptr,uiDSV);
    ud=textureDesc(desktopWidth,desktopHeight,DXGI_FORMAT_R8G8B8A8_UNORM,D3D12_RESOURCE_FLAG_NONE);
    hr(gameDevice->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&ud,D3D12_RESOURCE_STATE_COPY_SOURCE,nullptr,IID_PPV_ARGS(&uiCache)));
    hp.Type=D3D12_HEAP_TYPE_UPLOAD;D3D12_RESOURCE_DESC bd{};bd.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;bd.Width=capacity;bd.Height=1;bd.DepthOrArraySize=1;bd.MipLevels=1;bd.SampleDesc.Count=1;bd.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    hr(gameDevice->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&constants)));
    D3D12_RANGE none{};stage="map cloned constants";hr(constants->Map(0,&none,reinterpret_cast<void**>(&constantsCPU)));
    // An optional optic must never disable the independent stereo eye path.
    if(opticScope::enabled){
        try {if(!opticScope::initialize(gameDevice.Get(),sceneSamples)){opticScope::initializationError="Optical scope resource initialization failed";opticScope::enabled=false;}}
        catch(const std::exception& e){opticScope::initializationError=e.what();opticScope::enabled=false;}
    }
}
#ifdef DEMENTIUM_PROFILE_DRAWS
static gpuBatchProfile::Timer batchGpuTimer;
#endif
static void beginCommands() {hr(allocator->Reset());hr(commands->Reset(allocator.Get(),nullptr));
#ifdef DEMENTIUM_PROFILE_DRAWS
batchGpuTimer.begin(gameDevice.Get(),commands.Get());
#endif
}
static void submitCommands(ID3D12CommandQueue* queue=nullptr) {if(!queue)queue=xrQueue.Get();
#ifdef DEMENTIUM_PROFILE_DRAWS
batchGpuTimer.end(commands.Get());
#endif
hr(commands->Close());ID3D12CommandList* c[]={commands.Get()};queue->ExecuteCommandLists(1,c);syncGPU(queue);
#ifdef DEMENTIUM_PROFILE_DRAWS
batchGpuTimer.completed(queue);
#endif
}
static D3D12_RESOURCE_BARRIER transition(ID3D12Resource* r,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition={r,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,before,after};return b;
}
static void savePreview(ID3D12Resource* texture,const wchar_t* filename,std::vector<asyncPpmCapture::Image>& captures,ID3D12CommandQueue* ownerQueue=nullptr,unsigned step=4) {
    auto image=texture->GetDesc();D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT rows=0;UINT64 rowBytes=0,bytes=0;
    gameDevice->GetCopyableFootprints(&image,0,1,0,&footprint,&rows,&rowBytes,&bytes);
    D3D12_RESOURCE_DESC bd{};bd.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;bd.Width=bytes;bd.Height=1;bd.DepthOrArraySize=1;bd.MipLevels=1;bd.SampleDesc.Count=1;bd.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_READBACK;ComPtr<ID3D12Resource> readback;
    hr(gameDevice->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback)));
    beginCommands();D3D12_TEXTURE_COPY_LOCATION source{};source.pResource=texture;source.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    D3D12_TEXTURE_COPY_LOCATION destination{};destination.pResource=readback.Get();destination.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;destination.PlacedFootprint=footprint;
    commands->CopyTextureRegion(&destination,0,0,0,&source,nullptr);submitCommands(ownerQueue);
    unsigned char* pixels{};D3D12_RANGE range{0,static_cast<SIZE_T>(bytes)};hr(readback->Map(0,&range,reinterpret_cast<void**>(&pixels)));
    wchar_t path[MAX_PATH]{};GetModuleFileNameW(GetModuleHandleW(L"DementiumOpenXR.dll"),path,MAX_PATH);
    auto slash=wcsrchr(path,L'\\');if(slash) {try {
        wcscpy_s(slash+1,MAX_PATH-(slash+1-path),filename);
        asyncPpmCapture::Image copy;copy.path=path;copy.pitch=footprint.Footprint.RowPitch;copy.width=static_cast<unsigned>(image.Width);copy.height=image.Height;copy.step=(std::max)(1u,step);
        // GetCopyableFootprints may omit padding after the final row.
        copy.rgba.assign(pixels+footprint.Offset,pixels+static_cast<std::size_t>(bytes));
        captures.push_back(std::move(copy));
    }catch(...){++asyncPpmCapture::failed;}}
    D3D12_RANGE none{};readback->Unmap(0,&none);
}
// Preserve actual eye pixels for small mechanical parts and ejection checks.
static void saveEyePreview(unsigned eye,std::vector<asyncPpmCapture::Image>& captures) {savePreview(resolved[eye].Get(),eye?L"eye-right.ppm":L"eye-left.ppm",captures,nullptr,1);}
static bool tick(unsigned& submittedDraws,IDXGISwapChain* sourceChain=nullptr) {
    std::unique_lock<std::shared_mutex> lock(gate);
    if(failed||!gameQueue) return false;
    if(!xr) initialize();
#ifdef DEMENTIUM_PROFILE_DRAWS
    const double beforeWorldCompletion=gpuWaitMilliseconds;
#endif
    syncGPU(gameQueue.Get());
#ifdef DEMENTIUM_PROFILE_DRAWS
    lastWorldCompletionMilliseconds=gpuWaitMilliseconds-beforeWorldCompletion;
    worldCompletionWaitMilliseconds+=lastWorldCompletionMilliseconds;
    if(nativeGpuProfiling())nativeGpuTimer.completed(gameQueue.Get());
#endif
    submittedDraws=eyeDraws.exchange(0);
    lastUIDraws=uiDraws.exchange(0);
    auto now=GetTickCount64();
    if(submittedDraws)lastWorldTicks=now;
    if(lastUIDraws)lastUITicks=now;
    const bool inGameplay=gameplayActive&&gameplayActive();
#ifdef DEMENTIUM_PROFILE_DRAWS
    if(!inGameplay)++nonGameplayTicks;
    else if(submittedDraws)++freshWorldTicks;
    else ++cachedWorldTicks;
    if(worldCadenceCount<300){worldCadence[worldCadenceCount++]=!inGameplay?'M':submittedDraws?'F':'C';worldCadence[worldCadenceCount]=0;}
    if(inGameplay){const unsigned kind=submittedDraws?1:0;if(!worldCallerDepth[kind])worldCallerDepth[kind]=CaptureStackBackTrace(0,24,worldCallerStacks[kind],nullptr);}
    const auto executed=executedWorldDraws.exchange(0);
    if(inGameplay&&!submittedDraws&&executed)++cachedWithExecutedWorld;
#endif
    ComPtr<ID3D12Resource> menuSource;
    if(sourceChain&&swapchainQueue::lookup(sourceChain)&&stereoPresentation::desktopFallback(inGameplay,submittedDraws!=0,haveWorldFrame,now-lastWorldTicks)){
        ComPtr<IDXGISwapChain3> chain3;if(SUCCEEDED(sourceChain->QueryInterface(IID_PPV_ARGS(&chain3))))chain3->GetBuffer(chain3->GetCurrentBackBufferIndex(),IID_PPV_ARGS(&menuSource));
        if(menuSource){lastUIDraws=1;lastUITicks=now;}
    }
    if(active) {
        beginCommands();
        if(!inGameplay&&warmSceneResources)warmSceneResources(gameDevice.Get(),commands.Get(),sceneSamples);
        if(submittedDraws&&!menuSource&&drawScopeReticle()){++lastUIDraws;lastUITicks=now;}
        if(submittedDraws||lastUIDraws)cachedFadeAlpha=pendingFadeAlpha;
        if(extraUISuppressFade&&extraUISuppressFade())cachedFadeAlpha=0;
        const float handoffAlpha=presentationFade.update(now,inGameplay,haveWorldFrame||submittedDraws,now-lastWorldTicks,extraUIVisible&&extraUIVisible());
        const float presentationAlpha=(std::max)(cachedFadeAlpha,handoffAlpha);
        if(xr->fade.acquired){auto target=rtHeap->GetCPUDescriptorHandleForHeapStart();target.ptr+=3*gameDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);D3D12_RENDER_TARGET_VIEW_DESC view{};view.Format=static_cast<DXGI_FORMAT>(xr->colorFormat);view.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE2D;gameDevice->CreateRenderTargetView(xr->fade.texture(),&view,target);const float fadeColor[4]={0,0,0,presentationAlpha};commands->ClearRenderTargetView(target,fadeColor,0,nullptr);}
        if(submittedDraws&&prepareSceneExtras)prepareSceneExtras();
        if(submittedDraws)opticScope::resolve(commands.Get());
        for(int i=0;i<2;i++) {
            if(submittedDraws) {
            if(extraSceneDraw)extraSceneDraw(gameDevice.Get(),commands.Get(),sceneSamples,rtvs[i],dsvs[i],xr->eyes[i].view,xr->eyes[i].width,xr->eyes[i].height);
            opticScope::draw(commands.Get(),xr->eyes[i].view,xr->eyes[i].width,xr->eyes[i].height,rtvs[i],dsvs[i]);
            auto a=transition(eyeColor[i].Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
            auto resolve=transition(resolved[i].Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_RESOLVE_DEST);
            D3D12_RESOURCE_BARRIER resolveBarriers[]={a,resolve};commands->ResourceBarrier(2,resolveBarriers);
            commands->ResolveSubresource(resolved[i].Get(),0,eyeColor[i].Get(),0,DXGI_FORMAT_R8G8B8A8_UNORM);
            std::swap(resolveBarriers[0].Transition.StateBefore,resolveBarriers[0].Transition.StateAfter);
            std::swap(resolveBarriers[1].Transition.StateBefore,resolveBarriers[1].Transition.StateAfter);commands->ResourceBarrier(2,resolveBarriers);
            lastWorldViews[i]=xr->eyes[i].view;
            }
            auto b=transition(xr->eyes[i].texture(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_DEST);
            commands->ResourceBarrier(1,&b);
            commands->CopyResource(xr->eyes[i].texture(),resolved[i].Get());
            std::swap(b.Transition.StateBefore,b.Transition.StateAfter);commands->ResourceBarrier(1,&b);
        }
        if(submittedDraws&&!haveWorldFrame&&!xr->desktopDiagnostic) {
            // The headset may have been on a desk during the opening menus.
            // Establish the player's height when gameplay actually starts.
            originSet=false;originStartup.reset();
        }
        if(submittedDraws)haveWorldFrame=true;
        const bool customUI=extraUIVisible&&extraUIVisible();
        if(submittedDraws&&!lastUIDraws&&!customUI)haveUIFrame=false;
        if(xr->overlay.acquired) {
            if(lastUIDraws||customUI) {
                const bool capturedOpaque=uiOpaqueDraws.exchange(false);
                cachedUIOpaque=customUI?false:(menuSource?true:capturedOpaque);
                auto source=menuSource?menuSource.Get():uiColor.Get();
                if(extraUIDraw){
                    if(menuSource){auto toRT=transition(source,D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET);commands->ResourceBarrier(1,&toRT);}
                    if(lastUIDraws&&extraUICapture)extraUICapture(gameDevice.Get(),commands.Get(),source);
                    extraUIDraw(gameDevice.Get(),commands.Get(),source,desktopWidth,desktopHeight);
                    if(menuSource){auto toPresent=transition(source,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PRESENT);commands->ResourceBarrier(1,&toPresent);}
                }

                auto a=transition(source,menuSource?D3D12_RESOURCE_STATE_PRESENT:D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);
                auto b=transition(uiCache.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_COPY_DEST);
                D3D12_RESOURCE_BARRIER barriers[]={a,b};commands->ResourceBarrier(2,barriers);commands->CopyResource(uiCache.Get(),source);
                for(auto& barrier:barriers)std::swap(barrier.Transition.StateBefore,barrier.Transition.StateAfter);commands->ResourceBarrier(2,barriers);haveUIFrame=true;
            }
            if(haveUIFrame) {
                auto b=transition(xr->overlay.texture(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_DEST);commands->ResourceBarrier(1,&b);
                commands->CopyResource(xr->overlay.texture(),uiCache.Get());std::swap(b.Transition.StateBefore,b.Transition.StateAfter);commands->ResourceBarrier(1,&b);
            }
        }
        // The menu fallback transitions a DXGI buffer, so it must execute on
        // that buffer's owning presentation queue, including UI slot writes.
        submitCommands(menuSource?swapchainQueue::lookup(sourceChain).Get():nullptr);
        static bool previewSaved=false;
        static unsigned sceneFrames=0;if(submittedDraws)++sceneFrames;
        // A capture must survive desktop-only presents and use a complete pair.
        std::vector<asyncPpmCapture::Image> captureImages;
        const bool captureAvailable=!asyncPpmCapture::busy.load(std::memory_order_acquire);
        bool capture=captureRequested.exchange(false);
        if(capture&&!captureAvailable){captureRequested=true;capture=false;}
        if(capture&&captureObserved)captureObserved(frames,haveWorldFrame,submittedDraws,now-lastWorldTicks);
        if(capture&&sourceChain){
            ComPtr<IDXGISwapChain3> chain;ComPtr<ID3D12Resource> source;
            auto owner=swapchainQueue::lookup(sourceChain);
            if(owner&&SUCCEEDED(sourceChain->QueryInterface(IID_PPV_ARGS(&chain)))&&SUCCEEDED(chain->GetBuffer(chain->GetCurrentBackBufferIndex(),IID_PPV_ARGS(&source)))){
                beginCommands();auto barrier=transition(source.Get(),D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_COPY_SOURCE);commands->ResourceBarrier(1,&barrier);submitCommands(owner.Get());
                savePreview(source.Get(),L"native-source.ppm",captureImages,owner.Get());
                beginCommands();std::swap(barrier.Transition.StateBefore,barrier.Transition.StateAfter);commands->ResourceBarrier(1,&barrier);submitCommands(owner.Get());
            }
        }
        if(captureAvailable&&haveWorldFrame&&((sceneFrames>120&&!previewSaved)||capture)) {previewSaved=true;saveEyePreview(0,captureImages);saveEyePreview(1,captureImages);}
        static bool menuSaved=false,hudSaved=false;
        if(captureAvailable&&haveUIFrame&&((!haveWorldFrame&&frames>120&&!menuSaved)||(sceneFrames>120&&!hudSaved)||capture)) {
            if(haveWorldFrame){hudSaved=true;savePreview(uiCache.Get(),L"hud.ppm",captureImages);}else{menuSaved=true;savePreview(uiCache.Get(),L"menu.ppm",captureImages);}
        }
        if(!asyncPpmCapture::submit(captureImages))captureRequested=true;
        // The desktop renderer can present UI without redrawing the scene. Keep
        // its last complete stereo image and original render poses for reprojection.
        if(haveWorldFrame)for(int i=0;i<2;i++)xr->eyes[i].view=lastWorldViews[i];
        // A normal skipped scene present keeps its complete eye pair. Extended
        // loading/menu gaps must not hold an obsolete room in front of the user.
        // Gameplay stalls reuse the complete stereo pair and its original poses.
        // Never replace gameplay with the unfiltered desktop image/legacy HUD.
        submittedWorld=stereoPresentation::worldVisible(inGameplay,haveWorldFrame,now-lastWorldTicks);
        submittedUI=haveUIFrame&&(submittedWorld||now-lastUITicks<=250);
        if(presentationObserved)presentationObserved(inGameplay,submittedWorld,menuSource!=nullptr);
        xr->endFrame(submittedWorld,submittedUI,cachedUIOpaque,presentationAlpha); frames++;
    } else {xr->endFrame(false); syncGPU();}
    active=false;cursor=0;worldSeen=false;compositeSkipped=false;compositePasses=0;uiOpaqueDraws=false;pendingFadeAlpha=0;
    if(recenterRequested.exchange(false)){originSet=false;originStartup.reset();}
    double waitStarted=preciseMilliseconds();bool next=xr->beginFrame();lastXrBeginMilliseconds=preciseMilliseconds()-waitStarted;xrWaitMilliseconds+=lastXrBeginMilliseconds;
    if(next) {
        frameScopeFactor=sniperScope::factor();
        for(const auto& change:xr->referenceChanges){
            if(change.valid&&motionTracking::validPose(change.pose))xr->controls.rebase(change.pose);
            if(originSet&&change.valid&&motionTracking::validPose(change.pose)){origin=headReference::rebase(origin,change.pose);if(haveWorldFrame)for(auto& view:lastWorldViews)view.pose=headReference::rebase(view.pose,change.pose);++referenceRebases;}
            else {originSet=false;originStartup.reset();}
        }
        xr->referenceChanges.clear();
        bool focused=xr->state==XR_SESSION_STATE_FOCUSED;
        auto centered=recenterOrigin(xr->eyes[0].view,xr->eyes[1].view);
        // VD has no user-presence extension: require active controller poses
        // before committing a startup baseline, rather than a desk-only view.
        bool wornEvidence=xr->controls.hand[0].gripValid||xr->controls.hand[1].gripValid;
        if(xr->desktopDiagnostic){origin={};origin.orientation.w=1;originSet=true;}
        if(!originSet){if(originStartup.update(centered,now,haveWorldFrame&&focused&&xr->headTracked&&wornEvidence,origin))originSet=true;else origin=centered;}
        auto calibrate=postureCalibration.load();
        if(calibrate&&originSet&&focused&&xr->headTracked&&xr->floorHeightValid){
            auto mode=calibrate==2?headReference::Posture::Standing:headReference::Posture::Seated;
            if(postures.calibrate(mode,xr->floorHeight)){origin.position.y=centered.position.y;postureCalibration=0;}
        }
        float heightChange=0;
        if(postures.update(xr->floorHeight,now,focused&&xr->headTracked&&xr->floorHeightValid,heightChange)&&originSet)origin.position.y+=heightChange;
        beginCommands();const float clear[4]={0.004f,0.004f,0.004f,1};
        opticScope::update(now,xr->eyes[0].view,xr->eyes[1].view);opticScope::clear(commands.Get());
        for(int i=0;i<2;i++) {commands->ClearRenderTargetView(rtvs[i],clear,0,nullptr);commands->ClearDepthStencilView(dsvs[i],D3D12_CLEAR_FLAG_DEPTH|D3D12_CLEAR_FLAG_STENCIL,1,0,0,nullptr);}
        const float transparent[4]={0,0,0,0};commands->ClearRenderTargetView(uiRTV,transparent,0,nullptr);
        commands->ClearDepthStencilView(uiDSV,D3D12_CLEAR_FLAG_DEPTH|D3D12_CLEAR_FLAG_STENCIL,1,0,0,nullptr);
        submitCommands(); active=true;
    }
    return true;
}
static bool gameList(ID3D12GraphicsCommandList* list){
    if(!gameDevice)return false;auto& state=lists[list];
    if(!state.deviceChecked){state.gameDeviceOwned=deviceOwnership::belongs(list,gameDevice.Get());state.deviceChecked=true;}
    return state.gameDeviceOwned;
}
static bool copyConstants(UINT64 address,unsigned bytes,unsigned char* output) {
    std::lock_guard<std::mutex> lock(mapMutex);
    auto i=uploads.upper_bound(address);if(i==uploads.begin()) return false;--i;
    UINT64 delta=address-i->first;if(!i->second.canRead(static_cast<size_t>(delta),bytes)) return false;
    if(gameDevice){
        if(i->second.resource){if(!deviceOwnership::belongs(i->second.resource.Get(),gameDevice.Get()))return false;}
        else {ComPtr<IUnknown> expected;if(FAILED(gameDevice.As(&expected))||expected.Get()!=i->second.deviceIdentity.Get())return false;}
    }
    memcpy(output,i->second.cpu+delta,bytes);i->second.observedRead=true;return true;
}
// Snapshot the source matrix before eye transforms, solely for desktop input
// diagnosis. This separates native camera movement from stereo pose changes.
struct NativeViewEvidence {std::array<float,12> matrix{};unsigned frame{};bool valid{};};
static NativeViewEvidence nativeViewEvidence;
static NativeViewEvidence readNativeViewEvidence(){std::lock_guard<std::mutex> lock(mapMutex);return nativeViewEvidence;}
// Shader reflection reports row-major 3x4 affine matrices and row-major 4x4 projection.
// Preserve the game's depth mapping; replace horizontal/vertical FOV with OpenXR's asymmetric FOV.
static void transform(float* data,const XrView& view) {
    float* p=data;float* m=data+16;
    const auto& f=view.fov;float l=tanf(f.angleLeft),r=tanf(f.angleRight),b=tanf(f.angleDown),t=tanf(f.angleUp);
    float sign=p[14]>=0?1.f:-1.f;
    p[0]=2/(r-l);p[2]=-sign*(r+l)/(r-l);p[5]=2/(t-b);p[6]=-sign*(t+b)/(t-b);
    // Scope magnification belongs in both independent eye projections. Keep
    // the asymmetric eye offsets and depth mapping; do not enlarge a flat frame.
    p[0]*=frameScopeFactor;p[5]*=frameScopeFactor;
    // The native vertex shader converts depth as -2*clip.z-clip.w.
    // Its original near plane is about 20 cm, too large for tracked leaning.
    if(sign<0){float depthA=-2*p[10]-p[14],depthB=-2*p[11],farPlane=depthB/(depthA-p[14]);if(std::isfinite(farPlane)&&farPlane>1){float nearPlane=.02f;float depthScale=farPlane/(farPlane-nearPlane);p[10]=(depthScale+1)*.5f;p[11]=nearPlane*depthScale*.5f;}}
    // Rotate from the recentered HMD pose into each tracked eye's coordinates.
    XMVECTOR oq=XMVectorSet(origin.orientation.x,origin.orientation.y,origin.orientation.z,origin.orientation.w);
    XMVECTOR eq=XMVectorSet(view.pose.orientation.x,view.pose.orientation.y,view.pose.orientation.z,view.pose.orientation.w);
    XMMATRIX o=XMMatrixRotationQuaternion(oq),e=XMMatrixRotationQuaternion(eq);
    XMMATRIX relative=o*XMMatrixTranspose(e);
    XMFLOAT4X4 rot;XMStoreFloat4x4(&rot,relative);
    XMVECTOR stageDelta=XMVectorSet(view.pose.position.x-origin.position.x,view.pose.position.y-origin.position.y,view.pose.position.z-origin.position.z,0);
    XMFLOAT3 localDelta;XMStoreFloat3(&localDelta,XMVector3TransformNormal(stageDelta,XMMatrixTranspose(o)));
    float dx=localDelta.x*unitsPerMeter;
    float dy=localDelta.y*unitsPerMeter;
    float dz=localDelta.z*unitsPerMeter;
    // Game coordinates are confirmed from the projection's homogeneous-Z sign.
    if(sign>0) dz=-dz;
    float source[12];memcpy(source,m,sizeof(source));
    const float rr[3][3]={{rot._11,rot._21,rot._31},{rot._12,rot._22,rot._32},{rot._13,rot._23,rot._33}};
    for(int row=0;row<3;row++) for(int col=0;col<4;col++) {
        m[row*4+col]=rr[row][0]*source[col]+rr[row][1]*source[4+col]+rr[row][2]*source[8+col];
        if(col==3) m[row*4+col]-=rr[row][0]*dx+rr[row][1]*dy+rr[row][2]*dz;
    }
}
static void transformConstants(float* data,unsigned bytes,const XrView& view) {
    if(bytes==1200) {transform(data,view);return;}
    // Particle shaders store view first, projection second. Keep system/animation
    // parameters intact and transform the shared camera layout only.
    float camera[28];
    memcpy(camera,data+12,64);memcpy(camera+16,data,48);
    transform(camera,view);
    memcpy(data+12,camera,64);memcpy(data,camera+16,48);
}

// Apply identical native-hand clipping to the desktop source draw. Keep the
// original camera and restore its pipeline/root binding before duplicating eyes.
template<class Callback> static void sourceDraw(ID3D12GraphicsCommandList* list,Callback draw) {
    std::shared_lock<std::shared_mutex> lock(gate);
    if(!active||failed||(!nativeHandClip::currentMask&&!selectedWeaponDraw::replacing)||!gameList(list)){draw();return;}
    ++nativeHandClip::sourceRequests;
    auto& state=lists[list];PipelineInfo info{};bool known=false;
    {std::lock_guard<std::mutex> guard(mapMutex);auto it=pipelineInfo.find(state.pipeline);if(it!=pipelineInfo.end()){info=it->second;known=true;}}
    // A native draw can synchronously invoke the D3D12 validation callback,
    // which inspects the upload registry under mapMutex. Release it first.
    if(!known){++nativeHandClip::sourceFallback[0];draw();return;}
    if(info.constants!=1200){++nativeHandClip::sourceFallback[1];draw();return;}
    if(!state.cbv){++nativeHandClip::sourceFallback[3];draw();return;}
    alignas(16) unsigned char buffer[1280]{};
    if(!copyConstants(state.cbv,1200,buffer)){++nativeHandClip::sourceFallback[4];draw();return;}
    if(selectedWeaponDraw::body(info.constants,buffer,1200))return;
    // Replacement ownership does not depend on the optional clipping shader
    // or upload buffer. Those resources are only needed to modify a draw.
    if(!info.handPipeline){++nativeHandClip::sourceFallback[2];draw();return;}
    if(!constantsCPU){++nativeHandClip::sourceFallback[3];draw();return;}
    auto offset=cursor.fetch_add(1280);if(offset+1280>capacity){++nativeHandClip::sourceFallback[5];draw();return;}
    std::memcpy(buffer+1192,&nativeHandClip::currentMask,4);
    std::memcpy(constantsCPU+offset,buffer,1200);
    list->SetPipelineState(info.handPipeline.Get());
    list->SetGraphicsRootConstantBufferView(0,constants->GetGPUVirtualAddress()+offset);
    ++nativeHandClip::sourceApplied;
    draw();
    list->SetPipelineState(state.pipeline);list->SetGraphicsRootConstantBufferView(0,state.cbv);
}

template<class Callback> static void duplicate(ID3D12GraphicsCommandList* list,Callback draw) {
    std::shared_lock<std::shared_mutex> lock(gate);
    if(!active||failed||!gameList(list)) {++skips[0];return;}
    if(list->GetType()==D3D12_COMMAND_LIST_TYPE_BUNDLE)++bundleDraws;else ++directDraws;
    auto& s=lists[list];PipelineInfo info{};
    {std::lock_guard<std::mutex> guard(mapMutex);auto i=pipelineInfo.find(s.pipeline);if(i==pipelineInfo.end()){++skips[1];return;}info=i->second;}
    if(info.constants!=1200&&info.constants!=128) {++skips[2];return;}
    const bool adaptedSamples=info.stereoBase!=nullptr;
    if(info.color!=DXGI_FORMAT_R8G8B8A8_UNORM||info.depth!=DXGI_FORMAT_D24_UNORM_S8_UINT||
        (!adaptedSamples&&(info.samples!=sceneSamples||info.sampleQuality!=0))) {++skips[3];return;}
    if(adaptedSamples){
        info.handPipeline=info.stereoHand;info.fogPipeline=info.stereoFog;
        info.handFogPipeline=info.stereoHandFog;info.particleFogPipeline=info.stereoParticle;
    }
    if(!s.cbv||s.rtvCount!=1||!s.hasDepth||!s.viewportCount||!s.scissorCount){++skips[4];return;}
    alignas(16) unsigned char buffer[1280]{};
    if(!copyConstants(s.cbv,info.constants,buffer)){++skips[5];return;}
    if(selectedWeaponDraw::body(info.constants,buffer,info.constants))return;
    auto floats=reinterpret_cast<float*>(buffer)+(info.constants==128?12:0);
    if(!std::isfinite(floats[0])||!std::isfinite(floats[5])||fabsf(floats[15])>.001f){++skips[6];return;}
    if(xr->desktopDiagnostic&&info.constants==1200){
        std::lock_guard<std::mutex> evidenceLock(mapMutex);
        if(!nativeViewEvidence.valid||nativeViewEvidence.frame!=frames){
            std::memcpy(nativeViewEvidence.matrix.data(),floats+16,48);
            nativeViewEvidence.frame=frames;nativeViewEvidence.valid=true;
        }
    }
    alignas(16) float light[112]{};
    bool centerLight=info.headLightLayout&&xr->headTracked&&s.pixelCBV&&copyConstants(s.pixelCBV,448,reinterpret_cast<unsigned char*>(light));
    alignas(16) unsigned char particleSource[1040]{},particlePixel[1040]{};
    bool fixParticleFog=info.particleFogLayout&&s.pixelCBV&&(!nativeFog::active(GetTickCount64())||!modPreferences::blackFog.load())&&
        copyConstants(s.pixelCBV,1040,particleSource)&&particleFog::prepare(particlePixel,particleSource,1040,false);
    bool correctParticleFog=info.particleFogPipeline&&s.pixelCBV&&copyConstants(s.pixelCBV,1040,particleSource);
    const unsigned views=opticScope::active?3:2;
    UINT64 stride=(fixParticleFog||correctParticleFog)?2560:centerLight?1792:1280,allocation=stride*views;
    UINT64 offset=cursor.fetch_add(allocation);if(offset+allocation>capacity) {++skips[7];return;}
    auto head=xr->eyes[0].view.pose;
    auto rightOrientation=xr->eyes[1].view.pose.orientation;
    XMFLOAT4 centerOrientation;
    XMStoreFloat4(&centerOrientation,XMQuaternionSlerp(XMVectorSet(head.orientation.x,head.orientation.y,head.orientation.z,head.orientation.w),XMVectorSet(rightOrientation.x,rightOrientation.y,rightOrientation.z,rightOrientation.w),.5f));
    head.orientation={centerOrientation.x,centerOrientation.y,centerOrientation.z,centerOrientation.w};
    head.position.x=(head.position.x+xr->eyes[1].view.pose.position.x)*.5f;
    head.position.y=(head.position.y+xr->eyes[1].view.pose.position.y)*.5f;
    head.position.z=(head.position.z+xr->eyes[1].view.pose.position.z)*.5f;
    {std::lock_guard<std::mutex> guard(mapMutex);auto target=renderTargetResources.find(s.rtvs[0].ptr);if(target!=renderTargetResources.end())sceneTextures.insert(target->second);}
    worldSeen=true;
    if(adaptedSamples)list->SetPipelineState(info.stereoBase.Get());
    bool clipHands=info.constants==1200&&info.handPipeline&&nativeHandClip::currentMask;
    if(clipHands){std::memcpy(buffer+1192,&nativeHandClip::currentMask,4);list->SetPipelineState(info.handPipeline.Get());++nativeHandClip::eyeApplied;}
    bool correctFog=centerLight&&info.fogPipeline&&(!clipHands||info.handFogPipeline);
    if(correctFog)list->SetPipelineState(clipHands?info.handFogPipeline.Get():info.fogPipeline.Get());
    for(unsigned eye=0;eye<views;eye++) {
        const auto& renderView=eye==2?opticScope::camera:xr->eyes[eye].view;
        unsigned renderWidth=eye==2?opticScope::resolution:xr->eyes[eye].width;
        unsigned renderHeight=eye==2?opticScope::resolution:xr->eyes[eye].height;
        auto renderTarget=eye==2?opticScope::rtv:rtvs[eye];auto renderDepth=eye==2?opticScope::dsv:dsvs[eye];
        auto eyeOffset=offset+eye*stride;
        memcpy(constantsCPU+eyeOffset,buffer,info.constants);
        transformConstants(reinterpret_cast<float*>(constantsCPU+eyeOffset),info.constants,renderView);
        list->SetGraphicsRootConstantBufferView(0,constants->GetGPUVirtualAddress()+eyeOffset);
        if(centerLight){
            auto output=reinterpret_cast<float*>(constantsCPU+eyeOffset+1280);memcpy(output,light,448);
            XrPosef lamp=head;handheldLight::read(GetTickCount64(),lamp);
            headLightConstants::center(output,lamp,renderView.pose,unitsPerMeter);
            modPreferences::fogColor(output+92);
            if(correctFog)nativeFog::coefficients(output,reinterpret_cast<const float*>(buffer),reinterpret_cast<const float*>(constantsCPU+eyeOffset));
            if((!nativeFog::active(GetTickCount64())||!modPreferences::blackFog.load())||geometryIsolation::mode(GetTickCount64())==geometryIsolation::NoFog)output[96]=0;
            list->SetGraphicsRootConstantBufferView(1,constants->GetGPUVirtualAddress()+eyeOffset+1280);
        }
        if(correctParticleFog){
            auto output=constantsCPU+eyeOffset+1280;
            if(particleFogDepth::prepare(output,particleSource,reinterpret_cast<const float*>(buffer)+12,reinterpret_cast<const float*>(constantsCPU+eyeOffset)+12,
                nativeFog::active(GetTickCount64())&&modPreferences::blackFog.load()&&geometryIsolation::mode(GetTickCount64())!=geometryIsolation::NoFog)){
                modPreferences::fogColor(reinterpret_cast<float*>(output));
                list->SetPipelineState(info.particleFogPipeline.Get());
                list->SetGraphicsRootConstantBufferView(1,constants->GetGPUVirtualAddress()+eyeOffset+1280);++particleFogDraws;
            }else {list->SetPipelineState(adaptedSamples?info.stereoBase.Get():s.pipeline);list->SetGraphicsRootConstantBufferView(1,s.pixelCBV);}
        }
        if(fixParticleFog&&!correctParticleFog){memcpy(constantsCPU+eyeOffset+1280,particlePixel,1040);list->SetGraphicsRootConstantBufferView(1,constants->GetGPUVirtualAddress()+eyeOffset+1280);++particleFogDraws;}
        D3D12_VIEWPORT vp{0,0,static_cast<float>(renderWidth),static_cast<float>(renderHeight),0,1};
        D3D12_RECT rect{0,0,static_cast<LONG>(renderWidth),static_cast<LONG>(renderHeight)};
        list->RSSetViewports(1,&vp);list->RSSetScissorRects(1,&rect);list->OMSetRenderTargets(1,&renderTarget,FALSE,&renderDepth);
        draw();if(eye<2){++eyeDraws;if(info.constants==128)++particleDraws;}
#ifdef DEMENTIUM_PROFILE_DRAWS
        if(eye==0)++s.diagnosticWorldDraws;
#endif
    }
    if(adaptedSamples||clipHands||correctFog||correctParticleFog)list->SetPipelineState(s.pipeline);
    list->SetGraphicsRootConstantBufferView(0,s.cbv);
    if(centerLight||fixParticleFog||correctParticleFog)list->SetGraphicsRootConstantBufferView(1,s.pixelCBV);
    list->RSSetViewports(s.viewportCount,s.viewports.data());list->RSSetScissorRects(s.scissorCount,s.scissors.data());
    list->OMSetRenderTargets(s.rtvCount,s.rtvs.data(),FALSE,s.hasDepth?&s.dsv:nullptr);
}
static bool spriteAttribute(const ListState& state,const SpriteElement& element,UINT vertex,UINT instance,float* output) {
    if(element.slot>=state.vertices.size())return false;
    const auto& buffer=state.vertices[element.slot];
    UINT index=element.step?instance/element.step:vertex;
    UINT64 offset=UINT64(index)*buffer.StrideInBytes+element.offset;
    unsigned bytes=element.format==DXGI_FORMAT_R32G32B32A32_FLOAT?16:element.format==DXGI_FORMAT_R32G32B32_FLOAT?12:element.format==DXGI_FORMAT_R32G32_FLOAT?8:(element.format==DXGI_FORMAT_R8G8B8A8_UNORM||element.format==DXGI_FORMAT_B8G8R8A8_UNORM)?4:0;
    if(!bytes||offset+bytes>buffer.SizeInBytes)return false;
    output[0]=output[1]=output[2]=0;output[3]=1;
    if(bytes==4){unsigned char color[4];if(!copyConstants(buffer.BufferLocation+offset,4,color))return false;for(unsigned i=0;i<4;i++)output[i]=color[i]/255.f;if(element.format==DXGI_FORMAT_B8G8R8A8_UNORM)std::swap(output[0],output[2]);return true;}
    return copyConstants(buffer.BufferLocation+offset,bytes,reinterpret_cast<unsigned char*>(output));
}
static bool spriteBounds(const ListState& state,const SpriteLayout& layout,UINT count,UINT first,INT base,bool indexed,UINT instance,float* bounds,float* color) {
    if(!layout.valid||!count||count>256)return false;
    bounds[0]=bounds[1]=1e9f;bounds[2]=bounds[3]=-1e9f;
    for(UINT i=0;i<count;i++) {
        UINT vertex=first+i;
        if(indexed){unsigned bytes=state.indices.Format==DXGI_FORMAT_R16_UINT?2:state.indices.Format==DXGI_FORMAT_R32_UINT?4:0;UINT index=0;
            if(!bytes||UINT64(vertex+1)*bytes>state.indices.SizeInBytes||!copyConstants(state.indices.BufferLocation+UINT64(vertex)*bytes,bytes,reinterpret_cast<unsigned char*>(&index)))return false;
            if(INT64(index)+base<0)return false;vertex=static_cast<UINT>(INT64(index)+base);
        }
        float position[4],rows[4][4],clip[4]{};
        if(!spriteAttribute(state,layout.position,vertex,instance,position))return false;
        for(UINT r=0;r<4;r++){if(!spriteAttribute(state,layout.rows[r],vertex,instance,rows[r]))return false;for(UINT c=0;c<4;c++)clip[r]+=rows[r][c]*position[c];}
        if(!std::isfinite(clip[0])||!std::isfinite(clip[1])||fabsf(clip[3])<.00001f)return false;
        float x=clip[0]/clip[3],y=clip[1]/clip[3];bounds[0]=(std::min)(bounds[0],x);bounds[1]=(std::min)(bounds[1],y);bounds[2]=(std::max)(bounds[2],x);bounds[3]=(std::max)(bounds[3],y);
        if(i==0&&!spriteAttribute(state,layout.color,vertex,instance,color))color[0]=color[1]=color[2]=color[3]=1;
    }return true;
}
static bool gunAimOffset(const XrView& view,float& x,float& y) {
    float camera[28]{};camera[0]=camera[5]=1;camera[14]=-1;camera[16]=camera[21]=camera[26]=1;
    transform(camera,view);float distance=10*unitsPerMeter;
    XrVector3f target{0,0,-distance};motionTracking::Sample tracked;
    if(motionTracking::read(GetTickCount64(),tracked)){
        target=motionTracking::pointAhead(tracked.rightAim,10.f);
        target.x*=unitsPerMeter;target.y*=unitsPerMeter;target.z*=unitsPerMeter;
    }
    float px=camera[16]*target.x+camera[17]*target.y+camera[18]*target.z+camera[19];
    float py=camera[20]*target.x+camera[21]*target.y+camera[22]*target.z+camera[23];
    float pz=camera[24]*target.x+camera[25]*target.y+camera[26]*target.z+camera[27];
    if(pz>=-.05f)return false;
    float t=-1.4f/pz;x=px*t/2*frameScopeFactor;y=-py*t/(2.f*desktopHeight/desktopWidth)*frameScopeFactor;return true;
}
static bool drawScopeReticle(){
    if(frameScopeFactor<=1.01f)return false;
    auto view=xr->eyes[0].view;
    view.pose.position.x=(view.pose.position.x+xr->eyes[1].view.pose.position.x)*.5f;
    view.pose.position.y=(view.pose.position.y+xr->eyes[1].view.pose.position.y)*.5f;
    view.pose.position.z=(view.pose.position.z+xr->eyes[1].view.pose.position.z)*.5f;
    float x,y;if(!gunAimOffset(view,x,y))return false;
    LONG cx=static_cast<LONG>((.5f+x)*desktopWidth),cy=static_cast<LONG>((.5f+y)*desktopHeight);
    D3D12_RECT lines[]={{cx-18,cy-1,cx-5,cy+1},{cx+5,cy-1,cx+18,cy+1},{cx-1,cy-18,cx+1,cy-5},{cx-1,cy+5,cx+1,cy+18}};
    const float white[4]={1,1,1,1},outline[4]={0,0,0,1};
    for(auto line:lines){
        auto border=line;--border.left;--border.top;++border.right;++border.bottom;
        if(border.left<0||border.top<0||border.right>static_cast<LONG>(desktopWidth)||border.bottom>static_cast<LONG>(desktopHeight))continue;
        commands->ClearRenderTargetView(uiRTV,outline,1,&border);commands->ClearRenderTargetView(uiRTV,white,1,&line);
    }
    return true;
}
template<class Callback> static void duplicateUI(ID3D12GraphicsCommandList* list,UINT count,UINT instances,Callback draw,UINT first=0,INT base=0,bool indexed=false,UINT firstInstance=0) {
    if(!gameList(list))return;
    std::shared_lock<std::shared_mutex> lock(gate);
    if(!active||failed)return;
    auto& s=lists[list];PipelineInfo info{};
    {
        std::lock_guard<std::mutex> guard(mapMutex);auto p=pipelineInfo.find(s.pipeline);if(p==pipelineInfo.end())return;info=p->second;
        if(!info.sprite||info.samples!=1||info.color!=DXGI_FORMAT_R8G8B8A8_UNORM||s.rtvCount!=1)return;
    }
    bool solidTexture=false;std::array<float,4> solidColor{};
    unsigned spriteWidth=0,spriteHeight=0;
    if(info.usesTexture) {
        std::lock_guard<std::mutex> guard(mapMutex);
        for(UINT slot=0;slot<16;slot++) {
            if(!(info.textureMask&(1u<<slot)))continue;
            bool classified=false,sceneComposite=false;
            for(auto& heap:s.heaps) {
                if(s.textures[slot].ptr<heap.gpu||!heap.stride)continue;
                UINT64 delta=s.textures[slot].ptr-heap.gpu;
                if(delta>=UINT64(heap.count)*heap.stride||delta%heap.stride)continue;
                auto found=sampledResources.find(heap.cpu+static_cast<SIZE_T>(delta));
                if(slot==0){auto meta=sampledViews.find(heap.cpu+static_cast<SIZE_T>(delta));if(meta!=sampledViews.end()&&meta->second.resourceKnown){spriteWidth=static_cast<unsigned>(meta->second.resource.Width);spriteHeight=meta->second.resource.Height;}}
                if(found!=sampledResources.end()){classified=true;if(vignetteTextures.count(found->second))return;auto solid=solidTexels.find(found->second);if(slot==0&&solid!=solidTexels.end()){solidTexture=true;solidColor=solid->second;}sceneComposite=sceneTextures.count(found->second)!=0||capturedUITextures.count(found->second)!=0;}break;
            }
            if(sceneComposite){auto target=renderTargetResources.find(s.rtvs[0].ptr);if(target!=renderTargetResources.end())sceneTextures.insert(target->second);}
            if((!classified&&(worldSeen||haveWorldFrame))||sceneComposite)return;
        }
        // Unknown descriptors are excluded until classified, so no desktop
        // scene copy can silently cover independent left/right eye rendering.
    }
    if(!info.uiPipeline)return;
    {std::lock_guard<std::mutex> guard(mapMutex);auto target=renderTargetResources.find(s.rtvs[0].ptr);if(target!=renderTargetResources.end())capturedUITextures.insert(target->second);}
    float bounds[4]{},color[4]{};
    bool bounded=instances==1&&spriteBounds(s,info.layout,count,first,base,indexed,firstInstance,bounds,color);
    if(bounded&&info.usesTexture&&info.textureMask==1&&nativeUiText::inventoryCursor(spriteWidth,spriteHeight,count,
        extraUIVisible&&extraUIVisible(),[&](UINT i,float* uv){
        UINT vertex=first+i;if(indexed){UINT bytes=s.indices.Format==DXGI_FORMAT_R16_UINT?2:s.indices.Format==DXGI_FORMAT_R32_UINT?4:0;UINT index{};
            if(!bytes||UINT64(vertex+1)*bytes>s.indices.SizeInBytes||!copyConstants(s.indices.BufferLocation+UINT64(vertex)*bytes,bytes,reinterpret_cast<unsigned char*>(&index))||INT64(index)+base<0)return false;vertex=UINT(INT64(index)+base);}
        return spriteAttribute(s,info.layout.uv,vertex,firstInstance,uv);
    }))return;
    if(bounded&&info.usesTexture&&nativeScopeHud::suppress(nativeUiGameplay&&nativeUiGameplay(),
        sniperScope::visibleWeapon.load(),spriteWidth,spriteHeight,count,bounds))return;
    // Native sprites can use a full-screen quad clipped to a tiny scissor
    // (including the separate bars of the crosshair).
    if(bounded&&s.viewportCount&&s.scissorCount==1&&s.viewports[0].Width>0&&s.viewports[0].Height>0){auto v=s.viewports[0];auto r=s.scissors[0];bounds[0]=(std::max)(bounds[0],2*(r.left-v.TopLeftX)/v.Width-1);bounds[2]=(std::min)(bounds[2],2*(r.right-v.TopLeftX)/v.Width-1);bounds[1]=(std::max)(bounds[1],1-2*(r.bottom-v.TopLeftY)/v.Height);bounds[3]=(std::min)(bounds[3],1-2*(r.top-v.TopLeftY)/v.Height);}
    if(bounded&&solidTexture&&info.textureMask==1&&info.constants==0&&(worldSeen||haveWorldFrame)&&bounds[0]<-.98f&&bounds[1]<-.98f&&bounds[2]>.98f&&bounds[3]>.98f){
        UINT vertex=first;if(indexed){UINT size=s.indices.Format==DXGI_FORMAT_R16_UINT?2:4;vertex=0;if(!copyConstants(s.indices.BufferLocation+UINT64(first)*size,size,reinterpret_cast<unsigned char*>(&vertex)))return;vertex+=base;}
        float bias[4]{},output[4]{};bool valid=spriteAttribute(s,info.layout.colorBias,vertex,firstInstance,bias);
        for(UINT row=0;row<4;row++){float matrix[4]{};valid=spriteAttribute(s,info.layout.colorRows[row],vertex,firstInstance,matrix)&&valid;output[row]=bias[row];for(UINT c=0;c<4;c++)output[row]+=matrix[c]*solidColor[c]*color[c];}
        if(valid&&fabsf(output[0])<.01f&&fabsf(output[1])<.01f&&fabsf(output[2])<.01f){float alpha=(std::max)(0.f,(std::min)(1.f,output[3]));std::lock_guard<std::mutex> guard(mapMutex);pendingFadeAlpha=1-(1-pendingFadeAlpha)*(1-alpha);++uiDraws;return;}
    }
    if(!bounded&&(worldSeen||haveWorldFrame)){static std::atomic<unsigned> failures{};if(uiFailureTrace&&failures.fetch_add(1)<12)uiFailureTrace(s,info,count);}
    // An opaque intermediate sprite does not imply an opaque HUD layer.
    // Preserve the captured alpha; full menu backgrounds supply their own alpha.
    if(bounded&&(worldSeen||haveWorldFrame)) {
        static std::atomic<unsigned> traced{};if(uiTrace&&traced.fetch_add(1)<80)uiTrace(s,count,bounds,color);
        // Remove only small translucent solid black panels. Full-screen fades,
        // opaque menu backgrounds, and textured text outlines remain intact.
        bool compactPanel=bounds[2]-bounds[0]<1.8f&&bounds[3]-bounds[1]<.8f;
        if(compactPanel&&!info.usesTexture&&color[0]<.01f&&color[1]<.01f&&color[2]<.01f&&color[3]<.9f)return;
    }
    if(bounded&&info.usesTexture&&info.textureMask==1&&hideNativeStatus&&hideNativeStatus(bounds,spriteWidth,spriteHeight,count))return;
    if(bounded&&info.usesTexture&&info.textureMask==1&&nativeUiGameplay&&nativeUiText::isReloadMessage(spriteWidth,spriteHeight,count,nativeUiGameplay(),[&](UINT i,float* uv){
        UINT vertex=first+i;if(indexed){UINT bytes=s.indices.Format==DXGI_FORMAT_R16_UINT?2:s.indices.Format==DXGI_FORMAT_R32_UINT?4:0;UINT index{};
            if(!bytes||UINT64(vertex+1)*bytes>s.indices.SizeInBytes||!copyConstants(s.indices.BufferLocation+UINT64(vertex)*bytes,bytes,reinterpret_cast<unsigned char*>(&index))||INT64(index)+base<0)return false;vertex=UINT(INT64(index)+base);}
        return spriteAttribute(s,info.layout.uv,vertex,firstInstance,uv);
    })){if(nativeUiReloadSuppressed)nativeUiReloadSuppressed();return;}

    D3D12_VIEWPORT viewport{0,0,static_cast<float>(desktopWidth),static_cast<float>(desktopHeight),0,1};
    std::array<D3D12_RECT,16> scissors=s.scissors;
    if(s.viewportCount&&s.viewports[0].Width>0&&s.viewports[0].Height>0) {
        float sx=desktopWidth/s.viewports[0].Width,sy=desktopHeight/s.viewports[0].Height;
        for(UINT i=0;i<s.scissorCount;i++)scissors[i]={static_cast<LONG>(s.scissors[i].left*sx),static_cast<LONG>(s.scissors[i].top*sy),static_cast<LONG>(s.scissors[i].right*sx),static_cast<LONG>(s.scissors[i].bottom*sy)};
    }
    if(bounded&&(worldSeen||haveWorldFrame)&&count<=6&&bounds[2]-bounds[0]<.08f&&bounds[3]-bounds[1]<.1f&&fabsf(bounds[0]+bounds[2])<.08f&&fabsf(bounds[1]+bounds[3])<.08f) {
        if(frameScopeFactor>1.01f)return; // One controller-aligned reticle is drawn after the native UI.
        auto center=xr->eyes[0].view;
        center.pose.position.x=(center.pose.position.x+xr->eyes[1].view.pose.position.x)*.5f;
        center.pose.position.y=(center.pose.position.y+xr->eyes[1].view.pose.position.y)*.5f;
        center.pose.position.z=(center.pose.position.z+xr->eyes[1].view.pose.position.z)*.5f;
        float x,y;if(!gunAimOffset(center,x,y))return;
        viewport.TopLeftX=x*desktopWidth;viewport.TopLeftY=y*desktopHeight;
        for(UINT i=0;i<s.scissorCount;i++){scissors[i].left+=static_cast<LONG>(viewport.TopLeftX);scissors[i].right+=static_cast<LONG>(viewport.TopLeftX);scissors[i].top+=static_cast<LONG>(viewport.TopLeftY);scissors[i].bottom+=static_cast<LONG>(viewport.TopLeftY);}
    }
    list->RSSetViewports(1,&viewport);if(s.scissorCount)list->RSSetScissorRects(s.scissorCount,scissors.data());
    const bool replacePrompt=bounded&&info.promptPipeline&&
        ((nativeUiGameplay&&nativeUiText::interactionButton(bounds,spriteWidth,spriteHeight,count,nativeUiGameplay()))||
         nativeUiText::menuButton(bounds,spriteWidth,spriteHeight,count));
    list->SetPipelineState(replacePrompt?info.promptPipeline.Get():info.uiPipeline.Get());
    list->OMSetRenderTargets(1,&uiRTV,FALSE,s.hasDepth?&uiDSV:nullptr);draw();++uiDraws;
    list->OMSetRenderTargets(s.rtvCount,s.rtvs.data(),FALSE,s.hasDepth?&s.dsv:nullptr);
    list->RSSetViewports(s.viewportCount,s.viewports.data());list->RSSetScissorRects(s.scissorCount,s.scissors.data());
    list->SetPipelineState(s.pipeline);
}
}

