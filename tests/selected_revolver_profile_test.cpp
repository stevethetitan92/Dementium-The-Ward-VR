#include "../src/selected_revolver_profile.hpp"
#include <stdexcept>
#include <cstdio>
#include <limits>
void check(bool v){if(!v)throw std::runtime_error("revolver profile check failed");}
int main(int argc,char** argv){try{
 using namespace selectedRevolver;using namespace DirectX;
 if(argc==2){psxAssets::Pack actual;check(psxAssets::load(argv[1],actual));check(load(argv[1],actual));puts("PASS: actual staged revolver profile loaded");}
 std::vector<unsigned char> b(572),g{1,2},f{3,4};unsigned h[2]={0x43585044,1},count=8;
 std::uint64_t hashes[2]={selectedReload::fingerprint(g),selectedReload::fingerprint(f)};
 std::memcpy(b.data(),h,8);std::memcpy(b.data()+8,hashes,16);std::memcpy(b.data()+24,&count,4);
 psxAssets::Pack pack;
 for(unsigned i=0;i<8;i++){
  pack.sections.push_back({0,0,i*3,3});XMFLOAT4X4 m;
  XMStoreFloat4x4(&m,XMMatrixRotationX(.5f)*XMMatrixTranslation(0,.02f,-.03f));
  std::memcpy(b.data()+28+i*68,&i,4);std::memcpy(b.data()+32+i*68,&m,64);
 }
 check(decode(b,g,f,pack,profile)&&profile.ready);
 auto reject=[&](std::vector<unsigned char> bad){Profile p;check(!decode(bad,g,f,pack,p)&&!p.ready);};
 for(unsigned offset:{0u,4u,8u,16u,24u}){auto bad=b;bad[offset]^=1;reject(bad);}
 auto bad=b;bad.pop_back();reject(bad);
 unsigned zero=0;bad=b;std::memcpy(bad.data()+96,&zero,4);reject(bad);
 unsigned beyond=8;bad=b;std::memcpy(bad.data()+28,&beyond,4);reject(bad);
 float nan=std::numeric_limits<float>::quiet_NaN();bad=b;std::memcpy(bad.data()+32,&nan,4);reject(bad);
 float scale=1.1f;bad=b;std::memcpy(bad.data()+32,&scale,4);reject(bad);
 float reflection=-1;bad=b;std::memcpy(bad.data()+32,&reflection,4);reject(bad);
 pack.sections[0].weapon=1;reject(b);pack.sections[0].weapon=0;pack.sections[0].kind=1;reject(b);pack.sections[0].kind=0;
 auto weapon=XMMatrixRotationZ(.4f)*XMMatrixTranslation(1,2,3);
 auto same=[&](XMMATRIX a,XMMATRIX bb){XMFLOAT4X4 x,y;XMStoreFloat4x4(&x,a);XMStoreFloat4x4(&y,bb);for(unsigned r=0;r<4;r++)for(unsigned c=0;c<4;c++)check(std::fabs(x.m[r][c]-y.m[r][c])<1e-5f);};
 same(transform(0,false,weapon),weapon);same(transform(99,true,weapon),weapon);
 same(transform(0,true,weapon),XMLoadFloat4x4(&profile.parts[0].open)*weapon);
 auto v2=b;v2.resize(596);unsigned version=2;std::memcpy(v2.data()+4,&version,4);
 float contacts[]={.03f,.02f,-.1f,.04f,.06f,-.03f};std::memcpy(v2.data()+572,contacts,24);
 Profile calibrated;check(decode(v2,g,f,pack,calibrated)&&calibrated.contact.ready);
 XrPosef hand{};hand.orientation={0,0,.70710678f,.70710678f};hand.position={.1f,.2f,.3f};
 auto point=calibrated.contact.contact(hand);check(std::fabs(point.x-.04f)<1e-5f&&std::fabs(point.y-.24f)<1e-5f&&std::fabs(point.z-.27f)<1e-5f);
 auto v3=v2;v3.resize(620);version=3;std::memcpy(v3.data()+4,&version,4);
 unsigned rounds[]={2,3,4,5,6,7};std::memcpy(v3.data()+596,rounds,24);
 check(decode(v3,g,f,pack,profile)&&profile.roundsBound);
 for(int loaded=0;loaded<=6;loaded++){
  int visibleCount=0;for(unsigned section:rounds)visibleCount+=visible(section,true,loaded)?1:0;
  check(visibleCount==loaded&&visible(0,true,loaded)&&visible(1,true,loaded)&&visible(99,true,loaded));
 }
 for(unsigned section:rounds)check(visible(section,false,0)&&visible(section,true,-1)&&visible(section,true,7));
 bad=v3;std::memcpy(bad.data()+600,rounds,4);reject(bad);
 bad=v3;std::memcpy(bad.data()+596,&beyond,4);reject(bad);
 check(decode(v2,g,f,pack,profile)&&!profile.roundsBound&&visible(7,true,0));
 std::memcpy(v2.data()+572,&nan,4);reject(v2);
 puts("PASS: exact assets, revolver-only unique sections, rigid affine transforms, closed/fixed preservation");return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}
