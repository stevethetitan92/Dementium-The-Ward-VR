#pragma once
#include "tracking_math.hpp"
#include "wristband_fit.hpp"
#include <algorithm>
#include <array>
#include <mutex>
namespace wristband {
struct Snapshot {
    int health{},maximum{},weapon=-1,loaded{},reserve{},meds{};
    bool healthValid{},ammoValid{},medsValid{},world{};
    std::uint64_t time{},revision{};
};
inline bool same(const Snapshot& a,const Snapshot& b){return a.health==b.health&&a.maximum==b.maximum&&a.weapon==b.weapon&&a.loaded==b.loaded&&a.reserve==b.reserve&&a.meds==b.meds&&a.healthValid==b.healthValid&&a.ammoValid==b.ammoValid&&a.medsValid==b.medsValid&&a.world==b.world;}
inline Snapshot sanitize(Snapshot s){
    s.healthValid=s.healthValid&&s.maximum>0&&s.health>=0&&s.health<=s.maximum;
    const int capacities[]={6,15,30,8,5};s.ammoValid=s.ammoValid&&s.weapon>=0&&s.weapon<=4&&s.loaded>=0&&s.loaded<=capacities[s.weapon]&&s.reserve>=0&&s.reserve<=255;
    s.medsValid=s.medsValid&&s.meds>=0&&s.meds<=3;
    return s;
}
inline float healthFill(const Snapshot& s){return s.healthValid&&s.maximum>0?std::clamp(float(s.health)/s.maximum,0.f,1.f):0.f;}
class Model {
    mutable std::mutex guard;Snapshot value;
public:
    void publish(Snapshot next){next=sanitize(next);std::lock_guard<std::mutex> lock(guard);next.revision=value.revision+(!same(value,next));value=next;}
    Snapshot read(std::uint64_t now)const{std::lock_guard<std::mutex> lock(guard);auto s=value;if(now<s.time||now-s.time>=250){s.healthValid=s.ammoValid=s.medsValid=false;s.world=false;}return s;}
};
inline Model model;
struct Settings {bool enabled=true;unsigned wrist=0;bool always=false;XrVector3f offset[2]={{0,0,0},{.0263671875f,-.06884765625f,.056640625f}};XrVector3f rotation[2]{};float radiusX=fittedRadiusX,radiusY=fittedRadiusY,width=.028f;};
inline Settings settings;
// Derived from the local 9mm wrist-bone bind matrices (left 11, right 2).
// Band X=-hand Y, band Y=hand Z, band Z=-hand X; positive determinant.
inline const XrQuaternionf wristCalibration[2]={{.24456163f,.58892450f,.75345072f,.16031383f},{.04614504f,.03497673f,-.70680695f,.70498885f}};
inline XrPosef bind(const XrPosef& grip,unsigned hand){
    using namespace DirectX;auto e=settings.rotation[hand];auto local=motionTracking::rotation(wristCalibration[hand])*XMMatrixRotationRollPitchYaw(e.x,e.y,e.z)*XMMatrixTranslation(settings.offset[hand].x,settings.offset[hand].y,settings.offset[hand].z);
    auto world=local*motionTracking::rotation(grip.orientation)*XMMatrixTranslation(grip.position.x,grip.position.y,grip.position.z);
    XMFLOAT4 q;XMStoreFloat4(&q,XMQuaternionRotationMatrix(world));XMFLOAT4X4 m;XMStoreFloat4x4(&m,world);return {{q.x,q.y,q.z,q.w},{m._41,m._42,m._43}};
}
inline bool inspect(const XrPosef& band,const XrPosef& head,bool alreadyVisible){
    if(!motionTracking::nearHead(band,head))return false;
    auto p=motionTracking::relativePose(head,band).position;
    float d=std::sqrt(p.x*p.x+p.y*p.y+p.z*p.z);
    if(settings.always)return d<2;
    // Only a visibility gate: neither the pose nor face orientation is altered.
    return d>.10f&&d<(alreadyVisible?.9f:.75f)&&-p.z/d>(alreadyVisible?.55f:.7f);
}
}
