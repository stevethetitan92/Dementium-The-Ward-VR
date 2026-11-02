#define NOMINMAX
#include "../src/bridge.cpp"
#include <cstdio>
static unsigned calls[4]{},deviceQueries{};static UINT expectedCount;static const void* expectedData;static BOOL expectedContiguous{};static HRESULT queryResult=E_FAIL;
static HRESULT STDMETHODCALLTYPE missingDevice(ID3D12GraphicsCommandList*,REFIID,void** output){++deviceQueries;*output=nullptr;return queryResult;}
static bool contiguousTarget(ID3D12GraphicsCommandList* list,const D3D12_CPU_DESCRIPTOR_HANDLE* handle){__try{targetsHook(list,1,handle,TRUE,nullptr);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static void check(UINT count,const void* data,unsigned kind){if(count!=expectedCount||data!=expectedData)std::terminate();++calls[kind];}
static bool invoke(unsigned kind,ID3D12GraphicsCommandList* list,UINT count,const void* data){
    __try{switch(kind){case 0:viewportsHook(list,count,static_cast<const D3D12_VIEWPORT*>(data));break;case 1:scissorsHook(list,count,static_cast<const D3D12_RECT*>(data));break;case 2:targetsHook(list,count,static_cast<const D3D12_CPU_DESCRIPTOR_HANDLE*>(data),FALSE,nullptr);break;case 3:heapsHook(list,count,static_cast<ID3D12DescriptorHeap*const*>(data));break;}return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
int main(){
    auto list=reinterpret_cast<ID3D12GraphicsCommandList*>(std::uintptr_t(0x1000));auto& state=stereo::lists[list];state.viewportCount=3;state.scissorCount=2;state.rtvCount=1;const auto before=state;
    originalViewports=+[](ID3D12GraphicsCommandList*,UINT n,const D3D12_VIEWPORT* data){check(n,data,0);};originalScissors=+[](ID3D12GraphicsCommandList*,UINT n,const D3D12_RECT* data){check(n,data,1);};originalTargets=+[](ID3D12GraphicsCommandList*,UINT n,const D3D12_CPU_DESCRIPTOR_HANDLE* data,BOOL contiguous,const D3D12_CPU_DESCRIPTOR_HANDLE* depth){if(contiguous!=expectedContiguous||depth)std::terminate();check(n,data,2);};originalSetHeaps=+[](ID3D12GraphicsCommandList*,UINT n,ID3D12DescriptorHeap*const* data){check(n,data,3);};
    unsigned failures{};expectedCount=1;expectedData=nullptr;
    for(unsigned kind=0;kind<4;++kind){if(!invoke(kind,list,1,nullptr)){std::printf("FAIL: binding tracker kind%u raised SEH before original null-array forwarding\n",kind);++failures;}else if(calls[kind]!=1||state.viewportCount!=before.viewportCount||state.scissorCount!=before.scissorCount||state.rtvCount!=before.rtvCount){++failures;}state=before;}
    ID3D12DescriptorHeap* nullEntry[]={nullptr};ID3D12DescriptorHeap* laterNull[]={reinterpret_cast<ID3D12DescriptorHeap*>(std::uintptr_t(1)),nullptr};
    for(auto data:{nullEntry,laterNull}){expectedData=data;expectedCount=data==nullEntry?1:2;const auto previous=calls[3];if(!invoke(3,list,expectedCount,data)||calls[3]!=previous+1){std::puts("FAIL: heap null-entry validation queried an invalid object before original forwarding");++failures;}}
    expectedData=nullptr;expectedCount=UINT_MAX;const auto previous=calls[3];if(!invoke(3,list,UINT_MAX,nullptr)||calls[3]!=previous+1)++failures;
    if(failures)return 1;
    expectedCount=0;for(unsigned kind=0;kind<4;++kind){if(!invoke(kind,list,0,nullptr))return 1;}if(state.viewportCount||state.scissorCount||state.rtvCount||!state.heaps.empty())return 1;
    std::array<void*,9> table{};table[7]=reinterpret_cast<void*>(missingDevice);struct FakeList {void** vtable;} fake{table.data()};auto unavailable=reinterpret_cast<ID3D12GraphicsCommandList*>(&fake);auto& unchanged=stereo::lists[unavailable];unchanged.rtvCount=4;D3D12_CPU_DESCRIPTOR_HANDLE handle{123};expectedCount=1;expectedData=&handle;expectedContiguous=TRUE;
    for(auto result:{E_FAIL,S_OK}){queryResult=result;const auto priorTargetCalls=calls[2];if(!contiguousTarget(unavailable,&handle)||calls[2]!=priorTargetCalls+1||unchanged.rtvCount!=4)return 1;}if(deviceQueries!=2)return 1;
    originalViewports=nullptr;originalScissors=nullptr;originalTargets=nullptr;originalSetHeaps=nullptr;std::puts("PASS: known-null/oversized binding arrays and null heap entries bypass metadata queries and forward exact originals once; valid zero-count unbinds preserved; no COM/GPU/game/XR.");return 0;
}

