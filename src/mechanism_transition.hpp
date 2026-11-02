#pragma once
#include <algorithm>
#include <cstdint>
namespace mechanismMotion {
class Transition {
    float position{},from{},target{};std::uint64_t started{},last{};
public:
    float value()const{return position;}
    float update(bool open,std::uint64_t now){
        if(now<last){position=from=target=open?1.f:0.f;started=now;}
        last=now;
        const float requested=open?1.f:0.f;
        if(requested!=target){from=position;target=requested;started=now;}
        float t=std::clamp(float(now-started)/(open?180.f:220.f),0.f,1.f);
        t=t*t*(3-2*t);position=from+(target-from)*t;
        return position;
    }
};
}
