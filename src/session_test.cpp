#define WIN32_LEAN_AND_MEAN
#include "xr_session.hpp"
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <d3d12sdklayers.h>
#include <cstdio>
using Microsoft::WRL::ComPtr;
static void hr(HRESULT r) { if(FAILED(r)) throw std::runtime_error("D3D12 error: "+std::to_string(r)); }
int main() {
    try {
        ComPtr<ID3D12Debug> debug;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
        const wchar_t* path=L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\SteamVR\\bin\\win64\\openxr_loader.dll";
        LUID luid{}; D3D_FEATURE_LEVEL level{};
        {
            XrSessionBridge discovery;
            discovery.loader=LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            if(!discovery.loader) throw std::runtime_error("Loader missing");
            discovery.get=reinterpret_cast<PFN_xrGetInstanceProcAddr>(GetProcAddress(discovery.loader,"xrGetInstanceProcAddr"));
            PFN_xrCreateInstance create{}; discovery.load("xrCreateInstance",create);
            const char* ext=XR_KHR_D3D12_ENABLE_EXTENSION_NAME;
            XrInstanceCreateInfo ci{XR_TYPE_INSTANCE_CREATE_INFO}; strcpy_s(ci.applicationInfo.applicationName,"DementiumXR GPU discovery");
            ci.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,34); ci.enabledExtensionCount=1; ci.enabledExtensionNames=&ext;
            discovery.check(create(&ci,&discovery.instance),"create discovery instance");
            discovery.load("xrDestroyInstance",discovery.xrDestroyInstance);
            discovery.load("xrGetSystem",discovery.xrGetSystem);
            discovery.load("xrGetD3D12GraphicsRequirementsKHR",discovery.xrGetD3D12GraphicsRequirementsKHR);
            XrSystemGetInfo si{XR_TYPE_SYSTEM_GET_INFO}; si.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
            discovery.check(discovery.xrGetSystem(discovery.instance,&si,&discovery.system),"get discovery system");
            XrGraphicsRequirementsD3D12KHR gr{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR};
            discovery.check(discovery.xrGetD3D12GraphicsRequirementsKHR(discovery.instance,discovery.system,&gr),"get GPU requirements");
            luid=gr.adapterLuid; level=gr.minFeatureLevel;
        }
        ComPtr<IDXGIFactory4> factory; hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
        ComPtr<IDXGIAdapter> adapter; hr(factory->EnumAdapterByLuid(luid,IID_PPV_ARGS(&adapter)));
        ComPtr<ID3D12Device> device; hr(D3D12CreateDevice(adapter.Get(),level,IID_PPV_ARGS(&device)));
        ComPtr<ID3D12InfoQueue1> messages;DWORD callbackCookie=0;if(SUCCEEDED(device.As(&messages))){messages->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR,FALSE);messages->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION,FALSE);messages->RegisterMessageCallback([](D3D12_MESSAGE_CATEGORY,D3D12_MESSAGE_SEVERITY severity,D3D12_MESSAGE_ID id,LPCSTR text,void*){if(severity<=D3D12_MESSAGE_SEVERITY_WARNING){printf("D3D12 id=%u %s\n",id,text);fflush(stdout);}},D3D12_MESSAGE_CALLBACK_FLAG_NONE,nullptr,&callbackCookie);}
        ComPtr<ID3D12CommandQueue> queue; D3D12_COMMAND_QUEUE_DESC qd{}; hr(device->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue)));
        ComPtr<ID3D12CommandAllocator> allocator; hr(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
        ComPtr<ID3D12GraphicsCommandList> list; hr(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list))); hr(list->Close());
        ComPtr<ID3D12Fence> fence; hr(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));
        HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        if(!event) throw std::runtime_error("CreateEvent failed");
        ComPtr<ID3D12DescriptorHeap> heap; D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV; hd.NumDescriptors=2;
        hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)));
        XrSessionBridge xr; xr.initialize(path,device.Get(),queue.Get());
        printf("session_created=1\neye_size=%u,%u;%u,%u\n",xr.eyes[0].width,xr.eyes[0].height,xr.eyes[1].width,xr.eyes[1].height); fflush(stdout);
        unsigned frames=0; ULONGLONG deadline=GetTickCount64()+15000;
        while(frames<120&&GetTickCount64()<deadline) {
            bool render=xr.beginFrame();
            if(render) {
                hr(allocator->Reset()); hr(list->Reset(allocator.Get(),nullptr));
                auto rtv=heap->GetCPUDescriptorHandleForHeapStart();
                for(int i=0;i<2;i++) {
                    D3D12_RENDER_TARGET_VIEW_DESC view{};view.Format=static_cast<DXGI_FORMAT>(xr.colorFormat);view.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE2D;device->CreateRenderTargetView(xr.eyes[i].texture(),&view,rtv);
                    const float left[4]={0.015f,0.025f,0.05f,1};
                    const float right[4]={0.025f,0.015f,0.05f,1};
                    list->ClearRenderTargetView(rtv,i==0?left:right,0,nullptr);
                    rtv.ptr+=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
                }
                hr(list->Close()); ID3D12CommandList* lists[]={list.Get()}; queue->ExecuteCommandLists(1,lists);
                hr(queue->Signal(fence.Get(),frames+1)); hr(fence->SetEventOnCompletion(frames+1,event));
                if(WaitForSingleObject(event,5000)!=WAIT_OBJECT_0) throw std::runtime_error("GPU fence timeout");
                if(frames%30==0) printf("frame=%u state=%d left_position=%.4f,%.4f,%.4f right_position=%.4f,%.4f,%.4f\n",frames,xr.state,xr.eyes[0].view.pose.position.x,xr.eyes[0].view.pose.position.y,xr.eyes[0].view.pose.position.z,xr.eyes[1].view.pose.position.x,xr.eyes[1].view.pose.position.y,xr.eyes[1].view.pose.position.z);
                xr.endFrame(true); frames++;
            } else { xr.endFrame(false); Sleep(10); }
        }
        CloseHandle(event); printf("submitted_stereo_test_frames=%u\n",frames);
        return frames==120?0:2;
    } catch(const std::exception& e) { printf("error=%s\n",e.what()); return 1; }
}
