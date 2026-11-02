#pragma once
#include "native_copy_endpoints.hpp"
#include "native_copy_reader.hpp"
namespace nativeCopyObservation {
// Unused context construction for a future quiescent installation. Unlike test
// factories this preserves the endpoint's actual caller and barrier address.
// Device lifetime is owned by the installer; code/frame validation stays in
// Runtime. This helper neither arms Runtime nor proves a native caller valid.
struct Configuration {
    std::uintptr_t image{};ID3D12Device* device{};
    nativeCopyEndpoints::Context capture(const nativeCopyEndpoints::Observation& observation,std::uint64_t now)const{
        nativeCopyEndpoints::Context result{};
        if(!image||!device||!observation.list||observation.caller<image)return result;
        if(observation.kind!=nativeCopyEndpoints::Kind::Barrier&&observation.kind!=nativeCopyEndpoints::Kind::Copy)return result;
        const auto rva=observation.caller-image;
        const bool barrier=observation.kind==nativeCopyEndpoints::Kind::Barrier;
        if(barrier?(rva!=0x1eabf1&&rva!=0x1eac72&&rva!=0x1ea8eb&&rva!=0x1ea95a):
                   (rva!=0x1eac36&&rva!=0x1ea920))return result;
        if(barrier&&!observation.frame)return result;
        result.image=image;result.device=device;result.caller=observation.caller;
        result.frame=barrier?observation.frame:0;result.now=now;
        result.read=[](std::uintptr_t at,void* out,size_t bytes,void*){return nativeCopyReader::Reader{}(at,out,bytes);};
        return result;
    }
};
}
