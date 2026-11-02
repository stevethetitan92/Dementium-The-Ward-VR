#pragma once
#include "selected_reload_profile.hpp"
#include "revolver_contact.hpp"
#include "selected_frames.hpp"
#include "selected_ballistics.hpp"
namespace selectedRevolver {
struct Part {unsigned section{};DirectX::XMFLOAT4X4 open{};};
struct Profile {std::array<Part,8> parts{};std::array<unsigned,6> rounds{};arsenal::RevolverContact contact;bool ready{},roundsBound{};};
inline Profile profile;
inline DirectX::XMFLOAT3 spinPivot{},spinAxis{};
inline bool spinReady{};
inline bool alignBarrel(const psxAssets::Pack& pack){
    using namespace DirectX;
    if(!profile.ready||!profile.roundsBound||!selectedFrames::ready)return false;
    XMVECTOR centers[6];auto centerSum=XMVectorZero();
    spinReady=false;
    for(unsigned j=0;j<6;++j){const auto& s=pack.sections[profile.rounds[j]];
        auto lo=XMVectorReplicate(10),hi=XMVectorReplicate(-10);
        for(unsigned i=0;i<s.count;++i){const auto* p=pack.vertices[s.first+i].value;auto v=XMVectorSet(p[0],p[1],p[2],0);lo=XMVectorMin(lo,v);hi=XMVectorMax(hi,v);}
        centers[j]=(lo+hi)*.5f;
        centerSum+=centers[j];
    }
    auto axis=XMVector3Cross(centers[1]-centers[0],centers[2]-centers[0]);
    if(XMVectorGetX(XMVector3Length(axis))<1e-7f)return false;
    axis=XMVector3Normalize(axis);if(XMVectorGetX(axis)<0)axis=-axis;
    auto frame=XMLoadFloat4x4(&selectedFrames::frames[0].editorToAim);
    auto forward=XMVector3Normalize(XMVector3TransformNormal(axis,frame)),target=XMVectorSet(0,0,-1,0);
    float dot=std::clamp(XMVectorGetX(XMVector3Dot(forward,target)),-1.f,1.f);
    if(dot<.7f)return false; // Reject an unrelated or inverted asset binding.
    XMStoreFloat3(&spinPivot,centerSum/6);XMStoreFloat3(&spinAxis,axis);spinReady=true;
    auto correction=dot>.999999f?XMMatrixIdentity():XMMatrixRotationAxis(XMVector3Cross(forward,target),std::acos(dot));
    XMStoreFloat4x4(&selectedFrames::frames[0].editorToAim,frame*correction);
    XMFLOAT3 port;XMStoreFloat3(&port,XMVector3TransformCoord(XMVectorSet(profile.contact.port.x,profile.contact.port.y,profile.contact.port.z,1),correction));
    profile.contact.port={port.x,port.y,port.z};
    auto corrected=frame*correction;
    float muzzleZ=0;
    for(const auto& s:pack.sections)if(s.weapon==0&&s.kind==psxAssets::Weapon)
        for(unsigned i=0;i<s.count;++i){const auto* v=pack.vertices[s.first+i].value;
            muzzleZ=(std::min)(muzzleZ,XMVectorGetZ(XMVector3TransformCoord(XMVectorSet(v[0],v[1],v[2],1),corrected)));}
    // Use the front barrel face, not the center of the cylinder (the barrel
    // aligns with the upper chamber rather than the cylinder axle).
    auto lo=XMVectorReplicate(10),hi=XMVectorReplicate(-10);
    for(const auto& s:pack.sections)if(s.weapon==0&&s.kind==psxAssets::Weapon)
        for(unsigned i=0;i<s.count;++i){const auto* v=pack.vertices[s.first+i].value;auto point=XMVector3TransformCoord(XMVectorSet(v[0],v[1],v[2],1),corrected);
            if(XMVectorGetZ(point)<=muzzleZ+.001f){lo=XMVectorMin(lo,point);hi=XMVectorMax(hi,point);}}
    XMFLOAT3 c;XMStoreFloat3(&c,(lo+hi)*.5f);
    selectedBallistics::muzzle[0]={c.x,c.y,muzzleZ-.002f};selectedBallistics::ready[0]=true;
    return true;
}
inline bool decode(const std::vector<unsigned char>& bytes,const std::vector<unsigned char>& geometry,
                   const std::vector<unsigned char>& frames,const psxAssets::Pack& pack,Profile& out){
    if(bytes.size()!=572&&bytes.size()!=596&&bytes.size()!=620)return false;
    unsigned h[2],count;std::uint64_t hashes[2];
    std::memcpy(h,bytes.data(),8);std::memcpy(hashes,bytes.data()+8,16);std::memcpy(&count,bytes.data()+24,4);
    if(h[0]!=0x43585044||!((h[1]==1&&bytes.size()==572)||(h[1]==2&&bytes.size()==596)||(h[1]==3&&bytes.size()==620))||count!=8||hashes[0]!=selectedReload::fingerprint(geometry)||hashes[1]!=selectedReload::fingerprint(frames))return false;
    Profile candidate;
    for(unsigned i=0;i<8;i++){
        auto& p=candidate.parts[i];std::memcpy(&p.section,bytes.data()+28+i*68,4);std::memcpy(&p.open,bytes.data()+32+i*68,64);
        if(p.section>=pack.sections.size())return false;
        const auto& s=pack.sections[p.section];if(s.weapon!=0||s.kind!=psxAssets::Weapon)return false;
        for(unsigned j=0;j<i;j++)if(candidate.parts[j].section==p.section)return false;
        const auto& m=p.open.m;
        for(unsigned r=0;r<4;r++)for(unsigned c=0;c<4;c++)if(!std::isfinite(m[r][c])||std::fabs(m[r][c])>2)return false;
        for(unsigned r=0;r<3;r++){
            if(std::fabs(m[r][3])>1e-5f||std::fabs(m[3][r])>1)return false;
            for(unsigned c=0;c<3;c++){
                float dot=0;for(unsigned k=0;k<3;k++)dot+=m[r][k]*m[c][k];
                if(std::fabs(dot-(r==c?1.f:0.f))>1e-4f)return false;
            }
        }
        if(std::fabs(m[3][3]-1)>1e-5f)return false;
        float determinant=DirectX::XMVectorGetX(DirectX::XMMatrixDeterminant(DirectX::XMLoadFloat4x4(&p.open)));
        if(std::fabs(determinant-1)>1e-4f)return false;
    }
    if(h[1]>=2){float v[6];std::memcpy(v,bytes.data()+572,24);if(!candidate.contact.set({v[0],v[1],v[2]},{v[3],v[4],v[5]}))return false;}
    if(h[1]==3){
        std::memcpy(candidate.rounds.data(),bytes.data()+596,24);
        for(unsigned i=0;i<6;i++){
            bool found=false;for(const auto& p:candidate.parts)found|=p.section==candidate.rounds[i];
            if(!found)return false;
            for(unsigned j=0;j<i;j++)if(candidate.rounds[j]==candidate.rounds[i])return false;
        }
        candidate.roundsBound=true;
    }
    candidate.ready=true;out=candidate;return true;
}
inline bool load(const std::filesystem::path& folder,const psxAssets::Pack& pack){
    profile={};std::vector<unsigned char> b,g,f;
    bool ok=psxAssets::read(folder/L"SelectedRevolver.bin",b,620)&&
        psxAssets::read(folder/L"SelectedGeometry.psx",g,64*1024*1024)&&
        psxAssets::read(folder/L"SelectedFrames.bin",f,4096)&&decode(b,g,f,pack,profile);
    if(ok&&profile.roundsBound&&selectedFrames::ready){
        using namespace DirectX;XMFLOAT3 center{};
        for(auto id:profile.rounds){const auto& s=pack.sections[id];XMFLOAT3 mean{};
            auto model=XMLoadFloat4x4(&selectedFrames::frames[0].editorToAim);
            for(const auto& part:profile.parts)if(part.section==id){model=XMLoadFloat4x4(&part.open)*model;break;}
            for(unsigned i=0;i<s.count;++i){const auto* v=pack.vertices[s.first+i].value;XMFLOAT3 q;
                XMStoreFloat3(&q,XMVector3TransformCoord(XMVectorSet(v[0],v[1],v[2],1),model));mean.x+=q.x;mean.y+=q.y;mean.z+=q.z;}
            if(!s.count)return false;center.x+=mean.x/s.count;center.y+=mean.y/s.count;center.z+=mean.z/s.count;
        }
        // Speedloader approaches the center of all six open chambers.
        profile.contact.port={center.x/6,center.y/6,center.z/6};
    }
    return ok;
}
// Count representation only: the native adapter does not expose individual
// spent chambers. Missing/stale ammo data preserves the established geometry.
inline bool visible(unsigned section,bool ammoValid,int loaded){
    if(!profile.ready||!profile.roundsBound||!ammoValid||loaded<0||loaded>6)return true;
    for(unsigned i=0;i<6;i++)if(profile.rounds[i]==section)return i<static_cast<unsigned>(loaded);
    return true;
}
inline DirectX::XMMATRIX transform(unsigned section,bool open,const DirectX::XMMATRIX& weapon){
    if(profile.ready&&open)for(const auto& p:profile.parts)if(p.section==section)return DirectX::XMLoadFloat4x4(&p.open)*weapon;
    return weapon;
}
inline DirectX::XMMATRIX animatedTransform(unsigned section,float amount,const DirectX::XMMATRIX& weapon){
    using namespace DirectX;
    if(!std::isfinite(amount)||amount<=0)return weapon;
    amount=std::clamp(amount,0.f,1.f);
    if(profile.ready)for(const auto& p:profile.parts)if(p.section==section){
        auto m=XMLoadFloat4x4(&p.open);auto translation=m.r[3];m.r[3]=XMVectorSet(0,0,0,1);
        auto end=XMQuaternionNormalize(XMQuaternionRotationMatrix(m));
        if(XMVectorGetW(end)<0)end=-end;
        auto q=XMQuaternionSlerp(XMQuaternionIdentity(),end,amount);
        auto rotation=XMMatrixRotationQuaternion(q);
        // Follow the hinge arc. Linear translation beside a quaternion blend
        // makes an offset cylinder drift away from its hinge mid-transition.
        auto xyz=XMVectorSetW(end,0);
        float sine=XMVectorGetX(XMVector3Length(xyz));
        auto offset=translation*amount;
        if(sine>1e-5f){
            auto axis=xyz/sine;
            auto axial=axis*XMVector3Dot(axis,translation);
            auto radial=XMVectorSetW(translation,0)-axial;
            auto pivot=(radial+XMVector3Cross(axis,radial)*(XMVectorGetW(end)/sine))*.5f;
            offset=pivot-XMVector3TransformNormal(pivot,rotation)+axial*amount;
        }
        return rotation*XMMatrixTranslationFromVector(offset)*weapon;
    }
    return weapon;
}
inline DirectX::XMMATRIX spinTransform(unsigned section,float angle){
    using namespace DirectX;
    bool cylinder=profile.ready&&section==profile.parts[1].section;
    for(auto id:profile.rounds)cylinder|=profile.roundsBound&&section==id;
    if(!spinReady||!cylinder||!std::isfinite(angle))return XMMatrixIdentity();
    return XMMatrixTranslation(-spinPivot.x,-spinPivot.y,-spinPivot.z)*
        XMMatrixRotationAxis(XMLoadFloat3(&spinAxis),angle)*XMMatrixTranslation(spinPivot.x,spinPivot.y,spinPivot.z);
}
}
