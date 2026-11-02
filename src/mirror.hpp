#pragma once
#include <d3dcompiler.h>
namespace desktopMirror {
using Microsoft::WRL::ComPtr;
static ComPtr<ID3D12RootSignature> root;
static ComPtr<ID3D12PipelineState> pipeline;
static ComPtr<ID3D12DescriptorHeap> resources,targets;
static DXGI_FORMAT format{};
static unsigned rendered{};
static std::atomic<bool> captureRequested{};
static void initialize(DXGI_FORMAT targetFormat) {
    auto device=stereo::gameDevice.Get();
    const char shader[]=R"(
Texture2D scene:register(t0);Texture2D overlay:register(t1);Texture2D rightScene:register(t2);SamplerState linearClamp:register(s0);
cbuffer Parameters:register(b0){float4 fov;float4 state;float4 fade;}
struct Vertex{float4 position:SV_POSITION;float2 uv:TEXCOORD0;};
Vertex vs(uint id:SV_VertexID){Vertex o;o.uv=float2((id<<1)&2,id&2);o.position=float4(o.uv*float2(2,-2)+float2(-1,1),0,1);return o;}
float4 ps(Vertex v):SV_TARGET{
 bool rightEye=fade.y>0&&v.uv.x>=.5;
 if(fade.y>0)v.uv.x=frac(v.uv.x*2);
 float4 color=state.w>0?(rightEye?rightScene.Sample(linearClamp,v.uv):scene.Sample(linearClamp,v.uv)):float4(0,0,0,1);
 if(state.x>0){
  float2 ray=float2(lerp(fov.x,fov.y,v.uv.x),lerp(fov.w,fov.z,v.uv.y));
  float2 uv=float2((ray.x*1.4+(rightEye?-state.z:state.z))/2+.5,.5-ray.y*1.4/state.y);
  if(all(uv>=0)&&all(uv<=1)){float4 ui=overlay.Sample(linearClamp,uv);color.rgb=state.x>1?ui.rgb:ui.rgb+color.rgb*(1-ui.a);}
 }
 return float4(color.rgb*(1-fade.x),1);
})";
    ComPtr<ID3DBlob> vs,ps,error,serialized;
    stereo::hr(D3DCompile(shader,sizeof(shader),nullptr,nullptr,nullptr,"vs","vs_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&vs,&error));
    stereo::hr(D3DCompile(shader,sizeof(shader),nullptr,nullptr,nullptr,"ps","ps_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&ps,&error));
    D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,3,0,0,0};
    D3D12_ROOT_PARAMETER params[2]{};params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[0].DescriptorTable={1,&range};params[0].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    params[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params[1].Constants={0,0,12};params[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;sampler.MaxLOD=D3D12_FLOAT32_MAX;sampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC rd{2,params,1,&sampler,D3D12_ROOT_SIGNATURE_FLAG_NONE};
    stereo::hr(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&serialized,&error));
    stereo::hr(device->CreateRootSignature(0,serialized->GetBufferPointer(),serialized->GetBufferSize(),IID_PPV_ARGS(&root)));
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();pd.VS={vs->GetBufferPointer(),vs->GetBufferSize()};pd.PS={ps->GetBufferPointer(),ps->GetBufferSize()};
    pd.BlendState.RenderTarget[0].RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;pd.SampleMask=UINT_MAX;
    pd.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;pd.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;pd.RasterizerState.DepthClipEnable=TRUE;
    pd.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_ALWAYS;pd.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pd.NumRenderTargets=1;pd.RTVFormats[0]=targetFormat;pd.SampleDesc.Count=1;
    stereo::hr(device->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&pipeline)));
    D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=3;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    stereo::hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&resources)));hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=1;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    stereo::hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&targets)));
    D3D12_SHADER_RESOURCE_VIEW_DESC sd{};sd.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
    sd.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;sd.Texture2D.MipLevels=1;
    auto handle=resources->GetCPUDescriptorHandleForHeapStart();device->CreateShaderResourceView(stereo::resolved[0].Get(),&sd,handle);
    handle.ptr+=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    device->CreateShaderResourceView(stereo::uiCache.Get(),&sd,handle);
    handle.ptr+=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    device->CreateShaderResourceView(stereo::resolved[1].Get(),&sd,handle);format=targetFormat;
}
static void render(IDXGISwapChain* chain) {
    std::unique_lock<std::shared_mutex> lock(stereo::gate);
    if(!stereo::haveWorldFrame||stereo::failed)return;
    auto ownerQueue=swapchainQueue::lookup(chain);if(!ownerQueue)return;
    ComPtr<IDXGISwapChain3> modern;if(FAILED(chain->QueryInterface(IID_PPV_ARGS(&modern))))return;
    ComPtr<ID3D12Resource> backbuffer;stereo::hr(chain->GetBuffer(modern->GetCurrentBackBufferIndex(),IID_PPV_ARGS(&backbuffer)));
    auto desc=backbuffer->GetDesc();if(!pipeline||desc.Format!=format)initialize(desc.Format);
    auto device=stereo::gameDevice.Get();auto list=stereo::commands.Get();
    auto target=targets->GetCPUDescriptorHandleForHeapStart();device->CreateRenderTargetView(backbuffer.Get(),nullptr,target);
    stereo::beginCommands();
    D3D12_RESOURCE_BARRIER barriers[]={stereo::transition(backbuffer.Get(),D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET),
        stereo::transition(stereo::resolved[0].Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
        stereo::transition(stereo::uiCache.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
        stereo::transition(stereo::resolved[1].Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)};
    list->ResourceBarrier(4,barriers);const float black[4]={0,0,0,1};list->ClearRenderTargetView(target,black,0,nullptr);
    float height=static_cast<float>(desc.Height),width=height*stereo::xr->eyes[0].width/stereo::xr->eyes[0].height;
    float panels=stereo::xr->desktopDiagnostic?2.f:1.f;width*=panels;
    if(width>desc.Width){width=static_cast<float>(desc.Width);height=width*stereo::xr->eyes[0].height/stereo::xr->eyes[0].width/panels;}
    D3D12_VIEWPORT vp{(static_cast<float>(desc.Width)-width)*.5f,(desc.Height-height)*.5f,width,height,0,1};
    D3D12_RECT scissor{0,0,static_cast<LONG>(desc.Width),static_cast<LONG>(desc.Height)};
    auto f=stereo::lastWorldViews[0].fov;
    auto left=stereo::lastWorldViews[0].pose.position,right=stereo::lastWorldViews[1].pose.position;
    float halfIPD=.5f*sqrtf((left.x-right.x)*(left.x-right.x)+(left.y-right.y)*(left.y-right.y)+(left.z-right.z)*(left.z-right.z));
    float parameters[12]={tanf(f.angleLeft),tanf(f.angleRight),tanf(f.angleDown),tanf(f.angleUp),stereo::submittedUI?(stereo::cachedUIOpaque?2.f:1.f):0.f,
        2.f*stereo::desktopHeight/stereo::desktopWidth,-halfIPD,stereo::submittedWorld?1.f:0.f,stereo::submittedWorld?stereo::cachedFadeAlpha:0.f,stereo::xr->desktopDiagnostic?1.f:0.f,0,0};
    ID3D12DescriptorHeap* heaps[]={resources.Get()};list->SetDescriptorHeaps(1,heaps);list->SetGraphicsRootSignature(root.Get());
    list->SetPipelineState(pipeline.Get());list->SetGraphicsRootDescriptorTable(0,resources->GetGPUDescriptorHandleForHeapStart());
    list->SetGraphicsRoot32BitConstants(1,12,parameters,0);list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->RSSetViewports(1,&vp);list->RSSetScissorRects(1,&scissor);list->OMSetRenderTargets(1,&target,FALSE,nullptr);list->DrawInstanced(3,1,0,0);
    // DXGI's desktop swapchain belongs to the game queue. Submit mirror writes
    // on that queue so Present and the engine's frame fences retain ordering.
    for(auto& barrier:barriers)std::swap(barrier.Transition.StateBefore,barrier.Transition.StateAfter);list->ResourceBarrier(4,barriers);stereo::submitCommands(ownerQueue.Get());
    const bool capture=++rendered==120||captureRequested.exchange(false);
    if(capture&&asyncPpmCapture::busy.load(std::memory_order_acquire)){captureRequested=true;return;}
    if(capture) {
        std::vector<asyncPpmCapture::Image> images;
        stereo::beginCommands();auto b=stereo::transition(backbuffer.Get(),D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_COPY_SOURCE);list->ResourceBarrier(1,&b);stereo::submitCommands(ownerQueue.Get());
        stereo::savePreview(backbuffer.Get(),L"desktop-mirror.ppm",images,ownerQueue.Get());
        stereo::beginCommands();std::swap(b.Transition.StateBefore,b.Transition.StateAfter);list->ResourceBarrier(1,&b);stereo::submitCommands(ownerQueue.Get());
        if(!asyncPpmCapture::submit(images))captureRequested=true;
    }
}
}
