#include "../src/swapchain_queue.hpp"
#include "../src/device_ownership.hpp"
#include <d3d12sdklayers.h>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
static void check(HRESULT h){if(FAILED(h))throw std::runtime_error("Queue regression HRESULT "+std::to_string(static_cast<unsigned>(h)));}
int main(){try{
 ComPtr<ID3D12Debug> debug;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
 ComPtr<IDXGIFactory4> factory;check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));ComPtr<IDXGIAdapter> warp;check(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));
 ComPtr<ID3D12Device> device;check(D3D12CreateDevice(warp.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));
 D3D12_COMMAND_QUEUE_DESC q{};ComPtr<ID3D12CommandQueue> decoy,owner,xr;check(device->CreateCommandQueue(&q,IID_PPV_ARGS(&decoy)));check(device->CreateCommandQueue(&q,IID_PPV_ARGS(&owner)));check(device->CreateCommandQueue(&q,IID_PPV_ARGS(&xr)));
 DXGI_SWAP_CHAIN_DESC1 desc{};desc.Width=desc.Height=64;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;desc.AlphaMode=DXGI_ALPHA_MODE_PREMULTIPLIED;
 ComPtr<IDXGISwapChain1> chain;check(factory->CreateSwapChainForComposition(owner.Get(),&desc,nullptr,&chain));
 assert(!swapchainQueue::lookup(chain.Get()));assert(swapchainQueue::bind(chain.Get(),owner.Get()));auto selected=swapchainQueue::lookup(chain.Get());assert(selected.Get()==owner.Get()&&selected.Get()!=decoy.Get()&&selected.Get()!=xr.Get());
 ComPtr<ID3D12CommandAllocator> allocator;check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));ComPtr<ID3D12GraphicsCommandList> list;check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)));
 assert(deviceOwnership::belongs(list.Get(),device.Get())&&!deviceOwnership::belongs(list.Get(),nullptr));
 ComPtr<IDXGIAdapter1> otherAdapter;ComPtr<ID3D12Device> otherDevice;for(UINT i=0;SUCCEEDED(factory->EnumAdapters1(i,&otherAdapter));i++){DXGI_ADAPTER_DESC1 ad{};otherAdapter->GetDesc1(&ad);if(!(ad.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)&&SUCCEEDED(D3D12CreateDevice(otherAdapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&otherDevice))))break;otherAdapter.Reset();}
 if(otherDevice)assert(!deviceOwnership::belongs(list.Get(),otherDevice.Get()));
 ComPtr<ID3D12Resource> buffer;check(chain->GetBuffer(0,IID_PPV_ARGS(&buffer)));D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.NumDescriptors=1;hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;ComPtr<ID3D12DescriptorHeap> heap;check(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)));auto rtv=heap->GetCPUDescriptorHandleForHeapStart();device->CreateRenderTargetView(buffer.Get(),nullptr,rtv);
 D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition={buffer.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET};list->ResourceBarrier(1,&barrier);const float color[]={.1f,.2f,.3f,1};list->ClearRenderTargetView(rtv,color,0,nullptr);std::swap(barrier.Transition.StateBefore,barrier.Transition.StateAfter);list->ResourceBarrier(1,&barrier);check(list->Close());ID3D12CommandList* commands[]={list.Get()};selected->ExecuteCommandLists(1,commands);
 ComPtr<ID3D12Fence> fence;check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);check(selected->Signal(fence.Get(),1));check(fence->SetEventOnCompletion(1,event));assert(WaitForSingleObject(event,10000)==WAIT_OBJECT_0);CloseHandle(event);check(device->GetDeviceRemovedReason());
 ComPtr<ID3D12InfoQueue> info;if(SUCCEEDED(device.As(&info)))for(UINT64 i=0;i<info->GetNumStoredMessages();i++){SIZE_T size{};info->GetMessage(i,nullptr,&size);std::vector<char> bytes(size);auto m=reinterpret_cast<D3D12_MESSAGE*>(bytes.data());info->GetMessage(i,m,&size);if(m->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(m->pDescription);}
 std::cout<<"PASS: real DXGI swapchain uses its creating queue despite earlier decoy/XR queues; unbound chain fails closed; no D3D12 errors or game launch\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

