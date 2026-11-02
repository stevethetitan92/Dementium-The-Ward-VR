#pragma once
#include "arsenal_props.hpp"
#include "magazine_exit.hpp"
namespace magazineDrop {
using namespace DirectX;
struct Object {XMMATRIX world;XMFLOAT3 pivot,exit;std::vector<unsigned> sections;std::uint64_t born;float units;unsigned weapon;XrVector3f center;std::shared_ptr<dropSurface::Query> surface;};
inline std::vector<Object> objects;
inline std::array<bool,7> known{},empty{};
inline std::array<XMMATRIX,7> previous;
inline std::array<std::vector<unsigned>,7> previousSections;
inline std::uintptr_t scene{};inline std::uint64_t last{};
inline std::atomic<std::uint64_t> captureUntil{};
inline void(*capture)(){};
inline void update(unsigned weapon,bool isEmpty,const XMMATRIX& current,XrVector3f center,const std::vector<unsigned>& sections,std::uint64_t now,float units,bool valid){
 XMMATRIX mapping;std::uintptr_t currentScene{};
 if(!valid||weapon>=7||!arsenalProps::worldFrame||!arsenalProps::worldFrame(mapping,currentScene)){known.fill(false);objects.clear();return;}
 if(currentScene!=scene||now<last||(last&&now-last>150)){known.fill(false);objects.clear();scene=currentScene;}last=now;
 if(known[weapon]&&!empty[weapon]&&isEmpty&&!previousSections[weapon].empty()){
  if(objects.size()>=16)objects.erase(objects.begin());
  auto world=previous[weapon]*mapping;XMFLOAT3 pivot;XMStoreFloat3(&pivot,XMVector3TransformCoord(XMVectorSet(center.x,center.y,center.z,1),world));
  auto e=magazineExit::travel[weapon];XMFLOAT3 exit;XMStoreFloat3(&exit,XMVector3TransformNormal(XMVectorSet(e.x,e.y,e.z,0),world));
  objects.push_back({world,pivot,exit,previousSections[weapon],now,units,weapon,center,{}});
 }
 // While still inside the well, the rail moves with its weapon. Freeze that
 // last guided frame for free flight only after the magazine clears it.
 for(auto& o:objects)if(o.weapon==weapon&&now>=o.born&&float(now-o.born)*.001f<magazineExit::duration){
  o.world=current*mapping;
  XMStoreFloat3(&o.pivot,XMVector3TransformCoord(XMVectorSet(o.center.x,o.center.y,o.center.z,1),o.world));
  auto e=magazineExit::travel[weapon];XMStoreFloat3(&o.exit,XMVector3TransformNormal(XMVectorSet(e.x,e.y,e.z,0),o.world));
 }
 known[weapon]=true;empty[weapon]=isEmpty;previous[weapon]=current;previousSections[weapon]=sections;
}
template<class Draw> void draw(std::uint64_t now,Draw submit){
 XMMATRIX mapping;std::uintptr_t currentScene{};if(!arsenalProps::worldFrame||!arsenalProps::worldFrame(mapping,currentScene)||scene!=currentScene)return;
 objects.erase(std::remove_if(objects.begin(),objects.end(),[&](const Object& o){return now<o.born||now-o.born>8000;}),objects.end());
 auto inverse=XMMatrixInverse(nullptr,mapping);
 for(auto& o:objects){float t=float(now-o.born)*.001f;const auto& p=o.pivot;
  const auto& e=o.exit;const float f=magazineExit::fraction(t);
  if(t>=.09f&&t<magazineExit::duration&&captureUntil.load()>now&&captureUntil.exchange(0)&&capture)capture();
  XMMATRIX model;
  if(f<1)model=o.world*XMMatrixTranslation(e.x*f,e.y*f,e.z*f)*inverse;
  else {if(!o.surface)o.surface=dropSurface::request(currentScene,p.x+e.x,p.y+e.y,p.z+e.z,o.units);
   float floor{};bool hit=dropSurface::floor(o.surface,floor);auto motion=dropSurface::motion(p.y+e.y,floor,hit,o.units,t-magazineExit::duration,-e.y/(magazineExit::duration*o.units));float spin=motion.spinTime;
   model=o.world*XMMatrixTranslation(-p.x,-p.y,-p.z)*XMMatrixRotationRollPitchYaw(spin*2,spin*.8f,spin*1.5f)*XMMatrixTranslation(p.x+e.x*(1+spin/magazineExit::duration),motion.y,p.z+e.z*(1+spin/magazineExit::duration))*inverse;}
  for(auto id:o.sections)submit(id,model);
 }
}
}
