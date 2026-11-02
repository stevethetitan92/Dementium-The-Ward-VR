#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace sawMotion {
constexpr double pi=3.14159265358979323846;
struct Rotor {
 double speed=0,angle=0;
 double maxRPM=1800,spinUp=.18,spinDown=.22;
 std::uint64_t last=0;
 void step(double dt,bool held){
  if(!std::isfinite(dt)||dt<=0)return;
  if(dt>.25){speed=0;return;}
  const double top=maxRPM*2*pi/60,target=held?top:0;
  const double accel=held?top/spinUp:-top/spinDown;
  const double t=(std::min)(dt,std::fabs(target-speed)/std::fabs(accel));
  angle=std::fmod(angle+speed*t+.5*accel*t*t+target*(dt-t),2*pi);
  speed=std::clamp(speed+accel*t,0.0,top);
 }
 void tick(std::uint64_t now,bool held){
  if(!last||now<last){last=now;speed=0;return;}
  double dt=double(now-last)/1000;last=now;step(dt,held);
 }
};
inline Rotor blade;
}

