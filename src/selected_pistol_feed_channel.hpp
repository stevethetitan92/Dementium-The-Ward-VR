#pragma once
#include "selected_reload_profile.hpp"
#include <atomic>
namespace selectedPistolFeedChannel {
inline bool applied{};

inline bool decode(const std::vector<unsigned char>& b,const std::vector<unsigned char>& g,
                   const std::vector<unsigned char>& f,const psxAssets::Pack& pack,std::vector<psxAssets::Vertex>& out){
    // Optional, audited subtraction of the source slide's internal feed passage.
    // No replacement material: clipped vertices retain source triangle UVs.
    if(b.size()!=32+4923*sizeof(psxAssets::Vertex)||selectedReload::fingerprint(b)!=0x792366ae0a91aba3ull)return false;
    unsigned magic,version,id,count;std::uint64_t gh,fh;
    std::memcpy(&magic,b.data(),4);std::memcpy(&version,b.data()+4,4);
    std::memcpy(&gh,b.data()+8,8);std::memcpy(&fh,b.data()+16,8);
    std::memcpy(&id,b.data()+24,4);std::memcpy(&count,b.data()+28,4);
    if(magic!=0x46585044||version!=1||id!=60||count!=4923||gh!=selectedReload::fingerprint(g)||fh!=selectedReload::fingerprint(f)||id>=pack.sections.size())return false;
    const auto& base=pack.sections[id];
    if(base.weapon!=1||base.kind!=psxAssets::Weapon||base.count!=4884||base.first>pack.vertices.size()||base.count>pack.vertices.size()-base.first)return false;
    std::vector<psxAssets::Vertex> result(count);std::memcpy(result.data(),b.data()+32,count*sizeof(psxAssets::Vertex));
    out=std::move(result);return true;
}
inline bool load(const std::filesystem::path& folder,psxAssets::Pack& pack){
    applied=false;std::vector<unsigned char>b,g,f;std::vector<psxAssets::Vertex> replacement;
    if(!psxAssets::read(folder/L"SelectedPistolFeedChannel.bin",b,300000)||!psxAssets::read(folder/L"SelectedGeometry.psx",g,64*1024*1024)||!psxAssets::read(folder/L"SelectedFrames.bin",f,4096)||!decode(b,g,f,pack,replacement))return false;
    // Keep all original vertices and section identities for existing bindings.
    // Only the bound slide draw reads the appended, validated triangle bank.
    auto& frame=pack.sections[60];frame.first=unsigned(pack.vertices.size());frame.count=unsigned(replacement.size());
    pack.vertices.insert(pack.vertices.end(),replacement.begin(),replacement.end());applied=true;return true;
}
}

