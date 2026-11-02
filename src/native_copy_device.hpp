#pragma once
#include <d3d12.h>
#include <wrl/client.h>
namespace nativeCopyDevice {
// Unused admission prerequisite. COM identity establishes ownership only;
// signatures, native list identity, copy layouts and recording state are separate.
inline bool matches(ID3D12Device* expected,ID3D12GraphicsCommandList* list,ID3D12Resource* source,ID3D12Resource* destination){
    if(!expected||!list||!source||!destination||source==destination||list->GetType()!=D3D12_COMMAND_LIST_TYPE_COPY)return false;
    Microsoft::WRL::ComPtr<IUnknown> deviceIdentity,listDevice,sourceDevice,destinationDevice;
    if(FAILED(expected->QueryInterface(IID_PPV_ARGS(&deviceIdentity)))||
       FAILED(list->GetDevice(IID_PPV_ARGS(&listDevice)))||
       FAILED(source->GetDevice(IID_PPV_ARGS(&sourceDevice)))||
       FAILED(destination->GetDevice(IID_PPV_ARGS(&destinationDevice))))return false;
    return deviceIdentity.Get()==listDevice.Get()&&deviceIdentity.Get()==sourceDevice.Get()&&deviceIdentity.Get()==destinationDevice.Get();
}
}
