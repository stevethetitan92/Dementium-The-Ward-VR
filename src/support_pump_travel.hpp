#pragma once
#include <cmath>
#include <openxr/openxr.h>
namespace supportPumpTravel {
// Two-hand aim follows the line between the controllers. Its pump coordinate
// must follow that same line, rather than the unassisted right wrist's Z axis.
inline float length(XrVector3f p){return std::sqrt(p.x*p.x+p.y*p.y+p.z*p.z);}
inline float travel(XrVector3f rest,XrVector3f current){return length(rest)-length(current);}
inline bool valid(XrVector3f rest,XrVector3f current){
 const float a=length(rest),b=length(current);
 return std::isfinite(a)&&std::isfinite(b)&&a>=.08f&&a<=1.2f&&b>=.08f&&b<=1.2f;
}
}
