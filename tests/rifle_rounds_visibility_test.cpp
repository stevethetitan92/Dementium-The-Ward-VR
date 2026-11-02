#include "../src/selected_rifle_profile.hpp"
#include <cassert>
#include <cstdio>
int main(){using arsenal::Phase;using selectedRifle::roundsVisible;
 assert(!roundsVisible(Phase::Ready,true,0,100));
 assert(roundsVisible(Phase::Ready,true,1,0));
 assert(!roundsVisible(Phase::Open,true,30,100));
 for(auto p:{Phase::Held,Phase::Inserted,Phase::Charging}){
   assert(roundsVisible(p,true,0,100));assert(!roundsVisible(p,true,0,0));
   assert(!roundsVisible(p,false,30,100));}
 puts("PASS: empty magazine hides rounds; incoming rounds visible before native transfer; invalid ammo hides");
}
