#define NOMINMAX
#include "../src/native_fog.hpp"
#include "../src/head_light_constants.hpp"
#include "../src/flashlight_profile_range.hpp"
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <array>
#include <string>
using Microsoft::WRL::ComPtr;
static ComPtr<ID3D12InfoQueue> failureMessages;
static void check(HRESULT h){if(FAILED(h)){
 if(failureMessages)for(UINT64 i=0;i<failureMessages->GetNumStoredMessages();++i){SIZE_T n=0;failureMessages->GetMessage(i,nullptr,&n);std::vector<unsigned char> b(n);auto m=reinterpret_cast<D3D12_MESSAGE*>(b.data());if(SUCCEEDED(failureMessages->GetMessage(i,m,&n)))fprintf(stderr,"%s\n",m->pDescription);}
 throw std::runtime_error("fog GPU check failed "+std::to_string(static_cast<unsigned>(h)));}}
int main(){try{
 std::ifstream file(L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Dementium - The Ward\\rom.zar",std::ios::binary);file.seekg(657365504);std::vector<char> ps(8172);if(!file.read(ps.data(),ps.size()))throw std::runtime_error("native shader unavailable");
 ComPtr<IDxcBlob> patched;if(!nativeFog::patch({ps.data(),ps.size()},patched,L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Dementium - The Ward\\dxcompiler.dll"))throw std::runtime_error("native fog patch rejected");
 float old[16]{},vr[16]{},pixel[112]{};old[10]=1.002044916f;old[11]=.1002044976f;old[14]=-1;std::copy(old,old+16,vr);
 float farPlane=(-2*old[11])/(-2*old[10]-2*old[14]),depth=farPlane/(farPlane-.02f);vr[10]=(depth+1)*.5f;vr[11]=.01f*depth;
 if(!nativeFog::coefficients(pixel,old,vr))throw std::runtime_error("depth mapping rejected");
 unsigned lookupBits=0x3eff01fc;float lookup;std::memcpy(&lookup,&lookupBits,4);float scale=pixel[110]/lookup,bias=pixel[111]/lookup-1;
 for(float distance:{.02f,.1f,.2f,1.f,5.f,25.f,49.f}){
  float nativeZ=(-2*(old[10]*-distance+old[11])-old[14]*-distance)/(old[14]*-distance);
  float vrZ=(-2*(vr[10]*-distance+vr[11])-vr[14]*-distance)/(vr[14]*-distance);
  if(std::fabs((vrZ*pixel[110]+pixel[111])-(nativeZ*lookup+lookup))>2e-6f)throw std::runtime_error("native fog depth mismatch");
 }
 ComPtr<ID3D12Debug> debug;check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));
#ifndef DEMENTIUM_DEBUG_ALREADY_ENABLED
 debug->EnableDebugLayer();
#endif
ComPtr<IDXGIFactory4> factory;check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));ComPtr<IDXGIAdapter> warp;check(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));ComPtr<ID3D12Device> device;check(D3D12CreateDevice(warp.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));ComPtr<ID3D12InfoQueue> messages;check(device.As(&messages));failureMessages=messages;
 D3D12_COMMAND_QUEUE_DESC q{};ComPtr<ID3D12CommandQueue> queue;check(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)));ComPtr<ID3D12CommandAllocator> allocator;check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));ComPtr<ID3D12GraphicsCommandList> list;check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)));
 D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,8,0,0,0};D3D12_ROOT_PARAMETER params[3]{};params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;params[0].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;params[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[1].DescriptorTable={1,&range};params[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;params[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params[2].Constants={1,0,3};params[2].ShaderVisibility=D3D12_SHADER_VISIBILITY_VERTEX;
 D3D12_STATIC_SAMPLER_DESC samplers[8]{};for(unsigned i=0;i<8;i++){auto& s=samplers[i];s.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;s.AddressU=s.AddressV=s.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;s.MaxLOD=D3D12_FLOAT32_MAX;s.ShaderRegister=i;s.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;s.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;}
#ifdef DEMENTIUM_PROFILE_FILTER
 samplers[7].Filter=static_cast<D3D12_FILTER>(DEMENTIUM_PROFILE_FILTER);samplers[7].MaxAnisotropy=samplers[7].Filter==D3D12_FILTER_ANISOTROPIC?8:1;
 printf("profile_sampler_control filter=%x max_anisotropy=%u; runtime descriptor unverified\n",unsigned(samplers[7].Filter),samplers[7].MaxAnisotropy);
#endif
 D3D12_ROOT_SIGNATURE_DESC rd{3,params,8,samplers,D3D12_ROOT_SIGNATURE_FLAG_NONE};ComPtr<ID3DBlob> serialized,error;check(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&serialized,&error));ComPtr<ID3D12RootSignature> root;check(device->CreateRootSignature(0,serialized->GetBufferPointer(),serialized->GetBufferSize(),IID_PPV_ARGS(&root)));
 const char vsSource[]=R"(cbuffer Depth:register(b1){float scale,bias,beam;}struct O{float4 p:SV_POSITION;float2 uv:TEXCOORD0;float2 uv1:TEXCOORD1;float2 uv2:TEXCOORD2;float depth:POSITION2;float3 view:POSITION1;float3 n:NORMAL0;float3 t:TANGENT0;};O vs(uint id:SV_VertexID){O o;float2 p=float2((id<<1)&2,id&2);float z=.85+.149*p.x;o.p=float4(p*float2(2,-2)+float2(-1,1),(z-bias)/scale,1);o.uv=o.uv1=o.uv2=float2(.5,.5);o.depth=z;o.view=beam!=0?float3((p.x-.5)*4,0,1):float3(0,0,1);o.n=float3(0,0,1);o.t=float3(1,0,0);return o;})";
 ComPtr<ID3DBlob> vs;check(D3DCompile(vsSource,sizeof(vsSource)-1,nullptr,nullptr,nullptr,"vs","vs_5_0",0,0,&vs,&error));D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();pd.VS={vs->GetBufferPointer(),vs->GetBufferSize()};pd.PS={ps.data(),ps.size()};pd.SampleMask=UINT_MAX;pd.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;pd.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;pd.RasterizerState.DepthClipEnable=TRUE;pd.BlendState.RenderTarget[0].RenderTargetWriteMask=15;pd.DepthStencilState.DepthEnable=FALSE;pd.DepthStencilState.StencilEnable=FALSE;pd.NumRenderTargets=1;pd.RTVFormats[0]=DXGI_FORMAT_R8G8B8A8_UNORM;pd.SampleDesc.Count=1;pd.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;ComPtr<ID3D12PipelineState> nativePipeline,correctedPipeline;check(device->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&nativePipeline)));pd.PS={patched->GetBufferPointer(),patched->GetBufferSize()};check(device->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&correctedPipeline)));
 auto buffer=[&](UINT64 bytes,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state){D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=bytes;d.Height=1;d.DepthOrArraySize=d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;D3D12_HEAP_PROPERTIES h{};h.Type=type;ComPtr<ID3D12Resource> r;check(device->CreateCommittedResource(&h,D3D12_HEAP_FLAG_NONE,&d,state,nullptr,IID_PPV_ARGS(&r)));return r;};
 auto constants=buffer(512,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);pixel[0]=.15f;pixel[1]=.25f;pixel[2]=.4f;pixel[92]=pixel[93]=pixel[94]=1;pixel[100]=.1f;pixel[101]=.2f;pixel[102]=.3f;pixel[103]=1;reinterpret_cast<unsigned*>(pixel)[96]=1;reinterpret_cast<unsigned*>(pixel)[108]=1;void* mapped{};check(constants->Map(0,nullptr,&mapped));std::memcpy(mapped,pixel,sizeof pixel);constants->Unmap(0,nullptr);
 D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=16;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;ComPtr<ID3D12DescriptorHeap> srv;check(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&srv)));hd={};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=1;ComPtr<ID3D12DescriptorHeap> rtv;check(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtv)));
 std::array<ComPtr<ID3D12Resource>,8> textures,uploads;D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;D3D12_RESOURCE_DESC td{};td.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;td.Width=256;td.Height=1;td.DepthOrArraySize=td.MipLevels=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;
 for(unsigned i=0;i<8;i++){check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&td,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&textures[i])));uploads[i]=buffer(1024,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);check(uploads[i]->Map(0,nullptr,&mapped));auto bytes=static_cast<unsigned char*>(mapped);for(unsigned x=0;x<256;x++){for(unsigned c=0;c<4;c++)bytes[x*4+c]=i==3?static_cast<unsigned char>(255-x):255;}uploads[i]->Unmap(0,nullptr);D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=textures[i].Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=uploads[i].Get();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint.Footprint={DXGI_FORMAT_R8G8B8A8_UNORM,256,1,1,1024};list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition={textures[i].Get(),0,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};list->ResourceBarrier(1,&b);D3D12_SHADER_RESOURCE_VIEW_DESC view{};view.Format=td.Format;view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;view.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;view.Texture2D.MipLevels=1;auto handle=srv->GetCPUDescriptorHandleForHeapStart();handle.ptr+=i*device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);device->CreateShaderResourceView(textures[i].Get(),&view,handle);}
 // Second table keeps the same material/room textures, but deliberately binds
 // an all-white angular LUT at t7. It isolates missing cone attenuation from
 // a disabled spotlight branch without changing any light constants.
 for(unsigned i=0;i<8;++i){auto destinationHandle=srv->GetCPUDescriptorHandleForHeapStart();destinationHandle.ptr+=(i+8)*device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);D3D12_SHADER_RESOURCE_VIEW_DESC view{};view.Format=td.Format;view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;view.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;view.Texture2D.MipLevels=1;device->CreateShaderResourceView(textures[i==7?0:i].Get(),&view,destinationHandle);}
 td.Width=768;td.Height=16;td.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;ComPtr<ID3D12Resource> target;check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&td,D3D12_RESOURCE_STATE_RENDER_TARGET,nullptr,IID_PPV_ARGS(&target)));auto targetHandle=rtv->GetCPUDescriptorHandleForHeapStart();device->CreateRenderTargetView(target.Get(),nullptr,targetHandle);list->OMSetRenderTargets(1,&targetHandle,FALSE,nullptr);list->SetGraphicsRootSignature(root.Get());ID3D12DescriptorHeap* heaps[]={srv.Get()};list->SetDescriptorHeaps(1,heaps);list->SetGraphicsRootConstantBufferView(0,constants->GetGPUVirtualAddress());list->SetGraphicsRootDescriptorTable(1,srv->GetGPUDescriptorHandleForHeapStart());list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
 for(unsigned pass=0;pass<3;pass++){list->SetPipelineState(pass==2?correctedPipeline.Get():nativePipeline.Get());float mapping[]={pass?scale:1.f,pass?bias:0.f,0};list->SetGraphicsRoot32BitConstants(2,3,mapping,0);D3D12_VIEWPORT viewport{float(pass*256),0,256,16,0,1};D3D12_RECT scissor{LONG(pass*256),0,LONG((pass+1)*256),16};list->RSSetViewports(1,&viewport);list->RSSetScissorRects(1,&scissor);list->DrawInstanced(3,1,0,0);}
 D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition={target.Get(),0,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE};list->ResourceBarrier(1,&barrier);auto readback=buffer(3072*16,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=readback.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint.Footprint={DXGI_FORMAT_R8G8B8A8_UNORM,768,16,1,3072};D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=target.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);check(list->Close());ID3D12CommandList* lists[]={list.Get()};queue->ExecuteCommandLists(1,lists);ComPtr<ID3D12Fence> fence;check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));check(queue->Signal(fence.Get(),1));HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);check(fence->SetEventOnCompletion(1,event));if(WaitForSingleObject(event,10000)!=WAIT_OBJECT_0)throw std::runtime_error("GPU timeout");CloseHandle(event);
 check(readback->Map(0,nullptr,&mapped));auto bytes=static_cast<unsigned char*>(mapped);unsigned correctedError=0,hazeError=0;std::ofstream image("../../work/native-fog-comparison.ppm",std::ios::binary);image<<"P6\n768 16\n255\n";for(unsigned y=0;y<16;y++)for(unsigned x=0;x<768;x++){image.write(reinterpret_cast<char*>(bytes+y*3072+x*4),3);if(x<256)for(unsigned c=0;c<3;c++){int original=bytes[y*3072+x*4+c];correctedError=std::max(correctedError,unsigned(std::abs(original-int(bytes[y*3072+(x+512)*4+c]))));hazeError=std::max(hazeError,unsigned(std::abs(original-int(bytes[y*3072+(x+256)*4+c]))));}}readback->Unmap(0,nullptr);
 // Isolate the native spotlight branch with a known angular LUT. This is a
 // synthetic wall, not captured hallway data. Compare fog-patched/native PS,
 // lamp-off with room illumination retained, disabled cone, and flat angular LUT.
 check(allocator->Reset());check(list->Reset(allocator.Get(),nativePipeline.Get()));
