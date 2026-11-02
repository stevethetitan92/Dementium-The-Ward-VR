#pragma once
#include <d3d12.h>
#include "render_history.hpp"
namespace copyProvenance {
struct Trace {
    ID3D12GraphicsCommandList* list{};D3D12_COMMAND_LIST_TYPE queueType{};
    D3D12_TEXTURE_COPY_LOCATION source{},destination{};UINT x{},y{},z{};
    D3D12_BOX box{};bool hasBox{},injected{};
    renderHistory::Stage stage{};unsigned recordingThread{};
};
using History=renderHistory::History<16,Trace>;
struct BarrierTrace {ID3D12GraphicsCommandList* list{};std::uintptr_t caller{};D3D12_RESOURCE_BARRIER barrier{};UINT requested{},index{};bool known{},injected{};renderHistory::Stage stage{};};
using BarrierHistory=renderHistory::History<16,BarrierTrace>;
struct SubmittedList {const void* identity{};bool known{},barriersKnown{};History copies;BarrierHistory barriers;};
struct Submission {
    const void* queue{};unsigned queueType{},thread{},requested{},captured{};
    std::array<SubmittedList,8> lists;
};
inline thread_local const Submission* active{};
// An Execute callback can run on a different thread from recording. Snapshot
// closed-list evidence before the native call, not from that thread's history.
// Only raw identities/value records are retained, for the duration of the call.
struct Scope {
    Submission value;const Submission* previous=active;
    template<class Lookup> Scope(const void* queue,unsigned type,unsigned thread,UINT count,ID3D12CommandList*const* lists,Lookup lookup):Scope(queue,type,thread,count,lists,lookup,[](ID3D12CommandList*)->const BarrierHistory*{return nullptr;}){}
    template<class Lookup,class BarrierLookup> Scope(const void* queue,unsigned type,unsigned thread,UINT count,ID3D12CommandList*const* lists,Lookup lookup,BarrierLookup barrierLookup){
        value.queue=queue;value.queueType=type;value.thread=thread;value.requested=count;
        value.captured=lists?(count<value.lists.size()?count:static_cast<unsigned>(value.lists.size())):0;
        for(unsigned i=0;i<value.captured;++i){auto& output=value.lists[i];output.identity=lists[i];if(const auto* copies=lookup(lists[i])){output.known=true;output.copies=*copies;}if(const auto* barriers=barrierLookup(lists[i])){output.barriersKnown=true;output.barriers=*barriers;}}
        active=&value;
    }
    Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
    ~Scope(){active=previous;}
};
}
