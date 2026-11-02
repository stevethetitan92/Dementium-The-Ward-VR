#include "../src/sniper_scope.hpp"
#include <cstdio>
#include <stdexcept>
void check(bool value){if(!value)throw std::runtime_error("scope validation failed");}
int main(){try{
    check(!sniperScope::enabled);
    sniperScope::visibleWeapon=4;sniperScope::weaponTime=100;
    sniperScope::update(100,true,0,false);sniperScope::update(110,true,1,false);
    check(sniperScope::factor()==1&&!sniperScope::engaged.load());
    sniperScope::enabled=true;
    sniperScope::Controller c;
    check(c.update(1000,true,1,false,3)==1); // Held before equipping must not enter.
    c.update(1010,true,0,false,3);
    float previous=1;
    for(unsigned i=1;i<=12;i++){
        float value=c.update(1010+i*10,true,1,false,3);check(value>=previous&&value<=3.0001f);previous=value;
    }
    check(std::fabs(previous-3)<.0001f&&c.active());
    check(c.update(1140,true,.5f,false,3)==3); // Hysteresis keeps it held.
    for(unsigned i=1;i<=12;i++)c.update(1140+i*10,true,0,false,3);
    check(!c.active()&&c.update(1270,true,0,false,3)==1);
    c.update(1280,true,1,false,3);check(c.active());
    check(c.update(1290,true,1,true,3)==1&&!c.active());
    check(c.update(1300,true,1,false,3)==1); // B cancels until release.
    c.update(1310,true,0,false,3);c.update(1320,true,1,false,3);
    check(c.update(1330,false,1,false,3)==1);
    check(c.update(1340,true,1,false,3)==1); // Tracking recovery cannot reopen it.
    sniperScope::visibleWeapon=0;sniperScope::weaponTime=1400;
    sniperScope::update(1400,true,0,false);sniperScope::update(1410,true,1,false);check(sniperScope::factor()==1);
    sniperScope::visibleWeapon=4;sniperScope::weaponTime=1500;
    sniperScope::update(1500,true,0,false);sniperScope::update(1510,true,1,false);check(sniperScope::engaged.load());
    sniperScope::update(1650,true,1,false);check(sniperScope::factor()==1); // Expired weapon observation.
    sniperScope::weaponTime=1700;sniperScope::update(1700,true,0,false);sniperScope::update(1710,true,1,false);
    sniperScope::enabled=false;sniperScope::update(1720,true,1,false);
    check(sniperScope::factor()==1&&!sniperScope::engaged.load());
    puts("PASS: scope disabled by default, disabling resets zoom, sniper-only scope, smooth zoom/release, trigger hysteresis, B cancel, weapon expiry and tracking recovery.");return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}
