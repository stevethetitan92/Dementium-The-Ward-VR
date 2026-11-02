#include "../src/selected_rifle_profile.hpp"
#include <cassert>
#include <cstdio>
int main(){
 using selectedRifle::boltPull;
 assert(boltPull(0,true,1,0,100)==0);
 assert(boltPull(0,true,0,0,100)==.067f);
 assert(boltPull(.05f,true,10,0,100)==.05f);
 assert(boltPull(0,true,10,100,121)>.064f);
 assert(boltPull(0,true,10,100,175)==0);
 assert(boltPull(0,false,0,0,100)==0);
 for(unsigned i=100;i<1000;++i){float p=boltPull(0,true,10,100+(i-100)/75*75,i);assert(p>=0&&p<=.067f);}
 puts("PASS: bolt follows charging handle, holds empty, cycles at 800rpm, returns loaded, rejects unavailable ammo");
}

