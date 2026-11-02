#pragma once
#include "selected_frames.hpp"
#include <algorithm>
#include <mutex>
namespace selectedMeleeContact {
inline std::mutex mutex;
inline XrVector3f clubBase{},clubTip{};
inline bool clubReady{};
inline XrVector3f sawBase{},sawTip{};inline float sawRadius=.065f;inline bool sawReady{};
// Use the actual fitted rod ends. Controller -Z is not the shaft axis of
// the user's upright nightstick fit. End bands exclude the side handle.
inline bool load(const psxAssets::Pack& pack){
    using namespace DirectX;
    if(!selectedFrames::ready)return false;
    std::vector<XMFLOAT3> points;
    auto fit=XMLoadFloat4x4(&selectedFrames::frames[6].editorToAim);
    for(const auto& s:pack.sections)if(s.weapon==6&&s.kind==psxAssets::Weapon)
        for(unsigned j=0;j<s.count;++j){
            const auto* v=pack.vertices[s.first+j].value;XMFLOAT3 p;
            XMStoreFloat3(&p,XMVector3TransformCoord(XMVectorSet(v[0],v[1],v[2],1),fit));points.push_back(p);
        }
    // Bind the rounded cutting volume to the rotating blade section only.
    // Housing and handles cannot supply the blade dimensions.
    if(pack.version==3&&pack.rotorSection<pack.sections.size()){
        const auto& blade=pack.sections[pack.rotorSection];
        if(blade.weapon==5&&blade.kind==psxAssets::Weapon){
            const auto& c=pack.rotorPivot;const auto& n=pack.rotorAxis;
            float lo=10,hi=-10,r=0;
            for(unsigned j=0;j<blade.count;++j){const auto* v=pack.vertices[blade.first+j].value;
                float x=v[0]-c[0],y=v[1]-c[1],z=v[2]-c[2],a=x*n[0]+y*n[1]+z*n[2];
                lo=(std::min)(lo,a);hi=(std::max)(hi,a);
                r=(std::max)(r,std::sqrt((std::max)(0.f,x*x+y*y+z*z-a*a)));
            }
            if(r>.005f&&r<.3f&&hi>=lo&&hi-lo<.15f){
                auto fitSaw=XMLoadFloat4x4(&selectedFrames::frames[5].editorToAim);
                auto end=[&](float a){XMFLOAT3 v;XMStoreFloat3(&v,XMVector3TransformCoord(XMVectorSet(c[0]+n[0]*a,c[1]+n[1]*a,c[2]+n[2]*a,1),fitSaw));return XrVector3f{v.x,v.y,v.z};};
                std::lock_guard<std::mutex> lock(mutex);sawBase=end(lo);sawTip=end(hi);sawRadius=r;sawReady=true;
            }
        }
    }
    if(points.empty())return false;
    std::sort(points.begin(),points.end(),[](auto a,auto b){return a.x!=b.x?a.x<b.x:a.y!=b.y?a.y<b.y:a.z<b.z;});
    points.erase(std::unique(points.begin(),points.end(),[](auto a,auto b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}),points.end());
    float low[3]={10,10,10},high[3]={-10,-10,-10};
    for(auto p:points)for(unsigned a=0;a<3;++a){const float v=(&p.x)[a];low[a]=(std::min)(low[a],v);high[a]=(std::max)(high[a],v);}
    unsigned axis=0;for(unsigned a=1;a<3;++a)if(high[a]-low[a]>high[axis]-low[axis])axis=a;
    if(high[axis]-low[axis]<.15f||high[axis]-low[axis]>1.5f)return false;
    XrVector3f ends[2]{};unsigned count[2]{};
    for(auto p:points)for(unsigned e=0;e<2;++e)if(std::fabs((&p.x)[axis]-(e?high[axis]:low[axis]))<.015f){
        ends[e].x+=p.x;ends[e].y+=p.y;ends[e].z+=p.z;++count[e];
    }
    for(unsigned e=0;e<2;++e){if(!count[e])return false;ends[e].x/=count[e];ends[e].y/=count[e];ends[e].z/=count[e];}
    auto norm=[](XrVector3f p){return p.x*p.x+p.y*p.y+p.z*p.z;};
    if(norm(ends[0])>norm(ends[1]))std::swap(ends[0],ends[1]);
    std::lock_guard<std::mutex> lock(mutex);clubBase=ends[0];clubTip=ends[1];clubReady=true;return true;
}
inline void segment(unsigned melee,const XrPosef& pose,XrVector3f& base,XrVector3f& tip){
    using namespace DirectX;
    std::lock_guard<std::mutex> lock(mutex);
    if((melee==0&&!sawReady)||(melee==1&&!clubReady)){base=motionTracking::pointAhead(pose,melee==0?.18f:.08f);tip=motionTracking::pointAhead(pose,melee==0?.38f:.52f);return;}
    auto transform=selectedFrames::pose(pose);
    auto point=[&](XrVector3f p){XMFLOAT3 out;XMStoreFloat3(&out,XMVector3TransformCoord(XMVectorSet(p.x,p.y,p.z,1),transform));return XrVector3f{out.x,out.y,out.z};};
    base=point(melee==0?sawBase:clubBase);tip=point(melee==0?sawTip:clubTip);
}
}
