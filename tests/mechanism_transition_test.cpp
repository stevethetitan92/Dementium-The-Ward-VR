#include "../src/mechanism_transition.hpp"
#include "../src/selected_revolver_profile.hpp"
#include <cstdio>
#include <stdexcept>
void check(bool v){if(!v)throw std::runtime_error("mechanism transition failed");}
int main(){try{
 mechanismMotion::Transition c;check(c.update(false,100)==0);check(c.update(true,200)==0);
 check(std::fabs(c.update(true,290)-.5f)<1e-5f);check(c.update(true,380)==1);
 check(c.update(false,400)==1);check(std::fabs(c.update(false,510)-.5f)<1e-5f);
 check(std::fabs(c.update(true,510)-.5f)<1e-5f);check(c.update(true,690)==1);
 check(c.update(false,700)==1&&c.update(false,920)==0);
 using namespace DirectX;selectedRevolver::profile.ready=true;
 auto& p=selectedRevolver::profile.parts[0];p.section=42;
 XMStoreFloat4x4(&p.open,XMMatrixRotationZ(.8f)*XMMatrixTranslation(.04f,.03f,.01f));
 auto halfway=selectedRevolver::animatedTransform(42,.5f,XMMatrixIdentity());
 check(std::fabs(XMVectorGetX(XMMatrixDeterminant(halfway))-1)<1e-5f);
 // A point on the authored hinge stays fixed throughout opening and closing.
 auto pivot=XMVectorSet(.04f,.03f,.01f,1);
 XMStoreFloat4x4(&p.open,XMMatrixTranslationFromVector(-pivot)*XMMatrixRotationZ(1.2f)*XMMatrixTranslationFromVector(pivot));
 for(int i=0;i<=100;++i){
  auto pose=selectedRevolver::animatedTransform(42,i/100.f,XMMatrixIdentity());
  check(XMVectorGetX(XMVector3Length(XMVector3TransformCoord(pivot,pose)-pivot))<1e-5f);
 }
 auto endpoint=selectedRevolver::animatedTransform(42,1,XMMatrixIdentity());
 for(int r=0;r<4;++r)check(XMVectorGetX(XMVector4Length(endpoint.r[r]-XMLoadFloat4x4(&p.open).r[r]))<1e-5f);
 check(XMVectorGetX(selectedRevolver::animatedTransform(99,.5f,XMMatrixIdentity()).r[3])==0);
 puts("PASS: cylinder endpoints, smooth reversal, rigid interpolation and unchanged fixed parts");return 0;
}catch(const std::exception&e){puts(e.what());return 1;}}
