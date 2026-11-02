#pragma once
#include <atomic>
#include <cstdint>
namespace geometryIsolation {
// Bounded diagnostics for item 2. Expiry always restores ordinary rendering.
enum Mode:unsigned {Normal,NativeWeaponPose,NoSceneExtras,NoHandClip,NoWeaponCache,NoFog,NoFrustum,NoSectorVisibility};
inline std::atomic<unsigned> selected{};inline std::atomic<std::uint64_t> until{};
inline std::atomic<std::uint64_t> replayUntil{};
inline bool replay(std::uint64_t now){return now<replayUntil.load();}
inline Mode mode(std::uint64_t now){auto end=until.load();return now<end?static_cast<Mode>(selected.load()):Normal;}
inline void set(Mode value,std::uint64_t now){selected=value;until=value==Normal?0:now+45000;}
}
