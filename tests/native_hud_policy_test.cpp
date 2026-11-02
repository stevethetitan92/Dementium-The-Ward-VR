#include "../src/native_hud_policy.hpp"
#include <cassert>
#include <limits>
#include <iostream>
int main(){
 using nativeHud::Element;using nativeHud::classify;
 float hp[]{-.9626f,-.9333f,-.8598f,-.8500f};
 float ekg[]{-.8411f,-.9667f,-.7103f,-.8167f};
 float ammo[]{.7944f,-.9333f,.8692f,-.8500f};
 float reserve[]{.8645f,-.9333f,.9579f,-.8500f};
 assert(classify(hp,128,256,24,true)==Element::Health);
 assert(classify(ekg,28,18,4,true)==Element::Health);
 assert(classify(ammo,128,256,18,true)==Element::Ammo);
 assert(classify(reserve,128,256,24,true)==Element::Ammo);
 for(auto b:{hp,ekg,ammo,reserve})assert(classify(b,128,256,24,false)==Element::Other);
 float center[]{-.15f,-.95f,.15f,-.81f};
 assert(classify(center,128,256,24,true)==Element::Other);
 assert(classify(hp,256,256,24,true)==Element::Other);
 assert(classify(hp,128,256,6,true)==Element::Other);
 assert(classify(nullptr,128,256,24,true)==Element::Other);
 hp[0]=std::numeric_limits<float>::quiet_NaN();assert(classify(hp,128,256,24,true)==Element::Other);
 hp[0]=-1.1f;assert(classify(hp,128,256,24,true)==Element::Other);
 hp[0]=hp[2];assert(classify(hp,128,256,24,true)==Element::Other);
 std::cout<<"Native HUD policy: observed status draws filtered; menus, prompts, unknown and invalid draws preserved.\n";
}
