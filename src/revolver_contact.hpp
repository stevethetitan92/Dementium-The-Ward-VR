#pragma once
#include "reload_gestures.hpp"
namespace arsenal {
struct RevolverContact {
    XrVector3f port{},held{};bool ready{};
    bool set(XrVector3f p,XrVector3f h){
        for(float v:{p.x,p.y,p.z,h.x,h.y,h.z})if(!std::isfinite(v))return false;
        if(manualReload::distance(p,{0,0,0})>1||manualReload::distance(h,{0,0,0})>.3f)return false;
        port=p;held=h;ready=true;return true;
    }
    XrVector3f contact(const XrPosef& hand)const{
        if(!ready)return hand.position;
        DirectX::XMFLOAT3 p;
        DirectX::XMStoreFloat3(&p,DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(held.x,held.y,held.z,0),motionTracking::rotation(hand.orientation)));
        return {hand.position.x+p.x,hand.position.y+p.y,hand.position.z+p.z};
    }
};
inline RevolverContact replacementRevolverContact;
inline RevolverContact replacementRifleContact;
inline XrVector3f replacementRifleHandle{},replacementRiflePullAxis{0,0,1};
// Preserve the acquired grip offset; only the displayed hand follows the bounded rail.
inline XrPosef rifleHandleHand(XrPosef hand,XrVector3f contact,XrVector3f grabbed,XrVector3f axis,float travel){
    const float pull=std::clamp(travel,0.f,.07f);
    hand.position.x+=grabbed.x+axis.x*pull-contact.x;
    hand.position.y+=grabbed.y+axis.y*pull-contact.y;
    hand.position.z+=grabbed.z+axis.z*pull-contact.z;
    return hand;
}
inline RevolverContact replacementShotgunContact;
inline RevolverContact replacementHuntingContact;
inline XrVector3f replacementShotgunHandle{};
}
