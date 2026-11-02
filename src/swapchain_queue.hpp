#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <mutex>
#include <unordered_map>
namespace swapchainQueue {
using Microsoft::WRL::ComPtr;
struct Binding {ComPtr<IUnknown> identity;ComPtr<ID3D12CommandQueue> queue;};
inline std::mutex mutex;inline std::unordered_map<IUnknown*,Binding> bindings;
inline bool bind(IDXGISwapChain* chain,IUnknown* device){
    if(!chain||!device)return false;ComPtr<ID3D12CommandQueue> queue;ComPtr<IUnknown> identity;
    if(FAILED(device->QueryInterface(IID_PPV_ARGS(&queue)))||queue->GetDesc().Type!=D3D12_COMMAND_LIST_TYPE_DIRECT||FAILED(chain->QueryInterface(IID_PPV_ARGS(&identity))))return false;
    std::lock_guard<std::mutex> lock(mutex);bindings[identity.Get()]={identity,queue};return true;
}
inline ComPtr<ID3D12CommandQueue> lookup(IDXGISwapChain* chain){
    ComPtr<IUnknown> identity;if(!chain||FAILED(chain->QueryInterface(IID_PPV_ARGS(&identity))))return {};
    std::lock_guard<std::mutex> lock(mutex);auto found=bindings.find(identity.Get());return found==bindings.end()?ComPtr<ID3D12CommandQueue>{}:found->second.queue;
}
}