#ifdef DEMENTIUM_NATIVE_SCALAR_PROFILE
 D3D12_RESOURCE_DESC scalar{};scalar.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;scalar.Width=257;scalar.Height=1;scalar.DepthOrArraySize=scalar.MipLevels=1;scalar.Format=DXGI_FORMAT_R32_FLOAT;scalar.SampleDesc.Count=1;
 check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&scalar,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,nullptr,IID_PPV_ARGS(&textures[7])));uploads[7]=buffer(1280,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
 D3D12_SHADER_RESOURCE_VIEW_DESC scalarView{};scalarView.Format=scalar.Format;scalarView.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;scalarView.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;scalarView.Texture2D.MipLevels=1;auto scalarHandle=srv->GetCPUDescriptorHandleForHeapStart();scalarHandle.ptr+=7*device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);device->CreateShaderResourceView(textures[7].Get(),&scalarView,scalarHandle);
#endif
 check(uploads[7]->Map(0,nullptr,&mapped));
#ifndef DEMENTIUM_NATIVE_SCALAR_PROFILE
 auto spotLUT=static_cast<unsigned char*>(mapped);
#endif
 #ifdef DEMENTIUM_NATIVE_FLASHLIGHT_PROFILE
 std::array<unsigned char,256> profile{};file.clear();file.seekg(73267712);if(!file.read(reinterpret_cast<char*>(profile.data()),profile.size()))throw std::runtime_error("native flashlight profile unavailable");
 for(unsigned x=0;x<228;++x)if(profile[x])throw std::runtime_error("native flashlight profile prefix changed");
 const unsigned char tail[]={7,12,18,25,46,37,37,37,37,37,74,100,92,100,120,130,152,174,189,205,217,229,240,246,254,229,254,229};
 if(!std::equal(std::begin(tail),std::end(tail),profile.begin()+228))throw std::runtime_error("native flashlight profile tail changed");
