#pragma once
#include <d3d12.h>
namespace stereoPipelineSamples {
inline bool eligible(unsigned constants,const D3D12_GRAPHICS_PIPELINE_STATE_DESC& d) {
    return (constants==1200||constants==128)&&d.NumRenderTargets==1&&
        d.RTVFormats[0]==DXGI_FORMAT_R8G8B8A8_UNORM&&d.DSVFormat==DXGI_FORMAT_D24_UNORM_S8_UINT&&
        d.SampleDesc.Count>0&&d.RasterizerState.ForcedSampleCount==0;
}
inline D3D12_GRAPHICS_PIPELINE_STATE_DESC adapt(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& d,unsigned samples) {
    auto result=d;
    result.SampleDesc={samples,0};
    // A native cached blob describes the original sample configuration.
    result.CachedPSO={};
    return result;
}
}
