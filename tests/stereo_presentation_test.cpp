#include "../src/stereo_presentation.hpp"
#include <cassert>
#include <initializer_list>
int main(){using namespace stereoPresentation;
// Startup/menu can use native desktop; gameplay can never leak its native HUD.
assert(desktopFallback(false,false,false,0));
assert(!desktopFallback(true,false,false,0));
assert(!worldVisible(true,false,0));
for(auto age:{0ull,249ull,250ull,251ull,10000ull}){
assert(!desktopFallback(true,false,true,age));assert(worldVisible(true,true,age));
assert(!desktopFallback(false,true,true,age));
assert(desktopFallback(false,false,true,age)==(age>250));
assert(worldVisible(false,true,age)==(age<=250));
}
HandoffFade fade;
assert(fade.update(0,true,true,0)==0);
assert(fade.update(100,false,true,100)==0);
float last=0;
for(unsigned t=131;t<=250;++t){float a=fade.update(t,false,true,t);assert(a>=last&&a<=1);last=a;}
assert(last==1);
assert(fade.update(251,false,true,251)==1);
last=1;for(unsigned t=252;t<=431;++t){float a=fade.update(t,false,true,t);assert(a<=last&&a>=0);last=a;}
assert(last==0);
assert(fade.update(500,true,true,0)==1);
assert(fade.update(590,true,true,0)==.5f);
assert(fade.update(680,true,true,0)==0);
// Long gameplay stalls must not be mistaken for a cutscene.
assert(fade.update(10000,true,true,9000)==0);
// Custom inventory and mod menus retain their existing behavior.
assert(fade.update(11000,false,true,10000,true)==0);
assert(fade.update(11001,true,true,0,true)==0);
assert(fade.update(11002,true,true,0)==0);
// Dropped frames finish the ramp by wall time, not by frame count.
assert(fade.update(12000,false,true,1000)==1);
assert(fade.update(13000,false,true,2000)==0);
}
