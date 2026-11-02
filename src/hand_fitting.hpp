#pragma once
#include "tracking_math.hpp"
#include "measured_support_fit.hpp"
namespace handFitting {
// Canonical rig curls around +X. Rotate that axis onto each object's actual
// handle axis. Offsets are metres in the tracked weapon's aim frame.
struct Fit {float pitch,roll;XrVector3f wrist;float yaw{};};
inline constexpr Fit weapons[7]={
 {0,-1.57079633f,{.020f,-.055f,.057f}}, // revolver's measured grip axis
 {0,-1.57079633f,{.020f,0,.072f}}, // 9mm grip
 {0,-1.57079633f,{.020f,-.065f,.057f}}, // M16 grip, below the native body pivot
 {-.78539816f,-1.57079633f,{.020f,-.016f,.071f}}, // sloping shotgun stock neck
 {-.87266463f,-1.57079633f,{.020f,.055f,.046f}}, // hunting-rifle stock neck
 {0,-1.57079633f,{.020f,0,.072f}}, // buzzsaw motor handle
 {-1.57079633f,-1.57079633f,{.020f,.072f,.019f}} // baton shaft

};
inline XrPosef compose(const XrPosef& anchor,const Fit& fit){
    using namespace DirectX;
    auto m=XMMatrixRotationZ(fit.roll)*XMMatrixRotationY(fit.yaw)*XMMatrixRotationX(fit.pitch)*XMMatrixTranslation(fit.wrist.x,fit.wrist.y,fit.wrist.z)*motionTracking::rotation(anchor.orientation)*XMMatrixTranslation(anchor.position.x,anchor.position.y,anchor.position.z);
    XMFLOAT4 q;XMStoreFloat4(&q,XMQuaternionNormalize(XMQuaternionRotationMatrix(m)));XMFLOAT4X4 a;XMStoreFloat4x4(&a,m);
    return {{q.x,q.y,q.z,q.w},{a._41,a._42,a._43}};
}
inline XrPosef weapon(const XrPosef& aim,int index){return compose(aim,weapons[index>=0&&index<7?index:1]);}
inline XrPosef supportHand(const XrPosef& aim,const XrPosef& tracked){
    auto anchor=aim;anchor.position=tracked.position;
    return compose(anchor,{3.14159265f,0,{-.072f,-.032f,0},-1.57079633f});
}
inline bool measuredSupportPose(const XrPosef& aim,int weapon,XrPosef& out){
    const auto* fit=measuredSupport::find(weapon);if(!fit)return false;
    using namespace DirectX;
    auto m=motionTracking::rotation(fit->wrist.orientation)*XMMatrixTranslation(fit->wrist.position.x,fit->wrist.position.y,fit->wrist.position.z)*motionTracking::rotation(aim.orientation)*XMMatrixTranslation(aim.position.x,aim.position.y,aim.position.z);
    XMFLOAT4 q;XMStoreFloat4(&q,XMQuaternionNormalize(XMQuaternionRotationMatrix(m)));XMFLOAT4X4 a;XMStoreFloat4x4(&a,m);
    out={{q.x,q.y,q.z,q.w},{a._41,a._42,a._43}};return motionTracking::validPose(out);
}
inline XrVector3f support(int weapon,bool measured=false){
    if(measured){if(const auto* fit=measuredSupport::find(weapon))return fit->contact;}
    // Aim-local support sockets; reload handles/ports retain their separate gates.
    if(weapon==3)return {0,-.025f,-.22f};if(weapon==4)return {0,-.035f,-.30f};
    if(weapon==5)return {-.055f,-.015f,-.10f};return {0,-.025f,-.25f};
}
}
