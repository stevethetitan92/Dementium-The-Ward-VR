#include "../src/casing_flight.hpp"
#include "../src/casing_policy.hpp"
#include <cassert>
#include <cstdio>
using namespace DirectX;
int main(){
 casingFlight::Flight flight;flight.emit(1,XMMatrixIdentity(),1,100,1);assert(flight.size()==1);
 XMFLOAT3 a{},b{};
 flight.visit(200,XMMatrixIdentity(),[&](unsigned w,float,const XMMATRIX& m){assert(w==1);XMStoreFloat3(&a,m.r[3]);});
 flight.visit(200,XMMatrixTranslation(-2,0,0),[&](unsigned,float,const XMMATRIX& m){XMStoreFloat3(&b,m.r[3]);});
 assert(std::fabs(b.x-a.x+2)<1e-5f&&std::fabs(b.y-a.y)<1e-5f); // room-fixed, not camera-fixed
 float previousY=a.y;
 flight.visit(700,XMMatrixIdentity(),[&](unsigned,float,const XMMATRIX& m){XMStoreFloat3(&a,m.r[3]);});assert(a.y<previousY);
 flight.visit(1901,XMMatrixIdentity(),[](unsigned,float,const XMMATRIX&){});assert(!flight.size());
 for(int i=0;i<100;++i)flight.emit(2,XMMatrixIdentity(),1,2000,1);assert(flight.size()==64);
 flight.visit(1999,XMMatrixIdentity(),[](unsigned,float,const XMMATRIX&){});assert(!flight.size());
 casingPolicy::Ejection e;assert(!e.update(0,0,false,false,true));assert(!e.update(0,1,false,false,true));assert(!e.update(0,2,false,false,true));
 assert(!e.update(0,2,true,false,true));assert(!e.update(0,2,true,false,true));
 assert(!e.update(0,8,false,false,true));assert(!e.update(0,8,true,false,true));
 // A captured port follows the weapon at emission, then remains world-fixed.
 flight.clear();casingFlight::portReady[2]=true;casingFlight::ports[2]={.03f,.04f,-.18f};
 auto gun=XMMatrixRotationY(.8f)*XMMatrixTranslation(1,2,3);
 flight.emit(2,gun,1,3000,1);XMFLOAT3 expected;
 XMStoreFloat3(&expected,XMVector3TransformCoord(XMVectorSet(.03f,.04f,-.18f,1),gun));
 flight.visit(3000,XMMatrixIdentity(),[&](unsigned w,float,const XMMATRIX& m){
  assert(w==2);XMFLOAT3 actual;XMStoreFloat3(&actual,m.r[3]);
  assert(std::fabs(actual.x-expected.x)<1e-5f&&std::fabs(actual.y-expected.y)<1e-5f&&std::fabs(actual.z-expected.z)<1e-5f);
  auto base=XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0,0,-1,0),m));
  auto rear=XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0,0,1,0),gun));
  assert(XMVectorGetX(XMVector3Dot(base,rear))>.99999f);
 });
 assert(!e.update(3,0,false,false,true));assert(!e.update(3,1,false,false,true));assert(e.update(3,1,false,true,true)==1);assert(!e.update(3,1,false,true,true));
 puts("PASS: world-fixed casing flight, gravity, lifetime, bounded count, clock rollback and mechanism-specific ejection");
}
