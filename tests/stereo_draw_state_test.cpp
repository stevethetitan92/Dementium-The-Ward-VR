#define NOMINMAX
#ifdef DEMENTIUM_INSTALLED_DRAW_TEST
#include "../src/bridge.cpp"
#include "../src/native_copy_forwarders.hpp"
#include "../src/native_copy_session.hpp"
#ifdef DEMENTIUM_ALL_COMMAND_HOOKS
#include "../src/native_copy_minhook.hpp"
#endif
#else
#include "../src/stereo.hpp"
#endif
#include "../src/sphere_frustum.hpp"
#include <d3dcompiler.h>
#include <d3d12sdklayers.h>
#include <cstdio>
#include <array>
#include <fstream>
using Microsoft::WRL::ComPtr;
using namespace DirectX;
static void check(HRESULT result){if(FAILED(result))throw std::runtime_error("stereo draw GPU operation failed: "+std::to_string(static_cast<unsigned>(result)));}
#ifdef DEMENTIUM_INSTALLED_DRAW_TEST
static ID3D12GraphicsCommandList* installedOwner;static unsigned installedOwned{},installedNested{};static bool installedNestedInternal=true;
static nativeCopySession::Session* installedSession;static std::vector<unsigned> installedPrefix;static unsigned installedDispatches{},installedBypasses{},installedOtherOwned{};
static void STDMETHODCALLTYPE stereoDrawProbe(ID3D12GraphicsCommandList* list,UINT a,UINT b,UINT c,UINT d){if(list==installedOwner)++installedOwned;else{++installedNested;installedNestedInternal&=internal;}drawHook(list,a,b,c,d);}
struct InstalledDraw {void* target{};Draw previous{},adapterPrevious=nativeCopyForwarders::Adapter<&ID3D12GraphicsCommandList::DrawInstanced>::original;nativeCopyForwarders::Dispatch previousDispatch=nativeCopyForwarders::dispatch;bool initialized{};~InstalledDraw(){if(initialized){MH_DisableHook(target);MH_RemoveHook(target);MH_Uninitialize();originalDraw=previous;}nativeCopyForwarders::Adapter<&ID3D12GraphicsCommandList::DrawInstanced>::original=adapterPrevious;nativeCopyForwarders::dispatch=previousDispatch;}};
#ifdef DEMENTIUM_ALL_COMMAND_HOOKS
struct InstalledCommands {
    nativeCopyInstall::Batch<nativeCopyMinHook::Backend,51> batch{nativeCopyMinHook::Backend{}};bool initialized{},configured{};std::array<void*,47> previousAdapters{};std::array<void*,51> targets{};std::array<std::array<unsigned char,32>,51> bytes;
    nativeCopyForwarders::Dispatch previousDispatch=nativeCopyForwarders::dispatch;nativeCopyEndpoints::Close previousClose=nativeCopyEndpoints::originalClose;nativeCopyEndpoints::Reset previousReset=nativeCopyEndpoints::originalReset;nativeCopyEndpoints::Copy previousCopy=nativeCopyEndpoints::originalCopy;nativeCopyEndpoints::Barrier previousBarrier=nativeCopyEndpoints::originalBarrier;
    void install(ID3D12GraphicsCommandList* list){if(MH_Initialize()!=MH_OK)throw std::runtime_error("all-command initialize failed");initialized=true;auto adapters=nativeCopyForwarders::entries();for(size_t i=0;i<47;++i)previousAdapters[i]=*adapters[i].original;configured=true;auto bindings=candidateCommandBindings(list);for(size_t i=0;i<51;++i){targets[i]=bindings[i].target;std::memcpy(bytes[i].data(),targets[i],32);}nativeCopyForwarders::Adapter<&ID3D12GraphicsCommandList::DrawInstanced>::original=&stereoDrawProbe;if(!batch.install(bindings))throw std::runtime_error("all-command installation failed");}
    bool stop(){if(!initialized)return true;if(!batch.rollback())return false;bool restored=true;for(size_t i=0;i<51;++i)restored&=!std::memcmp(bytes[i].data(),targets[i],32);if(configured){auto adapters=nativeCopyForwarders::entries();for(size_t i=0;i<47;++i)*adapters[i].original=previousAdapters[i];nativeCopyEndpoints::originalClose=previousClose;nativeCopyEndpoints::originalReset=previousReset;nativeCopyEndpoints::originalCopy=previousCopy;nativeCopyEndpoints::originalBarrier=previousBarrier;nativeCopyForwarders::dispatch=previousDispatch;configured=false;}restored&=MH_Uninitialize()==MH_OK;initialized=false;return restored;}
    ~InstalledCommands(){if(!stop())std::terminate();}
};
#endif
#endif
int main(){try{
    ComPtr<ID3D12Debug> debug;check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();
    ComPtr<IDXGIFactory4> factory;check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));ComPtr<IDXGIAdapter> warp;check(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));
    ComPtr<ID3D12Device> device;check(D3D12CreateDevice(warp.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));ComPtr<ID3D12InfoQueue> messages;check(device.As(&messages));
    D3D12_COMMAND_QUEUE_DESC q{};ComPtr<ID3D12CommandQueue> queue;check(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)));
    ComPtr<ID3D12CommandAllocator> allocator;check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
    ComPtr<ID3D12GraphicsCommandList> list;check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)));
    D3D12_ROOT_PARAMETER parameter{};parameter.ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;parameter.ShaderVisibility=D3D12_SHADER_VISIBILITY_VERTEX;
    D3D12_ROOT_SIGNATURE_DESC rd{1,&parameter,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
#ifdef DEMENTIUM_DESCRIPTOR_SAMPLE
    D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0};
    D3D12_ROOT_PARAMETER parameters[3]={parameter,{},{}};parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[1].DescriptorTable={1,&range};parameters[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;rd.NumParameters=2;rd.pParameters=parameters;
#ifdef DEMENTIUM_TEXTURE_SAMPLE
    D3D12_DESCRIPTOR_RANGE samplerRange{D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,1,0,0,0};parameters[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[2].DescriptorTable={1,&samplerRange};parameters[2].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;rd.NumParameters=3;
#endif
#endif
    ComPtr<ID3DBlob> serialized,error;
    check(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&serialized,&error));ComPtr<ID3D12RootSignature> root;
    check(device->CreateRootSignature(0,serialized->GetBufferPointer(),serialized->GetBufferSize(),IID_PPV_ARGS(&root)));
    const char source[]=R"(
cbuffer Native:register(b0){float4 data[8];}
struct V{float4 p:SV_POSITION;float4 color:COLOR;};
V vs(uint id:SV_VertexID){V o;float2 p=float2((id<<1)&2,id&2);o.p=float4(p*float2(2,-2)+float2(-1,1),.5,1);o.color=data[7];return o;}
#ifdef TEXTURE_SAMPLE
Texture2D<float4> sampled:register(t0);SamplerState filtered:register(s0);
float4 ps(V input):SV_TARGET{return input.color*sampled.SampleLevel(filtered,float2(1.5,1.5),0);}
#elif defined(DESCRIPTOR_SAMPLE)
Buffer<float4> sampled:register(t0);
float4 ps(V input):SV_TARGET{return input.color*sampled[0];}
#else
float4 ps(V input):SV_TARGET{return input.color;}
#endif
)";
    const D3D_SHADER_MACRO* macros=nullptr;
