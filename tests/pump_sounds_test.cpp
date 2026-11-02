#include "../src/pump_sounds.hpp"
#include <cassert>
#include <cstdio>
int main(){
 pumpSounds::Stroke s;
 for(int cycle=0;cycle<8;++cycle){
  assert(s.update(true,true,false,0)==0);
  assert(s.update(true,true,true,.015f)==1);
  assert(s.update(true,true,true,.084f)==0);
  assert(s.update(false,true,false,.084f)==0);
  assert(s.update(true,true,true,.084f)==0);
  assert(s.update(true,true,true,.072f)==2);
  assert(s.update(true,true,true,.084f)==0); // rear-stop bounce
  assert(s.update(true,true,true,.03f)==0);
  assert(s.update(true,true,true,0)==0);
 }
 assert(s.update(true,true,true,.02f)==1);
 assert(s.update(true,true,true,.005f)==0); // short stroke cannot play closure
 assert(s.update(true,false,false,0)==0);
 puts("PASS: eight held-grip pump cycles, dropout, repeated samples, rear-stop bounce and short stroke audio");
}
