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
int main(int argc,char** argv){try{
 const bool wristOnly=argc==5&&!std::strcmp(argv[1],"--wrist");
 const bool live=argc==5&&!std::strcmp(argv[1],"--live");
 const bool selected=argc==5&&(!std::strcmp(argv[1],"--selected")||wristOnly||live);
 const int selectedWeapon=selected?std::atoi(argv[3]):-1;
 const int selectedAngle=selected?std::atoi(argv[4]):0;
 if(selected&&(selectedWeapon<0||selectedWeapon>6||selectedAngle<0||selectedAngle>3))return 2;
 const bool supportBank=argc==4&&!std::strcmp(argv[1],"--support-bank");
 const int supportWeapon=supportBank?std::atoi(argv[3]):-1;
 bool original=supportBank||(argc==2&&!std::strcmp(argv[1],"--original"));
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
 if(selected){if(!wristband::loadSelected(argv[2]))throw std::runtime_error("selected asset load failed");}
 else if(!replacementHands::load(supportBank?argv[2]:original?"assets/hands/NativeHands.bin":"assets/hands/FloatingHands.bin")||!huntingRifle::load("assets/hunting-rifle/HuntingRifle.bin")||!pillBottle::load("assets/hands/PillBottle.mesh"))throw std::runtime_error("asset load failed");
 if(original&&!weaponGripHands::load(L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Dementium - The Ward\\rom.zar"))throw std::runtime_error("native hand asset load failed");
 auto now=GetTickCount64();manualReload::Hands h;h.session=h.gameplay=h.tracked=h.gunTracked=h.leftPoseTracked=h.rightPoseTracked=true;h.weapon=4;h.time=now;h.gun.position={.16f,-.10f,-.45f};h.rightGrip=h.gun;h.left.position={-.15f,-.12f,-.45f};
 if(supportBank){
     if(!replacementHands::measuredSupport(supportWeapon))throw std::runtime_error("unsupported measured grip fixture");
     h.weapon=supportWeapon;h.grip=true;const auto contact=handFitting::support(supportWeapon,true);
     h.left.position={h.gun.position.x+contact.x,h.gun.position.y+contact.y,h.gun.position.z+contact.z};
     supportAim::publish(supportWeapon,true);
 }
 manualReload::publish(h);wristband::Snapshot status;status.world=true;status.time=now;status.healthValid=true;status.health=7;status.maximum=10;status.ammoValid=true;status.weapon=4;status.loaded=3;status.reserve=8;wristband::model.publish(status);wristband::settings.always=true;
 if(original){auto& capture=weaponGripHands::captures[2];capture.body={{.4958496f,-.4958496f,.5041504f,.5041504f},arsenal::profiles[3].anchor};capture.time=now;capture.valid=true;}
 if(live){h.weapon=selectedWeapon;h.gun.orientation.w=h.left.orientation.w=h.head.orientation.w=1;
     h.grip=selectedAngle>=2;supportAim::publish(h.weapon,h.grip);manualReload::publish(h);}
 pillBottle::holding=!original;wristband::prepareFrame();
 if(selected&&!live){
     using namespace DirectX;
     // Static asset views must not retain the live controller draw list.
     wristband::selectedDraws.clear();wristband::selectedWristVisible=false;
     XMFLOAT3 minimum{100,100,100},maximum{-100,-100,-100};
     for(const auto& section:wristband::selectedPack.sections)if(section.weapon==unsigned(selectedWeapon))
         for(unsigned i=section.first;i<section.first+section.count;i++){
             const auto* v=wristband::selectedPack.vertices[i].value;
             minimum.x=(std::min)(minimum.x,v[0]);minimum.y=(std::min)(minimum.y,v[1]);minimum.z=(std::min)(minimum.z,v[2]);
             maximum.x=(std::max)(maximum.x,v[0]);maximum.y=(std::max)(maximum.y,v[1]);maximum.z=(std::max)(maximum.z,v[2]);
         }
     const float extent=(std::max)({maximum.x-minimum.x,maximum.y-minimum.y,maximum.z-minimum.z});
     const auto turn=selectedAngle<2?XMMatrixRotationY(selectedAngle*XM_PI):XMMatrixRotationX((selectedAngle==2?1.f:-1.f)*XM_PIDIV2);
     const auto matrix=XMMatrixTranslation(-(minimum.x+maximum.x)/2,-(minimum.y+maximum.y)/2,-(minimum.z+maximum.z)/2)*turn*XMMatrixTranslation(0,0,-extent*1.5f);
     for(unsigned i=0;i<wristband::selectedPack.sections.size();i++)if(wristband::selectedPack.sections[i].weapon==unsigned(selectedWeapon))wristband::selectedDraws.push_back({i,matrix});
     if(selectedWeapon>=2&&selectedWeapon<=5){
         const char* names[]={"m16","shotgun","hunting-rifle","buzzsaw"};
         std::ifstream input(std::filesystem::path(argv[2])/(std::string(names[selectedWeapon-2])+"-wrist-frame.bin"),std::ios::binary);
         XMFLOAT4X4 frame{};if(!input.read(reinterpret_cast<char*>(&frame),sizeof frame))throw std::runtime_error("missing saved wrist frame");
         wristband::haveDisplay=false;wristband::prepareSelectedStatus(status,XMLoadFloat4x4(&frame)*matrix,true);
     }
 }
 if(wristOnly){
     using namespace DirectX;
     // View the actual saved wrist in its neutral frame, independently of any
     // controller calibration. Angles 0/1 are a simulated stereo eye pair.
     wristband::selectedDraws.clear();
     status.health=selectedWeapon==0?10:selectedWeapon==1?6:3;
     wristband::haveDisplay=false;
     const auto turn=XMMatrixRotationY(selectedAngle<2?0.f:XM_PI);
     wristband::prepareSelectedStatus(status,turn*XMMatrixTranslation(0,0,-.15f),true);
 }
 if(supportBank&&(wristband::replacementGrip.left!=handGrip::Pose::Support||wristband::gripTransition[0].to!=replacementHands::supportSection(supportWeapon)))throw std::runtime_error("measured support render path not selected");
 if(original&&(!replacementHands::nativeSource||!wristband::replacementVisible[0]||!wristband::replacementVisible[1]||wristband::forearmVisible[0]))throw std::runtime_error("native hand selection failed");
 const float bg[]={.025f,.028f,.035f,1};list->ClearRenderTargetView(rtv,bg,0,nullptr);list->ClearDepthStencilView(dsv,D3D12_CLEAR_FLAG_DEPTH,1,0,0,nullptr);
 XrView eye{XR_TYPE_VIEW};eye.pose.orientation.w=1;eye.fov={-.7f,.7f,.6f,-.6f};
 if(wristOnly||live)eye.pose.position.x=selectedAngle%2? .032f:-.032f;
 wristband::draw(device.Get(),list.Get(),1,rtv,dsv,eye,1024,768);
 // Independent inventory has its own dedicated offscreen renderer test.
 D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition={target.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE};list->ResourceBarrier(1,&barrier);
 D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT64 bytes{};device->GetCopyableFootprints(&color,0,1,0,&footprint,nullptr,nullptr,&bytes);
 auto buffer=color;buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.Width=bytes;buffer.Height=buffer.DepthOrArraySize=buffer.MipLevels=1;buffer.Format=DXGI_FORMAT_UNKNOWN;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;buffer.Flags=D3D12_RESOURCE_FLAG_NONE;heap.Type=D3D12_HEAP_TYPE_READBACK;
 ComPtr<ID3D12Resource> readback;check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback)));
 D3D12_TEXTURE_COPY_LOCATION src{},dst{};src.pResource=target.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;dst.pResource=readback.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=footprint;list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);
 check(list->Close());ID3D12CommandList* commands[]={list.Get()};queue->ExecuteCommandLists(1,commands);
 ComPtr<ID3D12Fence> fence;check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);check(queue->Signal(fence.Get(),1));check(fence->SetEventOnCompletion(1,event));if(WaitForSingleObject(event,30000)!=WAIT_OBJECT_0)throw std::runtime_error("offscreen timeout");CloseHandle(event);
 unsigned char* pixels{};D3D12_RANGE range{0,static_cast<SIZE_T>(bytes)};check(readback->Map(0,&range,reinterpret_cast<void**>(&pixels)));
 const auto output=selected?std::string("../../work/psx-runtime-staging/")+(live?"live-":wristOnly?"wrist-":"render-")+std::to_string(selectedWeapon)+"-"+std::to_string(selectedAngle)+".ppm":supportBank?std::string("../../work/measured-support-render-")+std::to_string(supportWeapon)+".ppm":original?"../../work/original-hands-render.ppm":"../../work/replacement-render.ppm";
 std::ofstream image(output,std::ios::binary);
 if(!image)throw std::runtime_error("cannot create render evidence: "+output);
 image<<"P6\n1024 768\n255\n";unsigned changed=0;
 for(unsigned y=0;y<768;y++)for(unsigned x=0;x<1024;x++){auto p=pixels+footprint.Offset+y*footprint.Footprint.RowPitch+x*4;image.write(reinterpret_cast<char*>(p),3);if(p[0]>20||p[1]>20||p[2]>20)++changed;}
 D3D12_RANGE none{};readback->Unmap(0,&none);
 image.close();if(!image)throw std::runtime_error("cannot finish render evidence: "+output);
 if(changed<1000)throw std::runtime_error("replacement render empty");
 if(diagnostics)for(UINT64 i=0;i<diagnostics->GetNumStoredMessages();i++){SIZE_T length{};diagnostics->GetMessage(i,nullptr,&length);std::vector<unsigned char> data(length);auto message=reinterpret_cast<D3D12_MESSAGE*>(data.data());check(diagnostics->GetMessage(i,message,&length));if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR){std::cerr<<message->pDescription<<'\n';return 1;}}
 std::cout<<"PASS: "<<(selected?"selected PSX weapon and exact saved hands":"legacy replacement scene")<<" rendered on D3D12 WARP; "<<changed<<" visible pixels; no game or XR session\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
