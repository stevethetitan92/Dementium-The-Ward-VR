#include "../src/cylinder_flick.hpp"
#include <stdexcept>
#include <cstdio>
static void check(bool v){if(!v)throw std::runtime_error("cylinder flick failed");}
static XrQuaternionf roll(float a){return {0,0,std::sin(a/2),std::cos(a/2)};}
int main(){
 cylinderFlick::Detector d;XrVector3f axis{0,0,1};
 for(unsigned i=0;i<50;++i)check(!d.update(roll(i*.003f),axis,100+i*10,true));
 d.reset();bool fired=false;for(unsigned i=0;i<12;++i)fired|=d.update(roll(i*.035f),axis,100+i*10,true);check(fired);
 d.reset();for(unsigned i=0;i<12;++i)check(!d.update(roll(-int(i)*.035f),axis,100+i*10,true));
 d.reset();for(unsigned i=0;i<12;++i)check(!d.update(roll(i*.035f),axis,100+i*10,false));
 d.reset();check(!d.update(roll(0),axis,100,true));check(!d.update(roll(.5f),axis,400,true));
 puts("PASS: closing-direction flick, slow motion, opposite motion, eligibility and tracking gap");
}
