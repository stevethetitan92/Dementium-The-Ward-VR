#include "../src/selected_cases.hpp"
#include <cassert>
#include <cstdio>
int main(int argc,char** argv){
 assert(argc==2);psxAssets::Pack pack;assert(psxAssets::load(argv[1],pack));
 const auto original=pack.vertices.size();selectedCases::load(argv[1],pack);
 assert(pack.vertices.size()>original);assert(selectedCases::sections[0]==~0u);
 for(unsigned w=1;w<5;++w){auto id=selectedCases::sections[w];assert(id<pack.sections.size());assert(selectedCases::contains(id));
  const auto& s=pack.sections[id];assert(s.weapon==w);
  for(unsigned i=0;i<s.count;++i)assert(pack.vertices[s.first+i].value[11]==7);
 }
 puts("Source cases load with texture/UV mode for four firearms; no revolver shot-case geometry");
}
