#pragma once
#include "pill_inventory_ui.hpp"
#include "save_slot_menu_adapter.hpp"
#include "save_slot_art.hpp"
#include "save_slot_layout.hpp"
namespace saveSlotUI {
using Microsoft::WRL::ComPtr;
inline ComPtr<ID3D12RootSignature> root;inline ComPtr<ID3D12PipelineState> pipeline;
inline ComPtr<ID3D12DescriptorHeap> srv,rtv;inline ComPtr<ID3D12Resource> atlas,upload,mapTexture;
inline ID3D12Device* owner{};inline DXGI_FORMAT targetFormat{};inline bool mapValid{};
inline bool initialize(ID3D12Device* device,ID3D12GraphicsCommandList* list,DXGI_FORMAT format){
    if(owner==device&&targetFormat==format&&pipeline)return true;
    root.Reset();pipeline.Reset();srv.Reset();rtv.Reset();atlas.Reset();upload.Reset();mapTexture.Reset();mapValid=false;owner=nullptr;
    const char shader[]=R"(Texture2D art:register(t0);Texture2D liveMap:register(t1);SamplerState linearClamp:register(s0);
cbuffer State:register(b0){uint4 words[5];}
uint value(uint i){return words[i/4][i%4];}
struct Vertex{float4 p:SV_POSITION;float2 uv:TEXCOORD0;};
Vertex vs(uint id:SV_VertexID){Vertex o;o.uv=float2((id<<1)&2,id&2);o.p=float4(o.uv*float2(2,-2)+float2(-1,1),0,1);return o;}
bool inside(float2 p,float2 a,float2 size){return all(p>=a)&&all(p<a+size);}
float4 label(float4 c,float2 p,float2 origin,uint id){float2 q=p-origin;if(inside(q,0,float2(370,28))){float4 a=art.SampleLevel(linearClamp,(q+float2(.5,id*32+.5))/float2(2048,1024),0);c.rgb=lerp(c.rgb,a.rgb,a.a);}return c;}
float4 ps(Vertex v):SV_TARGET{float2 p=v.uv*float2(640,360);float4 c=liveMap.SampleLevel(linearClamp,v.uv,0);c.a=1;
 // Compact right-side menu over the native animated background.
 p=(p-float2(asfloat(value(5)),asfloat(value(6))))/asfloat(value(7))+float2(35,148);
 if(!inside(p,float2(35,148),float2(465,209)))return c;
 if(value(0)==1){c=label(c,p,float2(55,155),value(2)!=0?1:0);
 for(uint i=0;i<3;i++){float y=190+i*40;if(inside(p,float2(48,y-3),float2(420,34))&&i==value(1))c.rgb=lerp(c.rgb,float3(.35,.025,.018),.45);c=label(c,p,float2(60,y),2+i);c=label(c,p,float2(215,y),(value(4)&(1<<i))!=0?5:6);}
 c=label(c,p,float2(55,321),11);
 }else if(value(0)==2){c=label(c,p,float2(55,170),7);c=label(c,p,float2(55,207),12);
 for(uint i=0;i<2;i++){float y=250+i*36;if(inside(p,float2(48,y-3),float2(420,32))&&i==value(3))c.rgb=lerp(c.rgb,float3(.35,.025,.018),.45);c=label(c,p,float2(60,y),8+i);}
 }else{c=label(c,p,float2(55,190),10);c=label(c,p,float2(55,270),11);}return c;}
)";
    ComPtr<ID3DBlob> vs,ps,error,serialized;
    if(FAILED(D3DCompile(shader,sizeof shader,nullptr,nullptr,nullptr,"vs","vs_5_0",0,0,&vs,&error)))return false;
    if(FAILED(D3DCompile(shader,sizeof shader,nullptr,nullptr,nullptr,"ps","ps_5_0",0,0,&ps,&error)))return false;
    D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,2,0,0,0};
    D3D12_ROOT_PARAMETER params[2]{};params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[0].DescriptorTable={1,&range};params[0].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    params[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params[1].Constants={0,0,20};params[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;sampler.MaxLOD=D3D12_FLOAT32_MAX;sampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC rd{2,params,1,&sampler,D3D12_ROOT_SIGNATURE_FLAG_NONE};
    if(FAILED(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&serialized,&error))||FAILED(device->CreateRootSignature(0,serialized->GetBufferPointer(),serialized->GetBufferSize(),IID_PPV_ARGS(&root))))return false;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();pd.VS={vs->GetBufferPointer(),vs->GetBufferSize()};pd.PS={ps->GetBufferPointer(),ps->GetBufferSize()};pd.BlendState.RenderTarget[0].RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;pd.SampleMask=UINT_MAX;pd.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;pd.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;pd.RasterizerState.DepthClipEnable=TRUE;pd.DepthStencilState.DepthEnable=FALSE;pd.DepthStencilState.StencilEnable=FALSE;pd.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;pd.NumRenderTargets=1;pd.RTVFormats[0]=format;pd.SampleDesc.Count=1;
    if(FAILED(device->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&pipeline))))return false;
    D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=2;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if(FAILED(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&srv))))return false;
    hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_NONE;if(FAILED(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtv))))return false;
    D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC td{};td.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;td.Width=2048;td.Height=1024;td.DepthOrArraySize=td.MipLevels=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;
    if(FAILED(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&td,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&atlas))))return false;
    auto bd=td;bd.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;bd.Width=2048*1024*4;bd.Height=1;bd.Format=DXGI_FORMAT_UNKNOWN;bd.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;hp.Type=D3D12_HEAP_TYPE_UPLOAD;
    if(FAILED(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&upload))))return false;
    std::uint32_t* mapped{};D3D12_RANGE empty{};if(FAILED(upload->Map(0,&empty,reinterpret_cast<void**>(&mapped))))return false;
    size_t n=0;for(auto run:saveSlotArt::runs){if(n+run.count>2048*1024){upload->Unmap(0,nullptr);return false;}std::fill_n(mapped+n,run.count,run.rgba);n+=run.count;}upload->Unmap(0,nullptr);if(n!=2048*1024)return false;
    D3D12_TEXTURE_COPY_LOCATION dest{},src{};dest.pResource=atlas.Get();dest.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;src.pResource=upload.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint.Footprint={DXGI_FORMAT_R8G8B8A8_UNORM,2048,1024,1,8192};list->CopyTextureRegion(&dest,0,0,0,&src,nullptr);
    D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition={atlas.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};list->ResourceBarrier(1,&barrier);
    D3D12_SHADER_RESOURCE_VIEW_DESC sd{};sd.Format=td.Format;sd.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;sd.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;sd.Texture2D.MipLevels=1;device->CreateShaderResourceView(atlas.Get(),&sd,srv->GetCPUDescriptorHandleForHeapStart());
    auto emptyMap=srv->GetCPUDescriptorHandleForHeapStart();emptyMap.ptr+=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);device->CreateShaderResourceView(nullptr,&sd,emptyMap);
    owner=device;targetFormat=format;return true;
}
inline void copyMapFrame(ID3D12Device* device,ID3D12GraphicsCommandList* list,ID3D12Resource* source){
    if(!initialize(device,list,source->GetDesc().Format))return;
    auto desc=source->GetDesc();if(desc.SampleDesc.Count!=1)return;
    if(!mapTexture||mapTexture->GetDesc().Width!=desc.Width||mapTexture->GetDesc().Height!=desc.Height){
        mapTexture.Reset();mapValid=false;desc.Flags=D3D12_RESOURCE_FLAG_NONE;
        D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
        if(FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,nullptr,IID_PPV_ARGS(&mapTexture))))return;
    }
    D3D12_RESOURCE_BARRIER barriers[2]{};
    for(auto& b:barriers)b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition={source,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE};
    barriers[1].Transition={mapTexture.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_DEST};
    list->ResourceBarrier(2,barriers);list->CopyResource(mapTexture.Get(),source);
    for(auto& b:barriers)std::swap(b.Transition.StateBefore,b.Transition.StateAfter);list->ResourceBarrier(2,barriers);
    D3D12_SHADER_RESOURCE_VIEW_DESC sd{};sd.Format=desc.Format;sd.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;sd.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;sd.Texture2D.MipLevels=1;
    auto handle=srv->GetCPUDescriptorHandleForHeapStart();handle.ptr+=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);device->CreateShaderResourceView(mapTexture.Get(),&sd,handle);mapValid=true;
}
inline bool draw(ID3D12Device* device,ID3D12GraphicsCommandList* list,ID3D12Resource* target,unsigned w,unsigned h){
 if(!saveSlotMenu::visible())return false;copyMapFrame(device,list,target);if(!mapValid)return false;
 auto v=saveSlotMenu::view();unsigned words[20]{};words[0]=unsigned(v.page);words[1]=v.selection;words[2]=v.fresh;words[3]=v.overwrite;for(unsigned i=0;i<3;i++)if(v.occupied[i])words[4]|=1u<<i;
 auto layout=saveSlotLayout::get(GetTickCount64());std::memcpy(words+5,&layout.x,4);std::memcpy(words+6,&layout.y,4);std::memcpy(words+7,&layout.scale,4);
 auto handle=rtv->GetCPUDescriptorHandleForHeapStart();device->CreateRenderTargetView(target,nullptr,handle);list->OMSetRenderTargets(1,&handle,FALSE,nullptr);
 D3D12_VIEWPORT viewport{0,0,float(w),float(h),0,1};D3D12_RECT scissor{0,0,LONG(w),LONG(h)};list->RSSetViewports(1,&viewport);list->RSSetScissorRects(1,&scissor);
 list->SetPipelineState(pipeline.Get());list->SetGraphicsRootSignature(root.Get());ID3D12DescriptorHeap* heaps[]={srv.Get()};list->SetDescriptorHeaps(1,heaps);list->SetGraphicsRootDescriptorTable(0,srv->GetGPUDescriptorHandleForHeapStart());list->SetGraphicsRoot32BitConstants(1,20,words,0);list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);list->DrawInstanced(3,1,0,0);return true;
}
}
