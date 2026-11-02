#include "../src/rifle_rounds.hpp"
#include <cassert>
#include <cstdio>
int main(int argc,char**argv){
 assert(argc==2);psxAssets::Pack p;assert(psxAssets::load(argv[1],p));assert(selectedFrames::load(argv[1]));assert(selectedRifle::load(argv[1],p));
 const auto original=p;const auto before=selectedRifle::profile;
 auto& bad=p.vertices[p.sections[before.sections[0]].first];bad.value[9]+=.001f;
 assert(!selectedRifle::fitMagazine(argv[1],p));assert(selectedRifle::profile.well.z==before.well.z);
 p=original;assert(selectedRifle::fitMagazine(argv[1],p));
 using namespace DirectX;auto frame=XMLoadFloat4x4(&selectedFrames::frames[2].editorToAim);
 for(unsigned i=0;i<p.vertices.size();++i){bool moved=false;for(unsigned k=0;k<2;++k){auto s=p.sections[before.sections[k]];moved|=i>=s.first&&i<s.first+s.count;}
  const auto* a=original.vertices[i].value;const auto* b=p.vertices[i].value;
  if(!moved){assert(std::memcmp(a,b,sizeof(psxAssets::Vertex))==0);continue;}
  assert(std::memcmp(a+3,b+3,9*sizeof(float))==0);
  auto delta=XMVector3TransformNormal(XMVectorSet(b[0]-a[0],b[1]-a[1],b[2]-a[2],0),frame);
  assert(XMVectorGetX(XMVector3Length(delta-XMVectorSet(0,0,-.001f,0)))<1e-7f);
 }
 assert(std::fabs(selectedRifle::profile.well.z-before.well.z+.001f)<1e-7f);
 const auto fitted=p;assert(!selectedRifle::fitMagazine(argv[1],p)); // No double shift.
 assert(std::memcmp(p.vertices.data(),fitted.vertices.data(),p.vertices.size()*sizeof(psxAssets::Vertex))==0);
 assert(rifleRounds::append(p));auto feed=rifleRounds::feedOffset;
 p=original;selectedRifle::profile=before;assert(rifleRounds::append(p));
 assert(std::fabs(feed.z-rifleRounds::feedOffset.z-.001f)<1e-6f); // Same chamber endpoint.
 puts("PASS: audited magazine/cartridge rigid fit, unchanged source attributes and other meshes, rejection, idempotence, chamber endpoint");
}
