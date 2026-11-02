#pragma once
#include "tracking_math.hpp"
#include "weapon_groups.hpp"
#include <atomic>
namespace bodyHolsters {
enum class Slot {Hip,Shoulder,LeftShoulder,None};
inline Slot slot(int weapon){return weapon==0||weapon==1?Slot::Hip:weapon>=2&&weapon<=4?Slot::Shoulder:weapon==5||weapon==6?Slot::LeftShoulder:Slot::None;}
inline constexpr XrVector3f anchors[3]={{.23f,-.45f,.01f},{.25f,-.12f,.16f},{-.25f,-.12f,.16f}};
inline std::atomic<float> hipHeight{-.45f};
inline XrVector3f anchor(unsigned i){auto a=anchors[i];if(i==0)a.y=hipHeight.load();return a;}
inline constexpr float radius[3]={.18f,.19f,.19f};
struct Result {int equip=-1;bool changed{},hidden{};};
class State {
    bool previousGrip[2]{},active[2]{},hidden{};int weapon=-1,pending=-1;unsigned owner=1;int remembered[3]={1,3,6};
public:
    bool stowed()const{return hidden;}
    void reveal(){hidden=false;active[0]=active[1]=false;owner=1;pending=-1;}
    void restoreStowed(int current){weapon=current;hidden=true;active[0]=active[1]=false;owner=1;pending=-1;}
    unsigned hand()const{return owner;}
    int stored(unsigned i)const{return i<3?remembered[i]:-1;}
    Result step(bool valid,bool grip,const XrVector3f& local,int current,unsigned owned,unsigned side=1,bool free=true){
        Result out;
        if(side>1)return out;
        if(!valid||current< -1||current>6||!std::isfinite(local.x)||!std::isfinite(local.y)||!std::isfinite(local.z)){previousGrip[side]=grip;active[side]=false;out.hidden=hidden;return out;}
        if(current!=weapon){if(current!=pending){owner=1;if(slot(current)!=slot(weapon))hidden=false;}else pending=-1;weapon=current;}
        if(!active[side]){active[side]=true;previousGrip[side]=grip;out.hidden=hidden;return out;}
        bool edge=grip&&!previousGrip[side];previousGrip[side]=grip;
        if(edge){int nearest=-1;float best=1;
            for(unsigned i=0;i<3;i++){if(side==0&&i!=2)continue;auto a=anchor(i);float x=local.x-a.x,y=local.y-a.y,z=local.z-a.z;float d=(x*x+y*y+z*z)/(radius[i]*radius[i]);if(d<best){best=d;nearest=i;}}
            if(nearest>=0){
                auto target=static_cast<Slot>(nearest);
                if(!hidden&&slot(current)==target&&owner==side){remembered[nearest]=current;hidden=true;out.changed=true;}
                else if(free&&(hidden||slot(current)!=target)){int candidate=remembered[nearest];
                    if(owned&(1u<<weaponGroups::ids[candidate])){
                        if(!hidden&&slot(current)!=Slot::None)remembered[unsigned(slot(current))]=current;
                        out.equip=candidate;pending=candidate;owner=side;hidden=false;out.changed=true;
                    }
                }
            }
        }
        out.hidden=hidden;return out;
    }
};
class GripLatch {
    bool pressed{};
public:
    bool update(float value){if(!std::isfinite(value)||value<.35f)pressed=false;else if(value>.65f)pressed=true;return pressed;}
};
inline GripLatch grip,leftGrip;
inline State state;inline std::atomic<bool> hidden{};
inline std::atomic<unsigned> heldHand{1};
inline std::atomic<unsigned> diagnosticMode{};inline std::atomic<std::uint64_t> diagnosticUntil{};
struct Display {XrPosef body{};unsigned owned{};int current=-1,hip=-1;bool valid{},stowed{};int leftShoulder=-1;};
inline bool showHip(const Display& d){return d.valid&&d.hip>=0&&d.hip<2&&
    (d.owned&(1u<<weaponGroups::ids[d.hip]))&&(d.stowed||slot(d.current)!=Slot::Hip);}
inline std::mutex stateMutex;inline Display display;
inline Display read(){std::lock_guard<std::mutex> lock(stateMutex);return display;}
inline void reveal(){std::lock_guard<std::mutex> lock(stateMutex);state.reveal();heldHand=1;hidden=false;display.stowed=false;}
inline void publish(bool valid,bool grip,const XrPosef& body,const XrPosef& hand,int weapon,unsigned owned,int& equip,bool& changed,unsigned side=1,bool free=true){
    std::lock_guard<std::mutex> lock(stateMutex);
    auto upright=motionTracking::uprightPose(body);
    auto local=motionTracking::relativePose(upright,hand).position;auto r=state.step(valid&&motionTracking::nearHead(hand,body),grip,local,weapon,owned,side,free);hidden=r.hidden;heldHand=state.hand();equip=r.equip;changed=r.changed;
    // A missing free-hand pose must not erase the body-mounted holster after
    // the other hand has already updated it. Gameplay gates rendering itself.
    display={upright,owned,weapon,state.stored(0),motionTracking::validPose(body),r.hidden,state.stored(2)};
}
}