#ifdef DEMENTIUM_NATIVE_SCALAR_PROFILE
 // Proven storage shape; controlled low=0/high=1, not captured hallway parameters.
 auto scalarSamples=static_cast<float*>(mapped);for(unsigned x=0;x<256;++x){
#ifdef DEMENTIUM_PROFILE_HIGH_FLOOR
     scalarSamples[x]=.95f+(1.f-.95f)*(profile[x]/255.f);
#else
     scalarSamples[x]=profile[x]/255.f;
#endif
 }scalarSamples[256]=scalarSamples[255];
#ifdef DEMENTIUM_PROFILE_NORMALIZE_TRIAL
 std::array<float,257> normalized{};
 if(!flashlightProfileRange::normalizeObservedFloor(scalarSamples,normalized))throw std::runtime_error("observed floor rejected");
 std::copy(normalized.begin(),normalized.end(),scalarSamples);
 puts("normalized high-floor trial: original angular shape restored; offline GPU test only");
#endif
#ifdef DEMENTIUM_PROFILE_HIGH_FLOOR
 puts("profile_range_control: low=.95/high=1 reproduces one static native branch; runtime branch selection unverified");
#endif
 puts("native_scalar_profile:257x1 R32_FLOAT, repeated endpoint, controlled range; runtime SRV/sampler/hallway unverified");
