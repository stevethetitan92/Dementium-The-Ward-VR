#include "../src/selected_wrist_finish.hpp"
#include "../src/selected_wrist_fade.hpp"
#include <cassert>
#include <cstdio>
int main(int argc,char** argv){
 assert(argc==2);psxAssets::Pack pack;assert(psxAssets::load(argv[1],pack));
 selectedWristFinish::apply(pack);auto original=pack.vertices;
 std::vector<float> materials;for(const auto& v:pack.vertices)materials.push_back(v.value[11]);
 selectedWristFade::apply(pack,[&](unsigned i,float m){materials[i]=m;});
 unsigned hiddenCaps=0,solidSkin=0,fadedSkin=0;
 for(const auto& s:pack.sections)for(unsigned j=0;j<s.count;++j){unsigned i=s.first+j;const auto* v=pack.vertices[i].value;
  assert(!std::memcmp(&pack.vertices[i],&original[i],sizeof(psxAssets::Vertex)));
  if(s.kind==psxAssets::Weapon){assert(materials[i]==v[11]);continue;}
  assert(materials[i]>=32&&materials[i]<41);assert(std::floor(materials[i])-32==v[11]);
  const float coverage=(materials[i]-std::floor(materials[i]))*4;
  assert(coverage>=0&&coverage<=1);
  if(v[11]==4){assert(coverage==0);++hiddenCaps;}
  else if(coverage==1)++solidSkin;else ++fadedSkin;
 }
 assert(hiddenCaps&&solidSkin&&fadedSkin);
 printf("PASS: %u hidden cap vertices, %u solid skin, %u feathered skin; weapons/UVs/fits/topology unchanged\n",hiddenCaps,solidSkin,fadedSkin);
}
