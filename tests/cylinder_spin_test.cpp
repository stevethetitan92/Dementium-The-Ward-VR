#include "../src/cylinder_spin.hpp"
#include <cassert>
#include <cstdio>
int main(){
 cylinderSpin::Closing spin;
 assert(spin.update(0,100,true)==0);
 for(unsigned t=110;t<=300;t+=10)assert(spin.update(float(t-100)/200,t,true)==0);
 assert(spin.update(.9f,310,true)==0);
 float previous=0;
 for(unsigned t=320;t<730;t+=10){auto a=spin.update(t<400?(400.f-t)/100:0,t,true);assert(a>previous&&a<6.284f);previous=a;}
 assert(spin.update(0,730,true)==0);assert(spin.update(0,740,true)==0);
 spin.update(1,750,true);spin.update(.8f,760,true);assert(spin.update(1,770,true)==0);
 spin.update(.8f,780,true);assert(spin.update(0,790,false)==0);
 assert(spin.update(0,800,true)==0);
 puts("PASS: closing-only decelerating full cylinder turn, no opening spin, interruption reset");
}
