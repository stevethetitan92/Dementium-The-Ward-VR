#pragma once
#include <cmath>
#include <algorithm>
namespace nativeBandFit {
// Generated from the actual native hand's eight-edge wrist opening.
inline constexpr float ring[8][2]={{0.008300000f,-0.017820000f},{-0.010990000f,-0.021000000f},{-0.020260000f,-0.011720000f},{-0.024660000f,0.002440000f},{-0.005860000f,0.018070000f},{0.014650000f,0.022950000f},{0.034180000f,0.009280000f},{0.024900000f,-0.007570000f}};
inline constexpr float center[2]={0.002532500f,-0.000671250f};
inline constexpr float sourceCenter[2]={-0.004364387f,-0.006181279f};
inline constexpr float sourceRadius[2]={0.026178095f,0.018696180f};
inline void map(float& x,float& y,float& z,unsigned side){
 float sign=side?1.f:-1.f,px=x*sign,py=y;
 float angle=std::atan2((py-sourceCenter[1])/sourceRadius[1],(px-sourceCenter[0])/sourceRadius[0]);
 float dx=std::cos(angle),dy=std::sin(angle),radius=1;
 for(unsigned i=0;i<8;i++){float ax=ring[i][0]-center[0],ay=ring[i][1]-center[1],ex=ring[(i+1)%8][0]-ring[i][0],ey=ring[(i+1)%8][1]-ring[i][1];
  float det=dx*ey-dy*ex;if(std::fabs(det)<1e-10f)continue;
  float t=(ax*ey-ay*ex)/det,u=(ax*dy-ay*dx)/det;if(t>0&&u>=0&&u<=1)radius=(std::min)(radius,t);
 }
 float sourceR=1/std::hypot(dx/sourceRadius[0],dy/sourceRadius[1]);
 float offset=std::clamp(std::hypot(px-sourceCenter[0],py-sourceCenter[1])-sourceR,-.0005f,.0005f);
 x=sign*(center[0]+dx*(radius+.0015f+offset));y=center[1]+dy*(radius+.0015f+offset);z+=.014f;
}
}
