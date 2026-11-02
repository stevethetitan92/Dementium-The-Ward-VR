#include "../src/native_scope_hud.hpp"
#include <cassert>
#include <cstdio>
#include <initializer_list>
int main(){
 float circle[]={-.2757f,-1.275f,.6589f,.3917f};
 float mask[]={-4.9486f,.3917f,9.0701f,8.725f};
 float label[]={.7617f,.8583f,.8458f,.9417f};
 float fade[]={-1,-1,1,1};float ammo[]={.7944f,-.9333f,.8692f,-.85f};
 for(unsigned size:{8u,256u})assert(nativeScopeHud::suppress(true,4,size,size,4,circle));
 assert(nativeScopeHud::suppress(true,4,1,1,4,mask));
 assert(nativeScopeHud::suppress(true,4,128,256,24,label));
 assert(nativeScopeHud::suppress(true,4,128,256,30,label));
 assert(!nativeScopeHud::suppress(true,4,1,1,4,fade));
 assert(!nativeScopeHud::suppress(true,4,128,256,24,ammo));
 assert(!nativeScopeHud::suppress(false,4,256,256,4,circle));
 assert(!nativeScopeHud::suppress(true,1,256,256,4,circle));
 assert(!nativeScopeHud::suppress(true,4,512,512,4,circle));
 puts("Native scope HUD shapes suppressed; menus, fades, ammo, other weapons retained");
}
