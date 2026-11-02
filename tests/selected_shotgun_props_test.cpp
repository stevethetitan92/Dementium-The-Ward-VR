#include "../src/selected_shotgun_props.hpp"
#include <cstdio>
#include <stdexcept>
void check(bool b){if(!b)throw std::runtime_error("shotgun props failed");}
int main(int argc,char**argv){try{
 using namespace selectedShotgun;
 std::vector<unsigned char>b(32),g{1,2},f{3};unsigned h[]={0x43585044,1},ids[]={0,1};
 std::uint64_t hashes[]={selectedReload::fingerprint(g),selectedReload::fingerprint(f)};
 std::memcpy(b.data(),h,8);std::memcpy(b.data()+8,hashes,16);std::memcpy(b.data()+24,ids,8);
 psxAssets::Pack pack;pack.sections.push_back({3,0,0,3});pack.sections.push_back({3,0,3,3});
 check(propVisible(0));check(decodeProps(b,g,f,pack,props));check(!propVisible(0)&&!propVisible(1)&&propVisible(2));
 auto reject=[&](std::vector<unsigned char> bad){Props p;check(!decodeProps(bad,g,f,pack,p)&&!p.ready);};
 for(unsigned offset:{0u,4u,8u,16u}){auto bad=b;bad[offset]^=1;reject(bad);}
 auto bad=b;std::memcpy(bad.data()+28,ids,4);reject(bad);bad.pop_back();reject(bad);
 pack.sections[0].kind=1;reject(b);pack.sections[0].kind=0;pack.sections[0].weapon=2;reject(b);
 if(argc==2){psxAssets::Pack actual;check(psxAssets::load(argv[1],actual));check(loadProps(argv[1],actual));for(unsigned i=0;i<actual.sections.size();++i)check(propVisible(i)==(i!=46&&i!=47));}
 puts("PASS: exact-asset cartridge visibility, unrelated parts preserved, malformed roles rejected");return 0;
 }catch(const std::exception&e){puts(e.what());return 1;}}
