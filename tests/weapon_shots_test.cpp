#include "../src/weapon_shots.hpp"
#include <cassert>
#include <cstdio>
int main(){
 weaponShots::Observer o;o.sample(6,24,100,true);assert(o.serial==0);
 o.sample(5,24,110,true);assert(o.serial==1&&o.fired==110);
 o.sample(5,24,111,true);assert(o.serial==1);
 o.sample(6,23,120,true);assert(o.serial==1); // reload, not a shot
 o.sample(5,23,400,true);assert(o.serial==1); // stale observation
 o.sample(4,23,410,false);o.sample(3,23,420,true);assert(o.serial==1);
 o.sample(2,23,430,true);assert(o.serial==2);
 o.sample(1,23,420,true);assert(o.serial==2); // clock rollback
 for(unsigned i=0;i<5;++i){assert(weaponShots::kick(i,100,100,true).pitch==0);
   auto k=weaponShots::kick(i,100,125,true);assert(k.pitch>0&&k.back>0);
   assert(weaponShots::kick(i,100,500,true).pitch==0);
   assert(weaponShots::kick(i,100,125,false).pitch==0);}
 puts("PASS: actual ammo-decrement events, reload/stale/invalid exclusion and bounded per-gun recoil");
}
