#include "../src/hand_curl_transition.hpp"
#include <cassert>
#include <cstdio>
int main(){handCurl::Transition t;assert(t.update(false,100,true)==0);assert(t.update(true,110,true)==0);
 float mid=t.update(true,160,true);assert(mid>.49f&&mid<.51f);
 assert(t.update(false,160,true)==mid);assert(t.update(false,210,true)<mid);assert(t.update(false,260,true)==0);
 t.update(true,270,true);assert(t.update(true,370,true)==1);assert(t.update(false,600,true)==0);
 assert(t.update(true,500,true)==1);assert(t.update(true,510,false)==0);assert(t.update(false,520,true)==0);
 puts("PASS: smooth curl, reversal continuity, exact endpoints and tracking/time reset");}
