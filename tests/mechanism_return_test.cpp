#include "../src/mechanism_return.hpp"
#include <cassert>
#include <cstdio>
int main(){mechanismReturn::Spring s;assert(s.update(.05f,true,100,true)==.05f);
 assert(s.seatedFraction()==0);
 assert(s.update(.03f,true,110,true)==.03f);assert(s.update(0,false,120,true)==.03f);assert(s.seatedFraction()==0);
 auto half=s.update(0,false,160,true);assert(half>0&&half<.03f);assert(std::fabs(s.seatedFraction()-.75f)<1e-5f);assert(s.update(0,false,200,true)==0);assert(s.seatedFraction()==1);
 assert(s.update(.06f,true,210,true)==.06f);assert(s.update(0,false,220,false)==0);
 s.update(.06f,true,230,true);assert(s.update(0,false,500,true)==0);
 mechanismReturn::LockedBolt bolt;
 assert(std::fabs(bolt.update(0,false,true,100,true)-.067f)<1e-6f);
 assert(std::fabs(bolt.update(.05f,true,true,120,true)-.067f)<1e-6f);
 assert(std::fabs(bolt.update(0,false,false,140,true)-.067f)<1e-6f); // native load must not snap to handle position
 assert(bolt.seatedFraction()==0);
 assert(std::fabs(bolt.update(0,false,false,180,true)-.01675f)<1e-6f);
 assert(std::fabs(bolt.seatedFraction()-.75f)<1e-6f);
 assert(bolt.update(0,false,false,220,true)==0&&bolt.seatedFraction()==1);
 assert(bolt.update(0,false,false,240,false)==0);
 puts("PASS: tracked handle, locked empty bolt continuity, synchronized feed fraction, exact rest and tracking reset");}

