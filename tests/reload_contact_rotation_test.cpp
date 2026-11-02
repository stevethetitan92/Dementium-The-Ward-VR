#include "../src/reload_gestures.hpp"
#include <cstdio>
#include <stdexcept>
using namespace manualReload;
void check(bool v){if(!v)throw std::runtime_error("contact rotation regression");}
int main(){try{
 for(bool pull:{false,true}){
  Gestures g;check(g.setReplacementGeometry({0,0,0},{0,.2f,0},{0,0,1},{.1f,0,0}));
  Hands h;h.tracked=h.session=h.gameplay=h.pistol=true;h.time=100;
  auto tick=[&]{++h.serial;h.time+=20;g.update(h,h.time);};
  tick();h.release=true;tick();h.release=false;h.grip=true;
  h.left.position={-.23f,-.45f,.02f};tick();
  h.left.position={-.1f,0,0};for(unsigned frame=0;frame<6;++frame)tick();h.grip=false;tick();
  check(g.transaction().stage()==Stage::Inserted);
  h.left.position={-.1f,.2f,0};h.grip=true;tick();
  check(g.transaction().stage()==Stage::SlideHeld);
  // Rotate the wrist 90deg around a stationary fingertip. The controller
  // origin moves 10cm rearward, but the contact has not pulled the slide.
  h.left.orientation={0,.70710678f,0,.70710678f};
  h.left.position={0,.2f,.1f};tick();
  check(std::abs(g.visual().slidePull)<.00001f);
  if(pull){h.left.position.z+=.05f;tick();check(std::abs(g.visual().slidePull-.05f)<.00001f);}
  h.grip=false;tick();check((g.transaction().pendingToken()!=0)==pull);
 }
 puts("PASS: fixed-contact wrist rotation cannot rack; real contact travel can.");return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}
