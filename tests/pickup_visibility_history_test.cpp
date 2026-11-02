#include "../src/pickup_visibility_history.hpp"
#include <cstdio>
#include <stdexcept>
static void require(bool value){if(!value)throw std::runtime_error("pickup transition history check failed");}
int main(){try{
    pickupVisibilityHistory::History history;pickupVisibilityHistory::Event event;event.scene=1;event.entity=2;event.id=100;event.time=100;event.visible=true;event.tested=true;event.nativeResult=event.effectiveResult=1;
    require(!history.observe(event)&&!history.snapshot().count);require(history.arm(100,100));event.time=99;require(!history.observe(event));event.time=100;require(history.observe(event));event.time=101;event.margin=.25f;require(!history.observe(event));
    event.time=102;event.visible=false;event.nativeResult=0;require(history.observe(event));event.time=103;event.visible=true;event.nativeResult=1;require(history.observe(event));auto transient=history.snapshot();require(transient.count==3&&transient.events[1].time==102&&!transient.events[1].visible&&transient.events[2].visible);
    event.time=104;event.sector.status=nativePickupSector::Status::StampDiffers;require(history.observe(event));event.time=105;event.nativeResult=std::numeric_limits<float>::quiet_NaN();require(history.observe(event));event.time=106;require(!history.observe(event));event.time=107;event.nativeResult=1;require(history.observe(event));history.stop();event.visible=false;require(!history.observe(event));require(history.snapshot().count==6);
    require(history.arm(0,45000));for(unsigned i=0;i<1100;++i){event.time=i;event.entity=i+10;require(history.observe(event));}auto bounded=history.snapshot();require(bounded.count==64&&bounded.events.front().time==1036&&bounded.events.back().time==1099);
    require(history.arm(10,10));event.time=20;require(!history.observe(event));require(!history.arm(UINT64_MAX-2,3)&&!history.snapshot().count);require(!history.arm(0,0));
    require(history.arm(100,100));event.time=100;event.sphereKnown=event.planesKnown=false;require(history.observe(event));
    event.time=101;event.sphereKnown=true;event.sphere={1,2,3,.5f};require(history.observe(event));
    event.time=102;event.planesKnown=true;event.testedPlanes[0]=.75f;require(history.observe(event));
    event.time=103;event.sphere[0]=2;event.testedPlanes[0]=.5f;require(!history.observe(event));
    event.time=104;event.sphereKnown=false;require(history.observe(event));event.time=105;event.planesKnown=false;require(history.observe(event));
    auto availability=history.snapshot();require(availability.count==5&&availability.events[1].sphere[0]==1&&availability.events[2].testedPlanes[0]==.75f&&!availability.events[4].planesKnown);
    std::puts("PASS: bounded pickup transitions retain evidence availability changes without recording every moving sphere/plane sample; prior transient, bounds and expiry checks pass; no pointer reads/game/XR.");return 0;
}catch(const std::exception& e){std::puts(e.what());return 1;}}
