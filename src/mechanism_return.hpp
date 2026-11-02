#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace mechanismReturn {
// Supplied M16 cartridge base is 66.067mm behind its seated bolt face.
// 67mm clears the base before return; 65mm intersects it during lift.
inline constexpr float rifleBoltTravel=.067f;
// Follow a held mechanism without lag; animate only its spring return.
class Spring {
    float position{},releaseFrom{};bool held{};std::uint64_t released{},last{};
public:
    void reset(){*this={};}
    float seatedFraction()const{return held?0.f:releaseFrom>1e-5f?std::clamp(1.f-position/releaseFrom,0.f,1.f):1.f;}
    float update(float pull,bool grabbed,std::uint64_t now,bool valid){
        if(!valid||!std::isfinite(pull)){reset();return 0;}
        if(now<last||(last&&now-last>100))reset();
        last=now;
        if(grabbed){held=true;return position=std::clamp(pull,0.f,.07f);}
        if(held){held=false;released=now;releaseFrom=position;}
        float t=std::clamp(float(now-released)/80.f,0.f,1.f);
        return position=releaseFrom*(1-t)*(1-t);
    }
};
// An empty rifle holds its bolt farther back than a partially pulled handle.
// Release from the displayed lock position when native loading completes.
class LockedBolt {
    Spring spring;
public:
    float seatedFraction()const{return spring.seatedFraction();}
    float update(float handle,bool grabbed,bool empty,std::uint64_t now,bool valid){
        if(!std::isfinite(handle))return spring.update(handle,false,now,false);
        return spring.update(empty?(std::max)(handle,rifleBoltTravel):handle,grabbed||empty,now,valid);
    }
};
}
