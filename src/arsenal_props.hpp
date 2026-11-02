#pragma once
#include "arsenal_runtime.hpp"
#include "sniper_scope.hpp"
#include "casing_policy.hpp"
#include "casing_flight.hpp"
#include "selected_reload_profile.hpp"
#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <fstream>
#include <string>
#include <filesystem>
namespace arsenalProps {
inline bool sourceShellReady{},sourceLoaderReady{};
using Microsoft::WRL::ComPtr;
using namespace DirectX;
struct Vertex {float x,y,z,r,g,b;};
inline std::array<std::vector<Vertex>,5> sourceCases;
inline std::array<bool,5> texturedCases{};
inline void loadCases(const std::filesystem::path& folder){
    casingFlight::portReady={};std::vector<unsigned char> ports,g,f;
    if(psxAssets::read(folder/L"SelectedEjectionPorts.bin",ports,108)&&ports.size()>=28&&
       psxAssets::read(folder/L"SelectedGeometry.psx",g,64*1024*1024)&&psxAssets::read(folder/L"SelectedFrames.bin",f,4096)){
        unsigned h[2],count;std::uint64_t hashes[2];std::memcpy(h,ports.data(),8);std::memcpy(hashes,ports.data()+8,16);std::memcpy(&count,ports.data()+24,4);
        if(h[0]==0x45585044&&h[1]==1&&count<=4&&ports.size()==28+count*16&&hashes[0]==selectedReload::fingerprint(g)&&hashes[1]==selectedReload::fingerprint(f))
            for(unsigned n=0;n<count;++n){unsigned w;XrVector3f p;std::memcpy(&w,ports.data()+28+n*16,4);std::memcpy(&p,ports.data()+32+n*16,12);
                if(w>0&&w<5&&std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::fabs(p.x)<1&&std::fabs(p.y)<1&&std::fabs(p.z)<1){casingFlight::ports[w]=p;casingFlight::portReady[w]=true;}}
    }
    for(unsigned w=1;w<5;++w){auto& out=sourceCases[w];out.clear();
        std::ifstream file(folder/(L"SelectedCase"+std::to_wstring(w)+L".bin"),std::ios::binary|std::ios::ate);
        if(!file)continue;auto size=file.tellg();if(size<12||size>4*1024*1024)continue;file.seekg(0);
        unsigned h[3]{};file.read(reinterpret_cast<char*>(h),12);
        if(h[0]!=0x43425044||h[1]!=1||!h[2]||h[2]%3||h[2]>60000||size!=12+size_t(h[2])*sizeof(Vertex))continue;
        std::vector<Vertex> candidate(h[2]);file.read(reinterpret_cast<char*>(candidate.data()),candidate.size()*sizeof(Vertex));if(!file)continue;
        bool valid=true;for(const auto& v:candidate){
            if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||std::fabs(v.x)>.501f||std::fabs(v.y)>.501f||std::fabs(v.z)>.501f||
                !std::isfinite(v.r)||!std::isfinite(v.g)||!std::isfinite(v.b)||v.r<0||v.r>1||v.g<0||v.g>1||v.b<0||v.b>1){valid=false;break;}}
        if(valid)out=std::move(candidate);
    }
}
inline std::vector<Vertex> mesh(bool casing=false){
    std::vector<Vertex> result;constexpr float pi=3.14159265359f;
    auto v=[](float angle,float z,float radius,float shade){return Vertex{std::cos(angle)*radius,std::sin(angle)*radius,z,.72f*shade,.55f*shade,.19f*shade};};
    for(unsigned i=0;i<16;i++){
        float a=2*pi*i/16,b=2*pi*(i+1)/16;
        auto p=v(a,-.5f,.5f,.8f),q=v(b,-.5f,.5f,.8f),r=v(a,.25f,.5f,1),s=v(b,.25f,.5f,1),tip=v(0,.5f,0,.8f);
        if(casing){
            r=v(a,.5f,.5f,1);s=v(b,.5f,.5f,1);
            auto innerA=v(a,.5f,.38f,.35f),innerB=v(b,.5f,.38f,.35f);
            result.insert(result.end(),{p,r,q,q,r,s,r,innerA,s,s,innerA,innerB,p,q,v(0,-.5f,0,.8f),innerA,v(0,.35f,0,.2f),innerB});
        }else result.insert(result.end(),{p,r,q,q,r,s,r,tip,s,p,q,v(0,-.5f,0,.8f)});
    }
    return result;
}
struct Prop {XrPosef pose;XrVector3f scale;XrVector3f color;bool casing{};unsigned weapon{};};
inline std::vector<Prop> displayedProps;
inline bool(*worldFrame)(XMMATRIX&,std::uintptr_t&){};
inline casingPolicy::Ejection ejections;
inline casingFlight::Flight flight;
inline std::uintptr_t caseScene{};
inline unsigned emittedCases{};
inline void casingProps(std::uint64_t now,std::vector<Prop>& out){
    const auto& h=manualReload::hands;XMMATRIX trackingToWorld;std::uintptr_t scene{};
    bool valid=h.session&&h.gameplay&&h.gunTracked&&now>=h.time&&now-h.time<100&&motionTracking::validPose(h.gun)&&
        worldFrame&&worldFrame(trackingToWorld,scene);
    if(!valid){
        flight.clear();
        for(unsigned w=0;w<5;++w)ejections.update(w,weaponShots::observers[w].serial,false,false,false);
        return;
    }
    if(scene!=caseScene){flight.clear();ejections.reset();caseScene=scene;}
    unsigned weapon=static_cast<unsigned>(h.weapon);
    if(weapon<5){
        bool open=arsenal::cylinderTransition.value()>=.999f;
        bool pump=arsenal::visuals[2].slidePull>=.045f;
        auto n=ejections.update(weapon,weaponShots::observers[weapon].serial,open,pump,true);
        if(n){auto gun=motionTracking::rotation(h.gun.orientation)*XMMatrixTranslation(h.gun.position.x,h.gun.position.y,h.gun.position.z);
            flight.emit(weapon,gun*trackingToWorld,h.scale,now,n);emittedCases+=n;}
    }
    flight.visit(now,XMMatrixInverse(nullptr,trackingToWorld),[&](unsigned weapon,float units,const XMMATRIX& model){
        XMVECTOR s,q,p;XMMatrixDecompose(&s,&q,&p,model);XMFLOAT4 orientation;XMFLOAT3 position,scale;
        XMStoreFloat4(&orientation,q);XMStoreFloat3(&position,p);XMStoreFloat3(&scale,s);
        float diameter=weapon==3?.018f:weapon==2?.0096f:.009f,length=weapon==3?.055f:weapon==2?.045f:weapon==4?.05f:.022f;
        out.push_back({{{orientation.x,orientation.y,orientation.z,orientation.w},{position.x,position.y,position.z}},
            {diameter*units*scale.x,diameter*units*scale.y,length*units*scale.z},{1,1,1},true,weapon});
    });
}
inline std::vector<Prop> props(std::uint64_t now){
    std::lock_guard<std::mutex> lock(manualReload::mutex);const auto& h=manualReload::hands;
    std::vector<Prop> out;casingProps(now,out);int i=arsenal::index(h.weapon);
    if(i<0||i>=4||!arsenal::enabled||!arsenal::activeLocked()||!h.gameplay||!h.gunTracked||now<h.time||now-h.time>=100||!motionTracking::nearHead(h.gun,h.head)||!std::isfinite(h.scale)||h.scale<.001f||h.scale>100)return out;
    auto kind=arsenal::profiles[i].kind;auto phase=arsenal::reloads[i].state();
    if(kind==arsenal::Kind::Sniper&&sniperScope::factor()>1.01f)return out;
    if(kind==arsenal::Kind::Sniper&&arsenal::reloads[i].usesDetachableMagazine())return out;
    if(kind==arsenal::Kind::Revolver&&!sourceLoaderReady&&arsenal::loader.active&&h.tracked&&motionTracking::nearHead(h.left,h.head)){
        auto heldPose=h.left;heldPose.position=arsenal::replacementRevolverContact.contact(h.left);
        XMFLOAT3 back;XMStoreFloat3(&back,XMVector3TransformNormal(XMVectorSet(0,0,.023f,0),motionTracking::rotation(heldPose.orientation)));
        heldPose.position.x+=back.x;heldPose.position.y+=back.y;heldPose.position.z+=back.z;
        // Carrier body remains in the hand, empty after a successful transfer.
        // The fitted renderer supplies the actual six chamber-aligned rounds.
        out.push_back({heldPose,{.043f,.043f,.008f},{.16f,.18f,.21f}});
    }
    if(phase==arsenal::Phase::Held&&kind!=arsenal::Kind::Rifle&&kind!=arsenal::Kind::Revolver&&!(kind==arsenal::Kind::Shotgun&&sourceShellReady)&&h.tracked&&motionTracking::nearHead(h.left,h.head)){
        bool shell=kind==arsenal::Kind::Shotgun;
        auto heldPose=h.left;if(shell)heldPose.position=arsenal::replacementShotgunContact.contact(h.left);if(kind==arsenal::Kind::Revolver)heldPose.position=arsenal::replacementRevolverContact.contact(h.left);
        out.push_back({heldPose,shell?XrVector3f{.018f,.018f,.055f}:XrVector3f{.009f,.009f,kind==arsenal::Kind::Sniper?.065f:.035f},shell?XrVector3f{1,.2f,.15f}:XrVector3f{1,1,1}});
    }
    if((kind==arsenal::Kind::Sniper&&!arsenal::reloads[i].usesDetachableMagazine())||(kind==arsenal::Kind::Rifle&&!arsenal::replacementRifleContact.ready)){
        auto p=h.gun;auto local=arsenal::handle(kind,h.scale);
        if(phase==arsenal::Phase::Charging)local.z+=arsenal::visuals[i].slidePull;
        else if(kind==arsenal::Kind::Sniper)local.z+=arsenal::reloads[i].boltOffset();
        XMFLOAT3 offset;XMStoreFloat3(&offset,XMVector3TransformNormal(XMVectorSet(local.x,local.y,local.z,0),motionTracking::rotation(p.orientation)));
        p.position.x+=offset.x;p.position.y+=offset.y;p.position.z+=offset.z;
        out.push_back({p,{.018f,.018f,.045f},{.4f,.5f,.6f}});
    }
    return out;
}
inline constexpr char shader[]=R"(
cbuffer Transform:register(b0){row_major float4x4 mvp;float4 tint;}
struct V {float3 p:POSITION;float3 c:COLOR;};struct O {float4 p:SV_POSITION;float3 c:COLOR;};
O vs(V v){O o;o.p=mul(float4(v.p,1),mvp);o.c=v.c*tint.xyz;return o;}
float4 ps(O v):SV_TARGET{return float4(v.c,1);}
)";
inline ComPtr<ID3D12RootSignature> root;inline ComPtr<ID3D12PipelineState> pipeline;
inline ComPtr<ID3D12Resource> vertices;inline D3D12_VERTEX_BUFFER_VIEW vb{};inline unsigned count{},samples{};inline ID3D12Device* owner{};
inline bool failed{};
inline std::array<unsigned,5> caseFirst{},caseCount{};
inline bool initialize(ID3D12Device* device,unsigned sampleCount){
    if(owner==device&&pipeline&&samples==sampleCount)return true;if(failed)return false;
    ComPtr<ID3DBlob> vs,ps,error;
    if(FAILED(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"vs","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vs,&error))||
       FAILED(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"ps","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&ps,&error))){failed=true;return false;}
    D3D12_ROOT_PARAMETER parameter{};parameter.ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameter.Constants.Num32BitValues=20;parameter.ShaderVisibility=D3D12_SHADER_VISIBILITY_VERTEX;
    D3D12_ROOT_SIGNATURE_DESC desc{};desc.NumParameters=1;desc.pParameters=&parameter;desc.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> serialized;
    if(FAILED(D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&serialized,&error))||
       FAILED(device->CreateRootSignature(0,serialized->GetBufferPointer(),serialized->GetBufferSize(),IID_PPV_ARGS(&root)))){failed=true;return false;}
    D3D12_INPUT_ELEMENT_DESC layout[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};p.pRootSignature=root.Get();p.VS={vs->GetBufferPointer(),vs->GetBufferSize()};p.PS={ps->GetBufferPointer(),ps->GetBufferSize()};
    p.InputLayout={layout,2};p.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;p.NumRenderTargets=1;p.RTVFormats[0]=DXGI_FORMAT_R8G8B8A8_UNORM;p.DSVFormat=DXGI_FORMAT_D24_UNORM_S8_UINT;p.SampleDesc.Count=sampleCount;p.SampleMask=UINT_MAX;
    p.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;p.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;p.RasterizerState.DepthClipEnable=TRUE;p.RasterizerState.MultisampleEnable=sampleCount>1;
    p.DepthStencilState.DepthEnable=TRUE;p.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ALL;p.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_LESS_EQUAL;
    p.DepthStencilState.StencilReadMask=p.DepthStencilState.StencilWriteMask=255;
    p.DepthStencilState.FrontFace={D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_COMPARISON_FUNC_ALWAYS};p.DepthStencilState.BackFace=p.DepthStencilState.FrontFace;
    auto& blend=p.BlendState.RenderTarget[0];blend.SrcBlend=blend.SrcBlendAlpha=D3D12_BLEND_ONE;blend.DestBlend=blend.DestBlendAlpha=D3D12_BLEND_ZERO;blend.BlendOp=blend.BlendOpAlpha=D3D12_BLEND_OP_ADD;blend.LogicOp=D3D12_LOGIC_OP_NOOP;blend.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
    if(FAILED(device->CreateGraphicsPipelineState(&p,IID_PPV_ARGS(&pipeline)))){failed=true;return false;}
    auto geometry=mesh();count=static_cast<unsigned>(geometry.size());
    for(unsigned w=1;w<5;++w){caseFirst[w]=static_cast<unsigned>(geometry.size());const auto& cases=sourceCases[w];caseCount[w]=static_cast<unsigned>(cases.size());geometry.insert(geometry.end(),cases.begin(),cases.end());}
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC buffer{};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.Width=geometry.size()*sizeof(Vertex);buffer.Height=1;buffer.DepthOrArraySize=1;buffer.MipLevels=1;buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if(FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&vertices)))){failed=true;return false;}
    void* data{};D3D12_RANGE empty{0,0};if(FAILED(vertices->Map(0,&empty,&data))){failed=true;return false;}
    std::memcpy(data,geometry.data(),geometry.size()*sizeof(Vertex));vertices->Unmap(0,nullptr);
    vb={vertices->GetGPUVirtualAddress(),static_cast<UINT>(buffer.Width),sizeof(Vertex)};owner=device;samples=sampleCount;return true;
}
inline XMMATRIX projection(const XrFovf& f){
    float l=std::tan(f.angleLeft),r=std::tan(f.angleRight),b=std::tan(f.angleDown),t=std::tan(f.angleUp),nearPlane=.02f,farPlane=1000;
    return XMMatrixSet(2/(r-l),0,0,0,0,2/(t-b),0,0,(r+l)/(r-l),(t+b)/(t-b),farPlane/(nearPlane-farPlane),-1,0,0,nearPlane*farPlane/(nearPlane-farPlane),0);
}
inline void draw(ID3D12Device* device,ID3D12GraphicsCommandList* list,unsigned sampleCount,D3D12_CPU_DESCRIPTOR_HANDLE rtv,D3D12_CPU_DESCRIPTOR_HANDLE dsv,const XrView& view,unsigned width,unsigned height){
    auto objects=props(GetTickCount64());displayedProps=objects;if(objects.empty()||!initialize(device,sampleCount))return;
    list->SetGraphicsRootSignature(root.Get());list->SetPipelineState(pipeline.Get());list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);list->IASetVertexBuffers(0,1,&vb);
    D3D12_VIEWPORT viewport{0,0,float(width),float(height),0,1};D3D12_RECT rect{0,0,LONG(width),LONG(height)};
    list->RSSetViewports(1,&viewport);list->RSSetScissorRects(1,&rect);list->OMSetRenderTargets(1,&rtv,FALSE,&dsv);
    auto camera=motionTracking::rotation(view.pose.orientation)*XMMatrixTranslation(view.pose.position.x,view.pose.position.y,view.pose.position.z);
    auto eye=XMMatrixInverse(nullptr,camera);
    for(const auto& object:objects){
        if(object.casing&&object.weapon<5&&texturedCases[object.weapon])continue;
        auto model=XMMatrixScaling(object.scale.x,object.scale.y,object.scale.z)*motionTracking::rotation(object.pose.orientation)*XMMatrixTranslation(object.pose.position.x,object.pose.position.y,object.pose.position.z);
        float values[20];XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(values),model*eye*projection(view.fov));values[16]=object.color.x;values[17]=object.color.y;values[18]=object.color.z;values[19]=1;
        if(object.casing&&(object.weapon>=5||!caseCount[object.weapon]))continue;
        list->SetGraphicsRoot32BitConstants(0,20,values,0);list->DrawInstanced(object.casing?caseCount[object.weapon]:count,1,object.casing?caseFirst[object.weapon]:0,0);
    }
}
}

