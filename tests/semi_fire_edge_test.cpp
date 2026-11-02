#include "../src/semi_fire_edge.hpp"
#include <cassert>
#include <cstdio>
int main(){semiFire::Edge e;
 assert(!e.update(3,true));assert(!e.update(3,true));assert(e.update(4,true));assert(!e.update(4,true));
 assert(!e.update(5,false));assert(!e.update(5,true));assert(!e.update(5,true));assert(e.update(6,true));
 assert(!e.update(2,true));assert(e.update(3,true));
 puts("PASS fresh routed edges only; no held repeat, stale edge, reload/tracking carryover, or activation shot");
}
