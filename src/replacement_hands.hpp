#pragma once
#include "wristband_mesh.hpp"
#include "hand_grip_state.hpp"
#include "replacement_band_fit.hpp"
#include "native_band_fit.hpp"
#include <filesystem>
#include <fstream>
namespace replacementHands {
inline std::vector<wristband::Vertex> geometry;inline std::vector<unsigned char> skinPNG;
inline unsigned first[64]{},count[64]{},sections{};
inline bool ready{},nativeSource{},savedFits{};
inline bool load(const std::filesystem::path& file){
    ready=false;savedFits=false;nativeSource=file.filename()==L"NativeHands.bin";geometry.clear();std::ifstream f(file,std::ios::binary|std::ios::ate);if(!f)return false;
    auto size=f.tellg();if(size<84||size>24*1024*1024)return false;f.seekg(0);
    unsigned header[3]{};f.read(reinterpret_cast<char*>(header),12);
    if(!f||header[0]!=0x444e4844||(header[1]!=1&&header[1]!=2)||(header[2]!=18&&header[2]!=28&&header[2]!=36&&header[2]!=64))return false;
    const bool fitted=nativeSource&&header[1]==2&&header[2]==64;
    sections=header[2];unsigned counts[64]{};f.read(reinterpret_cast<char*>(counts),sections*4);if(!f)return false;
    std::size_t total=0;
    for(unsigned i=0;i<sections;i++){if(!counts[i]||counts[i]>100000||counts[i]%3)return false;first[i]=static_cast<unsigned>(total);count[i]=counts[i];total+=count[i];}
    if(12+sections*4+total*sizeof(wristband::Vertex)!=static_cast<std::size_t>(size))return false;
    std::vector<wristband::Vertex> candidate(total);f.read(reinterpret_cast<char*>(candidate.data()),total*sizeof(wristband::Vertex));if(!f)return false;
    for(const auto& v:candidate){const float* p=&v.x;for(unsigned i=0;i<12;i++)if(!std::isfinite(p[i]))return false;
        const float bound=fitted?.75f:.3f;if(std::fabs(v.x)>bound||std::fabs(v.y)>bound||std::fabs(v.z)>bound)return false;
        float length=v.nx*v.nx+v.ny*v.ny+v.nz*v.nz;if(length<.9f||length>1.1f)return false;
    }
    std::ifstream skin(file.parent_path()/(nativeSource?L"NativeHandAlbedo.png":L"HandAlbedo.png"),std::ios::binary|std::ios::ate);if(!skin)return false;auto skinSize=skin.tellg();if(skinSize<8||skinSize>8*1024*1024)return false;skin.seekg(0);skinPNG.resize(static_cast<size_t>(skinSize));skin.read(reinterpret_cast<char*>(skinPNG.data()),skinSize);
    const unsigned char signature[]={137,80,78,71,13,10,26,10};if(!skin||std::memcmp(skinPNG.data(),signature,8))return false;
    geometry=std::move(candidate);savedFits=fitted;ready=true;return true;
}
inline unsigned section(handGrip::Pose pose,unsigned side){
    unsigned value=static_cast<unsigned>(pose);if(pose==handGrip::Pose::Support)value=6;if(pose==handGrip::Pose::Mechanism)value=4;
    return (std::min)(value,7u)*2+(side&1);
}
inline bool measuredSupport(int weapon){return nativeSource&&sections==64&&weapon>=2&&weapon<=(savedFits?5:4);}
inline unsigned supportSection(int weapon){return 36+static_cast<unsigned>(weapon)*4;}
inline bool canMorph(unsigned from,unsigned to){
    if(from>=sections||to>=sections||count[from]!=count[to])return false;
    // The appended banks use native support geometry, not the pistol topology.
    if(from>=36||to>=36)return from>=36&&to>=36&&(from-36)/4==(to-36)/4&&(from&1)==(to&1);
    return true;
}
inline XrPosef bind(const XrPosef& grip){
    using namespace DirectX;
    // Canonical hand: fingers -Z, back +Y, wrist lies 72 mm behind the grasp centre.
    auto world=XMMatrixTranslation(0,.032f,.072f)*motionTracking::rotation(grip.orientation)*XMMatrixTranslation(grip.position.x,grip.position.y,grip.position.z);
    XMFLOAT4 q;XMStoreFloat4(&q,XMQuaternionRotationMatrix(world));XMFLOAT4X4 m;XMStoreFloat4x4(&m,world);
    return {{q.x,q.y,q.z,q.w},{m._41,m._42,m._43}};
}
inline void statusFit(std::vector<wristband::Vertex>& mesh,unsigned side){
    for(auto& v:mesh){
        float x=v.x-.0018f,y=v.y+.0082f,z=(v.z-.003f)*.625f;
        float theta=std::atan2(y/wristband::fittedRadiusY,x/wristband::fittedRadiusX);
        if(!side)theta=3.14159265359f-theta;
        if(theta<0)theta+=6.28318530718f;
        float angle=theta*64/6.28318530718f;unsigned k=static_cast<unsigned>(angle)%64;float t=angle-std::floor(angle);
        float u=std::clamp((.350f-z-.336f)/.0035f,0.f,8.f);unsigned i=(std::min)(static_cast<unsigned>(u),7u);float f=u-i;
        float cy=bandCenters[i][0]*(1-f)+bandCenters[i+1][0]*f;
        float cz=bandCenters[i][1]*(1-f)+bandCenters[i+1][1]*f;
        float r0=bandRadial[i][k]*(1-t)+bandRadial[i][(k+1)%64]*t;
        float r1=bandRadial[i+1][k]*(1-t)+bandRadial[i+1][(k+1)%64]*t;
        float normalized=std::hypot(x/wristband::fittedRadiusX,y/wristband::fittedRadiusY);
        float offset=(normalized-1)*std::hypot(wristband::fittedRadiusX*std::cos(theta),wristband::fittedRadiusY*std::sin(theta));
        offset=-.00075f+(offset+.00075f)*.12f;
        float r=r0*(1-f)+r1*f+bandStandOff+offset;float sign=side?1.f:-1.f;
        v.x=sign*(cz+std::cos(theta)*r)*1.6f;v.y=(cy+.020f+std::sin(theta)*r)*1.6f;v.z=z*1.6f;
        v.nx=sign*std::cos(theta);v.ny=std::sin(theta);v.nz=0;
        if(nativeSource)nativeBandFit::map(v.x,v.y,v.z,side);
    }
}
}

