#pragma once
#include <openxr/openxr.h>
#include <DirectXMath.h>
#include <cmath>
#include <cstdint>
#include <mutex>
namespace motionTracking {
using namespace DirectX;
inline bool validPose(const XrPosef& p){
    const float values[]={p.position.x,p.position.y,p.position.z,p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w};
    for(float v:values)if(!std::isfinite(v))return false;
    float n=p.orientation.x*p.orientation.x+p.orientation.y*p.orientation.y+p.orientation.z*p.orientation.z+p.orientation.w*p.orientation.w;
    return n>.9f&&n<1.1f;
}
inline bool nearHead(const XrPosef& hand,const XrPosef& head){
    if(!validPose(hand)||!validPose(head))return false;
    float x=hand.position.x-head.position.x,y=hand.position.y-head.position.y,z=hand.position.z-head.position.z;
    return x*x+y*y+z*z<=9.f;
}
inline XMMATRIX rotation(const XrQuaternionf& q){return XMMatrixRotationQuaternion(XMQuaternionNormalize(XMVectorSet(q.x,q.y,q.z,q.w)));}
// One recentered reference for eyes, hands, shot rays and UI aiming. Head
// rotation is deliberately absent here: looking around must not steer a hand.
inline XrPosef relativePose(const XrPosef& reference,const XrPosef& pose){
    auto inverse=XMMatrixTranspose(rotation(reference.orientation));
    XMFLOAT3 position;XMStoreFloat3(&position,XMVector3TransformNormal(XMVectorSet(pose.position.x-reference.position.x,pose.position.y-reference.position.y,pose.position.z-reference.position.z,0),inverse));
    XMFLOAT4 q;XMStoreFloat4(&q,XMQuaternionRotationMatrix(rotation(pose.orientation)*inverse));
    return {{q.x,q.y,q.z,q.w},{position.x,position.y,position.z}};
}
inline XrVector3f pointAhead(const XrPosef& pose,float distance){
    XMFLOAT3 p;XMStoreFloat3(&p,XMVector3TransformNormal(XMVectorSet(0,0,-distance,0),rotation(pose.orientation)));
    return {pose.position.x+p.x,pose.position.y+p.y,pose.position.z+p.z};
}
// Body-mounted items turn with heading, not with a look down or head tilt.
inline XrPosef uprightPose(const XrPosef& head){
    auto out=head;auto forward=pointAhead(head,1);
    float x=forward.x-head.position.x,z=forward.z-head.position.z;
    float yaw=std::atan2(-x,-z);
    if(x*x+z*z<.0001f)yaw=2*std::atan2(head.orientation.y,head.orientation.w);
    out.orientation={0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)};return out;
}
inline void headRelativeAxes(const XrPosef& reference,const XrPosef& head,float& strafe,float& walk){
    auto forward=pointAhead(relativePose(reference,head),1);
    auto position=relativePose(reference,head).position;
    float x=forward.x-position.x,z=forward.z-position.z;
    float length=std::sqrt(x*x+z*z);if(length<.01f)return;
    float sine=-x/length,cosine=-z/length;
    float previous=strafe;strafe=previous*cosine-walk*sine;walk=previous*sine+walk*cosine;
}
inline bool validBasis(const float* m){
    for(unsigned r=0;r<3;r++)for(unsigned c=0;c<4;c++)if(!std::isfinite(m[r*4+c]))return false;
    for(unsigned c=0;c<3;c++)for(unsigned d=c;d<3;d++){
        float dot=0;for(unsigned r=0;r<3;r++)dot+=m[r*4+c]*m[r*4+d];
        if(std::fabs(dot-(c==d?1.f:0.f))>.02f)return false;
    }
    float determinant=m[0]*(m[5]*m[10]-m[6]*m[9])-m[1]*(m[4]*m[10]-m[6]*m[8])+m[2]*(m[4]*m[9]-m[5]*m[8]);
    return determinant>.98f&&determinant<1.02f;
}
inline XrVector3f worldPoint(const float* body,XrVector3f eye,XrVector3f local,float scale){
    return {eye.x+scale*(body[0]*local.x+body[1]*local.y+body[2]*local.z),
            eye.y+scale*(body[4]*local.x+body[5]*local.y+body[6]*local.z),
            eye.z+scale*(body[8]*local.x+body[9]*local.y+body[10]*local.z)};
}
// A controller lives in the camera's reference axes. The player's model axes
// can face backwards relative to that camera, so they are not an attachment basis.
inline bool cameraWorldBasis(const float* view,float* basis){
    if(!validBasis(view))return false;
    for(unsigned r=0;r<3;++r){
        for(unsigned c=0;c<3;++c)basis[r*4+c]=view[c*4+r];
        basis[r*4+3]=0;
    }
    return true;
}
inline void weaponMatrix(const float* body,XrVector3f eye,const XrPosef& local,float scale,float* out){
    XMFLOAT4X4 rotationMatrix;XMStoreFloat4x4(&rotationMatrix,rotation(local.orientation));
    const float* hand=&rotationMatrix._11;
    for(unsigned r=0;r<3;r++)for(unsigned c=0;c<3;c++){
        out[r*4+c]=0;for(unsigned k=0;k<3;k++)out[r*4+c]+=body[r*4+k]*hand[c*4+k];
    }
    auto p=worldPoint(body,eye,local.position,scale);out[3]=p.x;out[7]=p.y;out[11]=p.z;
}
struct Sample {XrPosef rightAim{{0,0,0,1},{0,0,0}};float scale=1;uint64_t time{};bool valid{};};
inline std::mutex sampleMutex;
inline Sample current;
inline Sample displayCurrent;
inline void publishDisplay(const XrPosef& origin,const XrPosef& aim,float scale,uint64_t time,bool valid){
    Sample next{};next.time=time;next.scale=scale;next.valid=valid&&validPose(origin)&&validPose(aim)&&std::isfinite(scale)&&scale>0;
    if(next.valid)next.rightAim=relativePose(origin,aim);
    std::lock_guard<std::mutex> lock(sampleMutex);displayCurrent=next;
}
inline bool readDisplay(uint64_t time,Sample& out){std::lock_guard<std::mutex> lock(sampleMutex);out=displayCurrent;return out.valid&&time>=out.time&&time-out.time<150;}
class FireGate {
    bool armed=false;
public:
    bool update(bool tracked,float trigger,bool requested){
        if(!tracked)armed=false;
        else if(trigger<.25f)armed=true;
        return tracked&&armed&&requested;
    }
};
inline void publish(const XrPosef& origin,const XrPosef& rightAim,float scale,uint64_t time,bool valid){
    Sample next{};next.time=time;next.scale=scale;
    next.valid=valid&&validPose(origin)&&validPose(rightAim)&&std::isfinite(scale)&&scale>0;
    if(next.valid)next.rightAim=relativePose(origin,rightAim);
    std::lock_guard<std::mutex> lock(sampleMutex);current=next;
}
inline bool read(uint64_t time,Sample& out){std::lock_guard<std::mutex> lock(sampleMutex);out=current;return out.valid&&time>=out.time&&time-out.time<150;}
}
