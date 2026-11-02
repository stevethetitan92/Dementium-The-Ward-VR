#include "../src/wristband_native.hpp"
#include "../src/wristband_renderer.hpp"
#include <iostream>
#include <stdexcept>
using namespace DirectX;
static void require(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
static bool equal(FXMMATRIX a,FXMMATRIX b){XMFLOAT4X4 x,y;XMStoreFloat4x4(&x,a);XMStoreFloat4x4(&y,b);for(int i=0;i<16;i++)if(std::fabs((&x._11)[i]-(&y._11)[i])>1e-5f)return false;return true;}
int main(int argc,char** argv){try{
 require(argc==2&&wristband::loadSelected(argv[1]),"asset load");
 manualReload::Hands h{};h.session=h.gameplay=h.gunTracked=h.leftPoseTracked=h.rightPoseTracked=true;
 h.head.orientation.w=h.gun.orientation.w=h.left.orientation.w=1;
 h.gun.position={.15f,-.15f,-.4f};h.left.position={-.15f,-.1f,-.5f};
 wristband::Snapshot s{};s.world=s.healthValid=s.ammoValid=true;s.health=10;s.maximum=10;s.loaded=6;s.reserve=24;
 auto prepare=[&]{h.time=GetTickCount64();s.time=h.time;manualReload::publish(h);wristband::model.publish(s);wristband::prepareFrame();};
 for(int id=0;id<7;id++){
  h.weapon=id;h.grip=false;supportAim::publish(id,false);prepare();
  unsigned actual=0;for(const auto& d:wristband::selectedDraws){const auto& part=wristband::selectedPack.sections[d.section];if(part.kind!=psxAssets::LeftSupport){require(part.weapon==unsigned(id),"wrong weapon leaked");require(equal(d.editorToWorld,selectedFrames::weapon(id,h.gun)),"weapon and right hand disagree");++actual;}}
  unsigned expected=0;for(const auto& part:wristband::selectedPack.sections)if(part.weapon==unsigned(id)&&part.kind!=psxAssets::LeftSupport)++expected;
  require(actual==expected&&actual>=2,"missing weapon/right hand");
  require(wristband::selectedWristVisible,"missing left HUD");
  const int lp=selectedFrames::frames[id].support?id:2;
  require(equal(wristband::selectedWristWorld,selectedFrames::wrist(lp,selectedFrames::freeHand(lp,h.left))),"free wrist does not follow left");
  if(selectedFrames::frames[id].support){h.grip=true;supportAim::publish(id,true);prepare();
   for(const auto& d:wristband::selectedDraws)require(equal(d.editorToWorld,selectedFrames::weapon(id,h.gun)),"support not attached to weapon");
   require(equal(wristband::selectedWristWorld,selectedFrames::wrist(id,selectedFrames::weapon(id,h.gun))),"attached wrist mismatch");
  }else{h.grip=true;supportAim::publish(id,true);prepare();
   require(equal(wristband::selectedWristWorld,selectedFrames::wrist(lp,selectedFrames::freeHand(lp,h.left))),"unsupported weapon attached left hand");
  }
 }
 h.weapon=2;h.grip=false;supportAim::publish(2,false);
 h.rightPoseTracked=false;prepare();require(wristband::selectedFrameWeapon==-1,"lost right still draws gun");require(wristband::selectedWristVisible,"lost right incorrectly hides left");
 h.leftPoseTracked=false;prepare();require(wristband::selectedDraws.empty()&&!wristband::selectedWristVisible,"lost controllers leave geometry");
 h.leftPoseTracked=h.rightPoseTracked=true;h.gameplay=false;prepare();require(wristband::selectedDraws.empty(),"menu leaks hands");
 h.gameplay=true;prepare();h.time=GetTickCount64()-200;manualReload::publish(h);wristband::prepareFrame();require(wristband::selectedDraws.empty(),"stale tracking draws");
 prepare();wristband::selectedRenderedWeapon=2;wristband::selectedRenderedAt=GetTickCount64();
 for(unsigned id=0;id<7;++id)require(wristband::replaceSelectedNative(id),"validated replacement lost ownership");
 wristband::selectedRenderedAt=GetTickCount64()-200;
 require(wristband::replaceSelectedNative(2),"door transition resurrects native weapon");
 require(!wristband::replaceSelectedNative(7),"invalid slot suppressed");
 wristband::selectedMode=false;require(!wristband::replaceSelectedNative(2),"unloaded replacement suppresses original");
 std::cout<<"PASS: seven weapon/right-hand frames; four attached support/wrist frames; free left wrist; tracking loss, menu, stale and replacement gates. Offline only.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
