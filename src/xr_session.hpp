#include "init_diagnostics.hpp"
#pragma once
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D12
#define XR_NO_PROTOTYPES
#include <windows.h>
#include <d3d12.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <string>
#include <cstdio>
#include <wrl/client.h>
#include "xr_controls.hpp"
#include "desktop_test_pose.hpp"

// The game integration must render TWO independent views. No flat-frame fallback.
// All calls belong on the rendering thread; the caller must finish GPU writes before endFrame.
class XrSessionBridge {
public:
    struct Eye {
        XrSwapchain swapchain{};
        uint32_t width{},height{},index{};
        bool acquired=false;
        std::vector<XrSwapchainImageD3D12KHR> images;
        XrView view{XR_TYPE_VIEW};
        ID3D12Resource* texture() const { return images.at(index).texture; }
    };
    Eye eyes[2];
    // Explicit desktop diagnostic mode still renders both independent eye
    // views through the production stereo path. It never substitutes for HMD
    // acceptance testing and is never enabled as a runtime failure fallback.
    bool desktopDiagnostic{};
    std::atomic<unsigned> diagnosticView{};
    Microsoft::WRL::ComPtr<ID3D12Device> diagnosticDevice;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> diagnosticImages;
    void diagnosticImage(Eye& eye,uint32_t width,uint32_t height){
        D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=width;d.Height=height;
        d.DepthOrArraySize=1;d.MipLevels=1;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.SampleDesc.Count=1;
        d.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        if(FAILED(diagnosticDevice->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_RENDER_TARGET,nullptr,IID_PPV_ARGS(&resource))))throw std::runtime_error("Desktop stereo image allocation failed");
        eye.width=width;eye.height=height;eye.images.resize(1,{XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});eye.images[0].texture=resource.Get();
        diagnosticImages.push_back(resource);
    }
    void initializeDesktop(ID3D12Device* device){
        desktopDiagnostic=true;diagnosticDevice=device;colorFormat=DXGI_FORMAT_R8G8B8A8_UNORM;
        state=XR_SESSION_STATE_FOCUSED;running=true;
        for(int i=0;i<2;i++){diagnosticImage(eyes[i],1440,1440);eyes[i].view.pose.orientation.w=1;eyes[i].view.pose.position.x=i?.032f:-.032f;eyes[i].view.fov={-.785398f,.785398f,.785398f,-.785398f};}
    }
    XrInstance instance{};
    XrSession session{};
    XrSpace space{};
    XrSpace viewSpace{};
    XrSpace floorSpace{};bool floorHeightValid{};float floorHeight{};
    Eye overlay;
    Eye fade;
    vrControls::OpenXRControls controls;
    int64_t colorFormat{};
    XrSystemId system{};
    char runtimeName[XR_MAX_RUNTIME_NAME_SIZE]{},systemName[XR_MAX_SYSTEM_NAME_SIZE]{};
    XrSessionState state=XR_SESSION_STATE_UNKNOWN;
    bool running=false, frameBegun=false, render=false, headTracked=false;
    XrTime predictedTime{};
    XrDuration predictedPeriod{};
    struct ReferenceChange {XrTime time;bool valid;XrPosef pose;};
    std::vector<ReferenceChange> pendingReferenceChanges,referenceChanges;
    double releaseMilliseconds{},endMilliseconds{};
    static double milliseconds(){LARGE_INTEGER t,f;QueryPerformanceCounter(&t);QueryPerformanceFrequency(&f);return 1000.*t.QuadPart/f.QuadPart;}
    PFN_xrGetInstanceProcAddr get{};
    HMODULE loader{};
    XrDebugUtilsMessengerEXT debugMessenger{};
    PFN_xrDestroyDebugUtilsMessengerEXT destroyDebug{};
    static XrBool32 XRAPI_PTR debugCallback(XrDebugUtilsMessageSeverityFlagsEXT, XrDebugUtilsMessageTypeFlagsEXT,
        const XrDebugUtilsMessengerCallbackDataEXT* data, void*) {
        fprintf(stderr,"OpenXR [%s] %s\n",data->functionName?data->functionName:"",data->message?data->message:"");
        return XR_FALSE;
    }
