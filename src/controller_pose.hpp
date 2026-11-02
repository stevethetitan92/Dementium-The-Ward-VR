#pragma once
#include "tracking_math.hpp"
namespace vrControls {
struct PoseContinuity {
    XrPosef pose{{0,0,0,1},{0,0,0}};
    bool have{},live{};
    bool update(const XrSpaceLocation& location){
        constexpr auto required=XR_SPACE_LOCATION_ORIENTATION_VALID_BIT|XR_SPACE_LOCATION_POSITION_VALID_BIT;
        live=(location.locationFlags&required)==required&&motionTracking::validPose(location.pose);
        if(live){pose=location.pose;have=true;}
        return live;
    }
    void rebase(const XrPosef& change){if(have&&!live)pose=motionTracking::relativePose(change,pose);}
};
}
