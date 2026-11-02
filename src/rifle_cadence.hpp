#pragma once
#include <cstdint>
namespace rifleCadence {
// Native M16 shots are emitted at animation frame 0/8, not by a timer.
// Schedule those existing events; never synthesize damage or debit ammo here.
class Clock {
    std::uint64_t due{},last{},observed{};
    bool armed{};
public:
    static constexpr std::uint64_t interval=75; // user target: 800 RPM
    unsigned shots{};
    bool ready(std::uint64_t now)const{return !armed||now<last||now>=due;}
    void reset(){armed=false;due=last=observed=0;}
    void shot(std::uint64_t now){
        ++shots;
        if(!armed||now<last||now>due+interval)due=now+interval;
        else due+=interval;
        last=observed=now;armed=true;
    }
    int beforeNativeFrame(unsigned state,unsigned frame,std::uint64_t now,bool eligible){
        if(!eligible||frame>64||now<observed){reset();return -1;}
        observed=now;
        if(!armed||(state!=3&&state!=4))return -1;
        if(state==3)return now>=due?21:-1; // native transition at frame22
        if(now>=due)return 7; // native increments then emits the frame8 shot
        // Keep the recovery animation moving, excluding native shot frames.
        auto elapsed=now>=last?now-last:0;
        return int((elapsed*6/interval)%7); // after native increment:1..7
    }
};
}
