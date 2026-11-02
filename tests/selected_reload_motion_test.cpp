#include "../src/selected_reload_motion.hpp"
#include <cstdio>
#include <stdexcept>
using namespace DirectX;
void check(bool v){if(!v)throw std::runtime_error("selected reload motion failed");}
XrVector3f point(XrVector3f p,const XMMATRIX& m){XMFLOAT3 r;XMStoreFloat3(&r,XMVector3TransformCoord(XMVectorSet(p.x,p.y,p.z,1),m));return {r.x,r.y,r.z};}
int main(){try{
 using namespace selectedReload;
 Geometry geometry;geometry.magazineContactEditor={.02f,.03f,.04f};
 auto frame=XMMatrixRotationY(.7f)*XMMatrixTranslation(.1f,.2f,.3f);
 XrPosef gun{{0,.70710678f,0,.70710678f},{.4f,-.2f,-.6f}};
 manualReload::Visual v;v.valid=true;v.leftInGun={{.3f,0,0,.9539392f},{-.2f,.1f,.15f}};
 XMMATRIX result;
 for(auto stage:{manualReload::Stage::Ready,manualReload::Stage::EmptyWell,manualReload::Stage::MagazineHeld,manualReload::Stage::Inserted,manualReload::Stage::SlideHeld,manualReload::Stage::CommitPending}){
  check(showPouchMagazine(1,stage,true,12)==(stage==manualReload::Stage::EmptyWell));
  check(!showPouchMagazine(1,stage,true,0)&&!showPouchMagazine(1,stage,false,12)&&!showPouchMagazine(2,stage,true,12));
 }
 XrVector3f pouch{-.28f,-.78f,-.08f};
 check(manualReload::distance(point(geometry.magazineContactEditor,pouchMagazine(geometry,frame,pouch,gun)),point(pouch,pose(gun)))<1e-5f);
 v.stage=manualReload::Stage::EmptyWell;
 check(!transform(Role::Magazine,geometry,v,frame,gun,result));
 check(transform(Role::Fixed,geometry,v,frame,gun,result));
 check(manualReload::distance(point({},result),point({},frame*pose(gun)))<1e-5f);
 v.stage=manualReload::Stage::MagazineHeld;
 check(transform(Role::Magazine,geometry,v,frame,gun,result));
 check(manualReload::distance(point(geometry.magazineContactEditor,result),point(v.leftInGun.position,pose(gun)))<1e-5f);
 geometry.heldContactGrip={.0476f,.0644f,-.0366f};
 check(transform(Role::Magazine,geometry,v,frame,gun,result));
 check(manualReload::distance(point(geometry.magazineContactEditor,result),point(geometry.heldContactGrip,pose(v.leftInGun)*pose(gun)))<1e-5f);
 v.stage=manualReload::Stage::Inserted;
 check(transform(Role::Magazine,geometry,v,frame,gun,result));
 check(manualReload::distance(point({},result),point({},frame*pose(gun)))<1e-5f);
 v.stage=manualReload::Stage::SlideHeld;v.slidePull=.05f;
 check(transform(Role::Slide,geometry,v,frame,gun,result));
 check(manualReload::distance(point({},result),point({},frame*pose(gun)))>.0499f);
 check(manualReload::distance(point({},result),point({},frame*XMMatrixTranslation(0,0,.05f)*pose(gun)))<1e-5f);
 v.valid=false;check(transform(Role::Magazine,geometry,v,frame,gun,result));
 check(manualReload::distance(point({},result),point({},frame*pose(gun)))<1e-5f);
 puts("PASS: magazine hide/hold/reseat, rotated gun contact, slide travel and invalid-sample rest fallback");return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}
