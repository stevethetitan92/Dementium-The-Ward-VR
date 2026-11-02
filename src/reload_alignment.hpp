#pragma once
#include "tracking_math.hpp"
#include <algorithm>
#include <array>
namespace manualReload {
inline std::array<XrVector3f,7> magazineExitAxes{{{0,-1,0},{0,-1,0},{0,-1,0},{0,-1,0},{0,-1,0},{0,-1,0},{0,-1,0}}};
inline std::array<float,7> magazineEntryClearance{};
// Render and insertion share this pose. Preserve the tracked finger contact
// while aligning the held magazine with its seated orientation in gun space.
class Alignment {
    bool captured{};std::uint64_t started{};
public:
    void reset(){captured=false;started=0;}
    bool active()const{return captured;}
    bool update(XrPosef raw,XrVector3f contact,XrVector3f offset,float distance,
                std::uint64_t now,bool holding,XrPosef& result,const XrVector3f* seat=nullptr,XrVector3f exitAxis={0,-1,0},float entryClearance=0){
        result=raw;
        if(!std::isfinite(entryClearance)||entryClearance<0||entryClearance>.35f){reset();return false;}
        if(!seat)entryClearance=0;
        if(!holding||!motionTracking::validPose(raw)||!std::isfinite(distance)||distance>(std::max)(.14f,entryClearance+.10f)){reset();return false;}
        if(seat){
            float dx=contact.x-seat->x,dy=contact.y-seat->y,dz=contact.z-seat->z;
            float along=dx*exitAxis.x+dy*exitAxis.y+dz*exitAxis.z;
            float lateral=dx*dx+dy*dy+dz*dz-along*along;
            // Capture the well entrance, not the receiver's top or side wall.
            // Once captured, allow a small overshoot and clamp the display to
            // the seated point so a brisk insertion does not pass through it.
            if(lateral>.06f*.06f||(!captured&&along<-.015f)){
                reset();return false;
            }
        }
        if(!captured){if(distance>(std::max)(.085f,entryClearance+.04f))return false;captured=true;started=now;}
        if(now<started){reset();return false;}
        float t=std::clamp(float(now-started)/100.f,0.f,1.f);t=t*t*(3-2*t);
        float feed=entryClearance>0?std::clamp((float(now-started)-100.f)/100.f,0.f,1.f):t;
        feed=feed*feed*(3-2*feed);
        using namespace DirectX;
        XMFLOAT4 q;XMStoreFloat4(&q,XMQuaternionSlerp(XMVectorSet(raw.orientation.x,raw.orientation.y,raw.orientation.z,raw.orientation.w),XMQuaternionIdentity(),t));
        result.orientation={q.x,q.y,q.z,q.w};
        XMFLOAT3 p;XMStoreFloat3(&p,XMVector3TransformNormal(XMVectorSet(offset.x,offset.y,offset.z,0),motionTracking::rotation(result.orientation)));
        if(seat){
            // Fitted detachable magazines approach from below in gun space.
            // Ease onto the well axis and stop at its seated contact, moving
            // the displayed hand with the magazine. Raw tracking still owns
            // capture/release and the gameplay insertion distance.
            float along=(contact.x-seat->x)*exitAxis.x+(contact.y-seat->y)*exitAxis.y+(contact.z-seat->z)*exitAxis.z;
            along=(std::max)(0.f,along);
            if(entryClearance>0){
                // Turn the magazine outside the gun before advancing into
                // its well. A fast initial sample already inside the receiver
                // is held at the entrance instead of rotating through metal.
                const float guarded=along+(std::max)(0.f,entryClearance-along)*(1-feed);
                const float rawAlong=(contact.x-seat->x)*exitAxis.x+(contact.y-seat->y)*exitAxis.y+(contact.z-seat->z)*exitAxis.z;
                contact.x+=exitAxis.x*(guarded-rawAlong);contact.y+=exitAxis.y*(guarded-rawAlong);contact.z+=exitAxis.z*(guarded-rawAlong);
                along=guarded;
            }
            contact.x+=(seat->x+exitAxis.x*along-contact.x)*t;
            contact.y+=(seat->y+exitAxis.y*along-contact.y)*t;
            contact.z+=(seat->z+exitAxis.z*along-contact.z)*t;
        }
        result.position={contact.x-p.x,contact.y-p.y,contact.z-p.z};
        return t>=1&&feed>=1;
    }
};
}
