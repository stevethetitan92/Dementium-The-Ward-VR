#define WIN32_LEAN_AND_MEAN
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D12
#define XR_NO_PROTOTYPES
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <cstdio>
#include <vector>
#include <cstring>

int wmain(int argc, wchar_t** argv) {
    const wchar_t* loader = argc > 1 ? argv[1] : L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\SteamVR\\bin\\win64\\openxr_loader.dll";
    HMODULE module = LoadLibraryExW(loader, nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!module) { printf("loader_error=%lu\n", GetLastError()); return 10; }
    auto get = reinterpret_cast<PFN_xrGetInstanceProcAddr>(GetProcAddress(module, "xrGetInstanceProcAddr"));
    if (!get) { puts("missing_xrGetInstanceProcAddr"); return 11; }
    PFN_xrEnumerateInstanceExtensionProperties enumerate{};
    PFN_xrCreateInstance create{};
    if (XR_FAILED(get(XR_NULL_HANDLE,"xrEnumerateInstanceExtensionProperties",reinterpret_cast<PFN_xrVoidFunction*>(&enumerate))) ||
        XR_FAILED(get(XR_NULL_HANDLE,"xrCreateInstance",reinterpret_cast<PFN_xrVoidFunction*>(&create)))) return 12;
    uint32_t count=0;
    XrResult result=enumerate(nullptr,0,&count,nullptr);
    printf("enumerate_result=%d\n",result);
    if(XR_FAILED(result)) return 13;
    std::vector<XrExtensionProperties> extensions(count,{XR_TYPE_EXTENSION_PROPERTIES});
    result=enumerate(nullptr,count,&count,extensions.data());
    if(XR_FAILED(result)) return 13;
    bool d3d12=false;
    for(auto& e:extensions) { printf("extension=%s\n",e.extensionName); if(!strcmp(e.extensionName,XR_KHR_D3D12_ENABLE_EXTENSION_NAME)) d3d12=true; }
    if(!d3d12) { puts("missing_XR_KHR_D3D12_enable"); return 14; }
    const char* enabled=XR_KHR_D3D12_ENABLE_EXTENSION_NAME;
    XrInstanceCreateInfo ci{XR_TYPE_INSTANCE_CREATE_INFO};
    strcpy_s(ci.applicationInfo.applicationName,"DementiumOpenXR Probe");
    strcpy_s(ci.applicationInfo.engineName,"DementiumOpenXR");
    ci.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,34);
    ci.enabledExtensionCount=1; ci.enabledExtensionNames=&enabled;
    XrInstance instance{}; result=create(&ci,&instance);
    printf("create_instance_result=%d\n",result);
    if(XR_FAILED(result)) return 15;
    PFN_xrDestroyInstance destroy{}; PFN_xrGetInstanceProperties properties{}; PFN_xrGetSystem system{};
    get(instance,"xrDestroyInstance",reinterpret_cast<PFN_xrVoidFunction*>(&destroy));
    get(instance,"xrGetInstanceProperties",reinterpret_cast<PFN_xrVoidFunction*>(&properties));
    get(instance,"xrGetSystem",reinterpret_cast<PFN_xrVoidFunction*>(&system));
    XrInstanceProperties ip{XR_TYPE_INSTANCE_PROPERTIES}; properties(instance,&ip);
    printf("runtime=%s\nruntime_version=%llu\n",ip.runtimeName,static_cast<unsigned long long>(ip.runtimeVersion));
    XrSystemGetInfo si{XR_TYPE_SYSTEM_GET_INFO}; si.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId id{}; result=system(instance,&si,&id);
    char name[XR_MAX_RESULT_STRING_SIZE]{}; PFN_xrResultToString toString{};
    get(instance,"xrResultToString",reinterpret_cast<PFN_xrVoidFunction*>(&toString));
    toString(instance,result,name); printf("get_system_result=%d\nget_system_name=%s\n",result,name);
    if(XR_FAILED(result)) { destroy(instance); return result==XR_ERROR_FORM_FACTOR_UNAVAILABLE ? 20 : 21; }
    PFN_xrGetD3D12GraphicsRequirementsKHR requirements{};
    get(instance,"xrGetD3D12GraphicsRequirementsKHR",reinterpret_cast<PFN_xrVoidFunction*>(&requirements));
    XrGraphicsRequirementsD3D12KHR gr{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR}; result=requirements(instance,id,&gr);
    printf("graphics_requirements_result=%d\nadapter_luid=%08lx:%08lx\nminimum_feature_level=%x\n",result,gr.adapterLuid.HighPart,gr.adapterLuid.LowPart,gr.minFeatureLevel);
    destroy(instance); FreeLibrary(module);
    return XR_FAILED(result) ? 22 : 0;
}
