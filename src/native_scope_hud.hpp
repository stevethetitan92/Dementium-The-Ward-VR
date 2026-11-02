#pragma once
#include <cmath>
namespace nativeScopeHud {
// Observed native hunting scope sprite contract: an 8px scanline tile,
// 256px circular reticle, four oversized solid masks and the x1.0 label.
// Classify before viewport clipping; retain ordinary full-screen fades.
inline bool suppress(bool gameplay,int weapon,unsigned width,unsigned height,unsigned count,const float* b){
    if(!gameplay||weapon!=4||!b)return false;
    for(unsigned i=0;i<4;i++)if(!std::isfinite(b[i]))return false;
    const float w=b[2]-b[0],h=b[3]-b[1];
    if(count==4&&width==height&&(width==256||width==8))
        return w>.85f&&w<1.05f&&h>1.5f&&h<1.85f;
    if(count==4&&width==1&&height==1)
        return w>4.f||h>4.f;
    if((count==24||count==30)&&width==128&&height==256)
        return b[0]>.70f&&b[2]<.90f&&b[1]>.80f&&b[3]<.98f;
    return false;
}
}
