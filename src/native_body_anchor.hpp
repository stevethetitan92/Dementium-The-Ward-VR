#pragma once
#include <cmath>
#include <cstring>
namespace nativeBodyAnchor {
// Column-vector affine matrices, matching the native vertex shader. Keep the
// quantized quaternion arithmetic: normalizing changes the native transform.
inline bool bone(const float* q,float* m){
    for(unsigned i=0;i<7;i++)if(!std::isfinite(q[i]))return false;
    const float x=q[0],y=q[1],z=q[2],w=q[3];
    const float n=x*x+y*y+z*z+w*w;if(n<.95f||n>1.05f)return false;
    const float a[]={1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w),q[4],
        2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w),q[5],
        2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y),q[6]};
    std::memcpy(m,a,sizeof a);return true;
}
inline void multiply(const float* a,const float* b,float* out){
    float c[12]{};for(unsigned r=0;r<3;r++){
        for(unsigned j=0;j<3;j++)for(unsigned k=0;k<3;k++)c[r*4+j]+=a[r*4+k]*b[k*4+j];
        c[r*4+3]=a[r*4+3];for(unsigned k=0;k<3;k++)c[r*4+3]+=a[r*4+k]*b[k*4+3];
    }std::memcpy(out,c,sizeof c);
}
inline bool inverse(const float* a,float* out){
    const float d=a[0]*(a[5]*a[10]-a[6]*a[9])-a[1]*(a[4]*a[10]-a[6]*a[8])+a[2]*(a[4]*a[9]-a[5]*a[8]);
    if(!std::isfinite(d)||std::fabs(d)<.000001f)return false;
    float b[]={ (a[5]*a[10]-a[6]*a[9])/d,(a[2]*a[9]-a[1]*a[10])/d,(a[1]*a[6]-a[2]*a[5])/d,0,
        (a[6]*a[8]-a[4]*a[10])/d,(a[0]*a[10]-a[2]*a[8])/d,(a[2]*a[4]-a[0]*a[6])/d,0,
        (a[4]*a[9]-a[5]*a[8])/d,(a[1]*a[8]-a[0]*a[9])/d,(a[0]*a[5]-a[1]*a[4])/d,0};
    for(unsigned r=0;r<3;r++)for(unsigned k=0;k<3;k++)b[r*4+3]-=b[r*4+k]*a[k*4+3];
    std::memcpy(out,b,sizeof b);return true;
}
// Preserve the settled weapon-body transform while leaving all other bones
// relative to it. This is a render-only correction; native animation is intact.
inline bool stabilize(const float* reference,const float* current,float* entity){
    float rest[12],animated[12],undo[12],relative[12],next[12];
    for(unsigned i=0;i<12;i++)if(!std::isfinite(entity[i]))return false;
    if(!bone(reference,rest)||!bone(current,animated)||!inverse(animated,undo))return false;
    multiply(rest,undo,relative);multiply(entity,relative,next);
    for(float v:next)if(!std::isfinite(v))return false;
    std::memcpy(entity,next,sizeof next);return true;
}
}
