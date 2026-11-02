#pragma once
#include "selected_revolver_profile.hpp"
namespace selectedSpeedloader {
inline unsigned section=~0u;
inline bool load(const std::filesystem::path& folder,psxAssets::Pack& pack){
 section=~0u;std::vector<unsigned char>b,g,f;
 if(!selectedRevolver::profile.ready||!psxAssets::read(folder/L"SelectedSpeedloader.bin",b,28+60000*sizeof(psxAssets::Vertex))||b.size()<28||
    !psxAssets::read(folder/L"SelectedGeometry.psx",g,64*1024*1024)||!psxAssets::read(folder/L"SelectedFrames.bin",f,4096))return false;
 unsigned magic,version,count;std::uint64_t hashes[2];
 std::memcpy(&magic,b.data(),4);std::memcpy(&version,b.data()+4,4);std::memcpy(hashes,b.data()+8,16);std::memcpy(&count,b.data()+24,4);
 if(magic!=0x53585044||version!=1||!count||count%3||count>60000||b.size()!=28+size_t(count)*sizeof(psxAssets::Vertex)||
    hashes[0]!=selectedReload::fingerprint(g)||hashes[1]!=selectedReload::fingerprint(f))return false;
 std::vector<psxAssets::Vertex> v(count);std::memcpy(v.data(),b.data()+28,count*sizeof(psxAssets::Vertex));
 for(const auto& vertex:v){const auto* p=vertex.value;
  for(unsigned i=0;i<12;++i)if(!std::isfinite(p[i]))return false;
  for(unsigned i=0;i<3;++i)if(std::fabs(p[i])>2)return false;
  float n=p[3]*p[3]+p[4]*p[4]+p[5]*p[5];if(n<.9f||n>1.1f||p[11]!=7)return false;
  for(unsigned i=6;i<11;++i)if(p[i]<0||p[i]>1)return false;
 }
 section=static_cast<unsigned>(pack.sections.size());
 pack.sections.push_back({0,psxAssets::Weapon,static_cast<unsigned>(pack.vertices.size()),count});
 pack.vertices.insert(pack.vertices.end(),v.begin(),v.end());return true;
}
inline DirectX::XMMATRIX transform(const DirectX::XMMATRIX& carrier){
 return DirectX::XMLoadFloat4x4(&selectedRevolver::profile.parts[1].open)*carrier;
}
}
