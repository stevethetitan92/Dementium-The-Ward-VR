#include "../src/reload_replay.hpp"
#include <cstdio>
#include <stdexcept>
void check(bool v){if(!v)throw std::runtime_error("reload replay failed");}
int main(){try{
 using namespace selectedReload;
 check(manualReload::setAmmoPouch({-.28f,-.78f,-.08f}));
 const auto calibratedPouch=manualReload::ammoPouch;
 check(!manualReload::setAmmoPouch({.3f,-.7f,0}));
 check(!manualReload::setAmmoPouch({-.3f,-2.f,0}));
 check(manualReload::distance(calibratedPouch,manualReload::ammoPouch)==0);
 profile.ready=true;profile.well={0,-.04f,0};profile.handle={0,.07f,0};profile.geometry.heldContactGrip={.04f,.06f,-.03f};
 manualReload::Gestures gestures;check(gestures.setReplacementGeometry(profile.well,profile.handle,{0,0,1},profile.geometry.heldContactGrip));
 manualReload::Hands h;h.weapon=1;h.session=h.gameplay=h.pistol=h.gunTracked=true;h.time=1000;h.gun.position={.2f,-.2f,-.4f};
 h.gun.orientation={0,.70710678f,0,.70710678f};gestureUntil=1100;
 for(unsigned i=1;i<=7;i++){gestureMode=i;check(replay(h,1000));++h.serial;gestures.update(h,1000);}
 check(gestures.transaction().pendingToken()!=0);
 auto before=h;check(!replay(h,1100));check(h.left.position.x==before.left.position.x);
 gestureUntil=2000;h.gameplay=false;check(!replay(h,1000));
 h.gameplay=true;h.pistol=false;h.weapon=0;auto left=h.left;
 gestureMode=1;check(replay(h,1000)&&!h.release&&!h.grip);
 gestureMode=2;check(replay(h,1000)&&h.release&&!h.grip);
 check(h.left.position.x==left.position.x&&h.left.orientation.w==left.orientation.w);
 gestureMode=3;check(!replay(h,1000));
 // Replacement revolver contact must land on the authored port even when
 // the gun is rotated; input release still owns the insertion transaction.
 check(selectedRevolver::profile.contact.set({-.063f,.038f,-.128f},{.048f,.064f,-.037f}));
 gestureMode=3;check(replay(h,1000)&&h.grip&&!h.release);
 auto pouch=motionTracking::relativePose(h.head,h.left);
 check(manualReload::distance(pouch.position,calibratedPouch)<.0001f);
 gestureMode=4;check(replay(h,1000)&&!h.grip&&!h.release);
 auto local=motionTracking::relativePose(h.gun,h.left);
 auto contact=selectedRevolver::profile.contact.contact(local);
 check(manualReload::distance(contact,selectedRevolver::profile.contact.port)<.0001f);
 gestureMode=5;check(replay(h,1000)&&h.grip&&!h.release);
 local=motionTracking::relativePose(h.gun,h.left);
 contact=selectedRevolver::profile.contact.contact(local);
 auto approach=selectedRevolver::profile.contact.port;approach.x-=.10f;
 check(manualReload::distance(contact,approach)<.0001f);
 gestureMode=6;check(!replay(h,1000));
 h.weapon=2;selectedRifle::profile.ready=true;selectedRifle::profile.well={-.04f,-.007f,-.159f};selectedRifle::profile.handle={-.04f,.114f,-.008f};
 selectedRifle::profile.geometry.heldContactGrip={.048f,.064f,-.037f};
 arsenal::RevolverContact rifleContact;check(rifleContact.set(selectedRifle::profile.well,selectedRifle::profile.geometry.heldContactGrip));
 for(unsigned mode=3;mode<=7;mode++){
  gestureMode=mode;check(replay(h,1000));
  check(h.grip==(mode==3||mode==5||mode==6));
  if(mode!=3){auto local=motionTracking::relativePose(h.gun,h.left);auto actual=rifleContact.contact(local);
   auto expected=mode>=5?selectedRifle::profile.handle:selectedRifle::profile.well;if(mode>=6)expected.z+=.05f;
   check(manualReload::distance(actual,expected)<.0001f);}
 }
 h.weapon=3;auto& shotgun=selectedShotgun::contact;check(shotgun.insertion.set({-.04f,-.038f,-.184f},{.048f,.064f,-.037f}));shotgun.pump={-.042f,-.025f,-.479f};
 for(unsigned mode=4;mode<=7;mode++){
  gestureMode=mode;check(replay(h,1000));auto local=motionTracking::relativePose(h.gun,h.left);
  auto actual=shotgun.insertion.contact(local);auto expected=mode>=5?shotgun.pump:shotgun.insertion.port;if(mode==6)expected.z+=.084087276f;
  check(manualReload::distance(actual,expected)<.0001f);check(h.grip==(mode==5||mode==6));
 }
 gestureMode=8;check(replay(h,1000)&&h.grip&&!h.release);
 auto shellLocal=motionTracking::relativePose(h.gun,h.left);auto shellExpected=shotgun.insertion.port;shellExpected.x-=.12f;shellExpected.y+=.06f;
 check(manualReload::distance(shotgun.insertion.contact(shellLocal),shellExpected)<.0001f);
 h.weapon=2;check(!replay(h,1000));h.weapon=3;check(!replay(h,2000));
 puts("PASS: bounded synthetic sequence reaches real gesture transaction; timeout/menu gates preserve input");return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}
