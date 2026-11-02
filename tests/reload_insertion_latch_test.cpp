#include "../src/manual_reload.hpp"
#include <stdexcept>
#include <cstdio>
void check(bool b){if(!b)throw std::runtime_error("insertion latch failed");}
int main(){try{
 using namespace manualReload;Pistol p;Input i;i.tracked=i.gameplay=i.pistol=true;
 p.update(i);i.releaseMagazine=true;p.update(i);i.releaseMagazine=false;p.update(i);
 i.ammoPouchDistance=0;i.leftGrip=true;p.update(i);check(p.stage()==Stage::MagazineHeld);
 i.magazineDistance=.08f;p.update(i);check(p.stage()==Stage::MagazineHeld);
 i.magazineDistance=.04f;p.update(i);check(p.stage()==Stage::MagazineHeld);
 i.magazineDistance=.03f;p.update(i);check(p.stage()==Stage::Inserted);
 check(p.blocksFire()&&p.pendingToken()==0);
 i.slideDistance=0;i.slidePull=.06f;p.update(i);check(p.stage()==Stage::Inserted);
 i.leftGrip=false;p.update(i);check(p.stage()==Stage::Inserted);
 i.leftGrip=true;i.slidePull=0;p.update(i);check(p.stage()==Stage::SlideHeld);
 i.slidePull=.05f;p.update(i);i.leftGrip=false;p.update(i);check(p.pendingToken()!=0);
 puts("PASS: push seats magazine, outside zone stays held, no automatic slide grab/ammo commit");return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}
