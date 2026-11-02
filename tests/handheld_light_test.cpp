#include "../src/handheld_light.hpp"
#include <cassert>
#include "../src/arsenal_core.hpp"
#include "../src/head_light_constants.hpp"
#include <cstdio>
int main(){using namespace handheldLight;
 assert(!manualReload::usesReloadHand(manualReload::Stage::EmptyWell));
 assert(manualReload::usesReloadHand(manualReload::Stage::MagazineHeld));
 assert(!arsenal::usesReloadHand(arsenal::Phase::Open));
 assert(!arsenal::usesReloadHand(arsenal::Phase::Opening));
 assert(arsenal::usesReloadHand(arsenal::Phase::Held));
 for(bool switchedOn:{false,true}){
  headLightConstants::enabled=switchedOn;
  for(bool busy:{false,true}){float light[112]{};light[7]=light[52]=light[60]=light[56]=1;
   reloadSuppressed=busy;XrPosef pose{{0,0,0,1},{}};
   assert(headLightConstants::center(light,pose,pose,1));
   assert(light[52]==float(switchedOn&&!busy)&&light[56]==1);
   assert(headLightConstants::enabled==switchedOn);
  }
 }
 reloadSuppressed=false;headLightConstants::enabled=true;
 ReloadHandoff handoff;
 assert(!handoff.blocked(false,false,.3f,true));
 assert(handoff.blocked(true,true,.04f,true));
 assert(handoff.blocked(false,true,.04f,true));
 assert(handoff.blocked(false,false,.04f,true));
 assert(handoff.blocked(false,true,.3f,true));
 assert(!handoff.blocked(false,false,.3f,true));
 assert(handoff.blocked(true,true,.04f,true));
 assert(handoff.blocked(false,false,.3f,false));
 assert(!handoff.blocked(false,false,.3f,true));
 for(int w=-1;w<8;++w)for(bool busy:{false,true})for(bool tracked:{false,true})for(bool right:{false,true})
  assert(eligible(w,right,tracked,busy)==(tracked&&!busy&&(w==-1||(right&&(w==0||w==1||w==6)))));
 for(int w=0;w<7;++w){
  assert(eligible(w,false,true,false,true)); // Stowing any gun frees the left lamp.
  assert(!eligible(w,false,false,false,true));
  assert(!eligible(w,false,true,true,true)); // Medicine/reload ownership still wins.
 }
 for(unsigned i=0;i<24;++i){using namespace DirectX;XMFLOAT4 q;XMStoreFloat4(&q,XMQuaternionRotationRollPitchYaw(i*.12f,i*.25f,i*.19f));
  XrPosef grip{{q.x,q.y,q.z,q.w},{1,2,3}},actual{};auto model=modelPose(grip);publish(model,100,true);assert(read(101,actual));
  auto facing=motionTracking::relativePose(grip,model);assert(std::fabs(facing.orientation.x)<1e-5f&&std::fabs(facing.orientation.y)<1e-5f&&std::fabs(facing.orientation.z)<1e-5f);
  auto frame=XMMatrixRotationY(.4f)*XMMatrixTranslation(.2f,.3f,.1f);
  auto handWorld=frame*motionTracking::rotation(grip.orientation);
  auto corrected=forwardHand(frame,handWorld,1);
  // The former upright lens (+Y) must become controller-forward (-Z), and
  // rotating the hand must keep its flashlight contact point fixed.
  auto editorUp=XMVector3TransformNormal(XMVectorSet(0,1,0,0),XMMatrixInverse(nullptr,frame));
  auto actualAxis=XMVector3TransformNormal(editorUp,corrected);
  auto wantedAxis=XMVector3TransformNormal(XMVectorSet(0,0,-1,0),motionTracking::rotation(grip.orientation));
  assert(XMVectorGetX(XMVector3Length(actualAxis-wantedAxis))<1e-5f);
  auto contact=XMVector3TransformCoord(XMVectorSet(.0475f,.025f,-.0325f,1),XMMatrixInverse(nullptr,frame));
  assert(XMVectorGetX(XMVector3Length(XMVector3TransformCoord(contact,corrected)-XMVector3TransformCoord(contact,handWorld)))<1e-5f);
  auto local=motionTracking::relativePose(model,actual);assert(std::fabs(local.position.x)<1e-5f&&std::fabs(local.position.y)<1e-5f&&std::fabs(local.position.z+.106f)<1e-5f);
  assert(!read(250,actual));publish(model,300,false);assert(!read(301,actual));
 }
 puts("PASS: free-hand ownership and 24 rotated lens/beam poses; stale poses rejected");
 Switch button;assert(!button.update(true,false,100));assert(!button.update(true,true,110));
 assert(!button.update(true,true,179));assert(button.update(true,true,180));
 assert(std::fabs(thumbPress(180)-1)<1e-5f);
 for(unsigned t=181;t<500;++t)assert(!button.update(true,true,t));
 assert(thumbPress(310)==0);assert(!button.update(true,false,510));
 assert(!button.update(true,true,520));assert(!button.update(false,false,530));assert(!button.update(true,false,700));
 puts("PASS: switch toggles once at thumb contact; hold and interrupted press do not repeat");
}
