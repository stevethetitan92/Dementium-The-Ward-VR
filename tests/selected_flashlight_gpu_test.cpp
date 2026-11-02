#include <Windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <wincodec.h>
#include <cstdio>
#include <stdexcept>
#include "../src/selected_flashlight.hpp"
void check(bool ok){if(!ok)throw std::runtime_error("flashlight asset/GPU check failed");}
int main(int argc,char**argv){try{
 check(argc==2);psxAssets::Pack pack;check(psxAssets::load(argv[1],pack));
 const auto original=pack.vertices;const auto sections=pack.sections.size();
 check(selectedFlashlight::load(argv[1],pack));check(selectedFlashlight::section==sections);
 check(pack.sections[sections].count+pack.sections.back().count==3522);
 check(selectedFlashlight::glassSection==sections+1);
 check(!std::memcmp(original.data(),pack.vertices.data(),original.size()*sizeof(psxAssets::Vertex)));
 using Microsoft::WRL::ComPtr;
 ComPtr<ID3D12Debug> debug;check(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))));debug->EnableDebugLayer();
 ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter> adapter;ComPtr<ID3D12Device> device;
 check(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))));check(SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))));
 check(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))));
 ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;
 check(SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator))));
 check(SUCCEEDED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list))));
 D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=1;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
 ComPtr<ID3D12DescriptorHeap> originalHeap;check(SUCCEEDED(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&originalHeap))));
 D3D12_SHADER_RESOURCE_VIEW_DESC nullView{};nullView.Format=DXGI_FORMAT_R8G8B8A8_UNORM;nullView.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;nullView.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;nullView.Texture2D.MipLevels=1;
 D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;D3D12_RESOURCE_DESC atlasDesc{};atlasDesc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;atlasDesc.Width=atlasDesc.Height=2;atlasDesc.DepthOrArraySize=atlasDesc.MipLevels=atlasDesc.SampleDesc.Count=1;atlasDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
 ComPtr<ID3D12Resource> atlas;check(SUCCEEDED(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&atlasDesc,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,nullptr,IID_PPV_ARGS(&atlas))));
 device->CreateShaderResourceView(atlas.Get(),&nullView,originalHeap->GetCPUDescriptorHandleForHeapStart());
 // Production's shader-visible heap is not a legal descriptor copy source.
 check(selectedFlashlight::initialize(device.Get(),list.Get(),atlas.Get()));
 check(selectedFlashlight::heap->GetDesc().NumDescriptors==4);
 for(const auto& texture:selectedFlashlight::textures){auto d=texture->GetDesc();check(d.Width==4096&&d.Height==4096&&d.Format==DXGI_FORMAT_R8G8B8A8_UNORM);}
 check(SUCCEEDED(list->Close()));D3D12_COMMAND_QUEUE_DESC qd{};ComPtr<ID3D12CommandQueue> queue;
 check(SUCCEEDED(device->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue))));ID3D12CommandList* commands[]={list.Get()};queue->ExecuteCommandLists(1,commands);
 ComPtr<ID3D12Fence> fence;check(SUCCEEDED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence))));check(SUCCEEDED(queue->Signal(fence.Get(),1)));
 HANDLE event=CreateEvent(nullptr,FALSE,FALSE,nullptr);check(event!=nullptr);check(SUCCEEDED(fence->SetEventOnCompletion(1,event)));const auto wait=WaitForSingleObject(event,30000);CloseHandle(event);check(wait==WAIT_OBJECT_0);check(SUCCEEDED(device->GetDeviceRemovedReason()));
 ComPtr<ID3D12InfoQueue> info;check(SUCCEEDED(device.As(&info)));for(UINT64 i=0;i<info->GetNumStoredMessages();++i){SIZE_T bytes{};info->GetMessage(i,nullptr,&bytes);std::vector<unsigned char> storage(bytes);auto* message=reinterpret_cast<D3D12_MESSAGE*>(storage.data());check(SUCCEEDED(info->GetMessage(i,message,&bytes)));if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR){puts(message->pDescription);check(false);}}
 puts("PASS: no D3D12 errors with shader-visible original atlas heap; source flashlight appended without changing weapon vertices; all three 4096 maps uploaded on WARP with isolated descriptor heap");return 0;
 }catch(const std::exception&e){puts(e.what());return 1;}}
