#pragma once
#include "selected_reload_profile.hpp"
#include "revolver_contact.hpp"
#include "selected_frames.hpp"
#include "selected_ballistics.hpp"
namespace selectedHunting {
inline unsigned section=~0u;
inline selectedReload::Geometry geometry;
inline bool bindMuzzle(const psxAssets::Pack& pack){
 using namespace DirectX;if(!selectedFrames::ready)return false;
 const auto frame=XMLoadFloat4x4(&selectedFrames::frames[4].editorToAim);float tip=0;
 for(const auto& s:pack.sections)if(s.weapon==4&&s.kind==psxAssets::Weapon&&s.count>1000)
  for(unsigned i=0;i<s.count;++i){auto* p=pack.vertices[s.first+i].value;tip=(std::min)(tip,XMVectorGetZ(XMVector3TransformCoord(XMVectorSet(p[0],p[1],p[2],1),frame)));}
 if(tip>-.1f||tip< -2)return false;
 auto low=XMVectorReplicate(10),high=XMVectorReplicate(-10);unsigned count=0;
 for(const auto& s:pack.sections)if(s.weapon==4&&s.kind==psxAssets::Weapon&&s.count>1000)
  for(unsigned i=0;i<s.count;++i){auto* p=pack.vertices[s.first+i].value;auto q=XMVector3TransformCoord(XMVectorSet(p[0],p[1],p[2],1),frame);if(XMVectorGetZ(q)<=tip+.001f){low=XMVectorMin(low,q);high=XMVectorMax(high,q);++count;}}
 if(!count)return false;XMFLOAT3 p;XMStoreFloat3(&p,(low+high)*.5f);selectedBallistics::muzzle[4]={p.x,p.y,tip-.002f};selectedBallistics::ready[4]=true;return true;
}
inline bool decode(const std::vector<unsigned char>& b,const std::vector<unsigned char>& g,const std::vector<unsigned char>& f,arsenal::RevolverContact& out){
    if(b.size()!=48)return false;
    unsigned h[2];std::uint64_t hashes[2];float p[6];
    std::memcpy(h,b.data(),8);std::memcpy(hashes,b.data()+8,16);std::memcpy(p,b.data()+24,24);
    if(h[0]!=0x48585044||h[1]!=1||hashes[0]!=selectedReload::fingerprint(g)||hashes[1]!=selectedReload::fingerprint(f))return false;
    arsenal::RevolverContact candidate;
    if(!candidate.set({p[0],p[1],p[2]},{p[3],p[4],p[5]}))return false;
    out=candidate;return true;
}
inline arsenal::RevolverContact load(const std::filesystem::path& folder,const psxAssets::Pack& pack){
    bindMuzzle(pack);
    arsenal::RevolverContact result;std::vector<unsigned char>b,g,f;
    section=~0u;
    if(psxAssets::read(folder/L"SelectedHuntingContact.bin",b,64)&&b.size()==64&&psxAssets::read(folder/L"SelectedGeometry.psx",g,64*1024*1024)&&psxAssets::read(folder/L"SelectedFrames.bin",f,4096)){
        unsigned id;XrVector3f center;std::memcpy(&id,b.data()+48,4);std::memcpy(&center,b.data()+52,12);
        if(id>=pack.sections.size()||pack.sections[id].weapon!=4||pack.sections[id].kind!=psxAssets::Weapon||!std::isfinite(center.x)||!std::isfinite(center.y)||!std::isfinite(center.z)||manualReload::distance(center,{})>1)return result;
        b.resize(48);
        if(decode(b,g,f,result)){section=id;geometry.magazineContactEditor=center;geometry.heldContactGrip=result.held;}
    }
    return result;
}
}
