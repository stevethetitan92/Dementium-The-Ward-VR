#pragma once
#include "optic_geometry.hpp"
#include "optic_lens_mesh.hpp"
#include "tracking_math.hpp"
#include "selected_ballistics.hpp"
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <mutex>
#include <atomic>
#include <string>
#include <stdexcept>
namespace opticScope {
using namespace DirectX;using Microsoft::WRL::ComPtr;
inline bool enabled=false;
inline std::string initializationError;
inline std::atomic<uint64_t> diagnosticUntil{};
inline constexpr unsigned resolution=512;
// Source ocular inner glass is approximately 15.5mm radius (outer rim 19.4mm).
inline constexpr float radius=.0155f;
// Candidate ocular center measured in the retained hunting body/aim frame.
inline constexpr XrVector3f ocular={-.04873196f,.08980059f,-.0323f};
struct Sample{XrPosef gun{};uint64_t time{};bool valid{};};
inline Sample sample;inline std::mutex mutex;
inline XrPosef lens{};inline XrView camera{XR_TYPE_VIEW};inline bool active{};
inline constexpr float zoom=1.5f;
inline ComPtr<ID3D12Resource> color,depth,resolved;
inline ComPtr<ID3D12DescriptorHeap> targets,depths,srv;
inline ComPtr<ID3D12Resource> lensVertices;
inline D3D12_VERTEX_BUFFER_VIEW lensView{};
inline ComPtr<ID3D12RootSignature> root;inline ComPtr<ID3D12PipelineState> pipeline;
inline D3D12_CPU_DESCRIPTOR_HANDLE rtv{},dsv{};
inline bool ready{};
inline void publish(const XrPosef& gun,uint64_t now,bool valid){std::lock_guard<std::mutex> lock(mutex);sample={gun,now,valid};}
inline XMMATRIX pose(const XrPosef& p){return motionTracking::rotation(p.orientation)*XMMatrixTranslation(p.position.x,p.position.y,p.position.z);}
inline void alignDisplay(const XrPosef& gun){
 if(!active||!motionTracking::validPose(gun))return;
 lens=gun;XMFLOAT3 p;XMStoreFloat3(&p,XMVector3TransformCoord(XMVectorSet(ocular.x,ocular.y,ocular.z,1),pose(gun)));lens.position={p.x,p.y,p.z};
}
inline opticGeometry::Aperture eyeBox(const XrPosef& eye){
 XMFLOAT3 p;XMStoreFloat3(&p,XMVector3TransformCoord(XMVectorSet(eye.position.x,eye.position.y,eye.position.z,1),XMMatrixInverse(nullptr,pose(lens))));
 return opticGeometry::aperture({p.x,p.y,p.z},radius,zoom);
}
inline void update(uint64_t now,const XrView& left,const XrView& right){
 Sample s;{std::lock_guard<std::mutex> lock(mutex);s=sample;}
 active=false;if(!enabled||!ready||!s.valid||now<s.time||now-s.time>100||!motionTracking::validPose(s.gun))return;
 lens=s.gun;XMFLOAT3 p;XMStoreFloat3(&p,XMVector3TransformCoord(XMVectorSet(ocular.x,ocular.y,ocular.z,1),pose(s.gun)));lens.position={p.x,p.y,p.z};
 auto l=eyeBox(left.pose),r=eyeBox(right.pose);active=l.visible||r.visible;if(!active)return;
 // The camera is rigidly attached to the optic, independent of the head.
 camera.pose=lens;XMStoreFloat3(&p,XMVector3TransformCoord(XMVectorSet(0,0,-.40f,1),pose(lens)));camera.pose.position={p.x,p.y,p.z};
 // Sample along the same muzzle ray as the shot. The image remains mounted
 // inside the physical ocular, while its crosshair agrees with hit testing.
 if(selectedBallistics::ready[4]){const auto m=selectedBallistics::muzzle[4];XMStoreFloat3(&p,XMVector3TransformCoord(XMVectorSet(m.x,m.y,m.z,1),pose(s.gun)));camera.pose.position={p.x,p.y,p.z};}
 float f=l.visible?l.halfFov:r.halfFov;if(l.visible&&r.visible)f=(l.halfFov+r.halfFov)*.5f;
 camera.fov={-f,f,f,-f};
}
inline D3D12_RESOURCE_BARRIER barrier(ID3D12Resource* r,D3D12_RESOURCE_STATES a,D3D12_RESOURCE_STATES b){D3D12_RESOURCE_BARRIER v{};v.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;v.Transition={r,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,a,b};return v;}
inline bool initialize(ID3D12Device* device,unsigned samples){
 if(ready)return true;
 D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.NumDescriptors=1;hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;if(FAILED(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&targets))))return false;
 hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_DSV;if(FAILED(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&depths))))return false;
 hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;if(FAILED(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&srv))))return false;
 D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;
 D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=d.Height=resolution;d.DepthOrArraySize=d.MipLevels=1;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.SampleDesc.Count=samples;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
 D3D12_CLEAR_VALUE clear{};clear.Format=d.Format;clear.Color[3]=1;
 if(FAILED(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_RENDER_TARGET,&clear,IID_PPV_ARGS(&color))))return false;
 d.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;clear={};clear.Format=d.Format;clear.DepthStencil.Depth=1;
 if(FAILED(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_DEPTH_WRITE,&clear,IID_PPV_ARGS(&depth))))return false;
 d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.SampleDesc.Count=1;d.Flags=D3D12_RESOURCE_FLAG_NONE;
 if(FAILED(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,nullptr,IID_PPV_ARGS(&resolved))))return false;
 rtv=targets->GetCPUDescriptorHandleForHeapStart();dsv=depths->GetCPUDescriptorHandleForHeapStart();device->CreateRenderTargetView(color.Get(),nullptr,rtv);device->CreateDepthStencilView(depth.Get(),nullptr,dsv);
 D3D12_SHADER_RESOURCE_VIEW_DESC sd{};sd.Format=d.Format;sd.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;sd.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;sd.Texture2D.MipLevels=1;device->CreateShaderResourceView(resolved.Get(),&sd,srv->GetCPUDescriptorHandleForHeapStart());
 const char shader[]=R"(
 Texture2D scene:register(t0);SamplerState smp:register(s0);
 cbuffer Data:register(b0){row_major float4x4 transform;float4 aperture;}
 struct V{float4 p:SV_POSITION;float2 uv:TEXCOORD0;};
 V vs(float3 position:POSITION,float2 uv:TEXCOORD0){V o;o.uv=uv;o.p=mul(float4(position,1),transform);return o;}
 float4 ps(V v):SV_TARGET{clip(1-dot(v.uv,v.uv));float2 uv=v.uv*float2(.5,-.5)+.5;float3 c=scene.Sample(smp,uv).rgb;
 float crossDistance=min(abs(v.uv.x),abs(v.uv.y));float stroke=max(.003,fwidth(crossDistance)*1.25);float reticle=1-smoothstep(stroke,stroke*2,crossDistance);
 // Only feather the final screen pixel at the glass edge: the source scope
 // mesh supplies its physical rim, without a second black vignette.
 float radial=length(v.uv);float edge=max(fwidth(radial),.001);
 float shadow=saturate((1-radial)/edge)*aperture.x;
 c*=1-reticle*.92;
 // Small illuminated center, with a one-pixel antialiased edge in either eye.
 float dotRadius=.012;float dotEdge=max(fwidth(radial),.002);
 float center=1-smoothstep(dotRadius,dotRadius+dotEdge,radial);
 c=lerp(c,float3(1,.025,.015),center);return float4(c*shadow,1);}
 )";
 ComPtr<ID3DBlob> vs,ps,error,serialized;
 if(FAILED(D3DCompile(shader,sizeof shader,nullptr,nullptr,nullptr,"vs","vs_5_0",0,0,&vs,&error))||FAILED(D3DCompile(shader,sizeof shader,nullptr,nullptr,nullptr,"ps","ps_5_0",0,0,&ps,&error)))throw std::runtime_error(error?static_cast<const char*>(error->GetBufferPointer()):"Optic shader compilation failed");
 D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0};D3D12_ROOT_PARAMETER p[2]{};p[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;p[0].Constants={0,0,20};p[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;p[1].DescriptorTable={1,&range};p[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
 D3D12_STATIC_SAMPLER_DESC sampler{};sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;sampler.MaxLOD=D3D12_FLOAT32_MAX;sampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
 D3D12_ROOT_SIGNATURE_DESC rd{2,p,1,&sampler,D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};if(FAILED(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&serialized,&error))||FAILED(device->CreateRootSignature(0,serialized->GetBufferPointer(),serialized->GetBufferSize(),IID_PPV_ARGS(&root))))return false;
 D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();pd.VS={vs->GetBufferPointer(),vs->GetBufferSize()};pd.PS={ps->GetBufferPointer(),ps->GetBufferSize()};pd.BlendState.RenderTarget[0].RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;pd.SampleMask=UINT_MAX;pd.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;pd.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;pd.RasterizerState.DepthClipEnable=TRUE;pd.DepthStencilState.DepthEnable=TRUE;pd.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ZERO;pd.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_LESS_EQUAL;pd.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;pd.NumRenderTargets=1;pd.RTVFormats[0]=d.Format;pd.DSVFormat=DXGI_FORMAT_D24_UNORM_S8_UINT;pd.SampleDesc.Count=samples;
 D3D12_INPUT_ELEMENT_DESC elements[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
 pd.InputLayout={elements,2};
 auto lensResult=device->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&pipeline));if(FAILED(lensResult))throw std::runtime_error("Curved lens pipeline "+std::to_string(lensResult));
 hp.Type=D3D12_HEAP_TYPE_UPLOAD;d={};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=sizeof(opticLensMesh::vertices);d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
 lensResult=device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&lensVertices));if(FAILED(lensResult))throw std::runtime_error("Curved lens buffer "+std::to_string(lensResult));
 void* mapped{};D3D12_RANGE readRange{0,0};if(FAILED(lensVertices->Map(0,&readRange,&mapped)))return false;
 std::memcpy(mapped,opticLensMesh::vertices,sizeof(opticLensMesh::vertices));lensVertices->Unmap(0,nullptr);
 lensView={lensVertices->GetGPUVirtualAddress(),UINT(sizeof(opticLensMesh::vertices)),sizeof(opticLensMesh::Vertex)};
 ready=true;return true;
}
inline void clear(ID3D12GraphicsCommandList* list){if(!active)return;const float c[]={0,0,0,1};list->ClearRenderTargetView(rtv,c,0,nullptr);list->ClearDepthStencilView(dsv,D3D12_CLEAR_FLAG_DEPTH|D3D12_CLEAR_FLAG_STENCIL,1,0,0,nullptr);}
inline void resolve(ID3D12GraphicsCommandList* list){if(!active)return;D3D12_RESOURCE_BARRIER b[]={barrier(color.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_RESOLVE_SOURCE),barrier(resolved.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RESOLVE_DEST)};list->ResourceBarrier(2,b);list->ResolveSubresource(resolved.Get(),0,color.Get(),0,DXGI_FORMAT_R8G8B8A8_UNORM);for(auto& a:b)std::swap(a.Transition.StateBefore,a.Transition.StateAfter);list->ResourceBarrier(2,b);}
inline void draw(ID3D12GraphicsCommandList* list,const XrView& view,unsigned w,unsigned h,D3D12_CPU_DESCRIPTOR_HANDLE target,D3D12_CPU_DESCRIPTOR_HANDLE depthTarget){
 if(!active)return;auto a=eyeBox(view.pose);if(!a.visible)return;
 const auto& f=view.fov;float l=std::tan(f.angleLeft),r=std::tan(f.angleRight),b=std::tan(f.angleDown),t=std::tan(f.angleUp),n=.02f,z=1000;
 auto projection=XMMatrixSet(2/(r-l),0,0,0,0,2/(t-b),0,0,(r+l)/(r-l),(t+b)/(t-b),z/(n-z),-1,0,0,n*z/(n-z),0);
 float values[20]{};XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(values),pose(lens)*XMMatrixInverse(nullptr,pose(view.pose))*projection);values[16]=a.transmission;values[17]=a.shiftX;values[18]=a.shiftY;values[19]=radius;
 list->SetGraphicsRootSignature(root.Get());list->SetPipelineState(pipeline.Get());ID3D12DescriptorHeap* heaps[]={srv.Get()};list->SetDescriptorHeaps(1,heaps);list->SetGraphicsRoot32BitConstants(0,20,values,0);list->SetGraphicsRootDescriptorTable(1,srv->GetGPUDescriptorHandleForHeapStart());list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);D3D12_VIEWPORT vp{0,0,float(w),float(h),0,1};D3D12_RECT scissor{0,0,LONG(w),LONG(h)};list->RSSetViewports(1,&vp);list->RSSetScissorRects(1,&scissor);list->OMSetRenderTargets(1,&target,FALSE,&depthTarget);list->IASetVertexBuffers(0,1,&lensView);list->DrawInstanced(opticLensMesh::count,1,0,0);
}
}
