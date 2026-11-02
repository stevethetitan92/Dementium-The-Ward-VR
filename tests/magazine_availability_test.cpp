#include "../src/manual_reload.hpp"
#include <cassert>
#include <cstdio>
using namespace manualReload;
int main(){Pistol p;Input in;in.tracked=in.gameplay=in.pistol=true;p.update(in);
 in.releaseMagazine=true;p.update(in);assert(p.stage()==Stage::EmptyWell);
 in.releaseMagazine=false;in.leftGrip=true;in.ammoPouchDistance=0;in.magazineAvailable=false;p.update(in);assert(p.stage()==Stage::EmptyWell);
 in.magazineAvailable=true;p.update(in);assert(p.stage()==Stage::EmptyWell); // still needs fresh squeeze
 in.leftGrip=false;p.update(in);in.leftGrip=true;p.update(in);assert(p.stage()==Stage::MagazineHeld);
 in.magazineDistance=0;p.update(in);assert(p.stage()==Stage::Inserted);
 in.leftGrip=false;p.update(in);in.slideDistance=0;in.leftGrip=true;p.update(in);in.slidePull=.05f;p.update(in);
 in.leftGrip=false;p.update(in);assert(p.pendingToken());
 unsigned writes=0;assert(completeReload(p,2,0,15,[&]{++writes;return true;}));assert(!writes&&p.stage()==Stage::Ready);
 puts("PASS: empty-pouch rejection, fresh squeeze after availability change, retained magazine reseating without ammo writes");}
