#include "../src/saw_motion.hpp"
#include <cassert>
#include <cstdio>
int main(){
 using namespace sawMotion;
 for(int hz: {60,72,90,120,144}){
  Rotor r;double dt=1./hz;for(int i=0;i<hz;i++)r.step(dt,true);
  assert(std::fabs(r.speed-60*pi)<1e-7);
  for(int i=0;i<hz/2;i++)r.step(dt,false);
  assert(r.speed==0);double stop=r.angle;r.step(.01,false);assert(r.angle==stop);
 }
 Rotor r;r.step(.18,true);assert(std::fabs(r.speed-60*pi)<1e-7);r.step(.11,false);assert(std::fabs(r.speed-30*pi)<1e-7);r.step(.11,false);assert(r.speed==0);
 r.step(.10,true);double partial=r.speed;r.step(.01,false);assert(r.speed<partial);r.step(.02,true);assert(r.speed>partial);
 r.step(1,true);assert(r.speed==0);r.tick(1000,true);r.tick(900,true);assert(r.speed==0);
 r.step(.18,true);r.step(.22,false);assert(r.speed==0);
 puts("PASS trigger acceleration, release braking, re-press continuity, stop, tracking gap, backward clock, and 60-144 Hz independence");
}

