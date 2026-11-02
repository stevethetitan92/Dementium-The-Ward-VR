#define NOMINMAX
#include "../src/native_copy_observation.hpp"
#include <cstdio>
#include <stdexcept>
static void require(bool value){if(!value)throw std::runtime_error("observation provenance check failed");}
static nativeCopyObservation::Configuration actualConfig;
static unsigned factoryCalls,barrierCalls,copyCalls;
static const D3D12_RESOURCE_BARRIER* expectedBarrier;
static const D3D12_TEXTURE_COPY_LOCATION* expectedDestination;
static const D3D12_TEXTURE_COPY_LOCATION* expectedSource;
static const D3D12_BOX* expectedBox;
static nativeCopyEndpoints::Context actualFactory(const nativeCopyEndpoints::Observation& input){
    ++factoryCalls;require(input.caller!=0);auto result=actualConfig.capture(input,456);
    require(!result.read);return result;
}
static void STDMETHODCALLTYPE barrierOriginal(ID3D12GraphicsCommandList*,UINT count,const D3D12_RESOURCE_BARRIER* values){
    require(count==1&&values==expectedBarrier);++barrierCalls;
}
static void STDMETHODCALLTYPE copyOriginal(ID3D12GraphicsCommandList*,const D3D12_TEXTURE_COPY_LOCATION* destination,UINT x,UINT y,UINT z,const D3D12_TEXTURE_COPY_LOCATION* source,const D3D12_BOX* box){
    require(destination==expectedDestination&&source==expectedSource&&box==expectedBox&&x==7&&y==8&&z==9);++copyCalls;
}
int main(){try{
    using namespace nativeCopyEndpoints;
    constexpr std::uintptr_t base=0x10000000;
    auto device=reinterpret_cast<ID3D12Device*>(std::uintptr_t(1));
    auto list=reinterpret_cast<ID3D12GraphicsCommandList*>(std::uintptr_t(2));
    nativeCopyObservation::Configuration config{base,device};
    unsigned checks{};
    for(auto rva:{0x1eabf1,0x1eac72,0x1ea8eb,0x1ea95a,0x1eac36,0x1ea920}){
        const bool barrier=rva!=0x1eac36&&rva!=0x1ea920;
        Observation input{barrier?Kind::Barrier:Kind::Copy,base+rva,list,0xabcdef};
        auto output=config.capture(input,123);
        require(output.image==base&&output.device==device&&output.caller==input.caller&&output.frame==(barrier?input.frame:0)&&output.now==123&&output.read);
        unsigned value=73,copy{};require(output.read(reinterpret_cast<std::uintptr_t>(&value),&copy,sizeof copy,output.readerContext)&&copy==value);
        input.kind=barrier?Kind::Copy:Kind::Barrier;require(!config.capture(input,123).read);
        input.kind=barrier?Kind::Barrier:Kind::Copy;input.caller++;require(!config.capture(input,123).read);
        checks+=4;
    }
    Observation input{Kind::Barrier,base+0x1eabf1,list,0};require(!config.capture(input,1).read);
    input.frame=1;input.list=nullptr;require(!config.capture(input,1).read);input.list=list;
    input.caller=base-1;require(!config.capture(input,1).read);input.caller=base+0x1eabf1;
    require(!nativeCopyObservation::Configuration{}.capture(input,1).read);
    require(!nativeCopyObservation::Configuration{base,nullptr}.capture(input,1).read);
    input.kind=static_cast<Kind>(99);input.caller=base+0x1eac36;
    require(!config.capture(input,1).read);
    // Real ABI endpoint entry must reject this executable's unaudited caller,
    // preserving the original arguments without querying inert COM identities.
    nativeCopyRuntime::Runtime candidateRuntime;actualConfig=config;
    D3D12_RESOURCE_BARRIER value{};D3D12_TEXTURE_COPY_LOCATION destination{},source{};D3D12_BOX box{};
    expectedBarrier=&value;expectedDestination=&destination;expectedSource=&source;expectedBox=&box;
    nativeCopyEndpoints::runtime=&candidateRuntime;nativeCopyEndpoints::contextFactory=actualFactory;
    nativeCopyEndpoints::originalBarrier=barrierOriginal;nativeCopyEndpoints::originalCopy=copyOriginal;
    nativeCopyEndpoints::barrier(list,1,&value);nativeCopyEndpoints::copy(list,&destination,7,8,9,&source,&box);
    require(factoryCalls==2&&barrierCalls==1&&copyCalls==1&&!candidateRuntime.pending(list));
    unsigned interveningCalls{};
    auto invoke=+[](void* payload){++*static_cast<unsigned*>(payload);};
    nativeCopyEndpoints::intervening(list,invoke,&interveningCalls);require(interveningCalls==1);
    nativeCopyEndpoints::runtime=nullptr;
    nativeCopyEndpoints::intervening(list,invoke,&interveningCalls);require(interveningCalls==2);
    nativeCopyEndpoints::barrier(list,1,&value);nativeCopyEndpoints::copy(list,&destination,7,8,9,&source,&box);
    require(factoryCalls==2&&barrierCalls==2&&copyCalls==2);
    nativeCopyEndpoints::contextFactory=nullptr;nativeCopyEndpoints::originalBarrier=nullptr;nativeCopyEndpoints::originalCopy=nullptr;
    std::printf("PASS: %u provenance/reader checks plus actual ABI endpoint caller rejection/original argument forwarding and disabled bypass; no COM calls/game/XR.\n",checks+6);return 0;
}catch(const std::exception& e){std::puts(e.what());return 1;}}

