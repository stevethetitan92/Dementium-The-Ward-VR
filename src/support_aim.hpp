#pragma once
#include "tracking_math.hpp"
#include "measured_support_fit.hpp"
#include <atomic>
#include <algorithm>
namespace supportAim {
inline std::atomic<int> activeWeapon{-1};
inline std::atomic<unsigned> attachments{},detachments{};
inline std::atomic<unsigned> diagnosticMode{};
inline std::atomic<std::uint64_t> diagnosticUntil{},diagnosticStarted{};
inline bool activeFor(int weapon){return activeWeapon.load()==weapon;}
inline const XrVector3f* (*selectedContact)(int){};
class Controller {
    bool attached{},armed=true,havePumpOrigin{};int previousWeapon=-1;
    XrVector3f pumpOrigin{};
public:
    bool active()const{return attached;}
    XrVector3f pumpRest()const{return pumpOrigin;}
    XrPosef update(const XrPosef& primary,const XrPosef& support,int weapon,bool held,bool allowed){
        using namespace DirectX;auto result=primary;
        if(weapon!=previousWeapon){armed=true;attached=false;havePumpOrigin=false;previousWeapon=weapon;}
        const auto* fit=measuredSupport::find(weapon);
        const auto* contact=selectedContact?selectedContact(weapon):(fit?&fit->contact:nullptr);
        if(!held){attached=false;armed=true;havePumpOrigin=false;return result;}
        if(!allowed||!contact||!motionTracking::validPose(primary)||!motionTracking::validPose(support)){
            attached=false;return result;
        }
        auto local=XMVectorSet(contact->x,contact->y,contact->z,0);
        auto expected=XMVector3TransformNormal(local,motionTracking::rotation(primary.orientation));
        auto delta=XMVectorSet(support.position.x-primary.position.x,support.position.y-primary.position.y,support.position.z-primary.position.z,0);
        const float length=XMVectorGetX(XMVector3Length(delta));
        if(!std::isfinite(length)||length<.08f||length>1.2f){attached=false;armed=false;return result;}
        if(!attached&&armed&&XMVectorGetX(XMVector3Length(delta-expected))<.09f)attached=true;
        if(!attached)return result;
        // Retain the rest position for the whole squeeze, including successive
        // shots and brief tracking interruptions. Starting each pump where the
        // hand happens to be after firing loses the first part of a fast stroke.
        if(!havePumpOrigin){
            XMFLOAT3 rest;XMStoreFloat3(&rest,XMVector3TransformNormal(delta,XMMatrixTranspose(motionTracking::rotation(primary.orientation))));
            pumpOrigin={rest.x,rest.y,rest.z};havePumpOrigin=true;
        }
        auto from=XMVector3Normalize(expected),to=XMVector3Normalize(delta);
        const float dot=std::clamp(XMVectorGetX(XMVector3Dot(from,to)),-1.f,1.f);
        auto axis=XMVector3Cross(from,to);float sine=XMVectorGetX(XMVector3Length(axis));
        // An opposite target is not a valid support hold. Avoid an arbitrary
        // 180-degree axis and require release before accepting another grip.
        if(dot<-.99f){attached=false;armed=false;return result;}
        if(sine>1e-6f){
            auto swing=XMMatrixRotationAxis(axis,std::atan2(sine,dot));XMFLOAT4 q;
            XMStoreFloat4(&q,XMQuaternionNormalize(XMQuaternionRotationMatrix(motionTracking::rotation(primary.orientation)*swing)));
            result.orientation={q.x,q.y,q.z,q.w};
        }
        return result;
    }
};
inline Controller controller;
inline void publish(int weapon,bool attached){
    const int next=attached?weapon:-1,previous=activeWeapon.exchange(next);
    if(previous!=next){if(previous!=-1)++detachments;if(next!=-1)++attachments;}
}
}
