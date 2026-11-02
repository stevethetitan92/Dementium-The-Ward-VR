#pragma once
#include <d3d12.h>
#include <atomic>
#include <functional>
#include <new>

// The resource owns this notification object; it must never own the resource.
// Install only once per capture entry, while the caller owns a live resource.
namespace resourceCaptureLifetime {
inline constexpr GUID tag={0x5e83ff84,0x718e,0x461e,{0x9a,0x97,0x4d,0xd2,0x37,0xc9,0x81,0x52}};
class Notification final : public IUnknown {
    std::atomic<ULONG> references{1};
    std::function<void()> onDestroy;
public:
    explicit Notification(std::function<void()> fn):onDestroy(std::move(fn)){}
    ~Notification(){if(onDestroy)onDestroy();}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out) override {
        if(!out)return E_POINTER;*out=nullptr;
        if(iid!=__uuidof(IUnknown))return E_NOINTERFACE;
        *out=static_cast<IUnknown*>(this);AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{return ++references;}
    ULONG STDMETHODCALLTYPE Release() override{auto n=--references;if(!n)delete this;return n;}
    void cancel(){onDestroy={};}
};
inline bool attach(ID3D12Resource* resource,std::function<void()> onDestroy){
    auto* notification=new(std::nothrow) Notification(std::move(onDestroy));
    if(!notification)return false;
    auto result=resource->SetPrivateDataInterface(tag,notification);
    if(FAILED(result))notification->cancel();
    notification->Release();return SUCCEEDED(result);
}
}
