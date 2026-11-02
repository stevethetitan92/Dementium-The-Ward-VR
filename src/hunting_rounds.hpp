#pragma once
#include "selected_reload_profile.hpp"
namespace huntingRounds {
inline unsigned first=~0u;
inline bool contains(unsigned id){return first!=~0u&&id>=first&&id<first+5;}
inline unsigned count(bool valid,int loaded,int reserve,bool incoming){return valid&&loaded>=0&&reserve>=0?unsigned(std::clamp(incoming?loaded+reserve:loaded,0,5)):0;}
inline bool append(const std::filesystem::path& folder,psxAssets::Pack& pack){
 first=~0u;std::vector<unsigned char>b,g;
 if(!psxAssets::read(folder/L"SelectedHuntingRounds.bin",b,4*1024*1024)||b.size()<28||!psxAssets::read(folder/L"SelectedGeometry.psx",g,64*1024*1024))return false;
 unsigned magic,version,n;std::uint64_t hashes[2];std::memcpy(&magic,b.data(),4);std::memcpy(&version,b.data()+4,4);std::memcpy(hashes,b.data()+8,16);std::memcpy(&n,b.data()+24,4);
 if(magic!=0x42585044||version!=1||!n||n>12000||n%3||b.size()!=28+size_t(n)*5*sizeof(psxAssets::Vertex)||hashes[0]!=selectedReload::fingerprint(g)||hashes[1]!=selectedReload::fingerprint(pack.texture))return false;
 std::vector<psxAssets::Vertex> vertices(n*5);std::memcpy(vertices.data(),b.data()+28,vertices.size()*sizeof(psxAssets::Vertex));
 for(const auto& v:vertices){for(float f:v.value)if(!std::isfinite(f))return false;if(v.value[9]<0||v.value[9]>1||v.value[10]<0||v.value[10]>1||v.value[11]!=7)return false;}
 first=unsigned(pack.sections.size());
 for(unsigned r=0;r<5;++r){auto start=unsigned(pack.vertices.size());pack.vertices.insert(pack.vertices.end(),vertices.begin()+r*n,vertices.begin()+(r+1)*n);pack.sections.push_back({4,psxAssets::Weapon,start,n});}
 return true;
}
}
