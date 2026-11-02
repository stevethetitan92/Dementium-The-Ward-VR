#pragma once
#include "reload_gestures.hpp"
#include <array>

// Pure draw-time transforms. These never write game ammo or source vertices.
// A package-specific, validated binding supplies role and contact points.
namespace selectedReload {
using namespace DirectX;
enum class Role { Fixed, Magazine, Slide };
struct Geometry {
    XrVector3f magazineContactEditor{};
    XrVector3f pullAxisGrip{0,0,1};
    XrVector3f heldContactGrip{};
};
inline XMMATRIX pose(const XrPosef& p){
    return motionTracking::rotation(p.orientation)*XMMatrixTranslation(p.position.x,p.position.y,p.position.z);
}
inline bool showPouchMagazine(int weapon,manualReload::Stage stage,bool ammoValid,int reserve,int loaded=0){
    return weapon==1&&stage==manualReload::Stage::EmptyWell&&ammoValid&&reserve>=0&&loaded>=0&&(reserve>0||loaded>0);
}
inline XMMATRIX pouchMagazine(const Geometry& geometry,const XMMATRIX& editorToGrip,
                             XrVector3f pouch,const XrPosef& head){
    auto orientation=editorToGrip;orientation.r[3]=XMVectorSet(0,0,0,1);
    const auto& c=geometry.magazineContactEditor;
    return XMMatrixTranslation(-c.x,-c.y,-c.z)*orientation*
           XMMatrixTranslation(pouch.x,pouch.y,pouch.z)*pose(head);
}
inline bool transform(Role role,const Geometry& geometry,const manualReload::Visual& visual,
                      const XMMATRIX& editorToGrip,const XrPosef& gun,XMMATRIX& result){
    result=editorToGrip*pose(gun);
    if(role==Role::Fixed||!visual.valid)return true;
    if(role==Role::Magazine){
        if(visual.stage==manualReload::Stage::EmptyWell)return false;
        if(visual.stage==manualReload::Stage::MagazineHeld){
            if(!motionTracking::validPose(visual.leftInGun))return false;
            // Center the known magazine contact at the tracked left grip.
            // Retain its seated orientation relative to the grip basis.
            auto orientation=editorToGrip;orientation.r[3]=XMVectorSet(0,0,0,1);
            const auto& c=geometry.magazineContactEditor;
            const auto& hand=geometry.heldContactGrip;
            result=XMMatrixTranslation(-c.x,-c.y,-c.z)*orientation*
                XMMatrixTranslation(hand.x,hand.y,hand.z)*pose(visual.leftInGun)*pose(gun);
        }
    }else if(role==Role::Slide&&visual.stage==manualReload::Stage::SlideHeld){
        if(!std::isfinite(visual.slidePull))return true;
        float travel=std::clamp(visual.slidePull,0.f,.07f);
        const auto& a=geometry.pullAxisGrip;
        result=editorToGrip*XMMatrixTranslation(a.x*travel,a.y*travel,a.z*travel)*pose(gun);
    }
    return true;
}
}
