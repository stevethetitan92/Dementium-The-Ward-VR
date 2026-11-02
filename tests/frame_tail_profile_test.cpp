#include "../src/frame_tail_profile.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
int main(){
    frameTail::Tracker t;
    t.begin(0);assert(t.take().interval==0);
    t.finish(8,7,1,3,2);t.begin(11);
    auto s=t.take();assert(s.interval==11&&s.hook==7&&s.desktopPresent==1&&s.outsidePresent==3&&s.xrBegin==3&&s.worldCompletion==2);
    assert(s.interval==s.hook+s.desktopPresent+s.outsidePresent);
    t.finish(19,7,1,3,2);t.begin(119);s=t.take();assert(s.interval==108&&s.outsidePresent==100);
    t.finish(220,100,1,95,2);t.begin(222);s=t.take();assert(s.interval==103&&s.hook==100&&s.xrBegin==95);
    t.finish(323,1,100,0,0);t.begin(325);s=t.take();assert(s.interval==103&&s.desktopPresent==100);
    assert(t.take().interval==0);
    t.begin(326);assert(t.take().interval==0); // Unfinished interval is not fabricated.
    t.finish(325,0,0,0,0);t.begin(327);assert(t.take().interval==0);
    puts("frame tail profile: same-interval attribution, reset and incomplete samples pass");
}
