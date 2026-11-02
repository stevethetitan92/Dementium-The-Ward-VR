#include "../src/selected_hunting_texture.hpp"
#include <cstdio>
#include <stdexcept>
static void check(bool value){if(!value)throw std::runtime_error("Hunting texture preservation failed");}
int main(int argc,char** argv){try{
 check(argc==2);psxAssets::Pack original;check(psxAssets::load(argv[1],original));
 auto expanded=original;check(selectedHuntingTexture::apply(argv[1],expanded));
 unsigned changed=0;
 for(const auto& section:original.sections)for(unsigned j=0;j<section.count;++j){
  const unsigned i=section.first+j;const auto* a=original.vertices[i].value;const auto* b=expanded.vertices[i].value;
  check(std::memcmp(a,b,9*sizeof(float))==0);check(a[11]==b[11]);
  if(section.weapon!=4||section.kind!=psxAssets::Weapon)check(std::memcmp(a,b,48)==0);
  else {check(b[9]>=2058.f/8192&&b[9]<=3082.f/8192&&b[10]>=2.f/8192&&b[10]<=1026.f/8192);if(a[9]!=b[9]||a[10]!=b[10])++changed;}
 }
 check(changed>1000);check(original.texture==expanded.texture);
 auto mismatched=original;mismatched.texture.back()^=1;auto unchanged=mismatched.vertices;
 check(!selectedHuntingTexture::apply(argv[1],mismatched));check(std::memcmp(unchanged.data(),mismatched.vertices.data(),unchanged.size()*sizeof(psxAssets::Vertex))==0);
 check(!selectedHuntingTexture::apply(std::filesystem::path(argv[1])/"missing-marker",mismatched));
 std::printf("PASS: %u rifle UVs expanded; all positions, normals, factors, other weapons and hands preserved; mismatched/missing marker rejected\n",changed);return 0;
 }catch(const std::exception& e){puts(e.what());return 1;}}
