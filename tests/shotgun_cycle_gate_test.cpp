#include "../src/arsenal_core.hpp"
#include <cassert>
#include <cstdio>
int main(){using namespace arsenal;
 for(auto phase:{Phase::Ready,Phase::Inserted,Phase::Charging,Phase::Pending})assert(allowsSupport(Kind::Shotgun,phase));
 for(auto phase:{Phase::Open,Phase::Held,Phase::Opening})assert(!allowsSupport(Kind::Shotgun,phase));
 for(auto kind:{Kind::Rifle,Kind::Sniper}){assert(allowsSupport(kind,Phase::Ready));assert(!allowsSupport(kind,Phase::Held));}
 for(float travel:{.03f,.084f}){
  Reload r;Event e;e.valid=true;r.update(Kind::Shotgun,e);
  e.grip=true;r.update(Kind::Shotgun,e);r.requireCycle();assert(allowsSupport(Kind::Shotgun,r.state()));
  e.atHandle=true;r.update(Kind::Shotgun,e);assert(r.state()==Phase::Charging&&allowsSupport(Kind::Shotgun,r.state()));
  e.travel=travel;r.update(Kind::Shotgun,e);e.travel=0;e.cycleComplete=false;r.update(Kind::Shotgun,e);
  if(travel<.08f){assert(!r.token());e.grip=false;r.update(Kind::Shotgun,e);assert(r.state()==Phase::Inserted);}
  else {auto token=r.token();assert(token&&r.rounds()==0);assert(r.acknowledge(token,true));r.update(Kind::Shotgun,e);assert(r.state()==Phase::Ready&&!r.token());}
 }
 assert(manualReload::pouchDistance({-.23f,-.7f,.02f})<.001f);
 assert(manualReload::pouchDistance({.3f,-.7f,.02f})>.13f);
 assert(manualReload::pouchDistance({-.23f,-1.2f,.02f})>.13f);
 puts("PASS: continuous-grip pump, full physical stroke, incomplete stroke rejection and bounded hip zone");
}
