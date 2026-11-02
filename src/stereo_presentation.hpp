#pragma once
#include <cstdint>
namespace stereoPresentation {
inline bool desktopFallback(bool gameplay,bool drewWorld,bool cachedWorld,std::uint64_t age){return !gameplay&&!drewWorld&&(!cachedWorld||age>250);}
inline bool worldVisible(bool gameplay,bool cachedWorld,std::uint64_t age){return cachedWorld&&(gameplay||age<=250);}
// Presentation-only easing. Never interpolate tracked poses or advance simulation.
// The old stereo pair fades out during the tail of the existing 250 ms grace
// period; the new source then fades in. Ordinary cached gameplay stays untouched.
class HandoffFade {
    bool initialized_=false, world_=false, revealing_=false;
    std::uint64_t changed_=0;
    static float smooth(float t){return t*t*(3.f-2.f*t);}
public:
    float update(std::uint64_t now,bool gameplay,bool cachedWorld,
                 std::uint64_t worldAge,bool customMenu=false){
        const bool world=worldVisible(gameplay,cachedWorld,worldAge);
        if(!initialized_||customMenu){
            initialized_=true;world_=world;revealing_=false;return 0.f;
        }
        if(world!=world_){world_=world;changed_=now;revealing_=true;}
        float alpha=0.f;
        if(revealing_){
            const auto elapsed=now>=changed_?now-changed_:0;
            if(elapsed>=180)revealing_=false;
            else alpha=1.f-smooth(static_cast<float>(elapsed)/180.f);
        }
        if(world&&!gameplay&&worldAge>130){
            const float t=static_cast<float>(worldAge-130)/120.f;
            const float outgoing=smooth(t<1.f?t:1.f);
            if(outgoing>alpha)alpha=outgoing;
        }
        return alpha;
    }
};
}
