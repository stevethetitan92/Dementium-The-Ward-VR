#pragma once
#include "selected_reload_profile.hpp"
namespace selectedShotgun {
// Authored demonstration cartridges are not live ammunition. Keep them out
// of the draw list; the existing reload adapter owns the held shell and ammo.
struct Props {std::array<unsigned,2> cartridges{};bool ready{};};
inline Props props;
inline bool decodeProps(const std::vector<unsigned char>& b,const std::vector<unsigned char>& g,const std::vector<unsigned char>& f,const psxAssets::Pack& pack,Props& out){
    if(b.size()!=32)return false;
    unsigned h[2];std::uint64_t hashes[2];std::memcpy(h,b.data(),8);std::memcpy(hashes,b.data()+8,16);
    if(h[0]!=0x43585044||h[1]!=1||hashes[0]!=selectedReload::fingerprint(g)||hashes[1]!=selectedReload::fingerprint(f))return false;
    Props p;std::memcpy(p.cartridges.data(),b.data()+24,8);
    if(p.cartridges[0]==p.cartridges[1])return false;
    for(auto id:p.cartridges)if(id>=pack.sections.size()||pack.sections[id].weapon!=3||pack.sections[id].kind!=psxAssets::Weapon)return false;
    p.ready=true;out=p;return true;
}
inline bool loadProps(const std::filesystem::path& folder,const psxAssets::Pack& pack){
    props={};std::vector<unsigned char>b,g,f;
    return psxAssets::read(folder/L"SelectedShotgunProps.bin",b,32)&&psxAssets::read(folder/L"SelectedGeometry.psx",g,64*1024*1024)&&psxAssets::read(folder/L"SelectedFrames.bin",f,4096)&&decodeProps(b,g,f,pack,props);
}
inline bool propVisible(unsigned id){return !props.ready||(id!=props.cartridges[0]&&id!=props.cartridges[1]);}
}
