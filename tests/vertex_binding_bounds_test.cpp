#define NOMINMAX
#include "../src/bridge.cpp"
#include <cstdio>
static unsigned forwards{};static UINT expectedStart{},expectedCount{};static const D3D12_VERTEX_BUFFER_VIEW* expectedViews;
static void STDMETHODCALLTYPE original(ID3D12GraphicsCommandList*,UINT start,UINT count,const D3D12_VERTEX_BUFFER_VIEW* views){if(start!=expectedStart||count!=expectedCount||views!=expectedViews)std::terminate();++forwards;}
// Isolated SEH boundary lets the before-fix invalid-slot write be reported
// without invoking a native driver or generating an OS crash dialog.
static bool invoke(ID3D12GraphicsCommandList* list,UINT start,UINT count,const D3D12_VERTEX_BUFFER_VIEW* views){
    __try{verticesHook(list,start,count,views);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
int main(){
    auto list=reinterpret_cast<ID3D12GraphicsCommandList*>(std::uintptr_t(0x1000));auto& state=stereo::lists[list];for(auto& view:state.vertices)view={123,456,16};const auto before=state.vertices;originalVertexBuffers=original;
    for(auto pair:{std::pair<UINT,UINT>{UINT_MAX,1},{UINT_MAX,2},{1,UINT_MAX},{33,0},{0,33}}){expectedStart=pair.first;expectedCount=pair.second;expectedViews=nullptr;if(!invoke(list,pair.first,pair.second,nullptr)){std::puts("FAIL: vertex binding tracker raised SEH before original forwarding for overflowing/out-of-range slot range");return 1;}if(std::memcmp(before.data(),state.vertices.data(),sizeof before)){std::puts("FAIL: invalid range changed tracked vertex bindings");return 1;}}
    if(forwards!=5)return 1;
    D3D12_VERTEX_BUFFER_VIEW last{789,128,32};expectedStart=31;expectedCount=1;expectedViews=&last;if(!invoke(list,31,1,&last)||state.vertices[31].BufferLocation!=789||forwards!=6)return 1;
    expectedStart=0;expectedCount=32;expectedViews=nullptr;if(!invoke(list,0,32,nullptr)||forwards!=7)return 1;for(const auto& view:state.vertices)if(view.BufferLocation||view.SizeInBytes||view.StrideInBytes)return 1;
    internal=true;expectedStart=UINT_MAX;expectedCount=1;if(!invoke(list,UINT_MAX,1,nullptr)||forwards!=8)return 1;internal=false;
    expectedStart=32;expectedCount=0;if(!invoke(list,32,0,nullptr)||forwards!=9)return 1;for(const auto& view:state.vertices)if(view.BufferLocation||view.SizeInBytes||view.StrideInBytes)return 1;
    originalVertexBuffers=nullptr;std::puts("PASS: vertex tracker rejects overflow/out-of-range metadata writes, forwards exact original arguments once, preserves valid last-slot/full-unbind behavior; stub originals, no COM/GPU/game/XR.");return 0;
}
