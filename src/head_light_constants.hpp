#pragma once
#include <openxr/openxr.h>
#include <DirectXMath.h>
#include <cmath>
#include <atomic>
#include "handheld_light.hpp"
namespace headLightConstants {
inline std::atomic<bool> enabled{true};
inline bool center(float* data,const XrPosef& head,const XrPosef& eye,float scale){
    // Audited native 448-byte pixel buffer: light zero is the equipped lamp;
    // light one is room illumination. Preserve all colors and attenuation.
    if(!data||!std::isfinite(scale)||scale<=0||data[7]!=1||data[52]==0||data[60]==0)return false;
    if(!enabled.load()||handheldLight::reloadSuppressed.load()){data[52]=0;return true;}
    using namespace DirectX;
    auto quaternion=[](XrQuaternionf q){return XMVectorSet(q.x,q.y,q.z,q.w);};
    auto h=quaternion(head.orientation),e=quaternion(eye.orientation);
    float hn=XMVectorGetX(XMVector4LengthSq(h)),en=XMVectorGetX(XMVector4LengthSq(e));
    if(!std::isfinite(hn)||!std::isfinite(en)||fabsf(hn-1)>.01f||fabsf(en-1)>.01f)return false;
    auto inverse=XMQuaternionConjugate(e);
    auto delta=XMVectorSet(head.position.x-eye.position.x,head.position.y-eye.position.y,head.position.z-eye.position.z,0);
    auto position=XMVector3Rotate(delta,inverse)*scale;
    auto direction=XMVector3Rotate(XMVector3Rotate(XMVectorSet(0,0,-1,0),h),inverse);
    XMFLOAT3 p,d;XMStoreFloat3(&p,position);XMStoreFloat3(&d,direction);
    if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))return false;
    data[4]=p.x;data[5]=p.y;data[6]=p.z;
    data[12]=d.x;data[13]=d.y;data[14]=d.z;return true;
}
}
