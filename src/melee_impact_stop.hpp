#pragma once
#include "arsenal_core.hpp"
#include <mutex>
namespace meleeImpactStop {
// Short-lived contact constraint; never changes the tracked controller pose.
// Withdrawal releases immediately. Bounds prevent stale contact trapping a hand.
class Stop {
    arsenal::Segment safe{};XrVector3f normal{};std::uint64_t time{};float scale{1};
public:
    void clear(){time=0;}
    void contact(arsenal::Segment before,arsenal::Segment after,std::uint64_t now,float units){
        if(time&&now>=time&&now-time<=100){
            auto offset=correction(after,now);
            if(offset.x!=0||offset.y!=0||offset.z!=0){time=now;return;}
        }
        XrVector3f v{after.tip.x-before.tip.x,after.tip.y-before.tip.y,after.tip.z-before.tip.z};
        float d=manualReload::distance(v,{0,0,0});if(d<.001f*units)return;
        safe=before;normal={v.x/d,v.y/d,v.z/d};time=now;scale=units;
    }
    XrVector3f correction(arsenal::Segment current,std::uint64_t now){
        if(!time||now<time||now-time>100){clear();return {};}
        auto project=[&](XrVector3f a,XrVector3f b){return (a.x-b.x)*normal.x+(a.y-b.y)*normal.y+(a.z-b.z)*normal.z;};
        float depth=(std::max)(project(current.base,safe.base),project(current.tip,safe.tip));
        if(depth<=0||depth>.75f*scale){clear();return {};}
        return {-depth*normal.x,-depth*normal.y,-depth*normal.z};
    }
};
inline std::mutex mutex;inline Stop club;
inline XrVector3f correction(arsenal::Segment current,std::uint64_t now){std::lock_guard<std::mutex> lock(mutex);return club.correction(current,now);}
inline void contact(arsenal::Segment a,arsenal::Segment b,std::uint64_t now,float units){std::lock_guard<std::mutex> lock(mutex);club.contact(a,b,now,units);}
inline void apply(float* matrix,XrVector3f base,XrVector3f tip,float scale,std::uint64_t now){
    auto world=[&](XrVector3f p){return XrVector3f{matrix[3]+scale*(matrix[0]*p.x+matrix[1]*p.y+matrix[2]*p.z),matrix[7]+scale*(matrix[4]*p.x+matrix[5]*p.y+matrix[6]*p.z),matrix[11]+scale*(matrix[8]*p.x+matrix[9]*p.y+matrix[10]*p.z)};};
    std::lock_guard<std::mutex> lock(mutex);auto offset=club.correction({world(base),world(tip)},now);
    matrix[3]+=offset.x;matrix[7]+=offset.y;matrix[11]+=offset.z;
}
}
