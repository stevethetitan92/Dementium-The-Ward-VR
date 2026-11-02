#pragma once
#include "tracking_math.hpp"
namespace leftHandAlignment {
using namespace DirectX;
inline XMMATRIX basis(){
    // Measured anatomical axes of the current saved M16/free-left frame:
    // little-to-index, wrist-to-middle knuckle, and their cross product.
    return XMMatrixSet(0,.939692611f,.342020170f,0,
                      0,.342020170f,-.939692611f,0,
                      -1,0,0,0, 0,0,0,1);
}
inline XrQuaternionf calibrate(const XrQuaternionf& rawController,const XrQuaternionf& headYaw){
    // User holds palm down, fingers forward. Across points right, length
    // forward; anatomical cross points up (palm is the opposite side).
    auto target=XMMatrixSet(1,0,0,0, 0,0,-1,0, 0,1,0,0, 0,0,0,1);
    auto correction=XMMatrixTranspose(basis())*target*motionTracking::rotation(headYaw)*
                    XMMatrixTranspose(motionTracking::rotation(rawController));
    XMFLOAT4 q;XMStoreFloat4(&q,XMQuaternionRotationMatrix(correction));
    return {q.x,q.y,q.z,q.w};
}
inline XrPosef apply(XrPosef raw,const XrQuaternionf& correction){
    XMFLOAT4 q;XMStoreFloat4(&q,XMQuaternionRotationMatrix(
        motionTracking::rotation(correction)*motionTracking::rotation(raw.orientation)));
    raw.orientation={q.x,q.y,q.z,q.w};return raw;
}
inline XrQuaternionf thumbUp(const XrQuaternionf& correction){
    // Quarter-turn about wrist-to-knuckle direction: across moves toward
    // the previous anatomical normal while forward remains unchanged.
    auto turn=XMMatrixSet(0,0,1,0, 0,1,0,0, -1,0,0,0, 0,0,0,1);
    auto m=XMMatrixTranspose(basis())*turn*basis()*motionTracking::rotation(correction);
    XMFLOAT4 q;XMStoreFloat4(&q,XMQuaternionRotationMatrix(m));return {q.x,q.y,q.z,q.w};
}
}
