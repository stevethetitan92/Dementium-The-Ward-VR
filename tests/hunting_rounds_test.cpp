#include "../src/hunting_rounds.hpp"
#include <cassert>
#include <cstdio>
int main(int argc,char** argv){
 assert(argc==2);psxAssets::Pack pack;assert(psxAssets::load(argv[1],pack));
 auto before=pack.vertices;auto sections=pack.sections.size();assert(huntingRounds::append(argv[1],pack));assert(pack.sections.size()==sections+5);
 assert(std::memcmp(before.data(),pack.vertices.data(),before.size()*sizeof(psxAssets::Vertex))==0);
 for(unsigned r=0;r<5;++r){auto id=huntingRounds::first+r;assert(huntingRounds::contains(id));const auto& s=pack.sections[id];assert(s.weapon==4&&s.count==8232);
  for(unsigned j=0;j<s.count;++j)assert(pack.vertices[s.first+j].value[11]==7);
 }
 assert(huntingRounds::count(true,0,16,false)==0);assert(huntingRounds::count(true,3,16,false)==3);assert(huntingRounds::count(true,0,2,true)==2);assert(huntingRounds::count(true,4,16,true)==5);assert(huntingRounds::count(false,5,16,false)==0);
 pack.texture[0]^=1;assert(!huntingRounds::append(argv[1],pack));assert(huntingRounds::first==~0u);
 puts("Five textured source rounds, original geometry preservation, partial/empty counts and mismatched-atlas rejection passed");
}
