#pragma once
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace sniperScope {
inline std::atomic<int> visibleWeapon{-1};
inline std::atomic<uint64_t> weaponTime{};
inline std::atomic<float> magnification{1.f};
inline std::atomic<bool> engaged{};
inline bool consumedCancel=false;
// Scope presentation is deferred while manual reloads are being verified.
// An omitted setting must not re-enable it in a fresh test runtime.
inline bool enabled=false;
inline float zoom=3.f;
class Controller {
    bool armed=false,held=false;
    float amount=0;
    uint64_t previous{};
public:
    float update(uint64_t now,bool available,float trigger,bool cancel,float targetZoom){
        float dt=previous&&now>=previous?static_cast<float>((std::min)(now-previous,uint64_t(50)))/1000.f:0.f;previous=now;
        if(!available||cancel){armed=false;held=false;amount=0;return 1.f;}
        if(trigger<.25f){armed=true;held=false;}
        else if(trigger>.65f&&armed)held=true;
        // Hysteresis avoids repeated opening at a noisy trigger boundary.
        if(held)amount=(std::min)(1.f,amount+dt/.12f);
        else amount=(std::max)(0.f,amount-dt/.12f);
        float eased=amount*amount*(3-2*amount);
        return 1.f+eased*(targetZoom-1.f);
    }
    bool active()const{return held;}
};
inline Controller controller;
inline void update(uint64_t now,bool trackedGameplay,float trigger,bool cancel){
    consumedCancel=cancel&&(engaged.load()||magnification.load()>1.01f);
    auto stamp=weaponTime.load();
    bool available=enabled&&trackedGameplay&&visibleWeapon.load()==4&&now>=stamp&&now-stamp<150;
    auto value=controller.update(now,available,trigger,cancel,zoom);
    magnification=value;engaged=available&&controller.active();
}
inline float factor(){return magnification.load();}
}