#ifdef DEMENTIUM_DESCRIPTOR_SAMPLE
    const D3D_SHADER_MACRO descriptorMacros[]={{"DESCRIPTOR_SAMPLE","1"},
#ifdef DEMENTIUM_TEXTURE_SAMPLE
    {"TEXTURE_SAMPLE","1"},
#endif
    {nullptr,nullptr}};macros=descriptorMacros;
#endif
    ComPtr<ID3DBlob> vs,ps;check(D3DCompile(source,sizeof(source)-1,nullptr,macros,nullptr,"vs","vs_5_0",0,0,&vs,&error));check(D3DCompile(source,sizeof(source)-1,nullptr,macros,nullptr,"ps","ps_5_0",0,0,&ps,&error));
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();pd.VS={vs->GetBufferPointer(),vs->GetBufferSize()};pd.PS={ps->GetBufferPointer(),ps->GetBufferSize()};pd.SampleMask=UINT_MAX;
    pd.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;pd.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;pd.RasterizerState.DepthClipEnable=TRUE;
    pd.BlendState.RenderTarget[0].RenderTargetWriteMask=15;pd.DepthStencilState.DepthEnable=TRUE;pd.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ALL;pd.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_LESS_EQUAL;
    pd.NumRenderTargets=1;pd.RTVFormats[0]=DXGI_FORMAT_R8G8B8A8_UNORM;pd.DSVFormat=DXGI_FORMAT_D24_UNORM_S8_UINT;pd.SampleDesc.Count=stereo::sceneSamples;pd.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    ComPtr<ID3D12PipelineState> pipeline;check(device->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&pipeline)));
    auto buffer=[&](UINT64 bytes,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state){D3D12_HEAP_PROPERTIES h{};h.Type=type;D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=bytes;d.Height=1;d.DepthOrArraySize=d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;ComPtr<ID3D12Resource> r;check(device->CreateCommittedResource(&h,D3D12_HEAP_FLAG_NONE,&d,state,nullptr,IID_PPV_ARGS(&r)));return r;};
    auto native=buffer(1280,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);auto clones=buffer(stereo::capacity,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
#ifdef DEMENTIUM_DESCRIPTOR_SAMPLE
    auto sampled=buffer(256,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);void* sampleCPU{};check(sampled->Map(0,nullptr,&sampleCPU));const float white[]={1,1,1,1};std::memcpy(sampleCPU,white,sizeof white);sampled->Unmap(0,nullptr);
    D3D12_DESCRIPTOR_HEAP_DESC sampleHeapDescription{};sampleHeapDescription.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;sampleHeapDescription.NumDescriptors=2;sampleHeapDescription.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;ComPtr<ID3D12DescriptorHeap> sampleHeap;check(device->CreateDescriptorHeap(&sampleHeapDescription,IID_PPV_ARGS(&sampleHeap)));
    auto sampleCPUHandle=sampleHeap->GetCPUDescriptorHandleForHeapStart();sampleCPUHandle.ptr+=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);auto sampleGPUHandle=sampleHeap->GetGPUDescriptorHandleForHeapStart();sampleGPUHandle.ptr+=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_SHADER_RESOURCE_VIEW_DESC sampleView{};sampleView.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;sampleView.ViewDimension=D3D12_SRV_DIMENSION_BUFFER;sampleView.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;sampleView.Buffer.NumElements=1;
