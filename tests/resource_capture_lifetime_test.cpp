#include "../src/resource_capture_lifetime.hpp"
#include "../src/upload_capture.hpp"
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cassert>
#include <cstdio>
using Microsoft::WRL::ComPtr;
int main(){
    ComPtr<IDXGIFactory4> factory;assert(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))));
    ComPtr<IDXGIAdapter> adapter;assert(SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))));
    ComPtr<ID3D12Device> device;assert(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))));
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;desc.Width=1152;desc.Height=1;desc.DepthOrArraySize=1;desc.MipLevels=1;desc.SampleDesc.Count=1;desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    unsigned destroyed=0;
    for(unsigned n=0;n<10000;++n){
        ComPtr<ID3D12Resource> native;
        assert(SUCCEEDED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&native))));
        assert(resourceCaptureLifetime::attach(native.Get(),[&]{++destroyed;}));
        ComPtr<ID3D12Resource> capturePin=native;
        UploadCapture capture;void* mapped=nullptr;D3D12_RANGE noRead{0,0};
        assert(SUCCEEDED(native->Map(0,&noRead,&mapped)));capture.mapped(mapped,1152);
        static_cast<unsigned char*>(mapped)[0]=77;
        capture.unmapped(0,1152);D3D12_RANGE written{0,1152};native->Unmap(0,&written);
        assert(!capture.mappings&&capture.cpu==capture.shadow.data());
        capturePin.Reset();assert(destroyed==n); // native ownership still valid
        assert(capture.canRead(0,1)&&capture.cpu[0]==77);
        native.Reset();assert(destroyed==n+1); // private interface has no resource cycle
    }
    std::printf("PASS: 10000 upload lifetimes; shadows readable after capture unpin; destruction notifications exact.\n");
}
