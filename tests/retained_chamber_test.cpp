#include "../src/arsenal_core.hpp"
#include <cassert>
#include <cstdio>
int main(){
 for(bool retained:{false,true})for(bool heldSeat:{false,true}){
  manualReload::Pistol p;manualReload::Input e;e.tracked=e.gameplay=e.pistol=true;e.chamberLoaded=retained;p.update(e);
  e.releaseMagazine=true;p.update(e);e.releaseMagazine=false;e.leftGrip=true;e.ammoPouchDistance=0;p.update(e);
  e.chamberLoaded=!retained; // Later native values cannot replace the ejection snapshot.
  e.magazineDistance=0;e.leftGrip=heldSeat;p.update(e);
  assert(p.stage()==(retained?manualReload::Stage::CommitPending:manualReload::Stage::Inserted));
  if(retained){int loaded=3,reserve=20;unsigned calls=0;
   assert(manualReload::completeReload(p,loaded,reserve,15,[&]{++calls;manualReload::Transfer t;assert(manualReload::transfer(loaded,reserve,15,t));loaded=t.loaded;reserve=t.reserve;return true;}));
   assert(loaded==15&&reserve==8&&calls==1&&!p.blocksFire());
   assert(!manualReload::completeReload(p,loaded,reserve,15,[]{assert(false);return true;}));}
  arsenal::Reload r;arsenal::Event a;a.valid=true;a.chamberLoaded=retained;r.update(arsenal::Kind::Rifle,a);
  a.release=true;r.update(arsenal::Kind::Rifle,a);a.release=false;a.grip=true;a.atPouch=true;r.update(arsenal::Kind::Rifle,a);
  a.chamberLoaded=!retained;a.atPort=true;a.seatContact=heldSeat;a.grip=heldSeat;r.update(arsenal::Kind::Rifle,a);
  assert(r.state()==(retained?arsenal::Phase::Pending:arsenal::Phase::Inserted));
  if(retained){auto token=r.token();assert(r.acknowledge(token,true)&&!r.blocked());assert(!r.acknowledge(token,true));}
 }
 puts("PASS: pistol/M16 retained chamber, empty reloads still require rack, held/released insertion, ammo conservation and single commit");
}
