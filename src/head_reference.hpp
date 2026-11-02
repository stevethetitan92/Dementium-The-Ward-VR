#pragma once
#include "tracking_math.hpp"
namespace headReference {
using namespace DirectX;
inline XrPosef center(const XrView& left,const XrView& right){
 XrPosef p{};p.position={(left.pose.position.x+right.pose.position.x)*.5f,(left.pose.position.y+right.pose.position.y)*.5f,(left.pose.position.z+right.pose.position.z)*.5f};
 XMFLOAT3 f;XMStoreFloat3(&f,XMVector3TransformNormal(XMVectorSet(0,0,-1,0),motionTracking::rotation(left.pose.orientation)));
 float yaw=std::atan2(-f.x,-f.z);p.orientation={0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)};return p;
}
// Transform the previous application anchor into a runtime's new coordinates.
// Dashboard/recenter events must not redefine the player's physical baseline.
inline XrPosef rebase(const XrPosef& anchor,const XrPosef& newInPrevious){return motionTracking::relativePose(newInPrevious,anchor);}
class Startup {
 bool pending{};std::uint64_t since{},last{};XrPosef candidate{{0,0,0,1},{}};
public:
 void reset(){pending=false;since=last=0;}
 bool update(const XrPosef& pose,std::uint64_t now,bool eligible,XrPosef& result){
  if(!eligible||!motionTracking::validPose(pose)){reset();return false;}
  auto d=motionTracking::relativePose(candidate,pose).position;
  float dot=std::fabs(candidate.orientation.x*pose.orientation.x+candidate.orientation.y*pose.orientation.y+candidate.orientation.z*pose.orientation.z+candidate.orientation.w*pose.orientation.w);
  if(!pending||now<last||now-last>150||d.x*d.x+d.y*d.y+d.z*d.z>.0025f||dot<.998f){candidate=pose;since=now;pending=true;}
  last=now;if(now-since<750)return false;result=candidate;return true;
 }
};
enum class Posture {Unknown,Seated,Standing};
inline const char* name(Posture p){return p==Posture::Seated?"seated":p==Posture::Standing?"standing":"unknown";}
class Postures {
 float heights[2]{};bool known[2]{};Posture current=Posture::Unknown,candidate=Posture::Unknown;std::uint64_t since{};
public:
 bool calibrate(Posture p,float height){
  if(p==Posture::Unknown||!std::isfinite(height)||height<.5f||height>2.5f)return false;
  unsigned i=p==Posture::Standing;unsigned other=1-i;
  if(known[other]&&((i&&height-heights[other]<.2f)||(!i&&heights[other]-height<.2f)))return false;
  heights[i]=height;known[i]=true;current=p;candidate=p;since=0;return true;
 }
 float height(Posture p)const{return p==Posture::Unknown?0:heights[p==Posture::Standing];}
 Posture mode()const{return current;}
 bool ready()const{return known[0]&&known[1];}
 void resetMode(){current=candidate=Posture::Unknown;since=0;}
 bool update(float height,std::uint64_t now,bool valid,float& change){
  change=0;if(!valid||!ready()||!std::isfinite(height)){candidate=Posture::Unknown;since=0;return false;}
  float a=std::fabs(height-heights[0]),b=std::fabs(height-heights[1]);
  auto next=a<=.10f&&a+.08f<b?Posture::Seated:b<=.10f&&b+.08f<a?Posture::Standing:Posture::Unknown;
  if(next==Posture::Unknown||next==current){candidate=next;since=now;return false;}
  if(next!=candidate||now<since){candidate=next;since=now;return false;}
  if(now-since<3000)return false;
  change=current==Posture::Unknown?0:this->height(next)-this->height(current);current=next;return true;
 }
};
}
