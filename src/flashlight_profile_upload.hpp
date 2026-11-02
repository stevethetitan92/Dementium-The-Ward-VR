#pragma once
#include "flashlight_profile_range.hpp"
#include <d3d12.h>
#include <wrl/client.h>
#include <mutex>
#include <vector>
#include <cstring>
#include <atomic>

namespace flashlightProfileUpload {
inline std::atomic<bool> enabled{false};
// Immutable, bounded process-lifetime uploads: command lists can outlive the
// hook invocation. Never recycle or rewrite a resource still used by the GPU.
struct Entry {
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
    std::array<float,257> values;
};
inline ID3D12Resource* get(ID3D12Resource* destination,const std::array<float,257>& values){
    static auto* entries=new std::vector<Entry>();
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    if(FAILED(destination->GetDevice(IID_PPV_ARGS(&device))))return nullptr;
    for(auto& entry:*entries)if(entry.device.Get()==device.Get()&&entry.values==values)return entry.buffer.Get();
    if(entries->size()>=8)return nullptr; // Fail open to native upload.
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;desc.Width=1280;desc.Height=1;desc.DepthOrArraySize=1;desc.MipLevels=1;desc.SampleDesc.Count=1;desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    Entry entry;entry.device=device;entry.values=values;
    if(FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&entry.buffer))))return nullptr;
    void* mapped{};D3D12_RANGE empty{0,0};
    if(FAILED(entry.buffer->Map(0,&empty,&mapped)))return nullptr;
    std::memset(mapped,0,1280);std::memcpy(mapped,values.data(),sizeof(float)*257);
    D3D12_RANGE written{0,1280};entry.buffer->Unmap(0,&written);
    entries->push_back(std::move(entry));return entries->back().buffer.Get();
}
}
