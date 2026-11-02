#pragma once
#include "tracking_math.hpp"
#include "psx_asset_pack.hpp"
#include <array>
#include <cstring>
namespace selectedFrames {
using namespace DirectX;
struct Frame { std::uint32_t support; XMFLOAT4X4 editorToAim,wristToEditor,editorToFree; XrVector3f contact; };
static_assert(sizeof(Frame)==208);
inline std::array<Frame,7> frames{};
inline bool ready{};
inline bool rigid(const XMFLOAT4X4& f){
    for(const auto v:std::array<float,16>{f._11,f._12,f._13,f._14,f._21,f._22,f._23,f._24,f._31,f._32,f._33,f._34,f._41,f._42,f._43,f._44})if(!std::isfinite(v))return false;
    if(std::fabs(f._14)+std::fabs(f._24)+std::fabs(f._34)>1e-5f||std::fabs(f._44-1)>1e-5f)return false;
    auto m=XMLoadFloat4x4(&f);m.r[3]=XMVectorSet(0,0,0,1);
    XMFLOAT4X4 unit;XMStoreFloat4x4(&unit,m*XMMatrixTranspose(m));
    const float* p=&unit._11;for(unsigned i=0;i<16;i++)if(std::fabs(p[i]-(i%5==0?1.f:0.f))>1e-4f)return false;
    return XMVectorGetX(XMMatrixDeterminant(m))>.999f&&std::fabs(f._41)<3&&std::fabs(f._42)<3&&std::fabs(f._43)<3;
}
inline bool load(const std::filesystem::path& folder){
    std::vector<unsigned char> bytes;if(!psxAssets::read(folder/L"SelectedFrames.bin",bytes,4096)||bytes.size()!=12+sizeof(frames))return false;
    std::uint32_t h[3];std::memcpy(h,bytes.data(),12);if(h[0]!=0x46585044||h[1]!=1||h[2]!=7)return false;
    std::array<Frame,7> candidate;std::memcpy(candidate.data(),bytes.data()+12,sizeof(candidate));
    for(unsigned i=0;i<7;i++){const auto& f=candidate[i];
        if(f.support!=unsigned(i>=2&&i<=5)||!rigid(f.editorToAim)||!rigid(f.wristToEditor)||!rigid(f.editorToFree))return false;
        const auto& c=f.contact;if(!std::isfinite(c.x)||!std::isfinite(c.y)||!std::isfinite(c.z))return false;
        const float length=c.x*c.x+c.y*c.y+c.z*c.z;if(f.support&&(length<.0064f||length>1.44f))return false;
    }
    frames=candidate;ready=true;return true;
}
inline const XrVector3f* contact(int weapon){return ready&&weapon>=0&&weapon<7&&frames[weapon].support?&frames[weapon].contact:nullptr;}
inline XMMATRIX pose(const XrPosef& p){return motionTracking::rotation(p.orientation)*XMMatrixTranslation(p.position.x,p.position.y,p.position.z);}
inline XMMATRIX weapon(int id,const XrPosef& p){return XMLoadFloat4x4(&frames[id].editorToAim)*pose(p);}
inline XMMATRIX freeHand(int id,const XrPosef& p){return XMLoadFloat4x4(&frames[id].editorToFree)*pose(p);}
inline XMMATRIX wrist(int id,const XMMATRIX& editorToWorld){return XMLoadFloat4x4(&frames[id].wristToEditor)*editorToWorld;}
}
