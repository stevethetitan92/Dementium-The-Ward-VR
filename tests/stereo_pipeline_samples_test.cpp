#include <initializer_list>
#include "../src/stereo_pipeline_samples.hpp"
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cassert>
#include <d3d12sdklayers.h>
#include <vector>
#include <cstdio>
using Microsoft::WRL::ComPtr;

void renderCheck(ID3D12Device* dev,ID3D12PipelineState* pso,ID3D12RootSignature* root){
 ComPtr<ID3D12CommandQueue> queue;D3D12_COMMAND_QUEUE_DESC qd{};assert(SUCCEEDED(dev->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue))));
 ComPtr<ID3D12CommandAllocator> alloc;assert(SUCCEEDED(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&alloc))));
 ComPtr<ID3D12GraphicsCommandList> cmd;assert(SUCCEEDED(dev->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,alloc.Get(),pso,IID_PPV_ARGS(&cmd))));
 ComPtr<ID3D12DescriptorHeap> rtv,dsv;D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.NumDescriptors=2;hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;assert(SUCCEEDED(dev->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtv))));hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_DSV;assert(SUCCEEDED(dev->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&dsv))));
 ComPtr<ID3D12Resource> colors[2],depths[2],resolved[2],readback[2];
 auto barrier=[&](ID3D12Resource* r,D3D12_RESOURCE_STATES a,D3D12_RESOURCE_STATES b){D3D12_RESOURCE_BARRIER v{};v.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;v.Transition={r,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,a,b};cmd->ResourceBarrier(1,&v);};
 for(int eye=0;eye<2;++eye){
 D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=32;d.Height=32;d.DepthOrArraySize=1;d.MipLevels=1;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.SampleDesc.Count=2;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
 assert(SUCCEEDED(dev->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_RENDER_TARGET,nullptr,IID_PPV_ARGS(&colors[eye]))));
 d.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;assert(SUCCEEDED(dev->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_DEPTH_WRITE,nullptr,IID_PPV_ARGS(&depths[eye]))));
 d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.Flags=D3D12_RESOURCE_FLAG_NONE;d.SampleDesc.Count=1;assert(SUCCEEDED(dev->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_RESOLVE_DEST,nullptr,IID_PPV_ARGS(&resolved[eye]))));
 auto rh=rtv->GetCPUDescriptorHandleForHeapStart();rh.ptr+=eye*dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);auto dh=dsv->GetCPUDescriptorHandleForHeapStart();dh.ptr+=eye*dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
 dev->CreateRenderTargetView(colors[eye].Get(),nullptr,rh);dev->CreateDepthStencilView(depths[eye].Get(),nullptr,dh);
 float clear[4]={0,0,0,1};cmd->ClearRenderTargetView(rh,clear,0,nullptr);cmd->ClearDepthStencilView(dh,D3D12_CLEAR_FLAG_DEPTH,1,0,0,nullptr);
 D3D12_VIEWPORT vp{0,0,32,32,0,1};D3D12_RECT rect{0,0,32,32};cmd->RSSetViewports(1,&vp);cmd->RSSetScissorRects(1,&rect);cmd->OMSetRenderTargets(1,&rh,FALSE,&dh);cmd->SetGraphicsRootSignature(root);cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);cmd->DrawInstanced(3,1,0,0);
 barrier(colors[eye].Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_RESOLVE_SOURCE);cmd->ResolveSubresource(resolved[eye].Get(),0,colors[eye].Get(),0,DXGI_FORMAT_R8G8B8A8_UNORM);barrier(resolved[eye].Get(),D3D12_RESOURCE_STATE_RESOLVE_DEST,D3D12_RESOURCE_STATE_COPY_SOURCE);
 D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT64 bytes{};dev->GetCopyableFootprints(&d,0,1,0,&footprint,nullptr,nullptr,&bytes);
 heap.Type=D3D12_HEAP_TYPE_READBACK;D3D12_RESOURCE_DESC bd{};bd.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;bd.Width=bytes;bd.Height=1;bd.DepthOrArraySize=1;bd.MipLevels=1;bd.SampleDesc.Count=1;bd.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;assert(SUCCEEDED(dev->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback[eye]))));
 D3D12_TEXTURE_COPY_LOCATION dst{},src{};dst.pResource=readback[eye].Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=footprint;src.pResource=resolved[eye].Get();src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;cmd->CopyTextureRegion(&dst,0,0,0,&src,nullptr);
 }
 assert(SUCCEEDED(cmd->Close()));ID3D12CommandList* lists[]={cmd.Get()};queue->ExecuteCommandLists(1,lists);ComPtr<ID3D12Fence> fence;assert(SUCCEEDED(dev->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence))));assert(SUCCEEDED(queue->Signal(fence.Get(),1)));HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);assert(event);assert(SUCCEEDED(fence->SetEventOnCompletion(1,event)));assert(WaitForSingleObject(event,10000)==WAIT_OBJECT_0);CloseHandle(event);
 for(auto& r:readback){unsigned char* data{};assert(SUCCEEDED(r->Map(0,nullptr,reinterpret_cast<void**>(&data))));assert(data[16*256+16*4]==255&&data[16*256+16*4+1]==0);r->Unmap(0,nullptr);}
}

