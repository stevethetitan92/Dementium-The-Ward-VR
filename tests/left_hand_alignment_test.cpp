#include "../src/left_hand_alignment.hpp"
#include <cstdio>
#include <stdexcept>
using namespace DirectX;
int main(){try{
 for(float yaw:{0.f,.7f,-1.8f})for(float roll:{0.f,.5f,2.2f}){
  XMFLOAT4 q;XMStoreFloat4(&q,XMQuaternionRotationRollPitchYaw(.4f,yaw,roll));
  XrPosef raw{{q.x,q.y,q.z,q.w},{.2f,-.4f,.1f}};
  XrQuaternionf head{0,std::sin(yaw/2),0,std::cos(yaw/2)};
  auto c=leftHandAlignment::calibrate(raw.orientation,head);
  auto p=leftHandAlignment::apply(raw,c);
  if(!motionTracking::validPose(p)||p.position.x!=raw.position.x||p.position.y!=raw.position.y||p.position.z!=raw.position.z)throw std::runtime_error("pose");
  auto actual=leftHandAlignment::basis()*motionTracking::rotation(p.orientation)*XMMatrixTranspose(motionTracking::rotation(head));
  XMFLOAT4X4 m;XMStoreFloat4x4(&m,actual);
  if(std::fabs(m._11-1)>1e-5||std::fabs(m._23+1)>1e-5||std::fabs(m._32-1)>1e-5)throw std::runtime_error("axes");
  auto upright=leftHandAlignment::apply(raw,leftHandAlignment::thumbUp(c));
  XMStoreFloat4x4(&m,leftHandAlignment::basis()*motionTracking::rotation(upright.orientation)*XMMatrixTranspose(motionTracking::rotation(head)));
  if(std::fabs(m._12-1)>1e-5||std::fabs(m._23+1)>1e-5||std::fabs(m._31+1)>1e-5)throw std::runtime_error("thumb-up axes");
  // Identical rigid correction keeps hand/object relative distances intact.
  auto a=XMVectorSet(.04f,.06f,-.03f,1),b=XMVectorSet(.05f,.07f,-.04f,1);
  float before=XMVectorGetX(XMVector3Length(a-b));
  float after=XMVectorGetX(XMVector3Length(XMVector3TransformCoord(a,motionTracking::rotation(c))-XMVector3TransformCoord(b,motionTracking::rotation(c))));
  if(std::fabs(before-after)>1e-6)throw std::runtime_error("contact");
 }
 auto saved=leftHandAlignment::thumbUp({.425309420f,.192508742f,.685109317f,-.559175730f});
 printf("thumb_up_xyzw=%.9f,%.9f,%.9f,%.9f\n",saved.x,saved.y,saved.z,saved.w);
 puts("PASS: physical-pose alignment, yaw independence, position and contact-distance preservation");return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}
