#include "../src/inventory_layout.hpp"
#include <cassert>
#include <cstdio>
int main(){using namespace inventoryLayout;
 for(unsigned i=0;i<count;i++){
  const auto a=slots[i];assert(a.x>=0&&a.y>=0&&a.x+a.w<=width&&a.y+a.h<=height);
  for(unsigned j=i+1;j<count;j++){const auto b=slots[j];assert(a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y);}
 }
 assert(neighbor(pill,Direction::right)==mapSlot);
 assert(neighbor(mapSlot,Direction::left)==pill);
 assert(neighbor(0,Direction::right)==1);
 assert(neighbor(0,Direction::down)==4);
 assert(neighbor(7,Direction::right)==7);
 // All item slots must be reachable by buttons, including the last key slot.
 bool seen[count]{};seen[0]=true;
 for(unsigned pass=0;pass<count;pass++)for(unsigned i=0;i<count;i++)if(seen[i])
  for(auto d:{Direction::left,Direction::right,Direction::up,Direction::down})seen[neighbor(i,d)]=true;
 for(bool reachable:seen)assert(reachable);
 puts("PASS: reference layout bounds, separation, spatial navigation and all 24 regions reachable");
}
