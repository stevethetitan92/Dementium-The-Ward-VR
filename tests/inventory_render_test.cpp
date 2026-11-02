// Real D3D12 WARP rendering of the actual replacement assets. No game or XR session.
#include "../src/wristband_native.hpp"
#include "../src/wristband_renderer.hpp"
#include "../src/pill_inventory_ui.hpp"
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <fstream>
#include <iostream>
using Microsoft::WRL::ComPtr;
static void check(HRESULT value){if(FAILED(value))throw std::runtime_error("D3D12 offscreen failure: "+std::to_string(static_cast<unsigned>(value)));}
static unsigned nativeRequests{};
static bool menuSpy(gamepadBridge::MenuTarget target){if(target!=gamepadBridge::MenuTarget::map)throw std::runtime_error("native weapons inventory requested");++nativeRequests;return true;}
int main(int argc,char** argv){try{
 bool empty=argc==2&&!std::strcmp(argv[1],"--empty"),full=argc==2&&!std::strcmp(argv[1],"--full");
 unsigned page=argc==2&&!std::strcmp(argv[1],"--settings")?1:argc==2&&!std::strcmp(argv[1],"--controls")?2:argc==2&&!std::strcmp(argv[1],"--reloading")?3:0;
 auto request=gamepadBridge::requestMenu;gamepadBridge::requestMenu=menuSpy;
 vrControls::Input input{};input.active=true;input.y=true;vrControls::Output routed{};routed.fire=true;routed.toggleInventory=true;routed.walk=1;
 pillInventory::route(input,routed,true);
 if(!pillInventory::open()||routed.toggleInventory||routed.fire||routed.walk||!routed.menuCapture||nativeRequests!=1)throw std::runtime_error("independent inventory must capture gameplay while requesting only native live map");
 input.y=false;pillInventory::route(input,routed,true);input.b=true;pillInventory::route(input,routed,true);
 if(pillInventory::open()||routed.back||nativeRequests!=1)throw std::runtime_error("custom back must close locally when no native map is active");
 gamepadBridge::requestMenu=request;pillInventory::controller={};
 if(page){vrControls::Input tab{};tab.active=true;tab.l3=tab.r3=true;auto r=pillInventory::controller.step(tab,true,255,2);if(!r.captured||r.equip!=-1||r.use)throw std::runtime_error("mod menu leaked gameplay");tab.l3=tab.r3=false;pillInventory::controller.step(tab,true,255,2);if(page>=2){tab.rightGrip=1;pillInventory::controller.step(tab,true,255,2);tab.rightGrip=0;pillInventory::controller.step(tab,true,255,2);}if(page==3){tab.rightGrip=1;r=pillInventory::controller.step(tab,true,255,2);if(r.toggleHaze||r.equip!=-1||r.use)throw std::runtime_error("help page A leaked action");}if(pillInventory::controller.menuPage()!=page)throw std::runtime_error("wrong controls page");}
 bodyHolsters::State storage;auto anchor=bodyHolsters::anchors[0];storage.step(true,false,anchor,1,255);storage.step(true,true,anchor,1,255);if(!storage.stowed())throw std::runtime_error("holster setup");storage.reveal();if(storage.stowed())throw std::runtime_error("inventory equip must reveal holstered gear");
 ComPtr<ID3D12Debug> debug;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
 ComPtr<IDXGIFactory4> factory;check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));ComPtr<IDXGIAdapter> adapter;check(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)));
 ComPtr<ID3D12Device> device;check(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));
 ComPtr<ID3D12InfoQueue> diagnostics;device.As(&diagnostics);
 D3D12_COMMAND_QUEUE_DESC q{};ComPtr<ID3D12CommandQueue> queue;check(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)));
 ComPtr<ID3D12CommandAllocator> allocator;check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
 ComPtr<ID3D12GraphicsCommandList> list;check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)));
 D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.NumDescriptors=1;hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;ComPtr<ID3D12DescriptorHeap> rtvHeap;check(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtvHeap)));
 hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_DSV;ComPtr<ID3D12DescriptorHeap> dsvHeap;check(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&dsvHeap)));
 D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;D3D12_RESOURCE_DESC color{};color.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;color.Width=1024;color.Height=768;color.DepthOrArraySize=color.MipLevels=1;color.Format=DXGI_FORMAT_R8G8B8A8_UNORM;color.SampleDesc.Count=1;color.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
 ComPtr<ID3D12Resource> target;check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&color,D3D12_RESOURCE_STATE_RENDER_TARGET,nullptr,IID_PPV_ARGS(&target)));
 auto rtv=rtvHeap->GetCPUDescriptorHandleForHeapStart();device->CreateRenderTargetView(target.Get(),nullptr,rtv);
 auto depth=color;depth.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;depth.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;D3D12_CLEAR_VALUE clear{};clear.Format=depth.Format;clear.DepthStencil.Depth=1;
 ComPtr<ID3D12Resource> z;check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&depth,D3D12_RESOURCE_STATE_DEPTH_WRITE,&clear,IID_PPV_ARGS(&z)));
 auto dsv=dsvHeap->GetCPUDescriptorHandleForHeapStart();device->CreateDepthStencilView(z.Get(),nullptr,dsv);
 // Seed opaque pixels so the test proves paint clears the surrounding area.
 const float bg[]={.5f,.25f,.75f,1};list->ClearRenderTargetView(rtv,bg,0,nullptr);
 pillInventory::Counts state;state.valid=true;state.owned=255;state.loaded={6,4,30,8,0};state.reserve={24,24,100,16,0};state.equipped=1;state.keys=full?0x3fff:0x3;if(empty){state.valid=false;state.owned=0;}
 if(pillInventory::counts().valid)throw std::runtime_error("offline process accepted as native game");
 pillBottle::carried=empty?0:full?2:2;
 // Exercise the separate native-map snapshot resource and transition barriers.
 pillInventory::copyMapFrame(device.Get(),list.Get(),target.Get());
 if(!pillInventory::mapValid)throw std::runtime_error("map copy failed");
 if(!pillInventory::paint(device.Get(),list.Get(),target.Get(),1024,768,state))throw std::runtime_error("inventory paint failed");
 D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition={target.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE};list->ResourceBarrier(1,&barrier);
 D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT64 bytes{};device->GetCopyableFootprints(&color,0,1,0,&footprint,nullptr,nullptr,&bytes);
 auto buffer=color;buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.Width=bytes;buffer.Height=buffer.DepthOrArraySize=buffer.MipLevels=1;buffer.Format=DXGI_FORMAT_UNKNOWN;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;buffer.Flags=D3D12_RESOURCE_FLAG_NONE;heap.Type=D3D12_HEAP_TYPE_READBACK;
 ComPtr<ID3D12Resource> readback;check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback)));
 D3D12_TEXTURE_COPY_LOCATION src{},dst{};src.pResource=target.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;dst.pResource=readback.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=footprint;list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);
 check(list->Close());ID3D12CommandList* commands[]={list.Get()};queue->ExecuteCommandLists(1,commands);
 ComPtr<ID3D12Fence> fence;check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);check(queue->Signal(fence.Get(),1));check(fence->SetEventOnCompletion(1,event));if(WaitForSingleObject(event,30000)!=WAIT_OBJECT_0)throw std::runtime_error("offscreen timeout");CloseHandle(event);
 unsigned char* pixels{};D3D12_RANGE range{0,static_cast<SIZE_T>(bytes)};check(readback->Map(0,&range,reinterpret_cast<void**>(&pixels)));
 const float panelWidth=.70f*(std::min)(1024.f,768.f*1000.f/900.f),panelHeight=panelWidth*900.f/1000.f;
 for(unsigned y=0;y<768;y++)for(unsigned x=0;x<1024;x++){
  if(x+1<(1024-panelWidth)*.5f||x>(1024+panelWidth)*.5f||y+1<(768-panelHeight)*.5f||y>(768+panelHeight)*.5f){
   auto pixel=pixels+footprint.Offset+y*footprint.Footprint.RowPitch+x*4;
   if(pixel[0]||pixel[1]||pixel[2]||pixel[3])throw std::runtime_error("inventory surround must be transparent black");
  }
 }
 auto center=pixels+footprint.Offset+384*footprint.Footprint.RowPitch+512*4;
 if(!page&&(center[0]<120||center[0]>135||center[1]<58||center[1]>70||center[2]<185||center[2]>198))throw std::runtime_error("map snapshot did not survive custom target clear");
 if(center[3]!=255)throw std::runtime_error("inventory panel must remain opaque");
 std::ofstream image(page==1?"../../work/mod-menu-review/settings.rgba":page==2?"../../work/mod-menu-review/controls.rgba":page==3?"../../work/mod-menu-review/reloading.rgba":empty?"../../work/inventory-empty.rgba":full?"../../work/inventory-full.rgba":"../../work/inventory-independent.rgba",std::ios::binary);unsigned changed=0;
 for(unsigned y=0;y<768;y++)for(unsigned x=0;x<1024;x++){auto pixel=pixels+footprint.Offset+y*footprint.Footprint.RowPitch+x*4;image.write(reinterpret_cast<char*>(pixel),4);if(pixel[0]>30||pixel[1]>30||pixel[2]>30)++changed;}
 D3D12_RANGE none{};readback->Unmap(0,&none);if(changed<1000)throw std::runtime_error("replacement render empty");
 if(diagnostics)for(UINT64 i=0;i<diagnostics->GetNumStoredMessages();i++){SIZE_T length{};diagnostics->GetMessage(i,nullptr,&length);std::vector<unsigned char> data(length);auto message=reinterpret_cast<D3D12_MESSAGE*>(data.data());check(diagnostics->GetMessage(i,message,&length));if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR){std::cerr<<message->pDescription<<'\n';return 1;}}
 std::cout<<"PASS: independent inventory input interception and D3D12 WARP render; "<<changed<<" visible pixels; no game or XR session\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}


