#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

// A separate format prevents editor-space vertices from being mistaken for
// the older, controller-space NativeHands.bin banks. Loading is transactional.
namespace psxAssets {
struct Vertex { float value[12]; };
struct Section { std::uint32_t weapon, kind, first, count; };
enum : std::uint32_t { Weapon=0, RightHand=1, LeftSupport=2 };
struct Pack {
    std::vector<Vertex> vertices;
    std::vector<Section> sections;
    std::vector<unsigned char> texture;
    std::uint32_t textureSize{};
    std::uint32_t version{}, rotorSection=~0u;
    std::array<float,3> rotorPivot{},rotorAxis{};
};
inline bool read(const std::filesystem::path& path,std::vector<unsigned char>& bytes,std::size_t limit) {
    std::ifstream f(path,std::ios::binary|std::ios::ate);
    if(!f)return false;
    const auto length=f.tellg();
    if(length<=0||static_cast<std::uint64_t>(length)>limit)return false;
    bytes.resize(static_cast<std::size_t>(length));f.seekg(0);
    return bool(f.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())));
}
inline std::uint32_t big32(const unsigned char* p) {
    return (std::uint32_t(p[0])<<24)|(std::uint32_t(p[1])<<16)|(std::uint32_t(p[2])<<8)|p[3];
}
inline bool decode(const std::vector<unsigned char>& bytes,const std::vector<unsigned char>& texture,Pack& result) {
    // magic, version, frame (1 = editor), section count, vertex count, atlas size.
    if(bytes.size()<24)return false;
    std::uint32_t h[6]{};std::memcpy(h,bytes.data(),sizeof h);
    if(h[0]!=0x58535044||h[1]<1||h[1]>3||h[2]!=1||h[4]>1000000||!h[4])return false;
    if(h[1]<3&&(h[1]==1?h[3]!=26:h[3]!=27))return false;
    if(h[1]==3&&(h[3]<18||h[3]>256))return false;
    if(h[1]==1?h[5]!=4096:h[5]!=8192)return false;
    // V3 carries an optional rotor binding followed by pivot and unit axis.
    // It permits independent material/part sections without changing hand IDs.
    const std::size_t headerBytes=h[1]==3?52:24;
    if(bytes.size()<headerBytes)return false;
    const std::size_t start=headerBytes+std::size_t(h[3])*sizeof(Section);
    if(start+std::size_t(h[4])*sizeof(Vertex)!=bytes.size())return false;
    constexpr unsigned char png[]={137,80,78,71,13,10,26,10};
    if(texture.size()<33||texture.size()>64*1024*1024||std::memcmp(texture.data(),png,8)||
       big32(texture.data()+8)!=13||std::memcmp(texture.data()+12,"IHDR",4)||
       big32(texture.data()+16)!=h[5]||big32(texture.data()+20)!=h[5])return false;
    Pack candidate;candidate.version=h[1];candidate.sections.resize(h[3]);candidate.vertices.resize(h[4]);
    if(h[1]==3){
        std::memcpy(&candidate.rotorSection,bytes.data()+24,4);
        std::memcpy(candidate.rotorPivot.data(),bytes.data()+28,12);
        std::memcpy(candidate.rotorAxis.data(),bytes.data()+40,12);
        for(unsigned i=0;i<3;i++)if(!std::isfinite(candidate.rotorPivot[i])||!std::isfinite(candidate.rotorAxis[i])||std::fabs(candidate.rotorPivot[i])>10)return false;
        if(candidate.rotorSection!=~0u){
            const auto& a=candidate.rotorAxis;const float norm=a[0]*a[0]+a[1]*a[1]+a[2]*a[2];
            if(candidate.rotorSection>=h[3]||std::fabs(norm-1.f)>.001f)return false;
        }
    }
    std::memcpy(candidate.sections.data(),bytes.data()+headerBytes,h[3]*sizeof(Section));
    std::memcpy(candidate.vertices.data(),bytes.data()+start,h[4]*sizeof(Vertex));
    std::array<unsigned,7> guns{},right{},left{};std::size_t end=0;
    for(const auto& section:candidate.sections){
        if(section.weapon>=7||section.kind>LeftSupport||section.first!=end||!section.count||section.count%3)return false;
        end+=section.count;if(end>candidate.vertices.size())return false;
        auto& counts=section.kind==Weapon?guns:section.kind==RightHand?right:left;
        ++counts[section.weapon];
    }
    if(end!=candidate.vertices.size())return false;
    if(candidate.rotorSection!=~0u){const auto& s=candidate.sections[candidate.rotorSection];if(s.weapon!=5||s.kind!=Weapon)return false;}
    for(unsigned i=0;i<7;i++)if(!guns[i]||right[i]!=1||left[i]!=unsigned(i>=2&&i<=5))return false;
    for(const auto& vertex:candidate.vertices){
        const float* p=vertex.value;
        for(unsigned i=0;i<12;i++)if(!std::isfinite(p[i]))return false;
        if(std::fabs(p[0])>10||std::fabs(p[1])>10||std::fabs(p[2])>10)return false;
        const float n=p[3]*p[3]+p[4]*p[4]+p[5]*p[5];
        if(n<.9f||n>1.1f||p[9]<0||p[9]>1||p[10]<0||p[10]>1||(p[11]!=4&&p[11]!=7&&(h[1]<2||p[11]!=5)))return false;
    }
    candidate.texture=texture;candidate.textureSize=h[5];result=std::move(candidate);return true;
}
inline bool load(const std::filesystem::path& folder,Pack& result) {
    std::vector<unsigned char> bytes,texture;
    return read(folder/L"SelectedGeometry.psx",bytes,64*1024*1024)&&
        read(folder/L"SelectedGeometry.png",texture,64*1024*1024)&&decode(bytes,texture,result);
}
}
