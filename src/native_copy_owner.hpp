#pragma once
#include "native_copy_configuration.hpp"
#include "native_copy_observation.hpp"
#include "native_copy_install.hpp"
#include <memory>
namespace nativeCopyOwner {
// UNUSED single-attempt installation owner. No Initialize call site exists.
// Caller owns backend lifetime and establishes ALL recording/callback quiescence
// for install/stop. Explicit stop is required before destruction. No GPU fence
// retirement or native module lifetime is inferred here. Admission defaults OFF.
template<class Backend,size_t Count=51> class Installation {
    nativeCopyInstall::Batch<Backend,Count> batch;
    nativeCopyRuntime::Runtime runtime;
    nativeCopyObservation::Configuration context;
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    std::unique_ptr<nativeCopyConfiguration::Snapshot> snapshot;
    bool attempted{};
    std::exception_ptr failure;
    inline static Installation* current{};
    static nativeCopyEndpoints::Context factory(const nativeCopyEndpoints::Observation& observation){return current?current->context.capture(observation,GetTickCount64()):nativeCopyEndpoints::Context{};}
    void clear(){current=nullptr;context={};device.Reset();snapshot.reset();}
public:
    explicit Installation(Backend backend):batch(backend){}
    Installation(const Installation&)=delete;Installation& operator=(const Installation&)=delete;
    ~Installation(){if(snapshot)std::terminate();}
    bool installed()const{return snapshot&&batch.installed();}
    bool pendingCleanup()const{return snapshot&&!batch.installed();}
    std::exception_ptr lastFailure()const{return failure;}
    template<class Builder> bool install(ID3D12Device* expected,ID3D12GraphicsCommandList* list,std::uintptr_t image,Builder builder,bool recordingQuiescent){
        if(attempted||!recordingQuiescent||!expected||!list||!image||current||nativeCopyEndpoints::runtime||nativeCopyEndpoints::contextFactory||nativeCopyForwarders::dispatch)return false;
        Microsoft::WRL::ComPtr<ID3D12Device> actual;Microsoft::WRL::ComPtr<IUnknown> actualIdentity,expectedIdentity;
        if(FAILED(list->GetDevice(IID_PPV_ARGS(&actual)))||FAILED(actual.As(&actualIdentity))||FAILED(expected->QueryInterface(IID_PPV_ARGS(&expectedIdentity)))||actualIdentity.Get()!=expectedIdentity.Get())return false;
        attempted=true;
        try{
            snapshot=std::make_unique<nativeCopyConfiguration::Snapshot>();device=expected;context={image,device.Get()};current=this;
            nativeCopyEndpoints::runtime=&runtime;nativeCopyEndpoints::contextFactory=&factory;nativeCopyForwarders::dispatch=&nativeCopyEndpoints::intervening;
            // Builder configures existing bridge bodies before hooks become live.
            const auto bindings=builder(list);
            if(batch.install(bindings))return true;
        }catch(...){failure=std::current_exception();}
        if(snapshot&&nativeCopyConfiguration::release(batch,*snapshot,true))clear();
        return false;
    }
    // Same clock as the observation factory; callers cannot mix time domains.
    bool arm(std::uint64_t duration,unsigned budget){return installed()&&runtime.arm(GetTickCount64(),duration,budget);}
    void cancel(){runtime.cancel();}
    bool stop(bool recordingQuiescent){
        if(!snapshot)return true;
        if(!recordingQuiescent||!nativeCopyConfiguration::release(batch,*snapshot,true))return false;
        clear();return true;
    }
};
}
