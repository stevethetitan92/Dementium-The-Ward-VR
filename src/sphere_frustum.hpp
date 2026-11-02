#pragma once
#include <cmath>
namespace sphereFrustum {
// Native shaders apply a row-major projection after a 3x4 world/view matrix,
// then convert clip Z to -2*z-w. Test the same six homogeneous inequalities.
inline bool contains(const float* projection,const float* view,const float* center,float radius){
    if(!std::isfinite(radius)||radius<0)return false;
    for(unsigned i=0;i<16;i++)if(!std::isfinite(projection[i]))return false;
    for(unsigned i=0;i<12;i++)if(!std::isfinite(view[i]))return false;
    for(unsigned i=0;i<3;i++)if(!std::isfinite(center[i]))return false;
    float planes[6][4]{};
    for(unsigned j=0;j<4;j++){
        planes[0][j]=projection[12+j]+projection[j];
        planes[1][j]=projection[12+j]-projection[j];
        planes[2][j]=projection[12+j]+projection[4+j];
        planes[3][j]=projection[12+j]-projection[4+j];
        planes[4][j]=-2*projection[8+j]-projection[12+j];
        planes[5][j]=2*projection[8+j]+2*projection[12+j];
    }
    for(auto& plane:planes){
        float world[4]{};
        for(unsigned j=0;j<4;j++)world[j]=plane[0]*view[j]+plane[1]*view[4+j]+plane[2]*view[8+j];
        world[3]+=plane[3];
        float norm=std::sqrt(world[0]*world[0]+world[1]*world[1]+world[2]*world[2]);
        if(!std::isfinite(norm)||norm<1e-7f)return false;
        float margin=world[0]*center[0]+world[1]*center[1]+world[2]*center[2]+world[3]+radius*norm;
        if(!std::isfinite(margin)||margin<0)return false;
    }
    return true;
}
}
