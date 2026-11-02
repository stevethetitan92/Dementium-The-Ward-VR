#include "../src/native_flash_alignment.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
int main(){
 for(unsigned i=0;i<24;++i){float c=std::cos(i*.23f),s=std::sin(i*.23f);
  float aim[]={c,0,s,2,0,1,0,3,-s,0,c,4};float model[]={-c,0,-s,2.1f,0,1,0,2.9f,s,0,-c,4.2f};
  float before[12];std::memcpy(before,model,sizeof model);float local[]={.1f,.02f,.5f},tip[]={-.039f,.09134f,-.7198f};
  assert(nativeFlashAlignment::align(model,aim,local,tip));
  for(unsigned r=0;r<3;++r){float got=model[r*4+3],want=aim[r*4+3];for(unsigned k=0;k<3;++k){got+=model[r*4+k]*local[k];want+=aim[r*4+k]*tip[k];assert(model[r*4+k]==before[r*4+k]);}assert(std::fabs(got-want)<1e-5f);}
  std::memcpy(before,model,sizeof model);local[0]=std::numeric_limits<float>::quiet_NaN();assert(!nativeFlashAlignment::align(model,aim,local,tip));assert(!std::memcmp(before,model,sizeof model));
 }
 puts("PASS: native flash aligns to selected muzzle across 24 rotations, basis preserved, invalid data leaves draw unchanged");
}
