#include "../src/selected_speedloader.hpp"
#include <cstdio>
#include <stdexcept>
void check(bool v){if(!v)throw std::runtime_error("revolver asset repair failed");}
int main(int argc,char** argv){try{
 check(argc==2);std::filesystem::path folder(argv[1]);psxAssets::Pack pack;
 check(psxAssets::load(folder,pack));check(selectedFrames::load(folder));check(selectedRevolver::load(folder,pack));
 auto originalVertices=pack.vertices.size(),originalSections=pack.sections.size();
 check(selectedSpeedloader::load(folder,pack));check(pack.sections.size()==originalSections+1&&pack.vertices.size()==originalVertices+4704);
 using namespace DirectX;
 auto& p=selectedRevolver::profile;
 for(unsigned j=1;j<8;++j)check(!std::memcmp(&p.parts[0].open,&p.parts[j].open,sizeof p.parts[0].open));
 check(selectedRevolver::alignBarrel(pack));
 auto matrix=XMLoadFloat4x4(&selectedFrames::frames[0].editorToAim);
 auto axis=XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(.9625261f,-.26565492f,-.0545067f,0),matrix));
 check(XMVectorGetZ(axis)<-.9999f); // Independent fitted cartridge long-axis measurement.
 auto once=selectedFrames::frames[0].editorToAim;check(selectedRevolver::alignBarrel(pack));
 check(!std::memcmp(&once,&selectedFrames::frames[0].editorToAim,sizeof once));
 auto spin=selectedRevolver::spinTransform(p.parts[1].section,XM_PIDIV2);
 auto corrected=XMLoadFloat4x4(&selectedFrames::frames[0].editorToAim);
 auto up=XMVector3TransformNormal(XMVectorSet(0,1,0,0),XMMatrixInverse(nullptr,corrected));
 auto turned=XMVector3Normalize(XMVector3TransformNormal(up,spin*corrected));
 check(XMVectorGetX(turned)>.9999f); // upper chamber moves right, clockwise from shooter
 auto pivot=XMLoadFloat3(&selectedRevolver::spinPivot);
 check(XMVectorGetX(XMVector3Length(XMVector3TransformCoord(pivot,spin)-pivot))<1e-5f);
 for(auto id:p.rounds){auto roundSpin=selectedRevolver::spinTransform(id,XM_PIDIV2);XMFLOAT4X4 a,b;XMStoreFloat4x4(&a,spin);XMStoreFloat4x4(&b,roundSpin);check(!std::memcmp(&a,&b,sizeof a));}
 XMFLOAT4X4 extractor,identity;XMStoreFloat4x4(&extractor,selectedRevolver::spinTransform(p.parts[0].section,XM_PIDIV2));XMStoreFloat4x4(&identity,XMMatrixIdentity());check(!std::memcmp(&extractor,&identity,sizeof identity));
 puts("PASS: actual asset loader, eight-part hinge, source loader append, barrel alignment and idempotence");return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}
