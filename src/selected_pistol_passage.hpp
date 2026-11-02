#pragma once
#include "selected_reload_profile.hpp"
namespace selectedPistolPassage {
inline bool applied{};
struct Replacement {unsigned section;std::vector<psxAssets::Vertex> vertices;};
inline bool decode(const std::vector<unsigned char>& b,const std::vector<unsigned char>& g,
                   const std::vector<unsigned char>& f,const psxAssets::Pack& pack,std::vector<Replacement>& out){
 if(b.size()!=513604||selectedReload::fingerprint(b)!=0xf40b90695f9707b5ull)return false;
 unsigned magic,version,count;std::uint64_t gh,fh;
 std::memcpy(&magic,b.data(),4);std::memcpy(&version,b.data()+4,4);std::memcpy(&gh,b.data()+8,8);std::memcpy(&fh,b.data()+16,8);std::memcpy(&count,b.data()+24,4);
 if(magic!=0x43585044||version!=1||count!=6||gh!=selectedReload::fingerprint(g)||fh!=selectedReload::fingerprint(f))return false;
 constexpr unsigned ids[]={59,67,72,73,74,77},bases[]={3387,1284,84,534,588,2724},counts[]={4203,1599,372,777,705,3042};
 size_t offset=28;std::vector<Replacement> result;
 for(unsigned i=0;i<6;++i){
  unsigned h[3];if(offset+sizeof h>b.size())return false;std::memcpy(h,b.data()+offset,sizeof h);offset+=sizeof h;
  if(h[0]!=ids[i]||h[1]!=bases[i]||h[2]!=counts[i]||h[0]>=pack.sections.size())return false;
  const auto& s=pack.sections[h[0]];
  if(s.weapon!=1||s.kind!=psxAssets::Weapon||s.count!=h[1]||s.first>pack.vertices.size()||s.count>pack.vertices.size()-s.first)return false;
  size_t bytes=size_t(h[2])*sizeof(psxAssets::Vertex);if(bytes>b.size()-offset)return false;
  Replacement r{h[0],std::vector<psxAssets::Vertex>(h[2])};std::memcpy(r.vertices.data(),b.data()+offset,bytes);offset+=bytes;result.push_back(std::move(r));
 }
 if(offset!=b.size())return false;out=std::move(result);return true;
}
inline bool load(const std::filesystem::path& folder,psxAssets::Pack& pack){
 applied=false;std::vector<unsigned char>b,g,f;std::vector<Replacement> replacements;
 if(!psxAssets::read(folder/L"SelectedPistolPassage.bin",b,600000)||!psxAssets::read(folder/L"SelectedGeometry.psx",g,64*1024*1024)||!psxAssets::read(folder/L"SelectedFrames.bin",f,4096)||!decode(b,g,f,pack,replacements))return false;
 // Call only AFTER source magazine round placement and exit-rail calibration.
 // Preserve original vertices and section IDs; only draw ranges change.
 size_t extra=0;for(const auto& r:replacements)extra+=r.vertices.size();pack.vertices.reserve(pack.vertices.size()+extra);
 for(const auto& r:replacements){auto& s=pack.sections[r.section];s.first=unsigned(pack.vertices.size());s.count=unsigned(r.vertices.size());pack.vertices.insert(pack.vertices.end(),r.vertices.begin(),r.vertices.end());}
 applied=true;return true;
}
}
