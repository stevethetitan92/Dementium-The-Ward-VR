#pragma once
#include <cmath>
#include <algorithm>
namespace opticGeometry {
struct Eye {float x,y,z;};
struct Aperture {bool visible{};float transmission{},halfFov{},shiftX{},shiftY{};};
// Eye is measured in the ocular plane: +Z is towards the shooter. Keep the
// sight image present across normal carrying/aiming positions as requested;
// only the physical lens is magnified, never the surrounding world.
inline Aperture aperture(Eye eye,float radius,float zoom){
    Aperture out;
    if(!std::isfinite(eye.x)||!std::isfinite(eye.y)||!std::isfinite(eye.z)||
       !std::isfinite(radius)||!std::isfinite(zoom)||radius<=0||zoom<1||zoom>8)return out;
    if(eye.z<=.005f)return out;
    out.visible=true;out.transmission=1;
    // Fixed optical magnification. Moving the stock toward the eye must not
    // change the scene camera's field of view or make its image swim.
    constexpr float relief=.10f;
    out.halfFov=std::atan(radius/(relief*zoom));
    return out;
}
}
