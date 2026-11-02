#include "../src/rifle_cadence.hpp"
#include <cassert>
#include <cstdio>
int main(){
 rifleCadence::Clock c;c.shot(100);
 assert(!c.ready(174)&&c.ready(175)); // rapid re-press cannot bypass interval
 assert(c.beforeNativeFrame(3,2,174,true)==-1);
 assert(c.beforeNativeFrame(3,2,175,true)==21);
 c.shot(175);assert(c.beforeNativeFrame(4,1,249,true)<7);
 assert(c.beforeNativeFrame(4,1,250,true)==7);
 // 60 Hz native updates alternate four/five ticks; retain fractional cadence.
 c.reset();c.shot(0);unsigned n=1;
 for(unsigned tick=1;tick<=600;++tick){auto t=(tick*1000ull)/60;
   if(c.beforeNativeFrame(4,1,t,true)==7){c.shot(t);++n;}}
 assert(n==134); // initial shot plus floor(10000/75)
 assert(c.beforeNativeFrame(4,1,10001,false)==-1);
 assert(c.beforeNativeFrame(4,1,10002,true)==-1);
 c.shot(20000);assert(c.beforeNativeFrame(4,1,19000,true)==-1);
 c.shot(30000);assert(c.beforeNativeFrame(4,90,30080,true)==-1);
 c.shot(40000);assert(c.beforeNativeFrame(4,1,41000,true)==7);c.shot(41000);
 assert(c.beforeNativeFrame(4,1,41001,true)<7); // no catch-up burst
 puts("PASS: 800RPM native-event schedule, frame quantization, interruption and no catch-up burst");
}
