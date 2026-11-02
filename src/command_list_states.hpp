#pragma once
#include <map>
#include <mutex>
namespace commandListStates {
// Native recording of the SAME command list must already be serialized by its
// caller. Protect registry lookup/insertion for DIFFERENT lists; never retain
// this lock across D3D12 calls or callbacks. std::map keeps references stable.
// State follows the command-list object through sequential thread handoff.
template<class Key,class State> class Registry {
    std::mutex mutex;
    std::map<Key,State> entries;
public:
    State& operator[](Key key){std::lock_guard<std::mutex> guard(mutex);return entries[key];}
    State* find(Key key){std::lock_guard<std::mutex> guard(mutex);auto found=entries.find(key);return found==entries.end()?nullptr:&found->second;}
    // Keep the stable node if a native object address is reused. Creation and
    // successful Reset clear its contents; failed calls preserve prior state.
    void reset(Key key){(*this)[key]=State{};}
};
}
