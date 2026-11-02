#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <array>
#include <atomic>
#include <cstdint>
namespace nativeGpuProfile {
// Opt-in diagnostic only. Timestamp lists bracket the ORIGINAL submission in
// separate ExecuteCommandLists calls, which guarantees workload ordering.
// Native lists and their grouping are unchanged. Added submission overhead must
// be assessed against the disabled baseline; this is not a production speedup.
template<unsigned Capacity=64> class Timer {
 static_assert(Capacity>0&&Capacity<=4096,"bounded query capacity required");
 struct Pair {Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> before,after;};
 std::array<Pair,Capacity> pairs{};
 Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
 Microsoft::WRL::ComPtr<ID3D12QueryHeap> heap;
 Microsoft::WRL::ComPtr<ID3D12Resource> readback;
 Microsoft::WRL::ComPtr<ID3D12CommandQueue> boundQueue;
 std::atomic<DWORD> owner{};
 unsigned pending{};bool attempted{},available{};
 bool initialize(ID3D12Device* device,ID3D12CommandQueue* queue){
  if(attempted)return available;attempted=true;
  if(queue->GetDesc().Type!=D3D12_COMMAND_LIST_TYPE_DIRECT)return false;
  D3D12_QUERY_HEAP_DESC q{};q.Type=D3D12_QUERY_HEAP_TYPE_TIMESTAMP;q.Count=Capacity*2;
  D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_READBACK;
  D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=Capacity*16;d.Height=1;d.DepthOrArraySize=d.MipLevels=d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  available=SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)))&&SUCCEEDED(device->CreateQueryHeap(&q,IID_PPV_ARGS(&heap)))&&SUCCEEDED(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback)));
  if(available)boundQueue=queue;else ++failures;
  return available;
 }
 bool prepare(ID3D12Device* device,unsigned i){
  auto& p=pairs[i];if(p.before&&p.after)return true;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> a,b;
  if(FAILED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&a))))return false;
  a->EndQuery(heap.Get(),D3D12_QUERY_TYPE_TIMESTAMP,i*2);if(FAILED(a->Close()))return false;
  if(FAILED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&b))))return false;
  b->EndQuery(heap.Get(),D3D12_QUERY_TYPE_TIMESTAMP,i*2+1);b->ResolveQueryData(heap.Get(),D3D12_QUERY_TYPE_TIMESTAMP,i*2,2,readback.Get(),i*16);if(FAILED(b->Close()))return false;
  p.before=a;p.after=b;return true;
 }
public:
 double milliseconds{};unsigned samples{},failures{},capacitySkips{};
 std::atomic<unsigned> foreignThreadSkips{};
 // Must be called by the presentation thread AFTER its existing game-queue
 // completion fence. No new fence/wait is introduced here. Foreign-thread
 // submissions are intentionally unmeasured and never touch query slots.
 void completed(ID3D12CommandQueue* queue){
  if(owner.load()&&owner.load()!=GetCurrentThreadId())return;
  owner=GetCurrentThreadId();if(!pending)return;
  UINT64 frequency{};void* data{};D3D12_RANGE r{0,pending*16};
  if(queue!=boundQueue.Get()){++failures;return;} // Wrong queue cannot prove reuse safe.
  if(FAILED(queue->GetTimestampFrequency(&frequency))||!frequency||FAILED(readback->Map(0,&r,&data))){++failures;pending=0;return;}
  const auto* ticks=static_cast<const UINT64*>(data);
  for(unsigned i=0;i<pending;++i){if(ticks[i*2+1]<ticks[i*2]){++failures;continue;}milliseconds+=double(ticks[i*2+1]-ticks[i*2])*1000./double(frequency);++samples;}
  D3D12_RANGE none{};readback->Unmap(0,&none);pending=0;
 }
 template<class Submit> void execute(ID3D12Device* device,ID3D12CommandQueue* queue,UINT n,ID3D12CommandList*const* lists,Submit submit){
  if(owner.load()!=GetCurrentThreadId()){++foreignThreadSkips;submit(queue,n,lists);return;}
  if(!n||!initialize(device,queue)||queue!=boundQueue.Get()){submit(queue,n,lists);return;}
  if(pending==Capacity){++capacitySkips;submit(queue,n,lists);return;}
  if(!prepare(device,pending)){++failures;submit(queue,n,lists);return;}
  auto& p=pairs[pending++];ID3D12CommandList* first[]={p.before.Get()},*last[]={p.after.Get()};
  submit(queue,1,first);submit(queue,n,lists);submit(queue,1,last);
 }
 void resetTotals(){milliseconds=0;samples=failures=capacitySkips=0;foreignThreadSkips=0;}
};
}
