#pragma once
#include "native_copy_session.hpp"
namespace nativeCopyDispatch {
// Unused hook bodies. Emitters are original functions bound to the incoming
// list. Other command methods still require session.intervening before forward.
// deadline=0 disables new admission; pending sequences still complete/replay.
template<class Admission,class Barrier,class Copy> void barrier(nativeCopySession::Session& session,ID3D12GraphicsCommandList* list,std::uint64_t caller,std::uint64_t now,std::uint64_t deadline,UINT count,const D3D12_RESOURCE_BARRIER* barriers,Admission admit,Barrier originalBarrier,Copy originalCopy){
    if(!session.owns(list)){originalBarrier(count,barriers);return;}
    auto lifetime=session.retain();
    auto emitBarrier=[&](const D3D12_RESOURCE_BARRIER& value){originalBarrier(1,&value);};
    auto emitCopy=[&](const D3D12_TEXTURE_COPY_LOCATION& dst,const D3D12_TEXTURE_COPY_LOCATION& src,const D3D12_BOX& region){originalCopy(&dst,0,0,0,&src,&region);};
    const bool pending=session.pending();
    if(pending){if(session.end(caller,now,count,barriers,emitBarrier,emitCopy))return;}
    else if(session.canBegin(list)&&deadline&&now<deadline&&count==1&&barriers){auto plan=admit();if(session.begin(plan,list,now,deadline,*barriers,emitBarrier,emitCopy))return;}
    session.intervening(emitBarrier,emitCopy,[&]{originalBarrier(count,barriers);});
}
template<class Barrier,class Copy> void copy(nativeCopySession::Session& session,ID3D12GraphicsCommandList* list,std::uint64_t caller,std::uint64_t now,const D3D12_TEXTURE_COPY_LOCATION* destination,UINT x,UINT y,UINT z,const D3D12_TEXTURE_COPY_LOCATION* source,const D3D12_BOX* box,Barrier originalBarrier,Copy originalCopy){
    if(!session.owns(list)){originalCopy(destination,x,y,z,source,box);return;}
    auto lifetime=session.retain();
    auto emitBarrier=[&](const D3D12_RESOURCE_BARRIER& value){originalBarrier(1,&value);};
    auto emitCopy=[&](const D3D12_TEXTURE_COPY_LOCATION& dst,const D3D12_TEXTURE_COPY_LOCATION& src,const D3D12_BOX& region){originalCopy(&dst,0,0,0,&src,&region);};
    if(session.copy(caller,now,destination,x,y,z,source,box,emitBarrier,emitCopy))return;
    session.intervening(emitBarrier,emitCopy,[&]{originalCopy(destination,x,y,z,source,box);});
}
}
