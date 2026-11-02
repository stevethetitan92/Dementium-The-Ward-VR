#include "../src/arsenal_core.hpp"
#include <cstdio>
#include <stdexcept>
void check(bool b){if(!b)throw std::runtime_error("speedloader regression");}
int main(){try{
 using namespace arsenal;
 for(int loaded=0;loaded<=6;++loaded)for(int reserve=0;reserve<=12;++reserve){
  if(!canGrabSpare(loaded,reserve,6,false)){check(loaded==6||reserve==0);continue;}
  Reload r;r.useSpeedloader(true);Event e;e.valid=true;r.update(Kind::Revolver,e);
  e.release=true;r.update(Kind::Revolver,e);e.release=false;e.grip=e.atPouch=true;r.update(Kind::Revolver,e);check(r.state()==Phase::Held);
  e.atPort=e.seatContact=true;r.update(Kind::Revolver,e);check(r.token()&&r.rounds()==~0u);
  manualReload::Transfer t;check(feed(loaded,reserve,6,r.rounds(),t));
  check(t.amount==(std::min)(6-loaded,reserve)&&t.loaded+t.reserve==loaded+reserve);
  auto token=r.token();check(r.acknowledge(token,true)&&!r.acknowledge(token,true));
  r.update(Kind::Revolver,e);check(r.state()==Phase::Open&&!r.token());
  // Keeping the empty loader gripped cannot grab or transfer another batch.
  r.update(Kind::Revolver,e);check(r.state()==Phase::Open&&!r.token());
 }
 puts("PASS: partial/full speedloader amounts, empty/full grab rejection, ammo conservation and one transfer per squeeze");return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}
