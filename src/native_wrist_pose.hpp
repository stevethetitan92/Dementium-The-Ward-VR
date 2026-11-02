#pragma once
#include "tracking_math.hpp"
namespace nativeWristPose {
using namespace DirectX;
inline std::mutex mutex;
inline XrPosef local{};
inline std::uint64_t captured{};
inline XrPosef leftGripFit{};inline bool haveLeftGripFit{};
inline XMMATRIX affine(const float* a){
 return XMMatrixSet(a[0],a[4],a[8],0,a[1],a[5],a[9],0,a[2],a[6],a[10],0,a[3],a[7],a[11],1);
}
// The extracted mesh uses canonical=(-wrist.y,wrist.z,-wrist.x).
// Undo that mapping, then use the actual animated wrist and rendered entity.
inline void capture(const float* aim,const float* entity,const XrPosef& bone,float scale,std::uint64_t now){
 if(!motionTracking::validPose(bone)||!std::isfinite(scale)||scale<=0)return;
 auto canonical=XMMatrixSet(0,-1,0,0,0,0,1,0,-1,0,0,0,0,0,0,1);
 auto m=canonical*motionTracking::rotation(bone.orientation)*XMMatrixTranslation(bone.position.x,bone.position.y,bone.position.z)*affine(entity)*XMMatrixInverse(nullptr,affine(aim));
 XMFLOAT4X4 v;XMStoreFloat4x4(&v,m);
 v._41/=scale;v._42/=scale;v._43/=scale;
 XMFLOAT4 q;XMStoreFloat4(&q,XMQuaternionNormalize(XMQuaternionRotationMatrix(m)));
 XrPosef next{{q.x,q.y,q.z,q.w},{v._41,v._42,v._43}};
 if(!motionTracking::validPose(next))return;
 std::lock_guard<std::mutex> lock(mutex);local=next;captured=now;
}
inline bool frame(const XrPosef& aim,std::uint64_t now,XrPosef& out){
 XrPosef p;{std::lock_guard<std::mutex> lock(mutex);if(!captured||now<captured||now-captured>=100)return false;p=local;}
 auto m=motionTracking::rotation(p.orientation)*XMMatrixTranslation(p.position.x,p.position.y,p.position.z)*motionTracking::rotation(aim.orientation)*XMMatrixTranslation(aim.position.x,aim.position.y,aim.position.z);
 XMFLOAT4X4 v;XMStoreFloat4x4(&v,m);XMFLOAT4 q;XMStoreFloat4(&q,XMQuaternionRotationMatrix(m));out={{q.x,q.y,q.z,q.w},{v._41,v._42,v._43}};return true;
}
inline bool leftFrame(const XrPosef& aim,const XrPosef& rightGrip,const XrPosef& leftGrip,std::uint64_t now,bool rightTracked,XrPosef& out){
 XrPosef right;
 if(rightTracked&&frame(aim,now,right)){
    auto fit=motionTracking::relativePose(rightGrip,right);
    // Reflect both the mesh's local frame and its controller attachment.
    // A single reflection would turn the wrist frame inside out.
    fit.position.x=-fit.position.x;fit.orientation.y=-fit.orientation.y;fit.orientation.z=-fit.orientation.z;
    std::lock_guard<std::mutex> lock(mutex);leftGripFit=fit;haveLeftGripFit=true;
 }
 XrPosef fit;{std::lock_guard<std::mutex> lock(mutex);if(!haveLeftGripFit)return false;fit=leftGripFit;}
 auto m=motionTracking::rotation(fit.orientation)*XMMatrixTranslation(fit.position.x,fit.position.y,fit.position.z)*motionTracking::rotation(leftGrip.orientation)*XMMatrixTranslation(leftGrip.position.x,leftGrip.position.y,leftGrip.position.z);
 XMFLOAT4X4 v;XMStoreFloat4x4(&v,m);XMFLOAT4 q;XMStoreFloat4(&q,XMQuaternionRotationMatrix(m));out={{q.x,q.y,q.z,q.w},{v._41,v._42,v._43}};return true;
}
}
