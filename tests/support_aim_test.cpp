#include "../src/support_aim.hpp"
#include <cstdio>
#include <stdexcept>
static void require(bool v){if(!v)throw std::runtime_error("two-hand support aiming failed");}
static float gap(XrVector3f a,XrVector3f b){return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));}
static XrVector3f world(const XrPosef& p,XrVector3f local){
    using namespace DirectX;XMFLOAT3 v;XMStoreFloat3(&v,XMVector3TransformNormal(XMVectorSet(local.x,local.y,local.z,0),motionTracking::rotation(p.orientation)));
    return {v.x+p.position.x,v.y+p.position.y,v.z+p.position.z};
}
int main(){try{
    using namespace DirectX;unsigned verified=0;
    for(const auto& fit:measuredSupport::fits)for(unsigned step=0;step<24;step++){
        XMFLOAT4 q;XMStoreFloat4(&q,XMQuaternionRotationRollPitchYaw(.03f*step,.09f*step,-.02f*step));
        XrPosef primary{{q.x,q.y,q.z,q.w},{1.2f,-.3f,.7f}},left{{0,0,0,1},world(primary,fit.contact)};
        supportAim::Controller controller;auto out=controller.update(primary,left,fit.weapon,true,true);require(controller.active());
        require(gap(out.position,primary.position)<1e-7f);require(gap(world(out,fit.contact),left.position)<.00001f);
        require(gap(controller.pumpRest(),fit.contact)<.00001f);
        // A moving support hand must steer the same measured contact vector.
        auto delta=XMVectorSet(left.position.x-primary.position.x,left.position.y-primary.position.y,left.position.z-primary.position.z,0);
        delta=XMVector3TransformNormal(delta,XMMatrixRotationY(.38f));XMFLOAT3 p;XMStoreFloat3(&p,delta);
        left.position={p.x+primary.position.x,p.y+primary.position.y,p.z+primary.position.z};
        out=controller.update(primary,left,fit.weapon,true,true);require(controller.active());
        require(gap(controller.pumpRest(),fit.contact)<.00001f); // motion cannot move the pump's rest point
        require(gap(out.position,primary.position)<1e-7f);require(gap(world(out,fit.contact),left.position)<.00001f);
        out=controller.update(primary,left,fit.weapon,false,true);require(!controller.active());
        require(gap(world(out,fit.contact),world(primary,fit.contact))<.00001f);
        left.position=world(primary,fit.contact);controller.update(primary,left,fit.weapon,true,true);require(controller.active());
        controller.update(primary,left,fit.weapon,true,false);require(!controller.active());
        controller.update(primary,left,fit.weapon,true,true);require(controller.active()); // return to valid nearby support while grip stays held
        require(gap(controller.pumpRest(),fit.contact)<.00001f);
        controller.update(primary,left,fit.weapon,false,true);controller.update(primary,left,fit.weapon,true,true);require(controller.active());
        ++verified;
    }
    supportAim::Controller c;XrPosef primary{{0,0,0,1},{0,0,0}},far{{0,0,0,1},{0,0,-.7f}};
    c.update(primary,far,2,true,true);require(!c.active());
    far.position=measuredSupport::fits[0].contact;c.update(primary,far,2,true,true);require(c.active());
    far.position={0,0,0};c.update(primary,far,2,true,true);require(!c.active());
    c.update(primary,far,0,false,true);c.update(primary,far,0,true,true);require(!c.active());
    std::printf("PASS: %u measured weapon/orientation cases, rigid primary pivot, support-driven aim, release, tracking/reload cancellation and reach gates.\n",verified);return 0;
}catch(const std::exception& e){std::puts(e.what());return 1;}}
