#pragma once
#include "native_wrist_pose.hpp"
namespace nativeTransformCache {
// Native 7C990 skips entity composition when options[0] is already set.
// Its cached row-vector transform is entity * parent. Replace only that entity
// factor, preserving the parent camera and any outer render transform.
inline bool replace(const float* cached,const float* previous,const float* next,float* out){
 using namespace DirectX;
 for(unsigned i=0;i<12;i++)if(!std::isfinite(cached[i])||!std::isfinite(previous[i])||!std::isfinite(next[i]))return false;
 auto old=nativeWristPose::affine(previous);float determinant=XMVectorGetX(XMMatrixDeterminant(old));
 if(!std::isfinite(determinant)||std::fabs(determinant)<.000001f)return false;
 auto result=nativeWristPose::affine(next)*XMMatrixInverse(nullptr,old)*nativeWristPose::affine(cached);
 XMFLOAT4X4 m;XMStoreFloat4x4(&m,result);
 float values[]={m._11,m._21,m._31,m._41,m._12,m._22,m._32,m._42,m._13,m._23,m._33,m._43};
 for(float v:values)if(!std::isfinite(v))return false;
 std::memcpy(out,values,sizeof values);return true;
}
}
