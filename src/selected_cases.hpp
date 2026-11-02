#pragma once
#include "selected_reload_profile.hpp"
#include <string>
namespace selectedCases {
inline std::array<unsigned,5> sections{~0u,~0u,~0u,~0u,~0u};
inline bool contains(unsigned i){for(auto s:sections)if(s!=~0u&&s==i)return true;return false;}
inline void load(const std::filesystem::path& folder,psxAssets::Pack& pack){
 sections.fill(~0u);std::vector<unsigned char> g;
 if(!psxAssets::read(folder/L"SelectedGeometry.psx",g,64*1024*1024))return;
 for(unsigned w=1;w<5;++w){std::vector<unsigned char> b;
  if(!psxAssets::read(folder/(L"SelectedTexturedCase"+std::to_wstring(w)+L".bin"),b,4*1024*1024)||b.size()<20)continue;
  unsigned h[3];std::uint64_t hash;std::memcpy(h,b.data(),12);std::memcpy(&hash,b.data()+12,8);
  if(h[0]!=0x54585044||h[1]!=1||!h[2]||h[2]%3||h[2]>60000||b.size()!=20+size_t(h[2])*sizeof(psxAssets::Vertex)||hash!=selectedReload::fingerprint(g))continue;
  std::vector<psxAssets::Vertex> v(h[2]);std::memcpy(v.data(),b.data()+20,v.size()*sizeof(v[0]));bool valid=true;
  for(const auto& vert:v){const auto* p=vert.value;
   for(unsigned i=0;i<12;++i)if(!std::isfinite(p[i]))valid=false;
   for(unsigned i=0;i<3;++i)if(std::fabs(p[i])>.501f)valid=false;
   for(unsigned i=6;i<11;++i)if(p[i]<0||p[i]>1)valid=false;
   float n=p[3]*p[3]+p[4]*p[4]+p[5]*p[5];if(n<.9f||n>1.1f||p[11]!=7)valid=false;
  }
  if(!valid)continue;
  sections[w]=unsigned(pack.sections.size());pack.sections.push_back({w,psxAssets::Weapon,unsigned(pack.vertices.size()),unsigned(v.size())});pack.vertices.insert(pack.vertices.end(),v.begin(),v.end());
 }
}
}
