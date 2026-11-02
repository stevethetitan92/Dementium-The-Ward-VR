#include "../src/simulation_clock.hpp"
#include <cassert>
#include <cstdio>
#include <limits>
#include <initializer_list>
int main(){
    for(double source:{30.,60.,60000./1001.})for(double render:{60.,67.,72.,90.,120.,144.}){
        simulationClock::Clock c;unsigned updates=0;
        const unsigned frames=unsigned(render*60);
        for(unsigned i=0;i<frames;++i)updates+=c.update(i/render,source);
        const double expected=source*60;
        assert(std::abs(double(updates)-expected)<=1.01);
    }
    simulationClock::Clock c;
    assert(c.update(0,60));assert(!c.update(.001,60));
    assert(c.update(10,60)); // Loading pause: only one update, no giant debt.
    assert(!c.update(10.001,60));assert(c.update(10.017,60));
    assert(c.update(1,60)); // Backward clock/restart.
    assert(!c.update(1.001,60));c.reset();assert(c.update(1.002,60));
    assert(c.update(1.003,30));assert(!c.update(1.004,30));
    assert(c.update(std::numeric_limits<double>::quiet_NaN(),60));assert(!c.initialized);
    unsigned count=0;simulationClock::Clock slow;
    for(unsigned i=0;i<450;i++)count+=slow.update(i/45.,60);
    assert(count==450); // Explicit limitation: no burst updates below native Hz.
    simulationClock::Clock jitter;unsigned jitterCount=0;
    for(unsigned i=0;i<9000;i++)jitterCount+=jitter.update(i/90.+(i%2)*.002,60);
    assert(jitterCount>=5999&&jitterCount<=6001);
    puts("PASS native-rate clock at 6 render rates; jitter, pause, reset, rate change, invalid time and under-budget behavior");
}
