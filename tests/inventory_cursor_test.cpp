#include "../src/native_ui_text_policy.hpp"
#include <cassert>
#include <limits>
int main(){
 float values[4][2]={{0,0},{1,0},{0,1},{1,1}};
 auto read=[&](unsigned i,float* v){v[0]=values[i][0];v[1]=values[i][1];return true;};
 assert(nativeUiText::inventoryCursor(40,40,4,true,read));
 assert(!nativeUiText::inventoryCursor(40,40,4,false,read));
 assert(!nativeUiText::inventoryCursor(64,64,4,true,read)); // native map markers
 assert(!nativeUiText::inventoryCursor(40,40,6,true,read));
 values[1][0]=.5f;assert(!nativeUiText::inventoryCursor(40,40,4,true,read));
 values[1][0]=std::numeric_limits<float>::quiet_NaN();assert(!nativeUiText::inventoryCursor(40,40,4,true,read));
 assert(!nativeUiText::inventoryCursor(40,40,4,true,[](unsigned,float*){return false;}));
}
