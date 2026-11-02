#include "../src/selected_shotgun_motion.hpp"
#include "../src/shotgun_pump_cursor.hpp"
#include <cstdio>
#include <stdexcept>
#define check(b) do { if(!(b)){printf("failed line %d\n",__LINE__);throw std::runtime_error("shotgun motion failed");} } while(0)
int main(int argc,char** argv){try{
 using namespace selectedShotgun;using namespace DirectX;
 check(argc==3);std::filesystem::path folder=argv[1];psxAssets::Pack pack;check(psxAssets::load(folder,pack));
 std::vector<unsigned char>b,g,f;check(psxAssets::read(argv[2],b,1024*1024));
 check(psxAssets::read(folder/"SelectedGeometry.psx",g,64*1024*1024));check(psxAssets::read(folder/"SelectedFrames.bin",f,4096));
 Motion motion;check(decode(b,g,f,pack,motion));check(motion.keys.size()==144&&motion.sections.size()==12);
 check(PumpCursor::validCycle(motion));PumpCursor cursor;
 for(unsigned i=0;i<100;i++)cursor.update(motion,1,.016f,true);
 check(!cursor.complete()&&cursor.time()>1.5f&&cursor.time()<1.8f);
 for(unsigned i=0;i<100;i++)cursor.update(motion,0,.016f,true);
 check(cursor.complete()&&std::fabs(cursor.time()-motion.keys.back().seconds)<1e-5f);
 auto reject=[&](std::vector<unsigned char> bad){Motion out;check(!decode(bad,g,f,pack,out)&&!out.ready);};
 for(unsigned offset:{0u,4u,8u,16u,24u}){auto bad=b;bad[offset]^=255;reject(bad);}
 auto bad=b;std::memcpy(bad.data()+36,bad.data()+32,4);reject(bad);
 bad=b;bad.pop_back();reject(bad);
 const size_t start=32+motion.sections.size()*4,stride=8+motion.sections.size()*64;
 bad=b;float invalid=0;std::memcpy(bad.data()+start+stride,&invalid,4);reject(bad);
 bad=b;invalid=2;std::memcpy(bad.data()+start+8,&invalid,4);reject(bad);
 for(unsigned k=0;k<motion.keys.size();k++)for(unsigned j=0;j<motion.sections.size();j++){
  XMFLOAT4X4 expected,actual;std::memcpy(&expected,b.data()+start+k*stride+8+j*64,64);
  XMStoreFloat4x4(&actual,sample(motion,motion.sections[j],motion.keys[k].seconds));
  for(unsigned r=0;r<4;r++)for(unsigned c=0;c<4;c++)check(std::fabs(actual.m[r][c]-expected.m[r][c])<2e-5f);
 }
 for(float t=0;t<3.05f;t+=.007f)for(auto section:motion.sections){
  auto m=sample(motion,section,t);check(std::fabs(XMVectorGetX(XMMatrixDeterminant(m))-1)<1e-4f);
  if(section==34){XMFLOAT4X4 a;XMStoreFloat4x4(&a,m);check(std::fabs(a._41)+std::fabs(a._42)+std::fabs(a._43)<1e-5f);}
 }
 puts("PASS: actual shotgun clip, exact keys, rigid interpolation, fixed chassis and malformed rejection");return 0;
 }catch(const std::exception& e){puts(e.what());return 1;}}

