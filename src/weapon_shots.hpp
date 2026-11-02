#pragma once
#include <array>
#include <cstdint>
#include <cmath>
namespace weaponShots {
struct Observer {
    int loaded{},reserve{};std::uint64_t sampled{},fired{};unsigned serial{};bool ready{};
    void sample(int a,int b,std::uint64_t now,bool valid){
        if(!valid||a<0||a>100||b<0||b>10000){ready=false;fired=0;return;}
        if(ready&&now>=sampled&&now-sampled<=100&&reserve==b&&a==loaded-1){fired=now;++serial;}
        loaded=a;reserve=b;sampled=now;ready=true;
    }
};
inline std::array<Observer,5> observers; // protected by manualReload::mutex
struct Kick {float pitch,back;};
inline Kick kick(unsigned weapon,std::uint64_t shot,std::uint64_t now,bool valid){
    // Degrees/rise/recovery are deliberately distinct; no camera pushback.
    constexpr float degrees[]={4.0f,2.4f,1.2f,5.0f,3.5f};
    constexpr float metres[]={.018f,.012f,.007f,.026f,.020f};
    constexpr float duration[]={240,190,110,300,260};
    if(!valid||weapon>=5||!shot||now<shot||now-shot>=duration[weapon])return {};
    float t=float(now-shot)/duration[weapon];
    float envelope=t<.12f?(t/.12f):std::pow((1-t)/.88f,2.f);
    return {degrees[weapon]*.01745329252f*envelope,metres[weapon]*envelope};
}
}
