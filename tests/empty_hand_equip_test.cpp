#include "../src/weapon_groups.hpp"
#include <cassert>
#include <cstdio>
int main(){
    weaponGroups::Requests requests;
    assert(!requests.submit(-1,-1,10));
    assert(!requests.submit(-2,5,10));
    assert(!requests.submit(-1,7,10));
    assert(requests.submit(-1,5,11));
    auto request=requests.take();
    assert(request.current==-1&&request.target==5&&request.time==11);
    request=requests.take();assert(request.current==-1&&request.target==-1);
    assert(requests.submit(6,5,12));
    request=requests.take();assert(request.current==6&&request.target==5);
    puts("Explicit owned-weapon selection can recover from empty hands; empty cycle and invalid IDs rejected.");
}
