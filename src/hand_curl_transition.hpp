#pragma once
#include <algorithm>
#include <cstdint>
namespace handCurl {
class Transition {
    float value_{},from{},target{};std::uint64_t start{},last{};bool ready{};
public:
    void reset(){*this={};}
    float update(bool holding,std::uint64_t now,bool valid){
        if(!valid){reset();return 0;}
        float next=holding?1.f:0.f;
        if(!ready||now<last||now-last>100){ready=true;value_=from=target=next;start=now;}
        if(next!=target){from=value_;target=next;start=now;}
        last=now;float t=std::clamp(float(now-start)/100.f,0.f,1.f);t=t*t*(3-2*t);
        return value_=from+(target-from)*t;
    }
};
}
