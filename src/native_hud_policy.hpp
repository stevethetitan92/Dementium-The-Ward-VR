#pragma once
#include <cmath>
namespace nativeHud {
enum class Element { Other, Health, Ammo };
// Bounds and atlas dimensions observed in smooth-runtime-validation.log.
// Fail open for menus, unknown textures, partial/offscreen bounds and prompts.
inline Element classify(const float* b,unsigned width,unsigned height,unsigned count,bool gameplay){
    if(!gameplay||!b)return Element::Other;
    for(unsigned i=0;i<4;++i)if(!std::isfinite(b[i]))return Element::Other;
    if(b[0]<-1||b[1]<-1||b[2]>1||b[3]>1||b[2]<=b[0]||b[3]<=b[1])return Element::Other;
    if(b[1]<-.99f||b[3]>-.80f)return Element::Other;
    const bool numbers=width==128&&height==256&&(count==18||count==24);
    if(b[0]<-.98f||b[2]>-.70f){
        if(numbers&&b[0]>=.78f&&b[2]<=.98f)return Element::Ammo;
        return Element::Other;
    }
    if(numbers||(width==28&&height==18&&count==4))return Element::Health;
    return Element::Other;
}
}
