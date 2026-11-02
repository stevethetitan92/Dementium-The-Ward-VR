#pragma once
#include "native_copy_preflight.hpp"
namespace nativeCopySequence {
// Diagnostic matcher only: completion is learned AFTER the End. It cannot be
// used alone to omit an earlier Begin barrier or repair an aborted recording.
class Matcher {
    enum class Stage {Idle,Prepared,Copied};Stage stage=Stage::Idle;
    nativeCopyPreflight::Plan plan;const void* owner{};std::uint64_t generation{},started{},until{};
    static bool location(const D3D12_TEXTURE_COPY_LOCATION& a,const D3D12_TEXTURE_COPY_LOCATION& b){
        if(a.pResource!=b.pResource||a.Type!=b.Type)return false;
        if(a.Type==D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX)return a.SubresourceIndex==b.SubresourceIndex;
        if(a.Type!=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT)return false;
        const auto& x=a.PlacedFootprint;const auto& y=b.PlacedFootprint;return x.Offset==y.Offset&&x.Footprint.Format==y.Footprint.Format&&x.Footprint.Width==y.Footprint.Width&&x.Footprint.Height==y.Footprint.Height&&x.Footprint.Depth==y.Footprint.Depth&&x.Footprint.RowPitch==y.Footprint.RowPitch;
    }
    bool context(const void* list,std::uint64_t epoch,std::uint64_t now) const{return owner==list&&generation==epoch&&now>=started&&now<until;}
public:
    void reset(){stage=Stage::Idle;plan={};owner=nullptr;generation=started=until=0;}
    bool begin(const nativeCopyPreflight::Plan& input,const void* list,std::uint64_t epoch,std::uint64_t now,std::uint64_t deadline){
        if(stage!=Stage::Idle){reset();return false;}
        if(!input.valid||!list||!epoch||now>=deadline)return false;
        const bool upload=input.site==nativeCopyBarrierPolicy::Site::UploadBegin;
        if((!upload&&input.site!=nativeCopyBarrierPolicy::Site::ReadbackBegin)||input.copyReturnRva!=(upload?0x1eac36:0x1ea920)||input.endReturnRva!=(upload?0x1eac72:0x1ea95a))return false;
        plan=input;owner=list;generation=epoch;started=now;until=deadline;stage=Stage::Prepared;return true;
    }
    bool copy(const void* list,std::uint64_t epoch,std::uint64_t caller,std::uint64_t now,const D3D12_TEXTURE_COPY_LOCATION* destination,UINT x,UINT y,UINT z,const D3D12_TEXTURE_COPY_LOCATION* source,const D3D12_BOX* box){
        const auto& expected=plan.box;
        if(stage!=Stage::Prepared||!context(list,epoch,now)||caller!=plan.copyReturnRva||!destination||!source||!box||x||y||z||
           !location(*destination,plan.destination)||!location(*source,plan.source)||box->left!=expected.left||box->top!=expected.top||box->front!=expected.front||box->right!=expected.right||box->bottom!=expected.bottom||box->back!=expected.back){reset();return false;}
        stage=Stage::Copied;return true;
    }
    bool end(const void* list,std::uint64_t epoch,std::uint64_t caller,std::uint64_t now,UINT count,const D3D12_RESOURCE_BARRIER* barriers){
        bool matched=false;
        if(stage==Stage::Copied&&context(list,epoch,now)&&caller==plan.endReturnRva&&count==1&&barriers&&barriers->Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION&&barriers->Flags==D3D12_RESOURCE_BARRIER_FLAG_NONE){
            const bool upload=plan.site==nativeCopyBarrierPolicy::Site::UploadBegin;const auto& transition=barriers->Transition;
            matched=transition.pResource==(upload?plan.destination.pResource:plan.source.pResource)&&transition.Subresource==D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES&&transition.StateBefore==(upload?D3D12_RESOURCE_STATE_COPY_DEST:D3D12_RESOURCE_STATE_COPY_SOURCE)&&transition.StateAfter==D3D12_RESOURCE_STATE_COMMON;
        }
        reset();return matched;
    }
};
}
