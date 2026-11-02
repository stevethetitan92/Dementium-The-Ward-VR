#pragma once
#include "pill_bottle.hpp"
#include "gamepad_bridge.hpp"
#include "wristband_native.hpp"
#include "inventory_model.hpp"
#include "inventory_reference_art.hpp"
#include "inventory_items.hpp"
#include "mod_preferences.hpp"
#include "mod_menu_art.hpp"
namespace pillInventory {
inline inventoryScreen::Controller controller;
inline std::atomic<bool> shown{},selected{};
inline int queuedEquip=-1;inline std::uint64_t queuedAt{};
inline bool ownsMap{},closingMap{},closeSent{};
inline std::uint64_t mapRequestedAt{},closeRequestedAt{},suppressFadeUntil{};
inline bool suppressFade(){return ownsMap||GetTickCount64()<suppressFadeUntil;}
inline bool mapValid{};
inline bool acceptsMap(){return ownsMap&&gamepadBridge::mapMenuActive();}
inline bool open(){return shown.load();}
struct Counts{bool valid{};unsigned owned{},keys{};std::array<unsigned,5> loaded{},reserve{};int equipped=-1;};
inline Counts counts(){
    Counts c;if(!wristband::adapterVerified||!wristband::image)return c;
    auto base=reinterpret_cast<unsigned char*>(wristband::image);
    if(!*reinterpret_cast<void**>(base+0x3a5330))return c;
    constexpr unsigned loaded[]={0x396346,0x396344,0x39634a,0x396348,0x39634c};
    constexpr unsigned reserve[]={0x396345,0x396343,0x396349,0x396347,0x39634b};
    c.valid=true;c.owned=base[0x396380];
    // Read-only native inventory bits, with guards for both predicate and item IDs.
    const unsigned char keyGuard[]={0x81,0xfa,0xe8,3,0,0,0x73,0x1b,0x8b,0xc2,0x48,0x8d,0x0d,0x9f,0x4d,0x30};
    if(!std::memcmp(base+0x915d0,keyGuard,sizeof keyGuard)&&!std::memcmp(base+0x2ea990,inventoryItems::ids.data(),sizeof(unsigned)*inventoryItems::ids.size())){
        std::uint32_t flags[2];std::memcpy(flags,base+0x396380,sizeof flags);c.keys=inventoryItems::ownedMask(flags[0],flags[1]);
    }
    for(unsigned i=0;i<5;i++){c.loaded[i]=base[loaded[i]];c.reserve[i]=base[reserve[i]];}
    // Read native equipment independently of whether a gun is currently being
    // rendered, so the marker also works while holstered or in this screen.
    auto player=*reinterpret_cast<unsigned char**>(base+0x3a5340);
    if(manualReload::readable(player,0x320)&&*reinterpret_cast<std::uintptr_t*>(player)==wristband::image+0x30c0f8){
        auto head=reinterpret_cast<std::uintptr_t>(player)+0x308,link=*reinterpret_cast<std::uintptr_t*>(head);
        if(link!=head&&link>=0x98&&manualReload::readable(reinterpret_cast<void*>(link-0x98),0xd0)){
            auto held=reinterpret_cast<unsigned char*>(link-0x98);
            constexpr std::uintptr_t tables[]={0x30c500,0x30c640,0x30c900,0x30d2e0,0x30d668,0x30cb70,0x30d1a8};
            for(int i=0;i<7;i++)if(*reinterpret_cast<std::uintptr_t*>(held)==wristband::image+tables[i]&&(*reinterpret_cast<unsigned*>(held+0x44)&(i>=5?0x08:0x18))==(i>=5?0x08:0x18))c.equipped=i;
        }
    }
    return c;
}
inline void route(const vrControls::Input& raw,vrControls::Output& out,bool allowed=true){
    auto c=counts();auto now=GetTickCount64();bool was=controller.visible();
    auto result=controller.step(raw,allowed,c.valid?c.owned:0,pillBottle::installed?pillBottle::carried.load():0);
    shown=controller.visible();selected=controller.selection()==8;
    if(shown&&!was){mapValid=false;closingMap=closeSent=false;mapRequestedAt=now;ownsMap=gamepadBridge::requestMenu(gamepadBridge::MenuTarget::map);if(ownsMap)gamepadBridge::shortenInventoryMapWait();}
    if(!shown&&was){closingMap=ownsMap;closeSent=false;closeRequestedAt=now;suppressFadeUntil=now+2000;}
    // Keep ownership through the native fade/deferred transition. Closing the
    // custom controller must not expose native map input or cancel queued equip.
    if(closingMap){
        if(!closeSent&&acceptsMap())closeSent=gamepadBridge::requestMenu(gamepadBridge::MenuTarget::map);
        if((closeSent&&gamepadBridge::menuState()==0)||now-closeRequestedAt>10000){ownsMap=closingMap=closeSent=false;}
    }
    // Consume Y locally: the native inventory is never requested or displayed.
    out.toggleInventory=false;out.toggleModMenu=false;
    if(result.toggleBlackFog){if(modPreferences::toggleBlackFog())vrFeedback::queue(0,.18f,18);}
    if(result.toggleHaze){if(modPreferences::toggleHaze())vrFeedback::queue(0,.18f,18);}
    if(result.equip>=0){queuedEquip=result.equip;queuedAt=now;bodyHolsters::reveal();}
    if(result.use)pillBottle::requestUse();
    if(result.changed)vrFeedback::queue(0,.18f,18);
    if(result.captured||closingMap){out={};out.menuCapture=true;out.context=shown?vrControls::Context::inventory:vrControls::Context::gameplay;}
    if(!raw.active){queuedEquip=-1;return;}
    if(!allowed&&!ownsMap){queuedEquip=-1;return;}
    // Equip on the native player update after the custom screen closes and the
    // native menu has returned to gameplay; menu selection needs no hand pose.
    if(queuedEquip>=0&&!shown){
        if(now<queuedAt||now-queuedAt>3000)queuedEquip=-1;
        else if(!closingMap&&gamepadBridge::menuState()==0&&c.valid){weaponGroups::equipFromInventory(c.equipped,queuedEquip,now);queuedEquip=-1;}
    }
}
using Microsoft::WRL::ComPtr;
inline ComPtr<ID3D12RootSignature> root;
inline ComPtr<ID3D12PipelineState> pipeline;
inline ComPtr<ID3D12DescriptorHeap> srv,rtv;
inline ComPtr<ID3D12Resource> atlas,upload,mapTexture;
inline ID3D12Device* owner{};inline DXGI_FORMAT targetFormat{};
inline bool initialize(ID3D12Device* device,ID3D12GraphicsCommandList* list,DXGI_FORMAT format){
    if(owner==device&&targetFormat==format&&pipeline)return true;
    root.Reset();pipeline.Reset();srv.Reset();rtv.Reset();atlas.Reset();upload.Reset();mapTexture.Reset();mapValid=false;owner=nullptr;
    const char shader[]=R"(
Texture2D art:register(t0);SamplerState linearClamp:register(s0);
Texture2D liveMap:register(t1);
cbuffer State:register(b0){uint4 words[5];}
uint value(uint i){return words[i/4][i%4];}
struct Vertex{float4 p:SV_POSITION;float2 uv:TEXCOORD0;};
Vertex vs(uint id:SV_VertexID){Vertex o;o.uv=float2((id<<1)&2,id&2);o.p=float4(o.uv*float2(2,-2)+float2(-1,1),0,1);return o;}
float4 sampleAt(float2 p){return art.SampleLevel(linearClamp,(p+float2(1024.5,.5))/float2(2048,3072),0);}
bool inside(float2 p,float2 a,float2 size){return all(p>=a)&&all(p<a+size);}
float4 ps(Vertex v):SV_TARGET{
 float2 p=v.uv*float2(1000,900);float4 color=art.SampleLevel(linearClamp,(p+.5)/float2(2048,3072),0);
 if(value(17)>0){
  float2 origin=float2(value(17)==2?1024:0,value(17)==3?2048:1024);
  color=art.SampleLevel(linearClamp,(p+origin+.5)/float2(2048,3072),0);
  if(value(17)==1&&inside(p,float2(780,245),float2(100,44))){float2 q=p-float2(780,245)+float2(0,value(18)!=0?1939:1989);float4 label=art.SampleLevel(linearClamp,(q+.5)/float2(2048,3072),0);color.rgb=lerp(color.rgb,label.rgb,label.a);}
  if(value(17)==1){
   if(inside(p,float2(780,405),float2(100,44))){float2 q=p-float2(780,405)+float2(0,(value(19)&1)!=0?1939:1989);float4 label=art.SampleLevel(linearClamp,(q+.5)/float2(2048,3072),0);color.rgb=lerp(color.rgb,label.rgb,label.a);}
   float y=(value(19)>>1)!=0?365:205;
   if(inside(p,float2(64,y+4),float2(872,142))&&!inside(p,float2(68,y+8),float2(864,134)))color.rgb=float3(.38,.11,.1);
  }
  return color;
 }
 const float4 rects[24]={float4(30,620,220,110),float4(270,620,220,110),float4(510,620,220,110),float4(750,620,220,110),float4(30,750,220,110),float4(270,750,220,110),float4(510,750,220,110),float4(750,750,220,110),float4(30,80,120,150),float4(170,80,570,500),float4(760,80,100,65),float4(880,80,100,65),float4(760,152,100,65),float4(880,152,100,65),float4(760,224,100,65),float4(880,224,100,65),float4(760,296,100,65),float4(880,296,100,65),float4(760,368,100,65),float4(880,368,100,65),float4(760,440,100,65),float4(880,440,100,65),float4(760,512,100,65),float4(880,512,100,65)};
 for(int slot=0;slot<24;slot++){
  float4 r=rects[slot];if(inside(p,r.xy,r.zw)){
   float2 local=p-r.xy;
   if(slot>=9){
    if(slot>=10&&((value(16)>>(slot-10))&1u)!=0&&local.y>=4&&local.y<60){uint k=slot-10;float2 uv=(float2((k%7)*140,900+(k/7)*56)+float2(local.x,local.y-4)+.5)/float2(2048,3072);float4 label=art.SampleLevel(linearClamp,uv,0);color.rgb=lerp(color.rgb,label.rgb,label.a);}

    if(slot==9&&value(15)!=0){float2 uv=(local-float2(0,73))/float2(570,354);if(all(uv>=0)&&all(uv<=1))color=liveMap.SampleLevel(linearClamp,float2(185.0/860,0)+uv*float2(515.0/860,320.0/360),0);}
    if(slot==int(value(0))&&(local.x<2||local.y<2||local.x>r.z-3||local.y>r.w-3))color=float4(.86,.84,.70,1);return color;
   }
   float2 q=local*float2(272,107)/r.zw;
   const int weapons[9]={0,1,3,2,4,5,6,-1,-2};const uint ids[9]={6,2,3,5,7,4,1,0,0};
   int weapon=weapons[slot];bool owned=value(3)!=0&&((value(2)&(1u<<ids[slot]))!=0);
   if(slot==7)owned=true;if(slot==8)owned=value(1)>0;
   // The count communicates empty medicine; never darken a rectangular patch.
   // Original source icons are baked into this layout. Dim only unavailable
   // items; selection uses a border without an opaque legacy-card rectangle.
   if(slot<8&&!owned&&q.y>28&&q.y<84)color.rgb*=.24;
   if(slot<5&&inside(q,float2(127,80),float2(132,25))){
    float2 n=q-float2(127,80);int digit=int(n.x/18);uint glyph=11;
    if(owned){uint number=digit<3?value(4+weapon):value(9+weapon);uint index=digit<3?digit:digit-4;
     if(digit==3)glyph=10;else if(index==0)glyph=(number/100)%10;else if(index==1)glyph=(number/10)%10;else glyph=number%10;
    }else if(digit==3)glyph=10;
    if(digit<7){float4 ink=sampleAt(float2(glyph*22+fmod(n.x,18),520+n.y));if(max(ink.r,max(ink.g,ink.b))>.025)color=ink;}
   }
   if(slot==8&&inside(local,float2(33,119),float2(54,25))){
    float2 n=local-float2(33,119);int digit=int(n.x/18);uint glyph=digit==0?min(value(1),2u):digit==1?10:2;
    float4 ink=sampleAt(float2(glyph*22+fmod(n.x,18),520+n.y));if(max(ink.r,max(ink.g,ink.b))>.025)color=ink;
   }
   if(weapon>=0&&value(14)==uint(weapon+1)&&inside(q,float2(179,11),float2(82,12))){float4 ink=sampleAt(float2(0,565)+q-float2(179,11));if(max(ink.r,max(ink.g,ink.b))>.025)color=ink;}
   if(slot==7&&inside(q,float2(225,92),float2(35,15))){float4 ink=sampleAt(float2(280,565)+q-float2(225,92));if(max(ink.r,max(ink.g,ink.b))>.025)color=ink;}
   // Chamfered ivory focus border; a muted rust underline marks equipped gear.
   bool shape=q.x+q.y>=8&&(272-q.x)+q.y>=8;
   bool border=q.x<2||q.x>269||q.y<2||q.y>104||q.x+q.y<11||(272-q.x)+q.y<11;
   if(shape&&border&&slot==int(value(0)))color=float4(.86,.84,.70,1);
  }
 }
 return float4(color.rgb,1);
})";
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
    D3D12_RESOURCE_DESC td{};td.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;td.Width=2048;td.Height=3072;td.DepthOrArraySize=td.MipLevels=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;
    if(FAILED(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&td,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&atlas))))return false;
    auto bd=td;bd.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;bd.Width=2048*3072*4;bd.Height=1;bd.Format=DXGI_FORMAT_UNKNOWN;bd.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;hp.Type=D3D12_HEAP_TYPE_UPLOAD;
    if(FAILED(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&upload))))return false;
    std::uint32_t* mapped{};D3D12_RANGE empty{};if(FAILED(upload->Map(0,&empty,reinterpret_cast<void**>(&mapped))))return false;
    size_t n=0;for(auto run:inventoryReferenceArt::runs){if(n+run.count>2048*1024){upload->Unmap(0,nullptr);return false;}std::fill_n(mapped+n,run.count,run.rgba);n+=run.count;}if(n!=2048*1024){upload->Unmap(0,nullptr);return false;}for(auto run:modMenuArt::runs){if(n+run.count>2048*3072){upload->Unmap(0,nullptr);return false;}std::fill_n(mapped+n,run.count,run.rgba);n+=run.count;}upload->Unmap(0,nullptr);if(n!=2048*3072)return false;
    D3D12_TEXTURE_COPY_LOCATION dest{},src{};dest.pResource=atlas.Get();dest.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;src.pResource=upload.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint.Footprint={DXGI_FORMAT_R8G8B8A8_UNORM,2048,3072,1,8192};list->CopyTextureRegion(&dest,0,0,0,&src,nullptr);
    D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition={atlas.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};list->ResourceBarrier(1,&barrier);
    D3D12_SHADER_RESOURCE_VIEW_DESC sd{};sd.Format=td.Format;sd.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;sd.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;sd.Texture2D.MipLevels=1;device->CreateShaderResourceView(atlas.Get(),&sd,srv->GetCPUDescriptorHandleForHeapStart());
    auto emptyMap=srv->GetCPUDescriptorHandleForHeapStart();emptyMap.ptr+=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);device->CreateShaderResourceView(nullptr,&sd,emptyMap);
    owner=device;targetFormat=format;return true;
}
// Copy only fresh native map draws before custom paint clears its target.
// A separate resource avoids simultaneous render-target/SRV access and prevents
// recursively capturing the previously composed custom inventory.
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
inline void captureMap(ID3D12Device* device,ID3D12GraphicsCommandList* list,ID3D12Resource* source){
    if(open()&&acceptsMap())copyMapFrame(device,list,source);
}
inline bool paint(ID3D12Device* device,ID3D12GraphicsCommandList* list,ID3D12Resource* target,unsigned w,unsigned h,const Counts& c=counts()){
    if(w<64||h<64||!initialize(device,list,target->GetDesc().Format))return false;
    unsigned words[20]{};words[0]=controller.selection();words[1]=(std::min)(2u,pillBottle::carried.load());words[2]=c.owned;words[3]=c.valid;
    for(unsigned i=0;i<5;i++){words[4+i]=c.loaded[i];words[9+i]=c.reserve[i];}words[14]=c.equipped+1;
    words[15]=mapValid;words[16]=c.keys;words[17]=controller.menuPage();words[18]=modPreferences::whiteHaze.load();words[19]=unsigned(modPreferences::blackFog.load())|(controller.settingSelection()<<1);
    auto handle=rtv->GetCPUDescriptorHandleForHeapStart();device->CreateRenderTargetView(target,nullptr,handle);list->OMSetRenderTargets(1,&handle,FALSE,nullptr);
    // Only the inventory panel is opaque; reveal the world outside its bounds.
    const float transparent[]={0,0,0,0};list->ClearRenderTargetView(handle,transparent,0,nullptr);
    // Leave 15% margin per edge for headset readability; preserve aspect and controls.
    float width=.70f*(std::min)(float(w),float(h)*1000.f/900.f),height=width*900.f/1000.f;
    D3D12_VIEWPORT viewport{(w-width)*.5f,(h-height)*.5f,width,height,0,1};D3D12_RECT scissor{0,0,LONG(w),LONG(h)};list->RSSetViewports(1,&viewport);list->RSSetScissorRects(1,&scissor);
    list->SetPipelineState(pipeline.Get());list->SetGraphicsRootSignature(root.Get());ID3D12DescriptorHeap* heaps[]={srv.Get()};list->SetDescriptorHeaps(1,heaps);list->SetGraphicsRootDescriptorTable(0,srv->GetGPUDescriptorHandleForHeapStart());list->SetGraphicsRoot32BitConstants(1,20,words,0);list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);list->DrawInstanced(3,1,0,0);return true;
}
inline bool draw(ID3D12Device* device,ID3D12GraphicsCommandList* list,ID3D12Resource* target,unsigned w,unsigned h){return (open()||closingMap)&&paint(device,list,target,w,h);}
}

