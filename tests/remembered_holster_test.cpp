#include "../src/body_holsters.hpp"
#include <cassert>
#include <cstdio>
int main(){
 bodyHolsters::State flashlightOnly;flashlightOnly.step(true,false,bodyHolsters::anchors[0],-1,255);auto drawn=flashlightOnly.step(true,true,bodyHolsters::anchors[0],-1,255);assert(drawn.equip==1&&!drawn.hidden);
 using namespace bodyHolsters;unsigned all=255;State s;
 Display display{};display.valid=true;display.owned=all;
 for(int stored=0;stored<2;++stored){display.hip=stored;
  for(int current=0;current<7;++current){display.current=current;display.stowed=false;assert(showHip(display)==(current>=2));
   display.stowed=true;assert(showHip(display));}}
 display.owned=0;assert(!showHip(display));display.owned=all;display.valid=false;assert(!showHip(display));
 hipHeight=-.72f;State calibrated;auto hip=anchor(0);
 calibrated.step(true,false,hip,1,all);assert(calibrated.step(true,true,hip,1,all).hidden);
 assert(hip.y==-.72f);hipHeight=anchors[0].y;
 s.step(true,false,anchors[0],0,all);
 auto r=s.step(true,true,anchors[0],0,all);assert(r.changed&&r.hidden&&s.stored(0)==0);
 r=s.step(true,true,anchors[0],0,all);assert(!r.changed);
 s.step(true,false,anchors[0],0,all);r=s.step(true,true,anchors[0],0,all);assert(r.equip==0&&!r.hidden);
 s.step(true,false,anchors[2],5,all);r=s.step(true,true,anchors[2],5,all);assert(r.hidden&&s.stored(2)==5);
 s.step(true,false,anchors[2],5,all);r=s.step(true,true,anchors[2],5,all);assert(r.equip==5);
 State unavailable;unsigned onlyRevolver=1u<<weaponGroups::ids[0];
 unavailable.step(true,false,anchors[0],3,onlyRevolver);r=unavailable.step(true,true,anchors[0],3,onlyRevolver);assert(r.equip==-1&&!r.changed);
 GripLatch grip;assert(!grip.update(.5f));assert(grip.update(.8f));assert(grip.update(.5f));assert(!grip.update(.2f));
 State left;
 left.step(true,false,anchors[2],1,all,0);
 r=left.step(true,true,anchors[2],1,all,0,false);assert(!r.changed); // occupied hand
 left.step(true,false,anchors[2],1,all,0);
 r=left.step(true,true,anchors[2],1,all,0);assert(r.equip==6&&left.hand()==0&&left.stored(0)==1);
 left.step(true,true,anchors[2],6,all,0);assert(left.hand()==0); // native equip acknowledgement
 left.step(true,false,anchors[2],6,all,0);
 r=left.step(true,true,anchors[2],6,all,0);assert(r.hidden&&left.stored(2)==6);
 left.step(true,false,anchors[2],6,all,1);
 r=left.step(true,true,anchors[2],6,all,1);assert(r.equip==6&&left.hand()==1&&!r.hidden);
 State wrongSlot;wrongSlot.step(true,false,anchors[0],1,all,0);
 r=wrongSlot.step(true,true,anchors[0],1,all,0);assert(!r.changed);
 State tracking;tracking.step(false,true,anchors[2],1,all,0);tracking.step(true,true,anchors[2],1,all,0);
 r=tracking.step(true,true,anchors[2],1,all,0);assert(!r.changed); // held grip across tracking recovery
 XrPosef head{{-.70710678f,0,0,.70710678f},{0,1.6f,0}},hand{{0,0,0,1},{anchors[2].x,1.6f+anchors[2].y,anchors[2].z}};
 auto upright=motionTracking::uprightPose(head);auto chest=motionTracking::relativePose(upright,hand).position;
 assert(std::fabs(chest.x-anchors[2].x)<.0001f&&std::fabs(chest.y-anchors[2].y)<.0001f&&std::fabs(chest.z-anchors[2].z)<.0001f);
 puts("Remembered hip/left-shoulder weapons retained; no random fallback; one action per grip press with hysteresis");
}
