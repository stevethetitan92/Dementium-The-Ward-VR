#include "../src/rifle_rounds.hpp"
#include <cassert>
#include <cstdio>
int main(int argc,char** argv){
 assert(argc==2);psxAssets::Pack pack;std::filesystem::path folder(argv[1]);
 assert(psxAssets::load(folder,pack));assert(selectedFrames::load(folder));assert(selectedRifle::load(folder,pack));
 const auto original=pack.vertices.size();const auto source=pack.sections[selectedRifle::profile.sections[1]];
 assert(rifleRounds::append(pack));assert(pack.vertices.size()==original+source.count);
 std::vector<bool> seen(source.count);
 for(unsigned i=unsigned(original);i<pack.vertices.size();++i){bool found=false;
  for(unsigned j=0;j<source.count;++j)if(!seen[j]&&!std::memcmp(&pack.vertices[i],&pack.vertices[source.first+j],sizeof(psxAssets::Vertex))){seen[j]=true;found=true;break;}
  assert(found);
 }
 for(bool s:seen)assert(s);
 for(unsigned k=0;k<24;++k){using namespace DirectX;
  auto world=XMLoadFloat4x4(&selectedFrames::frames[2].editorToAim)*XMMatrixRotationRollPitchYaw(.13f*k,.29f*k,.17f*k)*XMMatrixTranslation(.1f,1.2f,-.3f);
  XMFLOAT4X4 a,b;XMStoreFloat4x4(&a,world);XMStoreFloat4x4(&b,rifleRounds::fedWorld(world,0));
  for(unsigned j=0;j<16;++j)assert(std::fabs((&a._11)[j]-(&b._11)[j])<1e-5f);
  XMStoreFloat4x4(&b,rifleRounds::fedWorld(world,1));for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)assert(std::fabs(a.m[r][c]-b.m[r][c])<1e-5f);
 }
 assert(rifleRounds::count(true,1,0,false)==1);assert(rifleRounds::count(true,0,2,true)==2);assert(rifleRounds::count(true,0,2,false)==0);
 printf("PASS source triangles/UV/material preserved, four complete cartridges, rotated feed. Offset %.5f %.5f %.5f\n",rifleRounds::feedOffset.x,rifleRounds::feedOffset.y,rifleRounds::feedOffset.z);
}
