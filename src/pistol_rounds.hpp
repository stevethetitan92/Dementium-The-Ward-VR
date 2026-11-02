#pragma once
#include "selected_reload_profile.hpp"
#include "selected_frames.hpp"
namespace pistolRounds {
inline unsigned first=~0u;
inline DirectX::XMFLOAT3 feedOffset{};
inline bool feedReady{};
// The supplied pistol barrel is section 62 in the hash-validated reload pack.
// Bind its breech, rather than placing a round at a controller-space guess.
inline void bindFeed(const psxAssets::Pack& pack){
 using namespace DirectX;feedReady=false;feedOffset={};
 if(!selectedReload::profile.ready||!selectedFrames::ready||first==~0u||pack.sections.size()<=62)return;
 const auto& barrel=pack.sections[62];const auto& round=pack.sections[first];
 if(barrel.weapon!=1||barrel.kind!=psxAssets::Weapon||barrel.count!=972)return;
 auto frame=XMLoadFloat4x4(&selectedFrames::frames[1].editorToAim);
 auto bounds=[&](const psxAssets::Section& s,XMFLOAT3& lo,XMFLOAT3& hi){
  lo={10,10,10};hi={-10,-10,-10};
  for(unsigned i=0;i<s.count;++i){const auto* v=pack.vertices[s.first+i].value;XMFLOAT3 p;
   XMStoreFloat3(&p,XMVector3TransformCoord(XMVectorSet(v[0],v[1],v[2],1),frame));
   lo.x=(std::min)(lo.x,p.x);lo.y=(std::min)(lo.y,p.y);lo.z=(std::min)(lo.z,p.z);
   hi.x=(std::max)(hi.x,p.x);hi.y=(std::max)(hi.y,p.y);hi.z=(std::max)(hi.z,p.z);}
 };
 XMFLOAT3 bl,bh,rl,rh;bounds(barrel,bl,bh);bounds(round,rl,rh);
 const float length=rh.z-rl.z;
 if(length<.015f||length>.04f||bh.z-bl.z<.1f||bh.x-bl.x>.04f||bh.y-bl.y>.04f)return;
 feedOffset={(bl.x+bh.x-rl.x-rh.x)*.5f,(bl.y+bh.y-rl.y-rh.y)*.5f,bh.z-length*.5f-(rl.z+rh.z)*.5f};
 feedReady=std::isfinite(feedOffset.x)&&std::isfinite(feedOffset.y)&&std::isfinite(feedOffset.z)&&
  std::fabs(feedOffset.x)<.03f&&feedOffset.y>0&&feedOffset.y<.06f&&feedOffset.z<0&&feedOffset.z>-.1f;
}
inline DirectX::XMMATRIX fedWorld(const DirectX::XMMATRIX& weaponWorld,float amount){
 using namespace DirectX;if(!feedReady||!std::isfinite(amount))return weaponWorld;
 amount=std::clamp(amount,0.f,1.f);
 // Lift onto the bore before the slide seats the cartridge forward.
 const float lift=std::clamp(amount*2.f,0.f,1.f);
 auto frame=XMLoadFloat4x4(&selectedFrames::frames[1].editorToAim);
 return frame*XMMatrixTranslation(feedOffset.x*lift,feedOffset.y*lift,feedOffset.z*amount)*XMMatrixInverse(nullptr,frame)*weaponWorld;
}
struct SourceVertex {float value[12];};
inline std::vector<SourceVertex> cartridge;
inline bool load(const std::filesystem::path& folder){
 cartridge.clear();std::vector<unsigned char> bytes;
 if(!psxAssets::read(folder/L"SelectedPistolRound.bin",bytes,4*1024*1024)||bytes.size()<12)return false;
 unsigned h[3];std::memcpy(h,bytes.data(),12);
 const unsigned stride=h[1]==1?9:12;
 if(h[0]!=0x52425044||(h[1]!=1&&h[1]!=2)||!h[2]||h[2]%3||h[2]>60000||bytes.size()!=12+size_t(h[2])*stride*sizeof(float))return false;
 std::vector<SourceVertex> candidate(h[2]);for(unsigned i=0;i<h[2];++i){std::memcpy(candidate[i].value,bytes.data()+12+size_t(i)*stride*sizeof(float),stride*sizeof(float));if(h[1]==1)candidate[i].value[11]=4;}
 for(const auto& v:candidate){
  for(float x:v.value)if(!std::isfinite(x))return false;
  for(unsigned i=0;i<3;++i)if(std::fabs(v.value[i])>.501f)return false;
  for(unsigned i=6;i<9;++i)if(v.value[i]<0||v.value[i]>1)return false;
  if(v.value[9]<0||v.value[9]>1||v.value[10]<0||v.value[10]>1||(v.value[11]!=4&&v.value[11]!=7))return false;
 }
 cartridge=std::move(candidate);return true;
}
inline bool contains(unsigned id){return first!=~0u&&id>=first&&id<first+15;}
inline void append(psxAssets::Pack& pack){
 using namespace DirectX;first=~0u;feedReady=false;if(!selectedReload::profile.ready||cartridge.empty())return;
 const auto s=pack.sections[selectedReload::profile.sections[0]];
 auto toGrip=XMLoadFloat4x4(&selectedFrames::frames[1].editorToAim),toEditor=XMMatrixInverse(nullptr,toGrip);
 float top=-10,bottom=10,xmin=10,xmax=-10;std::vector<XMFLOAT3> points;
 for(unsigned i=0;i<s.count;++i){auto* v=pack.vertices[s.first+i].value;XMFLOAT3 p;XMStoreFloat3(&p,XMVector3TransformCoord(XMVectorSet(v[0],v[1],v[2],1),toGrip));points.push_back(p);top=(std::max)(top,p.y);bottom=(std::min)(bottom,p.y);xmin=(std::min)(xmin,p.x);xmax=(std::max)(xmax,p.x);}
 float z=0;unsigned n=0;for(auto p:points)if(p.y>top-.008f){z+=p.z;++n;}if(!n)return;z/=n;
 float bottomZ=0;unsigned bottomCount=0;for(auto p:points)if(p.y<bottom+.008f){bottomZ+=p.z;++bottomCount;}if(!bottomCount||top-bottom<.02f)return;bottomZ/=bottomCount;
 first=static_cast<unsigned>(pack.sections.size());const auto& source=cartridge;
 for(unsigned round=0;round<15;++round){
  unsigned start=static_cast<unsigned>(pack.vertices.size());
  // Follow the fitted magazine's sloping spine and keep the bottom round
  // above its base; a vertical stack at the feed-lip Z exits the rear wall.
  float y=top-.002f-round*((top-bottom-.012f)/14.f);
  float along=std::clamp((top-y)/(top-bottom),0.f,1.f),roundZ=z+(bottomZ-z)*along;
  auto model=XMMatrixScaling(.009f,.009f,.028f)*XMMatrixRotationY(3.14159265f)*XMMatrixTranslation((xmin+xmax)*.5f,y,roundZ)*toEditor;
  for(size_t i=0;i<source.size();i+=3){XMVECTOR p[3];for(unsigned j=0;j<3;++j){auto* v=source[i+j].value;p[j]=XMVector3TransformCoord(XMVectorSet(v[0],v[1],v[2],1),model);}
   XMFLOAT3 normal;XMStoreFloat3(&normal,XMVector3Normalize(XMVector3Cross(p[1]-p[0],p[2]-p[0])));
   for(unsigned j=0;j<3;++j){XMFLOAT3 q;XMStoreFloat3(&q,p[j]);psxAssets::Vertex v{};const auto* c=source[i+j].value+6;float values[]={q.x,q.y,q.z,normal.x,normal.y,normal.z,c[0],c[1],c[2],c[3],c[4],c[5]};std::memcpy(v.value,values,sizeof values);pack.vertices.push_back(v);}
  }
  pack.sections.push_back({1,psxAssets::Weapon,start,static_cast<unsigned>(source.size())});
 }
 bindFeed(pack);
}
inline unsigned count(bool valid,int loaded,int reserve,bool incoming){if(!valid||loaded<0||reserve<0)return 0;return static_cast<unsigned>(std::clamp(incoming?loaded+reserve:loaded,0,15));}
}