#ifdef DEMENTIUM_TEXTURE_SAMPLE
    D3D12_RESOURCE_DESC textureDescription{};textureDescription.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;textureDescription.Width=textureDescription.Height=1;textureDescription.DepthOrArraySize=textureDescription.MipLevels=1;textureDescription.SampleDesc.Count=1;textureDescription.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;D3D12_HEAP_PROPERTIES textureHeap{};textureHeap.Type=D3D12_HEAP_TYPE_DEFAULT;ComPtr<ID3D12Resource> sampledTexture;check(device->CreateCommittedResource(&textureHeap,D3D12_HEAP_FLAG_NONE,&textureDescription,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&sampledTexture)));
    D3D12_TEXTURE_COPY_LOCATION sampleSource{},sampleDestination{};sampleSource.pResource=sampled.Get();sampleSource.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;sampleSource.PlacedFootprint.Footprint={textureDescription.Format,1,1,1,256};sampleDestination.pResource=sampledTexture.Get();sampleDestination.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;list->CopyTextureRegion(&sampleDestination,0,0,0,&sampleSource,nullptr);auto sampleBarrier=stereo::transition(sampledTexture.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);list->ResourceBarrier(1,&sampleBarrier);
    sampleView={};sampleView.Format=textureDescription.Format;sampleView.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;sampleView.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;sampleView.Texture2D.MipLevels=1;
    sampleHeapDescription.Type=D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;ComPtr<ID3D12DescriptorHeap> samplerHeap;check(device->CreateDescriptorHeap(&sampleHeapDescription,IID_PPV_ARGS(&samplerHeap)));D3D12_SAMPLER_DESC sampler{};sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_POINT;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;sampler.MaxLOD=D3D12_FLOAT32_MAX;sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_NEVER;auto samplerCPUHandle=samplerHeap->GetCPUDescriptorHandleForHeapStart();samplerCPUHandle.ptr+=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);auto samplerGPUHandle=samplerHeap->GetGPUDescriptorHandleForHeapStart();samplerGPUHandle.ptr+=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);device->CreateSampler(&sampler,samplerCPUHandle);sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_BORDER;device->CreateSampler(&sampler,samplerHeap->GetCPUDescriptorHandleForHeapStart());
    device->CreateShaderResourceView(nullptr,&sampleView,sampleHeap->GetCPUDescriptorHandleForHeapStart());device->CreateShaderResourceView(sampledTexture.Get(),&sampleView,sampleCPUHandle);
#else
    device->CreateShaderResourceView(nullptr,&sampleView,sampleHeap->GetCPUDescriptorHandleForHeapStart());device->CreateShaderResourceView(sampled.Get(),&sampleView,sampleCPUHandle);
#endif
#endif
    auto bindSample=[&]{
#ifdef DEMENTIUM_DESCRIPTOR_SAMPLE
#ifdef DEMENTIUM_TEXTURE_SAMPLE
        ID3D12DescriptorHeap* heaps[]={samplerHeap.Get(),sampleHeap.Get()};list->SetDescriptorHeaps(2,heaps);
#ifdef DEMENTIUM_WRONG_SAMPLER
        list->SetGraphicsRootDescriptorTable(2,samplerHeap->GetGPUDescriptorHandleForHeapStart());
#else
        list->SetGraphicsRootDescriptorTable(2,samplerGPUHandle);
#endif
#else
        ID3D12DescriptorHeap* heaps[]={sampleHeap.Get()};list->SetDescriptorHeaps(1,heaps);
#endif
#ifdef DEMENTIUM_WRONG_DESCRIPTOR
        list->SetGraphicsRootDescriptorTable(1,sampleHeap->GetGPUDescriptorHandleForHeapStart());
#else
        list->SetGraphicsRootDescriptorTable(1,sampleGPUHandle);
#endif
#endif
    };
    void* nativeCPU{};void* cloneCPU{};D3D12_RANGE none{};check(native->Map(0,&none,&nativeCPU));check(clones->Map(0,&none,&cloneCPU));
    auto data=static_cast<float*>(nativeCPU);std::memset(data,0,1280);data[0]=data[5]=data[10]=1;data[12]=data[17]=1;data[22]=1.002044916f;data[23]=.1002044976f;data[26]=-1;data[29]=data[31]=1;
    D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.NumDescriptors=3;hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;ComPtr<ID3D12DescriptorHeap> rtv;check(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtv)));hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_DSV;ComPtr<ID3D12DescriptorHeap> dsv;check(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&dsv)));
    std::array<ComPtr<ID3D12Resource>,3> colors,depths,resolved,readback;std::array<D3D12_CPU_DESCRIPTOR_HANDLE,3> rt{},ds{};
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
    for(unsigned i=0;i<3;++i){
        D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=d.Height=64;d.DepthOrArraySize=d.MipLevels=1;d.SampleDesc.Count=stereo::sceneSamples;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_RENDER_TARGET,nullptr,IID_PPV_ARGS(&colors[i])));
        d.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;D3D12_CLEAR_VALUE clear{};clear.Format=d.Format;clear.DepthStencil.Depth=1;
        check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_DEPTH_WRITE,&clear,IID_PPV_ARGS(&depths[i])));
        rt[i]=rtv->GetCPUDescriptorHandleForHeapStart();rt[i].ptr+=i*device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);ds[i]=dsv->GetCPUDescriptorHandleForHeapStart();ds[i].ptr+=i*device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        device->CreateRenderTargetView(colors[i].Get(),nullptr,rt[i]);device->CreateDepthStencilView(depths[i].Get(),nullptr,ds[i]);
        const float blue[]={0,0,1,1};list->ClearRenderTargetView(rt[i],blue,0,nullptr);list->ClearDepthStencilView(ds[i],D3D12_CLEAR_FLAG_DEPTH|D3D12_CLEAR_FLAG_STENCIL,1,0,0,nullptr);
        d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.Flags=D3D12_RESOURCE_FLAG_NONE;check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_RESOLVE_DEST,nullptr,IID_PPV_ARGS(&resolved[i])));
        readback[i]=buffer(64*256,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
    }
    // No XR initialization or runtime calls: only the view values consumed by duplicate.
    XrSessionBridge views;for(unsigned eye=0;eye<2;++eye){auto& e=views.eyes[eye];e.width=e.height=64;e.view.pose.orientation.w=1;e.view.pose.position.x=eye?.032f:-.032f;e.view.fov={-.785398f,.785398f,.785398f,-.785398f};stereo::rtvs[eye]=rt[eye+1];stereo::dsvs[eye]=ds[eye+1];}
    stereo::xr=&views;stereo::gameDevice=device;stereo::constants=clones;stereo::constantsCPU=static_cast<unsigned char*>(cloneCPU);stereo::cursor=0;stereo::active=true;stereo::failed=false;stereo::origin.orientation.w=1;
    auto& upload=stereo::uploads[native->GetGPUVirtualAddress()];upload.resource=native;upload.mapped(nativeCPU,1280);
    stereo::PipelineInfo info{};info.constants=128;info.color=pd.RTVFormats[0];info.depth=pd.DSVFormat;info.samples=pd.SampleDesc.Count;stereo::pipelineInfo[pipeline.Get()]=info;
    auto& state=stereo::lists[list.Get()];state.rootSignature=root.Get();state.pipeline=pipeline.Get();state.cbv=native->GetGPUVirtualAddress();state.rtvCount=state.viewportCount=state.scissorCount=1;state.rtvs[0]=rt[0];state.hasDepth=true;state.dsv=ds[0];state.viewports[0]={8,4,32,24,0,1};state.scissors[0]={12,8,32,24};
    list->SetGraphicsRootSignature(root.Get());bindSample();list->SetPipelineState(pipeline.Get());list->SetGraphicsRootConstantBufferView(0,state.cbv);list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);list->OMSetRenderTargets(1,rt.data(),FALSE,ds.data());list->RSSetViewports(1,state.viewports.data());list->RSSetScissorRects(1,state.scissors.data());
    unsigned callbacks=0;
