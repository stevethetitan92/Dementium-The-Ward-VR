#include "../src/pistol_rounds.hpp"
#include <cassert>
#include <cstdio>
int main(int argc,char** argv){
 assert(argc==2);std::filesystem::path folder(argv[1]);psxAssets::Pack pack;
 assert(psxAssets::load(folder,pack));assert(selectedFrames::load(folder));assert(selectedReload::loadProfile(folder,pack));
 assert(pistolRounds::load(folder));assert(!pistolRounds::cartridge.empty());
 const auto original=pack.vertices.size();pistolRounds::append(pack);
 assert(pack.vertices.size()==original+15*pistolRounds::cartridge.size());
 assert(pistolRounds::feedReady);
 using namespace DirectX;
 for(unsigned orientation=0;orientation<24;++orientation){
  auto world=XMLoadFloat4x4(&selectedFrames::frames[1].editorToAim)*XMMatrixRotationRollPitchYaw(.17f*orientation,.31f*orientation,.11f*orientation)*XMMatrixTranslation(.2f,1.3f,-.4f);
  auto start=pistolRounds::fedWorld(world,0);XMFLOAT4X4 a,b;XMStoreFloat4x4(&a,start);XMStoreFloat4x4(&b,world);
  for(unsigned k=0;k<16;++k)assert(std::fabs((&a._11)[k]-(&b._11)[k])<1e-5f);
  auto end=pistolRounds::fedWorld(world,1);XMStoreFloat4x4(&a,end);
  for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)assert(std::fabs(a.m[r][c]-b.m[r][c])<1e-5f);
 }
 printf("Feed offset: %.5f %.5f %.5f\n",pistolRounds::feedOffset.x,pistolRounds::feedOffset.y,pistolRounds::feedOffset.z);
 for(unsigned round=0;round<15;++round)for(size_t i=0;i<pistolRounds::cartridge.size();++i){
  const auto* src=pistolRounds::cartridge[i].value;const auto* dst=pack.vertices[original+round*pistolRounds::cartridge.size()+i].value;
  assert(src[11]==7);assert(!std::memcmp(src+6,dst+6,6*sizeof(float)));
 }
 puts("All 15 fitted cartridge instances preserve source color, UV and texture mode");
}
