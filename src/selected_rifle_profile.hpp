#pragma once
#include "selected_reload_profile.hpp"
#include "arsenal_core.hpp"
#include "selected_frames.hpp"
#include "selected_ballistics.hpp"
#include "mechanism_return.hpp"
namespace selectedRifle {
struct Profile {
    std::array<unsigned,4> sections{}; // magazine, seated rounds, handle, latch
    selectedReload::Geometry geometry;
    XrVector3f well{},handle{};
    bool ready{};
    selectedReload::Role role(unsigned id)const{
        if(ready){for(unsigned i=0;i<4;i++)if(sections[i]==id)return i<2?selectedReload::Role::Magazine:selectedReload::Role::Slide;}
        return selectedReload::Role::Fixed;
    }
};
inline Profile profile;
inline unsigned boltSection=~0u;
inline bool fitMagazine(const std::filesystem::path& folder,psxAssets::Pack& pack){
    // This supplied magazine's rear edge overlaps its well by less than 1mm.
    // A rigid 1mm forward correction clears the measured straight rail. Keep
    // the source shape, normals, UVs and materials; move its cartridges and
    // contact anchors together. Restrict the correction to the audited asset.
    if(!profile.ready||!selectedFrames::ready)return false;
    std::vector<unsigned char> f;
    if(!psxAssets::read(folder/L"SelectedFrames.bin",f,4096)||selectedReload::fingerprint(f)!=0x6a02da9dcd80f02dull)return false;
    const unsigned counts[]={4818,32928};
    const std::uint64_t expected[]={0xedde4bf75762a0e8ull,0x953ef3fdb2ba31c2ull};
    for(unsigned k=0;k<2;++k){
        if(profile.sections[k]>=pack.sections.size())return false;
        const auto& s=pack.sections[profile.sections[k]];
        if(s.weapon!=2||s.kind!=psxAssets::Weapon||s.count!=counts[k]||s.first>pack.vertices.size()||s.count>pack.vertices.size()-s.first)return false;
        auto p=reinterpret_cast<const unsigned char*>(&pack.vertices[s.first]);
        std::uint64_t hash=14695981039346656037ull;
        for(size_t j=0;j<s.count*sizeof(psxAssets::Vertex);++j)hash=(hash^p[j])*1099511628211ull;
        if(hash!=expected[k])return false;
    }
    using namespace DirectX;
    XMFLOAT3 delta;XMStoreFloat3(&delta,XMVector3TransformNormal(XMVectorSet(0,0,-.001f,0),XMMatrixInverse(nullptr,XMLoadFloat4x4(&selectedFrames::frames[2].editorToAim))));
    for(unsigned k=0;k<2;++k){const auto& s=pack.sections[profile.sections[k]];
        for(unsigned j=0;j<s.count;++j){auto v=pack.vertices[s.first+j].value;v[0]+=delta.x;v[1]+=delta.y;v[2]+=delta.z;}}
    auto& c=profile.geometry.magazineContactEditor;c.x+=delta.x;c.y+=delta.y;c.z+=delta.z;
    profile.well.z-=.001f;
    return true;
}
inline bool bindMuzzle(const psxAssets::Pack& pack){
    using namespace DirectX;
    selectedBallistics::ready[2]=false;
    if(!selectedFrames::ready||!profile.ready)return false;
    const auto frame=XMLoadFloat4x4(&selectedFrames::frames[2].editorToAim);
    float tip=0;
    auto visit=[&](auto fn){for(unsigned id=0;id<pack.sections.size();++id){
        const auto& s=pack.sections[id];
        if(s.weapon!=2||s.kind!=psxAssets::Weapon||profile.role(id)!=selectedReload::Role::Fixed)continue;
        if(s.first>pack.vertices.size()||s.count>pack.vertices.size()-s.first)continue;
        for(unsigned i=0;i<s.count;++i){const auto* p=pack.vertices[s.first+i].value;
            fn(XMVector3TransformCoord(XMVectorSet(p[0],p[1],p[2],1),frame));}
    }};
    visit([&](XMVECTOR q){tip=(std::min)(tip,XMVectorGetZ(q));});
    if(tip>-.2f||tip< -2.f)return false;
    auto low=XMVectorReplicate(10),high=XMVectorReplicate(-10);unsigned count=0;
    visit([&](XMVECTOR q){if(XMVectorGetZ(q)<=tip+.001f){low=XMVectorMin(low,q);high=XMVectorMax(high,q);++count;}});
    XMFLOAT3 size,center;XMStoreFloat3(&size,high-low);XMStoreFloat3(&center,(high+low)*.5f);
    // The selected M16's foremost fixed surface is its circular flash hider.
    // Reject an absent or implausible rim rather than aiming from arbitrary geometry.
    if(count<6||size.x<.005f||size.y<.005f||size.x>.06f||size.y>.06f||std::fabs(size.x-size.y)>.005f)return false;
    selectedBallistics::muzzle[2]={center.x,center.y,tip-.002f};
    selectedBallistics::ready[2]=true;return true;
}
inline float boltPull(float handlePull,bool ammoValid,int loaded,std::uint64_t shot,std::uint64_t now){
    float pull=std::clamp(handlePull,0.f,.07f);
      if(ammoValid&&loaded==0)return (std::max)(pull,mechanismReturn::rifleBoltTravel);
    if(shot&&now>=shot&&now-shot<75){
        float t=float(now-shot)/75.f;
        float cycle=t<.28f?t/.28f:(1-t)/.72f;
          pull=(std::max)(pull,mechanismReturn::rifleBoltTravel*cycle);
    }
    return pull;
}
inline bool roundsVisible(arsenal::Phase phase,bool ammoValid,int loaded,int reserve){
    if(!ammoValid||loaded<0||reserve<0)return false;
    if(phase==arsenal::Phase::Open)return false;
    // Native transfer waits for charging. The newly inserted magazine must
    // already show its reserve rounds during that interval.
    if(phase==arsenal::Phase::Held||phase==arsenal::Phase::Inserted||phase==arsenal::Phase::Charging)
        return reserve>0||loaded>0;
    return loaded>0;
}
inline bool decode(const std::vector<unsigned char>& b,const std::vector<unsigned char>& g,
                   const std::vector<unsigned char>& f,const psxAssets::Pack& pack,Profile& out){
    if(b.size()!=100)return false;
    unsigned h[2];std::uint64_t hashes[2];std::memcpy(h,b.data(),8);std::memcpy(hashes,b.data()+8,16);
    if(h[0]!=0x46585044||h[1]!=1||hashes[0]!=selectedReload::fingerprint(g)||hashes[1]!=selectedReload::fingerprint(f))return false;
    Profile p;std::memcpy(p.sections.data(),b.data()+24,16);
    for(unsigned i=0;i<4;i++){
        auto id=p.sections[i];if(id>=pack.sections.size()||pack.sections[id].weapon!=2||pack.sections[id].kind!=psxAssets::Weapon)return false;
        for(unsigned j=0;j<i;j++)if(p.sections[j]==id)return false;
    }
    float v[15];std::memcpy(v,b.data()+40,60);
    for(float x:v)if(!std::isfinite(x)||std::fabs(x)>1)return false;
    p.geometry.magazineContactEditor={v[0],v[1],v[2]};p.well={v[3],v[4],v[5]};p.handle={v[6],v[7],v[8]};
    p.geometry.heldContactGrip={v[9],v[10],v[11]};p.geometry.pullAxisGrip={v[12],v[13],v[14]};
    // Existing rifle gesture engine measures rearward +Z travel.
    if(std::fabs(v[12])>1e-5f||std::fabs(v[13])>1e-5f||std::fabs(v[14]-1)>1e-5f)return false;
    manualReload::Gestures check;
    if(!check.setReplacementGeometry(p.well,p.handle,p.geometry.pullAxisGrip,p.geometry.heldContactGrip))return false;
    p.ready=true;out=p;return true;
}
inline bool load(const std::filesystem::path& folder,const psxAssets::Pack& pack){
    profile={};boltSection=~0u;std::vector<unsigned char>b,g,f;
    bool ok=psxAssets::read(folder/L"SelectedRifle.bin",b,100)&&
        psxAssets::read(folder/L"SelectedGeometry.psx",g,64*1024*1024)&&
        psxAssets::read(folder/L"SelectedFrames.bin",f,4096)&&decode(b,g,f,pack,profile);
    if(ok){std::vector<unsigned char> binding;
        if(psxAssets::read(folder/L"SelectedRifleBolt.bin",binding,28)&&binding.size()==28){
            unsigned h[2],id;std::uint64_t hashes[2];std::memcpy(h,binding.data(),8);std::memcpy(hashes,binding.data()+8,16);std::memcpy(&id,binding.data()+24,4);
            if(h[0]==0x42585044&&h[1]==1&&hashes[0]==selectedReload::fingerprint(g)&&hashes[1]==selectedReload::fingerprint(f)&&
               id<pack.sections.size()&&pack.sections[id].weapon==2&&pack.sections[id].kind==psxAssets::Weapon&&profile.role(id)==selectedReload::Role::Fixed)boltSection=id;
        }
    }
    bindMuzzle(pack);
    return ok;
}
inline bool transform(unsigned section,arsenal::Phase phase,manualReload::Visual visual,
                      const DirectX::XMMATRIX& editorToGrip,const XrPosef& gun,DirectX::XMMATRIX& result){
    visual.stage=phase==arsenal::Phase::Open?manualReload::Stage::EmptyWell:
        phase==arsenal::Phase::Held?manualReload::Stage::MagazineHeld:
        phase==arsenal::Phase::Charging?manualReload::Stage::SlideHeld:manualReload::Stage::Ready;
    return selectedReload::transform(section==boltSection?selectedReload::Role::Slide:profile.role(section),profile.geometry,visual,editorToGrip,gun,result);
}
}



