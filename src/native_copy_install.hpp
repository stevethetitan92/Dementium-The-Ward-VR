#pragma once
#include <array>
#include <cstddef>
#include <exception>
namespace nativeCopyInstall {
struct Binding {void* target{};void* detour{};void** original{};};
// Unused transactional registration helper. Backend must remain initialized,
// callbacks/storage stable and recording quiescent through install/rollback.
// Backend create failure must not leave a created hook; cleanup returns success
// without throwing. Failed cleanup retains trampoline storage for explicit retry.
// Unresolved cleanup at destruction fails fast rather than discard active hooks.
// Never uses global enable/remove-all, so unrelated hooks remain untouched.
template<class Backend,size_t Count=47> class Batch {
    Backend backend;std::array<Binding,Count> bindings{};std::array<void*,Count> previous{};std::array<bool,Count> owned{};bool configured{},committed{};
public:
    explicit Batch(Backend value):backend(value){}
    Batch(const Batch&)=delete;Batch& operator=(const Batch&)=delete;
    ~Batch(){if(!rollback())std::terminate();}
    bool installed()const{return committed;}
    bool pendingCleanup()const{return configured&&!committed;}
    bool rollback()noexcept{
        if(!configured)return true;
        committed=false;bool complete=true;
        for(size_t i=Count;i-->0;)if(owned[i]){auto& binding=bindings[i];if(!backend.disable(binding.target)||!backend.remove(binding.target)){complete=false;continue;}owned[i]=false;}
        for(size_t i=0;i<Count;++i)if(!owned[i])*bindings[i].original=previous[i];configured=!complete;return complete;
    }
    bool install(const std::array<Binding,Count>& requested){
        if(configured)return false;
        for(size_t i=0;i<Count;++i){const auto& b=requested[i];if(!b.target||!b.detour||!b.original)return false;for(size_t j=0;j<i;++j)if(b.target==requested[j].target||b.original==requested[j].original)return false;}
        bindings=requested;for(size_t i=0;i<Count;++i)previous[i]=*bindings[i].original;configured=true;
        for(size_t i=0;i<Count;++i){if(!backend.create(bindings[i])){rollback();return false;}owned[i]=true;}
        for(const auto& binding:bindings)if(!backend.enable(binding.target)){rollback();return false;}
        committed=true;return true;
    }
};
}
