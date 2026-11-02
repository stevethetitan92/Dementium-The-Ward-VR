// Exercise the real adapter against synthetic engine records and native-call spies.
// Does not load or run the game executable.
#include "../src/pill_bottle.hpp"
#include <cassert>
#include <iostream>
static unsigned applies{},removes{};static bool allowApply=true;
static bool applySpy(int id,void*){assert(id>=600&&id<1000);++applies;return allowApply;}
static unsigned effects{};
static void effectSpy(int id,void*){assert(id>=600&&id<1000);++effects;auto& deficit=*reinterpret_cast<unsigned char*>(pillBottle::image+0x396340);deficit=deficit>2?deficit-2:0;}
static void removeSpy(void*){++removes;}
static float* raySpy(void*,float* ray){for(unsigned i=0;i<6;i++)ray[i]=0;ray[5]=-1;return ray;}
int main(){
 using namespace pillBottle;std::vector<unsigned char> imageBytes(0x3a6000),player(0x400),weapon(0x100),flashlight(0x100),pickup(0x100);int scene;
 image=reinterpret_cast<std::uintptr_t>(imageBytes.data());
 auto put=[&](unsigned char* p,std::uintptr_t value){std::memcpy(p,&value,sizeof value);};
 put(imageBytes.data()+0x3a5330,reinterpret_cast<std::uintptr_t>(&scene));put(imageBytes.data()+0x3a5340,reinterpret_cast<std::uintptr_t>(player.data()));
 put(player.data(),image+0x30c0f8);put(weapon.data(),image+0x30c640);put(player.data()+0x308,reinterpret_cast<std::uintptr_t>(weapon.data()+0x98));
 put(flashlight.data(),image+0x30cdd0);put(flashlight.data()+0x98,reinterpret_cast<std::uintptr_t>(weapon.data()+0x98));
 put(player.data()+0x308,reinterpret_cast<std::uintptr_t>(flashlight.data()+0x98));
 put(weapon.data()+0x98,reinterpret_cast<std::uintptr_t>(player.data()+0x308));
 float body[12]={1,0,0,0,0,1,0,0,0,0,1,0};std::memcpy(player.data()+8,body,sizeof body);std::memcpy(pickup.data()+8,body,sizeof body);
 put(pickup.data(),image+0x30bc78);int id=600;std::memcpy(pickup.data()+0xb8,&id,4);
 put(pickup.data()+0x98,reinterpret_cast<std::uintptr_t>(pickup.data()));put(pickup.data()+0xa0,reinterpret_cast<std::uintptr_t>(pickup.data()));
 installed=true;originalApply=applySpy;applyEffect=effectSpy;removePickup=removeSpy;arsenal::rayAccessor=raySpy;imageBytes[0x396340]=10;
 assert(load("assets/hands/PillBottle.mesh"));
 manualReload::Hands h;h.session=h.gameplay=h.tracked=h.gunTracked=h.leftPoseTracked=true;h.weapon=1;
 auto publish=[&]{h.time=GetTickCount64();nativeHandClip::drawn[0]=h.time;manualReload::publish(h);motionTracking::publish({{0,0,0,1},{}},h.gun,1,h.time,true);records[600]={pickup.data(),reinterpret_cast<std::uintptr_t>(&scene),h.time};};
 publish();XrVector3f world{};assert(handWorld(h,player.data(),h.time,world)); // flashlight first must not block pickup placement
 h.tracked=false;manualReload::publish(h);
 for(int query=0;query<100;query++)assert(apply(600,player.data()));
 assert(applies==0&&carried==0&&removes==0&&records.count(600)); // examining/eligibility cannot grant bottles
 commitPickup(pickup.data());assert(carried==1&&removes==1&&applies==0);
 h.tracked=true;h.grip=true;publish();assert(!apply(600,player.data()));assert(!holding&&applies==0&&carried==1&&removes==1);
 // Actual Take commits one more bottle; a third stays in the world.
 for(int item=601;item<=602;item++){
  std::memcpy(pickup.data()+0xb8,&item,4);records.clear();records[item]={pickup.data(),reinterpret_cast<std::uintptr_t>(&scene),GetTickCount64()};
  assert(apply(item,player.data())==(item<602));auto count=carried.load();
  assert(carried==count);commitPickup(pickup.data());
 }
 assert(carried==2&&removes==2&&applies==0);
 records.clear();h.grip=false;publish();records.clear();tick(player.data());
 h.left.position=pocketAnchor;h.grip=true;publish();records.clear();tick(player.data());assert(holding);
 h.confirm=true;publish();records.clear();tick(player.data());assert(applies==1&&effects==1&&imageBytes[0x396340]==8&&carried==1&&!holding&&removes==2);
 // A rejected heal preserves the supply and holding state; holding A cannot repeat.
 h.grip=false;h.confirm=false;publish();records.clear();tick(player.data());
 h.grip=true;publish();records.clear();tick(player.data());assert(holding);
 allowApply=false;h.confirm=true;publish();records.clear();tick(player.data());assert(applies==2&&carried==1&&holding);tick(player.data());assert(applies==2);
 h.grip=false;publish();records.clear();tick(player.data());assert(!holding&&carried==1);
 allowApply=true;requestUse();h.gameplay=false;publish();records.clear();tick(player.data());assert(carried==1);
 h.gameplay=true;publish();records.clear();tick(player.data());assert(carried==0&&applies==3);
 requestUse();put(imageBytes.data()+0x3a5330,reinterpret_cast<std::uintptr_t>(&player));tick(player.data());assert(carried==0&&applies==3);
 assert(supply.collect(603));carried=supply.count();
 // Live controller routing: a weapon-occupied right hand cannot take a bottle.
 put(imageBytes.data()+0x3a5330,reinterpret_cast<std::uintptr_t>(&scene));
 h.rightPoseTracked=true;h.rightGrip.position=pocketAnchor;h.grip=false;h.confirm=false;
 auto controller=[&](bool grip,bool trigger,bool free){publish();records.clear();publishControls({true,{false,grip},{false,trigger},free,GetTickCount64()});tick(player.data());};
 controller(false,false,false);controller(true,false,false);assert(!holding&&carried==1);
 controller(false,false,true);controller(true,false,true);assert(holding&&heldSide==1);
 const auto before=applies;controller(true,false,true);assert(applies==before);
 controller(true,true,true);assert(!holding&&heldSide==-1&&carried==0&&applies==before+1);
 controller(true,true,true);assert(applies==before+1); // held trigger cannot consume twice
 assert(supply.collect(604));carried=supply.count();
 controller(false,false,true);controller(true,true,true);assert(holding); // pre-held trigger needs release
 controller(true,true,true);assert(carried==1);
 controller(true,false,true);controller(true,true,true);assert(carried==0&&!holding);
 installed=false;image=0;records.clear();holding=false;
 std::cout<<"PASS: two-bottle storage, full pickup rejection, chest retrieve, explicit heal, refusal preservation, menu request and scene cancellation; no game launched\n";
}
