#pragma once
#include "selected_frames.hpp"
#include <algorithm>
namespace leftMeleeWrist {
using namespace DirectX;
inline XMFLOAT4X4 local[2]{},handToAim[2]{};inline bool ready[2]{};
inline bool cap(const psxAssets::Pack& pack,unsigned weapon,unsigned kind,XMMATRIX transform,XMVECTOR& center,XMVECTOR& axis){
 center=axis=XMVectorZero();unsigned count=0;
 for(const auto& section:pack.sections)if(section.weapon==weapon&&section.kind==kind)
  for(unsigned j=0;j<section.count;++j){const auto* p=pack.vertices[section.first+j].value;if(p[11]!=4)continue;
   center+=XMVectorSet(p[0],p[1],p[2],0);axis+=XMVectorSet(p[3],p[4],p[5],0);++count;}
 if(!count||XMVectorGetX(XMVector3LengthSq(axis))<1e-8f)return false;
 center=XMVector3TransformCoord(center/float(count),transform);axis=XMVector3Normalize(XMVector3TransformNormal(axis,transform));return true;
}
inline void load(const psxAssets::Pack& pack){
 XMVECTOR source,sourceAxis;auto free=XMLoadFloat4x4(&selectedFrames::frames[2].editorToFree);
 const bool sourceValid=cap(pack,2,psxAssets::LeftSupport,free,source,sourceAxis);
 for(unsigned i=0;i<2;++i){ready[i]=false;XMVECTOR target,targetAxis;
  auto fitted=XMLoadFloat4x4(&selectedFrames::frames[i+5].editorToAim);
  XMVECTOR handCenter=XMVectorZero();unsigned handCount=0;
  for(const auto& section:pack.sections)if(section.weapon==i+5&&section.kind==psxAssets::RightHand)
   for(unsigned j=0;j<section.count;++j){const auto* p=pack.vertices[section.first+j].value;handCenter+=XMVectorSet(p[0],p[1],p[2],0);++handCount;}
  // Reflect around the fitted hand's center, not the controller origin; the
  // latter moves an offset gripping hand away from the unchanged handle.
  float centerX=handCount?XMVectorGetX(XMVector3TransformCoord(handCenter/float(handCount),fitted)):0;
  auto mirrored=fitted*XMMatrixScaling(-1,1,1)*XMMatrixTranslation(2*centerX,0,0);
  XMStoreFloat4x4(&handToAim[i],mirrored);
  if(!sourceValid||!cap(pack,i+5,psxAssets::RightHand,mirrored,target,targetAxis))continue;
  float dot=std::clamp(XMVectorGetX(XMVector3Dot(sourceAxis,targetAxis)),-1.f,1.f);
  auto cross=XMVector3Cross(sourceAxis,targetAxis);
  XMMATRIX rotate=XMMatrixIdentity();
  if(dot<-.9999f){auto orthogonal=XMVector3Normalize(XMVector3Cross(sourceAxis,std::fabs(XMVectorGetY(sourceAxis))<.9f?XMVectorSet(0,1,0,0):XMVectorSet(1,0,0,0)));rotate=XMMatrixRotationAxis(orthogonal,XM_PI);}
  else rotate=XMMatrixRotationQuaternion(XMQuaternionNormalize(XMVectorSetW(cross,1+dot)));
  auto align=XMMatrixTranslationFromVector(-source)*rotate*XMMatrixTranslationFromVector(target);
  XMStoreFloat4x4(&local[i],XMLoadFloat4x4(&selectedFrames::frames[2].wristToEditor)*free*align);ready[i]=true;
 }
}
}
