#pragma once
#include "psx_asset_pack.hpp"
#include "selected_reload_motion.hpp"
#include <atomic>
namespace selectedReload {
// Desktop-only draw inspection; never updates the reload transaction/ammo.
inline std::atomic<unsigned> diagnosticMode{};
inline std::atomic<std::uint64_t> diagnosticUntil{};
struct Profile {
    Geometry geometry;
    XrVector3f well{},handle{};
    std::array<unsigned,7> sections{}; // magazine followed by six slide pieces
    bool ready{};
    Role role(unsigned section)const{
        if(!ready)return Role::Fixed;
        if(section==sections[0])return Role::Magazine;
        for(unsigned i=1;i<sections.size();i++)if(section==sections[i])return Role::Slide;
        return Role::Fixed;
    }
};
inline Profile profile;
inline std::uint64_t fingerprint(const std::vector<unsigned char>& bytes){
    std::uint64_t h=14695981039346656037ull;for(auto b:bytes){h^=b;h*=1099511628211ull;}return h;
}
inline bool decodeProfile(const std::vector<unsigned char>& bytes,const std::vector<unsigned char>& geometry,
                          const std::vector<unsigned char>& frames,const psxAssets::Pack& pack,Profile& out){
    // Fixed little-endian layout avoids compiler packing and unbounded counts.
    if(bytes.size()!=100&&bytes.size()!=112)return false;
    unsigned header[2];std::memcpy(header,bytes.data(),8);
    std::uint64_t hashes[2];std::memcpy(hashes,bytes.data()+8,16);
    if(header[0]!=0x52585044||!((header[1]==1&&bytes.size()==100)||(header[1]==2&&bytes.size()==112))||hashes[0]!=fingerprint(geometry)||hashes[1]!=fingerprint(frames))return false;
    Profile candidate;std::memcpy(candidate.sections.data(),bytes.data()+24,28);
    float values[12];std::memcpy(values,bytes.data()+52,48);
    for(auto v:values)if(!std::isfinite(v)||std::fabs(v)>1)return false;
    candidate.geometry.magazineContactEditor={values[0],values[1],values[2]};
    candidate.well={values[3],values[4],values[5]};candidate.handle={values[6],values[7],values[8]};
    candidate.geometry.pullAxisGrip={values[9],values[10],values[11]};
    if(header[1]==2)std::memcpy(&candidate.geometry.heldContactGrip,bytes.data()+100,12);
    manualReload::Gestures validation;
    if(!validation.setReplacementGeometry(candidate.well,candidate.handle,candidate.geometry.pullAxisGrip,candidate.geometry.heldContactGrip))return false;
    for(unsigned i=0;i<candidate.sections.size();i++){
        auto id=candidate.sections[i];if(id>=pack.sections.size())return false;
        const auto& section=pack.sections[id];if(section.weapon!=1||section.kind!=psxAssets::Weapon)return false;
        for(unsigned j=0;j<i;j++)if(candidate.sections[j]==id)return false;
    }
    candidate.ready=true;out=candidate;return true;
}
inline bool loadProfile(const std::filesystem::path& folder,const psxAssets::Pack& pack){
    std::vector<unsigned char> bytes,geometry,frames;profile={};
    if(!psxAssets::read(folder/L"SelectedReload.bin",bytes,112)||
       !psxAssets::read(folder/L"SelectedGeometry.psx",geometry,64*1024*1024)||
       !psxAssets::read(folder/L"SelectedFrames.bin",frames,4096))return false;
    return decodeProfile(bytes,geometry,frames,pack,profile);
}
}
