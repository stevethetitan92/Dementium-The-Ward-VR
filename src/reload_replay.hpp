#pragma once
#include "selected_reload_profile.hpp"
#include "selected_revolver_profile.hpp"
#include "selected_rifle_profile.hpp"
#include "selected_shotgun_contact.hpp"
#include "selected_hunting_contact.hpp"
namespace selectedReload {
inline std::atomic<unsigned> gestureMode{};
inline std::atomic<std::uint64_t> gestureUntil{};
// Explicit desktop test inputs only. The real simulation-thread reload adapter
// still owns state transitions, fire blocking, ammunition transfer and feedback.
inline bool replay(manualReload::Hands& h,std::uint64_t now){
    if(!profile.ready||now>=gestureUntil.load()||!h.gameplay||!h.session||!h.gunTracked)return false;
    const auto mode=gestureMode.load();if(mode<1||mode>9||(mode==8&&h.weapon!=3))return false;
    // Cylinder toggle exercises the same B edge without inventing hand poses
    // or advancing ammunition insertion states.
    if(h.weapon==0){
        if(mode<=2){h.release=mode==2;h.grip=false;return true;}
        const auto& contact=selectedRevolver::profile.contact;
        if((mode>5&&mode!=9)||!contact.ready)return false;
        h.release=false;h.grip=mode==3||mode==5||mode==9;
        XrVector3f target=(mode==2||mode==3)?manualReload::ammoPouch:XrVector3f{contact.port.x-contact.held.x,contact.port.y-contact.held.y,contact.port.z-contact.held.z};
        // Hold the acquired round beside the cylinder, without releasing it.
        // This is a bounded desktop inspection pose, not an ammo operation.
        if(mode==5)target.x-=.10f;
        auto reference=(mode==2||mode==3)?h.head:h.gun;h.left.orientation=reference.orientation;
        XMFLOAT3 p;XMStoreFloat3(&p,XMVector3TransformCoord(XMVectorSet(target.x,target.y,target.z,1),pose(reference)));
        h.left.position={p.x,p.y,p.z};h.leftPoseTracked=h.tracked=true;return true;
    }
    if(h.weapon==2&&selectedRifle::profile.ready){
        const auto& p=selectedRifle::profile;
        h.release=mode==2;h.grip=mode==3||mode==5||mode==6||mode==9;
        XrVector3f target=(mode==2||mode==3)?manualReload::ammoPouch:(mode>=5&&mode!=9)?p.handle:p.well;
        auto reference=(mode==2||mode==3)?h.head:h.gun;h.left.orientation=reference.orientation;
        if(mode!=2&&mode!=3){target.x-=p.geometry.heldContactGrip.x;target.y-=p.geometry.heldContactGrip.y;target.z-=p.geometry.heldContactGrip.z;if((mode>=6&&mode!=9))target.z+=.05f;}
        XMFLOAT3 v;XMStoreFloat3(&v,XMVector3TransformCoord(XMVectorSet(target.x,target.y,target.z,1),pose(reference)));
        h.left.position={v.x,v.y,v.z};h.leftPoseTracked=h.tracked=true;return true;
    }
    if(h.weapon==3&&selectedShotgun::contact.insertion.ready){
        const auto& p=selectedShotgun::contact;
        h.release=mode==2;h.grip=mode==3||mode==5||mode==6||mode==8||mode==9;
        XrVector3f target=(mode==2||mode==3)?manualReload::ammoPouch:mode==8?p.insertion.port:(mode>=5&&mode!=9)?p.pump:p.insertion.port;
        // Bounded inspection pose beside the receiver, keeping the acquired
        // shell gripped. This does not insert a shell or change ammunition.
        if(mode==8){target.x-=.12f;target.y+=.06f;}
        auto reference=(mode==2||mode==3)?h.head:h.gun;h.left.orientation=reference.orientation;
        if(mode!=2&&mode!=3){target.x-=p.insertion.held.x;target.y-=p.insertion.held.y;target.z-=p.insertion.held.z;if(mode==6)target.z+=.084087276f;}
        XMFLOAT3 v;XMStoreFloat3(&v,XMVector3TransformCoord(XMVectorSet(target.x,target.y,target.z,1),pose(reference)));
        h.left.position={v.x,v.y,v.z};h.leftPoseTracked=h.tracked=true;return true;
    }
    if(h.weapon==4&&arsenal::replacementHuntingContact.ready){
        const auto& p=arsenal::replacementHuntingContact;h.release=mode==2;h.grip=mode==3||mode==5||mode==9;
        auto target=(mode==2||mode==3)?manualReload::ammoPouch:XrVector3f{p.port.x-p.held.x,p.port.y-p.held.y,p.port.z-p.held.z};
        if(mode==5)target.x-=.12f;
        auto reference=(mode==2||mode==3)?h.head:h.gun;h.left.orientation=reference.orientation;XMFLOAT3 v;
        XMStoreFloat3(&v,XMVector3TransformCoord(XMVectorSet(target.x,target.y,target.z,1),pose(reference)));
        h.left.position={v.x,v.y,v.z};h.leftPoseTracked=h.tracked=true;return true;
    }
    if(!h.pistol)return false;
    h.release=mode==2;h.grip=mode==3||mode==5||mode==6||mode==9;
    XrVector3f target{};
    XMMATRIX reference;
    if(mode==2||mode==3){target=manualReload::ammoPouch;reference=pose(h.head);h.left.orientation=h.head.orientation;}
    else {
        target=(mode>=5&&mode!=9)?profile.handle:profile.well;
        const auto& contact=profile.geometry.heldContactGrip;
        target.x-=contact.x;target.y-=contact.y;target.z-=contact.z;
        if((mode>=6&&mode!=9)){const auto& axis=profile.geometry.pullAxisGrip;target.x+=axis.x*.05f;target.y+=axis.y*.05f;target.z+=axis.z*.05f;}
        reference=pose(h.gun);h.left.orientation=h.gun.orientation;
    }
    XMFLOAT3 position;XMStoreFloat3(&position,XMVector3TransformCoord(XMVectorSet(target.x,target.y,target.z,1),reference));
    h.left.position={position.x,position.y,position.z};h.leftPoseTracked=h.tracked=true;
    return true;
}
}
