#include "../src/magazine_exit.hpp"
#include "../src/selected_rifle_profile.hpp"
#include "../src/selected_hunting_contact.hpp"
#include "../src/drop_surface.hpp"
#include <cassert>
#include <cstdio>
int main(int argc,char**argv){assert(argc==2);psxAssets::Pack p;assert(psxAssets::load(argv[1],p));assert(selectedFrames::load(argv[1]));assert(selectedReload::loadProfile(argv[1],p));assert(selectedRifle::load(argv[1],p));selectedHunting::load(argv[1],p);
 for(unsigned w:{1u,2u,4u}){unsigned section=w==1?selectedReload::profile.sections[0]:w==2?selectedRifle::profile.sections[0]:selectedHunting::section;
  printf("binding weapon=%u section=%u\n",w,section);fflush(stdout);assert(magazineExit::bind(p,w,section));auto e=magazineExit::travel[w];auto axis=DirectX::XMVectorSet(e.x,e.y,e.z,0);float length=DirectX::XMVectorGetX(DirectX::XMVector3Length(axis));axis=DirectX::XMVector3Normalize(axis);
  float low=10,high=-10;const auto& s=p.sections[section];for(unsigned j=0;j<s.count;++j){auto v=p.vertices[s.first+j].value;float d=DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMVectorSet(v[0],v[1],v[2],0),axis));low=(std::min)(low,d);high=(std::max)(high,d);}
  assert(low+length>high+.009f);
  if(w==2||w==4){auto rail=manualReload::magazineExitAxes[w];assert(std::fabs(rail.x)<1e-6f&&std::fabs(rail.y+1)<1e-6f&&std::fabs(rail.z)<1e-6f);}
  printf("weapon=%u clearance travel=%.4fm\n",w,length);
 }
 assert(magazineExit::fraction(0)==0&&magazineExit::fraction(.09f)==.5f&&magazineExit::fraction(.18f)==1);
 for(float speed:{-1.f,0.f,1.f}){auto m=dropSurface::motion(1,0,false,1,0,speed);assert(m.y==1);float dt=.0001f;auto next=dropSurface::motion(1,0,false,1,dt,speed);assert(std::fabs((next.y-m.y)/dt+speed)<.002f);}
 puts("PASS: measured full-magazine axial clearance, exit timing and continuous ballistic velocity");
}
