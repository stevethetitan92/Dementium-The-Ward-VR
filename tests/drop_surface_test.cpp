#include "../src/drop_surface.hpp"
#include <cstdio>
#include <stdexcept>
void check(bool b){if(!b)throw std::runtime_error("drop floor regression");}
int main(){try{
 for(float units:{.01f,1.f,100.f}){
  float last=2*units;bool fell=false,bounced=false;
  for(unsigned i=0;i<800;++i){auto p=dropSurface::motion(2*units,.1f*units,true,units,i*.01f);
   check(std::isfinite(p.y)&&p.y>=.1f*units-1e-5f*units);fell|=p.y<last;bounced|=fell&&p.y>last;last=p.y;}
  check(fell&&bounced&&std::fabs(last-.1f*units)<1e-5f*units);
 }
 check(dropSurface::motion(0,.1f,true,1,1).y==.1f);
 check(dropSurface::motion(2,0,false,1,1).y<0);
 auto q=dropSurface::request(7,0,2,0,1);float floor{};check(!dropSurface::floor(q,floor));
 dropSurface::trace=[](const dropSurface::Query& q,float& y){y=.1f;return q.scene==7;};
 dropSurface::service();check(dropSurface::floor(q,floor)&&floor==.1f&&dropSurface::pending.empty());
 puts("PASS: floor collision, damped bounce/rest, world-scale invariance, no-hit gravity, queued publication");return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}
