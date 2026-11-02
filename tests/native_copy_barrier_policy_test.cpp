#include "../src/native_copy_barrier_policy.hpp"
#include <cstdio>
#include <stdexcept>
using namespace nativeCopyBarrierPolicy;
static void require(bool value){if(!value)throw std::runtime_error("native copy barrier classification check failed");}
int main(){try{
    Proof proof{true,true,true,true};D3D12_RESOURCE_DESC resource{};resource.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;resource.SampleDesc.Count=1;
    const std::uintptr_t sites[]={0x1eabf1,0x1eac72,0x1ea8eb,0x1ea95a};const Site roles[]={Site::UploadBegin,Site::UploadEnd,Site::ReadbackBegin,Site::ReadbackEnd};unsigned compared{};
    for(unsigned index=0;index<4;++index){
        D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition.pResource=reinterpret_cast<ID3D12Resource*>(std::uintptr_t(1));barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        const auto copyState=index<2?D3D12_RESOURCE_STATE_COPY_DEST:D3D12_RESOURCE_STATE_COPY_SOURCE;barrier.Transition.StateBefore=(index&1)?copyState:D3D12_RESOURCE_STATE_COMMON;barrier.Transition.StateAfter=(index&1)?D3D12_RESOURCE_STATE_COMMON:copyState;
        require(classify(proof,sites[index],resource,1,&barrier)==roles[index]);++compared;
        for(unsigned mutation=0;mutation<20;++mutation){auto p=proof;auto d=resource;auto b=barrier;auto caller=sites[index];UINT count=1;const D3D12_RESOURCE_BARRIER* input=&b;
            switch(mutation){
            case 0:p.signaturesVerified=false;break;case 1:p.nativeCopyList=false;break;case 2:p.copyQueue=false;break;case 3:p.resourceDescriptionKnown=false;break;
            case 4:d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;break;case 5:d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE3D;break;
            case 6:d.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;break;case 7:d.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;break;case 8:d.Flags=D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;break;
            case 9:d.SampleDesc.Count=4;break;case 10:count=0;break;case 11:count=2;break;case 12:input=nullptr;break;
            case 13:b.Type=D3D12_RESOURCE_BARRIER_TYPE_ALIASING;break;case 14:b.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;break;
            case 15:b.Flags=D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;break;case 16:b.Flags=D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;break;
            case 17:b.Transition.pResource=nullptr;break;case 18:b.Transition.Subresource=0;break;case 19:++caller;break;
            }
            require(classify(p,caller,d,count,input)==Site::None);++compared;
        }
        auto other=barrier;other.Transition.StateBefore=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;require(classify(proof,sites[index],resource,1,&other)==Site::None);++compared;
        other=barrier;other.Transition.StateAfter=D3D12_RESOURCE_STATE_GENERIC_READ;require(classify(proof,sites[index],resource,1,&other)==Site::None);++compared;
        for(unsigned otherSite=0;otherSite<4;++otherSite)if(otherSite!=index){require(classify(proof,sites[otherSite],resource,1,&barrier)==Site::None);++compared;}
    }
    std::printf("PASS: %u exact-site COPY transition classifications reject unproven ownership/signatures, DIRECT/unknown lists, special resources, split/alias/UAV barriers, wrong roles/states and unrelated calls; classification only, no commands changed.\n",compared);return 0;
}catch(const std::exception& error){std::puts(error.what());return 1;}}
