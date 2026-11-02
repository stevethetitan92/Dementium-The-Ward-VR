#pragma once
#include "selected_frames.hpp"
#include <algorithm>
#include <atomic>
namespace mountedLight {
inline std::atomic<std::uint64_t> diagnosticUntil{};
inline std::array<XrVector3f,7> offsets{};
inline std::array<unsigned,7> tape{~0u,~0u,~0u,~0u,~0u,~0u,~0u};
inline bool eligible(int weapon){return weapon>=2&&weapon<=4&&tape[weapon]!=~0u;}
inline XrPosef pose(int weapon,const XrPosef& gun){
 using namespace DirectX;auto result=gun;const auto o=offsets[weapon];XMFLOAT3 p;
 XMStoreFloat3(&p,XMVector3TransformNormal(XMVectorSet(o.x,o.y,o.z,0),motionTracking::rotation(gun.orientation)));
 result.position.x+=p.x;result.position.y+=p.y;result.position.z+=p.z;return result;
}
struct Point{float x,y;};
inline float cross(Point a,Point b,Point c){return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);}
inline void append(psxAssets::Pack& pack){
 using namespace DirectX;tape.fill(~0u);if(!selectedFrames::ready)return;
 for(unsigned w=2;w<=4;++w){
  const auto matrix=XMLoadFloat4x4(&selectedFrames::frames[w].editorToAim);float front=10;std::vector<XMFLOAT3> points;
  for(const auto& s:pack.sections)if(s.weapon==w&&s.kind==psxAssets::Weapon)
   for(unsigned j=0;j<s.count;++j){auto p=pack.vertices[s.first+j].value;XMFLOAT3 q;XMStoreFloat3(&q,XMVector3TransformCoord(XMVectorSet(p[0],p[1],p[2],1),matrix));points.push_back(q);front=(std::min)(front,q.z);}
  if(front>-.2f||front< -2)continue;
  XMFLOAT3 low{10,10,front},high{-10,-10,front};unsigned count=0;
  for(auto p:points)if(p.z<front+.001f){low.x=(std::min)(low.x,p.x);low.y=(std::min)(low.y,p.y);high.x=(std::max)(high.x,p.x);high.y=(std::max)(high.y,p.y);++count;}
  if(count<3||high.x-low.x>.1f||high.y-low.y>.1f)continue;
  const float bx=(low.x+high.x)*.5f,by=(low.y+high.y)*.5f;
  const float separation=w==2?.065f:.037f;
  offsets[w]={bx+separation,by,front+.21f};
  // Convex tape wrap around the barrel and flashlight handle, not floating rings.
  std::vector<Point> samples;
  for(unsigned side=0;side<2;++side)for(unsigned j=0;j<32;++j){float a=j*XM_2PI/32,r=side?.0185f:(w==2?.035f:.0125f);samples.push_back({bx+(side?separation:0)+r*std::cos(a),by+r*std::sin(a)});}
  std::sort(samples.begin(),samples.end(),[](Point a,Point b){return a.x<b.x||(a.x==b.x&&a.y<b.y);});
  std::vector<Point> hull;for(auto p:samples){while(hull.size()>1&&cross(hull[hull.size()-2],hull.back(),p)<=0)hull.pop_back();hull.push_back(p);}auto lower=hull.size();
  for(auto i=samples.rbegin()+1;i!=samples.rend();++i){while(hull.size()>lower&&cross(hull[hull.size()-2],hull.back(),*i)<=0)hull.pop_back();hull.push_back(*i);}hull.pop_back();
  const auto start=unsigned(pack.vertices.size());
  for(float band:{-.035f,.035f})for(unsigned j=0;j<hull.size();++j){auto a=hull[j],b=hull[(j+1)%hull.size()];float dx=b.x-a.x,dy=b.y-a.y,len=std::sqrt(dx*dx+dy*dy);float z=offsets[w].z+band;
   auto vertex=[&](Point p,float depth){return psxAssets::Vertex{{p.x,p.y,depth,dy/len,-dx/len,0,.025f,.025f,.025f,0,0,4}};};
   auto v0=vertex(a,z-.005f),v1=vertex(b,z-.005f),v2=vertex(b,z+.005f),v3=vertex(a,z+.005f);pack.vertices.insert(pack.vertices.end(),{v0,v1,v2,v0,v2,v3});}
  tape[w]=unsigned(pack.sections.size());pack.sections.push_back({7,psxAssets::Weapon,start,unsigned(pack.vertices.size())-start});
 }
}
}
