#include "../src/medicine_effect.hpp"
#include <cassert>
#include <cstdio>
int main(){
    unsigned deficit=4,calls=0;bool allowed=true,works=true;
    auto use=[&](int id){return medicine::heal(id,[&]{return deficit;},[&](int){return allowed;},[&](int){++calls;if(works)deficit=deficit>2?deficit-2:0;});};
    assert(use(600)&&deficit==2&&calls==1);
    assert(use(999)&&deficit==0&&calls==2);
    assert(!use(600)&&calls==2); // full health retains bottle
    deficit=4;works=false;assert(!use(600)&&deficit==4&&calls==3);
    allowed=false;assert(!use(600)&&calls==3);
    allowed=true;assert(!use(599)&&!use(1000)&&calls==3);
    deficit=255;assert(!use(600)&&calls==3);
    puts("Medicine requires a real health improvement; failed effects, full health, and invalid items retain the bottle.");
}
