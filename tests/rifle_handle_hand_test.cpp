#include "../src/revolver_contact.hpp"
#include <cassert>
#include <cstdio>
int main(){
 using namespace arsenal;
 RevolverContact fit;assert(fit.set({0,0,0},{.02f,.01f,-.03f}));
 for(float pull:{-.2f,0.f,.035f,.07f,.2f})for(float angle:{0.f,.6f,1.2f}){
  DirectX::XMFLOAT4 q;DirectX::XMStoreFloat4(&q,DirectX::XMQuaternionRotationRollPitchYaw(angle,angle*.5f,0));
  XrPosef raw{{q.x,q.y,q.z,q.w},{.04f,.03f,pull}};
  XrVector3f grab{.02f,.01f,-.1f},axis{0,0,1};
  auto constrained=rifleHandleHand(raw,fit.contact(raw),grab,axis,pull);
  auto contact=fit.contact(constrained);
  assert(manualReload::distance(contact,{grab.x,grab.y,grab.z+std::clamp(pull,0.f,.07f)})<1e-6f);
  assert(constrained.orientation.x==raw.orientation.x&&constrained.orientation.w==raw.orientation.w);
  assert(raw.position.z==pull);
 }
 puts("PASS: M16 hand contact stops at both rail ends; rotated grip contact preserved; raw pose unchanged");
}