#ifdef DEMENTIUM_INSTALLED_DRAW_TEST
    installedOwner=list.Get();const auto previousDraws=draws.load();
    D3D12_RESOURCE_DESC chainTextureDesc{};chainTextureDesc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;chainTextureDesc.Width=8;chainTextureDesc.Height=4;chainTextureDesc.DepthOrArraySize=chainTextureDesc.MipLevels=1;chainTextureDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;chainTextureDesc.SampleDesc.Count=1;ComPtr<ID3D12Resource> chainTexture;check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&chainTextureDesc,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&chainTexture)));
    nativeCopyEpoch::Source chainEpochs;nativeCopySession::Session chainSession;chainSession.created(chainEpochs,list.Get());installedSession=&chainSession;nativeCopyPreflight::Plan chainPlan;chainPlan.valid=true;chainPlan.site=nativeCopyBarrierPolicy::Site::UploadBegin;chainPlan.copyReturnRva=0x1eac36;chainPlan.endReturnRva=0x1eac72;chainPlan.source.pResource=native.Get();chainPlan.source.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;chainPlan.source.PlacedFootprint.Footprint={DXGI_FORMAT_R8G8B8A8_UNORM,8,4,1,256};chainPlan.destination.pResource=chainTexture.Get();chainPlan.destination.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;chainPlan.box={0,0,0,8,4,1};D3D12_RESOURCE_BARRIER chainBegin{};chainBegin.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;chainBegin.Transition={chainTexture.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_DEST};auto chainBarrier=[](const auto&){installedPrefix.push_back(1);};auto chainCopy=[](const auto&,const auto&,const auto&){installedPrefix.push_back(2);};if(!chainSession.begin(chainPlan,list.Get(),10,100,chainBegin,chainBarrier,chainCopy)||!chainSession.copy(chainPlan.copyReturnRva,20,&chainPlan.destination,0,0,0,&chainPlan.source,&chainPlan.box,chainBarrier,chainCopy))throw std::runtime_error("draw chain fixture admission failed");
    auto dispatchDraw=+[](ID3D12GraphicsCommandList* incoming,void(*invoke)(void*),void* context){if(!installedSession->owns(incoming)){++installedBypasses;invoke(context);return;}if(!installedSession->pending()){++installedOtherOwned;invoke(context);return;}++installedDispatches;installedSession->intervening([](const auto&){installedPrefix.push_back(1);},[](const auto&,const auto&,const auto&){installedPrefix.push_back(2);},[&]{if(installedPrefix!=std::vector<unsigned>({1,2}))throw std::runtime_error("draw chain prefix order differs");invoke(context);installedPrefix.push_back(3);});};
#ifdef DEMENTIUM_ALL_COMMAND_HOOKS
    {InstalledCommands detour;detour.install(list.Get());nativeCopyForwarders::dispatch=dispatchDraw;bindSample();list->DrawInstanced(3,1,0,0);if(!detour.stop())throw std::runtime_error("all-command restore failed");}
#else
    {InstalledDraw detour;detour.target=(*reinterpret_cast<void***>(list.Get()))[12];detour.previous=originalDraw;if(MH_Initialize()!=MH_OK)throw std::runtime_error("draw probe initialize failed");detour.initialized=true;auto receiver=nativeCopyForwarders::chain<&ID3D12GraphicsCommandList::DrawInstanced>(&stereoDrawProbe,originalDraw);if(MH_CreateHook(detour.target,reinterpret_cast<void*>(&nativeCopyForwarders::Adapter<&ID3D12GraphicsCommandList::DrawInstanced>::hook),receiver)!=MH_OK||MH_EnableHook(detour.target)!=MH_OK)throw std::runtime_error("draw probe install failed");nativeCopyForwarders::dispatch=dispatchDraw;bindSample();list->DrawInstanced(3,1,0,0);}
#endif
    bool validBypass=installedBypasses==3;
#ifdef DEMENTIUM_ALL_COMMAND_HOOKS
    validBypass=installedBypasses>=3&&installedOtherOwned>0;std::printf("installed_all_commands restored_targets=51 other_owned=%u nested_bypasses=%u\n",installedOtherOwned,installedBypasses);
