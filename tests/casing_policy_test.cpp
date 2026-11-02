#include "../src/casing_policy.hpp"
#include <cassert>
#include <cstdio>
int main(){casingPolicy::Ejection p;
 for(unsigned w:{1u,2u,4u}){assert(p.update(w,0,false,false,true)==0);assert(p.update(w,1,false,false,true)==1);assert(p.update(w,1,false,false,true)==0);}
 assert(p.update(0,0,false,false,true)==0);assert(p.update(0,1,false,false,true)==0);
 assert(p.update(0,2,false,false,true)==0);assert(p.update(0,2,true,false,true)==0);
 assert(p.update(0,2,true,false,true)==0);assert(p.update(0,2,false,false,true)==0);assert(p.update(0,2,true,false,true)==0);
 assert(p.update(3,0,false,false,true)==0);assert(p.update(3,1,false,false,true)==0);
 assert(p.update(3,1,false,true,true)==1);assert(p.update(3,1,false,true,true)==0);
 assert(p.update(3,1,false,false,true)==0);assert(p.update(3,1,false,true,true)==0);
 assert(p.update(2,10,false,false,false)==0);assert(p.update(2,10,false,false,true)==0);
 assert(p.update(2,11,false,false,true)==1);
 p.reset();assert(p.update(0,40,true,false,true)==0);
 for(unsigned shot=3;shot<30;++shot){assert(p.update(0,shot,false,false,true)==0);assert(p.update(0,shot,true,false,true)==0);}
 puts("PASS: self-loading shot ejection, no revolver ejection, shotgun pump only, no duplicates or stale bursts");
}