#else
 for(unsigned x=0;x<256;++x)for(unsigned c=0;c<4;++c)spotLUT[x*4+c]=profile[x];
#endif
 puts("native_flashlight_profile: exact256 archive bytes used as synthetic fixture angular LUT; actual hallway binding unverified");
#else
 for(unsigned x=0;x<256;++x)for(unsigned c=0;c<4;++c)spotLUT[x*4+c]=x>=230?255:0;
#endif
 uploads[7]->Unmap(0,nullptr);
 barrier.Transition={textures[7].Get(),0,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_DEST};list->ResourceBarrier(1,&barrier);
 D3D12_TEXTURE_COPY_LOCATION lutSource{},lutDestination{};lutSource.pResource=uploads[7].Get();lutSource.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;lutSource.PlacedFootprint.Footprint={DXGI_FORMAT_R8G8B8A8_UNORM,256,1,1,1024};lutDestination.pResource=textures[7].Get();lutDestination.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
#ifdef DEMENTIUM_NATIVE_SCALAR_PROFILE
 lutSource.PlacedFootprint.Footprint={DXGI_FORMAT_R32_FLOAT,257,1,1,1280};
#endif
 list->CopyTextureRegion(&lutDestination,0,0,0,&lutSource,nullptr);
 std::swap(barrier.Transition.StateBefore,barrier.Transition.StateAfter);list->ResourceBarrier(1,&barrier);
 td.Width=1280;td.Height=16;td.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;ComPtr<ID3D12Resource> spotTarget;check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&td,D3D12_RESOURCE_STATE_RENDER_TARGET,nullptr,IID_PPV_ARGS(&spotTarget)));device->CreateRenderTargetView(spotTarget.Get(),nullptr,targetHandle);
 std::array<ComPtr<ID3D12Resource>,5> lampConstants;
 float lamp[112]{};lamp[0]=lamp[1]=lamp[2]=.05f;lamp[7]=lamp[11]=1;lamp[14]=-1;
 lamp[28]=lamp[29]=lamp[30]=.7f;lamp[32]=lamp[33]=lamp[34]=.1f;lamp[52]=lamp[56]=lamp[60]=1;lamp[103]=1;reinterpret_cast<unsigned*>(lamp)[108]=1;
