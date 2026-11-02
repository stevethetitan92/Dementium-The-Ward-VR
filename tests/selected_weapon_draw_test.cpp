#include "../src/selected_weapon_draw.hpp"
#include <cassert>
#include <cstdio>
int main(){
 // Every validated replacement remains owned across a transition even when
 // no replacement frame was rendered; non-VR and unloaded packs fail open.
 for(unsigned weapon=0;weapon<7;weapon++){
  assert(selectedWeaponDraw::ownsBody(true,weapon,true));
  assert(!selectedWeaponDraw::ownsBody(true,weapon,false));
  assert(!selectedWeaponDraw::ownsBody(false,weapon,true));
 }
 assert(!selectedWeaponDraw::ownsBody(true,7,true));
 unsigned char buffer[1200]{};std::uint32_t enabled=1;
 std::memcpy(buffer+160,&enabled,4);
 assert(!selectedWeaponDraw::body(1200,buffer,sizeof buffer));
 {
  selectedWeaponDraw::Scope selected(true);
  assert(selectedWeaponDraw::body(1200,buffer,sizeof buffer));
  assert(!selectedWeaponDraw::body(128,buffer,sizeof buffer));
  assert(!selectedWeaponDraw::body(1200,nullptr,1200));
  assert(!selectedWeaponDraw::body(1200,buffer,163));
  {selectedWeaponDraw::Scope fallback(false);assert(!selectedWeaponDraw::body(1200,buffer,sizeof buffer));}
  assert(selectedWeaponDraw::body(1200,buffer,sizeof buffer));
  enabled=0;std::memcpy(buffer+160,&enabled,4);
  assert(!selectedWeaponDraw::body(1200,buffer,sizeof buffer));
  enabled=2;std::memcpy(buffer+160,&enabled,4);
  assert(!selectedWeaponDraw::body(1200,buffer,sizeof buffer));
 }
 assert(!selectedWeaponDraw::replacing);
 std::puts("PASS: held-body filtering, effect pipeline preservation, fail-open layouts and scope restoration");
}
