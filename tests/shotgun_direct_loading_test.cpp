#include "../src/arsenal_core.hpp"
#include <cassert>
#include <cstdio>
using namespace arsenal;
int main(){
 for(bool empty:{false,true})for(bool fired:{false,true})for(bool success:{false,true}){
  Reload r;Event e;e.valid=true;r.update(Kind::Shotgun,e);if(fired)r.requireCycle();
  // B does nothing to this action, and is never required for the pickup.
  auto start=r.state();e.release=true;r.update(Kind::Shotgun,e);assert(r.state()==start);e.release=false;
  e.grip=e.atPouch=true;e.emptyOnGrab=empty;r.update(Kind::Shotgun,e);assert(r.state()==Phase::Held);
  e.atPouch=false;e.atPort=e.seatContact=true;r.update(Kind::Shotgun,e);assert(r.token()&&r.rounds()==1);
  auto token=r.token();r.update(Kind::Shotgun,e);assert(r.token()==token);
  assert(r.acknowledge(token,success));assert(!r.acknowledge(token,true));
  assert(r.state()==((empty||fired)?Phase::Inserted:Phase::Ready));
  // Held grip cannot create a second shell transaction.
  e.atPort=e.seatContact=false;e.atPouch=true;r.update(Kind::Shotgun,e);assert(!r.token()&&r.state()!=Phase::Held);
  e.grip=false;r.update(Kind::Shotgun,e);e.grip=true;r.update(Kind::Shotgun,e);assert(r.state()==Phase::Held);
  // Dropping away or losing tracking preserves the chamber requirement.
  auto lost=r;e.valid=false;lost.update(Kind::Shotgun,e);assert(lost.state()==((empty||fired)?Phase::Inserted:Phase::Ready));
  e.valid=true;e.grip=false;e.atPouch=false;r.update(Kind::Shotgun,e);assert(r.state()==((empty||fired)?Phase::Inserted:Phase::Ready));
 }
 // After a shot: insert directly, then pump without ever pressing B.
 Reload r;Event e;e.valid=true;r.update(Kind::Shotgun,e);r.requireCycle();
 e.grip=e.atPouch=true;r.update(Kind::Shotgun,e);e.atPouch=false;e.atPort=e.seatContact=true;r.update(Kind::Shotgun,e);
 assert(r.acknowledge(r.token(),true)&&r.state()==Phase::Inserted);
 e.grip=false;e.atPort=e.seatContact=false;r.update(Kind::Shotgun,e);
 e.grip=e.atHandle=true;r.update(Kind::Shotgun,e);assert(r.state()==Phase::Charging);
 e.travel=.084f;r.update(Kind::Shotgun,e);e.travel=0;r.update(Kind::Shotgun,e);
 assert(r.token()&&r.rounds()==0);assert(r.acknowledge(r.token(),true)&&r.state()==Phase::Ready);
 // Firing/brief invalid samples cannot require releasing a retained support grip.
 for(unsigned shot=0;shot<8;++shot){
  r.requireCycle();e.valid=false;r.update(Kind::Shotgun,e);
  e.valid=true;e.supportHeld=e.grip=e.atHandle=true;e.travel=0;r.update(Kind::Shotgun,e);
  assert(r.state()==Phase::Charging&&!r.token());
  r.update(Kind::Shotgun,e);assert(!r.token());
  e.travel=.084f;r.update(Kind::Shotgun,e);e.travel=0;r.update(Kind::Shotgun,e);
  assert(r.token()&&r.rounds()==0);assert(r.acknowledge(r.token(),true));assert(r.state()==Phase::Ready);
 }
 puts("PASS: no-B top-up and empty/post-shot loading, one-shell commits, interruption recovery, retained pump requirement and continuous-grip pump");
}
