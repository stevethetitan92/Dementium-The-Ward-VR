#include "../src/native_copy_install.hpp"
#include <map>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
static void require(bool value){if(!value)throw std::runtime_error("copy hook batch rollback check failed");}
static void* pointer(unsigned n){return reinterpret_cast<void*>(std::uintptr_t(n));}
struct State {std::map<void*,bool> hooks;int failCreate=-1,failEnable=-1;void* failDisable{};void* failRemove{};unsigned creates{},enables{},removes{},disables{};};
struct Backend {
    State* state;
    bool create(const nativeCopyInstall::Binding& b){auto call=state->creates++;if(int(call)==state->failCreate||state->hooks.count(b.target))return false;state->hooks[b.target]=false;*b.original=pointer(0x8000+call);return true;}
    bool enable(void* target){if(int(state->enables++)==state->failEnable)return false;state->hooks.at(target)=true;return true;}
    bool disable(void* target)noexcept{++state->disables;if(target==state->failDisable)return false;state->hooks[target]=false;return true;}
    bool remove(void* target)noexcept{++state->removes;if(target==state->failRemove)return false;state->hooks.erase(target);return true;}
};
template<size_t Count> unsigned failureMatrix(){
    unsigned cases{};
    for(unsigned mode=0;mode<2;++mode)for(int failure=0;failure<int(Count);++failure){State state;state.hooks[pointer(0x77)]=true;if(mode)state.failEnable=failure;else state.failCreate=failure;std::array<void*,Count> original;std::array<nativeCopyInstall::Binding,Count> bindings;
        for(unsigned i=0;i<Count;++i){original[i]=pointer(0x4000+i);bindings[i]={pointer(0x1000+i*16),pointer(0x2000+i*16),&original[i]};}
        {nativeCopyInstall::Batch<Backend,Count> batch(Backend{&state});require(!batch.install(bindings)&&!batch.installed()&&!batch.pendingCleanup());require(state.hooks.size()==1&&state.hooks.at(pointer(0x77)));for(unsigned i=0;i<Count;++i)require(original[i]==pointer(0x4000+i));require(state.removes==unsigned(mode?Count:failure));}++cases;
    }
    return cases;
}
int main(){try{
    const auto cases=failureMatrix<47>()+failureMatrix<51>();require(cases==196);
    // Rollback itself can fail while recovering a failed installation. Keep
    // the outstanding native trampoline alive and reject a second install.
    for(bool enableFailure:{false,true})for(bool removeFailure:{false,true}){
        State cleanup;cleanup.hooks[pointer(0x77)]=true;
        std::array<void*,51> saved;std::array<nativeCopyInstall::Binding,51> request;
        for(unsigned i=0;i<51;++i){saved[i]=pointer(0x4000+i);request[i]={pointer(0x1000+i*16),pointer(0x2000+i*16),&saved[i]};}
        if(enableFailure)cleanup.failEnable=50;else cleanup.failCreate=50;
        if(removeFailure)cleanup.failRemove=request[7].target;else cleanup.failDisable=request[7].target;
        nativeCopyInstall::Batch<Backend,51> batch(Backend{&cleanup});
        require(!batch.install(request)&&batch.pendingCleanup()&&!batch.installed()&&cleanup.hooks.size()==2);
        require(saved[7]!=pointer(0x4007));for(unsigned i=0;i<51;++i)if(i!=7)require(saved[i]==pointer(0x4000+i));
        const auto creates=cleanup.creates;require(!batch.install(request)&&cleanup.creates==creates);
        cleanup.failDisable=cleanup.failRemove=nullptr;require(batch.rollback()&&!batch.pendingCleanup());
        require(cleanup.hooks.size()==1&&cleanup.hooks.at(pointer(0x77)));
        for(unsigned i=0;i<51;++i)require(saved[i]==pointer(0x4000+i));
    }
    for(bool removeFailure:{false,true})for(unsigned failure=0;failure<51;++failure){
        State cleanup;cleanup.hooks[pointer(0x77)]=true;
        std::array<void*,51> saved;std::array<nativeCopyInstall::Binding,51> request;
        for(unsigned i=0;i<51;++i){saved[i]=pointer(0x4000+i);request[i]={pointer(0x1000+i*16),pointer(0x2000+i*16),&saved[i]};}
        nativeCopyInstall::Batch<Backend,51> batch(Backend{&cleanup});require(batch.install(request));
        if(removeFailure)cleanup.failRemove=request[failure].target;else cleanup.failDisable=request[failure].target;
        require(!batch.rollback()&&batch.pendingCleanup()&&!batch.installed()&&cleanup.hooks.size()==2&&saved[failure]!=pointer(0x4000+failure));
        for(unsigned i=0;i<51;++i)if(i!=failure)require(saved[i]==pointer(0x4000+i));
        cleanup.failDisable=cleanup.failRemove=nullptr;
        require(batch.rollback()&&!batch.pendingCleanup()&&cleanup.hooks.size()==1&&cleanup.hooks.at(pointer(0x77)));
        for(unsigned i=0;i<51;++i)require(saved[i]==pointer(0x4000+i));
    }
    State state;state.hooks[pointer(0x77)]=true;std::array<void*,47> original{};std::array<nativeCopyInstall::Binding,47> bindings;for(unsigned i=0;i<47;++i)bindings[i]={pointer(0x1000+i*16),pointer(0x2000+i*16),&original[i]};
    {nativeCopyInstall::Batch<Backend> batch(Backend{&state});auto bad=bindings;bad[4].target=bad[3].target;require(!batch.install(bad)&&!state.creates);bad=bindings;bad[4].original=bad[3].original;require(!batch.install(bad)&&!state.creates);bad=bindings;bad[4].detour=nullptr;require(!batch.install(bad)&&!state.creates);require(batch.install(bindings)&&batch.installed()&&state.hooks.size()==48);require(!batch.install(bindings));for(const auto& pair:state.hooks)require(pair.second);}
    require(state.hooks.size()==1&&state.hooks.at(pointer(0x77)));for(auto value:original)require(!value);
    for(bool removeFailure:{false,true}){State cleanup;cleanup.hooks[pointer(0x77)]=true;std::array<void*,47> saved{};auto request=bindings;for(unsigned i=0;i<47;++i)request[i].original=&saved[i];nativeCopyInstall::Batch<Backend> batch(Backend{&cleanup});require(batch.install(request));if(removeFailure)cleanup.failRemove=request[7].target;else cleanup.failDisable=request[7].target;require(!batch.rollback()&&batch.pendingCleanup()&&!batch.installed()&&cleanup.hooks.size()==2&&saved[7]!=nullptr);for(unsigned i=0;i<47;++i)if(i!=7)require(!saved[i]);cleanup.failDisable=cleanup.failRemove=nullptr;require(batch.rollback()&&!batch.pendingCleanup()&&cleanup.hooks.size()==1&&!saved[7]);}
    std::puts("PASS: 196 create/enable failure positions across47/51-hook tables roll back owned targets/storage; disable/remove failures retain trampoline and report pending cleanup until retry; success/destructor/duplicate/null/reinstall preserve unrelated hooks; CPU backend, no actual hooks/game/XR.");return 0;
}catch(const std::exception& error){std::puts(error.what());return 1;}}
