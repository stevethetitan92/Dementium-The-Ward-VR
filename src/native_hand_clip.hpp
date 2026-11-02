#pragma once
#include <atomic>
#include <cstdint>
namespace nativeHandClip {
inline thread_local unsigned currentMask{};
inline std::atomic<std::uint64_t> drawn[2]{};
inline std::atomic<std::uint64_t> diagnosticUntil{};
inline std::atomic<bool> vrGameplay{};
inline bool originalHands{};
// Diagnose requested hiding versus an actual shader substitution. These counts
// are evidence of the path taken, not proof that every skin triangle vanished.
inline std::atomic<unsigned> sourceRequests{},sourceApplied{},eyeApplied{};
inline std::atomic<unsigned> sourceFallback[6]{};
inline unsigned mask(unsigned weapon,std::uint64_t now){
    constexpr unsigned right[]={30,0x4000001eu,0,0,0,0x80000002u,7};
    constexpr unsigned left[]={0,3072,6,6,12,0x80000030u,0};
    if(weapon>6)return 0;unsigned result=0;
    constexpr unsigned originalRight[]={2,2,0,0,0,2,3};
    constexpr unsigned originalLeft[]={0,1024,2,2,4,16,0};
    for(unsigned side=0;side<2;side++){auto time=drawn[side].load();if(vrGameplay.load()||(time&&now>=time&&now-time<100)||now<diagnosticUntil.load())result|=originalHands?(side?originalRight[weapon]:originalLeft[weapon]):(side?right[weapon]:left[weapon]);}
    return result;
}
struct Scope {unsigned old=currentMask;explicit Scope(unsigned next){currentMask=next;}~Scope(){currentMask=old;}};
// Same native VS constant layout and arithmetic; only adds per-bone clipping.
// Source disassembly: uQuats uses two float4 entries per bone; NORMAL.w is
// that entry index. The mask lives in previously unused c74.z padding.
inline constexpr char shader[]=R"(
cbuffer Globals:register(b0){
 row_major float4x4 uProjection;row_major float3x4 uModelView;row_major float3x4 um34Texture;
 bool uEnableAnimation;bool uEnableTangent;bool uEnableTexcoord2;float pad;
 float4 uQuats[62];float3 uPositionScale;float pad2;float2 uTexcoord2Scale;uint hideBones;float pad3;
}
struct V{float3 p:SV_POSITION;float2 uv:TEXCOORD0;float2 uv1:TEXCOORD1;float2 uv2:TEXCOORD2;float4 n:NORMAL0;float3 t:NORMAL1;};
struct O{float4 p:SV_POSITION;float2 uv:TEXCOORD0;float2 uv1:TEXCOORD1;float2 uv2:TEXCOORD2;float depth:POSITION2;float3 view:POSITION1;float3 n:NORMAL0;float3 t:TANGENT0;float clip:SV_ClipDistance0;};
float3 rotate(float3 v,float4 q){return v+2*cross(q.xyz,cross(q.xyz,v)+q.w*v);}
O vs(V v){O o;float3 p=v.p*uPositionScale,n=v.n.xyz,t=v.t;uint index=(uint)v.n.w;o.clip=1;
 if(uEnableAnimation){float4 q=uQuats[index]/4096;p=rotate(p,q)+uQuats[index+1].xyz/4096;n=rotate(n,q);t=rotate(t,q);o.clip=(hideBones&(1u<<(index/2)))?-1:1;}
 float3 view=mul(uModelView,float4(p,1));float4 projected=mul(uProjection,float4(view,1));
 o.p=float4(projected.xy,-2*projected.z-projected.w,projected.w);o.depth=projected.z;o.view=-view;
 o.n=mul((float3x3)uModelView,n);o.t=mul((float3x3)uModelView,t);
 float2 uv=float2(dot(float3(v.uv,1),um34Texture[0].xyw),dot(float3(v.uv,1),um34Texture[1].xyw));
 // The buzzsaw's palms share its body bone. Its audited atlas places all skin
 // below v=70/128, and its metal above it, so bone clipping alone is insufficient.
 if((hideBones&0x80000000u)&&uv.y>=.546875)o.clip=-1;
 // The pistol's remaining fixed fingers share gun bone 5. The audited native
 // atlas puts those triangles only in the top-left and top-right skin patches.
 if((hideBones&0x40000000u)&&index/2==5&&uv.y<.25&&(uv.x<.20||uv.x>.78))o.clip=-1;
 o.uv=uv;o.uv1=v.uv1;o.uv2=(uEnableTexcoord2?v.uv2:uv)*uTexcoord2Scale;return o;
})";
}