int main(){
 ComPtr<ID3D12Debug> debug;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
 ComPtr<IDXGIFactory4> factory;assert(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))));
 ComPtr<IDXGIAdapter> adapter;assert(SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))));
 ComPtr<ID3D12Device> device;assert(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))));
 D3D12_ROOT_SIGNATURE_DESC rd{};ComPtr<ID3DBlob> blob,error;assert(SUCCEEDED(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&error)));
 ComPtr<ID3D12RootSignature> root;assert(SUCCEEDED(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root))));
 const char shader[]= "float4 vs(uint id:SV_VertexID):SV_Position {return float4(id==1?3:-1,id==2?3:-1,0.5,1);} float4 ps():SV_Target {return float4(1,0,0,1);}";
 ComPtr<ID3DBlob> vs,ps;assert(SUCCEEDED(D3DCompile(shader,sizeof(shader),nullptr,nullptr,nullptr,"vs","vs_5_0",0,0,&vs,&error)));assert(SUCCEEDED(D3DCompile(shader,sizeof(shader),nullptr,nullptr,nullptr,"ps","ps_5_0",0,0,&ps,&error)));
 D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};d.pRootSignature=root.Get();d.VS={vs->GetBufferPointer(),vs->GetBufferSize()};d.PS={ps->GetBufferPointer(),ps->GetBufferSize()};d.NumRenderTargets=1;d.RTVFormats[0]=DXGI_FORMAT_R8G8B8A8_UNORM;d.DSVFormat=DXGI_FORMAT_D24_UNORM_S8_UINT;d.SampleMask=UINT_MAX;d.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
 d.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;d.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;d.RasterizerState.DepthClipEnable=TRUE;
 d.BlendState.RenderTarget[0].RenderTargetWriteMask=15;d.DepthStencilState.DepthEnable=TRUE;d.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ALL;d.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_LESS;
 for(unsigned count:{1u,2u,4u,8u}){
 d.SampleDesc={count,0};assert(stereoPipelineSamples::eligible(1200,d));assert(stereoPipelineSamples::eligible(128,d));assert(!stereoPipelineSamples::eligible(32,d));
 auto adapted=stereoPipelineSamples::adapt(d,2);assert(d.SampleDesc.Count==count);assert(adapted.SampleDesc.Count==2&&adapted.SampleDesc.Quality==0);assert(adapted.PS.pShaderBytecode==d.PS.pShaderBytecode);assert(adapted.DepthStencilState.DepthFunc==d.DepthStencilState.DepthFunc);
 ComPtr<ID3D12PipelineState> pso;assert(SUCCEEDED(device->CreateGraphicsPipelineState(&adapted,IID_PPV_ARGS(&pso))));renderCheck(device.Get(),pso.Get(),root.Get());
 }
 d.CachedPSO={shader,sizeof shader};assert(stereoPipelineSamples::adapt(d,2).CachedPSO.CachedBlobSizeInBytes==0);
 d.DSVFormat=DXGI_FORMAT_UNKNOWN;assert(!stereoPipelineSamples::eligible(1200,d));
 ComPtr<ID3D12InfoQueue> messages;if(SUCCEEDED(device.As(&messages))){for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T size{};messages->GetMessage(i,nullptr,&size);std::vector<unsigned char> data(size);auto m=reinterpret_cast<D3D12_MESSAGE*>(data.data());messages->GetMessage(i,m,&size);if(m->Severity<=D3D12_MESSAGE_SEVERITY_ERROR){puts(m->pDescription);assert(false);}}}
 puts("PASS: sample 1/2/4/8 descriptor adaptation, native preservation, shader preservation, cache invalidation, rejected UI; four real D3D12 WARP PSOs; both-eye draw/resolve/readback red pixels verified for each; no D3D12 errors.");
}
