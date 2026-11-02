#pragma once
#include <DirectXMath.h>
#include <algorithm>
#include <cstdint>
namespace supportTransition {
using namespace DirectX;
class Blend {
    XMFLOAT4X4 previous{},offset{};bool ready{},attached{},moving{};int weapon{-1};std::uint64_t started{},last{};
public:
    void reset(){ready=moving=false;weapon=-1;}
    XMMATRIX update(const XMMATRIX& target,bool next,int id,std::uint64_t now,bool valid,bool reloadOwns){
        if(!valid){reset();return target;}
        if(!ready||id!=weapon||now<last||now-last>100||reloadOwns){
            ready=true;attached=next;weapon=id;moving=false;
        }else if(next!=attached){
            auto relative=XMLoadFloat4x4(&previous)*XMMatrixInverse(nullptr,target);
            const float distance=XMVectorGetX(XMVector3Length(relative.r[3]));
            // A tracking jump must not create a long hand flight.
            moving=distance<=.25f;
            if(moving){XMStoreFloat4x4(&offset,relative);started=now;}
            attached=next;
        }
        last=now;auto result=target;
        if(moving){float t=std::clamp(float(now-started)/100.f,0.f,1.f);t=t*t*(3-2*t);
            auto delta=XMLoadFloat4x4(&offset);auto q=XMQuaternionNormalize(XMQuaternionRotationMatrix(delta));
            auto eased=XMMatrixRotationQuaternion(XMQuaternionSlerp(q,XMQuaternionIdentity(),t));
            eased.r[3]=XMVectorSetW(XMVectorScale(delta.r[3],1-t),1);
            result=eased*target;if(t>=1)moving=false;
        }
        XMStoreFloat4x4(&previous,result);return result;
    }
};
inline Blend left;
}
