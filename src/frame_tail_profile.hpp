#pragma once
#include <cmath>

// Present-to-present timing, not compositor dropped-frame accounting. Keep the
// components of the same slow interval together instead of summing unrelated maxima.
namespace frameTail {
struct Sample {
    double interval{}, hook{}, desktopPresent{}, outsidePresent{}, xrBegin{}, worldCompletion{}, stereoTick{}, xrEnd{};
};
class Tracker {
    double entered{}, returned{};
    Sample previous{}, worst{};
    bool started{}, finished{};
public:
    void begin(double now) {
        if(started&&finished&&std::isfinite(now)&&now>=returned) {
            auto sample=previous;
            sample.interval=now-entered;
            sample.outsidePresent=now-returned;
            if(sample.interval>worst.interval)worst=sample;
        }
        entered=now;started=std::isfinite(now);finished=false;
    }
    void finish(double now,double hook,double desktop,double xr,double world,double tick=0,double end=0) {
        if(!started||!std::isfinite(now)||now<entered)return;
        returned=now;previous={0,hook,desktop,0,xr,world,tick,end};finished=true;
    }
    Sample take(){auto result=worst;worst={};return result;}
};
inline thread_local Tracker tracker;
}
