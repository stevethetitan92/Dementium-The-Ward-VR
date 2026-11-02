#pragma once
// Synthetic desktop-only viewpoints. These do not move the native player or
// establish physical headset/controller acceptance.
namespace desktopTestPose {
struct Pose { float yawY{}, pitchX{}, w{1}, x{}, y{}, z{}; };
inline Pose make(unsigned mode) {
    Pose p;
    if(mode==1){p.yawY=.5f;p.w=.8660254f;}
    if(mode==2||mode==4||mode==5){p.pitchX=-.5f;p.w=.8660254f;}
    if(mode==3)p.x=.30f;
    // Preserve the look-down orientation while comparing a near and far view.
    // OpenXR forward is -Z. Translation is in the recentered tracking space.
    if(mode==4){p.y=-.35f;p.z=-.45f;}
    if(mode==5){p.y=-.35f;p.z=.30f;}
    return p;
}
}
