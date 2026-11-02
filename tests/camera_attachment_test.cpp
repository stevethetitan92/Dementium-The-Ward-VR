#include "../src/tracking_math.hpp"
#include <cstdio>
#include <stdexcept>
#include <limits>
static void check(bool v){if(!v)throw std::runtime_error("camera attachment basis failed");}
int main(){try{
 const float view[]={-.940378189f,0,.340240180f,1.33398938f,0,1,0,-13.6998806f,-.340240180f,0,-.940378189f,.516853690f};
 const float player[]={.947750449f,0,.318849176f,1.43020236f,0,1,0,13.6998806f,-.318849176f,0,.947750449f,.032158755f};
 float basis[12];check(motionTracking::cameraWorldBasis(view,basis));
 XrPosef hand{{0,0,0,1},{.2f,-.3f,-.4f}};XrVector3f eye{1.43020236f,13.6998806f,.032158755f};
 float corrected[12],previous[12];motionTracking::weaponMatrix(basis,eye,hand,1,corrected);motionTracking::weaponMatrix(player,eye,hand,1,previous);
 for(unsigned r=0;r<3;r++){
  float point=0,old=0;for(unsigned k=0;k<3;k++){point+=view[r*4+k]*(corrected[k*4+3]-(&eye.x)[k]);old+=view[r*4+k]*(previous[k*4+3]-(&eye.x)[k]);}
  check(std::fabs(point-(&hand.position.x)[r])<.0001f);
  if(r==0||r==2)check(std::fabs(old-(&hand.position.x)[r])>.3f);
  for(unsigned c=0;c<3;c++){float v=0;for(unsigned k=0;k<3;k++)v+=view[r*4+k]*corrected[k*4+c];check(std::fabs(v-(r==c?1.f:0.f))<.0001f);}
 }
 float bad[12];std::memcpy(bad,view,sizeof bad);bad[0]=std::numeric_limits<float>::quiet_NaN();check(!motionTracking::cameraWorldBasis(bad,basis));
 std::puts("PASS: captured live camera/player axes reproduce reversed right/depth placement; camera-derived basis closes position and orientation in camera space; invalid basis rejected.");return 0;
}catch(const std::exception& e){std::puts(e.what());return 1;}}
