#pragma once
#include "psx_asset_pack.hpp"
namespace selectedHuntingTexture {
inline bool apply(const std::filesystem::path& folder,psxAssets::Pack& pack){
 std::vector<unsigned char> marker;
 if(!psxAssets::read(folder/L"SelectedHuntingFullTexture.bin",marker,16)||marker.size()!=16)return false;
 std::uint32_t header[2];std::uint64_t expected;std::memcpy(header,marker.data(),8);std::memcpy(&expected,marker.data()+8,8);
 std::uint64_t hash=14695981039346656037ull;for(auto b:pack.texture){hash^=b;hash*=1099511628211ull;}
 if(header[0]!=0x48545844||header[1]!=1||expected!=hash)return false;
 for(const auto& s:pack.sections)if(s.weapon==4&&s.kind==psxAssets::Weapon)
  for(unsigned i=0;i<s.count;++i){auto& v=pack.vertices[s.first+i];
   const float x=v.value[9]*8192,y=v.value[10]*8192;
   if(x>=2058&&x<=2186&&y>=2&&y<=130){
    v.value[9]=(2058.5f+(x-2058.5f)*(1023.f/127.f))/8192;
    v.value[10]=(2.5f+(y-2.5f)*(1023.f/127.f))/8192;
   }
  }
 return true;
}
}
