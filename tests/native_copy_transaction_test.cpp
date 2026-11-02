#include "../src/native_copy_transaction.hpp"
#include <vector>
#include <cstdio>
#include <stdexcept>
static void require(bool value){if(!value)throw std::runtime_error("buffered copy transaction check failed");}
int main(){try{
    unsigned cases{};
    for(bool upload:{true,false})for(unsigned scenario=0;scenario<15;++scenario){
        nativeCopyPreflight::Plan plan;plan.valid=true;plan.site=upload?nativeCopyBarrierPolicy::Site::UploadBegin:nativeCopyBarrierPolicy::Site::ReadbackBegin;plan.copyReturnRva=upload?0x1eac36:0x1ea920;plan.endReturnRva=upload?0x1eac72:0x1ea95a;
        D3D12_TEXTURE_COPY_LOCATION image{},linear{};image.pResource=reinterpret_cast<ID3D12Resource*>(std::uintptr_t(1));image.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;linear.pResource=reinterpret_cast<ID3D12Resource*>(std::uintptr_t(2));linear.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;linear.PlacedFootprint.Footprint={DXGI_FORMAT_R8G8B8A8_UNORM,8,8,1,256};plan.source=upload?linear:image;plan.destination=upload?image:linear;plan.box={0,0,0,8,8,1};
        D3D12_RESOURCE_BARRIER begin{},end{};begin.Type=end.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;begin.Transition={image.pResource,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COMMON,upload?D3D12_RESOURCE_STATE_COPY_DEST:D3D12_RESOURCE_STATE_COPY_SOURCE};end=begin;end.Transition.StateBefore=begin.Transition.StateAfter;end.Transition.StateAfter=D3D12_RESOURCE_STATE_COMMON;
        int list{};nativeCopyTransaction::Buffer transaction;std::vector<unsigned> events;
        auto emitBarrier=[&](const D3D12_RESOURCE_BARRIER& barrier){require(!transaction.pending());require(barrier.Transition.pResource==image.pResource&&barrier.Transition.StateBefore==D3D12_RESOURCE_STATE_COMMON);events.push_back(1);};
        auto emitCopy=[&](const D3D12_TEXTURE_COPY_LOCATION& dst,const D3D12_TEXTURE_COPY_LOCATION& src,const D3D12_BOX& box){require(!transaction.pending()&&dst.pResource==plan.destination.pResource&&src.pResource==plan.source.pResource&&box.right==8);events.push_back(2);};
        if(scenario>=10){auto bad=begin;switch(scenario){case 10:bad.Transition.pResource=linear.pResource;break;case 11:bad.Flags=D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;break;case 12:bad.Transition.Subresource=0;break;case 13:bad.Transition.StateBefore=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;break;case 14:bad.Transition.StateAfter=D3D12_RESOURCE_STATE_GENERIC_READ;break;}require(!transaction.begin(plan,&list,1,10,15,bad,emitBarrier,emitCopy)&&events.empty());++cases;continue;}
        if(scenario==1){require(!transaction.begin(plan,&list,1,20,15,begin,emitBarrier,emitCopy)&&events.empty());++cases;continue;}
        require(transaction.begin(plan,&list,1,10,15,begin,emitBarrier,emitCopy)&&events.empty());
        if(scenario==2){transaction.flush(emitBarrier,emitCopy);require(events==std::vector<unsigned>{1}&&!transaction.pending());++cases;continue;}
        if(scenario==3){transaction.afterReset(true);require(events.empty()&&!transaction.pending());++cases;continue;}
        if(scenario==4){auto wrong=plan.source;wrong.pResource=nullptr;require(!transaction.copy(&list,1,plan.copyReturnRva,20,&plan.destination,0,0,0,&wrong,&plan.box,emitBarrier,emitCopy));require(events==std::vector<unsigned>{1});events.push_back(9);require(events==std::vector<unsigned>({1,9}));++cases;continue;}
        require(transaction.copy(&list,1,plan.copyReturnRva,20,&plan.destination,0,0,0,&plan.source,&plan.box,emitBarrier,emitCopy)&&events.empty());
        if(scenario==5){transaction.flush(emitBarrier,emitCopy);events.push_back(9);require(events==std::vector<unsigned>({1,2,9}));++cases;continue;}
        if(scenario==6){transaction.afterReset(true);require(events.empty()&&!transaction.pending());++cases;continue;}
        if(scenario==7){auto wrong=end;wrong.Transition.pResource=linear.pResource;require(!transaction.end(&list,1,plan.endReturnRva,30,1,&wrong,emitBarrier,emitCopy));events.push_back(3);require(events==std::vector<unsigned>({1,2,3}));++cases;continue;}
        if(scenario==8){bool threw=false;try{transaction.flush([&](const D3D12_RESOURCE_BARRIER&){require(!transaction.pending());throw std::runtime_error("emitter failure");},emitCopy);}catch(...){threw=true;}require(threw&&!transaction.pending()&&events.empty());transaction.flush(emitBarrier,emitCopy);require(events.empty());++cases;continue;}
        if(scenario==9){transaction.afterReset(false);require(transaction.pending()&&events.empty());} // Failed Reset preserves held commands.
        require(transaction.end(&list,1,plan.endReturnRva,30,1,&end,emitBarrier,emitCopy));require(events==std::vector<unsigned>{2}&&!transaction.pending());transaction.flush(emitBarrier,emitCopy);require(events.size()==1);++cases;
    }
    std::printf("PASS: %u buffered upload/readback transactions delay ALL emission until validation, preserve original-prefix ordering on mismatch/Close, discard only on successful Reset, finish admitted work after expiry and cannot double-replay after exceptions; no GPU commands.\n",cases);return 0;
}catch(const std::exception& error){std::puts(error.what());return 1;}}

