#pragma once
#include "selected_reload_profile.hpp"
namespace selectedShotgun {
struct Rigid {DirectX::XMFLOAT4 rotation{};DirectX::XMFLOAT3 translation{};};
struct Key {float seconds{},pump{};std::vector<Rigid> parts;};
struct Motion {std::vector<unsigned> sections;std::vector<Key> keys;bool ready{};};
inline Motion motion;
inline bool decode(const std::vector<unsigned char>& b,const std::vector<unsigned char>& g,
                   const std::vector<unsigned char>& f,const psxAssets::Pack& pack,Motion& out){
    using namespace DirectX;
    if(b.size()<32)return false;
    unsigned h[2],counts[2];std::uint64_t hashes[2];
    std::memcpy(h,b.data(),8);std::memcpy(hashes,b.data()+8,16);std::memcpy(counts,b.data()+24,8);
    if(h[0]!=0x53585044||h[1]!=1||hashes[0]!=selectedReload::fingerprint(g)||hashes[1]!=selectedReload::fingerprint(f))return false;
    const unsigned n=counts[0],k=counts[1];
    if(n==0||n>32||k<2||k>1024||b.size()!=32+size_t(n)*4+size_t(k)*(8+size_t(n)*64))return false;
    Motion candidate;candidate.sections.resize(n);std::memcpy(candidate.sections.data(),b.data()+32,n*4);
    for(unsigned i=0;i<n;i++){
        auto id=candidate.sections[i];if(id>=pack.sections.size()||pack.sections[id].weapon!=3||pack.sections[id].kind!=psxAssets::Weapon)return false;
        for(unsigned j=0;j<i;j++)if(candidate.sections[j]==id)return false;
    }
    size_t offset=32+n*4;
    for(unsigned i=0;i<k;i++){
        Key key;std::memcpy(&key.seconds,b.data()+offset,4);std::memcpy(&key.pump,b.data()+offset+4,4);offset+=8;
        if(!std::isfinite(key.seconds)||key.seconds<0||key.seconds>30||(!i&&key.seconds!=0)||(i&&key.seconds<=candidate.keys.back().seconds)||
           !std::isfinite(key.pump)||key.pump<-.00001f||key.pump>1.00001f)return false;
        for(unsigned j=0;j<n;j++){
            XMFLOAT4X4 matrix;std::memcpy(&matrix,b.data()+offset,64);offset+=64;
            for(unsigned r=0;r<4;r++)for(unsigned c=0;c<4;c++)if(!std::isfinite(matrix.m[r][c])||std::fabs(matrix.m[r][c])>2)return false;
            for(unsigned r=0;r<3;r++){
                if(std::fabs(matrix.m[r][3])>1e-5f||std::fabs(matrix.m[3][r])>1)return false;
                for(unsigned c=0;c<3;c++){float dot=0;for(unsigned d=0;d<3;d++)dot+=matrix.m[r][d]*matrix.m[c][d];if(std::fabs(dot-(r==c?1.f:0.f))>1e-4f)return false;}
            }
            auto m=XMLoadFloat4x4(&matrix);
            if(std::fabs(matrix.m[3][3]-1)>1e-5f||std::fabs(XMVectorGetX(XMMatrixDeterminant(m))-1)>1e-4f)return false;
            XMVECTOR scale,q,t;if(!XMMatrixDecompose(&scale,&q,&t,m))return false;
            Rigid rigid;XMStoreFloat4(&rigid.rotation,XMQuaternionNormalize(q));XMStoreFloat3(&rigid.translation,t);key.parts.push_back(rigid);
        }
        candidate.keys.push_back(std::move(key));
    }
    candidate.ready=true;out=std::move(candidate);return true;
}
inline bool loadMotion(const std::filesystem::path& folder,const psxAssets::Pack& pack){
    motion={};std::vector<unsigned char>b,g,f;
    return psxAssets::read(folder/L"SelectedShotgunMotion.bin",b,3*1024*1024)&&
        psxAssets::read(folder/L"SelectedGeometry.psx",g,64*1024*1024)&&
        psxAssets::read(folder/L"SelectedFrames.bin",f,4096)&&decode(b,g,f,pack,motion);
}
inline DirectX::XMMATRIX sample(const Motion& motion,unsigned section,float seconds){
    using namespace DirectX;
    if(!motion.ready||!std::isfinite(seconds))return XMMatrixIdentity();
    auto id=std::find(motion.sections.begin(),motion.sections.end(),section);if(id==motion.sections.end())return XMMatrixIdentity();
    auto index=size_t(id-motion.sections.begin());
    seconds=std::clamp(seconds,motion.keys.front().seconds,motion.keys.back().seconds);
    auto upper=std::upper_bound(motion.keys.begin(),motion.keys.end(),seconds,[](float t,const Key& k){return t<k.seconds;});
    const auto& a=upper==motion.keys.begin()?motion.keys.front():*(upper-1);
    const auto& b=upper==motion.keys.end()?motion.keys.back():*upper;
    float amount=b.seconds>a.seconds?(seconds-a.seconds)/(b.seconds-a.seconds):0;
    auto q=XMQuaternionSlerp(XMLoadFloat4(&a.parts[index].rotation),XMLoadFloat4(&b.parts[index].rotation),amount);
    auto t=XMVectorLerp(XMLoadFloat3(&a.parts[index].translation),XMLoadFloat3(&b.parts[index].translation),amount);
    return XMMatrixRotationQuaternion(q)*XMMatrixTranslationFromVector(t);
}
}