#ifdef DEMENTIUM_FOG_CONE_TEST
 // Same native wall and spotlight, now behind dense BLACK fog. The lamp must
 // reveal its cone while off-cone pixels retain the original fog exactly.
 reinterpret_cast<unsigned*>(lamp)[96]=1;lamp[110]=lookup;lamp[111]=lookup;
#endif
 XrPosef head{{0,0,0,1},{0,0,0}};
 for(unsigned pass=0;pass<5;++pass){
     float settings[112];std::copy(lamp,lamp+112,settings);
     headLightConstants::enabled=pass!=2;
     if(!headLightConstants::center(settings,head,head,1))throw std::runtime_error("synthetic native spotlight not recognized");
     if(pass==3)settings[60]=0; // Positive control: native omnidirectional branch.
     if(settings[56]!=1||settings[32]!=.1f)throw std::runtime_error("lamp update modified room illumination");
     lampConstants[pass]=buffer(512,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);check(lampConstants[pass]->Map(0,nullptr,&mapped));std::memcpy(mapped,settings,sizeof settings);lampConstants[pass]->Unmap(0,nullptr);
 }
 headLightConstants::enabled=true;
 list->OMSetRenderTargets(1,&targetHandle,FALSE,nullptr);list->SetGraphicsRootSignature(root.Get());list->SetDescriptorHeaps(1,heaps);list->SetGraphicsRootDescriptorTable(1,srv->GetGPUDescriptorHandleForHeapStart());list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
 for(unsigned pass=0;pass<5;++pass){list->SetPipelineState(pass?correctedPipeline.Get():nativePipeline.Get());list->SetGraphicsRootConstantBufferView(0,lampConstants[pass]->GetGPUVirtualAddress());auto table=srv->GetGPUDescriptorHandleForHeapStart();if(pass==4)table.ptr+=8*device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);list->SetGraphicsRootDescriptorTable(1,table);float mapping[]={1,0,1};list->SetGraphicsRoot32BitConstants(2,3,mapping,0);D3D12_VIEWPORT viewport{float(pass*256),0,256,16,0,1};D3D12_RECT scissor{LONG(pass*256),0,LONG((pass+1)*256),16};list->RSSetViewports(1,&viewport);list->RSSetScissorRects(1,&scissor);list->DrawInstanced(3,1,0,0);}
 barrier.Transition={spotTarget.Get(),0,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE};list->ResourceBarrier(1,&barrier);auto spotReadback=buffer(5120*16,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);dst.pResource=spotReadback.Get();dst.PlacedFootprint.Footprint={DXGI_FORMAT_R8G8B8A8_UNORM,1280,16,1,5120};src.pResource=spotTarget.Get();list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);check(list->Close());queue->ExecuteCommandLists(1,lists);check(queue->Signal(fence.Get(),2));event=CreateEventW(nullptr,FALSE,FALSE,nullptr);check(fence->SetEventOnCompletion(2,event));if(WaitForSingleObject(event,10000)!=WAIT_OBJECT_0)throw std::runtime_error("spotlight GPU timeout");CloseHandle(event);
 check(spotReadback->Map(0,nullptr,&mapped));bytes=static_cast<unsigned char*>(mapped);unsigned spotError=0,flatLutError=0;
 for(unsigned y=0;y<16;++y)for(unsigned x=0;x<256;++x)for(unsigned c=0;c<4;++c)spotError=std::max(spotError,unsigned(std::abs(int(bytes[y*5120+x*4+c])-int(bytes[y*5120+(x+256)*4+c]))));
 for(unsigned y=0;y<16;++y)for(unsigned x=0;x<256;++x)for(unsigned c=0;c<4;++c)flatLutError=std::max(flatLutError,unsigned(std::abs(int(bytes[y*5120+(x+768)*4+c])-int(bytes[y*5120+(x+1024)*4+c]))));
 auto red=[&](unsigned pass,unsigned x){return int(bytes[8*5120+(pass*256+x)*4]);};
 const int centerGain=red(1,128)-red(2,128),edgeGain=red(1,16)-red(2,16),broadEdgeGain=red(3,16)-red(2,16),roomEdge=red(2,16);
 const int nativeEdge=red(0,16),patchedEdge=red(1,16),nativeCenter=red(0,128),patchedCenter=red(1,128);
 std::ofstream spotImage("../../work/native-spotlight-regression.ppm",std::ios::binary);spotImage<<"P6\n1280 16\n255\n";for(unsigned y=0;y<16;++y)for(unsigned x=0;x<1280;++x)spotImage.write(reinterpret_cast<char*>(bytes+y*5120+x*4),3);
 spotReadback->Unmap(0,nullptr);
 printf("native_spotlight patched_error=%u center_gain=%d edge_gain=%d disabled_cone_edge_gain=%d room_edge=%d flat_lut_vs_disabled_error=%u\n",spotError,centerGain,edgeGain,broadEdgeGain,roomEdge,flatLutError);
 const bool edgeMatches=
