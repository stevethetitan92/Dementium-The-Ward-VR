#include "../src/arsenal_core.hpp"
#include <cstdio>
#include <stdexcept>
using namespace arsenal;
void check(bool value){if(!value)throw std::runtime_error("all-gun insertion failed");}
int main(){try{
 for(bool magazine:{false,true}){
  check(!canGrabSpare(0,0,6,magazine));check(!canGrabSpare(-1,6,6,magazine));check(!canGrabSpare(7,6,6,magazine));
  check(canGrabSpare(2,1,6,magazine));check(canGrabSpare(6,1,6,magazine)==magazine);
  check(canGrabSpare(2,0,6,magazine)==magazine); // retained rounds can be reseated without reserve
 }
 for(auto kind:{Kind::Revolver,Kind::Rifle,Kind::Sniper}){
  Reload r;r.useDetachableMagazine(kind==Kind::Sniper);
  Event e;e.valid=true;r.update(kind,e);e.release=true;r.update(kind,e);
  check(r.state()==Phase::Open);
  if(kind==Kind::Sniper){
   e.release=false;r.update(kind,e);e.release=true;r.update(kind,e);
   check(r.state()==Phase::Open&&!r.token()&&r.boltOffset()==0);
  }
  e.release=false;e.grip=e.atPouch=true;r.update(kind,e);check(r.state()==Phase::Held);
  e.atPort=true;r.update(kind,e);check(r.state()==Phase::Held);
  e.seatContact=true;r.update(kind,e);
  if(kind==Kind::Rifle){
   check(r.state()==Phase::Inserted&&!r.token());
   e.atHandle=true;e.travel=.08f;r.update(kind,e);check(r.state()==Phase::Inserted);
   e.grip=false;r.update(kind,e);e.grip=true;e.travel=0;r.update(kind,e);check(r.state()==Phase::Charging);
  }else{
   auto token=r.token();check(token&&r.rounds()==(kind==Kind::Sniper?~0u:1u));
   r.update(kind,e);check(r.token()==token);
   check(r.acknowledge(token,true)&&!r.acknowledge(token,true));
   r.update(kind,e);check(!r.token()&&r.state()==(kind==Kind::Sniper?Phase::Ready:Phase::Open));
   if(kind==Kind::Sniper){r.requireCycle();check(r.state()==Phase::Ready);}
  }
 }
 {Reload r;Event e;e.valid=true;r.update(Kind::Shotgun,e);r.requireCycle();
  e.release=true;r.update(Kind::Shotgun,e);check(r.state()==Phase::Inserted&&r.blocked());
  e.release=false;e.grip=e.atPouch=true;r.update(Kind::Shotgun,e);
  e.atPort=e.seatContact=true;r.update(Kind::Shotgun,e);check(r.token()&&r.rounds()==1);
  check(r.acknowledge(r.token(),true)&&r.blocked());
  e.grip=false;r.update(Kind::Shotgun,e);e.release=true;r.update(Kind::Shotgun,e);
  check(r.state()==Phase::Inserted&&r.blocked());
 }
 puts("PASS: all firearm push insertion, fresh grip ownership, single commit, hunting rifle no bolt, shotgun post-shot loading retains pump gate");
 return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}
