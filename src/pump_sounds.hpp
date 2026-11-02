#pragma once
#include <cmath>
#include <algorithm>
namespace pumpSounds {
// Stroke events survive a brief tracking gap. A repeated sample, or reacquiring
// a hand already at the rear stop, must not replay the rearward stroke.
class Stroke {
    bool pulled{},returned{};float peak{};
public:
    enum Event { None=0,Pull=1,Return=2 };
    unsigned update(bool valid,bool held,bool active,float travel){
        if(!held){*this={};return None;}
        if(!valid||!std::isfinite(travel))return None;
        if(travel<=.005f&&(returned||!active))*this={};
        if(!active)return None;
        unsigned event=None;
        if(!pulled&&travel>.01f){pulled=true;event|=Pull;}
        peak=(std::max)(peak,travel);
        if(pulled&&!returned&&peak>=.07f&&travel<peak-.01f){returned=true;event|=Return;}
        return event;
    }
};
}