#if defined(DEMENTIUM_PROFILE_HIGH_FLOOR) && !defined(DEMENTIUM_PROFILE_NORMALIZE_TRIAL)
     edgeGain>20;
#else
     std::abs(edgeGain)<=2;
#endif
#ifdef DEMENTIUM_FOG_CONE_TEST
 printf("black_fog native_center=%d lit_center=%d native_edge=%d patched_edge=%d lamp_off_edge=%d\n",nativeCenter,patchedCenter,nativeEdge,patchedEdge,roomEdge);
 if(patchedCenter-nativeCenter<80||std::abs(nativeEdge-patchedEdge)>1||centerGain<80||std::abs(edgeGain)>1||roomEdge>5)throw std::runtime_error("black fog cone separation failed");
#else
 if(flatLutError>1||spotError>1||centerGain<80||!edgeMatches||broadEdgeGain<20||roomEdge<10)throw std::runtime_error("native spotlight cone or room-light preservation failed");
#endif
 for(UINT64 i=0;i<messages->GetNumStoredMessages();i++){SIZE_T n{};messages->GetMessage(i,nullptr,&n);std::vector<char> storage(n);auto message=reinterpret_cast<D3D12_MESSAGE*>(storage.data());messages->GetMessage(i,message,&n);if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(message->pDescription);}
 printf("fog corrected_max_error=%u uncorrected_error=%u\n",correctedError,hazeError);if(correctedError>1||hazeError<5)throw std::runtime_error("fog equivalence failed");puts("PASS: actual native PS, WARP rendered native fog preserved with VR near depth; no D3D12 errors.");return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}










