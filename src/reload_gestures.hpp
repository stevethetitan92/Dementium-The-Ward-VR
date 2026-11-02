#pragma once
#include "manual_reload.hpp"
#include "tracking_math.hpp"
#include "reload_alignment.hpp"
#include <algorithm>
namespace manualReload {
// Runtime writes and reads use the existing reload mutex. Pure tests can
// configure this before feeding samples. Relative to the yaw-only head frame.
inline XrVector3f ammoPouch{-.23f,-.45f,.02f};
inline bool setAmmoPouch(XrVector3f p){
    if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||
       p.x>-.03f||p.x<-.85f||p.y>-.15f||p.y<-1.3f||std::fabs(p.z)>.65f)return false;
    ammoPouch=p;return true;
}
inline XrVector3f trackingAnchor(XrVector3f p,float units){return {p.x/units,p.y/units,p.z/units};}
inline float distance(XrVector3f a,XrVector3f b){float x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return std::sqrt(x*x+y*y+z*z);}
inline float pouchDistance(XrVector3f hand){
    // A bounded hip zone accommodates seated/standing reach without requiring
    // the controller to hit one invisible point. Preserve the calibrated side.
    auto target=ammoPouch;target.y=std::clamp(hand.y,ammoPouch.y-.30f,ammoPouch.y+.08f);
    return distance(hand,target);
}
struct Hands {
    XrPosef gun{{0,0,0,1},{}},left{{0,0,0,1},{}},head{{0,0,0,1},{}};
    XrPosef rightGrip{{0,0,0,1},{}};bool leftPoseTracked{},rightPoseTracked{};
    bool leftDisplayAvailable{},rightDisplayAvailable{};
    bool tracked{},gunTracked{},gameplay{},pistol{},release{},grip{},session{};
    float scale=1;
    std::uint64_t time{},serial{};
    int weapon=-1;float trigger{};bool confirm{};
    unsigned primaryHand=1;
    bool magazineAvailable=true;
    bool chamberLoaded=false;
    bool supportHeld{};
    XrVector3f supportPumpPosition{}; // Controller position in the unassisted gun frame.
    XrVector3f supportPumpOrigin{}; // Rest position captured when support grip attaches.
};
struct Visual {
    Stage stage=Stage::Ready;
    XrPosef leftInGun{{0,0,0,1},{}};
    float slidePull{},scale=1;
    bool valid{},alignedHand{};
};
// Anchors relative to the pistol grip, in metres. Aim orientation is used for
// the weapon; grip orientation is used for the hand-held magazine.
inline constexpr XrVector3f magazineWell{-3.f/4096,-199.f/4096,-91.f/4096},slideHandle{0,113.f/4096,-25.f/4096};
inline constexpr XrVector3f modelGrip{182.f/4096,-101.f/4096,1453.f/4096};
inline void alignPistol(float* matrix){
    // Model +Z forward -> OpenXR -Z, and the model's body pivot -> hand grip.
    for(unsigned row=0;row<3;row++){
        matrix[row*4]=-matrix[row*4];matrix[row*4+2]=-matrix[row*4+2];
        matrix[row*4+3]-=matrix[row*4]*modelGrip.x+matrix[row*4+1]*modelGrip.y+matrix[row*4+2]*modelGrip.z;
    }
}
class Gestures {
    Pistol reload;
    XrVector3f well=magazineWell,handle=slideHandle,pullAxis{0,0,1};
    XrVector3f handContact{};
    bool physicalAnchors=false;
    XrVector3f grabPosition{};
    bool wasSlide=false;
    std::uint64_t lastSerial{};
    Visual rendered;
    Alignment alignment;
public:
    // Configure only at rest. Replacement anchors are already physical metres
    // in the grip frame, unlike the native model-unit anchors.
    bool setReplacementGeometry(XrVector3f magazine,XrVector3f slide,XrVector3f axis,XrVector3f contact={}){
        if(reload.stage()!=Stage::Ready)return false;
        for(auto p:{magazine,slide,axis,contact})if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))return false;
        float length=distance(axis,{0,0,0});
        if(length<.99f||length>1.01f||distance(magazine,{0,0,0})>1||distance(slide,{0,0,0})>1||distance(contact,{0,0,0})>.3f)return false;
        well=magazine;handle=slide;handContact=contact;pullAxis={axis.x/length,axis.y/length,axis.z/length};physicalAnchors=true;return true;
    }
    Pistol& transaction(){return reload;}
    Visual visual() const{return rendered;}
    void update(const Hands& h,std::uint64_t now){
        bool valid=h.tracked&&h.session&&h.gameplay&&h.pistol&&now>=h.time&&now-h.time<150&&
            motionTracking::validPose(h.gun)&&motionTracking::validPose(h.left)&&motionTracking::validPose(h.head)&&std::isfinite(h.scale)&&h.scale>=.001f&&h.scale<=100&&
            distance(h.gun.position,h.head.position)<3&&distance(h.left.position,h.head.position)<3;
        if(h.serial==lastSerial&&valid)return;
        lastSerial=h.serial;
        Input in;in.tracked=valid;in.gameplay=h.gameplay;in.pistol=h.pistol;in.releaseMagazine=h.release;in.leftGrip=h.grip;
        in.magazineAvailable=h.magazineAvailable;
        in.chamberLoaded=h.chamberLoaded;
        XrVector3f currentContact{};
        if(valid){
            auto local=motionTracking::relativePose(h.gun,h.left);
            auto contact=local.position;
            if(physicalAnchors){
                DirectX::XMFLOAT3 offset;
                DirectX::XMStoreFloat3(&offset,DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(handContact.x,handContact.y,handContact.z,0),motionTracking::rotation(local.orientation)));
                contact.x+=offset.x;contact.y+=offset.y;contact.z+=offset.z;
            }
            currentContact=contact;
            auto pouch=motionTracking::relativePose(h.head,h.left).position;
            // Body-relative left hip, independent of looking up/down/rolling.
            // h.head orientation is yaw-only, prepared at publication time.
            in.ammoPouchDistance=pouchDistance(pouch);
            in.magazineDistance=distance(contact,physicalAnchors?well:trackingAnchor(well,h.scale));
            in.slideDistance=distance(contact,physicalAnchors?handle:trackingAnchor(handle,h.scale));
            if(physicalAnchors){
                // Grasp the slide's short longitudinal surface, not one point.
                XrVector3f d{contact.x-handle.x,contact.y-handle.y,contact.z-handle.z};
                float along=std::clamp(d.x*pullAxis.x+d.y*pullAxis.y+d.z*pullAxis.z,-.035f,.035f);
                in.slideDistance=distance(d,{along*pullAxis.x,along*pullAxis.y,along*pullAxis.z});
            }
            if(reload.stage()==Stage::SlideHeld&&wasSlide){
                XrVector3f delta{contact.x-grabPosition.x,contact.y-grabPosition.y,contact.z-grabPosition.z};
                float travel=delta.x*pullAxis.x+delta.y*pullAxis.y+delta.z*pullAxis.z;
                float lateral=distance(delta,{travel*pullAxis.x,travel*pullAxis.y,travel*pullAxis.z});
                // A large lateral departure releases the grasp, never racks it.
                if(lateral>.15f){in.tracked=false;valid=false;}
                in.slidePull=std::clamp(travel,0.f,.07f);
            }
            rendered.leftInGun=local;rendered.scale=h.scale;rendered.slidePull=in.slidePull;
            bool aligning=physicalAnchors&&reload.stage()==Stage::MagazineHeld;
            bool aligned=alignment.update(local,contact,handContact,in.magazineDistance,now,aligning,rendered.leftInGun,&well,magazineExitAxes[1],magazineEntryClearance[1]);
            rendered.alignedHand=alignment.active();
            if(reload.stage()==Stage::SlideHeld&&wasSlide){
                // Constrain only the visible grasp while the physical sample
                // still controls release/travel and rejects lateral departure.
                auto target=grabPosition;target.x+=pullAxis.x*in.slidePull;target.y+=pullAxis.y*in.slidePull;target.z+=pullAxis.z*in.slidePull;
                rendered.leftInGun.position.x+=target.x-contact.x;rendered.leftInGun.position.y+=target.y-contact.y;rendered.leftInGun.position.z+=target.z-contact.z;
                rendered.alignedHand=true;
            }
            // Both a push and release must enter through the aligned well;
            // letting go beside the receiver cannot bypass that constraint.
            if(aligning&&!aligned)in.magazineDistance=100;
        }
        auto before=reload.stage();reload.update(in);
        if(!valid){alignment.reset();wasSlide=false;rendered.valid=false;rendered.alignedHand=false;return;}
        if((before!=Stage::SlideHeld||!wasSlide)&&reload.stage()==Stage::SlideHeld)grabPosition=currentContact;
        wasSlide=reload.stage()==Stage::SlideHeld;
        rendered.stage=reload.stage();rendered.valid=true;
    }
};
// Native expanded animation records: four Q12 quaternion words and three Q12
// translation words. The shader consumes two float4s per bone, not matrices.
struct Bone {std::int32_t q[4];std::int32_t p[3];};
static_assert(sizeof(Bone)==28);
inline bool validBone(const Bone& b){
    double n=0;for(auto x:b.q)n+=double(x)*x/(4096.*4096.);
    return n>.8&&n<1.2;
}
inline void rotateHeldPart(Bone& bone,const XrQuaternionf& hand){
    using namespace DirectX;
    XrQuaternionf original{bone.q[0]/4096.f,bone.q[1]/4096.f,bone.q[2]/4096.f,bone.q[3]/4096.f};
    auto flip=motionTracking::rotation({0,1,0,0});
    XMFLOAT4 q;XMStoreFloat4(&q,XMQuaternionRotationMatrix(motionTracking::rotation(original)*flip*motionTracking::rotation(hand)*flip));
    float values[]={q.x,q.y,q.z,q.w};
    for(unsigned i=0;i<4;i++)bone.q[i]=static_cast<std::int32_t>(std::lround(values[i]*4096));
}
inline void applyParts(Bone* bones,const Visual& v){
    if(!v.valid)return;
    if(v.stage==Stage::EmptyWell){bones[6].p[1]=-40960000;}
    if(v.stage==Stage::MagazineHeld){
        rotateHeldPart(bones[6],v.leftInGun.orientation);
        const auto& p=v.leftInGun.position;
        float position[]={modelGrip.x-p.x*v.scale,modelGrip.y+p.y*v.scale,modelGrip.z-p.z*v.scale};
        for(unsigned i=0;i<3;i++)bones[6].p[i]=static_cast<std::int32_t>(std::lround(position[i]*4096));
    }
    if(v.stage==Stage::SlideHeld)bones[8].p[2]-=static_cast<std::int32_t>(std::lround(v.slidePull*v.scale*4096));
    if((v.stage==Stage::MagazineHeld||v.stage==Stage::SlideHeld)&&validBone(bones[10])&&validBone(bones[11])){
        // Reuse the existing coarse left-hand bone, without moving the whole
        // native forearm through the player. Individual finger IK is separate.
        const auto& p=v.leftInGun.position;rotateHeldPart(bones[11],v.leftInGun.orientation);
        float position[]={modelGrip.x-p.x*v.scale,modelGrip.y+p.y*v.scale,modelGrip.z-p.z*v.scale};
        for(unsigned i=0;i<3;i++)bones[11].p[i]=static_cast<std::int32_t>(std::lround(position[i]*4096));
        bones[10].p[1]=-40960000;
    }
}
}
