#pragma once
#include "psx_asset_pack.hpp"
#include <DirectXMath.h>
#include <algorithm>
namespace selectedWristFinish {
using namespace DirectX;
inline unsigned shortened{};
inline void apply(psxAssets::Pack& pack){
 shortened=0;std::vector<psxAssets::Vertex> result;result.reserve(pack.vertices.size());
 for(auto& section:pack.sections){
  const unsigned first=unsigned(result.size());
  if(section.kind==psxAssets::Weapon){result.insert(result.end(),pack.vertices.begin()+section.first,pack.vertices.begin()+section.first+section.count);section.first=first;continue;}
  XMVECTOR center=XMVectorZero(),out=XMVectorZero();unsigned caps=0;
  for(unsigned i=0;i<section.count;++i){auto* p=pack.vertices[section.first+i].value;if(p[11]==4){center+=XMVectorSet(p[0],p[1],p[2],0);out+=XMVectorSet(p[3],p[4],p[5],0);++caps;}}
  if(!caps||XMVectorGetX(XMVector3LengthSq(out))<1e-6f){result.insert(result.end(),pack.vertices.begin()+section.first,pack.vertices.begin()+section.first+section.count);section.first=first;continue;}
  center/=float(caps);out=XMVector3Normalize(out);const auto inward=-out;
  // All retained wrist HUD frames have their proximal edge 88 mm from the
  // original cap. Leave 8 mm of wrist below it on both hands, in fitted space.
  constexpr float cut=.080f;
  auto distance=[&](const psxAssets::Vertex& v){return XMVectorGetX(XMVector3Dot(XMVectorSet(v.value[0],v.value[1],v.value[2],0)-center,inward))-cut;};
  std::vector<XMFLOAT3> boundary;
  psxAssets::Vertex skin{};float nearest=1e30f;bool haveSkin=false;
  for(unsigned i=0;i<section.count;++i){const auto& q=pack.vertices[section.first+i];
   if(q.value[11]!=5&&q.value[11]!=7)continue;
   float d=std::fabs(distance(q));if(d<nearest){nearest=d;skin=q;haveSkin=true;}
  }
  for(unsigned i=0;i<section.count;i+=3){std::vector<psxAssets::Vertex> polygon;
   for(unsigned j=0;j<3;++j){const auto& a=pack.vertices[section.first+i+j];const auto& b=pack.vertices[section.first+i+(j+1)%3];float da=distance(a),db=distance(b);
    if(da>=0)polygon.push_back(a);
    if((da>=0)!=(db>=0)){float t=da/(da-db);psxAssets::Vertex v{};for(unsigned k=0;k<12;++k)v.value[k]=a.value[k]+t*(b.value[k]-a.value[k]);
     auto n=XMVector3Normalize(XMVectorSet(v.value[3],v.value[4],v.value[5],0));XMFLOAT3 nn;XMStoreFloat3(&nn,n);v.value[3]=nn.x;v.value[4]=nn.y;v.value[5]=nn.z;v.value[11]=a.value[11];polygon.push_back(v);boundary.push_back({v.value[0],v.value[1],v.value[2]});}
   }
   for(size_t j=1;j+1<polygon.size();++j){result.push_back(polygon[0]);result.push_back(polygon[j]);result.push_back(polygon[j+1]);}
  }
  const auto plane=center+inward*cut;
  auto u=XMVector3Normalize(XMVector3Cross(out,std::fabs(XMVectorGetY(out))<.9f?XMVectorSet(0,1,0,0):XMVectorSet(1,0,0,0))),v=XMVector3Cross(out,u);
  auto angle=[&](const XMFLOAT3& p){auto d=XMLoadFloat3(&p)-plane;return std::atan2(XMVectorGetX(XMVector3Dot(d,v)),XMVectorGetX(XMVector3Dot(d,u)));};
  std::sort(boundary.begin(),boundary.end(),[&](auto a,auto b){return angle(a)<angle(b);});
  boundary.erase(std::unique(boundary.begin(),boundary.end(),[](auto a,auto b){return XMVectorGetX(XMVector3LengthSq(XMLoadFloat3(&a)-XMLoadFloat3(&b)))<1e-10f;}),boundary.end());
  XMFLOAT3 capNormal;XMStoreFloat3(&capNormal,out);
  auto vertex=[&](XMVECTOR p){psxAssets::Vertex q=skin;XMFLOAT3 pp;XMStoreFloat3(&pp,p);
   q.value[0]=pp.x;q.value[1]=pp.y;q.value[2]=pp.z;q.value[3]=capNormal.x;q.value[4]=capNormal.y;q.value[5]=capNormal.z;
   if(!haveSkin){q.value[6]=.65f;q.value[7]=.36f;q.value[8]=.25f;q.value[11]=4;}
   return q;};
  // Close the cut on one plane, using the adjacent wrist's skin material.
  // A fixed local skin sample avoids stretching fingers across the cap.
  for(size_t j=0;j<boundary.size();++j){result.push_back(vertex(plane));result.push_back(vertex(XMLoadFloat3(&boundary[j])));result.push_back(vertex(XMLoadFloat3(&boundary[(j+1)%boundary.size()])));}
  section.first=first;section.count=unsigned(result.size())-first;++shortened;
 }
 pack.vertices=std::move(result);
}
}
