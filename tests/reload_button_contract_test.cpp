#include "../src/arsenal_core.hpp"
#include <cstdio>
#include <stdexcept>
void check(bool v){if(!v)throw std::runtime_error("B reload contract failed");}
int main(){try{
 using namespace arsenal;
 Reload cylinder;Event e;e.valid=true;cylinder.update(Kind::Revolver,e);
 e.release=true;cylinder.update(Kind::Revolver,e);check(cylinder.state()==Phase::Open);
 cylinder.update(Kind::Revolver,e);check(cylinder.state()==Phase::Open); // held B is not another press
 e.release=false;e.grip=true;e.atPouch=true;cylinder.update(Kind::Revolver,e);check(cylinder.state()==Phase::Held);
 e.release=true;cylinder.update(Kind::Revolver,e);check(cylinder.state()==Phase::Ready&&!cylinder.token());
 e.release=false;e.grip=false;cylinder.update(Kind::Revolver,e);e.release=true;cylinder.update(Kind::Revolver,e);check(cylinder.state()==Phase::Open);
 Reload rifle;e={};e.valid=true;rifle.update(Kind::Rifle,e);e.release=true;rifle.update(Kind::Rifle,e);
 e.release=false;rifle.update(Kind::Rifle,e);e.release=true;rifle.update(Kind::Rifle,e);check(rifle.state()==Phase::Open&&rifle.blocked());
 manualReload::Pistol pistol;manualReload::Input p;p.tracked=p.gameplay=p.pistol=true;pistol.update(p);
 p.releaseMagazine=true;pistol.update(p);p.releaseMagazine=false;p.leftGrip=true;p.ammoPouchDistance=0;pistol.update(p);
 p.leftGrip=false;p.magazineDistance=0;pistol.update(p);check(pistol.stage()==manualReload::Stage::Inserted);
 p.releaseMagazine=true;pistol.update(p);check(pistol.stage()==manualReload::Stage::EmptyWell&&!pistol.pendingToken());
 puts("PASS: B edge toggle for cylinder, close while holding round, magazine re-eject and empty-rifle fire block");return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}