#endif
    if(chainSession.pending()||installedDispatches!=1||!validBypass||installedPrefix!=std::vector<unsigned>({1,2,3}))throw std::runtime_error("active-stereo chain replay differs");installedSession=nullptr;
    std::printf("installed_active_stereo owned=%u nested=%u nested_internal=%d mod_draws=%u\n",installedOwned,installedNested,installedNestedInternal,draws.load()-previousDraws);if(installedOwned!=1||installedNested!=3||!installedNestedInternal||internal||draws.load()-previousDraws!=1)throw std::runtime_error("installed active-stereo draw reentry/count differs");
#else
    stereo::duplicate(list.Get(),[&]{++callbacks;list->DrawInstanced(3,1,0,0);});if(callbacks!=2)throw std::runtime_error("production stereo path did not draw both eyes");
#endif
    // Clones captured green. The original GPU buffer is now red. A forgotten
    // root-CBV restoration would incorrectly leave this native draw green.
    data[28]=1;data[29]=0;list->DrawInstanced(3,1,0,0);
    for(unsigned i=0;i<3;++i){auto barrier=stereo::transition(colors[i].Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_RESOLVE_SOURCE);list->ResourceBarrier(1,&barrier);list->ResolveSubresource(resolved[i].Get(),0,colors[i].Get(),0,DXGI_FORMAT_R8G8B8A8_UNORM);barrier=stereo::transition(resolved[i].Get(),D3D12_RESOURCE_STATE_RESOLVE_DEST,D3D12_RESOURCE_STATE_COPY_SOURCE);list->ResourceBarrier(1,&barrier);
        D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=readback[i].Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint.Footprint={DXGI_FORMAT_R8G8B8A8_UNORM,64,64,1,256};D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=resolved[i].Get();src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);}
    check(list->Close());ID3D12CommandList* lists[]={list.Get()};queue->ExecuteCommandLists(1,lists);ComPtr<ID3D12Fence> fence;check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));check(queue->Signal(fence.Get(),1));HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!event)throw std::runtime_error("fence event unavailable");check(fence->SetEventOnCompletion(1,event));if(WaitForSingleObject(event,10000)!=WAIT_OBJECT_0)throw std::runtime_error("stereo GPU timeout");CloseHandle(event);
    for(unsigned i=0;i<3;++i){void* pointer{};check(readback[i]->Map(0,nullptr,&pointer));auto pixels=static_cast<const unsigned char*>(pointer);for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x){bool red=i==0&&x>=12&&x<32&&y>=8&&y<24;unsigned char expected[]={static_cast<unsigned char>(red?255:0),static_cast<unsigned char>(i?255:0),static_cast<unsigned char>(i||red?0:255),255};for(unsigned channel=0;channel<4;++channel)if(pixels[(y*64+x)*4+channel]!=expected[channel])throw std::runtime_error("native state restoration or independent eye target pixels differ");}readback[i]->Unmap(0,&none);}
    // Exercise sourceDraw separately: a mask-aware test VS consumes the exact
    // c74.z padding written by production. A subsequent native draw must use
    // the original PSO and original CBV, without duplicate masking a failure.
    const char maskSource[]=R"(
