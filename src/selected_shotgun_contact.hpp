#pragma once
#include "selected_reload_profile.hpp"
#include "revolver_contact.hpp"
namespace selectedShotgun {
struct Contact {arsenal::RevolverContact insertion;XrVector3f pump{};};
inline Contact contact;
inline bool decodeContact(const std::vector<unsigned char>& b,const std::vector<unsigned char>& g,const std::vector<unsigned char>& f,Contact& out){
    if(b.size()!=60)return false;
    unsigned h[2];std::uint64_t hashes[2];float v[9];std::memcpy(h,b.data(),8);std::memcpy(hashes,b.data()+8,16);std::memcpy(v,b.data()+24,36);
    if(h[0]!=0x48585044||h[1]!=1||hashes[0]!=selectedReload::fingerprint(g)||hashes[1]!=selectedReload::fingerprint(f))return false;
    for(float x:v)if(!std::isfinite(x)||std::fabs(x)>1)return false;
    Contact p;if(!p.insertion.set({v[0],v[1],v[2]},{v[6],v[7],v[8]}))return false;
    p.pump={v[3],v[4],v[5]};if(manualReload::distance(p.pump,{0,0,0})>1)return false;
    out=p;return true;
}
inline bool loadContact(const std::filesystem::path& folder){
    contact={};std::vector<unsigned char>b,g,f;
    return psxAssets::read(folder/L"SelectedShotgunContact.bin",b,60)&&
        psxAssets::read(folder/L"SelectedGeometry.psx",g,64*1024*1024)&&
        psxAssets::read(folder/L"SelectedFrames.bin",f,4096)&&decodeContact(b,g,f,contact);
}
}
