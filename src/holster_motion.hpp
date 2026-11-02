#pragma once
#include <DirectXMath.h>
#include <algorithm>
#include <cstdint>
#include <cmath>
namespace holsterMotion {
using namespace DirectX;
// Render-only handoff. Inventory, tracking and the firing ray never use this.
class Blend {
 XMFLOAT4X4 previous{},offset{};bool ready{},held{},moving{};std::uint64_t last{},start{};
public:
 void reset(){ready=moving=false;}
 XMMATRIX update(const XMMATRIX& target,bool inHand,std::uint64_t now){
  if(!ready||now<last||now-last>100){ready=true;moving=false;held=inHand;}
  else if(held!=inHand){
   auto delta=XMLoadFloat4x4(&previous)*XMMatrixInverse(nullptr,target);
   // Never animate a stale tracking jump across the room.
   float distance=XMVectorGetX(XMVector3Length(delta.r[3]));
   moving=std::isfinite(distance)&&distance<.45f;
   if(moving){XMStoreFloat4x4(&offset,delta);start=now;}
   held=inHand;
  }
  auto result=target;
  if(moving){
   float t=std::clamp(float(now-start)/140.f,0.f,1.f);t=t*t*(3-2*t);
   auto delta=XMLoadFloat4x4(&offset);
   auto rotation=XMQuaternionNormalize(XMQuaternionRotationMatrix(delta));
   auto eased=XMMatrixRotationQuaternion(XMQuaternionSlerp(rotation,XMQuaternionIdentity(),t));
   eased.r[3]=XMVectorSetW(XMVectorScale(delta.r[3],1-t),1);
   result=eased*target;if(t>=1)moving=false;
  }
  last=now;XMStoreFloat4x4(&previous,result);return result;
 }
};
inline Blend hip[2];
inline void reset(){for(auto& b:hip)b.reset();}
}
