#include "../src/reload_gestures.hpp"
#include <cassert>
#include <cstdio>
int main(){using namespace DirectX;using namespace manualReload;
 XrVector3f seat{.01f,.04f,-.08f},offset{.04f,.06f,-.03f};
 for(float length:{.0586f,.1288f,.1860f})for(unsigned rotation=0;rotation<24;++rotation){
  XrVector3f axis{0,-.977f,.21325f};auto av=XMVector3Normalize(XMVectorSet(axis.x,axis.y,axis.z,0));XMFLOAT3 aa;XMStoreFloat3(&aa,av);axis={aa.x,aa.y,aa.z};
  XMFLOAT4 q;XMStoreFloat4(&q,XMQuaternionRotationRollPitchYaw(rotation*.19f,rotation*.27f,rotation*.11f));
  XrPosef raw{{q.x,q.y,q.z,q.w},{0,0,0}},out;Alignment guide;
  XrVector3f entry{seat.x+axis.x*(length+.02f),seat.y+axis.y*(length+.02f),seat.z+axis.z*(length+.02f)};
  assert(!guide.update(raw,entry,offset,length+.02f,1000,true,out,&seat,axis,length)&&guide.active());
  float previous=length+.03f;
  // A rapid tracked push must not rotate the model inside the receiver.
  for(unsigned ms=10;ms<=200;ms+=10){
   bool ready=guide.update(raw,seat,offset,0,1000+ms,true,out,&seat,axis,length);
   auto contact=XMVector3TransformNormal(XMVectorSet(offset.x,offset.y,offset.z,0),motionTracking::rotation(out.orientation))+XMVectorSet(out.position.x,out.position.y,out.position.z,0);
   auto delta=contact-XMVectorSet(seat.x,seat.y,seat.z,0);float along=XMVectorGetX(XMVector3Dot(delta,av));
   assert(along>=-1e-6f&&along<=previous+1e-6f);previous=along;
   if(ms<=100)assert(along>=length-1e-6f);
   if(ms>=100)assert(std::fabs(std::fabs(out.orientation.w)-1)<1e-5f);
   assert(ready==(ms==200));
  }
  assert(previous<1e-6f);
  assert(!guide.update(raw,seat,offset,0,1210,false,out,&seat,axis,length)&&!guide.active());
  assert(!guide.update(raw,{seat.x+.08f,seat.y,seat.z},offset,.08f,1220,true,out,&seat,axis,length)&&!guide.active());
 }
 // A captured magazine's visible contact reaches the well despite modest
 // lateral hand error. Too far from the mouth still cannot acquire the rail.
 for(float lateral:{-.05f,.05f}){
  Alignment guide;XrPosef raw{{0,0,0,1},{0,0,0}},out;
  XrVector3f contact{seat.x+lateral,seat.y-.02f,seat.z};
  float d=distance(contact,seat);
  assert(d>.035f);
  assert(!guide.update(raw,contact,offset,d,1000,true,out,&seat,{0,-1,0},.0586f));
  assert(guide.update(raw,contact,offset,d,1200,true,out,&seat,{0,-1,0},.0586f));
  assert(distance({out.position.x+offset.x,out.position.y+offset.y,out.position.z+offset.z},seat)<.035f);
  guide.reset();contact.x=seat.x+.07f;
  assert(!guide.update(raw,contact,offset,d,1300,true,out,&seat,{0,-1,0},.0586f)&&!guide.active());
 }
 puts("PASS: magazine rail orientation, feed, guided seating with lateral tolerance, release/side rejection");
}
