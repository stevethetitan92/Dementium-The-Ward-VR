#include "../src/melee_contact_trace.hpp"
#include <cassert>
int main(){meleeTrace::Buffer<2> b;std::array<meleeTrace::Event,2> out{};meleeTrace::Event e{};e.actor=42;b.push(e);assert(b.drain(out)==0);b.enabled=true;b.push(e);e.actor=43;b.push(e);b.push(e);assert(b.dropped==1);assert(b.drain(out)==2);assert(out[0].actor==42&&out[1].actor==43);assert(b.drain(out)==0);b.push(e);assert(b.drain(out)==1);}
