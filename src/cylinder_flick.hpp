#pragma once
#include "tracking_math.hpp"
#include <cstdint>
namespace cylinderFlick {
class Detector {
 XrQuaternionf previous{};std::uint64_t time{},start{};float angle{};bool sampled{};
public:
 void reset(){sampled=false;time=start=0;angle=0;}
 bool update(XrQuaternionf q,XrVector3f closingAxis,std::uint64_t now,bool eligible){
  using namespace DirectX;
  if(!eligible){reset();return false;}
  if(!sampled||now<time||now-time>100){previous=q;time=now;sampled=true;angle=0;start=now;return false;}
  if(now==time)return false;
  auto delta=XMQuaternionNormalize(XMQuaternionRotationMatrix(motionTracking::rotation(q)*XMMatrixTranspose(motionTracking::rotation(previous))));
  if(XMVectorGetW(delta)<0)delta=-delta;
  float sine=XMVectorGetX(XMVector3Length(delta));
  float turn=sine>1e-6f?2*std::atan2(sine,XMVectorGetW(delta))*XMVectorGetX(XMVector3Dot(delta/sine,XMVectorSet(closingAxis.x,closingAxis.y,closingAxis.z,0))):0;
  float dt=float(now-time)/1000;previous=q;time=now;
  if(!std::isfinite(turn)||std::fabs(turn)/dt>18){angle=0;start=now;return false;}
  if(turn<=0||now-start>220){angle=0;start=now;}
  if(turn>0)angle+=turn;
  if(angle>=.22f&&turn/dt>=2.f){reset();return true;}
  return false;
 }
};
}
