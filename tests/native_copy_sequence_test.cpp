#include "../src/native_copy_sequence.hpp"
#include <cstdio>
#include <stdexcept>
using namespace nativeCopySequence;
static void require(bool value){if(!value)throw std::runtime_error("native copy sequence check failed");}
int main(){try{
    unsigned cases{};
    for(bool upload:{true,false})for(unsigned mutation=0;mutation<27;++mutation){
        nativeCopyPreflight::Plan plan;plan.valid=true;plan.site=upload?nativeCopyBarrierPolicy::Site::UploadBegin:nativeCopyBarrierPolicy::Site::ReadbackBegin;plan.copyReturnRva=upload?0x1eac36:0x1ea920;plan.endReturnRva=upload?0x1eac72:0x1ea95a;
        D3D12_TEXTURE_COPY_LOCATION image{},linear{};image.pResource=reinterpret_cast<ID3D12Resource*>(std::uintptr_t(1));image.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;linear.pResource=reinterpret_cast<ID3D12Resource*>(std::uintptr_t(2));linear.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;linear.PlacedFootprint.Footprint={DXGI_FORMAT_R8G8B8A8_UNORM,8,8,1,256};plan.source=upload?linear:image;plan.destination=upload?image:linear;plan.box={0,0,0,8,8,1};
        D3D12_RESOURCE_BARRIER end{};end.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;end.Transition={image.pResource,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,upload?D3D12_RESOURCE_STATE_COPY_DEST:D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_COMMON};
        int list{},other{};Matcher matcher;
        if(mutation==1){auto invalid=plan;invalid.valid=false;require(!matcher.begin(invalid,&list,1,10,100));require(!matcher.end(&list,1,plan.endReturnRva,30,1,&end));++cases;continue;}
        require(matcher.begin(plan,&list,1,10,100));auto source=plan.source,destination=plan.destination;auto box=plan.box;auto copyCaller=plan.copyReturnRva;const void* owner=&list;std::uint64_t epoch=1,time=20;UINT x{};const D3D12_BOX* inputBox=&box;
        switch(mutation){case 2:owner=&other;break;case 3:epoch=2;break;case 4:++copyCaller;break;case 5:time=100;break;case 6:time=9;break;case 7:x=1;break;case 8:source.pResource=image.pResource==source.pResource?linear.pResource:image.pResource;break;case 9:destination.Type=destination.Type==D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT?D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX:D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;break;case 10:inputBox=nullptr;break;case 11:box.right=7;break;case 12:matcher.reset();break;case 13:require(!matcher.end(&list,1,plan.endReturnRva,15,1,&end));break;case 14:require(!matcher.begin(plan,&list,1,11,100));break;}
        const bool copied=matcher.copy(owner,epoch,copyCaller,time,&destination,x,0,0,&source,inputBox);
        if(mutation>=2&&mutation<=14)require(!copied);else require(copied);
        if(mutation==15)require(!matcher.copy(&list,1,copyCaller,21,&destination,0,0,0,&source,&box));
        auto finalCaller=plan.endReturnRva;std::uint64_t finalEpoch=1,finalTime=30;if(mutation==16)++finalCaller;if(mutation==17)finalEpoch=2;if(mutation==18)finalTime=100;
        UINT finalCount=1;const D3D12_RESOURCE_BARRIER* finalInput=&end;switch(mutation){case 19:end.Transition.pResource=linear.pResource;break;case 20:end.Flags=D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;break;case 21:end.Transition.Subresource=0;break;case 22:end.Transition.StateBefore=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;break;case 23:end.Transition.StateAfter=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;break;case 24:finalCount=2;break;case 25:finalInput=nullptr;break;case 26:end.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;break;}require(matcher.end(&list,finalEpoch,finalCaller,finalTime,finalCount,finalInput)==(mutation==0));require(!matcher.end(&list,1,plan.endReturnRva,31,1,&end));++cases;
    }
    std::printf("PASS: %u complete upload/readback matcher cases reject missing/duplicate/wrong/expired/reset generations and early End; diagnostic completion only, no commands changed.\n",cases);return 0;
}catch(const std::exception& error){std::puts(error.what());return 1;}}


