#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <cstring>
namespace gpuBatchProfile {
// Render-thread-only instrumentation of mod-owned command lists. The caller
// already waits for this list before allocator reuse; this adds no GPU wait.
class Timer {
 Microsoft::WRL::ComPtr<ID3D12QueryHeap> heap;
 Microsoft::WRL::ComPtr<ID3D12Resource> readback;
 bool attempted{},recording{},resolved{};
public:
 double totalMilliseconds{};unsigned samples{},failures{};
 bool begin(ID3D12Device* device,ID3D12GraphicsCommandList* list){
  recording=resolved=false;
  if(!attempted){attempted=true;D3D12_QUERY_HEAP_DESC h{};h.Type=D3D12_QUERY_HEAP_TYPE_TIMESTAMP;h.Count=2;
   D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_READBACK;
   D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=16;d.Height=1;d.DepthOrArraySize=d.MipLevels=d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
   if(FAILED(device->CreateQueryHeap(&h,IID_PPV_ARGS(&heap)))||FAILED(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback)))){heap.Reset();readback.Reset();++failures;}
  }
  if(!heap||!readback)return false;
  list->EndQuery(heap.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0);recording=true;return true;
 }
 void end(ID3D12GraphicsCommandList* list){if(!recording)return;list->EndQuery(heap.Get(),D3D12_QUERY_TYPE_TIMESTAMP,1);list->ResolveQueryData(heap.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0,2,readback.Get(),0);recording=false;resolved=true;}
 // Call ONLY after the existing submission fence has completed.
 bool completed(ID3D12CommandQueue* queue){
  if(!resolved)return false;resolved=false;UINT64 frequency{};void* data{};D3D12_RANGE range{0,16};
  if(FAILED(queue->GetTimestampFrequency(&frequency))||!frequency||FAILED(readback->Map(0,&range,&data))){++failures;return false;}
  UINT64 ticks[2]{};memcpy(ticks,data,16);D3D12_RANGE none{};readback->Unmap(0,&none);
  if(ticks[1]<ticks[0]){++failures;return false;}totalMilliseconds+=double(ticks[1]-ticks[0])*1000./double(frequency);++samples;return true;
 }
 void resetTotals(){totalMilliseconds=0;samples=failures=0;}
};
}
