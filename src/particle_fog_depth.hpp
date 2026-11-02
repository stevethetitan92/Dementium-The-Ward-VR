#pragma once
#include <cmath>
#include <cstring>
#include <cstddef>
namespace particleFogDepth {
inline bool supported(const void* code,std::size_t bytes){
    constexpr unsigned char signature[]={0x44,0x58,0x42,0x43,0x0e,0x23,0x44,0xc7,0xc0,0x4f,0xd3,0x24,0xda,0x0c,0x9c,0xfd,0xbd,0xb6,0xee,0x5e};
    return code&&bytes==1828&&!std::memcmp(code,signature,sizeof(signature));
}
// POSITION2 from the native particle VS contains the same converted clip
// depth as SV_POSITION: -2 * projected.z - projected.w. Recover the original
// projection's ratio before evaluating its unchanged 128-entry fog LUT.
inline bool prepare(void* out,const void* source,const float* native,const float* eye,bool enabled){
    if(!out||!source||!native||!eye||std::fabs(native[14])<.01f||std::fabs(eye[14])<.01f||std::fabs(eye[11])<1e-8f)return false;
    const float a=(-2*native[10]-native[14])/native[14],b=-2*native[11]/native[14];
    const float ea=(-2*eye[10]-eye[14])/eye[14],eb=-2*eye[11]/eye[14];
    const float scale=b/eb,bias=a-scale*ea;
    if(!std::isfinite(scale)||!std::isfinite(bias))return false;
    std::memcpy(out,source,1040);
    if(!enabled){const unsigned zero=0;std::memcpy(static_cast<char*>(out)+12,&zero,4);}
    const float remap[4]={scale,bias,0,0};std::memcpy(static_cast<char*>(out)+1040,remap,16);
    return true;
}
inline constexpr char shader[]=R"(
cbuffer Globals:register(b0){float3 fogColor;bool fogEnabled;float4 lut[64];float4 depthRemap;};
Texture2D tex:register(t0);SamplerState samp:register(s0);
float4 ps(float4 position:SV_POSITION,float2 uv:TEXCOORD0,float4 color:COLOR0,float4 clip:POSITION2):SV_TARGET {
    float4 result=tex.Sample(samp,uv)*color;
    if(fogEnabled){
        float ratio=(clip.z/clip.w)*depthRemap.x+depthRemap.y;
        float coord=saturate(ratio*.5+.5)*128;
        uint index=min((uint)floor(coord),127u);
        float fraction=coord>=128?1:frac(coord);
        float base=lut[index>>2][index&3];
        uint slope=index+128;
        float visibility=saturate(base+fraction*lut[slope>>2][slope&3]);
        result.rgb=fogColor+visibility*(result.rgb-fogColor);
    }
    return result;
})";
}
