#pragma once
#include <cmath>
namespace simulationClock {
// One native update at most per rendered frame. Never manufacture a burst of
// simulation updates after a loading screen or tracking/session interruption.
struct Clock {
    double next{}, previous{}, rate{};
    bool initialized{};
    void reset() { initialized=false; }
    bool update(double now,double hz) {
        if(!std::isfinite(now)||!std::isfinite(hz)||hz<30||hz>120){reset();return true;}
        const double step=1.0/hz;
        if(!initialized||hz!=rate||now<previous||now-previous>.25){
            initialized=true;rate=hz;previous=now;next=now+step;return true;
        }
        previous=now;
        if(now+1e-9<next)return false;
        next+=step;
        // A missed budget cannot be repaired by accelerating several seconds
        // of gameplay. Bound debt to one step; below native Hz remains slow.
        if(next<now-step)next=now+step;
        return true;
    }
};
}
