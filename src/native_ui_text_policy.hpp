#pragma once
#include <cmath>
namespace nativeUiText {
// Native software pointer captured in cursor-audit/sprites.txt: one 40x40
// texture, four full-texture vertices. Only exclude it from custom inventory.
template<class Read> bool inventoryCursor(unsigned width,unsigned height,unsigned count,bool inventory,Read read){
 if(!inventory||width!=40||height!=40||count!=4)return false;
 constexpr float expected[4][2]={{0,0},{1,0},{0,1},{1,1}};
 for(unsigned i=0;i<4;i++){float uv[4]{};if(!read(i,uv))return false;
  for(unsigned j=0;j<2;j++)if(!std::isfinite(uv[j])||std::fabs(uv[j]-expected[i][j])>.0001f)return false;}
 return true;
}
// Exact UV sequence captured from the native English WEAPON RELOADED draw.
// Match content, not its screen rectangle; all unknown text passes through.
inline constexpr unsigned short reloadUV[] = {25,39,36,39,25,49,36,39,25,49,36,49,0,28,7,28,0,38,7,28,0,38,7,38,91,12,100,12,91,22,100,12,91,22,100,22,94,28,101,28,94,38,101,28,94,38,101,38,85,28,93,28,85,38,93,28,85,38,93,38,75,28,84,28,75,38,84,28,75,38,84,38,0,12,12,12,0,27,12,12,0,27,12,27,111,28,119,28,111,38,119,28,111,38,119,38,0,28,7,28,0,38,7,28,0,38,7,38,55,28,62,28,55,38,62,28,55,38,62,38,85,28,93,28,85,38,93,28,85,38,93,38,91,12,100,12,91,22,100,12,91,22,100,22,117,12,124,12,117,22,124,12,117,22,124,22,0,28,7,28,0,38,7,28,0,38,7,38,117,12,124,12,117,22,124,12,117,22,124,22};
template<class Read> bool isReloadMessage(unsigned width,unsigned height,unsigned count,bool gameplay,Read read){
 if(!gameplay||width!=128||height!=256||count!=90)return false;
 for(unsigned i=0;i<count;i++){float uv[4]{};if(!read(i,uv))return false;
  for(unsigned j=0;j<2;j++){float v=uv[j]*(j?256.f:128.f);if(!std::isfinite(v)||std::fabs(v-reloadUV[i*2+j])>.002f)return false;}}
 return true;
}
}

namespace nativeUiText {
inline bool interactionButton(const float* b,unsigned w,unsigned h,unsigned count,bool gameplay){
 return gameplay&&b&&count==4&&((w==15&&h==15)||(w==20&&h==13))&&
 b[0]>-.075f&&b[2]<.075f&&b[0]<0&&b[2]>0&&b[1]>-.85f&&b[3]<-.65f;
}
// Captured save/menu footer sprites include both the key box and its caption.
// Restrict replacements to their observed bottom corners, never arbitrary text.
inline bool menuButton(const float* b,unsigned w,unsigned h,unsigned count){
 if(!b||count!=4||b[1]>-.97f||b[3]>-.80f)return false;
 return (w==48&&h==20&&b[0]<-.97f&&b[2]<-.70f)||
        (w==65&&h==22&&b[0]>.65f&&b[2]>.97f);
}
// Original authored pixel glyph. The native quad supplies position and fade.
inline constexpr char buttonShader[]=R"(
struct NativeSprite {float4 position:SV_POSITION;float2 uv:TEXCOORD0;float2 uv1:TEXCOORD1;float4 row0:COLOR0;float4 row1:COLOR1;float4 row2:COLOR2;float4 row3:COLOR3;float4 bias:COLOR4;float4 color:COLOR5;};
Texture2D nativeSource:register(t0);
float4 ps(NativeSprite input):SV_Target {
 float2 uv=input.uv;float4 color=input.color;
 uint w,h;nativeSource.GetDimensions(w,h);
 if((w==48&&h==20)||(w==65&&h==22)) {
  int2 texel=min(int2(uv*float2(w,h)),int2(w-1,h-1));
  float4 source=nativeSource.Load(int3(texel,0));
  bool back=w==48;
  int2 lo=back?int2(6,2):int2(2,4);
  int2 hi=back?int2(23,14):int2(20,16);
  if(all(texel>=lo)&&all(texel<hi)) {
   int2 glyph=texel-(back?int2(12,4):int2(8,6));
   const uint a[7]={14,17,17,31,17,17,17};
   const uint b[7]={30,17,17,30,17,17,30};
   bool ink=false;
   if(glyph.x>=0&&glyph.x<5&&glyph.y>=0&&glyph.y<7)
    ink=((back?b[glyph.y]:a[glyph.y])&(1u<<(4-glyph.x)))!=0;
   source.rgb=ink?0.02:0.96;
  }
  source*=color;
  return float4(dot(input.row0,source),dot(input.row1,source),dot(input.row2,source),dot(input.row3,source))+input.bias;
 }
 uv.x=(uv.x-.5)*(float(w)/max(float(h),1.0))+.5;
 if(uv.x<0||uv.x>1)discard;
 int2 p=int2(floor(saturate(uv)*15));
 bool edge=p.x==0||p.x>=14||p.y==0||p.y>=14;
 const uint rows[7]={14,17,17,31,17,17,17};
 int x=p.x-5,y=p.y-4;bool ink=false;
 if(x>=0&&x<5&&y>=0&&y<7)ink=(rows[y]&(1u<<(4-x)))!=0;
 float shade=edge||ink?0.02:0.96;return float4(shade,shade,shade,color.a);
})";
}
