#define NOMINMAX
#include "../src/bridge.cpp"
#include <cstdio>
struct Heap final:ID3D12DescriptorHeap {
    HRESULT result=E_FAIL;ID3D12Device* parent{};unsigned descriptions{},queries{},cpu{},gpu{};
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void** out)override{*out=nullptr;return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{return 1;}ULONG STDMETHODCALLTYPE Release()override{return 1;}
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID,UINT*,void*)override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID,UINT,const void*)override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID,const IUnknown*)override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetName(LPCWSTR)override{return S_OK;}
    HRESULT STDMETHODCALLTYPE GetDevice(REFIID,void** out)override{++queries;*out=parent;if(SUCCEEDED(result)&&parent)parent->AddRef();return result;}
    D3D12_DESCRIPTOR_HEAP_DESC STDMETHODCALLTYPE GetDesc()override{++descriptions;return {D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,4,D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,0};}
    D3D12_CPU_DESCRIPTOR_HANDLE STDMETHODCALLTYPE GetCPUDescriptorHandleForHeapStart()override{++cpu;return {1000};}
    D3D12_GPU_DESCRIPTOR_HANDLE STDMETHODCALLTYPE GetGPUDescriptorHandleForHeapStart()override{++gpu;return {2000};}
};
static unsigned forwards{},adds{},releases{},strides{};static ID3D12DescriptorHeap* heapExpected;
static UINT STDMETHODCALLTYPE stride(ID3D12Device*,D3D12_DESCRIPTOR_HEAP_TYPE){++strides;return 32;}
static ULONG STDMETHODCALLTYPE add(ID3D12Device*){++adds;return 1;}static ULONG STDMETHODCALLTYPE release(ID3D12Device*){++releases;return 1;}
static bool invoke(ID3D12GraphicsCommandList* list,ID3D12DescriptorHeap*const* heaps){__try{heapsHook(list,1,heaps);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
int main(){
    auto list=reinterpret_cast<ID3D12GraphicsCommandList*>(std::uintptr_t(0x1000));auto& state=stereo::lists[list];state.descriptorHeaps[0]=reinterpret_cast<void*>(std::uintptr_t(0x55));state.textures[0]={999};state.heaps.push_back({10,20,3,32});const auto before=state;Heap heap;ID3D12DescriptorHeap* heaps[]={&heap};heapExpected=&heap;
    originalSetHeaps=+[](ID3D12GraphicsCommandList*,UINT n,ID3D12DescriptorHeap*const* value){if(n!=1||value[0]!=heapExpected)std::terminate();++forwards;};
    for(auto result:{E_FAIL,S_OK}){heap.result=result;if(!invoke(list,heaps)){std::puts("FAIL: heap metadata raised SEH after unavailable/null GetDevice");return 1;}if(state.descriptorHeaps!=before.descriptorHeaps||state.textures[0].ptr!=999||state.heaps.size()!=1||state.heaps[0].gpu!=10||heap.cpu||heap.gpu)return 1;}
    if(forwards!=2||heap.queries!=2)return 1;
    std::array<void*,16> table{};table[1]=reinterpret_cast<void*>(add);table[2]=reinterpret_cast<void*>(release);table[15]=reinterpret_cast<void*>(stride);struct Device{void** vtable;} device{table.data()};heap.parent=reinterpret_cast<ID3D12Device*>(&device);
    if(!invoke(list,heaps)||forwards!=3||state.descriptorHeaps[0]!=&heap||state.textures[0].ptr||state.heaps.size()!=1||state.heaps[0].gpu!=2000||state.heaps[0].cpu!=1000||state.heaps[0].stride!=32||adds!=1||releases!=1||strides!=1)return 1;
    const auto queries=heap.queries;internal=true;if(!invoke(list,heaps))return 1;internal=false;if(forwards!=4||heap.queries!=queries)return 1;originalSetHeaps=nullptr;
    std::puts("PASS: heap device failure/null preserves complete prior metadata and exact forwarding; successful binding commits after queries succeed, injected calls skip metadata queries; COM-shaped stubs only, no driver/GPU/game/XR.");return 0;
}

