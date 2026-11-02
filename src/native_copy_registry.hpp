#pragma once
#include "native_copy_session.hpp"
#include <map>
#include <mutex>
namespace nativeCopyRegistry {
// Unused bounded session registry. Only observed live COPY-list creations or
// successful Resets establish recording identity. Same-list native recording
// remains caller-serialized. Nodes never erase, preserving handoff references.
// Finish/flush or establish discarded recordings before registry destruction.
template<size_t Limit=256> class Registry {
    std::mutex mutex;std::map<ID3D12GraphicsCommandList*,nativeCopySession::Session> entries;nativeCopyEpoch::Source epochs;
    nativeCopySession::Session* acquire(ID3D12GraphicsCommandList* list){
        if(!list||list->GetType()!=D3D12_COMMAND_LIST_TYPE_COPY)return nullptr;
        std::lock_guard<std::mutex> guard(mutex);auto found=entries.find(list);if(found!=entries.end())return &found->second;if(entries.size()>=Limit)return nullptr;return &entries.try_emplace(list).first->second;
    }
public:
    nativeCopySession::Session* find(ID3D12GraphicsCommandList* list){std::lock_guard<std::mutex> guard(mutex);auto found=entries.find(list);return found==entries.end()?nullptr:&found->second;}
    size_t size(){std::lock_guard<std::mutex> guard(mutex);return entries.size();}
    // Recording MUST be quiescent: the registry mutex protects node storage,
    // not per-list recording state. Nonzero forbids detour/runtime teardown;
    // cancellation alone does not discard or flush a held native prefix.
    size_t pendingRecordings(){std::lock_guard<std::mutex> guard(mutex);size_t count{};for(const auto& entry:entries)if(entry.second.pending())++count;return count;}
    bool created(ID3D12GraphicsCommandList* list){auto state=acquire(list);return state&&state->created(epochs,list);}
    template<class Forward> HRESULT reset(ID3D12GraphicsCommandList* list,Forward forward){
        if(auto state=find(list))return state->reset(epochs,forward);
        const auto result=forward();if(SUCCEEDED(result))created(list);return result;
    }
    template<class Barrier,class Copy,class Forward> HRESULT close(ID3D12GraphicsCommandList* list,Barrier barrier,Copy copy,Forward forward){if(auto state=find(list))return state->close(barrier,copy,forward);return forward();}
    template<class Barrier,class Copy,class Forward> void intervening(ID3D12GraphicsCommandList* list,Barrier barrier,Copy copy,Forward forward){if(auto state=find(list))state->intervening(barrier,copy,forward);else forward();}
};
}
