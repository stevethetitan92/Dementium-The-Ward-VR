#include "../src/native_ui_text_policy.hpp"
#include <cassert>
#include <limits>
int main(){
 float back[]={-1,-1,-.775701f,-.833333f},confirm[]={.696262f,-1,1,-.816667f};
 assert(nativeUiText::menuButton(back,48,20,4));
 assert(nativeUiText::menuButton(confirm,65,22,4));
 assert(!nativeUiText::menuButton(back,65,22,4));
 assert(!nativeUiText::menuButton(confirm,48,20,4));
 assert(!nativeUiText::menuButton(back,48,20,6));
 float middle[]={-.2f,-.1f,.2f,.1f};
 assert(!nativeUiText::menuButton(middle,48,20,4));
 float center[]={-.025f,-.80f,.025f,-.70f};
 assert(nativeUiText::interactionButton(center,15,15,4,true));
 assert(nativeUiText::interactionButton(center,20,13,4,true));
 assert(!nativeUiText::interactionButton(center,15,15,4,false));
 assert(!nativeUiText::interactionButton(center,128,256,4,true));
 assert(!nativeUiText::interactionButton(center,15,15,6,true));
 float corner[]={-.95f,-.80f,-.90f,-.70f};
 assert(!nativeUiText::interactionButton(corner,15,15,4,true));
 auto good=[](unsigned i,float* uv){uv[0]=nativeUiText::reloadUV[i*2]/128.f;uv[1]=nativeUiText::reloadUV[i*2+1]/256.f;return true;};
 assert(nativeUiText::isReloadMessage(128,256,90,true,good));
 assert(!nativeUiText::isReloadMessage(128,256,90,false,good));
 assert(!nativeUiText::isReloadMessage(256,256,90,true,good));
 assert(!nativeUiText::isReloadMessage(128,256,89,true,good));
 for(unsigned n=0;n<180;n++)assert(!nativeUiText::isReloadMessage(128,256,90,true,[&](unsigned i,float* uv){good(i,uv);if(i==n/2)uv[n%2]+=.01f;return true;}));
 assert(!nativeUiText::isReloadMessage(128,256,90,true,[](unsigned,float*){return false;}));
 assert(!nativeUiText::isReloadMessage(128,256,90,true,[](unsigned,float* uv){uv[0]=std::numeric_limits<float>::quiet_NaN();return true;}));
}

