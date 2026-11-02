#include "../src/wristband_mesh.hpp"
#include <cassert>
#include <iostream>
int main(){
 using namespace wristband;
 bool percent=false;for(auto g:numberGlyphs)if(g.code=='%')percent=g.count>0;assert(percent);
 Snapshot s;s.healthValid=true;s.maximum=10;s.ammoValid=true;s.weapon=2;s.reserve=100;
 for(int hp=0;hp<=10;++hp){
  s.health=hp;auto c=healthColor(s);
  if(hp>=8)assert(c.g>c.r&&c.g>c.b);
  else if(hp>=5)assert(c.r>.8f&&c.g>.8f&&c.b<.1f);
  else assert(c.r>.8f&&c.g<.2f&&c.b<.1f);
  std::vector<Vertex> ekg;ekgMesh(ekg,c);assert(ekg.size()>=42);
  for(size_t i=ekg.size()-36;i<ekg.size();++i)assert(ekg[i].r==c.r&&ekg[i].g==c.g&&ekg[i].b==c.b);
  auto full=statusMesh(s);if(hp>0)assert(full[ekg.size()].r==c.r&&full[ekg.size()].g==c.g&&full[ekg.size()].b==c.b);
 }
 s.healthValid=false;auto unknown=healthColor(s);assert(unknown.r==ivory.r&&unknown.g==ivory.g);s.healthValid=true;
 for(int hp=0;hp<=10;++hp){s.health=hp;assert(std::lround(healthFill(s)*100)==hp*10);for(int rounds:{0,1,30}){s.loaded=rounds;auto mesh=statusMesh(s);assert(!mesh.empty()&&mesh.size()<60000);for(auto v:mesh)assert(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z));}}
 s.health=5;s.loaded=12;auto a=statusMesh(s);s.medsValid=true;s.meds=3;auto b=statusMesh(s);assert(a.size()==b.size());
 Model m;s.time=100;s.world=true;m.publish(s);auto stale=m.read(350);assert(!stale.healthValid&&!stale.ammoValid);assert(!statusMesh(stale).empty());
 std::cout<<"PASS health 0..100, zero/partial/full ammo, percent glyph, no meds, stale data, finite bounded geometry\n";
}

