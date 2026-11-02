#pragma once
#include <cmath>
namespace nativeFlashAlignment {
// Both native matrices are row-major 3x4 (translation in column 3).
inline bool align(float* model,const float* aim,const float* local,const float* muzzle){
 for(unsigned i=0;i<12;++i)if(!std::isfinite(model[i])||!std::isfinite(aim[i]))return false;
 for(unsigned i=0;i<3;++i)if(!std::isfinite(local[i])||!std::isfinite(muzzle[i])||std::fabs(local[i])>4||std::fabs(muzzle[i])>4)return false;
 float change[3]{};
 for(unsigned r=0;r<3;++r){float from=model[r*4+3],to=aim[r*4+3];for(unsigned c=0;c<3;++c){from+=model[r*4+c]*local[c];to+=aim[r*4+c]*muzzle[c];}change[r]=to-from;if(!std::isfinite(change[r]))return false;}
 for(unsigned r=0;r<3;++r)model[r*4+3]+=change[r];
 return true;
}
}
