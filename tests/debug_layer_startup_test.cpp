#include "../src/debug_layer_startup.hpp"
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <cassert>
#include <cstring>
#include <iostream>
using Microsoft::WRL::ComPtr;
int main(int argc,char** argv){
    const bool suppress=argc==2&&!std::strcmp(argv[1],"--off");
    ComPtr<ID3D12Debug> debug;
    assert(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))));
    assert(MH_Initialize()==MH_OK);
    if(suppress) assert(debugLayerStartup::suppressEnable(debug.Get()));
    // Exercise the real API as both bootstrap and the native game would.
    debug->EnableDebugLayer();
    ComPtr<ID3D12Debug> second;
    assert(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&second))));
    second->EnableDebugLayer();
    ComPtr<IDXGIFactory4> factory;assert(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))));
    ComPtr<IDXGIAdapter> warp;assert(SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp))));
    ComPtr<ID3D12Device> device;assert(SUCCEEDED(D3D12CreateDevice(warp.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))));
    ComPtr<ID3D12InfoQueue> info;
    const bool validation=SUCCEEDED(device.As(&info));
    assert(validation!=suppress);
    assert(SUCCEEDED(device->GetDeviceRemovedReason()));
    std::cout << "PASS: fresh WARP device validation=" << validation << "; repeated native/bootstrap enable requests, no game attachment\n";
    info.Reset();device.Reset();MH_Uninitialize();
}
