#pragma once
#include "selected_frames.hpp"
#include <atomic>
namespace selectedHuntingFlash {
inline std::atomic<std::uint64_t> captureUntil{};
inline void(*capture)(){};
inline unsigned section=~0u;
inline bool visible(std::uint64_t shot,std::uint64_t now){return shot&&now>=shot&&now-shot<45;}
inline void append(psxAssets::Pack& pack){
 using namespace DirectX;section=~0u;if(!selectedFrames::ready)return;
 auto frame=XMLoadFloat4x4(&selectedFrames::frames[4].editorToAim);
 auto inverse=XMMatrixInverse(nullptr,frame);
 std::vector<XMFLOAT3> points;float front=10;
 // The measured weapon frame points down -Z. Only actual weapon geometry
 // contributes to the barrel tip; fitted hands and optional props do not.
 for(const auto& s:pack.sections)if(s.weapon==4&&s.kind==psxAssets::Weapon)
  for(unsigned i=0;i<s.count;++i){auto* v=pack.vertices[s.first+i].value;XMFLOAT3 p;
   XMStoreFloat3(&p,XMVector3TransformCoord(XMVectorSet(v[0],v[1],v[2],1),frame));
   front=(std::min)(front,p.z);points.push_back(p);}
 if(points.empty()||front>=0||front< -2)return;
 float x=0,y=0;unsigned n=0;for(auto p:points)if(p.z<front+.003f){x+=p.x;y+=p.y;++n;}
 if(!n)return;x/=n;y/=n;
 const auto start=static_cast<unsigned>(pack.vertices.size());
 for(unsigned core=0;core<2;++core)for(unsigned plane=0;plane<3;++plane){
  float radius=core?.012f:.025f,length=core?.055f:.105f,angle=plane*1.04719755f;
  float dx=std::cos(angle)*radius,dy=std::sin(angle)*radius;
  XMFLOAT3 triangle[]={{x-dx,y-dy,front-.002f},{x+dx,y+dy,front-.002f},{x,y,front-length}};
  for(auto p:triangle){XMFLOAT3 q;XMStoreFloat3(&q,XMVector3TransformCoord(XMVectorSet(p.x,p.y,p.z,1),inverse));
   psxAssets::Vertex v{};float values[]={q.x,q.y,q.z,0,1,0,1.f,core?.95f:.58f,core?.65f:.12f,0,0,4};
   std::memcpy(v.value,values,sizeof(values));pack.vertices.push_back(v);}
 }
 section=static_cast<unsigned>(pack.sections.size());pack.sections.push_back({4,psxAssets::Weapon,start,18});
}
}
