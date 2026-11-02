#pragma once
#include <dxgi1_6.h>

namespace mirrorPresentPolicy {
// Affects only the native desktop swapchain. OpenXR wait/begin/end and eye
// submission remain unchanged. Keep opt-in until matched runtime validation.
inline UINT flags(bool enabled, bool stereoHealthy, bool platformSupported,
                  bool windowed, UINT creationFlags, UINT sync, UINT original) {
    if (!enabled || !stereoHealthy || !platformSupported || !windowed || sync != 0 ||
        !(creationFlags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) ||
        (original & DXGI_PRESENT_TEST)) return original;
    return original | DXGI_PRESENT_ALLOW_TEARING;
}
}
