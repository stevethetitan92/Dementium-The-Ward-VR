#include "../src/selected_rifle_profile.hpp"
#include <cstdio>
#include <stdexcept>
#include <string>
void checked(bool b,int line){if(!b)throw std::runtime_error("rifle profile failed at line "+std::to_string(line));}
#define check(b) checked((b),__LINE__)
int main(int argc,char** argv){try{
 using namespace selectedRifle;using namespace DirectX;
 if(argc==2){psxAssets::Pack actual;check(psxAssets::load(argv[1],actual));check(selectedFrames::load(argv[1]));check(selectedRifle::load(argv[1],actual));
  check(selectedBallistics::ready[2]);const auto p=selectedBallistics::muzzle[2];
  check(std::fabs(p.x+.039f)<.0001f&&std::fabs(p.y-.09134037f)<.0001f&&std::fabs(p.z+.71980223f)<.0001f);
  // Rotate the real muzzle around both axes: it remains on the transformed bore,
  // never on the old controller ray at x=y=0.
  for(int i=0;i<24;++i){auto m=XMMatrixRotationRollPitchYaw(i*.17f,i*.31f,i*.13f)*XMMatrixTranslation(1,2,3);
   auto origin=XMVector3TransformCoord(XMVectorSet(p.x,p.y,p.z,1),m);
   auto bore=XMVector3TransformCoord(XMVectorSet(p.x,p.y,p.z+.3f,1),m);
   auto direction=XMVector3TransformNormal(XMVectorSet(0,0,-1,0),m);
   check(XMVectorGetX(XMVector3Length((origin-bore)-direction*.3f))<1e-5f);
  }
  auto saved=profile;profile.ready=false;check(!bindMuzzle(actual)&&!selectedBallistics::ready[2]);profile=saved;
  psxAssets::Pack empty;check(!bindMuzzle(empty)&&!selectedBallistics::ready[2]);
  check(bindMuzzle(actual));puts("PASS: actual M16 muzzle, rotated bore alignment and rejected unavailable geometry");}
 std::vector<unsigned char>b(100),g{1,2},f{3};unsigned h[]={0x46585044,1},ids[]={0,1,2,3};
 std::uint64_t hashes[]={selectedReload::fingerprint(g),selectedReload::fingerprint(f)};
 std::memcpy(b.data(),h,8);std::memcpy(b.data()+8,hashes,16);std::memcpy(b.data()+24,ids,16);
 float v[]={.1f,-.02f,0, 0,.06f,-.15f, 0,.11f,0, .04f,.06f,-.03f, 0,0,1};std::memcpy(b.data()+40,v,60);
 psxAssets::Pack pack;for(unsigned i=0;i<4;i++)pack.sections.push_back({2,0,i*3,3});
 check(decode(b,g,f,pack,profile));
 auto reject=[&](std::vector<unsigned char> bad){Profile p;check(!decode(bad,g,f,pack,p)&&!p.ready);};
 for(unsigned offset:{0u,4u,8u,16u}){auto bad=b;bad[offset]^=1;reject(bad);}
 auto bad=b;std::memcpy(bad.data()+28,ids,4);reject(bad);
 pack.sections[0].weapon=1;reject(b);pack.sections[0].weapon=2;
 pack.sections[0].kind=1;reject(b);pack.sections[0].kind=0;
 float wrong=-1;bad=b;std::memcpy(bad.data()+96,&wrong,4);reject(bad);
 manualReload::Visual visual;visual.valid=true;visual.leftInGun.orientation.w=1;visual.slidePull=.05f;
 XrPosef gun{};gun.orientation.w=1;XMMATRIX result;
 check(!transform(0,arsenal::Phase::Open,visual,XMMatrixIdentity(),gun,result));
 check(!transform(1,arsenal::Phase::Open,visual,XMMatrixIdentity(),gun,result));
 check(transform(2,arsenal::Phase::Open,visual,XMMatrixIdentity(),gun,result));
 check(transform(0,arsenal::Phase::Inserted,visual,XMMatrixIdentity(),gun,result));
 for(unsigned i:{2u,3u}){check(transform(i,arsenal::Phase::Charging,visual,XMMatrixIdentity(),gun,result));check(std::fabs(XMVectorGetZ(result.r[3])-.05f)<1e-6f);}
 check(transform(99,arsenal::Phase::Charging,visual,XMMatrixIdentity(),gun,result));check(XMVectorGetZ(result.r[3])==0);
 puts("PASS: rifle exact-asset roles, malformed rejection, magazine states and paired handle motion");return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}


