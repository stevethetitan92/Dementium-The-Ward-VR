#pragma once
#include <cstdint>
#include <cstring>
namespace selectedWeaponDraw {
// Scope only the existing held-weapon render call. Keep that call running so
// particles, muzzle flashes and its other render work are not discarded.
inline thread_local bool replacing{};
// Replacement ownership persists through menus, tracking interruptions and
// door fades. A recent replacement draw is a visibility fact, not ownership.
inline bool ownsBody(bool packReady,unsigned weapon,bool vrActive){
    return packReady&&weapon<7&&vrActive;
}
struct Scope {bool old;explicit Scope(bool value):old(replacing){replacing=value;}~Scope(){replacing=old;}};
// Existing native shader contract: three matrices occupy 160 bytes, followed
// by uEnableAnimation. Unknown layouts fail open. The separate 128-byte
// particle/effect pipeline and non-animated geometry are always preserved.
inline bool body(unsigned constantBytes,const unsigned char* data,unsigned available){
    if(!replacing||constantBytes!=1200||!data||available<164)return false;
    std::uint32_t animated{};std::memcpy(&animated,data+160,4);
    return animated==1;
}
}
