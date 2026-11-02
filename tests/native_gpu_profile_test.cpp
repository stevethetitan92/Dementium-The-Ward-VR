#include "../src/native_gpu_profile.hpp"
#include <dxgi1_4.h>
#include <d3d12sdklayers.h>
#include <cstdio>
#include <stdexcept>
#include <thread>
using Microsoft::WRL::ComPtr;
void check(HRESULT h){if(FAILED(h))throw std::runtime_error("fixture HRESULT failed");}
int main(){try{
 ComPtr<ID3D12Debug> debug;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
 ComPtr<IDXGIFactory4> factory;check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));ComPtr<IDXGIAdapter> warp;check(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));ComPtr<ID3D12Device> d;check(D3D12CreateDevice(warp.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&d)));
 D3D12_COMMAND_QUEUE_DESC qd{};ComPtr<ID3D12CommandQueue> q,other;check(d->CreateCommandQueue(&qd,IID_PPV_ARGS(&q)));check(d->CreateCommandQueue(&qd,IID_PPV_ARGS(&other)));
 ComPtr<ID3D12CommandAllocator> alloc;check(d->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&alloc)));ComPtr<ID3D12GraphicsCommandList> c;check(d->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,alloc.Get(),nullptr,IID_PPV_ARGS(&c)));
 D3D12_RESOURCE_DESC rd{};rd.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;rd.Width=64;rd.Height=1;rd.DepthOrArraySize=rd.MipLevels=rd.SampleDesc.Count=1;rd.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
 D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_UPLOAD;ComPtr<ID3D12Resource> source,result;
 check(d->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&rd,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&source)));hp.Type=D3D12_HEAP_TYPE_READBACK;check(d->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&rd,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&result)));
 void* memory{};check(source->Map(0,nullptr,&memory));memset(memory,0x5a,64);source->Unmap(0,nullptr);c->CopyBufferRegion(result.Get(),0,source.Get(),0,64);check(c->Close());
 ComPtr<ID3D12Fence> fence;check(d->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));HANDLE event=CreateEvent(nullptr,FALSE,FALSE,nullptr);if(!event)throw std::runtime_error("event failed");UINT64 fenceValue{};
 auto wait=[&]{check(q->Signal(fence.Get(),++fenceValue));if(fence->GetCompletedValue()<fenceValue){check(fence->SetEventOnCompletion(fenceValue,event));if(WaitForSingleObject(event,10000)!=WAIT_OBJECT_0)throw std::runtime_error("fence failed");}};
 std::array<ComPtr<ID3D12GraphicsCommandList>,5> nativeLists;nativeLists[0]=c;for(unsigned i=1;i<5;++i){check(d->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,alloc.Get(),nullptr,IID_PPV_ARGS(&nativeLists[i])));nativeLists[i]->CopyBufferRegion(result.Get(),0,source.Get(),0,64);check(nativeLists[i]->Close());}
 nativeGpuProfile::Timer<4> timer;timer.completed(q.Get());unsigned originalCalls{};
 auto submit=[&](ID3D12CommandQueue* queue,UINT n,ID3D12CommandList*const* lists){if(n==1)for(auto& native:nativeLists)if(lists[0]==native.Get())++originalCalls;queue->ExecuteCommandLists(n,lists);};
 for(unsigned frame=0;frame<20;++frame){
  for(unsigned batch=0;batch<5;++batch){ID3D12CommandList* native[]={nativeLists[batch].Get()};timer.execute(d.Get(),q.Get(),1,native,submit);}
  if(frame==0){timer.completed(other.Get());if(timer.samples)throw std::runtime_error("wrong queue exposed samples");}
  wait();timer.completed(q.Get());
 }
 std::thread foreign([&]{ID3D12CommandList* native[]={c.Get()};timer.execute(d.Get(),q.Get(),1,native,submit);});foreign.join();wait();
 if(timer.samples!=80||timer.capacitySkips!=20||timer.foreignThreadSkips!=1||timer.failures!=1||originalCalls!=101)throw std::runtime_error("coverage or native passthrough failed");
 check(result->Map(0,nullptr,&memory));for(unsigned i=0;i<64;++i)if(static_cast<unsigned char*>(memory)[i]!=0x5a)throw std::runtime_error("native copy lost");result->Unmap(0,nullptr);
 ComPtr<ID3D12InfoQueue> info;if(SUCCEEDED(d.As(&info))){for(UINT64 i=0;i<info->GetNumStoredMessages();++i){SIZE_T n{};info->GetMessage(i,nullptr,&n);auto bytes=std::make_unique<unsigned char[]>(n);auto msg=reinterpret_cast<D3D12_MESSAGE*>(bytes.get());check(info->GetMessage(i,msg,&n));if(msg->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(msg->pDescription);}}
 timer.resetTotals();if(timer.samples||timer.milliseconds||timer.capacitySkips||timer.foreignThreadSkips)throw std::runtime_error("reset failed");CloseHandle(event);
 puts("PASS: 80 WARP native batch timestamps, 20 capacity passthroughs, foreign-thread passthrough, wrong-queue rejection, 101 preserved copies, no D3D12 errors. No VR performance claim.");return 0;
 }catch(const std::exception& e){puts(e.what());return 1;}}
