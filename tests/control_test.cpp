#include "../src/control_router.hpp"
#include "../src/weapon_groups.hpp"
#include "../src/medicine_inventory.hpp"
#include <stdexcept>
#include <cstdio>
#include <initializer_list>
using namespace vrControls;
static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(){try{
    require(weaponGroups::next(1,255)==0&&weaponGroups::next(0,255)==1,"hip group");
    require(weaponGroups::next(6,255)==5&&weaponGroups::next(5,255)==6,"chest group");
    require(weaponGroups::next(3,255)==2&&weaponGroups::next(2,255)==4&&weaponGroups::next(4,255)==3,"shoulder group");
    require(weaponGroups::next(3,1u<<7)==4&&weaponGroups::next(1,1u<<2)==1,"skip unowned and preserve single weapon");
    medicine::Inventory meds;require(!meds.consume(true)&&!meds.collect(599),"invalid/empty medicine");
    require(meds.collect(600)&&meds.collect(601)&&!meds.collect(602)&&meds.count()==2,"medicine cap");
    require(!meds.consume(false)&&meds.count()==2&&meds.consume(true)&&meds.count()==1,"accepted heals only");
    Router router;Input in{};in.active=true;in.l3=true;
    require(!router.step(in,0).toggleMap,"L3 must defer to chord");
    in.r3=true;auto out=router.step(in,80);require(out.toggleModMenu&&!out.toggleMap&&!out.flashlight,"chord must beat flashlight");
    require(!router.step(in,300).toggleModMenu,"held chord must not repeat");
    router.reset();in={};in.active=true;in.r3=true;require(!router.step(in,0).nextItem,"R3 alone has no item action");
    in.l3=true;require(router.step(in,10).toggleModMenu,"reverse-order chord");
    router.reset();in={};in.active=true;in.l3=true;router.step(in,0);require(!router.step(in,1000).toggleMap,"held L3 must still permit late chord");in.l3=false;out=router.step(in,1100);require(out.flashlight&&!out.toggleMap&&out.context==Context::gameplay,"L3 toggles light, never map");
    require(!router.step(in,1110).flashlight,"release cannot repeat");
    router.setContext(Context::physicalMap);
    in.rightTrigger=1;in.rightX=.8f;out=router.step(in,200);require(!out.fire&&out.turn>.5f,"physical map suppresses fire but allows turn");
    router.reset();in={};in.active=true;in.x=true;out=router.step(in,0);require(out.nextItem&&!out.openMap&&out.context==Context::gameplay,"X cycles without opening map");
    require(!router.step(in,10).nextItem,"held X cannot repeat");in.x=false;router.step(in,20);in.x=true;require(router.step(in,30).nextItem,"new X press cycles");
    router.reset();in={};in.active=true;in.rightY=-1;require(!router.step(in,0).nextItem,"down no longer cycles");in.rightY=0;router.step(in,10);in.rightY=-1;require(!router.step(in,20).nextItem,"repeated down never cycles");
    for(auto mode:{Context::inventory,Context::modMenu,Context::physicalMap}){router.reset();router.setContext(mode);in={};in.active=true;in.x=true;out=router.step(in,0);require(!out.nextItem,"menus and physical map cannot cycle");require(!out.openMap,"X never opens map");}
    router.reset();in={};in.active=true;in.y=true;in.leftY=1;in.rightX=1;in.rightTrigger=1;out=router.step(in,0);require(out.context==Context::inventory&&out.navigateY==1&&!out.walk&&!out.turn&&!out.fire,"inventory input priority");
    in.y=false;in.a=true;out=router.step(in,20);require(out.confirm,"A activates inventory item");require(!router.step(in,40).confirm,"holding A cannot repeatedly consume an item");
    in.a=false;in.b=true;out=router.step(in,60);require(out.back&&!out.toggleMap,"B is back, never map");
    require(out.context==Context::gameplay&&!out.fire,"B must leave inventory routing without firing");
    in.b=false;out=router.step(in,80);require(out.walk==1&&out.turn==1&&!out.fire,"closing inventory restores movement and turn, held trigger stays safe");
    in.rightTrigger=0;router.step(in,100);in.rightTrigger=1;require(router.step(in,120).fire,"trigger release rearms firing after menu");
    router.reset();in={};in.active=true;in.l3=true;router.step(in,0);in.active=false;router.step(in,1);in.active=true;in.l3=false;require(!router.step(in,2).flashlight,"lost input cancels pending light");
    for(auto mode:{Context::inventory,Context::modMenu}){router.reset();router.setContext(mode);in={};in.active=true;in.l3=true;router.step(in,0);in.l3=false;require(!router.step(in,1).flashlight,"menus suppress flashlight");}
    puts("PASS: chord priority, map context, one-shot X cycling; no down cycling, inventory suppression, edge-triggered confirm");return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}



