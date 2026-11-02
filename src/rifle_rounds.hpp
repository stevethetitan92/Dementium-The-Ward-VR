#pragma once
#include "selected_rifle_profile.hpp"
namespace rifleRounds {
using namespace DirectX;
inline unsigned first=~0u;
inline XMFLOAT3 feedOffset{};
inline bool contains(unsigned id){return first!=~0u&&id>=first&&id<first+4;}
inline bool append(psxAssets::Pack& pack){
 first=~0u;feedOffset={};
 if(!selectedRifle::profile.ready||!selectedFrames::ready||!selectedBallistics::ready[2])return false;
 const auto source=pack.sections[selectedRifle::profile.sections[1]];
 if(source.count!=32928||source.weapon!=2||selectedRifle::boltSection>=pack.sections.size())return false;
 const auto frame=XMLoadFloat4x4(&selectedFrames::frames[2].editorToAim);
 // Four complete supplied cartridges, ordered from the feed lips down.
 // Keep each case and projectile together; do not cut triangles at a height.
 const XMFLOAT2 centers[]={{-.042829f,.077631f},{-.03486f,.07216f},{-.042829f,.066491f},{-.03486f,.061021f}};
 std::array<std::vector<psxAssets::Vertex>,4> rounds;
 for(unsigned i=0;i<source.count;i+=3){unsigned owner=4;
  for(unsigned j=0;j<3;++j){const auto& v=pack.vertices[source.first+i+j];XMFLOAT3 q;
   XMStoreFloat3(&q,XMVector3TransformCoord(XMVectorSet(v.value[0],v.value[1],v.value[2],1),frame));
   unsigned nearest=0;float distance=10;
   for(unsigned k=0;k<4;++k){float x=q.x-centers[k].x,y=q.y-centers[k].y,d=x*x+y*y;if(d<distance){nearest=k;distance=d;}}
   if(distance>.000026f||(owner!=4&&owner!=nearest))return false;owner=nearest;
  }
  for(unsigned j=0;j<3;++j)rounds[owner].push_back(pack.vertices[source.first+i+j]);
 }
 for(const auto& r:rounds)if(r.size()!=8232)return false;
 float rear=-10,front=10,boltFront=10;
 for(const auto& v:rounds[0]){auto p=XMVector3TransformCoord(XMVectorSet(v.value[0],v.value[1],v.value[2],1),frame);float z=XMVectorGetZ(p);rear=(std::max)(rear,z);front=(std::min)(front,z);}
 const auto bolt=pack.sections[selectedRifle::boltSection];
 for(unsigned i=0;i<bolt.count;++i){auto v=pack.vertices[bolt.first+i].value;boltFront=(std::min)(boltFront,XMVectorGetZ(XMVector3TransformCoord(XMVectorSet(v[0],v[1],v[2],1),frame)));}
 const auto bore=selectedBallistics::muzzle[2];
 feedOffset={bore.x-centers[0].x,bore.y-centers[0].y,boltFront-rear};
 if(rear-front<.04f||rear-front>.07f||feedOffset.y<0||feedOffset.y>.04f||feedOffset.z>-.02f||feedOffset.z<-.1f)return false;
 first=unsigned(pack.sections.size());
 for(const auto& r:rounds){pack.sections.push_back({2,psxAssets::Weapon,unsigned(pack.vertices.size()),unsigned(r.size())});pack.vertices.insert(pack.vertices.end(),r.begin(),r.end());}
 return true;
}
inline unsigned count(bool valid,int loaded,int reserve,bool incoming){return valid&&loaded>=0&&reserve>=0?unsigned(std::clamp(incoming?loaded+reserve:loaded,0,4)):0;}
inline XMMATRIX fedWorld(const XMMATRIX& world,float amount){
 if(first==~0u||!std::isfinite(amount))return world;
 amount=std::clamp(amount,0.f,1.f);float lift=std::clamp(amount*2.f,0.f,1.f);
 auto frame=XMLoadFloat4x4(&selectedFrames::frames[2].editorToAim);
 return frame*XMMatrixTranslation(feedOffset.x*lift,feedOffset.y*lift,feedOffset.z*amount)*XMMatrixInverse(nullptr,frame)*world;
}
}
