#pragma once
#include <cstdint>
#include <cstring>
#include <cmath>

namespace nativeMapTransition {
// Retail UI request and update sites establish +60 as an absolute per-update
// fade step. Never skip the deferred screen lifecycle or overwrite its pointers.
inline constexpr unsigned char requestGuard[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x8b,0xfa};
inline constexpr unsigned char stepGuard[]={0xf3,0x0f,0x10,0x5f,0x60,0x0f,0x2f,0xf3,0x76,0x07};
inline constexpr unsigned char targetGuard[]={0xf3,0x0f,0x10,0x43,0x58,0xc7,0x43,0x5c,0x00,0x00,0x80,0xbf};
inline bool supported(const unsigned char* image){
    return image&&!std::memcmp(image+0xbbcb0,requestGuard,sizeof requestGuard)
        &&!std::memcmp(image+0xbb837,stepGuard,sizeof stepGuard)
        &&!std::memcmp(image+0xbbd72,targetGuard,sizeof targetGuard);
}
template<class T> inline T read(const unsigned char* root,unsigned offset){T v;std::memcpy(&v,root+offset,sizeof v);return v;}
inline bool shorten(unsigned char* root,bool verified){
    if(!root||!verified)return false;
    const auto address=reinterpret_cast<std::uintptr_t>(root);
    if(read<std::uintptr_t>(root,8)!=address+0x15a8||read<std::uintptr_t>(root,0x10)!=address+0xe0
        ||root[0x18]!=1||root[0x64]!=1||root[0x68]!=0||(read<std::uint16_t>(root,0x1a)&1))return false;
    const float current=read<float>(root,0x58),target=read<float>(root,0x5c),step=read<float>(root,0x60);
    if(!std::isfinite(current)||current< -1.f||current>0.f||target!= -1.f
        ||!std::isfinite(step)||step<=0.f||step>=.5f)return false;
    // At most two fade updates for the full 0 -> -1 range, followed by the
    // original deferred transition. Other menus, loading and cutscenes untouched.
    const float faster=.5f;std::memcpy(root+0x60,&faster,sizeof faster);return true;
}
}
