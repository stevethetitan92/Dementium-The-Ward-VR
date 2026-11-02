#include "../src/hand_grip_state.hpp"
#include "../src/pill_hold_core.hpp"
#include <cassert>
#include <limits>
#include <iostream>
int main(){using namespace handGrip;Input i;i.gameplay=i.tracking=i.leftTracked=i.rightTracked=true;i.time=i.now=100;i.weapon=2;i.leftGrip=true;i.supportDistance=.04f;
 pillHold::Hold bottle;assert(!bottle.grab(true,false,false));assert(bottle.grab(true,true,true));assert(bottle.update(true,true,true)==pillHold::Action::None);
 assert(bottle.update(true,true,false)==pillHold::Action::None);assert(bottle.update(true,true,true)==pillHold::Action::Consume);
 bottle.acknowledge(false);assert(bottle.active());assert(bottle.update(true,true,true)==pillHold::Action::None);
 assert(bottle.update(true,false,true)==pillHold::Action::Release);assert(!bottle.active());assert(bottle.grab(true,true,false));
 assert(bottle.update(false,true,true)==pillHold::Action::Release);assert(!bottle.active());
 auto s=select(i);assert(s.left==Pose::Support&&s.right==Pose::Weapon);
 i.reload=Reload::Held;s=select(i);assert(s.left==Pose::Magazine&&s.leftObject==Object::Magazine);
 i.weapon=3;s=select(i);assert(s.left==Pose::Shell&&s.leftObject==Object::Shell);
 i.weapon=4;i.magazineReload=true;s=select(i);assert(s.left==Pose::Magazine&&s.leftObject==Object::Magazine);i.magazineReload=false;
 i.weapon=4;i.reload=Reload::Charging;assert(select(i).left==Pose::Mechanism);
 i.leftObject=Object::Bottle;assert(select(i).left==Pose::Bottle);
 i.leftObject=Object::None;i.reload=Reload::Ready;i.supportDistance=std::numeric_limits<float>::quiet_NaN();assert(select(i).left==Pose::Relaxed);
 for(int w=0;w<7;w++){i.weapon=w;i.trigger=true;s=select(i);assert(s.right==(w==6?Pose::Nightstick:w==5?Pose::Buzzsaw:Pose::Trigger));}
 i.gameplay=false;assert(!select(i).valid);i.gameplay=true;i.now=200;assert(!select(i).valid);i.now=99;assert(!select(i).valid);
 std::cout<<"Hand grip priority, weapon coverage, reload ownership and tracking gates passed\n";
}