#define XR_PROC(name) PFN_##name name{}
    XR_PROC(xrDestroyInstance); XR_PROC(xrGetSystem); XR_PROC(xrCreateSession);
    XR_PROC(xrDestroySession); XR_PROC(xrCreateReferenceSpace); XR_PROC(xrDestroySpace);
    XR_PROC(xrEnumerateViewConfigurationViews); XR_PROC(xrEnumerateSwapchainFormats);
    XR_PROC(xrCreateSwapchain); XR_PROC(xrDestroySwapchain); XR_PROC(xrEnumerateSwapchainImages);
    XR_PROC(xrPollEvent); XR_PROC(xrBeginSession); XR_PROC(xrEndSession);
    XR_PROC(xrWaitFrame); XR_PROC(xrBeginFrame); XR_PROC(xrEndFrame); XR_PROC(xrLocateViews);
    XR_PROC(xrAcquireSwapchainImage); XR_PROC(xrWaitSwapchainImage); XR_PROC(xrReleaseSwapchainImage);
    XR_PROC(xrGetD3D12GraphicsRequirementsKHR);
    XR_PROC(xrLocateSpace);
#undef XR_PROC
    template<class T> void load(const char* name,T& out) {
        check(get(instance,name,reinterpret_cast<PFN_xrVoidFunction*>(&out)),name);
    }
    void check(XrResult r,const char* call) {
        if(XR_FAILED(r)){char text[XR_MAX_RESULT_STRING_SIZE]="unavailable (no instance/result formatter)";PFN_xrResultToString describe{};if(instance&&get&&XR_SUCCEEDED(get(instance,"xrResultToString",reinterpret_cast<PFN_xrVoidFunction*>(&describe)))&&describe)describe(instance,r,text);initDiagnostics::write("OpenXR api=%s result=%d description=%s",call,int(r),text);}
        if(XR_FAILED(r)) throw std::runtime_error(std::string(call)+" failed: "+std::to_string(r));
    }
    void initialize(const wchar_t* loaderPath,ID3D12Device* device,ID3D12CommandQueue* queue,float renderScale=1.f) {
        if(instance) throw std::runtime_error("Already initialized");
        initDiagnostics::mark("OpenXR loader");initDiagnostics::write("loader_requested=%ls",loaderPath);
        loader=LoadLibraryExW(loaderPath,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if(!loader){DWORD e=GetLastError();initDiagnostics::os("LoadLibraryExW(OpenXR)",e);throw std::runtime_error("OpenXR loader unavailable; see initialization log");}
        initDiagnostics::module(loader);
        get=reinterpret_cast<PFN_xrGetInstanceProcAddr>(GetProcAddress(loader,"xrGetInstanceProcAddr"));
        if(!get) throw std::runtime_error("OpenXR loader missing entry point");
        PFN_xrCreateInstance create{}; load("xrCreateInstance",create);
        const char* extensions[]={XR_KHR_D3D12_ENABLE_EXTENSION_NAME,XR_EXT_DEBUG_UTILS_EXTENSION_NAME};
        XrInstanceCreateInfo ci{XR_TYPE_INSTANCE_CREATE_INFO};
        strcpy_s(ci.applicationInfo.applicationName,"DementiumOpenXR");
        ci.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,34);
        ci.enabledExtensionCount=2; ci.enabledExtensionNames=extensions;
        initDiagnostics::mark("xrCreateInstance");check(create(&ci,&instance),"xrCreateInstance");initDiagnostics::modules();
        PFN_xrCreateDebugUtilsMessengerEXT createDebug{};
        load("xrCreateDebugUtilsMessengerEXT",createDebug); load("xrDestroyDebugUtilsMessengerEXT",destroyDebug);
        XrDebugUtilsMessengerCreateInfoEXT debug{XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        debug.messageSeverities=XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT|XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        debug.messageTypes=XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug.userCallback=debugCallback; check(createDebug(instance,&debug,&debugMessenger),"create debug messenger");
#define LOAD(name) load(#name,name)
        LOAD(xrDestroyInstance); LOAD(xrGetSystem); LOAD(xrCreateSession); LOAD(xrDestroySession);
        LOAD(xrCreateReferenceSpace); LOAD(xrDestroySpace); LOAD(xrEnumerateViewConfigurationViews);
        LOAD(xrEnumerateSwapchainFormats); LOAD(xrCreateSwapchain); LOAD(xrDestroySwapchain);
        LOAD(xrEnumerateSwapchainImages); LOAD(xrPollEvent); LOAD(xrBeginSession); LOAD(xrEndSession);
        LOAD(xrWaitFrame); LOAD(xrBeginFrame); LOAD(xrEndFrame); LOAD(xrLocateViews);
        LOAD(xrAcquireSwapchainImage); LOAD(xrWaitSwapchainImage); LOAD(xrReleaseSwapchainImage);
        LOAD(xrGetD3D12GraphicsRequirementsKHR);
        LOAD(xrLocateSpace);
#undef LOAD
        XrSystemGetInfo si{XR_TYPE_SYSTEM_GET_INFO}; si.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        check(xrGetSystem(instance,&si,&system),"xrGetSystem");
        // Read-only capabilities; telemetry failure must not disable VR.
        PFN_xrGetInstanceProperties properties{};PFN_xrGetSystemProperties systemProperties{};
        if(XR_SUCCEEDED(get(instance,"xrGetInstanceProperties",reinterpret_cast<PFN_xrVoidFunction*>(&properties)))&&properties){XrInstanceProperties p{XR_TYPE_INSTANCE_PROPERTIES};if(XR_SUCCEEDED(properties(instance,&p))){strcpy_s(runtimeName,p.runtimeName);initDiagnostics::write("OpenXR_runtime=%s version=%llu",p.runtimeName,static_cast<unsigned long long>(p.runtimeVersion));}}
        if(XR_SUCCEEDED(get(instance,"xrGetSystemProperties",reinterpret_cast<PFN_xrVoidFunction*>(&systemProperties)))&&systemProperties){XrSystemProperties p{XR_TYPE_SYSTEM_PROPERTIES};if(XR_SUCCEEDED(systemProperties(instance,system,&p)))strcpy_s(systemName,p.systemName);}

        XrGraphicsRequirementsD3D12KHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR};
        check(xrGetD3D12GraphicsRequirementsKHR(instance,system,&requirements),"xrGetD3D12GraphicsRequirementsKHR");
        LUID actual=device->GetAdapterLuid();initDiagnostics::write("runtime=%s headset=%s device_LUID=%08lx:%08lx required_LUID=%08lx:%08lx min_feature_level=0x%x",runtimeName,systemName,actual.HighPart,actual.LowPart,requirements.adapterLuid.HighPart,requirements.adapterLuid.LowPart,requirements.minFeatureLevel);
        if(memcmp(&actual,&requirements.adapterLuid,sizeof(LUID))) throw std::runtime_error("Game GPU does not match OpenXR GPU");
        D3D12_FEATURE_DATA_FEATURE_LEVELS levels{1,&requirements.minFeatureLevel,{}};
        if(FAILED(device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS,&levels,sizeof(levels))))
            throw std::runtime_error("Game device lacks required OpenXR feature level");
        XrGraphicsBindingD3D12KHR binding{XR_TYPE_GRAPHICS_BINDING_D3D12_KHR}; binding.device=device; binding.queue=queue;
        XrSessionCreateInfo sci{XR_TYPE_SESSION_CREATE_INFO}; sci.next=&binding; sci.systemId=system;
        initDiagnostics::mark("xrCreateSession");check(xrCreateSession(instance,&sci,&session),"xrCreateSession");
        XrReferenceSpaceCreateInfo rs{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        rs.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL; rs.poseInReferenceSpace.orientation.w=1;
        check(xrCreateReferenceSpace(session,&rs,&space),"xrCreateReferenceSpace");
        // STAGE supplies a floor height independently of LOCAL recentering.
        // An unsupported floor space leaves posture explicitly unavailable.
        rs.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_STAGE;
        XrResult floorResult=xrCreateReferenceSpace(session,&rs,&floorSpace);
        if(XR_FAILED(floorResult)){floorSpace=XR_NULL_HANDLE;if(floorResult!=XR_ERROR_REFERENCE_SPACE_UNSUPPORTED)check(floorResult,"create floor reference");}
        uint32_t n=0;
        check(xrEnumerateViewConfigurationViews(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,0,&n,nullptr),"enumerate views");
        if(n!=2) throw std::runtime_error("Expected two stereo views");
        XrViewConfigurationView views[2]={{XR_TYPE_VIEW_CONFIGURATION_VIEW},{XR_TYPE_VIEW_CONFIGURATION_VIEW}};
        check(xrEnumerateViewConfigurationViews(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,2,&n,views),"enumerate views");
        check(xrEnumerateSwapchainFormats(session,0,&n,nullptr),"enumerate formats");
        std::vector<int64_t> formats(n); check(xrEnumerateSwapchainFormats(session,n,&n,formats.data()),"enumerate formats");
        int64_t chosen=0;
        for(auto preferred:{DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,DXGI_FORMAT_B8G8R8A8_UNORM_SRGB})
            if(!chosen) for(auto f:formats) if(f==preferred) chosen=f;
        if(!chosen) throw std::runtime_error("No supported sRGB eye format");
        colorFormat=chosen;
        for(int i=0;i<2;i++) {
            auto& e=eyes[i]; e.width=static_cast<uint32_t>(views[i].recommendedImageRectWidth*renderScale)&~3u; e.height=static_cast<uint32_t>(views[i].recommendedImageRectHeight*renderScale)&~3u;
            XrSwapchainCreateInfo sc{XR_TYPE_SWAPCHAIN_CREATE_INFO};
            sc.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT|XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
            sc.format=chosen; sc.sampleCount=1; sc.width=e.width; sc.height=e.height; sc.faceCount=1; sc.arraySize=1; sc.mipCount=1;
            check(xrCreateSwapchain(session,&sc,&e.swapchain),"xrCreateSwapchain");
            check(xrEnumerateSwapchainImages(e.swapchain,0,&n,nullptr),"enumerate images");
            e.images.resize(n,{XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
            check(xrEnumerateSwapchainImages(e.swapchain,n,&n,reinterpret_cast<XrSwapchainImageBaseHeader*>(e.images.data())),"enumerate images");
        }
        controls.initialize(instance,session,get);
    }
    void createOverlay(uint32_t width,uint32_t height) {
        if(desktopDiagnostic){diagnosticImage(overlay,width,height);diagnosticImage(fade,1,1);return;}
        XrReferenceSpaceCreateInfo rs{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};rs.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_VIEW;rs.poseInReferenceSpace.orientation.w=1;
        check(xrCreateReferenceSpace(session,&rs,&viewSpace),"create HUD view space");
        overlay.width=width;overlay.height=height;
        XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};ci.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT|XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
        ci.format=colorFormat;ci.sampleCount=1;ci.width=width;ci.height=height;ci.faceCount=1;ci.arraySize=1;ci.mipCount=1;
        check(xrCreateSwapchain(session,&ci,&overlay.swapchain),"create HUD swapchain");
        uint32_t n=0;check(xrEnumerateSwapchainImages(overlay.swapchain,0,&n,nullptr),"enumerate HUD images");
        overlay.images.resize(n,{XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
        check(xrEnumerateSwapchainImages(overlay.swapchain,n,&n,reinterpret_cast<XrSwapchainImageBaseHeader*>(overlay.images.data())),"enumerate HUD images");
        fade.width=fade.height=1;ci.width=ci.height=1;
        check(xrCreateSwapchain(session,&ci,&fade.swapchain),"create full-view fade swapchain");
        check(xrEnumerateSwapchainImages(fade.swapchain,0,&n,nullptr),"enumerate fade images");fade.images.resize(n,{XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
        check(xrEnumerateSwapchainImages(fade.swapchain,n,&n,reinterpret_cast<XrSwapchainImageBaseHeader*>(fade.images.data())),"enumerate fade images");
    }
    bool beginFrame() {
        if(desktopDiagnostic){
            if(frameBegun)throw std::runtime_error("Previous desktop stereo frame not ended");
            unsigned mode=diagnosticView.load();
            const auto testPose=desktopTestPose::make(mode);
            XrQuaternionf q{testPose.pitchX,testPose.yawY,0,testPose.w};
            auto rotation=motionTracking::rotation(q);
            for(int i=0;i<2;i++){
                DirectX::XMFLOAT3 offset;DirectX::XMStoreFloat3(&offset,DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(i?.032f:-.032f,0,0,0),rotation));
                eyes[i].view.pose={q,{offset.x+testPose.x,offset.y+testPose.y,offset.z+testPose.z}};eyes[i].acquired=true;
            }
            headTracked=true;frameBegun=render=true;overlay.acquired=fade.acquired=true;return true;
        }
        headTracked=false;
        floorHeightValid=false;
        if(frameBegun) throw std::runtime_error("Previous frame not ended");
        XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
        XrResult result{};
        while((result=xrPollEvent(instance,&event))==XR_SUCCESS) {
            if(event.type==XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                state=reinterpret_cast<XrEventDataSessionStateChanged*>(&event)->state;
                if(state==XR_SESSION_STATE_READY) {
                    XrSessionBeginInfo info{XR_TYPE_SESSION_BEGIN_INFO}; info.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    check(xrBeginSession(session,&info),"xrBeginSession"); running=true;
                } else if(state==XR_SESSION_STATE_STOPPING) { check(xrEndSession(session),"xrEndSession"); running=false; }
                else if(state==XR_SESSION_STATE_EXITING||state==XR_SESSION_STATE_LOSS_PENDING) running=false;
            } else if(event.type==XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) running=false;
            else if(event.type==XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING){
                auto change=reinterpret_cast<XrEventDataReferenceSpaceChangePending*>(&event);
                if(change->session==session&&change->referenceSpaceType==XR_REFERENCE_SPACE_TYPE_LOCAL)pendingReferenceChanges.push_back({change->changeTime,change->poseValid!=0,change->poseInPreviousSpace});
            }
            event={XR_TYPE_EVENT_DATA_BUFFER};
        }
        check(result,"xrPollEvent");
        if(!running) return false;
        XrFrameWaitInfo wait{XR_TYPE_FRAME_WAIT_INFO}; XrFrameState fs{XR_TYPE_FRAME_STATE};
        check(xrWaitFrame(session,&wait,&fs),"xrWaitFrame");
        XrFrameBeginInfo bi{XR_TYPE_FRAME_BEGIN_INFO}; check(xrBeginFrame(session,&bi),"xrBeginFrame");
        frameBegun=true; predictedTime=fs.predictedDisplayTime; predictedPeriod=fs.predictedDisplayPeriod; render=fs.shouldRender!=0;
        for(auto it=pendingReferenceChanges.begin();it!=pendingReferenceChanges.end();){if(predictedTime>=it->time){referenceChanges.push_back(*it);it=pendingReferenceChanges.erase(it);}else ++it;}
        if(!render) return false;
        XrViewLocateInfo li{XR_TYPE_VIEW_LOCATE_INFO}; li.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        li.displayTime=predictedTime; li.space=space;
        XrViewState vs{XR_TYPE_VIEW_STATE}; XrView v[2]={{XR_TYPE_VIEW},{XR_TYPE_VIEW}}; uint32_t n=0;
        check(xrLocateViews(session,&li,&vs,2,&n,v),"xrLocateViews");
        auto flags=XR_VIEW_STATE_ORIENTATION_VALID_BIT|XR_VIEW_STATE_POSITION_VALID_BIT;
        if(n!=2||(vs.viewStateFlags&flags)!=flags||!motionTracking::validPose(v[0].pose)||!motionTracking::validPose(v[1].pose)) {render=false; return false;}
        auto tracked=XR_VIEW_STATE_ORIENTATION_TRACKED_BIT|XR_VIEW_STATE_POSITION_TRACKED_BIT;
        headTracked=(vs.viewStateFlags&tracked)==tracked;
        if(floorSpace&&viewSpace){XrSpaceLocation floor{XR_TYPE_SPACE_LOCATION};auto r=xrLocateSpace(viewSpace,floorSpace,predictedTime,&floor);
            auto required=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
            floorHeightValid=XR_SUCCEEDED(r)&&(floor.locationFlags&required)==required&&std::isfinite(floor.pose.position.y)&&floor.pose.position.y>.3f&&floor.pose.position.y<2.7f;
            if(floorHeightValid)floorHeight=floor.pose.position.y;
        }
        for(int i=0;i<2;i++) {
            auto& e=eyes[i]; e.view=v[i]; XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            check(xrAcquireSwapchainImage(e.swapchain,&ai,&e.index),"xrAcquireSwapchainImage"); e.acquired=true;
            XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO}; wi.timeout=XR_INFINITE_DURATION;
            check(xrWaitSwapchainImage(e.swapchain,&wi),"xrWaitSwapchainImage");
        }
        if(overlay.swapchain) {
            XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};check(xrAcquireSwapchainImage(overlay.swapchain,&ai,&overlay.index),"acquire HUD image");overlay.acquired=true;
            XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wi.timeout=XR_INFINITE_DURATION;check(xrWaitSwapchainImage(overlay.swapchain,&wi),"wait HUD image");
        }
        if(fade.swapchain){XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};check(xrAcquireSwapchainImage(fade.swapchain,&ai,&fade.index),"acquire fade image");fade.acquired=true;XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wi.timeout=XR_INFINITE_DURATION;check(xrWaitSwapchainImage(fade.swapchain,&wi),"wait fade image");}
        controls.update(predictedTime,space,GetTickCount64());return true;
    }
    void endFrame(bool bothEyesRendered,bool overlayRendered=false,bool overlayOpaque=false,float fadeAlpha=0) {
        if(desktopDiagnostic){for(auto& e:eyes)e.acquired=false;overlay.acquired=fade.acquired=false;frameBegun=render=false;return;}
        if(!frameBegun) return;
        double releaseStarted=milliseconds();
        XrCompositionLayerProjectionView views[2]={{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}};
        bool complete=render&&bothEyesRendered&&eyes[0].acquired&&eyes[1].acquired;
        for(int i=0;i<2;i++) {
            auto& e=eyes[i];
            if(e.acquired) { XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO}; check(xrReleaseSwapchainImage(e.swapchain,&ri),"xrReleaseSwapchainImage"); e.acquired=false; }
            views[i].pose=e.view.pose; views[i].fov=e.view.fov; views[i].subImage.swapchain=e.swapchain;
            views[i].subImage.imageRect.extent={static_cast<int32_t>(e.width),static_cast<int32_t>(e.height)};
        }
        XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION}; layer.space=space; layer.viewCount=2; layer.views=views;
        bool hud=render&&overlayRendered&&overlay.acquired;
        if(overlay.acquired) {XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};check(xrReleaseSwapchainImage(overlay.swapchain,&ri),"release HUD image");overlay.acquired=false;}
        XrCompositionLayerQuad quad{XR_TYPE_COMPOSITION_LAYER_QUAD};quad.layerFlags=complete&&!overlayOpaque?XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT:0;
        quad.space=viewSpace;quad.eyeVisibility=XR_EYE_VISIBILITY_BOTH;quad.subImage.swapchain=overlay.swapchain;
        quad.subImage.imageRect.extent={static_cast<int32_t>(overlay.width),static_cast<int32_t>(overlay.height)};
        quad.pose.orientation.w=1;quad.pose.position.z=-1.4f;quad.size.width=2.f;
        quad.size.height=overlay.width?2.f*overlay.height/overlay.width:1.f;
        bool showFade=(complete||hud)&&fade.acquired&&fadeAlpha>0;
        if(fade.acquired){XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};check(xrReleaseSwapchainImage(fade.swapchain,&ri),"release fade image");fade.acquired=false;}
        auto fadeQuad=quad;fadeQuad.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;fadeQuad.subImage.swapchain=fade.swapchain;fadeQuad.subImage.imageRect.extent={1,1};fadeQuad.pose.position.z=-.1f;fadeQuad.size={20,20};
        const XrCompositionLayerBaseHeader* layers[3]{};uint32_t count=0;
        if(complete)layers[count++]=reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer);
        if(hud)layers[count++]=reinterpret_cast<XrCompositionLayerBaseHeader*>(&quad);
        if(showFade)layers[count++]=reinterpret_cast<XrCompositionLayerBaseHeader*>(&fadeQuad);
        XrFrameEndInfo ei{XR_TYPE_FRAME_END_INFO}; ei.displayTime=predictedTime; ei.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        ei.layerCount=count;ei.layers=count?layers:nullptr;
        releaseMilliseconds+=milliseconds()-releaseStarted;double endStarted=milliseconds();XrResult r=xrEndFrame(session,&ei);endMilliseconds+=milliseconds()-endStarted; frameBegun=false; render=false; check(r,"xrEndFrame");
    }
    ~XrSessionBridge() {
        controls.shutdown();
        if(frameBegun) { try {endFrame(false);} catch(...) {} }
        for(auto& e:eyes) if(e.swapchain&&xrDestroySwapchain) xrDestroySwapchain(e.swapchain);
        if(overlay.swapchain&&xrDestroySwapchain)xrDestroySwapchain(overlay.swapchain);
        if(fade.swapchain&&xrDestroySwapchain)xrDestroySwapchain(fade.swapchain);
        if(viewSpace&&xrDestroySpace)xrDestroySpace(viewSpace);
        if(floorSpace&&xrDestroySpace)xrDestroySpace(floorSpace);
        if(space&&xrDestroySpace) xrDestroySpace(space);
        if(session&&xrDestroySession) xrDestroySession(session);
        if(debugMessenger&&destroyDebug) destroyDebug(debugMessenger);
        if(instance&&xrDestroyInstance) xrDestroyInstance(instance);
        if(loader) FreeLibrary(loader);
    }
};
