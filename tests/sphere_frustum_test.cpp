#include "../src/sphere_frustum.hpp"
#include <array>
#include <cstdio>
#include <limits>
#include <random>
#include <stdexcept>
using Matrix=std::array<float,16>;
using View=std::array<float,12>;
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static Matrix projection(float nearPlane=.02f){
    Matrix p{};float a=50/(50-nearPlane);p[0]=p[5]=1;p[10]=(a+1)*.5f;p[11]=nearPlane*a*.5f;p[14]=-1;return p;
}
static View identity(){return {1,0,0,0,0,1,0,0,0,0,1,0};}
// Independent point oracle: run the native vertex shader's matrix arithmetic
// and homogeneous clipping directly, rather than extracting world planes.
static bool pointOracle(const Matrix& p,const View& m,const float* point){
    double v[4]={0,0,0,1},clip[4]{};
    for(int i=0;i<3;i++){v[i]=m[i*4+3];for(int j=0;j<3;j++)v[i]+=double(m[i*4+j])*point[j];}
    for(int i=0;i<4;i++)for(int j=0;j<4;j++)clip[i]+=double(p[i*4+j])*v[j];
    clip[2]=-2*clip[2]-clip[3];
    return clip[0]>=-clip[3]&&clip[0]<=clip[3]&&clip[1]>=-clip[3]&&clip[1]<=clip[3]&&clip[2]>=0&&clip[2]<=clip[3];
}
// Independent nonzero-radius oracle: evaluate the shader chain at the center
// and three world-axis offsets, then maximize each clip inequality over the
// sphere. This validates conservative half-space rejection, not exact sphere /
// frustum intersection at corners (which can intentionally over-accept).
static double sphereSupportMargin(const Matrix& p,const View& m,const float* center,double radius){
    auto inequalities=[&](const double* point){
        double v[4]={0,0,0,1},clip[4]{};
        for(int i=0;i<3;++i){v[i]=m[i*4+3];for(int j=0;j<3;++j)v[i]+=double(m[i*4+j])*point[j];}
        for(int i=0;i<4;++i)for(int j=0;j<4;++j)clip[i]+=double(p[i*4+j])*v[j];
        clip[2]=-2*clip[2]-clip[3];
        return std::array<double,6>{clip[3]+clip[0],clip[3]-clip[0],clip[3]+clip[1],clip[3]-clip[1],clip[2],clip[3]-clip[2]};
    };
    double point[]={center[0],center[1],center[2]};auto middle=inequalities(point);
    std::array<double,6> lengthSquared{};
    for(unsigned axis=0;axis<3;++axis){point[axis]+=1;auto offset=inequalities(point);point[axis]-=1;for(unsigned plane=0;plane<6;++plane){double gradient=offset[plane]-middle[plane];lengthSquared[plane]+=gradient*gradient;}}
    double minimum=std::numeric_limits<double>::infinity();
    for(unsigned plane=0;plane<6;++plane){double length=std::sqrt(lengthSquared[plane]);require(length>1e-9,"oracle has degenerate clip inequality");minimum=std::fmin(minimum,middle[plane]/length+radius);}
    return minimum;
}
int main(){try{
    auto p=projection();auto view=identity();float point[]={0,0,-5};
    require(sphereFrustum::contains(p.data(),view.data(),point,0),"central point rejected");
    point[2]=1;require(!sphereFrustum::contains(p.data(),view.data(),point,.1f),"behind-camera point accepted");
    point[2]=-.01f;require(!sphereFrustum::contains(p.data(),view.data(),point,0),"near-plane point accepted");
    require(sphereFrustum::contains(p.data(),view.data(),point,.015f),"near-plane intersecting sphere rejected");
    point[2]=-55;require(!sphereFrustum::contains(p.data(),view.data(),point,.1f),"beyond-far sphere accepted");
    auto left=identity(),right=identity();left[3]=.032f;right[3]=-.032f;
    float edge[]={1.02f,0,-1};
    require(!sphereFrustum::contains(p.data(),left.data(),edge,0)&&sphereFrustum::contains(p.data(),right.data(),edge,0),"right-eye-only object lost");
    edge[0]=-1.02f;
    require(sphereFrustum::contains(p.data(),left.data(),edge,0)&&!sphereFrustum::contains(p.data(),right.data(),edge,0),"left-eye-only object lost");
    auto native=p;native[5]=1.767617f;
    float floorPickup[]={0,-1.5f,-.5f};
    require(!sphereFrustum::contains(native.data(),view.data(),floorPickup,.03f),"floor fixture unexpectedly inside native view");
    View down={1,0,0,0,0,.5f,-.8660254f,0,0,.8660254f,.5f,0};
    require(sphereFrustum::contains(p.data(),down.data(),floorPickup,.03f),"look-down floor pickup rejected");
    auto asymmetric=p;asymmetric[2]=.2f;asymmetric[6]=-.15f;
    std::mt19937 random(0x23c011);std::uniform_real_distribution<float> coord(-12,12);
    unsigned cases=0;
    for(auto matrix:{view,left,right,down})for(auto lens:{p,asymmetric,native})for(int i=0;i<2000;i++){
        float v[]={coord(random),coord(random),coord(random)};
        require(sphereFrustum::contains(lens.data(),matrix.data(),v,0)==pointOracle(lens,matrix,v),"plane test disagrees with native shader clipping");++cases;
    }
    View affine={1.2f,.17f,-.08f,.3f,-.11f,.8f,.13f,-.6f,.04f,-.07f,1.1f,.2f};
    std::uniform_real_distribution<float> radii(.002f,2.f);unsigned sphereCases{},sphereAccepted{},sphereRejected{},sphereBoundary{};
    for(auto matrix:{view,left,right,down,affine})for(auto lens:{p,asymmetric,native})for(unsigned i=0;i<2000;++i){
        float center[]={coord(random),coord(random),coord(random)},radius=radii(random);
        const auto margin=sphereSupportMargin(lens,matrix,center,radius);
        if(std::fabs(margin)<.0001){++sphereBoundary;continue;}
        const bool expected=margin>=0,actual=sphereFrustum::contains(lens.data(),matrix.data(),center,radius);
        require(actual==expected,"nonzero sphere rejection disagrees with shader-chain support oracle");++sphereCases;if(actual)++sphereAccepted;else ++sphereRejected;
    }
    require(sphereCases>29000&&sphereAccepted>100&&sphereRejected>100,"nonzero sphere oracle coverage insufficient");
    float tangent[]={1.1f,0,-1};
    require(sphereFrustum::contains(p.data(),view.data(),tangent,.08f),"side-plane intersection rejected");
    require(!sphereFrustum::contains(p.data(),view.data(),tangent,.05f),"outside side-plane sphere accepted");
    auto invalid=p;invalid[0]=std::numeric_limits<float>::quiet_NaN();
    require(!sphereFrustum::contains(invalid.data(),view.data(),floorPickup,.1f),"invalid matrix accepted");
    require(!sphereFrustum::contains(p.data(),view.data(),floorPickup,-1),"negative radius accepted");
    auto zero=Matrix{};require(!sphereFrustum::contains(zero.data(),view.data(),floorPickup,0),"degenerate projection accepted");
    std::printf("PASS: %u shader-oracle point comparisons, both-eye edge visibility, looked-down floor pickup, clipping boundaries and invalid data; no game/headset.\n",cases);
    std::printf("PASS: %u nonzero-radius shader-chain support comparisons, %u accepted / %u rejected, %u numerical-boundary exclusions; translated/rotated/asymmetric/nonuniform affine views, conservative half-space test only.\n",sphereCases,sphereAccepted,sphereRejected,sphereBoundary);
    return 0;
}catch(const std::exception& error){std::fprintf(stderr,"FAIL: %s\n",error.what());return 1;}}
