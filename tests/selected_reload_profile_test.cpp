#include "../src/selected_reload_profile.hpp"
#include <cstdio>
#include <stdexcept>
#include <limits>
void check(bool v){if(!v)throw std::runtime_error("reload profile validation failed");}
int main(int argc,char** argv){try{
 using namespace selectedReload;
 if(argc==2){psxAssets::Pack actual;check(psxAssets::load(argv[1],actual));check(loadProfile(argv[1],actual)&&profile.ready);puts("PASS: staged package and reload profile loaded together");}
 std::vector<unsigned char> geometry{1,2,3},frames{4,5,6},bytes(100);
 unsigned h[]={0x52585044,1};std::memcpy(bytes.data(),h,8);
 std::uint64_t hashes[]={fingerprint(geometry),fingerprint(frames)};std::memcpy(bytes.data()+8,hashes,16);
 unsigned ids[]={0,1,2,3,4,5,6};std::memcpy(bytes.data()+24,ids,28);
 float values[12]={0,0,0,0,0,0,0,.1f,0,0,0,1};std::memcpy(bytes.data()+52,values,48);
 psxAssets::Pack pack;for(unsigned i=0;i<7;i++)pack.sections.push_back({1,0,i*3,3});
 Profile result;check(decodeProfile(bytes,geometry,frames,pack,result));
 check(result.ready&&result.role(0)==Role::Magazine&&result.role(6)==Role::Slide&&result.role(7)==Role::Fixed);
 auto reject=[&](std::vector<unsigned char> bad){Profile untouched;check(!decodeProfile(bad,geometry,frames,pack,untouched)&&!untouched.ready);};
 for(unsigned offset:{0u,4u,8u,16u}){auto bad=bytes;bad[offset]^=1;reject(bad);}
 auto bad=bytes;bad.pop_back();reject(bad);bad=bytes;bad.push_back(0);reject(bad);
 bad=bytes;unsigned duplicate=0;std::memcpy(bad.data()+28,&duplicate,4);reject(bad);
 bad=bytes;unsigned beyond=7;std::memcpy(bad.data()+24,&beyond,4);reject(bad);
 bad=bytes;float nan=std::numeric_limits<float>::quiet_NaN();std::memcpy(bad.data()+52,&nan,4);reject(bad);
 bad=bytes;float zero=0;std::memcpy(bad.data()+96,&zero,4);reject(bad);
 pack.sections[0].kind=1;reject(bytes);pack.sections[0].kind=0;
 pack.sections[1].weapon=3;reject(bytes);pack.sections[1].weapon=1;
 geometry[0]^=1;reject(bytes);geometry[0]^=1;frames[0]^=1;reject(bytes);
 frames[0]^=1;auto v2=bytes;v2.resize(112);unsigned version=2;std::memcpy(v2.data()+4,&version,4);
 float contact[]={.0476f,.0644f,-.0366f};std::memcpy(v2.data()+100,contact,12);
 check(decodeProfile(v2,geometry,frames,pack,result));check(result.geometry.heldContactGrip.x==contact[0]);
 std::memcpy(v2.data()+100,&nan,4);reject(v2);
 puts("PASS: exact asset identity, unique pistol-only sections, finite anchors, unit axis and malformed profile rejection");return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}
