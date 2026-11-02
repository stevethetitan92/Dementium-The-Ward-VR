#include "../src/native_fog.hpp"
#include <cassert>
#include <cstdio>
int main(){
    nativeFog::enabled=false;
    nativeFog::diagnosticUntil=0;
    assert(!nativeFog::active(100));
    nativeFog::diagnosticUntil=45100;
    assert(nativeFog::active(100));
    assert(nativeFog::active(45099));
    assert(!nativeFog::active(45100));
    assert(!nativeFog::active(45101));
    assert(!nativeFog::enabled.load());
    nativeFog::diagnosticUntil=0;
    assert(!nativeFog::active(101));
    nativeFog::enabled=true;
    assert(nativeFog::active(100000));
    puts("PASS: temporary fog expiry/cancel preserve saved configuration");
}