cbuffer Native:register(b0){float4 data[75];}
struct V{float4 p:SV_POSITION;float4 color:COLOR;};
V vs(uint id:SV_VertexID){V o;float2 p=float2((id<<1)&2,id&2);o.p=float4(p*float2(2,-2)+float2(-1,1),.5,1);o.color=asuint(data[74].z)==1?float4(0,1,0,1):float4(1,0,1,1);return o;}
)";
    ComPtr<ID3DBlob> maskVS;check(D3DCompile(maskSource,sizeof(maskSource)-1,nullptr,nullptr,nullptr,"vs","vs_5_0",0,0,&maskVS,&error));auto maskDesc=pd;maskDesc.VS={maskVS->GetBufferPointer(),maskVS->GetBufferSize()};ComPtr<ID3D12PipelineState> maskPipeline;check(device->CreateGraphicsPipelineState(&maskDesc,IID_PPV_ARGS(&maskPipeline)));
    info.constants=1200;info.handPipeline=maskPipeline;stereo::pipelineInfo[pipeline.Get()]=info;nativeHandClip::currentMask=1;
    check(allocator->Reset());check(list->Reset(allocator.Get(),pipeline.Get()));auto barrier=stereo::transition(colors[0].Get(),D3D12_RESOURCE_STATE_RESOLVE_SOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);list->ResourceBarrier(1,&barrier);barrier=stereo::transition(resolved[0].Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_RESOLVE_DEST);list->ResourceBarrier(1,&barrier);
    const float blue[]={0,0,1,1};list->ClearRenderTargetView(rt[0],blue,0,nullptr);list->ClearDepthStencilView(ds[0],D3D12_CLEAR_FLAG_DEPTH|D3D12_CLEAR_FLAG_STENCIL,1,0,0,nullptr);
    state.viewports[0]={8,4,56,24,0,1};list->SetGraphicsRootSignature(root.Get());bindSample();list->SetGraphicsRootConstantBufferView(0,state.cbv);list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);list->OMSetRenderTargets(1,rt.data(),FALSE,ds.data());list->RSSetViewports(1,state.viewports.data());list->RSSetScissorRects(1,state.scissors.data());
    callbacks=0;stereo::sourceDraw(list.Get(),[&]{++callbacks;list->DrawInstanced(3,1,0,0);});if(callbacks!=1)throw std::runtime_error("source draw callback count differs");
    D3D12_RECT second{40,8,56,24};list->RSSetScissorRects(1,&second);list->DrawInstanced(3,1,0,0);nativeHandClip::currentMask=0;
    barrier=stereo::transition(colors[0].Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_RESOLVE_SOURCE);list->ResourceBarrier(1,&barrier);list->ResolveSubresource(resolved[0].Get(),0,colors[0].Get(),0,DXGI_FORMAT_R8G8B8A8_UNORM);barrier=stereo::transition(resolved[0].Get(),D3D12_RESOURCE_STATE_RESOLVE_DEST,D3D12_RESOURCE_STATE_COPY_SOURCE);list->ResourceBarrier(1,&barrier);
    D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=readback[0].Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint.Footprint={DXGI_FORMAT_R8G8B8A8_UNORM,64,64,1,256};D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=resolved[0].Get();src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);
    check(list->Close());queue->ExecuteCommandLists(1,lists);check(queue->Signal(fence.Get(),2));event=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!event)throw std::runtime_error("source fence event unavailable");check(fence->SetEventOnCompletion(2,event));if(WaitForSingleObject(event,10000)!=WAIT_OBJECT_0)throw std::runtime_error("source GPU timeout");CloseHandle(event);
    void* pointer{};check(readback[0]->Map(0,nullptr,&pointer));auto pixels=static_cast<const unsigned char*>(pointer);for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x){bool green=x>=12&&x<32&&y>=8&&y<24,red=x>=40&&x<56&&y>=8&&y<24;unsigned char expected[]={static_cast<unsigned char>(red?255:0),static_cast<unsigned char>(green?255:0),static_cast<unsigned char>(red||green?0:255),255};for(unsigned channel=0;channel<4;++channel)if(pixels[(y*64+x)*4+channel]!=expected[channel])throw std::runtime_error("source mask or native pipeline/CBV restoration differs");}readback[0]->Unmap(0,&none);
    // Native (unmodified) game VS, close and distant geometry in the same draw.
    // This also checks the GPU's actual clip-Z convention and stereo disparity.
    std::ifstream archive(L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Dementium - The Ward\\rom.zar",std::ios::binary);archive.seekg(657383424);std::vector<char> nativeVS(3524);if(!archive.read(nativeVS.data(),nativeVS.size())||std::memcmp(nativeVS.data(),"DXBC",4))throw std::runtime_error("audited native vertex shader unavailable");
    ComPtr<ID3D12ShaderReflection> reflection;check(D3DReflect(nativeVS.data(),nativeVS.size(),IID_PPV_ARGS(&reflection)));D3D12_SHADER_BUFFER_DESC cb{};check(reflection->GetConstantBufferByIndex(0)->GetDesc(&cb));if(cb.Size!=1200)throw std::runtime_error("native VS constant layout changed");
    const char whiteSource[]="float4 ps():SV_TARGET{return float4(1,1,1,1);}";ComPtr<ID3DBlob> whitePS;check(D3DCompile(whiteSource,sizeof(whiteSource)-1,nullptr,nullptr,nullptr,"ps","ps_5_0",0,0,&whitePS,&error));
    D3D12_INPUT_ELEMENT_DESC layout[]={{"SV_POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TEXCOORD",1,DXGI_FORMAT_R32G32_FLOAT,0,20,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TEXCOORD",2,DXGI_FORMAT_R32G32_FLOAT,0,28,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"NORMAL",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,36,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"NORMAL",1,DXGI_FORMAT_R32G32B32_FLOAT,0,52,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
    auto nativeDesc=pd;nativeDesc.VS={nativeVS.data(),nativeVS.size()};nativeDesc.PS={whitePS->GetBufferPointer(),whitePS->GetBufferSize()};nativeDesc.InputLayout={layout,6};ComPtr<ID3D12PipelineState> nativePipeline;check(device->CreateGraphicsPipelineState(&nativeDesc,IID_PPV_ARGS(&nativePipeline)));
    float geometry[6][16]{};float positions[6][3]={{-.1f,.15f,-1},{.1f,.15f,-1},{0,.35f,-1},{-.01f,-.035f,-.1f},{.01f,-.035f,-.1f},{0,-.015f,-.1f}};
    for(unsigned i=0;i<6;++i){std::memcpy(geometry[i],positions[i],12);geometry[i][11]=1;geometry[i][13]=1;}
    auto vertices=buffer(512,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);void* vertexCPU{};check(vertices->Map(0,&none,&vertexCPU));std::memcpy(vertexCPU,geometry,sizeof geometry);vertices->Unmap(0,nullptr);
    std::memset(data,0,1280);data[0]=.991187f;data[5]=1.767617f;data[10]=1.002044916f;data[11]=.1002044976f;data[14]=-1;data[16]=data[21]=data[26]=1;data[28]=data[33]=1;data[292]=data[293]=data[294]=data[296]=data[297]=1;
    info.handPipeline.Reset();info.constants=1200;stereo::pipelineInfo[nativePipeline.Get()]=info;state.pipeline=nativePipeline.Get();state.viewports[0]={0,0,64,64,0,1};state.scissors[0]={0,0,64,64};stereo::cursor=0;
    check(allocator->Reset());check(list->Reset(allocator.Get(),nativePipeline.Get()));
    for(unsigned i=0;i<3;++i){barrier=stereo::transition(colors[i].Get(),D3D12_RESOURCE_STATE_RESOLVE_SOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);list->ResourceBarrier(1,&barrier);barrier=stereo::transition(resolved[i].Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_RESOLVE_DEST);list->ResourceBarrier(1,&barrier);list->ClearRenderTargetView(rt[i],blue,0,nullptr);list->ClearDepthStencilView(ds[i],D3D12_CLEAR_FLAG_DEPTH|D3D12_CLEAR_FLAG_STENCIL,1,0,0,nullptr);}
    D3D12_VERTEX_BUFFER_VIEW vertexView{vertices->GetGPUVirtualAddress(),sizeof geometry,sizeof geometry[0]};list->SetGraphicsRootSignature(root.Get());bindSample();list->SetGraphicsRootConstantBufferView(0,state.cbv);list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);list->IASetVertexBuffers(0,1,&vertexView);list->OMSetRenderTargets(1,rt.data(),FALSE,ds.data());list->RSSetViewports(1,state.viewports.data());list->RSSetScissorRects(1,state.scissors.data());
    callbacks=0;stereo::duplicate(list.Get(),[&]{++callbacks;list->DrawInstanced(6,1,0,0);});if(callbacks!=2)throw std::runtime_error("native VS stereo draw missing");
    for(unsigned i=1;i<3;++i){barrier=stereo::transition(colors[i].Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_RESOLVE_SOURCE);list->ResourceBarrier(1,&barrier);list->ResolveSubresource(resolved[i].Get(),0,colors[i].Get(),0,DXGI_FORMAT_R8G8B8A8_UNORM);barrier=stereo::transition(resolved[i].Get(),D3D12_RESOURCE_STATE_RESOLVE_DEST,D3D12_RESOURCE_STATE_COPY_SOURCE);list->ResourceBarrier(1,&barrier);dst.pResource=readback[i].Get();src.pResource=resolved[i].Get();list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);}
    check(list->Close());queue->ExecuteCommandLists(1,lists);check(queue->Signal(fence.Get(),3));event=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!event)throw std::runtime_error("native VS event unavailable");check(fence->SetEventOnCompletion(3,event));if(WaitForSingleObject(event,10000)!=WAIT_OBJECT_0)throw std::runtime_error("native VS GPU timeout");CloseHandle(event);
    double centroids[2][2]{};
    for(unsigned eye=0;eye<2;++eye){check(readback[eye+1]->Map(0,nullptr,&pointer));auto image=static_cast<const unsigned char*>(pointer);double weights[2]{};for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x){unsigned region=y>=32;double weight=image[(y*64+x)*4]/255.;weights[region]+=weight;centroids[eye][region]+=(x+.5)*weight;}for(unsigned region=0;region<2;++region){if(weights[region]<10)throw std::runtime_error("native close or distant geometry clipped/missing");centroids[eye][region]/=weights[region];}readback[eye+1]->Unmap(0,&none);}
    double farDisparity=centroids[0][0]-centroids[1][0],nearDisparity=centroids[0][1]-centroids[1][1];printf("native_VS_GPU_disparity far=%.6f near=%.6f expected=2.048,20.48 pixels\n",farDisparity,nearDisparity);if(std::fabs(farDisparity-2.048)>.25||std::fabs(nearDisparity-20.48)>.25)throw std::runtime_error("native VS stereo parallax differs from independent pinhole expectation");
    // Compare the production pickup predicate with actual native-VS GPU clip
    // results. Binary occlusion queries avoid inferring visibility from color.
    // Zero-radius spheres correspond exactly to point primitives; cases near
    // XY raster boundaries are excluded from the comparison, not forced to pass.
    constexpr unsigned cases=512,queryCount=cases*2;
    auto pointDesc=nativeDesc;pointDesc.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;pointDesc.DepthStencilState.DepthEnable=FALSE;pointDesc.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ZERO;
    ComPtr<ID3D12PipelineState> pointPipeline;check(device->CreateGraphicsPipelineState(&pointDesc,IID_PPV_ARGS(&pointPipeline)));stereo::pipelineInfo[pointPipeline.Get()]=info;state.pipeline=pointPipeline.Get();
    D3D12_QUERY_HEAP_DESC queryDesc{D3D12_QUERY_HEAP_TYPE_OCCLUSION,queryCount,0};ComPtr<ID3D12QueryHeap> queries;check(device->CreateQueryHeap(&queryDesc,IID_PPV_ARGS(&queries)));auto queryReadback=buffer(queryCount*8,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
    std::array<std::array<float,16>,cases> points{};std::array<XrPosef,cases> heads{};std::array<XrFovf,cases> fovs{};
    std::uint32_t seed=0x6d9a1723;auto random=[&]{seed=1664525u*seed+1013904223u;return float(seed>>8)/float(0x1000000);};
    const float distances[]={.005f,.019f,.021f,.03f,.1f,1.f,49.f,51.f};
    for(unsigned i=0;i<cases;++i){
        auto& point=points[i];point[11]=point[13]=1;
        if(i<32){heads[i]={{0,0,0,1},{0,0,0}};fovs[i]={-.785398f,.785398f,.785398f,-.785398f};point[0]=(i&8)?.032f:-.032f;point[2]=-distances[i%8];}
        else{XMFLOAT4 orientation;XMStoreFloat4(&orientation,XMQuaternionRotationRollPitchYaw((random()-.5f)*1.4f,(random()-.5f)*2.4f,(random()-.5f)*.4f));heads[i]={{orientation.x,orientation.y,orientation.z,orientation.w},{(random()-.5f)*.8f,(random()-.5f)*.8f,(random()-.5f)*.8f}};fovs[i]={-.6f-.3f*random(),.6f+.3f*random(),.6f+.3f*random(),-.6f-.3f*random()};point[0]=(random()-.5f)*8;point[1]=(random()-.5f)*6;point[2]=random()*7-5;}
    }
    auto pointBuffer=buffer(sizeof points,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);check(pointBuffer->Map(0,&none,&pointer));std::memcpy(pointer,points.data(),sizeof points);pointBuffer->Unmap(0,nullptr);D3D12_VERTEX_BUFFER_VIEW pointView{pointBuffer->GetGPUVirtualAddress(),sizeof points,sizeof points[0]};
    std::array<bool,queryCount> expected{},stable{};stereo::cursor=0;
    check(allocator->Reset());check(list->Reset(allocator.Get(),pointPipeline.Get()));
    for(unsigned i=1;i<3;++i){barrier=stereo::transition(colors[i].Get(),D3D12_RESOURCE_STATE_RESOLVE_SOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);list->ResourceBarrier(1,&barrier);}
    list->SetGraphicsRootSignature(root.Get());bindSample();list->SetGraphicsRootConstantBufferView(0,state.cbv);list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);list->IASetVertexBuffers(0,1,&pointView);list->OMSetRenderTargets(1,rt.data(),FALSE,ds.data());list->RSSetViewports(1,state.viewports.data());list->RSSetScissorRects(1,state.scissors.data());
    for(unsigned i=0;i<cases;++i){
        for(unsigned eye=0;eye<2;++eye){
            auto& view=views.eyes[eye].view;view.pose=heads[i];view.fov=fovs[i];XMFLOAT3 offset;XMStoreFloat3(&offset,XMVector3Rotate(XMVectorSet(eye?.032f:-.032f,0,0,0),XMVectorSet(heads[i].orientation.x,heads[i].orientation.y,heads[i].orientation.z,heads[i].orientation.w)));view.pose.position.x+=offset.x;view.pose.position.y+=offset.y;view.pose.position.z+=offset.z;
            float camera[300];std::memcpy(camera,data,1200);stereo::transform(camera,view);expected[i*2+eye]=sphereFrustum::contains(camera,camera+16,points[i].data(),0);
            double position[4]={0,0,0,1},clip[4]{};for(unsigned row=0;row<3;++row)position[row]=camera[16+row*4]*points[i][0]+camera[17+row*4]*points[i][1]+camera[18+row*4]*points[i][2]+camera[19+row*4];for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)clip[row]+=camera[row*4+col]*position[col];clip[2]=-2*clip[2]-clip[3];
            if(clip[3]<-.02)stable[i*2+eye]=true;
            else if(clip[3]>.0001){const double x=clip[0]/clip[3],y=clip[1]/clip[3],z=clip[2]/clip[3];stable[i*2+eye]=std::fabs(std::fabs(x)-1)>.04&&std::fabs(std::fabs(y)-1)>.04&&std::fabs(z)>1e-5&&std::fabs(z-1)>1e-5;}
        }
        unsigned eye=0;stereo::duplicate(list.Get(),[&]{const unsigned query=i*2+eye++;list->BeginQuery(queries.Get(),D3D12_QUERY_TYPE_BINARY_OCCLUSION,query);list->DrawInstanced(1,1,i,0);list->EndQuery(queries.Get(),D3D12_QUERY_TYPE_BINARY_OCCLUSION,query);});if(eye!=2)throw std::runtime_error("native point query did not reach both eyes");
    }
    list->ResolveQueryData(queries.Get(),D3D12_QUERY_TYPE_BINARY_OCCLUSION,0,queryCount,queryReadback.Get(),0);check(list->Close());queue->ExecuteCommandLists(1,lists);check(queue->Signal(fence.Get(),4));event=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!event)throw std::runtime_error("native query event unavailable");check(fence->SetEventOnCompletion(4,event));if(WaitForSingleObject(event,10000)!=WAIT_OBJECT_0)throw std::runtime_error("native query GPU timeout");CloseHandle(event);
    check(queryReadback->Map(0,nullptr,&pointer));auto results=static_cast<const UINT64*>(pointer);unsigned compared=0,inside=0,outside=0;
    for(unsigned i=0;i<queryCount;++i)if(stable[i]){const bool visible=results[i]!=0;if(visible!=expected[i]){printf("native_query_mismatch case=%u eye=%u cpu=%d gpu=%d\n",i/2,i%2,expected[i],visible);throw std::runtime_error("pickup predicate disagrees with actual native VS GPU clip");}++compared;if(visible)++inside;else ++outside;}queryReadback->Unmap(0,&none);
    if(compared<800||inside<40||outside<40)throw std::runtime_error("native GPU frustum oracle coverage insufficient");printf("native_VS_frustum_queries compared=%u inside=%u outside=%u boundary_excluded=%u\n",compared,inside,outside,queryCount-compared);
    for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T bytes{};messages->GetMessage(i,nullptr,&bytes);std::vector<char> storage(bytes);auto message=reinterpret_cast<D3D12_MESSAGE*>(storage.data());messages->GetMessage(i,message,&bytes);if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(message->pDescription);}
    // Exercise production synchronization on the software queue as well as the
    // fault-injected helper tests. Neither route creates an XR session.
    stereo::fence=fence;stereo::fenceValue=4;stereo::xrQueue=queue;
    stereo::fenceEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!stereo::fenceEvent)throw std::runtime_error("production fence event unavailable");
    stereo::syncGPU(queue.Get());stereo::syncGPU();
    if(stereo::fenceValue!=6||fence->GetCompletedValue()<6)throw std::runtime_error("production queue fence incomplete");
    stereo::fenceValue=UINT64_MAX-1;bool exhausted=false;
    try{stereo::syncGPU(queue.Get());}catch(const std::runtime_error& e){exhausted=std::string(e.what()).find("exhausted")!=std::string::npos;}
    if(!exhausted)throw std::runtime_error("production fence reserved value guard failed");
    CloseHandle(stereo::fenceEvent);stereo::fenceEvent=nullptr;
    stereo::active=false;stereo::xr=nullptr;std::puts("PASS: production stereo/source draw state restoration and mask padding, 16384 exact GPU pixels, actual native VS at10cm/1m with near/far stereo parallax, production queue synchronization; no D3D12 errors, game or XR session.");return 0;
}catch(const std::exception& e){std::puts(e.what());return 1;}}



