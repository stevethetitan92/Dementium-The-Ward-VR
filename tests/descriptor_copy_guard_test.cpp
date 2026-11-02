#define NOMINMAX
#include "../src/bridge.cpp"
#include <cstdio>
#include <cstdlib>
static unsigned forwards{},simpleForwards{},strideQueries{};
struct Expected {UINT dn{},sn{};const D3D12_CPU_DESCRIPTOR_HANDLE* dest{};const UINT* ds{};const D3D12_CPU_DESCRIPTOR_HANDLE* source{};const UINT* ss{};D3D12_DESCRIPTOR_HEAP_TYPE type{};} expectedDescriptorCall;
static UINT STDMETHODCALLTYPE stride(ID3D12Device*,D3D12_DESCRIPTOR_HEAP_TYPE){++strideQueries;return 32;}
static void STDMETHODCALLTYPE original(ID3D12Device*,UINT dn,const D3D12_CPU_DESCRIPTOR_HANDLE* dest,const UINT* ds,UINT sn,const D3D12_CPU_DESCRIPTOR_HANDLE* source,const UINT* ss,D3D12_DESCRIPTOR_HEAP_TYPE type){if(dn!=expectedDescriptorCall.dn||dest!=expectedDescriptorCall.dest||ds!=expectedDescriptorCall.ds||sn!=expectedDescriptorCall.sn||source!=expectedDescriptorCall.source||ss!=expectedDescriptorCall.ss||type!=expectedDescriptorCall.type)std::terminate();++forwards;}
static bool invoke(ID3D12Device* device){__try{copyDescriptorsHook(device,expectedDescriptorCall.dn,expectedDescriptorCall.dest,expectedDescriptorCall.ds,expectedDescriptorCall.sn,expectedDescriptorCall.source,expectedDescriptorCall.ss,expectedDescriptorCall.type);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static void run(ID3D12Device* device,Expected arguments){expectedDescriptorCall=arguments;if(!invoke(device)){std::printf("FAIL: descriptor metadata raised SEH AFTER original forwarding count=%u\n",forwards);std::fflush(stdout);std::_Exit(1);}}
int main(){
    std::array<void*,16> table{};table[15]=reinterpret_cast<void*>(stride);struct Device {void** vtable;} fake{table.data()};auto device=reinterpret_cast<ID3D12Device*>(&fake);originalCopyDescriptors=original;
    D3D12_CPU_DESCRIPTOR_HANDLE source{1000},destination{2000};auto resource=reinterpret_cast<ID3D12Resource*>(std::uintptr_t(1));auto oldResource=reinterpret_cast<ID3D12Resource*>(std::uintptr_t(2));stereo::sampledResources[source.ptr]=resource;stereo::sampledResources[destination.ptr]=oldResource;const auto type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    run(device,{1,1,nullptr,nullptr,&source,nullptr,type});if(forwards!=1||strideQueries||stereo::sampledResources.at(destination.ptr)!=oldResource)return 1;
    run(device,{1,1,&destination,nullptr,nullptr,nullptr,type});if(forwards!=2||strideQueries||stereo::sampledResources.at(destination.ptr)!=oldResource)return 1;
    run(device,{0,0,nullptr,nullptr,nullptr,nullptr,type});if(forwards!=3||strideQueries)return 1;
    run(device,{1,1,&destination,nullptr,&source,nullptr,type});if(forwards!=4||strideQueries!=1||stereo::sampledResources.at(destination.ptr)!=resource)return 1;
    const UINT one=1;run(device,{1,1,&destination,&one,&source,&one,type});if(forwards!=5||strideQueries!=2)return 1;
    internal=true;stereo::sampledResources[source.ptr]=oldResource;run(device,{1,1,&destination,nullptr,&source,nullptr,type});internal=false;if(forwards!=6||strideQueries!=2||stereo::sampledResources.at(destination.ptr)!=resource)return 1;
    run(device,{UINT_MAX,UINT_MAX,nullptr,nullptr,nullptr,nullptr,type});if(forwards!=7||strideQueries!=2)return 1;
    run(device,{1,1,nullptr,nullptr,nullptr,nullptr,D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER});if(forwards!=8||strideQueries!=2)return 1;
    const UINT two=2;run(device,{1,1,&destination,&two,&source,&one,type});if(forwards!=9||strideQueries!=2||stereo::sampledResources.at(destination.ptr)!=resource){std::puts("FAIL: mismatched descriptor totals partially changed metadata or queried device");return 1;}
    run(device,{1,1,&destination,&one,&source,&two,type});if(forwards!=10||strideQueries!=2||stereo::sampledResources.at(destination.ptr)!=resource)return 1;
    const UINT zero=0;run(device,{1,1,&destination,&zero,&source,&zero,type});if(forwards!=11||strideQueries!=2)return 1;
    run(device,{UINT_MAX,UINT_MAX-1,&destination,nullptr,&source,nullptr,type});if(forwards!=12||strideQueries!=2)return 1;
    const UINT huge[]={UINT_MAX,UINT_MAX};if(descriptorRangeTotal(2,huge)!=UINT64(8589934590)||descriptorRangeTotal(UINT_MAX,nullptr)!=UINT_MAX)return 1;
    D3D12_CPU_DESCRIPTOR_HANDLE sparseSource[]={{1000},{1200}};auto third=reinterpret_cast<ID3D12Resource*>(std::uintptr_t(3));auto fourth=reinterpret_cast<ID3D12Resource*>(std::uintptr_t(4));stereo::sampledResources[1200]=third;
    run(device,{1,2,&destination,&two,sparseSource,nullptr,type});if(forwards!=13||strideQueries!=3||stereo::sampledResources.at(2000)!=oldResource||stereo::sampledResources.at(2032)!=third)return 1;
    D3D12_CPU_DESCRIPTOR_HANDLE sparseDestination[]={{3000},{4000}};stereo::sampledResources[1032]=fourth;
    run(device,{2,1,sparseDestination,nullptr,&source,&two,type});if(forwards!=14||strideQueries!=4||stereo::sampledResources.at(3000)!=oldResource||stereo::sampledResources.at(4000)!=fourth)return 1;
    D3D12_CPU_DESCRIPTOR_HANDLE withEmpty[]={{1000},{1100},{1200}};const UINT sourceSizes[]={1,0,1};run(device,{1,3,&destination,&two,withEmpty,sourceSizes,type});if(forwards!=15||strideQueries!=5||stereo::sampledResources.at(2000)!=oldResource||stereo::sampledResources.at(2032)!=third)return 1;
    originalCopySimple=+[](ID3D12Device*,UINT n,D3D12_CPU_DESCRIPTOR_HANDLE dest,D3D12_CPU_DESCRIPTOR_HANDLE src,D3D12_DESCRIPTOR_HEAP_TYPE kind){if(n||dest.ptr!=2000||src.ptr!=1000||kind!=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)std::terminate();++simpleForwards;};copySimpleHook(device,0,destination,source,type);if(simpleForwards!=1||strideQueries!=5)return 1;
    originalCopyDescriptors=nullptr;originalCopySimple=nullptr;std::puts("PASS: descriptor metadata guards required-null/empty/mismatched totals before shadow writes; optional sizes, sparse/contiguous/empty-interior layouts,64-bit counts and injected isolation preserved; exact originals; COM-shaped stubs only, no driver/GPU/game/XR.");return 0;
}


