#include "../src/reload_gestures.hpp"
#include <stdexcept>
#include <cstdio>
void check(bool v){if(!v)throw std::runtime_error("alignment failed");}
int main(){try{
 using namespace manualReload;Alignment a;
 XrPosef raw{{0,.70710678f,0,.70710678f},{.1f,.2f,.3f}},out;
 XrVector3f offset{.04f,.06f,-.03f},contact{.07f,.26f,.26f};
 check(!a.update(raw,contact,offset,.08f,1000,true,out)&&a.active());
 check(distance(out.position,raw.position)<1e-5f);
 check(!a.update(raw,contact,offset,.10f,1050,true,out)&&a.active());
 check(a.update(raw,contact,offset,.03f,1100,true,out));
 check(distance(out.position,{contact.x-offset.x,contact.y-offset.y,contact.z-offset.z})<1e-5f);
 check(std::fabs(out.orientation.w-1)<1e-5f);
 check(!a.update(raw,contact,offset,.15f,1110,true,out)&&!a.active());
 check(distance(out.position,raw.position)==0);
 a.update(raw,contact,offset,.03f,1200,true,out);
 check(!a.update(raw,contact,offset,.03f,1190,true,out)&&!a.active());
 a.update(raw,contact,offset,.03f,1300,true,out);
 check(!a.update(raw,contact,offset,.03f,1400,false,out)&&!a.active());
 XrVector3f seat{.01f,.2f,.1f},approach{.04f,.15f,.12f};a.reset();
 a.update(raw,approach,offset,.07f,1500,true,out,&seat);
 check(a.update(raw,approach,offset,.07f,1600,true,out,&seat));
 check(distance(out.position,{seat.x-offset.x,approach.y-offset.y,seat.z-offset.z})<1e-5f);
 approach.y=.23f;a.update(raw,approach,offset,.04f,1610,true,out,&seat);
 check(distance(out.position,{seat.x-offset.x,seat.y-offset.y,seat.z-offset.z})<1e-5f);
 check(!a.update(raw,approach,offset,.15f,1620,true,out,&seat)&&distance(out.position,raw.position)==0);
 a.reset();check(!a.update(raw,{seat.x,seat.y+.04f,seat.z},offset,.04f,1700,true,out,&seat)&&!a.active());
 check(!a.update(raw,{seat.x+.07f,seat.y-.01f,seat.z},offset,.071f,1800,true,out,&seat)&&!a.active());
 // Angled pistol/M16 rails must retain their longitudinal slope instead of
 // forcing a vertical path through the side of the magazine well.
 for(float slope:{-.22f,.18f}){
  XrVector3f axis{0,-std::sqrt(1-slope*slope),slope};a.reset();
  XrVector3f entry{seat.x+.012f,seat.y+axis.y*.065f,seat.z+axis.z*.065f};
  a.update(raw,entry,offset,.07f,2000,true,out,&seat,axis);
  check(a.update(raw,entry,offset,.07f,2100,true,out,&seat,axis));
  check(distance(out.position,{seat.x-offset.x,seat.y+axis.y*.065f-offset.y,seat.z+axis.z*.065f-offset.z})<1e-5f);
  XrVector3f overshoot{seat.x,seat.y-axis.y*.01f,seat.z-axis.z*.01f};
  check(a.update(raw,overshoot,offset,.01f,2110,true,out,&seat,axis));
  check(distance(out.position,{seat.x-offset.x,seat.y-offset.y,seat.z-offset.z})<1e-5f);
  a.reset();check(!a.update(raw,{seat.x,seat.y-axis.y*.04f,seat.z-axis.z*.04f},offset,.04f,2200,true,out,&seat,axis));
 }
 Gestures gestures;check(gestures.setReplacementGeometry({0,0,0},{0,.2f,0},{0,0,1},offset));
 Hands h;h.session=h.tracked=h.gameplay=h.pistol=true;h.time=2000;
 auto tick=[&]{++h.serial;gestures.update(h,h.time);};
 tick();h.release=true;tick();h.release=false;h.grip=true;h.left.position=ammoPouch;tick();
 check(gestures.transaction().stage()==Stage::MagazineHeld);
 h.left={{0,0,0,1},{-offset.x,-offset.y,-offset.z}};tick();
 check(gestures.transaction().stage()==Stage::MagazineHeld&&gestures.visual().alignedHand);
 h.time+=99;tick();check(gestures.transaction().stage()==Stage::MagazineHeld);
 h.time+=1;tick();check(gestures.transaction().stage()==Stage::Inserted&&!gestures.transaction().pendingToken());
 h.left.position={-offset.x,.2f-offset.y,-offset.z};tick();check(gestures.transaction().stage()==Stage::Inserted);
 puts("PASS: alignment contact preservation, completion, hysteresis, departure and interruption reset");return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}
