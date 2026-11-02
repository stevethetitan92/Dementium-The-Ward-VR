#pragma once
#include <algorithm>
#include <cstdint>
#include <cmath>
namespace cylinderSpin {
class Closing {
 float previous{};std::uint64_t started{},last{};bool spinning{};
public:
 float update(float opening,std::uint64_t now,bool valid){
  if(!valid||!std::isfinite(opening)||now<last||(last&&now-last>150)){*this={};previous=opening;last=now;return 0;}
  if(opening<previous-.0001f&&!spinning){spinning=true;started=now;}
  if(opening>previous+.0001f){spinning=false;}
  previous=opening;last=now;
  if(!spinning)return 0;
  float t=std::clamp(float(now-started)/420.f,0.f,1.f);
  if(t==1){spinning=false;return 0;}
  return 6.28318530718f*(1-(1-t)*(1-t));
 }
};
}
