#pragma once
#include <d3d12.h>
#include <wrl/client.h>
namespace deviceOwnership {
inline bool belongs(ID3D12DeviceChild* child,ID3D12Device* expected){
    if(!child||!expected)return false;
    Microsoft::WRL::ComPtr<ID3D12Device> actual;
    Microsoft::WRL::ComPtr<IUnknown> a,b;
    return SUCCEEDED(child->GetDevice(IID_PPV_ARGS(&actual)))&&SUCCEEDED(actual.As(&a))&&SUCCEEDED(expected->QueryInterface(IID_PPV_ARGS(&b)))&&a.Get()==b.Get();
}
}
