#pragma once
#include "selected_reload_profile.hpp"
namespace selectedHoldingHand {
inline unsigned section=~0u;
inline unsigned relaxedSection=~0u;
inline unsigned flashlightSection=~0u,flashlightPressSection=~0u;
inline bool auxiliary(unsigned id){return id==section||id==relaxedSection||id==flashlightSection||id==flashlightPressSection;}
inline bool decode(const std::vector<unsigned char>& b,const std::vector<unsigned char>& g,const std::vector<unsigned char>& f,const psxAssets::Pack& pack,std::vector<psxAssets::Vertex>& out){
    if(b.size()<28)return false;
    unsigned h[2],count;std::uint64_t hashes[2];std::memcpy(h,b.data(),8);std::memcpy(hashes,b.data()+8,16);std::memcpy(&count,b.data()+24,4);
    if(h[0]!=0x4c585044||h[1]!=1||hashes[0]!=selectedReload::fingerprint(g)||hashes[1]!=selectedReload::fingerprint(f)||!count||count>30000||count%3||b.size()!=28+std::size_t(count)*sizeof(psxAssets::Vertex))return false;
    const psxAssets::Section* base=nullptr;
    for(unsigned id=0;id<pack.sections.size();++id){const auto& s=pack.sections[id];
        if(auxiliary(id))continue;
        if(s.weapon==2&&s.kind==psxAssets::LeftSupport){if(base)return false;base=&s;}}
    if(!base||base->count!=count||std::size_t(base->first)+count>pack.vertices.size())return false;
    std::vector<psxAssets::Vertex> candidate(count);std::memcpy(candidate.data(),b.data()+28,count*sizeof(psxAssets::Vertex));
    for(unsigned i=0;i<count;++i){const auto* v=candidate[i].value;const auto* original=pack.vertices[base->first+i].value;
        for(unsigned j=0;j<12;++j)if(!std::isfinite(v[j]))return false;
        for(unsigned j=0;j<3;++j)if(std::fabs(v[j])>10)return false;
        float n=v[3]*v[3]+v[4]*v[4]+v[5]*v[5];if(n<.9f||n>1.1f)return false;
        // Finger posing cannot change texture/material correspondence or cap.
        if(std::memcmp(v+6,original+6,6*sizeof(float)))return false;
        if(original[11]==4&&std::memcmp(v,original,12*sizeof(float)))return false;
    }
    out=std::move(candidate);return true;
}
inline bool loadPose(const std::filesystem::path& folder,const wchar_t* name,psxAssets::Pack& pack,unsigned& target){
    target=~0u;std::vector<unsigned char>b,g,f;std::vector<psxAssets::Vertex> vertices;
    if(!psxAssets::read(folder/name,b,28+30000*sizeof(psxAssets::Vertex))||!psxAssets::read(folder/L"SelectedGeometry.psx",g,64*1024*1024)||!psxAssets::read(folder/L"SelectedFrames.bin",f,4096)||!decode(b,g,f,pack,vertices))return false;
    target=static_cast<unsigned>(pack.sections.size());pack.sections.push_back({2,psxAssets::LeftSupport,static_cast<unsigned>(pack.vertices.size()),static_cast<unsigned>(vertices.size())});
    pack.vertices.insert(pack.vertices.end(),vertices.begin(),vertices.end());return true;
}
inline bool load(const std::filesystem::path& folder,psxAssets::Pack& pack){
    section=relaxedSection=flashlightSection=flashlightPressSection=~0u;
    bool ok=loadPose(folder,L"SelectedHoldingHand.bin",pack,section);
    loadPose(folder,L"SelectedRelaxedHand.bin",pack,relaxedSection);
    loadPose(folder,L"SelectedFlashlightHand.bin",pack,flashlightSection);
    loadPose(folder,L"SelectedFlashlightPress.bin",pack,flashlightPressSection);return ok;
}
inline bool use(int weapon,bool valid,manualReload::Stage stage){return section!=~0u&&weapon==1&&valid&&stage==manualReload::Stage::MagazineHeld;}
}
