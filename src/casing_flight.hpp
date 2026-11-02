#pragma once
#include "tracking_math.hpp"
#include <vector>
#include <array>
#include <atomic>
namespace casingFlight {
using namespace DirectX;
inline std::array<XrVector3f,5> ports{};
inline std::array<bool,5> portReady{};
inline std::atomic<std::uint64_t> captureUntil{};
inline void (*capture)(){};
struct Case {XMFLOAT3 position,velocity;XMFLOAT4 orientation;float scale{};unsigned weapon{};std::uint64_t born{};};
class Flight {
    std::vector<Case> cases;
public:
    void clear(){cases.clear();}
    std::size_t size()const{return cases.size();}
    void emit(unsigned weapon,const XMMATRIX& gunToWorld,float units,std::uint64_t now,unsigned count){
        if(weapon>=5||!std::isfinite(units)||units<=0||count>6)return;
        for(unsigned n=0;n<count;++n){
            if(cases.size()>=64)cases.erase(cases.begin());
            Case c{};c.weapon=weapon;c.scale=units;c.born=now;
            // Provisional receiver/cylinder outlets in tracked gun space.
            auto p=weapon==0?XMVectorSet(-.035f,.025f,-.065f,1):XMVectorSet(.025f,.02f,weapon==1?-.06f:-.16f,1);
            if(portReady[weapon]){const auto& source=ports[weapon];p=XMVectorSet(source.x,source.y,source.z,1);}
            auto v=weapon==0?XMVectorSet(-.25f+.09f*n,-.25f,.45f,0):XMVectorSet(1.2f+.07f*n,.65f,.3f,0);
            XMStoreFloat3(&c.position,XMVector3TransformCoord(p,gunToWorld));
            XMStoreFloat3(&c.velocity,XMVector3TransformNormal(v,gunToWorld));
            // Supplied case geometry has its base at local -Z. At release the
            // base faces gun +Z (toward the shooter), with the mouth forward.
            XMVECTOR s,q,t;XMMatrixDecompose(&s,&q,&t,XMMatrixRotationY(XM_PI)*gunToWorld);XMStoreFloat4(&c.orientation,q);cases.push_back(c);
        }
    }
    template<class Draw> void visit(std::uint64_t now,const XMMATRIX& worldToTracking,Draw draw){
        cases.erase(std::remove_if(cases.begin(),cases.end(),[&](const Case& c){return now<c.born||now-c.born>1800;}),cases.end());
        for(const auto& c:cases){
            float t=float(now-c.born)*.001f;
            auto p=XMLoadFloat3(&c.position)+XMLoadFloat3(&c.velocity)*t+XMVectorSet(0,-4.905f*c.scale*t*t,0,0);
            auto model=XMMatrixRotationRollPitchYaw(t*12,t*7,t*9)*XMMatrixRotationQuaternion(XMLoadFloat4(&c.orientation))*XMMatrixTranslationFromVector(p)*worldToTracking;
            draw(c.weapon,c.scale,model);
            if(now>=c.born&&now-c.born>=15&&now-c.born<=60&&now<captureUntil.load()&&captureUntil.exchange(0)&&capture)capture();
        }
    }
};
}
